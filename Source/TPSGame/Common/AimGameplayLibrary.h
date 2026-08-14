#pragma once
#include "CoreMinimal.h"

/*
    Aim게임플레이 라이브러리 클래스
    로켓 쏠때 예측샷에 사용
*/

class TPSGAME_API FAimGameplayLibrary
{
public:
    static bool PredictIntercept(const FVector& Shooter, const FVector& Target,
        const FVector& TargetVel, float ProjSpeed, FVector& OutAim);
};