#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "TPSGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLivesChanged, int32, NewLives);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemiesChanged, int32, NewCount);
/*
	팀 공용 상태를 담는 GameState.

	AGameState(Base 아님)를 상속하는 이유는 MatchState 상태 머신 때문이다.
	GameMode를 AGameModeBase -> AGameMode로 승격해둔 것이 여기서 쓰인다.
	게임오버는 AGameMode::EndMatch()가 MatchState를 WaitingPostMatch로 바꾸고,
	그 값이 GameState를 통해 모든 클라이언트에 복제되는 흐름을 탄다.

	목숨을 PlayerState가 아니라 여기에 두는 것은 팀 공용 자원이기 때문이다.
	개인별로 두면 먼저 죽은 사람만 오래 관전하게 되어 코옵 협동감이 떨어진다.
*/
UCLASS()
class TPSGAME_API ATPSGameState : public AGameState
{
	GENERATED_BODY()

public:
	ATPSGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetRemainingLives() const { return RemainingLives; }

	// 서버 전용. GameMode가 사망/초기화 시 호출한다.
	void SetRemainingLives(int32 NewLives);

	int32 GetRemainingEnemies() const { return RemainingEnemies; }

	// 서버 전용. 적 사망/초기화 시 GameMode가 갱신한다.
	void SetRemainingEnemies(int32 NewCount);

	// 목숨 표시 UI가 구독한다. 서버·클라 양쪽에서 발화한다.
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnLivesChanged OnLivesChanged;

	// 남은 적 수 표시 UI가 구독한다.
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnemiesChanged OnEnemiesChanged;

protected:
	UFUNCTION()
	void OnRep_RemainingLives();

	UFUNCTION()
	void OnRep_RemainingEnemies();

private:
	/*
		팀 공용 남은 목숨.

		COND_None: 모든 플레이어가 봐야 하는 정보다.
		(탄약이 COND_OwnerOnly인 것과 대비되는 지점)
	*/
	UPROPERTY(ReplicatedUsing = OnRep_RemainingLives)
	int32 RemainingLives = 0;

	// 살아 있는 적 수. 승리 조건이자 진행도 표시용이라 전원이 봐야 한다.
	UPROPERTY(ReplicatedUsing = OnRep_RemainingEnemies)
	int32 RemainingEnemies = 0;
};
