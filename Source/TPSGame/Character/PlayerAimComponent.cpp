#include "PlayerAimComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/CommonCharacter.h"
#include "Weapon/WeaponBase.h"
#include "Common/TPSGameDefine.h"

UPlayerAimComponent::UPlayerAimComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPlayerAimComponent::SetupCameraRefs(USpringArmComponent* InBoom, UCameraComponent* InCamera)
{
    CameraBoom = InBoom;
    FollowCamera = InCamera;
}

void UPlayerAimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateCameraInterpolation(DeltaTime);
}

void UPlayerAimComponent::NotifyFired()
{
    bFireAimActive = true;
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(FireAimTimer, this, &UPlayerAimComponent::OnFireAimTimerEnd, FireAimHoldTime, false);
}

void UPlayerAimComponent::UpdateCameraInterpolation(float DeltaTime)
{
    const bool bCamZoom = bIsHoldingAim;
    const float TargetFOV = bCamZoom ? AimFOV : HipFOV;
    const float TargetArm = bCamZoom ? AimArmLength : HipArmLength;

    if (FollowCamera)
        FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed));

    if (CameraBoom)
    {
        const FVector TargetOffset = bCamZoom ? FVector(0, 75, 85) : FVector(0, 70, 90);
        CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetOffset, DeltaTime, AimInterpSpeed);
        CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaTime, AimInterpSpeed);
    }
}

FVector UPlayerAimComponent::ComputeAimPoint() const
{
    if (!FollowCamera || !GetWorld()) return FVector::ZeroVector;

    const FVector Start = FollowCamera->GetComponentLocation();
    const FVector End = Start + FollowCamera->GetForwardVector() * AimRange;

    FCollisionQueryParams Params;
    if (AActor* Owner = GetOwner())
    {
        Params.AddIgnoredActor(Owner);
        if (ACommonCharacter* C = Cast<ACommonCharacter>(Owner))
            if (AWeaponBase* W = C->GetCurrentWeapon())
                Params.AddIgnoredActor(W);
    }

    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Weapon, Params))
        return Hit.ImpactPoint;

    return End;
}

void UPlayerAimComponent::OnFireAimTimerEnd()
{
    bFireAimActive = false;
}