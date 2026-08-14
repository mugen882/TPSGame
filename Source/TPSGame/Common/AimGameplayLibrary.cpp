#include "Common/AimGameplayLibrary.h"

bool FAimGameplayLibrary::PredictIntercept(const FVector& Shooter, const FVector& Target,
    const FVector& TargetVel, float ProjSpeed, FVector& OutAim)
{
    const FVector ToTgt = Target - Shooter;
    const float a = TargetVel.SizeSquared() - ProjSpeed * ProjSpeed;
    const float b = 2.f * FVector::DotProduct(TargetVel, ToTgt);
    const float c = ToTgt.SizeSquared();

    float t;
    if (FMath::Abs(a) < KINDA_SMALL_NUMBER)            // 속도 동일 → 선형
    {
        if (FMath::Abs(b) < KINDA_SMALL_NUMBER) { OutAim = Target; return false; }
        t = -c / b;
    }
    else
    {
        const float disc = b * b - 4.f * a * c;
        if (disc < 0.f) { OutAim = Target; return false; } // 따라잡기 불가
        const float sq = FMath::Sqrt(disc);
        const float t1 = (-b + sq) / (2.f * a), t2 = (-b - sq) / (2.f * a);
        t = (t1 > 0.f && t2 > 0.f) ? FMath::Min(t1, t2) : FMath::Max(t1, t2);
    }
    if (t <= 0.f) { OutAim = Target; return false; }
    OutAim = Target + TargetVel * t;   // 발사 시점 t초 뒤 예측 위치
    return true;
}