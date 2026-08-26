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

	/*
		발사가 권위 경로와 연출 경로로 나뉘었다.

		FireAuthoritative   : 서버 전용. 트레이스 / 투사체 스폰 / 데미지 적용.
		TracePredictedImpact: 클라 전용. 판정 없이 탄착 지점만 예측한다.

		둘 다 명중 지점을 OutHit으로 돌려준다. 연출(GameplayCue)은 호출부인
		어빌리티가 실행하는데, Cue를 예측 키와 함께 실행해야 중복 재생이
		방지되기 때문이다. 무기는 예측 키를 알지 못하므로 판정만 담당한다.
	*/
	bool FireAuthoritative(const FVector& AimPoint, AController* InstigatorController, FHitResult& OutHit);
	bool TracePredictedImpact(const FVector& AimPoint, FHitResult& OutHit) const;

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


	// GameplayCue가 파라미터(위치/노멀)로 호출하는 버전
	void PlayImpactEffectAt(const FVector& Location, const FVector& Normal);

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
	// 서버에서만 호출된다. 데미지 적용은 여기서만 일어난다.
	// 명중했으면 OutHit을 채운다. 투사체 무기는 채우지 않는다.
	virtual bool FireInternal(const FVector& AimPoint, AController* InstigatorController, FHitResult& OutHit)
		PURE_VIRTUAL(AWeaponBase::FireInternal, return false;);

	/*
		판정 없는 예측 트레이스. 데미지를 적용하지 않는다.

		클라이언트가 서버 왕복을 기다리지 않고 즉시 탄착 연출을 띄우기 위한 것.
		투사체 무기는 클라가 투사체를 스폰하면 안 되므로 오버라이드하지 않는다.
	*/
	virtual bool TracePredictedImpactInternal(const FVector& /*AimPoint*/, FHitResult& /*OutHit*/) const { return false; }

public:
	// GameplayCue에서 호출된다.
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