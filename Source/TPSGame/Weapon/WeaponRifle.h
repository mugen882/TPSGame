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

protected:
	virtual bool FireInternal(const FVector& AimPoint, AController* InstigatorController) override;
};