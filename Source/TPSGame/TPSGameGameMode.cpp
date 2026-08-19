// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSGameGameMode.h"
#include "Character/PlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Common/TPSLog.h"
#include "GameFramework/GameState.h"

ATPSGameGameMode::ATPSGameGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_PlayerCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_PlayerController"));
	if (PlayerControllerBPClass.Class != NULL)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}

	// 맵 이동 시 PlayerState/커넥션을 유지한다. 코옵에서 재접속 없이 맵을 넘기기 위해 필요.
	bUseSeamlessTravel = true;

	// 모든 플레이어가 들어올 때까지 기다리지 않고 바로 시작.
	// 웨이브 시작 대기가 필요해지는 M5에서 다시 검토한다.
	bDelayedStart = false;
}

void ATPSGameGameMode::BeginPlay()
{
	Super::BeginPlay();
	
}
