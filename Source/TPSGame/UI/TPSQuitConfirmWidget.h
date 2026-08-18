#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TPSQuitConfirmWidget.generated.h"

UCLASS()
class TPSGAME_API UTPSQuitConfirmWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UButton> ConfirmButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<class UButton> CancelButton;

    UFUNCTION()
    void HandleConfirm();

    UFUNCTION()
    void HandleCancel();
};