// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TPSGameGameMode.generated.h"

/*
	AGameModeBase -> AGameMode 승격

	AGameModeBase에는 MatchState 개념이 없어 멀티플레이 매치 흐름
	(WaitingToStart / InProgress / WaitingPostMatch)을 다룰 수 없다.
	M3의 리스폰, M5의 웨이브 진행/게임오버 처리가 전부 이 상태 머신 위에 올라가므로
	지금 미리 올려둔다.
*/

UCLASS(minimalapi)
class ATPSGameGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATPSGameGameMode();

protected:
	virtual void BeginPlay() override;
};
