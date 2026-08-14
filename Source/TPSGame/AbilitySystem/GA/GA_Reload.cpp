#include "GA_Reload.h"
#include "Character/PlayerCharacter.h"
#include "Weapon/WeaponBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSGameplayTags.h"

UGA_Reload::UGA_Reload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	ActivationOwnedTags.AddTag(TAG_State_Reloading);

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Input_Reload);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_Reloading);
}

void UGA_Reload::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer FireTags;
		FireTags.AddTag(TAG_Ability_Fire); // 발사 어빌리티들에 공통으로 붙인 태그
		ASC->CancelAbilities(&FireTags);
	}

	ACommonCharacter* Character = ActorInfo ? Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	AWeaponBase* Weapon = Character->GetCurrentWeapon();
	if (Character->GetCharacterMovement()->IsFalling() ||
		!Weapon || !ReloadMontage || Weapon->IsAmmoFull())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float Duration = AnimInstance->Montage_Play(ReloadMontage);
	if (Duration <= 0.f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UGA_Reload::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, ReloadMontage);
}

void UGA_Reload::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

	if (bInterrupted)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACommonCharacter* Character = Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
		return;

	Character->OnAmmoReloaded.Broadcast();

	if (AWeaponBase* Weapon = Character->GetCurrentWeapon())
	{
		Weapon->Reload();
		Character->BroadcastAmmo();
	}

	// 어빌리티 종료 (State.Reloading 해제)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);

	// 다음 틱에 발사 재개 — 태그 해제가 확실히 반영된 뒤
	if (UWorld* World = Character->GetWorld())
	{
		TWeakObjectPtr<ACommonCharacter> WeakChar = Character;
		World->GetTimerManager().SetTimerForNextTick([WeakChar]()
			{
				if (WeakChar.IsValid())
					WeakChar->OnReloadFinished();
			});
	}
}