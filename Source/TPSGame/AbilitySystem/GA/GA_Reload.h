#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_Reload.generated.h"

class UAnimMontage;

UCLASS()
class TPSGAME_API UGA_Reload : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Reload();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	// UAbilityTask_PlayMontageAndWait 델리게이트 (파라미터 없음)
	UFUNCTION()
	void OnReloadMontageCompleted();

	UFUNCTION()
	void OnReloadMontageCancelled();

	// 서버에서만 탄약을 채운다.
	void FinishReload();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Reload")
	UAnimMontage* ReloadMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag InputTag;
};