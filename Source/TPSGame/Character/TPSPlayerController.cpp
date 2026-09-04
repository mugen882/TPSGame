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
#include "UI/TPSGameOverWidget.h"
#include "TPSGameGameMode.h"

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

void ATPSPlayerController::DiscardWidgetsFromPreviousWorld()
{
    UWorld* CurrentWorld = GetWorld();

    // 위젯을 만든 월드가 그대로면 유지한다. (같은 맵 안에서의 리스폰)
    if (WidgetOwningWorld.Get() == CurrentWorld)
    {
        return;
    }

    /*
        여기까지 왔다면 트래블로 월드가 교체된 것이다.

        SeamlessTravel은 PlayerController를 새 월드로 데려가지만
        뷰포트의 위젯은 전부 걷어낸다. UPROPERTY로 잡고 있는 위젯 포인터는
        GC되지 않아 그대로 살아 있으므로, "이미 있으니 만들지 않는다" 식의
        생성 가드는 여기서 조용히 오작동한다.
        위젯은 화면에 없고 코드는 다시 만들지 않아 UI가 통째로 사라진다.
    */
    const bool bHadAnyWidget = (HUDWidget != nullptr)
        || (GameOverWidget != nullptr)
        || (QuitConfirmWidget != nullptr);

    if (HUDWidget)
    {
        HUDWidget->RemoveFromParent();
        HUDWidget = nullptr;
    }

    if (GameOverWidget)
    {
        GameOverWidget->RemoveFromParent();
        GameOverWidget = nullptr;
    }

    if (QuitConfirmWidget)
    {
        QuitConfirmWidget->RemoveFromParent();
        QuitConfirmWidget = nullptr;
    }

    if (bHadAnyWidget)
    {
        UE_LOG(TPSLog, Log, TEXT("[CL] 월드 교체 감지 — 이전 월드의 위젯을 폐기한다"));

        // 결과 화면/일시정지에서 바꿔둔 입력 모드가 새 월드까지 따라올 수 있다.
        RestoreGameInputMode();
    }

    WidgetOwningWorld = nullptr;
}

void ATPSPlayerController::RestoreGameInputMode()
{
    SetShowMouseCursor(false);
    SetInputMode(FInputModeGameOnly());
}

void ATPSPlayerController::SetupHUDFor(APawn* InPawn)
{
    // 화면에 그리는 UI는 로컬 컨트롤러에서만 생성
    if (!IsLocalController())
    {
        return;
    }

    /*
        폰 판정보다 먼저 수행한다.
        트래블 정리는 어떤 폰에 빙의하든 반드시 한 번은 돌아야 한다.
    */
    DiscardWidgetsFromPreviousWorld();

    // 같은 월드 안에서 리스폰한 경우에도 결과 화면이 남아 있을 수 있다.
    if (GameOverWidget)
    {
        GameOverWidget->RemoveFromParent();
        GameOverWidget = nullptr;
        RestoreGameInputMode();
    }

    APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(InPawn);
    if (!PlayerCharacter)
    {
        return;
    }

    /*
        HUD는 월드당 한 번 생성한다.

        같은 월드 안에서의 리스폰은 기존 위젯을 재사용하고 InitFor로 참조만
        다시 연결한다. 트래블로 월드가 바뀐 경우는 위에서 이미 폐기됐으므로
        여기서 새로 만들어지고, NativeConstruct가 다시 돌면서 새 GameState의
        델리게이트에 정상적으로 바인딩된다.
    */
    if (!HUDWidget && HUDClass)
    {
        HUDWidget = CreateWidget<UTPSHUDWidget>(this, HUDClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
            WidgetOwningWorld = GetWorld();

            UE_LOG(TPSLog, Log, TEXT("[CL] HUD 생성 — World=%s"), *GetNameSafe(GetWorld()));
        }
        else
        {
            UE_LOG(TPSLog, Error, TEXT("[CL] HUD 생성 실패 — HUDClass=%s"), *GetNameSafe(HUDClass));
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
    WidgetOwningWorld = GetWorld();

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
    RestoreGameInputMode();
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

void ATPSPlayerController::Client_ShowGameOver_Implementation(bool bVictory)
{
    if (!IsLocalController()) return;

    if (!GameOverWidget && GameOverWidgetClass)
    {
        GameOverWidget = CreateWidget<UTPSGameOverWidget>(this, GameOverWidgetClass);
        if (GameOverWidget)
        {
            GameOverWidget->AddToViewport(10);   // HUD 위에 그린다
            WidgetOwningWorld = GetWorld();
        }
    }

    if (!GameOverWidget) return;

    GameOverWidget->SetResult(bVictory);
    GameOverWidget->SetVisibility(ESlateVisibility::Visible);

    /*
        결과 화면에서는 마우스로 버튼을 눌러야 하므로 입력 모드를 바꾼다.
        게임 입력은 이미 캐릭터가 죽어 DisableInput 상태다.
    */
    SetShowMouseCursor(true);
    SetInputMode(FInputModeUIOnly());
}

void ATPSPlayerController::RequestRestart()
{
    // 로컬에서 버튼을 누르면 서버로 넘긴다. 레벨 재로드는 서버만 할 수 있다.
    Server_RequestRestart();
}

void ATPSPlayerController::Server_RequestRestart_Implementation()
{
    if (ATPSGameGameMode* GM = GetWorld()->GetAuthGameMode<ATPSGameGameMode>())
    {
        GM->RequestRestartMatch();
    }
}

void ATPSPlayerController::Client_HideGameOver_Implementation()
{
    if (!IsLocalController()) return;

    if (GameOverWidget)
    {
        GameOverWidget->RemoveFromParent();
        GameOverWidget = nullptr;
    }

    /*
        입력 모드를 게임으로 되돌린다.

        SeamlessTravel에서는 PlayerController가 살아남으므로 UIOnly 상태가
        새 맵까지 따라온다. 결과 화면을 띄울 때 바꾼 것은 반드시 되돌려야 한다.
    */
    RestoreGameInputMode();
}
