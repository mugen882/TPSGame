#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

/*
    적 AI컨트롤러
    BT실행
    플레이어가 시야거리에 들어오고 나가는 것에 따른 처리
*/

UCLASS()
class TPSGAME_API AEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    AEnemyAIController();

    void NotifyDamagedBy(AActor* Attacker);

protected:
    virtual void OnPossess(APawn* InPawn) override;

    UFUNCTION()
    void OnTargetPerceived(AActor* Actor, FAIStimulus Stimulus);

 private:
    UPROPERTY(EditDefaultsOnly, Category="AI")
    class UBehaviorTree* BehaviorTree;

    UPROPERTY(VisibleAnywhere, Category="AI")
    class UAIPerceptionComponent* PerceptionComp;

    UPROPERTY()
    class UAISenseConfig_Sight* SightConfig;
};