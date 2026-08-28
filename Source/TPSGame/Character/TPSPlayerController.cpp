#include "Character/TPSPlayerController.h"
#include "UI/TPSHUDWidget.h"
#include "PlayerCharacter.h"
#include "Common/TPSGameDefine.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/TPSQuitConfirmWidget.h"
#include "Cheat/TPSCheatManager.h"
#include "Character/CommonCharacter.h"
#include "AbilitySystem/TPSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Common/TPSLog.h"

ATPSPlayerController::ATPSPlayerController()
{
	CheatClass = UTPSCheatManager::StaticClass();
}

void ATPSPlayerController::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	/*
		전용 서버 구성에서는 클라이언트 쪽 CheatManager가 기본으로 생성되지 않는다.
		bForce로 강제 생성해야 원격 클라 콘솔에서 TPSSuicide를 칠 수 있다.
		서버는 RPC만 받으면 되므로 로컬 컨트롤러에만 붙인다.
	*/
	if (IsLocalController())
	{
		AddCheats(true);
	}
#endif
}

void ATPSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
    SetupHUDFor(InPawn);   // 리슨 서버 호스트 경로
}


void ATPSPlayerController::AcknowledgePossession(APawn* InPawn)
{
    Super::AcknowledgePossession(InPawn);
    SetupHUDFor(InPawn);   // 원격 클라이언트 경로
}

void ATPSPlayerController::SetupHUDFor(APawn* InPawn)
{
    // 화면에 그리는 UI는 로컬 컨트롤러에서만 생성
    if (!IsLocalController())
    {
        return;
    }

    APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(InPawn);
    if (!PlayerCharacter)
    {
        return;
    }

    // HUD는 단 한 번만 생성하고, 이후 빙의 때는 참조만 다시 연결한다.
    if (!HUDWidget && HUDClass)
    {
        HUDWidget = CreateWidget<UTPSHUDWidget>(this, HUDClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
        }
    }

    if (HUDWidget)
    {
        HUDWidget->InitFor(PlayerCharacter);
    }
}

void ATPSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Pause 액션은 일시정지 중에도 실행돼야 함
    InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this,
        &ATPSPlayerController::OnPausePressed).bExecuteWhenPaused = true;
}

void ATPSPlayerController::OnPausePressed()
{
    if (QuitConfirmWidget)
    {
        CloseQuitConfirm();   // 이미 떠 있으면 토글로 닫기
        return;
    }

    if (!QuitConfirmWidgetClass)
    {
        return;
    }

    QuitConfirmWidget = CreateWidget<UTPSQuitConfirmWidget>(this, QuitConfirmWidgetClass);
    if (!QuitConfirmWidget)
    {
        return;
    }

    QuitConfirmWidget->AddToViewport(100);   // HUD 위에

    SetPause(true);

    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(QuitConfirmWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;
}

void ATPSPlayerController::CloseQuitConfirm()
{
    if (QuitConfirmWidget)
    {
        QuitConfirmWidget->RemoveFromParent();
        QuitConfirmWidget = nullptr;
    }

    SetPause(false);
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
}

void ATPSPlayerController::ConfirmQuit()
{
    // 종료 전 일시정지 해제 (안 하면 종료 시퀀스가 멈추는 경우 있음)
    SetPause(false);

    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

// ─────────────────────────────────────────────────────────────
//  치트 (서버 실행)
// ─────────────────────────────────────────────────────────────

ACommonCharacter* ATPSPlayerController::GetCheatTargetChecked(const TCHAR* CheatName)
{
#if UE_BUILD_SHIPPING
	return nullptr;
#else
	ACommonCharacter* CheatTarget = Cast<ACommonCharacter>(GetPawn());
	if (!CheatTarget)
	{
		UE_LOG(TPSLog, Warning, TEXT("[SV] %s — 빙의된 폰이 없다"), CheatName);
		return nullptr;
	}

	if (!CheatTarget->GetAbilitySystemComponent())
	{
		UE_LOG(TPSLog, Warning, TEXT("[SV] %s — ASC가 없다"), CheatName);
		return nullptr;
	}

	if (CheatTarget->IsDead())
	{
		UE_LOG(TPSLog, Verbose, TEXT("[SV] %s — 이미 사망 상태라 무시"), CheatName);
		return nullptr;
	}

	return CheatTarget;
#endif
}

void ATPSPlayerController::ServerCheatSuicide_Implementation()
{
	ACommonCharacter* CheatTarget = GetCheatTargetChecked(TEXT("TPSSuicide"));
	if (!CheatTarget)
	{
		return;
	}

	/*
		Health를 0으로 덮어쓴다.

		Damage 메타 어트리뷰트를 거치지 않는 이유는 그쪽이 GE 애셋과
		SetByCaller를 요구하기 때문이다. 치트에 애셋 의존을 만들고 싶지 않다.

		BaseValue가 바뀌면 어트리뷰트 변경 델리게이트가 발화하고,
		ACommonCharacter::HandleHealthChanged가 Health <= 0을 관측해
		ServerHandleDeathAuthority -> GE_Dead 적용까지 평소 경로 그대로 탄다.
	*/
	CheatTarget->GetAbilitySystemComponent()->ApplyModToAttribute(
		UTPSAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, 0.0f);

	UE_LOG(TPSLog, Log, TEXT("[SV] TPSSuicide — %s 사망 처리"), *CheatTarget->GetName());
}

void ATPSPlayerController::ServerCheatDamageSelf_Implementation(float Amount)
{
	ACommonCharacter* CheatTarget = GetCheatTargetChecked(TEXT("TPSDamageSelf"));
	if (!CheatTarget || Amount <= 0.0f)
	{
		return;
	}

	// PreAttributeChange가 0 미만으로 내려가지 않게 막아준다.
	CheatTarget->GetAbilitySystemComponent()->ApplyModToAttribute(
		UTPSAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, -Amount);

	UE_LOG(TPSLog, Log, TEXT("[SV] TPSDamageSelf %.1f — %s"), Amount, *CheatTarget->GetName());
}

void ATPSPlayerController::ServerCheatHealSelf_Implementation()
{
	ACommonCharacter* CheatTarget = GetCheatTargetChecked(TEXT("TPSHealSelf"));
	if (!CheatTarget)
	{
		return;
	}

	UAbilitySystemComponent* ASC = CheatTarget->GetAbilitySystemComponent();
	const float MaxHealth = ASC->GetNumericAttribute(UTPSAttributeSet::GetMaxHealthAttribute());

	ASC->ApplyModToAttribute(
		UTPSAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, MaxHealth);

	UE_LOG(TPSLog, Log, TEXT("[SV] TPSHealSelf — %s 체력 %.1f"), *CheatTarget->GetName(), MaxHealth);
}
