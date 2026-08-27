#include "AbilitySystem/GA/GA_FireBase.h"
#include "AbilitySystem/TPSAbilityTargetData.h"
#include "Common/TPSGameplayTags.h"
#include "Common/TPSLog.h"
#include "Weapon/WeaponBase.h"
#include "Character/PlayerCharacter.h"
#include "Character/CommonCharacter.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/EnemyCharacter.h"
#include "Common/AIBlackboardKeys.h"

/*
    Cue 실행용 예측 키 스코프.

    FScopedPredictionWindow(ASC, Key) 생성자는 IsNetSimulating() == false 검사가 있어
    서버에서만 동작한다. 클라이언트에서도 같은 키를 실어야 예측 재생과
    중복 방지가 성립하므로 직접 세팅한다.
*/
struct FTPSScopedCueKey
{
    FTPSScopedCueKey(UAbilitySystemComponent* InASC, const FPredictionKey& Key)
        : ASC(InASC)
    {
        if (ASC)
        {
            Saved = ASC->ScopedPredictionKey;
            ASC->ScopedPredictionKey = Key;
        }
    }
    ~FTPSScopedCueKey()
    {
        if (ASC) { ASC->ScopedPredictionKey = Saved; }
    }
    UAbilitySystemComponent* ASC = nullptr;
    FPredictionKey Saved;
};

UGA_FireBase::UGA_FireBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    /*
        기본값 LocalOnly에서는 소유 클라이언트에서만 실행됨

        LocalPredicted로 올리면 클라이언트가 즉시 반응하면서도
        서버가 같은 스펙을 이어받아 CanActivateAbility / CheckCost를
        독립적으로 재검증한다.
    */
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(TAG_Ability_Fire);
    SetAssetTags(AssetTags);

    // 재장전/사망/무기교체 중 발사 차단
    ActivationBlockedTags.AddTag(TAG_State_Reloading);
    ActivationBlockedTags.AddTag(TAG_State_Dead);
    ActivationBlockedTags.AddTag(TAG_State_Swapping);

    CooldownTagContainer.AddTag(TAG_Cooldown_Fire);
}

const FGameplayTagContainer* UGA_FireBase::GetCooldownTags() const
{
    return &CooldownTagContainer;
}

bool UGA_FireBase::ShouldFireImmediately(const FGameplayAbilityActorInfo* ActorInfo) const
{
    // 플레이어면 즉발, 아니면(적) 노티파이 대기
    return Cast<APlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr) != nullptr;
}

bool UGA_FireBase::PlayFireMontage(const FGameplayAbilityActorInfo* ActorInfo, bool bEndAbilityOnMontageEnd)
{   
    ACommonCharacter* Character = ActorInfo ? Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Character == nullptr)
    {   
        return false;
    }

    UAnimMontage* FireMontage = Character->GetFireMontage();
    if (FireMontage == nullptr)
    {
        return false;
    }

    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Character))
    {
        AActor* Target = nullptr;
        if (AAIController* AICon = Cast<AAIController>(Enemy->GetController()))
        {
            if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
            {
                Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey));
            }
        }
        Enemy->SetFireTarget(Target);
    }

    if (bEndAbilityOnMontageEnd)
    {
        // 적: 몽타주가 발사 타이밍(노티파이)과 종료 타이밍을 모두 잡는다.
        UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, FireMontage);
        if (!Task)
        {
            return false;   // 태스크 생성 실패 → 호출부가 즉시 EndAbility
        }
        Task->OnCompleted.AddDynamic(this, &UGA_FireBase::OnFireMontageEnded);
        Task->OnInterrupted.AddDynamic(this, &UGA_FireBase::OnFireMontageEnded);
        Task->OnCancelled.AddDynamic(this, &UGA_FireBase::OnFireMontageCancelled);
        MontageStartTime = GetWorld()->GetTimeSeconds();
        Task->ReadyForActivation();
    }
    else
    {
        // 플레이어: 어빌리티가 곧바로 끝나므로 몽타주는 연출용으로 재생한다.
        // (PlayMontageAndWait를 쓰면 EndAbility가 몽타주를 즉시 취소해버린다)
        // TODO(M2b-2): 이 재생은 로컬 전용이라 다른 플레이어에게 보이지 않는다.
        //              GameplayCue로 이관할 것.
        if (USkeletalMeshComponent* Mesh = Character->GetMesh())
        {
            if (UAnimInstance* Anim = Mesh->GetAnimInstance())
            {
                Anim->Montage_Play(FireMontage);
            }
        }
    }
    return true;
}

void UGA_FireBase::OnFireMontageEnded()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_FireBase::OnFireMontageCancelled()
{
    UE_LOG(TPSLog, Warning, TEXT("%s FireMontage 취소됨 (경과 %.3f초)"),
        *TPSNetDebug::TPSNetPrefix(GetAvatarActorFromActorInfo()),
        GetWorld()->GetTimeSeconds() - MontageStartTime);
    OnFireMontageEnded();
}

