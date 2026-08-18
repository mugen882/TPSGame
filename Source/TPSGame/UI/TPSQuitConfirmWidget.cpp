#include "UI/TPSQuitConfirmWidget.h"
#include "Character/TPSPlayerController.h"
#include "Components/Button.h"

void UTPSQuitConfirmWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ConfirmButton) { ConfirmButton->OnClicked.AddDynamic(this, &UTPSQuitConfirmWidget::HandleConfirm); }
    if (CancelButton) { CancelButton->OnClicked.AddDynamic(this, &UTPSQuitConfirmWidget::HandleCancel); }

    SetIsFocusable(true);
}

void UTPSQuitConfirmWidget::HandleConfirm()
{
    if (ATPSPlayerController* PC = GetOwningPlayer<ATPSPlayerController>())
    {
        PC->ConfirmQuit();
    }
}

void UTPSQuitConfirmWidget::HandleCancel()
{
    if (ATPSPlayerController* PC = GetOwningPlayer<ATPSPlayerController>())
    {
        PC->CloseQuitConfirm();
    }
}