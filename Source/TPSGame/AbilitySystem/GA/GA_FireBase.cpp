#include "AbilitySystem/GA/GA_FireBase.h"
#include "Common/TPSGameplayTags.h"
#include "Weapon/WeaponBase.h"
#include "Character/PlayerCharacter.h"
#include "Character/CommonCharacter.h"
#include "GameplayEffect.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
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

void UGA_FireBase::PlayFireMontage(const FGameplayAbilityActorInfo* ActorInfo)
{
    ACommonCharacter* Character = Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get());
    if (Character == nullptr)
    {
        return;
    }

    UAnimMontage* FireMontage = Character->GetFireMontage();
    if (FireMontage == nullptr)
    {
        return;
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

    UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, FireMontage);
    Task->OnCompleted.AddDynamic(this, &UGA_FireBase::OnFireMontageEnded);
    Task->OnInterrupted.AddDynamic(this, &UGA_FireBase::OnFireMontageEnded);
    Task->OnCancelled.AddDynamic(this, &UGA_FireBase::OnFireMontageEnded);
    Task->ReadyForActivation();
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

    PlayFireMontage(ActorInfo);

    // 발사 타이밍 분기
    if (ShouldFireImmediately(ActorInfo))
    {
        FireOnce(ActorInfo);

        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
    else
    {
        // 적은 노티파이가 발사
    }
}

void UGA_FireBase::FireOnce(const FGameplayAbilityActorInfo* ActorInfo) const
{
    ACommonCharacter* Char = GetOwningCharacter(ActorInfo);
    if (!Char) return;

    AWeaponBase* Weapon = Char->GetCurrentWeapon();
    if (!Weapon) return;

    const FVector AimPoint = Char->GetAimPoint();   // 카메라 기준 크로스헤어
    if (Weapon->Fire(AimPoint, Char->GetController()))
    {
        if (APlayerCharacter * Player = Cast<APlayerCharacter>(Char))
        {
            Player->BroadcastAmmo();    // 탄약 UI 갱신
		}
    }
}

ACommonCharacter* UGA_FireBase::GetOwningCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (!ActorInfo) return nullptr;
    return Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get());
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