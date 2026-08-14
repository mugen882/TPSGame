#pragma once
#include "CoreMinimal.h"
#include "AbilitySystem/GA/GA_FireBase.h"
#include "GA_FireMachineGun.generated.h"

/*
    머신건 발사 클래스
    연사 관련 처리
*/

UCLASS()
class TPSGAME_API UGA_FireMachineGun : public UGA_FireBase
{
    GENERATED_BODY()
public:
    UGA_FireMachineGun();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void InputReleased(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override { /* 연사는 타이머가 제어, 쿨다운 GE 미적용 */ }

private:
    void FireLoop();

private:
    FTimerHandle FireLoopTimer;
};