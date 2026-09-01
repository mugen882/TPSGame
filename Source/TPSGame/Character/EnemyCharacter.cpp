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
#include "Common/TPSLog.h"
#include "Net/UnrealNetwork.h"
#include "TPSGameGameMode.h"

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

    /*
        데디케이티드 서버는 체력바를 그리지 않는다.
    */
    if (GetNetMode() == NM_DedicatedServer)
    {
        if (HealthBarComponent)
        {
            HealthBarComponent->SetVisibility(false);
            HealthBarComponent->SetComponentTickEnabled(false);
        }
        return;
    }

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
        AttrSet->InitializeAttributes(GetEnemyBaseHP() * HealthMul);
    }
}

void AEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEnemyCharacter, bHoldingAim);
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

    const FVector AimPoint = PendingTarget->GetActorLocation() + FVector(0, 0, 50);

    /*
        적은 서버에서만 실행되므로 조준점 왕복이 필요 없다.
        연출은 GameplayCue로 실행해 모든 클라이언트에 전파한다.
        (예측 키가 없으므로 사격자 예외 없이 전원이 재생한다)
    */
    FHitResult Hit;
    GetCurrentWeapon()->FireAuthoritative(AimPoint, GetController(), Hit);

    ExecuteFireCue();
    if (Hit.bBlockingHit)
    {
        ExecuteImpactCue(Hit.ImpactPoint, Hit.ImpactNormal);
    }

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

    /*
        시체 정리는 서버에서만 예약한다.

        Destroy()는 복제되므로 클라이언트 사본도 함께 정리된다.
        클라에서 호출하면 자기만 먼저 사라져 서버와 어긋난다.
    */
    if (HasAuthority())
    {
        SetLifeSpan(CorpseLifeSpan);

        // 승리 판정은 서버가 한다. 남은 적 수를 다시 세게 한다.
        if (ATPSGameGameMode* GM = GetWorld()->GetAuthGameMode<ATPSGameGameMode>())
        {
            GM->NotifyEnemyDied(this);
        }
    }
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

void AEnemyCharacter::StopCombat()
{
    if (!HasAuthority()) return;

    if (AAIController* AICon = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AICon->GetBrainComponent())
        {
            Brain->StopLogic(TEXT("MatchEnded"));
        }
    }

    /*
        진행 중인 어빌리티를 끊는다.

        BT를 멈춰도 이미 활성화된 발사 어빌리티는 몽타주가 끝날 때까지 살아 있고,
        중간의 OnFireNotify가 발사를 수행한다. 승리 직후 적의 마지막 총알이
        플레이어를 죽이면 결과가 뒤집힐 수 있다.
    */
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->CancelAllAbilities();
    }

    // BT 정지만으로는 관성으로 미끄러진다.
    GetCharacterMovement()->StopMovementImmediately();

    PendingTarget = nullptr;
}