void UGA_FireBase::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* /*TriggerEventData*/)
{
    // 비용/쿨다운 커밋 — 쿨다운 중이면 실패 종료
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 발사 타이밍 분기
    if (ShouldFireImmediately(ActorInfo))
    {
        // 플레이어: 몽타주는 연출용으로만 재생하고 즉발 후 종료
        PlayFireMontage(ActorInfo, /*bEndAbilityOnMontageEnd=*/false);
        FireOnce(ActorInfo);

        /*
            서버가 원격 클라의 조준점을 기다리는 중이면 어빌리티를 끝내지 않는다.
            여기서 EndAbility를 부르면 대기 델리게이트가 해제되어
            조준점이 도착해도 발사가 이뤄지지 않는다.
        */
        if (!IsWaitingForClientAim())
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
    }
    else
    {
        // 적: 몽타주 노티파이가 발사하고 몽타주 종료가 어빌리티를 끝낸다.
        if (!PlayFireMontage(ActorInfo, /*bEndAbilityOnMontageEnd=*/true))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
    }
}

void UGA_FireBase::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    StopWaitForClientAim();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UGA_FireBase::ComputeLocalAimPoint(const FGameplayAbilityActorInfo* ActorInfo) const
{
    ACommonCharacter* Char = GetOwningCharacter(ActorInfo);
    return Char ? Char->GetAimPoint() : FVector::ZeroVector;
}

void UGA_FireBase::FireOnce(const FGameplayAbilityActorInfo* ActorInfo)
{
    ACommonCharacter* Char = GetOwningCharacter(ActorInfo);
    if (!Char) return;

    AWeaponBase* Weapon = Char->GetCurrentWeapon();
    if (!Weapon) return;

    const bool bLocal     = ActorInfo->IsLocallyControlled();
    const bool bAuthority = ActorInfo->IsNetAuthority();

    if (bLocal)
    {
        // 카메라를 가진 머신에서만 조준점을 계산할 수 있다.
        const FVector AimPoint = ComputeLocalAimPoint(ActorInfo);

        if (bAuthority)
        {
            // AI(서버에서 AIController가 조종) — 조준점을 스스로 알므로 왕복 없이 권위 발사.
            // 연출 Cue도 FireAuthoritative 안에서 실행되므로 여기서 따로 부르지 않는다.
            FireAuthoritative(ActorInfo, AimPoint);
        }
        else
        {
            /*
                원격 클라 — 판정은 서버에 맡기고 연출만 예측 재생한다.

                ExecuteGameplayCue는 비권위 + 로컬 예측 키일 때 로컬에서만 실행된다.
                이후 서버가 같은 예측 키로 멀티캐스트하면 GAS가 이 클라만 건너뛰므로
                중복 재생되지 않는다.
            */
            PredictFireCues(ActorInfo, AimPoint);
            SendAimPointToServer(ActorInfo, AimPoint);
        }
    }
    else if (bAuthority)
    {
        // 서버에서 원격 폰의 어빌리티가 실행된 경우. 조준점을 알 수 없으므로 기다린다.
        BeginWaitForClientAim(ActorInfo);
    }
}

void UGA_FireBase::SendAimPointToServer(const FGameplayAbilityActorInfo* ActorInfo, const FVector& AimPoint)
{
    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC) return;

    /*
        FScopedPredictionWindow

        이 블록 안에서 만든 예측 키를 서버가 받아, 클라이언트가 예측 실행한
        어빌리티 인스턴스와 이 조준점을 짝지을 수 있게 한다.
        키가 없으면 서버는 "어느 발사에 대한 조준점인지" 알 수 없다.
    */
    FScopedPredictionWindow ScopedPrediction(ASC, GetCurrentActivationInfo().GetActivationPredictionKey());

    FGameplayAbilityTargetDataHandle DataHandle;
    DataHandle.Add(new FTPSTargetData_AimPoint(AimPoint));

    ASC->ServerSetReplicatedTargetData(
        GetCurrentAbilitySpecHandle(),
        GetCurrentActivationInfo().GetActivationPredictionKey(),
        DataHandle,
        FGameplayTag(),
        ASC->ScopedPredictionKey);

    UE_LOG(TPSLog, Verbose, TEXT("%s 조준점 전송 %s"),
        *TPSNetDebug::TPSNetPrefix(ActorInfo->AvatarActor.Get()), *AimPoint.ToCompactString());
}

