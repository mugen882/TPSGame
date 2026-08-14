#include "AI/BTService_UpdateAim.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"

#include "Character/EnemyCharacter.h"
#include "AI/EnemyCombatProfile.h"
#include "Common/AimGameplayLibrary.h"

UBTService_UpdateAim::UBTService_UpdateAim()
{
    NodeName = TEXT("Update Aim");
    Interval = 0.05f;
    RandomDeviation = 0.0f;

    // Vector(AimLocation)로 필터링
    BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateAim, BlackboardKey));
    // Object로 필터링
    TargetActorKeySelector.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateAim, TargetActorKeySelector), AActor::StaticClass());
}

void UBTService_UpdateAim::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!BB || !AICon) return;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AICon->GetPawn());
    if (!Enemy) return;

    const UEnemyCombatProfile* Profile = Enemy->GetCombatProfile();
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKeySelector.SelectedKeyName));
    if (!Profile || !Target) return;

    const float Dist = FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());
    const float FireRange = Enemy->GetAttackRange();
    Enemy->SetHoldingAim(Dist <= FireRange);
    
    FVector AimPoint = Target->GetActorLocation() + FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());

    // 로켓 등 느린 투사체: 요격점 예측
    if (Profile->bLeadTarget)
    {
        FVector TargetVel = Target->GetVelocity();
        FVector Intercept;
        if (FAimGameplayLibrary::PredictIntercept(
            Enemy->GetActorLocation(), AimPoint, TargetVel,
            Profile->ProjectileSpeed, Intercept))
        {
            AimPoint = Intercept;
        }
        // 예측 실패면 현재 위치로 폴백
    }

    BB->SetValueAsVector(BlackboardKey.SelectedKeyName, AimPoint);
    AICon->SetFocalPoint(AimPoint);
}