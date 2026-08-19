#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(TPSLog, Log, All);

class AActor;

/*
    네트워크 로그 접두사.
    예) [SV|Authority|Listen] BP_PlayerCharacter_C_0
        [CL|Simulated|Client] BP_EnemyCharacter_C_3

    네트워크 버그는 대부분 "이 코드가 어느 머신에서 돌았나"가 핵심이므로
    모든 네트워크 관련 로그에 이 접두사를 붙인다.
*/
FString TPSNetPrefix(const AActor* Actor);