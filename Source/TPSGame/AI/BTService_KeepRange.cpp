#include "AI/BTService_KeepRange.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"
#include "Character/EnemyCharacter.h"
#include "AI/EnemyCombatProfile.h"
#include "Weapon/ProjectileRocket.h"
#include "Weapon/WeaponRocketLauncher.h"

UBTService_KeepRange::UBTService_KeepRange()
{
    NodeName = TEXT("Keep Range");
    Interval = 0.25f;
    RandomDeviation = 0.05f;
    bNotifyBecomeRelevant = true;

    BlackboardKey.AddObjectFilter(this,
        GET_MEMBER_NAME_CHECKED(UBTService_KeepRange, BlackboardKey), AActor::StaticClass());
}

void UBTService_KeepRange::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    FBTKeepRangeMemory* Mem = reinterpret_cast<FBTKeepRangeMemory*>(NodeMemory);

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!BB || !AICon) return;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AICon->GetPawn());
    if (!Enemy) return;

    const UEnemyCombatProfile* Profile = Enemy->GetCombatProfile();
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(BlackboardKey.SelectedKeyName));
    if (!Profile || !Target) return;

    const FVector SelfLoc = Enemy->GetActorLocation();
    const FVector TargetLoc = Target->GetActorLocation();
    const float   Dist = FVector::Dist(SelfLoc, TargetLoc);

    const float Preferred = Profile->GetPreferredRange();
    const float Tol = Profile->RangeTolerance;

    // 스트레이프 방향 주기적 반전
    Mem->StrafeFlipTimer += DeltaSeconds;
    if (Mem->StrafeFlipTimer >= StrafeFlipInterval)
    {
        Mem->StrafeFlipTimer = 0.f;
        Mem->StrafeSign *= -1.f;
    }

    float ExplosionRadius = 0.f;  // 로켓 자해 회피용 최소거리
	if (AWeaponRocketLauncher* RocketLauncher = Cast<AWeaponRocketLauncher>(Enemy->GetCurrentWeapon()))
	{
		if (RocketLauncher->GetProjectileClass())
		{
			if (const AProjectileRocket* RocketCDO = RocketLauncher->GetProjectileClass()->GetDefaultObject<AProjectileRocket>())
			{
				ExplosionRadius = RocketCDO->GetExplosionRadius();
			}
		}
	}

    if (Dist < ExplosionRadius)
    {
        if (!AICon->IsFollowingAPath())
		{
            // 로켓 자해 회피: 타겟 반대 방향으로 후퇴
            const FVector Away = (SelfLoc - TargetLoc).GetSafeNormal2D();
            const FVector Dest = SelfLoc + Away * (ExplosionRadius - Dist + 100.f);
            AICon->MoveToLocation(Dest, AcceptanceRadius, true, true, false, true);
        }
    }
    else if (Dist > Preferred + Tol)
    {
		if (!AICon->IsFollowingAPath())
		{
            // 너무 멀다 → 타겟 쪽으로 접근
			const FVector ToTarget = (TargetLoc - SelfLoc).GetSafeNormal2D();
			const FVector Dest = TargetLoc - ToTarget * Preferred;  // 타깃에서 Preferred만큼 앞
			AICon->MoveToLocation(Dest, AcceptanceRadius, true, true, false, true);
        }
    }
    else if (Dist < Preferred - Tol)
    {
		if (!AICon->IsFollowingAPath())
		{
            // 너무 가깝다 → 약간 뒤로
            const FVector Away = (SelfLoc - TargetLoc).GetSafeNormal2D();
            const FVector Dest = SelfLoc + Away * (Preferred - Dist);
            AICon->MoveToLocation(Dest, AcceptanceRadius, true, true, false, true);
        }
    }
    else
    {
        // 사거리 밴드 안. 머신건은 스트레이프, 그 외는 정지.
        if (Profile->bBurstFire)
        {
			if (!AICon->IsFollowingAPath())
			{
                const FVector ToTarget = (TargetLoc - SelfLoc).GetSafeNormal2D();
                const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget);
                const FVector Dest = SelfLoc + Right * (Mem->StrafeSign * StrafeDistance);
                AICon->MoveToLocation(Dest, AcceptanceRadius, true, true, false, true);
            }
        }
        else
        {
            AICon->StopMovement();  // 라이플/로켓: 사거리 OK면 멈춰서 사격
        }
    }
}

// 서비스 활성화시 한번 불림
void UBTService_KeepRange::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!AICon) return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AICon->GetPawn());
	if (!Enemy) return;

    FBTKeepRangeMemory* Mem = reinterpret_cast<FBTKeepRangeMemory*>(NodeMemory);
    Mem->StrafeSign = 1.f;
    Mem->StrafeFlipTimer = 0.f;
}