#pragma once

#include "CoreMinimal.h"
#include "Common/TPSGameTypes.h"
#include "EnemyCombatProfile.generated.h"

class AWeaponBase;
class UGameplayAbility;

/*
    적 전투 프로필
    사용하는 무기별 정보를 가지고 있음
*/

UCLASS()
class UEnemyCombatProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere)
    EWeaponType WeaponType = EWeaponType::Rifle;
    UPROPERTY(EditAnywhere)
    TSubclassOf<AWeaponBase> WeaponClass;
    UPROPERTY(EditAnywhere)
    TSubclassOf<UGameplayAbility> FireAbility; // GA_FireRifle / GA_FireRocketLauncher / GA_FireMachineGun
    UPROPERTY(EditAnywhere)
    float AttackRange = 1000.f;

	float GetPreferredRange() const {	return AttackRange * PreferredRangeRatio; }

    // 교전 사거리 밴드 (Combat 상태에서 이 거리대를 유지)
    UPROPERTY(EditAnywhere)
	float PreferredRangeRatio = 0.8f;
    UPROPERTY(EditAnywhere)
	float RangeTolerance = 250.f;   // PreferredRange ± Tolerance 범위 유지

    UPROPERTY(EditAnywhere)
    bool bRequiresLineOfSight = true;

    // 로켓: 느린 투사체 리드 조준
    UPROPERTY(EditAnywhere)
    bool bLeadTarget = false;
    UPROPERTY(EditAnywhere)
    float ProjectileSpeed = 3000.f;

    // 머신건: 제압 사격 버스트
    UPROPERTY(EditAnywhere)
    bool bBurstFire = false;
    UPROPERTY(EditAnywhere)
    float BurstDuration = 1.5f;
    UPROPERTY(EditAnywhere)
    float BurstCooldown = 1.0f;
    UPROPERTY(EditAnywhere)
	float SafeRangeMultiplier = 1.f; // 로켓: 스플래시 자해 방지 최소거리 배율. MinSafeRange = PreferredRange * SafeRangeMultiplier

private:
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};