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

		FireAuthoritative : 서버 전용. 트레이스 / 투사체 스폰 / 데미지 적용.
		PlayFireCosmetic  : 실행되는 모든 머신. 머즐 플래시 / 발사음 / 임팩트 연출.

		이전에는 Fire() 하나가 둘 다 했기 때문에, 클라이언트가 발사할 때
		클라이언트에서 트레이스하고 데미지까지 적용하는 클라 권위 구조였다.
	*/
	bool FireAuthoritative(const FVector& AimPoint, AController* InstigatorController);
	void PlayFireCosmetic(const FVector& AimPoint);

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
	// 서버에서만 호출된다. 데미지 적용은 여기서만 일어난다.
	virtual bool FireInternal(const FVector& AimPoint, AController* InstigatorController)
		PURE_VIRTUAL(AWeaponBase::FireInternal, return false;);

	/*
		판정 없는 연출 전용 트레이스. 데미지를 적용하지 않는다.

		호출된 머신에서만 실행된다. 클라이언트는 서버 왕복을 기다리지 않고
		즉시 탄착 피드백을 받고, 서버는 자기 권위 트레이스를 따로 돌린다.

		TODO(M2b-2): 적 발사는 서버에서만 호출되므로 데디케이티드 서버에서는
					 아무에게도 보이지 않는다. 또 플레이어 발사 연출도 남의
					 화면에는 전달되지 않는다. GameplayCue로 이관해야 한다.
	*/
	virtual void FireCosmeticInternal(const FVector& /*AimPoint*/) {}

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