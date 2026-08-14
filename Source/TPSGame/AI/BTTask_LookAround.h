#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_LookAround.generated.h"

/**
	제자리에서 좌우로 둘러보는 수색 연출 태스크.
	시작 방향을 기준으로 (-SweepAngle ~ +SweepAngle)을 왕복하며 회전하고,
	지정한 횟수만큼 둘러본 뒤 Succeeded 로 종료
  */
UCLASS()
class TPSGAME_API UBTTask_LookAround : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_LookAround();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	// 시작 Yaw 기준 좌우로 둘러볼 최대 각도(도)
	UPROPERTY(EditAnywhere, Category="LookAround", meta=(ClampMin="0.0"))
	float SweepAngle = 75.0f;

	// 회전 보간 속도(도/초). 클수록 빨리 돈다.
	UPROPERTY(EditAnywhere, Category="LookAround", meta=(ClampMin="1.0"))
	float RotationSpeed = 120.0f;

	// 한 쪽 끝(좌/우)에 도달했을 때 머무는 시간(초). 멈칫하며 살피는 느낌.
	UPROPERTY(EditAnywhere, Category="LookAround", meta=(ClampMin="0.0"))
	float PauseAtEnd = 0.4f;

	// 둘러보기 왕복(좌→우→중앙 등) 단계 수. 끝나면 태스크 종료.
	UPROPERTY(EditAnywhere, Category="LookAround", meta=(ClampMin="1"))
	int32 SweepCount = 2;

private:
	// 회전 단계
	enum class EPhase : uint8 { ToLeft, ToRight, ToCenter, Done };

	// 인스턴스별 런타임 상태
	struct FLookAroundMemory
	{
		float BaseYaw = 0.0f;       // 시작 시점의 Yaw
		float TargetYaw = 0.0f;     // 현재 보간 목표 Yaw
		float PauseTimer = 0.0f;    // 끝점 체류 카운트다운
		int32 SweepsDone = 0;       // 완료한 sweep 수
		EPhase Phase = EPhase::ToLeft;
	};
};
