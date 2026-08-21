#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "WeaponMachineGun.generated.h"

class UNiagaraSystem;
class UGameplayEffect;

/*
	머신건 클래스
	총 발사 할때 탄퍼짐 관련 처리가 있음
	히스트캔방식 사용
*/

UCLASS()
class TPSGAME_API AWeaponMachineGun : public AWeaponBase
{
	GENERATED_BODY()
public:
	AWeaponMachineGun();

public:
	virtual FGameplayAttribute GetAmmoAttribute() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool FireInternal(const FVector& AimPoint, AController* InstigatorController) override;

	UFUNCTION()
	void AmmoReload();

	UPROPERTY(EditAnywhere, Category="Spread")
	float BaseSpreadAngle = 1.0f;        // 평소(첫 발) 퍼짐

	UPROPERTY(EditAnywhere, Category="Spread")
	float MaxSpreadAngle = 8.0f;         // 연사 시 도달하는 최대 퍼짐

	UPROPERTY(EditAnywhere, Category="Spread")
	float SpreadPerShot = 1.0f;          // 한 발마다 증가량

	UPROPERTY(EditAnywhere, Category="Spread")
	float SpreadRecoveryRate = 6.0f;     // 초당 회복량(발사 멈췄을 때)

	float CurrentSpread = 0.f;           // 누적 상태 (런타임)
	float TimeSinceLastShot = 0.f;       // 마지막 발사 후 경과
};