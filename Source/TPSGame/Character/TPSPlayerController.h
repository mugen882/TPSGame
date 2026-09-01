#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TPSPlayerController.generated.h"

class UTPSHUDWidget;
class UTPSQuitConfirmWidget;
class UTPSGameOverWidget;

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

    /*
        게임오버 결과 화면 표시. 서버가 목숨 소진 시 호출한다.

        MatchState 복제만으로도 클라이언트가 종료를 알 수는 있지만,
        복제 도착과 위젯 준비 시점이 어긋날 수 있어 RPC로 직접 알린다.
        게임오버는 매치당 한 번뿐이라 RPC 비용도 문제가 되지 않는다.
    */
    UFUNCTION(Client, Reliable)
    void Client_ShowGameOver(bool bVictory);

    /*
        재시작 요청. 결과 화면의 버튼이 호출한다.

        클라이언트에서 직접 레벨을 다시 로드할 수 없으므로 서버를 거친다.
        서버는 게임오버 상태인지 재검증한 뒤에만 받아들인다.
    */
    UFUNCTION(Server, Reliable)
    void Server_RequestRestart();

    UFUNCTION(BlueprintCallable, Category="UI")
    void RequestRestart();

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

    // 재시작 시 결과 화면을 걷고 입력 모드를 게임으로 되돌린다.
    UFUNCTION(Client, Reliable)
    void Client_HideGameOver();

protected:
    virtual void SetupInputComponent() override;

	virtual void BeginPlay() override;

    virtual void OnPossess(APawn* InPawn) override;

    // 클라이언트 경로 — OnPossess는 서버 전용이라 원격 클라는 여기로 온다.
    virtual void AcknowledgePossession(APawn* InPawn) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UTPSQuitConfirmWidget> QuitConfirmWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UTPSQuitConfirmWidget> QuitConfirmWidget;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UTPSGameOverWidget> GameOverWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UTPSGameOverWidget> GameOverWidget;

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
