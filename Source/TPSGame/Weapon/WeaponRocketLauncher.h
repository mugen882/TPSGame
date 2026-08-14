#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "WeaponRocketLauncher.generated.h"

class AProjectile;

/*
	로켓 런처 클래스
	프로젝타일 방식
*/

UCLASS()
class TPSGAME_API AWeaponRocketLauncher : public AWeaponBase
{
	GENERATED_BODY()
public:
	AWeaponRocketLauncher();
	virtual void BeginPlay() override;

	TSubclassOf<AProjectile> GetProjectileClass() { return ProjectileClass; }

protected:
	virtual void FireInternal(const FVector& AimPoint, AController* InstigatorController) override;

	UPROPERTY(EditAnywhere, Category = "Weapon|Rocket")
	TSubclassOf<AProjectile> ProjectileClass;
};