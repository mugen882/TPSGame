#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DifficultySubsystem.generated.h"

UENUM(BlueprintType)
enum class EDifficulty : uint8
{
    Easy,
    Normal,
    Hard
};

/*
    난이도 서브시스템
*/
UCLASS()
class TPSGAME_API UDifficultySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Difficulty")
    void SetDifficulty(EDifficulty Level);

    UFUNCTION(BlueprintPure, Category = "Difficulty")
    EDifficulty GetDifficulty() const { return CurrentDifficulty; }

    // 적 스케일 배수 (ApplyCombatProfile에서 읽음)
    float EnemyDamageMul = 1.0f;
    float EnemyHealthMul = 1.0f;
    float EnemySpreadMul = 1.0f;   // 클수록 부정확 = 쉬움

private:
    EDifficulty CurrentDifficulty = EDifficulty::Normal;
};