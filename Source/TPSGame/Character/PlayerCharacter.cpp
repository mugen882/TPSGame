#include "PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Blueprint/UserWidget.h" 
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Weapon/WeaponBase.h"
#include "Common/TPSGameTypes.h"
#include "CommonCharacter.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSGameplayTags.h"
#include "Weapon/WeaponManagerComponent.h"
#include "Character/PlayerAimComponent.h"
#include "Common/TPSGameDefine.h"

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    bUseControllerRotationYaw = true;   // 캐릭터가 컨트롤러(카메라) 방향을 봄

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->bOrientRotationToMovement = false;  // 이동 방향으로 안 돎
    }

    GetCharacterMovement()->JumpZVelocity = 700.f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
    GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

    AimComponent = CreateDefaultSubobject<UPlayerAimComponent>(TEXT("AimComponent"));
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

    if (AimComponent)
        AimComponent->SetupCameraRefs(CameraBoom, FollowCamera);
}

void APlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (AimComponent)
        AimComponent->UpdateCameraInterpolation(DeltaTime);
}

void APlayerCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::DoJump);
        EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

        EIC->BindAction(FireAction, ETriggerEvent::Started, this, &APlayerCharacter::StartFire);
        EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopFire);

        EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);

        EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);

        // 조준 입력
        EIC->BindAction(AimAction, ETriggerEvent::Started, this, &APlayerCharacter::StartAim);
        EIC->BindAction(AimAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopAim);

        EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &APlayerCharacter::OnReloadInput);

        EIC->BindAction(ChangeRifleAction, ETriggerEvent::Started, this, &APlayerCharacter::ChangeRifle);
        EIC->BindAction(ChangeRocketLauncherAction, ETriggerEvent::Started, this, &APlayerCharacter::ChangeRocketLauncher);
        EIC->BindAction(ChangeMachineGunAction, ETriggerEvent::Started, this, &APlayerCharacter::ChangeMachineGun);
    }
}

void APlayerCharacter::OnDamaged(float InDamage)
{
    // 비네트 (방향 없는 일반 피격)
    OnPlayerDamaged.Broadcast();

    // 방향성 비네트
    if (LastDamageInstigator)
    {
        FVector ToAttacker = (LastDamageInstigator->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        float ForwardDot = FVector::DotProduct(GetActorForwardVector(), ToAttacker);
        float RightDot = FVector::DotProduct(GetActorRightVector(), ToAttacker);
        float HitAngle = FMath::Atan2(RightDot, ForwardDot) * (180.f / PI);
        OnPlayerDamagedDir.Broadcast(HitAngle);
    }

    if (HitReactMontage && !IsReloading() && !IsSwapping())
    {
        if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
        {
            if (!AnimInst->Montage_IsPlaying(HitReactMontage))
            {
                AnimInst->Montage_Play(HitReactMontage);
            }
        }
    }
}

void APlayerCharacter::HandleDeath()
{
    Super::HandleDeath();   // 공통 래그돌 + State.Dead 태그

    // 입력 차단
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
    }

    // 3초 후 레벨 재시작
    FTimerHandle RestartTimer;
    GetWorldTimerManager().SetTimer(RestartTimer, [this]()
        {
            UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()));
        }, 3.0f, false);
}

void APlayerCharacter::StartAim()
{
    if (AimComponent)
    {
        AimComponent->SetHoldingAim(true);
    }
}

void APlayerCharacter::StopAim()
{
    if (AimComponent)
    {
        AimComponent->SetHoldingAim(false);
    }
}

void APlayerCharacter::StartFire()
{
    bFireInputHeld = true;
    TryFire();
}

void APlayerCharacter::StopFire()
{
    bFireInputHeld = false;

    if (!AbilitySystemComponent) return;

    if (GetWeaponManager() == nullptr) return;

    if (AimComponent == nullptr) return;

    AbilitySystemComponent->CancelAbilities(&GetWeaponManager()->GetWeaponFireTag());
}

void APlayerCharacter::OnReloadInput()
{
    if (!AbilitySystemComponent) return;
    if (IsSwapping()) return;

    AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_Input_Reload));
}

void APlayerCharacter::ChangeRifle()
{
    if (GetWeaponManager() == nullptr) return;

    GetWeaponManager()->EquipWeapon(EWeaponType::Rifle);
}

void APlayerCharacter::ChangeRocketLauncher()
{
    if (GetWeaponManager() == nullptr) return;

    GetWeaponManager()->EquipWeapon(EWeaponType::RocketLauncher);
}

void APlayerCharacter::ChangeMachineGun()
{
    if (GetWeaponManager() == nullptr) return;

    GetWeaponManager()->EquipWeapon(EWeaponType::MachineGun);
}

void APlayerCharacter::DoJump()
{
    if (IsReloading() || IsSwapping()) return;

    Jump();
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void APlayerCharacter::EquipInitialWeapon()
{
    if (GetWeaponManager() == nullptr) return;

    if (WeaponManager->GetWeapons().Num() > 0)
    {
        GetWeaponManager()->DoEquipWeapon(EWeaponType::Rifle);
    }
}

FVector APlayerCharacter::GetAimPoint() const
{
    if (AimComponent == nullptr)
        return FVector::ZeroVector;

    return AimComponent->ComputeAimPoint();
}

bool APlayerCharacter::IsAiming() const
{
    if (AimComponent == nullptr)
        return false;

    return AimComponent->IsAiming();
}

void APlayerCharacter::OnReloadFinished()
{
    if (bFireInputHeld)
        TryFire();
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

    if (UTPSAttributeSet* AttrSet = GetAttributeSet())
    {
        AttrSet->InitializeAttributes(PlayerHP);
    }
}

void APlayerCharacter::TryFire()
{
    if (!AbilitySystemComponent) return;

    if (IsReloading() || IsSwapping()) return;

    if (WeaponManager == nullptr) return;

    if (GetWeaponManager() == nullptr) return;

    AWeaponBase* CurrentWeapon = WeaponManager->GetCurrentWeapon();
    // 탄약 없으면 발사 대신 리로드
    if (CurrentWeapon && !CurrentWeapon->HasAmmo())
    {
        AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_Input_Reload));
        return;
    }

    // 발사가 실제로 됐을 때만 자동조준
    bool bActivated = AbilitySystemComponent->TryActivateAbilitiesByTag(GetWeaponManager()->GetWeaponFireTag());
    if (bActivated)
    {
        if (AimComponent)
            AimComponent->NotifyFired();
    }
}