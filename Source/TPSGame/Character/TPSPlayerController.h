#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TPSPlayerController.generated.h"

class UTPSHUDWidget;
class UTPSQuitConfirmWidget;

/**
	HUD 소유/생성을 담당하는 플레이어 컨트롤러.
	캐릭터가 죽고 리스폰돼도 HUD 위젯은 컨트롤러 수명에 묶여 살아남는다.
 */
UCLASS()
class TPSGAME_API ATPSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UTPSHUDWidget* GetHUDWidget() const { return HUDWidget; }

public:
    UFUNCTION(BlueprintCallable, Category="UI")
    void CloseQuitConfirm();

    UFUNCTION(BlueprintCallable, Category="UI")
    void ConfirmQuit();

protected:
    virtual void SetupInputComponent() override;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UTPSQuitConfirmWidget> QuitConfirmWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UTPSQuitConfirmWidget> QuitConfirmWidget;

	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UTPSHUDWidget> HUDClass;

private:
	void OnPausePressed();

private:
	UPROPERTY()
	TObjectPtr<UTPSHUDWidget> HUDWidget;
};
