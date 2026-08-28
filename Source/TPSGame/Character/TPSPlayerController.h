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
	ATPSPlayerController();

	UTPSHUDWidget* GetHUDWidget() const { return HUDWidget; }

public:
    UFUNCTION(BlueprintCallable, Category="UI")
    void CloseQuitConfirm();

    UFUNCTION(BlueprintCallable, Category="UI")
    void ConfirmQuit();

public:
	/*
		치트 진입점. UTPSCheatManager가 호출한다.

		콘솔 명령은 입력한 머신에서 실행되므로, 원격 클라이언트의 치트가
		게임 상태를 바꾸려면 이 RPC를 반드시 거쳐야 한다.
	*/
	UFUNCTION(Server, Reliable)
	void ServerCheatSuicide();

	UFUNCTION(Server, Reliable)
	void ServerCheatDamageSelf(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerCheatHealSelf();

protected:
    virtual void SetupInputComponent() override;

	virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UTPSQuitConfirmWidget> QuitConfirmWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UTPSQuitConfirmWidget> QuitConfirmWidget;

	virtual void OnPossess(APawn* InPawn) override;

	// 클라이언트 경로 — OnPossess는 서버 전용이라 원격 클라는 여기로 온다.
	virtual void AcknowledgePossession(APawn* InPawn) override;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UTPSHUDWidget> HUDClass;

private:
	void OnPausePressed();

	// 서버/클라 공용 HUD 셋업
	void SetupHUDFor(APawn* InPawn);

	// 치트 대상 폰을 얻는다. 서버 전용. 유효하지 않거나 이미 죽었으면 nullptr.
	class ACommonCharacter* GetCheatTargetChecked(const TCHAR* CheatName);

private:
	UPROPERTY()
	TObjectPtr<UTPSHUDWidget> HUDWidget;
};
