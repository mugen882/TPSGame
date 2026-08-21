#include "AbilitySystem/GA/GA_FireMachineGun.h"
#include "Weapon/WeaponBase.h"
#include "Character/CommonCharacter.h"
#include "Common/TPSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Weapon/WeaponMachineGun.h"
#include "Character/PlayerCharacter.h"
#include "Character/PlayerAimComponent.h"

UGA_FireMachineGun::UGA_FireMachineGun()
{
    // 발사 중 재입력으로 어빌리티를 다시 트리거하지 않음
    bRetriggerInstancedAbility = false;

    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(TAG_Input_Fire_MachineGun);
    SetAssetTags(AssetTags);
}

void UGA_FireMachineGun::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* /*TriggerEventData*/)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 첫 발 즉시
    FireOnce(ActorInfo);

    // 이후 FireInterval마다 반복
    float Interval = 0.1f;
    if (ACommonCharacter* Char = GetOwningCharacter(ActorInfo))
        if (AWeaponBase* Weapon = Char->GetCurrentWeapon())
            Interval = Weapon->GetFireInterval();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(FireLoopTimer, this, &UGA_FireMachineGun::FireLoop, Interval, /*loop=*/true);
    }
}

void UGA_FireMachineGun::FireLoop()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    // 타이머는 액터/ASC 파괴 도중에도 한 번 더 돌 수 있으므로 널체크 후 종료
    if (!ASC || ASC->HasMatchingGameplayTag(TAG_State_Reloading))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    ACommonCharacter* Char = GetOwningCharacter(GetCurrentActorInfo());
    AWeaponBase* Weapon = Char ? Char->GetCurrentWeapon() : nullptr;

    // 무기가 머신건이 아니게 바뀌었거나 죽었으면 연사 종료
    if (!Weapon || !Weapon->IsA<AWeaponMachineGun>() || (Char && Char->IsDead()))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // 탄약 0 → 자동 리로드 시도
    if (!Char->HasCurrentWeaponAmmo())
    {
        // 이미 리로드 중이면 대기 (루프는 계속 돌며 HasAmmo 체크)
        if (!Char->IsReloading())
        {
            const bool bReloadStarted = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_Input_Reload));
            if (!bReloadStarted)
            {
                // 낙하 중 등으로 리로드가 불가하면 무한 재시도하지 않고 연사를 끝낸다.
                EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
            }
        }

        return;
    }

    if (APlayerCharacter* PC = Cast<APlayerCharacter>(Char))
    {
        if (UPlayerAimComponent* Aim = PC->GetAimComponent())
            Aim->NotifyFired();
    }

    FireOnce(GetCurrentActorInfo());
}

void UGA_FireMachineGun::InputReleased(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    // 버튼 떼면 연사 종료
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_FireMachineGun::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FireLoopTimer);   // 타이머 정리 필수
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}