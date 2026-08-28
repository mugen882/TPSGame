#include "Cheat/TPSCheatManager.h"

#include "Character/TPSPlayerController.h"
#include "Common/TPSLog.h"

ATPSPlayerController* UTPSCheatManager::GetTPSPlayerController() const
{
	// UCheatManager는 UCLASS(Within=PlayerController)라 아우터가 항상 컨트롤러다.
	return Cast<ATPSPlayerController>(GetOuterAPlayerController());
}

void UTPSCheatManager::TPSSuicide()
{
	ATPSPlayerController* PC = GetTPSPlayerController();
	if (!PC)
	{
		UE_LOG(TPSLog, Warning, TEXT("TPSSuicide — ATPSPlayerController를 찾을 수 없다"));
		return;
	}

	/*
		리슨 서버 호스트나 스탠드얼론이면 이 RPC는 로컬에서 그대로 실행된다.
		원격 클라이언트면 서버로 전송된다. 호출부에서 분기할 필요가 없다.
	*/
	PC->ServerCheatSuicide();
}

void UTPSCheatManager::TPSDamageSelf(float Amount)
{
	ATPSPlayerController* PC = GetTPSPlayerController();
	if (!PC)
	{
		return;
	}

	PC->ServerCheatDamageSelf(FMath::Max(Amount, 0.0f));
}

void UTPSCheatManager::TPSHealSelf()
{
	ATPSPlayerController* PC = GetTPSPlayerController();
	if (!PC)
	{
		return;
	}

	PC->ServerCheatHealSelf();
}
