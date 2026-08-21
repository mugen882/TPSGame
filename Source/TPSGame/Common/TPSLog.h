#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(TPSLog, Log, All);

class AActor;

namespace TPSNetDebug
{
	/*
		네트워크 로그 접두사.
		예) [SV|Authority|Listen] BP_PlayerCharacter_C_0
			[CL|Simulated|Client] BP_EnemyCharacter_C_3
			네트워크 버그는 대부분 "이 코드가 어느 머신에서 돌았나"가 핵심이므로
			모든 네트워크 관련 로그에 이 접두사를 붙인다.
	*/
	FString TPSNetPrefix(const AActor* Actor);

	/**
	 * 네트워크 디버깅용 문자열 변환 헬퍼.
	 * ENetRole/ENetMode는 UENUM이 아니라 UEnum::GetValueAsString()을 쓸 수 없다.
	 */
	inline const TCHAR* NetModeToString(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone:      return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("Dedicated");
		case NM_ListenServer:    return TEXT("Listen");
		case NM_Client:          return TEXT("Client");
		default:                 return TEXT("Unknown");
		}
	}

	inline const TCHAR* NetRoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:            return TEXT("None");
		case ROLE_SimulatedProxy:  return TEXT("Simulated");
		case ROLE_AutonomousProxy: return TEXT("Autonomous");
		case ROLE_Authority:       return TEXT("Authority");
		default:                   return TEXT("Unknown");
		}
	}

	TPSGAME_API FString DescribeNet(const AActor* Actor);
	TPSGAME_API double  GetSyncedTime(const AActor* Actor);
}