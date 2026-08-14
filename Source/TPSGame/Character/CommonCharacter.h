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
	virtual void PossessedBy(AController* NewController) override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	void InitAbilities();

	bool TargetIsDead(AActor* Actor);

	virtual void HandleDeath();

	virtual void HandleHealthChanged(const struct FOnAttributeChangeData& Data);

	virtual void OnDamaged(float InDamage)
		PURE_VIRTUAL(ACommonCharacter::OnDamaged, );

	float GetAimRange() const { return AimRange; }

	virtual void EquipInitialWeapon()
		PURE_VIRTUAL(ACommonCharacter::EquipInitialWeapon, );

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UWeaponManagerComponent> WeaponManager;

	UPROPERTY()
	AActor* LastDamageInstigator = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	UAbilitySystemComponent* AbilitySystemComponent;

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
	UTPSAttributeSet* AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<class UGameplayAbility>> DefaultAbilities;

	bool bAbilitiesGranted = false;
};
