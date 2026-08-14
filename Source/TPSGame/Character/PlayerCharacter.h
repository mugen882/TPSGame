#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CommonCharacter.h"
#include "InputActionValue.h"
#include "Weapon/WeaponManagerComponent.h"
#include "PlayerCharacter.generated.h"

class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class AWeaponBase;
class UPlayerAimComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDamaged);

// 데미지를 입었을때 공격받은 방향을 전달
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDamagedDirectional, float, HitAngle);

/*
	플레이어 캐릭터 클래스
*/

UCLASS()
class TPSGAME_API APlayerCharacter : public ACommonCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	virtual FVector GetAimPoint() const override;

	UFUNCTION(BlueprintPure, Category="Aim")
	bool IsAiming() const;

	UPlayerAimComponent* GetAimComponent() { return AimComponent; }

	virtual void OnReloadFinished() override;

	// 플레이어는 발사가 즉발(ActivateAbility의 FireOnce)이므로 발사 몽타주의 노티파이는 무시한다.
	virtual void OnFireNotify() override {}

	virtual void PossessedBy(AController* NewController) override;

public:
	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnPlayerDamaged OnPlayerDamaged;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnPlayerDamagedDirectional OnPlayerDamagedDir;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aim")
	TObjectPtr<UPlayerAimComponent> AimComponent;

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void OnDamaged(float InDamage) override;
	virtual void HandleDeath() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	EWeaponType CurrentWeaponType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player")
	float PlayerHP = 1.f;

private:
	void DoJump();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartAim();
	void StopAim();
	void StartFire();
	void StopFire();

	void OnReloadInput();
	void ChangeRifle();
	void ChangeRocketLauncher();
	void ChangeMachineGun();
	
	void OnFireAimTimerEnd();

	virtual void EquipInitialWeapon() override;

	void TryFire();

private:
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* FireAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* AimAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ReloadAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ChangeRifleAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ChangeRocketLauncherAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ChangeMachineGunAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category="Combat", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> HitReactMontage;

	bool bFireInputHeld = false;
};
