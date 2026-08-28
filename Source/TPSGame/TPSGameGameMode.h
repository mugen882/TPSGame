// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TPSGameGameMode.generated.h"

/*
	AGameModeBase -> AGameMode 승격

	AGameModeBase에는 MatchState 개념이 없어 멀티플레이 매치 흐름
	(WaitingToStart / InProgress / WaitingPostMatch)을 다룰 수 없다.
	리스폰, M5의 웨이브 진행/게임오버 처리가 전부 이 상태 머신 위에 올라가므로
	지금 미리 올려둔다.
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

		TODO(M5): 전원 사망 시 게임오버. 지금은 무조건 리스폰한다.
	*/
	void NotifyPlayerDied(class APlayerCharacter* DeadCharacter);

protected:
	virtual void BeginPlay() override;

	// 사망 후 리스폰까지의 대기 시간. 래그돌을 보여주는 시간이기도 하다.
	UPROPERTY(EditDefaultsOnly, Category = "Respawn")
	float RespawnDelay = 3.0f;

private:
	void RespawnPlayer(TWeakObjectPtr<AController> ControllerPtr, TWeakObjectPtr<APawn> DeadPawnPtr);
};
