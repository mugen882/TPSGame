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
	virtual bool FireInternal(const FVector& AimPoint, AController* InstigatorController) override;

	// 판정 없는 탄착 연출 (로컬 예측)
	virtual void FireCosmeticInternal(const FVector& AimPoint) override;
};