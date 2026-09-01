#include "Game/TPSGameState.h"
#include "Net/UnrealNetwork.h"

ATPSGameState::ATPSGameState()
{
}

void ATPSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATPSGameState, RemainingLives);
	DOREPLIFETIME(ATPSGameState, RemainingEnemies);
}

void ATPSGameState::SetRemainingLives(int32 NewLives)
{
	if (!HasAuthority()) return;

	RemainingLives = NewLives;

	// 서버는 자기 OnRep을 받지 않으므로 직접 알린다.
	OnLivesChanged.Broadcast(RemainingLives);
}

// 서버에서만 호출되는 OnRep. 클라에서만 호출된다.
void ATPSGameState::OnRep_RemainingLives()
{
	OnLivesChanged.Broadcast(RemainingLives);
}

void ATPSGameState::SetRemainingEnemies(int32 NewCount)
{
	if (!HasAuthority()) return;

	RemainingEnemies = NewCount;

	// 서버는 자기 OnRep을 받지 않으므로 직접 알린다.
	OnEnemiesChanged.Broadcast(RemainingEnemies);
}

// 서버에서만 호출되는 OnRep. 클라에서만 호출된다.
void ATPSGameState::OnRep_RemainingEnemies()
{
	OnEnemiesChanged.Broadcast(RemainingEnemies);
}
