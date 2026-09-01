#include "PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Blueprint/UserWidget.h" 
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "TPSGameGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Weapon/WeaponBase.h"
#include "Common/TPSGameTypes.h"
#include "CommonCharacter.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSGameplayTags.h"
#include "Weapon/WeaponManagerComponent.h"
#include "Character/PlayerAimComponent.h"
#include "Common/TPSGameDefine.h"
#include "AbilitySystem/TPSAttributeSet.h"
#include "Common/TPSLog.h"

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

    /*
        플레이어만 Mixed로 승격

        Mixed: 소유 클라이언트에게는 GameplayEffect를 온전히 복제하고,
               나머지 클라에게는 GameplayTag와 GameplayCue만 복제한다.

        주의: Mixed는 ASC의 OwnerActor가 유효한 커넥션에 소유되어 있어야 한다.
              여기서는 PlayerController가 캐릭터를 소유하므로 조건을 만족한다.
    */
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    }
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

    /*
        연사는 입력 홀드 반복으로 구현한다.

        매 틱 활성화를 시도하되 쿨다운 GE가 발사 간격을 강제하므로
        간격보다 빨리 나가지 않는다.
    */
    if (bFireInputHeld && IsLocallyControlled())
    {
        TryFire();
    }
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
    Super::HandleDeath();   // 공통 래그돌

    // 입력 차단. 시뮬레이티드 프록시에서는 GetController()가 null이므로 자연히 건너뛴다.
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
    }

    /*
        리스폰은 서버의 GameMode가 관장한다.

        이전에는 OpenLevel로 레벨을 통째로 리로드했다. 코옵에서 한 명이 죽었다고
        전원이 맵을 다시 로드하면 안 되므로 RestartPlayer 기반으로 교체했다.
    */
    if (!HasAuthority())
    {
        return;
    }

    if (!GetWorld())
    {
        UE_LOG(TPSLog, Error, TEXT("[SV] APlayerCharacter::HandleDeath() 월드가 유효하지 않음"));
	    return;
    }

    if (ATPSGameGameMode* GM = GetWorld()->GetAuthGameMode<ATPSGameGameMode>())
    {
        GM->NotifyPlayerDied(this);
    }
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

/*
    무기 교체 입력

    직접 EquipWeapon을 부르지 않고 GA_SwapWeapon을 활성화한다.
    TryActivateAbilitiesByTag는 LocalPredicted 어빌리티에 대해
    소유 클라 실행 + 서버로 ServerTryActivateAbility 전송을 함께 처리하므로
    별도의 RPC가 필요 없다.

    무기 종류는 "어떤 스펙이 활성화됐는가"로 전달된다.
    (BP 자식마다 TargetWeaponType과 InputTag를 다르게 설정)
*/
void APlayerCharacter::TryActivateSwapAbility(const FGameplayTag& InputTag)
{
    if (!AbilitySystemComponent) return;
    if (IsDead() || IsReloading() || IsSwapping()) return;

    AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(InputTag));
}

void APlayerCharacter::ChangeRifle()
{
    TryActivateSwapAbility(TAG_Input_Swap_Rifle);
}

void APlayerCharacter::ChangeRocketLauncher()
{
    TryActivateSwapAbility(TAG_Input_Swap_RocketLauncher);
}

void APlayerCharacter::ChangeMachineGun()
{
    TryActivateSwapAbility(TAG_Input_Swap_MachineGun);
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
        // 서버에서만 기록. 클라는 OnRep_CurrentWeaponType으로 따라온다.
        if (HasAuthority())
        {
            GetWeaponManager()->ApplyWeaponType(EWeaponType::Rifle);
        }
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
    if (CurrentWeapon && !HasCurrentWeaponAmmo())
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