void UGA_FireBase::BeginWaitForClientAim(const FGameplayAbilityActorInfo* ActorInfo)
{
    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC || TargetDataDelegateHandle.IsValid()) return;

    const FGameplayAbilitySpecHandle SpecHandle = GetCurrentAbilitySpecHandle();
    const FPredictionKey PredKey = GetCurrentActivationInfo().GetActivationPredictionKey();

    TargetDataDelegateHandle =
        ASC->AbilityTargetDataSetDelegate(SpecHandle, PredKey)
           .AddUObject(this, &UGA_FireBase::OnClientAimPointReceived);

    // 조준점이 도착하지 않으면 어빌리티가 영구히 매달린다.
    // InstancedPerActor라 그 상태에서는 다시 발사할 수도 없다.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            ClientAimTimeoutTimer, this, &UGA_FireBase::OnClientAimTimeout, ClientAimTimeout, false);
    }

    /*
        조준점이 어빌리티 활성화보다 먼저 도착했을 수 있다.
        (ServerSetReplicatedTargetData가 ServerTryActivateAbility보다 먼저 처리되는 경우)
        그 경우 GAS가 값을 보관해두므로 여기서 한 번 꺼내본다.
    */
    ASC->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, PredKey);
}

void UGA_FireBase::StopWaitForClientAim()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ClientAimTimeoutTimer);
    }

    if (!TargetDataDelegateHandle.IsValid()) return;

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->AbilityTargetDataSetDelegate(
                GetCurrentAbilitySpecHandle(),
                GetCurrentActivationInfo().GetActivationPredictionKey())
            .Remove(TargetDataDelegateHandle);
    }
    TargetDataDelegateHandle.Reset();
}

void UGA_FireBase::OnClientAimPointReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag /*ActivationTag*/)
{
    const FGameplayAbilitySpecHandle SpecHandle = GetCurrentAbilitySpecHandle();
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        // 소비하지 않으면 다음 발사에서 옛 조준점이 다시 잡힌다.
        ASC->ConsumeClientReplicatedTargetData(SpecHandle, ActivationInfo.GetActivationPredictionKey());
    }

    StopWaitForClientAim();

    const FTPSTargetData_AimPoint* AimData =
        static_cast<const FTPSTargetData_AimPoint*>(Data.Get(0));

    if (AimData && AimData->GetScriptStruct() == FTPSTargetData_AimPoint::StaticStruct())
    {
        FireAuthoritative(ActorInfo, AimData->AimPoint);
    }
    else
    {
        UE_LOG(TPSLog, Warning, TEXT("%s 예상치 못한 TargetData 타입 — 발사 무시"),
            *TPSNetDebug::TPSNetPrefix(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
    }

    EndAbility(SpecHandle, ActorInfo, ActivationInfo, true, false);
}

void UGA_FireBase::OnClientAimTimeout()
{
    UE_LOG(TPSLog, Warning, TEXT("%s 클라 조준점 미도착"),
        *TPSNetDebug::TPSNetPrefix(GetCurrentActorInfo() ? GetCurrentActorInfo()->AvatarActor.Get() : nullptr));

    StopWaitForClientAim();

    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

bool UGA_FireBase::ValidateAimPoint(ACommonCharacter* Char, AWeaponBase* Weapon, FVector& InOutAimPoint) const
{
    if (!Char || !Weapon) return false;

    const FVector Muzzle = Weapon->GetMuzzleLocation();
    FVector Delta = InOutAimPoint - Muzzle;
    const float Dist = Delta.Size();
    if (Dist <= KINDA_SMALL_NUMBER) return false;

    const FVector Dir = Delta / Dist;

    // 사거리 초과 → 클램프
    const float MaxDist = Weapon->GetFireRange() * AimRangeTolerance;
    if (Dist > MaxDist)
    {
        InOutAimPoint = Muzzle + Dir * MaxDist;
    }

    // 등 뒤 조준 거부
    if (AimMinForwardDot > -1.0f)
    {
        if (FVector::DotProduct(Char->GetActorForwardVector(), Dir) < AimMinForwardDot)
        {
            UE_LOG(TPSLog, Warning, TEXT("%s 조준점 검증 실패 (후방)"),
                *TPSNetDebug::TPSNetPrefix(Char));
            return false;
        }
    }

    return true;
}

void UGA_FireBase::FireAuthoritative(const FGameplayAbilityActorInfo* ActorInfo, const FVector& AimPoint)
{
    ACommonCharacter* Char = GetOwningCharacter(ActorInfo);
    if (!Char) return;

    AWeaponBase* Weapon = Char->GetCurrentWeapon();
    if (!Weapon) return;

    FVector ValidatedAim = AimPoint;
    if (!ValidateAimPoint(Char, Weapon, ValidatedAim))
    {
        return;   // 탄약과 쿨다운은 이미 소모됐다. 명중만 무효.
    }

    FHitResult Hit;
    Weapon->FireAuthoritative(ValidatedAim, Char->GetController(), Hit);

    // 서버 권위 연출 — 예측 키를 실어 보내 소유 클라의 중복 재생을 막는다.
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC) return;

    // 발사 연출: 예측 키를 실어 보내 사격자의 중복 재생을 막는다.
    {
        FScopedPredictionWindow ScopedPrediction(ASC, GetCurrentActivationInfo().GetActivationPredictionKey());
        Char->ExecuteFireCue();
    }

    if (!Hit.bBlockingHit) return;

    /*
        탄착 연출은 무기가 예측을 지원하는지에 따라 갈린다.

        지원(라이플)  : 예측 키를 실어 보낸다 → 사격자는 건너뛰고 나머지만 재생
        미지원(머신건): 키 없이 보낸다 → 사격자 포함 전원이 서버 탄착을 재생

        난수 퍼짐이 있는 무기는 클라 예측이 서버와 일치할 수 없으므로
        예측을 포기하고 서버 결과 하나만 보여준다.
    */
    if (Weapon->SupportsPredictedImpact())
    {
        FScopedPredictionWindow ScopedPrediction(ASC, GetCurrentActivationInfo().GetActivationPredictionKey());
        Char->ExecuteImpactCue(Hit.ImpactPoint, Hit.ImpactNormal);
    }
    else
    {
        /*
			ServerSetReplicatedTargetData가 델리게이트를 브로드캐스트하기 전에
			클라 예측 키로 스코프를 열어둔다. 그래서 여기서 아무것도 하지 않으면
			그 키가 그대로 실려 나가 사격자가 Cue를 건너뛴다.
			예측하지 않은 무기는 사격자도 서버 탄착을 봐야 하므로 무효 키로 덮는다.
		*/
        FTPSScopedCueKey ScopedCueKey(ASC, FPredictionKey());
        Char->ExecuteImpactCue(Hit.ImpactPoint, Hit.ImpactNormal);
    }
}

void UGA_FireBase::PredictFireCues(const FGameplayAbilityActorInfo* ActorInfo, const FVector& AimPoint)
{
    ACommonCharacter* Char = GetOwningCharacter(ActorInfo);
    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!Char || !ASC) return;

    AWeaponBase* Weapon = Char->GetCurrentWeapon();
    if (!Weapon) return;

    /*
       Cue에 어빌리티 활성화 예측 키를 실어 보낸다.

       클라: ExecuteGameplayCue가 로컬 키를 확인하고 로컬 재생만 수행
       서버: 같은 키로 멀티캐스트 → 그 키를 발급한 클라만 건너뛴다
   */
    FTPSScopedCueKey ScopedCueKey(ASC, GetCurrentActivationInfo().GetActivationPredictionKey());

    Char->ExecuteFireCue();

    // 서버가 어디를 맞췄는지 모르므로 판정 없는 트레이스로 탄착 위치만 추정한다.
    FHitResult Hit;
    if (Weapon->TracePredictedImpact(AimPoint, Hit) && Hit.bBlockingHit)
    {
        Char->ExecuteImpactCue(Hit.ImpactPoint, Hit.ImpactNormal);
    }
}

