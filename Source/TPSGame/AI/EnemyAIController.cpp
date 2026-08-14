#include "EnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/PlayerCharacter.h"
#include "Character/EnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Common/AIBlackboardKeys.h"
#include "Subsystem/DifficultySubsystem.h"
#include "AbilitySystem/TPSAttributeSet.h"
#include "Common/TPSLog.h"

AEnemyAIController::AEnemyAIController()
{
    PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));

    SightConfig->SightRadius = 2000.0f;         // 감지 거리
    SightConfig->LoseSightRadius = 2500.0f;     // 시야 잃는 거리
    SightConfig->PeripheralVisionAngleDegrees = 90.0f;  // 시야각 (좌우 90도)
    SightConfig->SetMaxAge(5.0f);               // 기억 유지 시간
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

    PerceptionComp->ConfigureSense(*SightConfig);
    PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (PerceptionComp)
    {
        PerceptionComp->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIController::OnTargetPerceived);
    }

    if (BehaviorTree)
    {
        RunBehaviorTree(BehaviorTree);
    }
}

void AEnemyAIController::OnTargetPerceived(AActor* Actor, FAIStimulus Stimulus)
{
    if (!Actor) return;

    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!BB)
    {
        UE_LOG(TPSLog, Warning, TEXT("Blackboard is NULL"));
        return;
    }

    if (APlayerCharacter* Player = Cast<APlayerCharacter>(Actor))
    {
        if (Stimulus.WasSuccessfullySensed())
        {
            // 보임 → 교전 상태
            BB->SetValueAsObject(TargetActorKey, Actor);
            BB->SetValueAsVector(LastKnownLocationKey, Actor->GetActorLocation());
            BB->SetValueAsBool(CanSeeTargetKey, true);
            if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn()))
            {
                Enemy->SetAiming(true);

                Enemy->GetCharacterMovement()->bOrientRotationToMovement = false;
                Enemy->bUseControllerRotationYaw = true;
            }	

            SetFocus(Actor);
        }
        else
        {
            BB->SetValueAsBool(CanSeeTargetKey, false);
            BB->ClearValue(TargetActorKey);
            BB->SetValueAsVector(LastKnownLocationKey, Stimulus.StimulusLocation);
            if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn()))
            {
                Enemy->SetAiming(false);

                Enemy->GetCharacterMovement()->bOrientRotationToMovement = true;
                Enemy->bUseControllerRotationYaw = false;
            }   

            ClearFocus(EAIFocusPriority::Gameplay);
        }
    }
    else
    {
        UE_LOG(TPSLog, Warning, TEXT("Cast to PlayerCharacter FAILED: %s"), *Actor->GetName());
    }
}

void AEnemyAIController::NotifyDamagedBy(AActor* Attacker)
{
    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!BB)
    {
        UE_LOG(TPSLog, Warning, TEXT("Blackboard is NULL"));
        return;
    }

    BB->SetValueAsObject(TargetActorKey, Attacker);
    BB->SetValueAsVector(LastKnownLocationKey, Attacker->GetActorLocation());
}