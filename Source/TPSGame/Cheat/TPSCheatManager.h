#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "TPSCheatManager.generated.h"

/**
	개발용 치트 커맨드 모음.

	콘솔 명령은 항상 "명령을 입력한 머신"에서 실행된다. 원격 클라이언트가 친
	치트는 그 클라이언트의 CheatManager에서 돌기 때문에, 게임 상태를 바꾸려면
	반드시 PlayerController의 Server RPC를 거쳐 서버로 넘겨야 한다.

	그래서 이 클래스는 판정을 직접 하지 않는다. 입력을 받아 서버로 전달하는
	역할만 하고, 실제 처리는 ATPSPlayerController의 ServerCheat* 함수에 있다.
 */
UCLASS()
class TPSGAME_API UTPSCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/*
		자기 폰을 즉시 사망시킨다.

		UCheatManager에도 Suicide가 있지만 그쪽은 AActor::KilledBy 경로라
		GAS를 타지 않는다. GE_Dead / State.Dead / 리스폰 흐름을 그대로
		검증하려면 이 명령을 써야 한다.
	*/
	UFUNCTION(exec)
	void TPSSuicide();

	// 자신에게 지정한 만큼 피해를 준다. 빈사 상태를 만들 때 사용.
	UFUNCTION(exec)
	void TPSDamageSelf(float Amount = 50.0f);

	// 체력을 최대치로 되돌린다.
	UFUNCTION(exec)
	void TPSHealSelf();

private:
	class ATPSPlayerController* GetTPSPlayerController() const;
};
