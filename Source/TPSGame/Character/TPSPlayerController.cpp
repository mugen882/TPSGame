#include "Character/TPSPlayerController.h"
#include "UI/TPSHUDWidget.h"
#include "PlayerCharacter.h"
#include "Common/TPSGameDefine.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/TPSQuitConfirmWidget.h"

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