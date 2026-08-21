#include "GA_Reload.h"
#include "Character/PlayerCharacter.h"
#include "Weapon/WeaponBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSGameplayTags.h"
#include "Common/TPSLog.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UGA_Reload::UGA_Reload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	/*
		LocalPredicted로 바꾼 이유

		기본값(LocalOnly)에서는 플레이어의 재장전이 소유 클라에서만 실행되어
		서버가 재장전 사실을 전혀 알지 못했다. 탄약이 서버 권위 어트리뷰트로
		옮겨간 지금은 서버도 이 어빌리티를 실행해야 탄약을 채울 수 있다.

		몽타주는 양쪽에서 예측 재생되고, 실제 탄약 충전은 서버에서만 수행한다.
	*/
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationOwnedTags.AddTag(TAG_State_Reloading);

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Input_Reload);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_Reloading);
	ActivationBlockedTags.AddTag(TAG_State_Swapping);
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
		!Weapon || !ReloadMontage || Character->IsCurrentWeaponAmmoFull())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ReloadMontage);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGA_Reload::OnReloadMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_Reload::OnReloadMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_Reload::OnReloadMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_Reload::OnReloadMontageCancelled);
	MontageTask->ReadyForActivation();
}

void UGA_Reload::OnReloadMontageCancelled()
{
	// 중단되면 탄약을 채우지 않는다.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UGA_Reload::OnReloadMontageCompleted()
{
	FinishReload();

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

	ACommonCharacter* Character = ActorInfo ? Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	// 어빌리티 종료 (State.Reloading 해제)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);

	if (!Character) return;

	Character->OnAmmoReloaded.Broadcast();

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

void UGA_Reload::FinishReload()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACommonCharacter* Character = ActorInfo ? Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	/*
		탄약 충전은 서버 전용.

		Cost(발사)와 달리 재장전은 예측하지 않는다.

		GE를 쓰지 않고 SetNumericAttributeBase를 직접 부르는 이유는,
		"MaxAmmo로 채운다"는 동작에 GE가 더해주는 것이 없기 때문이다.
	*/
	if (!Character->HasAuthority()) return;

	AWeaponBase* Weapon = Character->GetCurrentWeapon();
	if (!Weapon) return;

	const FGameplayAttribute Attr = Weapon->GetAmmoAttribute();
	if (!Attr.IsValid()) return;

	ASC->SetNumericAttributeBase(Attr, (float)Weapon->GetMaxAmmo());

	UE_LOG(TPSLog, Verbose, TEXT("%s 재장전 완료 — %d발"),
		*TPSNetDebug::TPSNetPrefix(Character), Weapon->GetMaxAmmo());
}
