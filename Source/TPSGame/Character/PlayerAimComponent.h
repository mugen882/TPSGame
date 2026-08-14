#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "PlayerAimComponent.generated.h"

class USpringArmComponent;
class UCameraComponent;

/*
	플레이어 조준 관련 컴포넌트
*/

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TPSGAME_API UPlayerAimComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPlayerAimComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void SetupCameraRefs(USpringArmComponent* InBoom, UCameraComponent* InCamera);

    void SetHoldingAim(bool bNew) { bIsHoldingAim = bNew; }
    void NotifyFired();

    UFUNCTION(BlueprintPure, Category="Aim")
    bool IsAiming() const { return bIsHoldingAim || bFireAimActive; }

    FVector ComputeAimPoint() const;

    void UpdateCameraInterpolation(float DeltaTime);

protected:
    void OnFireAimTimerEnd();

    UPROPERTY() TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY() TObjectPtr<UCameraComponent> FollowCamera;

private:
    UPROPERTY(EditAnywhere, Category="Aim")
    float HipFOV = 90.0f;
    UPROPERTY(EditAnywhere, Category="Aim")
    float AimFOV = 60.0f;
    UPROPERTY(EditAnywhere, Category="Aim")
    float HipArmLength = 300.0f;
    UPROPERTY(EditAnywhere, Category="Aim")
    float AimArmLength = 120.0f;
    UPROPERTY(EditAnywhere, Category="Aim")
    float AimInterpSpeed = 10.0f;
    UPROPERTY(EditAnywhere, Category="Aim")
    float AimRange = 5000.0f;
    UPROPERTY(EditAnywhere, Category="Aim")
    float FireAimHoldTime = 0.3f;

	bool bIsHoldingAim = false;     // 플레이어가 조준 버튼을 누르고 있는 상태
	bool bFireAimActive = false;    // 총 발사 후 잠시 조준 상태 유지
    FTimerHandle FireAimTimer;
};