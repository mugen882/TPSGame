#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "WeaponRifle.generated.h"

class UNiagaraSystem;

/*
	라이플 클래스
	히트스캔 방식사용
*/

UCLASS()
class TPSGAME_API AWeaponRifle : public AWeaponBase
{
	GENERATED_BODY()

public:
	AWeaponRifle();

public:
	virtual FGameplayAttribute GetAmmoAttribute() const override;

protected:
	virtual bool FireInternal(const FVector& AimPoint, AController* InstigatorController, FHitResult& OutHit) override;

	// 판정 없는 탄착 예측 (클라 연출용)
	virtual bool TracePredictedImpactInternal(const FVector& AimPoint, FHitResult& OutHit) const override;

	// 퍼짐이 없어 클라 예측과 서버 판정이 정확히 일치한다.
	virtual bool SupportsPredictedImpact() const override { return true; }
};