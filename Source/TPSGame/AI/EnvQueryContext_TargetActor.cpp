#include "AI/EnvQueryContext_TargetActor.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Common/AIBlackboardKeys.h"

void UEnvQueryContext_TargetActor::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    UObject* Owner = QueryInstance.Owner.Get();
    AAIController* AICon = Cast<AAIController>(Owner);
    if (!AICon)
    {
        if (APawn* Pawn = Cast<APawn>(Owner))
            AICon = Cast<AAIController>(Pawn->GetController());
    }
    if (!AICon) return;

    UBlackboardComponent* BB = AICon->GetBlackboardComponent();
    if (!BB) return;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey));
    if (!Target) return;

    UEnvQueryItemType_Actor::SetContextHelper(ContextData, Target);
}