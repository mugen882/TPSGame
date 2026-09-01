// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSGameGameMode.h"
#include "Character/PlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Common/TPSLog.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerController.h"
#include "Game/TPSGameState.h"
#include "Character/TPSPlayerController.h"
#include "Character/EnemyCharacter.h"
#include "EngineUtils.h"

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
	GameStateClass = ATPSGameState::StaticClass();

	bUseSeamlessTravel = true;

	// 모든 플레이어가 들어올 때까지 기다리지 않고 바로 시작.
	// 웨이브 시스템을 넣을 때 다시 검토한다 (전원 입장 후 시작이 필요해진다).
	bDelayedStart = false;
}

void ATPSGameGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (ATPSGameState* GS = GetTPSGameState())
	{
		GS->SetRemainingLives(TotalLives);
	}

	/*
		레벨에 배치된 적을 센다.

		카운터를 미리 잡아두지 않고 매번 순회하는 이유는, 웨이브처럼 적이
		나중에 스폰되는 구조로 가도 어긋나지 않기 때문이다. 순회는 적이
		죽는 시점에만 돌므로 비용이 문제되지 않는다.
	*/
	RefreshEnemyCount();
	
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

	ATPSGameState* GS = GetTPSGameState();
	if (!GS) return;

	const int32 NewLives = FMath::Max(0, GS->GetRemainingLives() - 1);
	GS->SetRemainingLives(NewLives);

	UE_LOG(TPSLog, Warning, TEXT("[SV] %s 사망 — 남은 목숨 %d"),
		*DeadCharacter->GetName(), NewLives);

	if (NewLives <= 0)
	{
		/*
			목숨 소진. 리스폰하지 않고 게임오버로 넘어간다.
			이미 죽은 폰은 래그돌로 남겨두어 결과 화면 뒤로 보이게 한다.
		*/
		EndMatchWithResult(false);
		return;
	}

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

void ATPSGameGameMode::EndMatchWithResult(bool bVictory)
{
	if (HasMatchEnded()) return;   // 동시 사망 등으로 중복 호출될 수 있다
	if (!GetWorld()) return;

	UE_LOG(TPSLog, Warning, TEXT("[SV] 매치 종료 — %s"),
		bVictory ? TEXT("승리") : TEXT("패배"));

	/*
		AGameMode::EndMatch()가 MatchState를 WaitingPostMatch로 바꾸고,
		그 값이 GameState를 통해 모든 클라이언트에 복제된다.
		GameMode를 AGameModeBase가 아니라 AGameMode로 승격해둔 것이 여기서 쓰인다.

		다만 MatchState 복제만으로는 UI를 띄우기에 타이밍이 불확실하므로
		각 컨트롤러에 Client RPC를 따로 보낸다.
	*/
	EndMatch();

	// 결과 화면 뒤에서 적이 계속 움직이지 않도록 정지시킨다.
	StopAllEnemyAI();

	int32 NotifiedCount = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		ATPSPlayerController* PC = Cast<ATPSPlayerController>(C);

		UE_LOG(TPSLog, Verbose, TEXT("[SV] PC 순회: %s (캐스트 %s)"),
			*GetNameSafe(C), PC ? TEXT("성공") : TEXT("실패"));

		if (PC)
		{
			PC->Client_ShowGameOver(bVictory);
			++NotifiedCount;
		}
	}
	UE_LOG(TPSLog, Verbose, TEXT("[SV] 게임오버 통지 %d명"), NotifiedCount);
}

void ATPSGameGameMode::NotifyEnemyDied(AEnemyCharacter* DeadEnemy)
{
	if (HasMatchEnded()) return;

	RefreshEnemyCount();
}

int32 ATPSGameGameMode::CountLivingEnemies() const
{
	int32 Count = 0;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		// IsDead()는 State.Dead 태그를 본다.
		if (Enemy && !Enemy->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

void ATPSGameGameMode::RefreshEnemyCount()
{
	ATPSGameState* GS = GetTPSGameState();
	if (!GS) return;

	const int32 Living = CountLivingEnemies();
	GS->SetRemainingEnemies(Living);

	// 적이 원래 없는 맵에서 즉시 승리가 뜨지 않도록 매치 시작 여부를 함께 본다.
	if (Living <= 0 && HasMatchStarted() && GS->GetRemainingLives() > 0)
	{
		EndMatchWithResult(true);
	}
}

void ATPSGameGameMode::RequestRestartMatch()
{
	/*
		게임오버 상태에서만 받아들인다.

		클라이언트가 보낸 요청이므로 서버가 조건을 재검증한다.
		진행 중에 호출되면 누구든 매치를 리셋할 수 있게 된다.
	*/
	if (!HasMatchEnded())
	{
		UE_LOG(TPSLog, Warning, TEXT("[SV] 재시작 요청 거부 — 게임오버 상태가 아님"));
		return;
	}

	UE_LOG(TPSLog, Warning, TEXT("[SV] 매치 재시작"));

	// 트래블 전에 결과 화면을 걷는다. SeamlessTravel에서 컨트롤러가 살아남으므로
	// 입력 모드를 되돌리지 않으면 새 맵에서 조작이 먹지 않는다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ATPSPlayerController* PC = Cast<ATPSPlayerController>(It->Get()))
		{
			PC->Client_HideGameOver();
		}
	}

	/*
		RestartGame()은 현재 레벨을 ServerTravel로 다시 로드한다.
		bUseSeamlessTravel이 켜져 있어 클라이언트는 재접속 없이 따라온다.
		모든 액터가 새로 만들어지므로 목숨은 BeginPlay에서 다시 초기화된다.
	*/
	RestartGame();
}

ATPSGameState* ATPSGameGameMode::GetTPSGameState() const
{
	return GetGameState<ATPSGameState>();
}

void ATPSGameGameMode::StopAllEnemyAI()
{
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (!Enemy || Enemy->IsDead()) continue;

		Enemy->StopCombat();
	}
}
