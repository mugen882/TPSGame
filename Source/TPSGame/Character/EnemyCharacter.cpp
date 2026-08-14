#include "Character/EnemyCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerCharacter.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Common/TPSGameTypes.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponRifle.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "UI/HealthBarWidget.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/EnemyAIController.h"
#include "Subsystem/DifficultySubsystem.h"
#include "Components/WidgetComponent.h"
#include "AbilitySystem/TPSAttributeSet.h"

AEnemyCharacter::AEnemyCharacter()
{
    HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
    HealthBarComponent->SetupAttachment(RootComponent);
    HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);   // 항상 카메라 향함 (billboard)
    HealthBarComponent->SetDrawSize(FVector2D(120.f, 16.f));
    HealthBarComponent->SetRelativeLocation(FVector(0, 0, 100.f)); // 머리 위 높이

    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;

    CombatComponent = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("CombatComponent"));
}

void AEnemyCharacter::BeginPlay()
{
    if (HasAuthority() && CombatComponent)
        CombatComponent->GrantFireAbilities();

    Super::BeginPlay();

    if (HealthBarComponent)
    {
        if (HealthBarWidgetClass)
        {
            HealthBarComponent->SetWidgetClass(HealthBarWidgetClass);
        }
        HealthBarComponent->InitWidget();   // Screen 스페이스 위젯 즉시 생성

        if (UHealthBarWidget* HBar = Cast<UHealthBarWidget>(HealthBarComponent->GetUserWidgetObject()))
        {
            HBar->SetOwningCharacter(this);
        }
    }
}

void AEnemyCharacter::EquipInitialWeapon()
{
    SwitchToRandomProfile();
}

void AEnemyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 죽으면 래그돌이 도는 중이므로 조준 회전을 멈춘다.
    if (IsDead()) return;

    if (PendingTarget == nullptr) return;

    const FVector ToTarget = PendingTarget->GetActorLocation() - GetActorLocation();
    const FRotator LookYaw(0.f, ToTarget.Rotation().Yaw, 0.f);  // yaw만
    const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), LookYaw, DeltaTime, 5.f);
    SetActorRotation(NewRot);
}

void AEnemyCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

    float HealthMul = 1.f;
    if (const UGameInstance* GI = GetGameInstance())
        if (const UDifficultySubsystem* Diff = GI->GetSubsystem<UDifficultySubsystem>())
            HealthMul = Diff->EnemyHealthMul;

    if (UTPSAttributeSet* AttrSet = GetAttributeSet())
    {
        AttrSet->InitializeAttributes(GetBaseHealth() * HealthMul);
    }
}

void AEnemyCharacter::SetFireTarget(AActor* Target)
{
    if (IsDead() || !Target || !GetCurrentWeapon() || TargetIsDead(Target)) return;
    PendingTarget = Target;
}

void AEnemyCharacter::FireOnce(AActor* Target)
{
    if (!Target)
    {
        return;
    }

    // 타깃을 향해 조준 회전
    const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
    FRotator LookRot = ToTarget.Rotation();
    LookRot.Pitch = 0.f;
    LookRot.Roll = 0.f;
    SetActorRotation(LookRot);

    // 컨트롤러 회전도 맞춰둠
    if (AController* C = GetController())
    {
        C->SetControlRotation(FRotator(0.f, LookRot.Yaw, 0.f));
    }

    // 발사 어빌리티 발동. 쿨다운 중이면 내부에서 알아서 실패하므로
    // BTTask 가 매 틱 호출해도 실제 발사는 무기 쿨다운 간격대로 나감
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (ASC && FireAbilityClass)
    {
        ASC->TryActivateAbilityByClass(FireAbilityClass);
    }
}

void AEnemyCharacter::PlayHitFlash()
{
    if (HitFlashMaterial)
    {
        GetMesh()->SetOverlayMaterial(HitFlashMaterial);
        GetWorldTimerManager().SetTimer(
            HitFlashTimer, this, &AEnemyCharacter::ClearHitFlash, 0.1f, false);
    }
}

void AEnemyCharacter::ClearHitFlash()
{
    GetMesh()->SetOverlayMaterial(nullptr);
}

void AEnemyCharacter::OnDamaged(float InDamage)
{
    PlayHitFlash();

    const UWorld* World = GetWorld();
    const float Now = World ? World->GetTimeSeconds() : LastHitTime;
    if (Now - LastHitTime > DamageDecayTime)   // 직전 피격에서 충분히 지났으면 누적 초기화
        RecentDamageAccum = 0.f;

    RecentDamageAccum += InDamage;
    LastHitTime = Now;

    AActor* Attacker = LastDamageInstigator;
    if (!Attacker || Attacker == this) return;

    if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
    {
        AICon->NotifyDamagedBy(Attacker);
    }
}

void AEnemyCharacter::OnFireNotify()
{
    if (!PendingTarget || !GetCurrentWeapon() || IsDead())
    {
        PendingTarget = nullptr;
        return;
    }

    if (IsReloading() == true)
    {
        return;
    }

    // 탄약 없으면 발사 대신 리로드
    if (GetCurrentWeapon() && !GetCurrentWeapon()->HasAmmo())
    {
        AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_Input_Reload));
        return;
    }

    const FVector AimPoint = PendingTarget->GetActorLocation() + FVector(0, 0, 50);
    GetCurrentWeapon()->Fire(AimPoint, GetController());

    PendingTarget = nullptr;
}

void AEnemyCharacter::HandleDeath()
{
    Super::HandleDeath();

    PendingTarget = nullptr;

    // AI 정지
    if (AAIController* AICon = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AICon->GetBrainComponent())
        {
            Brain->StopLogic(TEXT("Dead"));
        }
    }

    HealthBarComponent->SetVisibility(false);
}

FVector AEnemyCharacter::GetAimPoint() const
{
    // 자기 위치에서 전방으로 쏨 (조준은 AI가 회전으로 맞춤)
    return GetActorLocation() + GetActorForwardVector() * GetAimRange();
}

void AEnemyCharacter::OnReloadFinished()
{
    // 지금은 특별한 처리 없음.
}
