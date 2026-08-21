#include "AbilitySystem/GA/GA_SwapWeapon.h"
#include "Common/TPSGameplayTags.h"
#include "Common/TPSLog.h"
#include "Character/CommonCharacter.h"
#include "Weapon/WeaponManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

UGA_SwapWeapon::UGA_SwapWeapon()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 소유 클라에서 즉시 실행하고 서버가 같은 스펙을 이어 실행한다.
	// (기본값 LocalOnly로 두면 서버는 교체 사실을 전혀 알지 못한다)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 어빌리티가 활성화된 동안 State.Swapping 유지.
	// 발사/재장전/점프가 이 태그를 검사한다.
	ActivationOwnedTags.AddTag(TAG_State_Swapping);

	ActivationBlockedTags.AddTag(TAG_State_Dead);
	ActivationBlockedTags.AddTag(TAG_State_Reloading);
	ActivationBlockedTags.AddTag(TAG_State_Swapping);
}

bool UGA_SwapWeapon::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	ACommonCharacter* Character = ActorInfo ? Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		return false;
	}

	UWeaponManagerComponent* WM = Character->GetWeaponManager();
	if (!WM)
	{
		return false;
	}

	if (TargetWeaponType == EWeaponType::None || TargetWeaponType >= EWeaponType::MAX)
	{
		return false;
	}

	// 이미 들고 있는 무기면 활성화하지 않는다.
	if (WM->GetCurrentWeaponType() == TargetWeaponType)
	{
		return false;
	}

	// 소유하지 않은 무기면 활성화하지 않는다.
	// 서버에서도 동일하게 검사되므로 클라가 조작한 요청은 여기서 걸러진다.
	if (WM->GetWeapon(TargetWeaponType) == nullptr)
	{
		return false;
	}

	return true;
}

void UGA_SwapWeapon::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* /*TriggerEventData*/)
{
	bSwapPerformed = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACommonCharacter* Character = ActorInfo ? Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 교체 중에는 발사를 중단한다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer FireTags;
		FireTags.AddTag(TAG_Ability_Fire);
		ASC->CancelAbilities(&FireTags);
	}

	UE_LOG(TPSLog, Verbose, TEXT("%s GA_SwapWeapon 시작 — %d -> %d"),
		*TPSNetDebug::TPSNetPrefix(Character),
		(int32)(Character->GetWeaponManager() ? Character->GetWeaponManager()->GetCurrentWeaponType() : EWeaponType::None),
		(int32)TargetWeaponType);

	// 몽타주가 없으면 즉시 교체하고 끝낸다.
	if (!SwapMontage)
	{
		PerformSwap();
		FinishAbility(false);
		return;
	}

	// 노티파이가 보내는 교체 타이밍 이벤트를 먼저 구독한다.
	UAbilityTask_WaitGameplayEvent* EventTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_Event_WeaponSwap);
	if (EventTask)
	{
		EventTask->EventReceived.AddDynamic(this, &UGA_SwapWeapon::OnSwapEventReceived);
		EventTask->ReadyForActivation();
	}

	/*
		PlayMontageAndWait는 몽타주를 시뮬레이티드 프록시까지 복제해준다.
		기존의 AnimInstance->Montage_Play()는 로컬 재생이라
		다른 플레이어 화면에서는 교체 동작이 보이지 않았다.
	*/
	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SwapMontage);
	if (!MontageTask)
	{
		PerformSwap();
		FinishAbility(false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGA_SwapWeapon::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_SwapWeapon::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_SwapWeapon::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_SwapWeapon::OnMontageCancelled);
	MontageTask->ReadyForActivation();
}

void UGA_SwapWeapon::OnSwapEventReceived(FGameplayEventData /*Payload*/)
{
	PerformSwap();
}

void UGA_SwapWeapon::OnMontageFinished()
{
	/*
		노티파이가 오지 않았을 때의 폴백.
	*/
	PerformSwap();
	FinishAbility(false);
}

void UGA_SwapWeapon::OnMontageCancelled()
{
	// 중단된 경우 교체하지 않는다. 무기는 원래 것을 유지한다.
	FinishAbility(true);
}

void UGA_SwapWeapon::PerformSwap()
{
	if (bSwapPerformed)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACommonCharacter* Character = ActorInfo ? Cast<ACommonCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		return;
	}

	UWeaponManagerComponent* WM = Character->GetWeaponManager();
	if (!WM)
	{
		return;
	}

	// 사망 중이면 교체를 마무리하지 않는다.
	if (Character->IsDead())
	{
		return;
	}

	bSwapPerformed = true;

	/*
		서버 / 소유 클라 양쪽에서 호출된다.

		서버: 복제 변수 CurrentWeaponType을 쓴다 -> 모든 클라에 전파
		소유 클라: 같은 값을 로컬로 미리 써서 즉시 반응(예측). 서버 값이 도착하면
		           REPNOTIFY_Always 덕분에 OnRep이 반드시 실행되어 최종 확정된다.
	*/
	WM->ApplyWeaponType(TargetWeaponType);

	UE_LOG(TPSLog, Verbose, TEXT("%s GA_SwapWeapon 교체 완료 -> %d"),
		*TPSNetDebug::TPSNetPrefix(Character), (int32)TargetWeaponType);
}

void UGA_SwapWeapon::FinishAbility(bool bWasCancelled)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		true, bWasCancelled);
}
