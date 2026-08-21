#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Common/TPSGameTypes.h"
#include "GA_SwapWeapon.generated.h"

class UAnimMontage;

/*
    무기 교체 어빌리티

    기존에는 UWeaponManagerComponent::EquipWeapon()이 몽타주 재생, 상태 태그,
    발사 취소를 직접 처리했다. 그 방식은 두 가지 문제가 있었다.

      1) AddLooseGameplayTag(State.Swapping)은 복제되지 않는다.
      2) AnimInstance->Montage_Play()는 로컬 재생이라 다른 클라이언트에 보이지 않는다.

    어빌리티로 옮기면 둘 다 GAS가 해결해준다.
      - ActivationOwnedTags는 어빌리티가 실행되는 모든 머신(소유 클라 + 서버)에 적용된다.
        상태 태그를 검사하는 곳이 정확히 그 두 머신이므로 이걸로 충분하다.
      - UAbilityTask_PlayMontageAndWait는 몽타주를 시뮬레이티드 프록시까지 복제한다.
      - NetExecutionPolicy = LocalPredicted 이므로 소유 클라는 즉시 반응하고
        서버가 같은 스펙을 이어서 실행한다. 별도의 RPC가 필요 없다.

    교체할 무기 종류는 TargetWeaponType으로 지정한다.
    무기 종류별로 BP 자식을 하나씩 만들고 각각 InputTag를 다르게 주면,
    "어떤 스펙이 활성화됐는가" 자체가 무기 종류를 실어 나르므로
    커스텀 RPC로 enum을 보낼 필요가 없다.
*/
UCLASS()
class TPSGAME_API UGA_SwapWeapon : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SwapWeapon();

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	// 몽타주 노티파이가 보낸 Event.WeaponSwap 수신 — 실제 교체 타이밍
	UFUNCTION()
	void OnSwapEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	// 실제 교체 수행. 이벤트와 몽타주 종료 양쪽에서 불릴 수 있으므로 1회만 동작한다.
	void PerformSwap();

	void FinishAbility(bool bWasCancelled);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Swap")
	EWeaponType TargetWeaponType = EWeaponType::None;

	UPROPERTY(EditDefaultsOnly, Category = "Swap")
	TObjectPtr<UAnimMontage> SwapMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag InputTag;

	// 이벤트와 몽타주 종료가 중복 호출되는 것을 막는다.
	bool bSwapPerformed = false;
};
