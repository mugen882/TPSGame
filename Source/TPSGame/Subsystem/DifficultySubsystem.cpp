#include "Subsystem/DifficultySubsystem.h"
#include "Common/TPSLog.h"

void UDifficultySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 시작 기본 난이도
    SetDifficulty(EDifficulty::Hard);
}

void UDifficultySubsystem::SetDifficulty(EDifficulty Level)
{
    CurrentDifficulty = Level;

    switch (Level)
    {
    case EDifficulty::Easy:
        EnemyDamageMul = 0.6f;
        EnemyHealthMul = 0.75f;
        EnemySpreadMul = 1.6f;   // 더 많이 빗나감
        break;

    case EDifficulty::Normal:
        EnemyDamageMul = 1.0f;
        EnemyHealthMul = 1.0f;
        EnemySpreadMul = 1.0f;
        break;

    case EDifficulty::Hard:
        EnemyDamageMul = 1.5f;
        EnemyHealthMul = 1.4f;
        EnemySpreadMul = 0.5f;   // 더 정확
        break;

    default:
        // 새 난이도가 추가됐는데 케이스를 안 넣으면 Normal 기준으로 폴백
        EnemyDamageMul = 1.0f;
        EnemyHealthMul = 1.0f;
        EnemySpreadMul = 1.0f;
        break;
    }

    UE_LOG(TPSLog, Log, TEXT("[Difficulty] Set to %d (DmgMul=%.2f, HpMul=%.2f, SpreadMul=%.2f)"),
        (int32)Level, EnemyDamageMul, EnemyHealthMul, EnemySpreadMul);
}