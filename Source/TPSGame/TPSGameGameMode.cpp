// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSGameGameMode.h"
#include "Character/PlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Common/TPSLog.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerController.h"

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
	// 웨이브 시스템을 넣을 때 다시 검토한다 (전원 입장 후 시작이 필요해진다).
	bDelayedStart = false;
}

void ATPSGameGameMode::BeginPlay()
{
	Super::BeginPlay();
	
}

void ATPSGameGameMode::NotifyPlayerDied(APlayerCharacter* DeadCharacter)
{
	if (!DeadCharacter) return;

	AController* DeadController = DeadCharacter->GetController();
	if (!DeadController)
	{
		// 이미 빙의가 풀린 경우(중복 통지 등)
		return;
	}

	UE_LOG(TPSLog, Verbose, TEXT("[SV] %s 사망 — %.1f초 후 리스폰"),
		*DeadCharacter->GetName(), RespawnDelay);

	/*
		약참조로 캡처한다.

		RespawnDelay 동안 플레이어가 접속을 끊으면 컨트롤러와 폰이 사라진다.
		강참조로 잡으면 GC를 막고, 원시 포인터로 잡으면 댕글링이 된다.
	*/
	TWeakObjectPtr<AController> ControllerPtr(DeadController);
	TWeakObjectPtr<APawn> DeadPawnPtr(DeadCharacter);

	FTimerHandle RespawnTimer;
	GetWorldTimerManager().SetTimer(RespawnTimer,
		[this, ControllerPtr, DeadPawnPtr]()
		{
			RespawnPlayer(ControllerPtr, DeadPawnPtr);
		},
		RespawnDelay, false);
}

void ATPSGameGameMode::RespawnPlayer(TWeakObjectPtr<AController> ControllerPtr, TWeakObjectPtr<APawn> DeadPawnPtr)
{
	AController* PC = ControllerPtr.Get();
	if (!PC)
	{
		// 대기 중 접속 종료
		return;
	}

	/*
		옛 폰을 먼저 파괴한다.

		Destroy()는 빙의를 자동으로 해제하므로 별도 UnPossess가 필요 없다.
		파괴하지 않으면 래그돌이 월드에 영구히 남고, 복제 대상으로도 계속 남는다.
	*/
	if (APawn* DeadPawn = DeadPawnPtr.Get())
	{
		DeadPawn->Destroy();
	}

	/*
		RestartPlayer가 PlayerStart를 찾아 새 폰을 스폰하고 빙의시킨다.
		그 결과 PossessedBy가 다시 돌면서
		  - InitAbilityActorInfoAndBind
		  - GrantDefaultAbilities
		  - InitializeAttributes (체력 복원)
		  - SpawnWeapons / InitAmmoAttributes
		가 전부 새로 실행된다. HUD는 AcknowledgePossession에서 재바인딩된다.
	*/
	RestartPlayer(PC);

	UE_LOG(TPSLog, Verbose, TEXT("[SV] %s 리스폰 완료"), *PC->GetName());
}
