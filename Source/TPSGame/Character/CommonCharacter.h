#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Weapon/WeaponManagerComponent.h"
#include "CommonCharacter.generated.h"

class UAbilitySystemComponent;
class UTPSAttributeSet;
class AWeaponBase;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHitConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, EWeaponType, WeaponType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAmmoReloaded);
// 탄약 변경 델리게이트 (현재 탄약, 최대 탄약을 전달)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, int32, NewCurrentAmmo, int32, NewMaxAmmo);

/*
	공용 캐릭터 클래스
	적 / 플레이어의 공통 로직을 담고 있음
	UAbilitySystemComponent, UTPSAttributeSet을 들고있음

	[네트워크 노트]
	ASC는 캐릭터에 그대로 둔다. 적(AEnemyCharacter)이 같은 베이스를 쓰는데
	PlayerState가 없기 때문에, ASC를 PlayerState로 올리면 플레이어/적 경로가 갈라진다.
	웨이브 디펜스 코옵에서는 리스폰 시 어트리뷰트를 다시 초기화하면 되므로
	PlayerState 이관의 이점이 크지 않다고 판단.
*/

UCLASS()
class TPSGAME_API ACommonCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACommonCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void OnFireNotify()
		PURE_VIRTUAL(ACommonCharacter::OnFireNotify);

	virtual FVector GetAimPoint() const
		PURE_VIRTUAL(ACommonCharacter::GetAimPoint, return FVector::ZeroVector;);

	virtual void OnReloadFinished()
		PURE_VIRTUAL(ACommonCharacter::OnReloadFinished, );

	UFUNCTION(BlueprintCallable, Category = "Character")
	bool IsDead() const;

	bool IsReloading() const;
	bool IsSwapping() const;

	// 대상에게 데미지 GE 적용 (무기·투사체 공용)
	static void ApplyDamageEffect(
		AActor* Target,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float Damage,
		AActor* SourceActor);

	void NotifyDamageFrom(AActor* DamageInstigator);

	TObjectPtr<UAnimMontage> GetFireMontage() { return FireMontage; }

	FORCEINLINE AWeaponBase* GetCurrentWeapon()
	{
		return WeaponManager ? WeaponManager->GetCurrentWeapon() : nullptr;
	}

	UWeaponManagerComponent* GetWeaponManager() { return WeaponManager; }

	void SpawnWeapons();

	void BroadcastAmmo();

	/*
		현재 무기의 탄약 조회.

		탄약이 UTPSAttributeSet으로 이관되었으므로 무기 액터가 아니라 ASC에서 읽는다.
		호출부(TryFire, GA_Reload, 머신건 연사 루프, BT)가 무기 종류를 몰라도 되도록
		무기의 GetAmmoAttribute()를 경유한다.
	*/
	int32 GetCurrentWeaponAmmo();
	bool  HasCurrentWeaponAmmo();
	bool  IsCurrentWeaponAmmoFull();

	// 서버 전용. 소유한 모든 무기의 탄약을 MaxAmmo로 초기화한다.
	void InitAmmoAttributes();

	FORCEINLINE UTPSAttributeSet* GetAttributeSet() { return AttributeSet; }

public:
	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnHitConfirmed OnHitConfirmed;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnWeaponChanged OnWeaponChanged;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnAmmoReloaded OnAmmoReloaded;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnAmmoChanged OnAmmoChanged;

protected:
	// 서버 전용 초기화 경로
	virtual void PossessedBy(AController* NewController) override;

	// 클라이언트 초기화 경로 — PlayerState가 복제된 시점
	virtual void OnRep_PlayerState() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/*
		ASC ActorInfo 초기화 + 어트리뷰트 델리게이트 바인딩

		서버(PossessedBy) / 플레이어 클라(OnRep_PlayerState) / AI 프록시(BeginPlay)
		세 경로에서 모두 호출되며, 중복 호출에 안전하도록 작성했다.
	*/
	void InitAbilityActorInfoAndBind(const TCHAR* CallSite);

	// 서버 전용. DefaultAbilities 부여
	void GrantDefaultAbilities();

	bool TargetIsDead(AActor* Actor);

	virtual void HandleDeath();

	virtual void HandleHealthChanged(const struct FOnAttributeChangeData& Data);

	// 탄약 어트리뷰트 변경 → 탄약 UI 갱신 (예측 차감과 서버 확정 양쪽에서 발화)
	void HandleAmmoChanged(const struct FOnAttributeChangeData& Data);

	virtual void OnDamaged(float InDamage)
		PURE_VIRTUAL(ACommonCharacter::OnDamaged, );

	float GetAimRange() const { return AimRange; }

	virtual void EquipInitialWeapon()
		PURE_VIRTUAL(ACommonCharacter::EquipInitialWeapon, );

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UWeaponManagerComponent> WeaponManager;

	UPROPERTY()
	TObjectPtr<AActor> LastDamageInstigator = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

private:
	UFUNCTION()
	void HandleWeaponHitConfirmed();

	UFUNCTION()
	void ChangeWeapon(EWeaponType WeaponType);

private:
	const float AimRange = 10000.f;

	UPROPERTY(EditAnywhere, Category="Combat")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY()
	TObjectPtr<UTPSAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<class UGameplayAbility>> DefaultAbilities;

	bool bAbilitiesGranted = false;

	// 델리게이트 중복 바인딩 방지
	bool bAttributeDelegatesBound = false;
};
