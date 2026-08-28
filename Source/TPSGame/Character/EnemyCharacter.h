#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CommonCharacter.h"
#include "Character/EnemyCombatComponent.h"
#include "Common/TPSGameDefine.h"
#include "EnemyCharacter.generated.h"

class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UNiagaraSystem;
class UEnemyCombatComponent;
class UWidgetComponent;
class UEnemyCombatProfile;

/*
	적캐릭터 클래스
*/

UCLASS()
class TPSGAME_API AEnemyCharacter : public ACommonCharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetFireTarget(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void OnFireNotify() override;

	void HandleDeath() override;

	// 사망 후 시체가 남아 있는 시간 (서버에서만 적용)
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float CorpseLifeSpan = 5.0f;

	void FireOnce(AActor* Target);

	void SetAiming(bool bNew) { bIsAiming = bNew; }

	void SetHoldingAim(bool bNew) { bHoldingAim = bNew; }

	UFUNCTION(BlueprintPure)
	bool IsHoldingAim() { return bHoldingAim; }

	float GetLastHitTime() const { return LastHitTime; }

	FORCEINLINE float GetRecentDamageAccum() const { return RecentDamageAccum; }

	FORCEINLINE float GetDamageDecayTime() const { return DamageDecayTime; }

	UEnemyCombatComponent* GetCombatComponent() const { return CombatComponent; }

	UFUNCTION(BlueprintPure, Category="Combat")
	UEnemyCombatProfile* GetCombatProfile() { return CombatComponent->GetCurrentProfile(); }

	FGameplayAbilitySpecHandle GetFireAbilitySpecHandle() { return CombatComponent->GetFireAbilitySpecHandle(); }

	UFUNCTION(BlueprintPure, Category="Combat")
	float GetAttackRange() const { return CombatComponent->GetAttackRange(); }

	void SwitchToRandomProfile(bool bAvoidRepeat = true) { CombatComponent->SwitchToRandomProfile(bAvoidRepeat); }

	virtual FVector GetAimPoint() const override;

	float GetEnemyBaseHP() { return EnemyBaseHP; }

	virtual void OnReloadFinished() override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAiming = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy")
	float EnemyBaseHP = ENEMY_BASE_HP;   // 기본 체력 (난이도 배수 적용 전)

private:
	void PlayHitFlash();
	void ClearHitFlash();

	virtual void OnDamaged(float InDamage) override;

	virtual void EquipInitialWeapon() override;

private:
	UPROPERTY(EditAnywhere, Category="Combat")
    TObjectPtr<UMaterialInterface> HitFlashMaterial;

	UPROPERTY(VisibleAnywhere, Category="UI")
	TObjectPtr<UWidgetComponent> HealthBarComponent;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	UPROPERTY(EditAnywhere, Category="Combat")
	TSubclassOf<class UGameplayAbility> FireAbilityClass;

	UPROPERTY(VisibleAnywhere, Category="Combat")
	TObjectPtr<UEnemyCombatComponent> CombatComponent;

	FTimerHandle HitFlashTimer;

	UPROPERTY()
	TObjectPtr<AActor> PendingTarget = nullptr;  // 발사 예정 타깃 저장

	UPROPERTY(Replicated)
	bool bHoldingAim = false;

	float LastHitTime = 0.f;

	float RecentDamageAccum = 0.f;   // 최근 누적 피해
	float DamageDecayTime = 4.f;   // 최근 피해 누적 리셋 시간
};
