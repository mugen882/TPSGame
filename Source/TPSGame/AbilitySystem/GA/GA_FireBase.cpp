#include "AbilitySystem/GA/GA_FireBase.h"
#include "Common/TPSGameplayTags.h"
#include "Weapon/WeaponBase.h"
#include "Character/PlayerCharacter.h"
#include "Character/CommonCharacter.h"
#include "GameplayEffect.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/EnemyCharacter.h"
#include "Common/AIBlackboardKeys.h"

UGA_FireBase::UGA_FireBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

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
        Task->OnCancelled.AddDynamic(this, &UGA_FireBase::OnFireMontageEnded);
        Task->ReadyForActivation();
    }
    else
    {
        // 플레이어: 어빌리티가 곧바로 끝나므로 몽타주는 연출용으로 재생한다.
        // (PlayMontageAndWait를 쓰면 EndAbility가 몽타주를 즉시 취소해버린다)
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

        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
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

void UGA_FireBase::FireOnce(const FGameplayAbilityActorInfo* ActorInfo) const
{
    ACommonCharacter* Char = GetOwningCharacter(ActorInfo);
    if (!Char) return;

    AWeaponBase* Weapon = Char->GetCurrentWeapon();
    if (!Weapon) return;

    const FVector AimPoint = Char->GetAimPoint();   // 카메라 기준 크로스헤어

    Weapon->Fire(AimPoint, Char->GetController());
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