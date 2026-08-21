#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttributeSet.h"
#include "WeaponBase.generated.h"

class USkeletalMeshComponent;
class UNiagaraSystem;
class UGameplayEffect;

// 히트 확정 신호 (히트마커 UI용) — 소유 캐릭터가 바인딩
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponHitConfirmed);

/*
	무기의 베이스 클래스
	무기마다 공통으로 사용되는 기능들 정의
*/

UCLASS(Abstract)
class TPSGAME_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	// 발사 시도. 실제 발사가 이뤄졌으면 true.
	// 탄약 검사/차감은 더 이상 여기서 하지 않는다 (GAS Cost GE가 처리).
	bool Fire(const FVector& AimPoint, AController* InstigatorController);

	/*
		이 무기가 소비하는 탄약 어트리뷰트.

		탄약이 UTPSAttributeSet으로 이관되면서, "어떤 탄약 풀을 쓰는가"를
		무기 자신이 알려주도록 했다.
	*/
	virtual FGameplayAttribute GetAmmoAttribute() const;

	FORCEINLINE bool IsInfiniteAmmo() const { return bInfiniteAmmo; }

	FVector GetMuzzleLocation() const;

	int32 GetMaxAmmo() const { return MaxAmmo; }

	void SetWeaponVisible(bool bVisible);

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponHitConfirmed OnHitConfirmed;

	void PlayImpactEffect(const FHitResult& Hit);

	FORCEINLINE const float GetFireInterval() { return FireInterval; }

	FORCEINLINE const float GetFireRange() { return FireRange; }

	FORCEINLINE void SetDamage(float InDamage) { Damage = InDamage; }
	FORCEINLINE const float GetDamage() { return Damage; }
	FORCEINLINE const float GetBaseDamage() const { return BaseDamage; }

	FORCEINLINE void  SetSpreadMultiplier(float M) { SpreadMultiplier = M; }
	FORCEINLINE const float GetSpreadMultiplier() const { return SpreadMultiplier; }

protected:
	virtual void BeginPlay() override;

	// 파생 클래스가 실제 발사 메커니즘 구현 (히트스캔/투사체 등).
	// 실제로 발사가 이뤄졌으면 true. false면 탄약을 소모하지 않는다
	virtual bool FireInternal(const FVector& AimPoint, AController* InstigatorController)
		PURE_VIRTUAL(AWeaponBase::FireInternal, return false;);

	void ShowMuzzleFlash();

	void PlayFireSound();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float BaseDamage = 1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");

	// 탄약 (무기별 독립)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 MaxAmmo = 30;


	// 발사 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float FireInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	bool bInfiniteAmmo = false;

	float Damage = 1.f;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	float FireRange = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Weapon|FX")
	TObjectPtr<UNiagaraSystem> ImpactEffect = nullptr;

	UPROPERTY(EditAnywhere, Category="Combat")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
	// 머즐 플래시 VFX
	UPROPERTY(EditAnywhere, Category = "Weapon|FX")
	TObjectPtr<UNiagaraSystem> MuzzleFlash = nullptr;

	UPROPERTY(EditAnywhere, Category = "Weapon|FX")
	float MuzzleFlashScale = 1.0f;

	UPROPERTY(EditAnywhere, Category="Sound")
    TObjectPtr<class USoundBase> FireSound;

	float SpreadMultiplier = 1.f;   // 기본 1 = 플레이어는 영향 없음
};