ACommonCharacter* UGA_FireBase::GetOwningCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (!ActorInfo) return nullptr;
    return Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get());
}

/*
    무한 탄약 무기 예외 처리

    CostGameplayEffectClass는 어빌리티 클래스에 고정되지만, bInfiniteAmmo는
    무기 BP 설정이다. 두 경로를 잇기 위해 Cost 검사/적용을 무기 설정으로 게이트한다.
    (적도 탄약을 쓰고 재장전하므로 캐릭터 종류로는 분기하지 않는다)
*/
bool UGA_FireBase::CheckCost(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (ACommonCharacter* Char = GetOwningCharacter(ActorInfo))
    {
        if (AWeaponBase* Weapon = Char->GetCurrentWeapon())
        {
            if (Weapon->IsInfiniteAmmo())
            {
                return true;
            }
        }
    }
    return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UGA_FireBase::ApplyCost(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo) const
{
    if (ACommonCharacter* Char = GetOwningCharacter(ActorInfo))
    {
        if (AWeaponBase* Weapon = Char->GetCurrentWeapon())
        {
            if (Weapon->IsInfiniteAmmo())
            {
                return;
            }
        }
    }
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

void UGA_FireBase::ApplyCooldown(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo) const
{
    UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
    if (!CooldownGE) return;

    float Interval = 0.1f;
    if (ACommonCharacter* Char = GetOwningCharacter(ActorInfo))
    {
        if (AWeaponBase* Weapon = Char->GetCurrentWeapon())
        {
            Interval = Weapon->GetFireInterval();   // 발사 간격을 쿨다운으로 주입
        }
    }

    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
        CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
    if (Spec.IsValid())
    {
		Spec.Data->SetSetByCallerMagnitude(TAG_Data_CooldownDuration, Interval);    // Interval을 SetByCaller로 전달
        ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
    }
}
