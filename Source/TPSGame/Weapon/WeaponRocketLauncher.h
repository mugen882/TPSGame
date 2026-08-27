#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "WeaponRocketLauncher.generated.h"

class AProjectile;

/*
	로켓 런처 클래스
	프로젝타일 방식

	투사체 무기라 발사 시점에 탄착이 없다. 폭발 연출은 AProjectile 멀티캐스트가 담당한다.
	SupportsPredictedImpact는 기본값(false) 유지.
*/

UCLASS()
class TPSGAME_API AWeaponRocketLauncher : public AWeaponBase
{
	GENERATED_BODY()
public:
	AWeaponRocketLauncher();
	virtual void BeginPlay() override;

	TSubclassOf<AProjectile> GetProjectileClass() { return ProjectileClass; }

public:
	virtual FGameplayAttribute GetAmmoAttribute() const override;

protected:
	virtual bool FireInternal(const FVector& AimPoint, AController* InstigatorController, FHitResult& OutHit) override;

	UPROPERTY(EditAnywhere, Category = "Weapon|Rocket")
	TSubclassOf<AProjectile> ProjectileClass;
};