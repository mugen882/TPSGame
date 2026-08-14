#include "Subsystem/DifficultySubsystem.h"

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
    }

    UE_LOG(LogTemp, Log, TEXT("[Difficulty] Set to %d (DmgMul=%.2f, HpMul=%.2f, SpreadMul=%.2f)"),
        (int32)Level, EnemyDamageMul, EnemyHealthMul, EnemySpreadMul);
}