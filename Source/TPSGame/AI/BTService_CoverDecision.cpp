#include "AI/BTService_CoverDecision.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/EnemyCharacter.h"
#include "Common/AIBlackboardKeys.h"
#include "AbilitySystem/TPSAttributeSet.h"

UBTService_CoverDecision::UBTService_CoverDecision()
{
    NodeName = TEXT("Cover Decision");
    Interval = 0.1f;          // 판정 주기 (KeepRange보다 살짝 크게)
    RandomDeviation = 0.02f;
}

void UBTService_CoverDecision::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    FBTCoverDecisionMemory* Mem = reinterpret_cast<FBTCoverDecisionMemory*>(NodeMemory);

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AAIController* AICon = OwnerComp.GetAIOwner();
    AEnemyCharacter* Enemy = AICon ? Cast<AEnemyCharacter>(AICon->GetPawn()) : nullptr;
    if (!BB || !Enemy) return;

    float DamageThreshold = 100.f;
    if (const UTPSAttributeSet* AS = Enemy->GetAttributeSet())
        DamageThreshold = AS->GetMaxHealth() * DamageThresholdRatio;   // 최근 누적 피해가 이 이상이면 엄폐

    const float Now = Enemy->GetWorld()->GetTimeSeconds();
    // 버스트 쿨다운 중 (UBTTask_FireWeapon이 세팅)
    const bool bInCooldown = BB->GetValueAsBool(InBurstCooldownKey);

    // 누적 피해가 임계값 넘었나 (한 대가 아니라 집중포화일 때만)
    const float Recent = (Now - Enemy->GetLastHitTime() < Enemy->GetDamageDecayTime())
        ? Enemy->GetRecentDamageAccum() : 0.f;
    const bool bHeavyHit = Recent >= DamageThreshold;

    bool bWantCover = bInCooldown || bHeavyHit;

    // 한 번 숨기로 했으면 최소 유지 시간 동안 유지
    const bool bCurrentlyCovering = BB->GetValueAsBool(ShouldTakeCoverKey);
    if (bCurrentlyCovering && (Now - Mem->CoverEnterTime < MinCoverHoldTime))
    {
        bWantCover = true;   // 아직 최소 유지 시간 안 지남 → 계속 숨기
    }

    if (bWantCover && !bCurrentlyCovering)
    {
        Mem->CoverEnterTime = Now;   // 숨기 시작 시각 기록 (이 AI 전용)
    }

    BB->SetValueAsBool(ShouldTakeCoverKey, bWantCover);
}