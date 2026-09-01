// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TPSGameGameMode.generated.h"

/*
	AGameModeBase -> AGameMode 승격

	AGameModeBase에는 MatchState 개념이 없어 멀티플레이 매치 흐름
	(WaitingToStart / InProgress / WaitingPostMatch)을 다룰 수 없다.
	리스폰이 이 상태 머신 위에 올라가고, 웨이브 진행/게임오버도 같은 자리에 붙는다.
*/

UCLASS(minimalapi)
class ATPSGameGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATPSGameGameMode();

	/*
		플레이어 사망 통지. 서버에서만 호출된다.

		즉시 리스폰: RespawnDelay 후 옛 폰을 파괴하고 RestartPlayer로
		PlayerStart에 새 폰을 스폰한다. ASC가 캐릭터에 붙어 있으므로
		새 폰 = 새 ASC = 깨끗한 어트리뷰트/태그 상태가 되어 부활 처리가 단순하다.

		목숨이 남아 있으면 리스폰하고, 0이 되면 게임오버로 넘어간다.
	*/
	void NotifyPlayerDied(class APlayerCharacter* DeadCharacter);

	/*
		재시작 요청. 클라이언트의 재시작 버튼이 서버 RPC를 거쳐 여기로 온다.

		게임오버 상태에서만 받아들인다. 진행 중에 호출되면 무시한다 —
		클라이언트가 임의로 매치를 리셋할 수 있으면 안 된다.
	*/
	void RequestRestartMatch();

	/*
		적 사망 통지. 서버에서만 호출된다.

		살아 있는 적이 0이 되면 승리로 매치를 끝낸다.
		패배(목숨 소진)와 같은 배관을 쓰고 결과만 다르다.
	*/
	void NotifyEnemyDied(class AEnemyCharacter* DeadEnemy);

protected:
	virtual void BeginPlay() override;

	// 사망 후 리스폰까지의 대기 시간. 래그돌을 보여주는 시간이기도 하다.
	UPROPERTY(EditDefaultsOnly, Category = "Respawn")
	float RespawnDelay = 3.0f;

	/*
		팀 공용 목숨.

		개인별이 아니라 공용인 이유는 코옵 협동감 때문이다. 개인별로 두면
		먼저 죽은 사람만 오래 관전하게 된다. 공용이면 "우리 남은 목숨"이
		공통 자원이 되어 서로의 죽음이 팀 전체에 영향을 준다.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 TotalLives = 2;

private:
	// 목숨을 소진해 게임오버로 전환한다. 서버 전용.
	// 매치 종료. bVictory로 결과 화면 문구가 갈린다. 승패 모두 이 경로를 탄다.
	void EndMatchWithResult(bool bVictory);

	/*
		매치가 끝난 뒤 살아 있는 적의 AI를 정지시킨다.

		결과 화면이 떠 있는데 적이 계속 추격하고 사격하면 몰입이 깨지고,
		플레이어는 입력이 막힌 상태라 대응할 수도 없다.
	*/
	void StopAllEnemyAI();

	// 월드를 순회해 살아 있는 적 수를 센다. 서버 전용.
	int32 CountLivingEnemies() const;

	// 남은 적 수를 GameState에 반영하고, 0이면 승리 처리한다.
	void RefreshEnemyCount();

	class ATPSGameState* GetTPSGameState() const;

private:
	void RespawnPlayer(TWeakObjectPtr<AController> ControllerPtr, TWeakObjectPtr<APawn> DeadPawnPtr);
};
