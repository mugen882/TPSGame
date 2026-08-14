#include "AI/BTTask_LookAround.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTTask_LookAround::UBTTask_LookAround()
{
	NodeName = TEXT("Look Around");
	bNotifyTick = true;
}

uint16 UBTTask_LookAround::GetInstanceMemorySize() const
{
	return sizeof(FLookAroundMemory);
}

EBTNodeResult::Type UBTTask_LookAround::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	FLookAroundMemory* Mem = reinterpret_cast<FLookAroundMemory*>(NodeMemory);
	Mem->BaseYaw = Pawn->GetActorRotation().Yaw;
	Mem->TargetYaw = Mem->BaseYaw - SweepAngle;   // 먼저 왼쪽부터
	Mem->Phase = EPhase::ToLeft;
	Mem->PauseTimer = 0.0f;
	Mem->SweepsDone = 0;

	// 보간은 TickTask 에서 진행 → InProgress 반환
	return EBTNodeResult::InProgress;
}

void UBTTask_LookAround::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FLookAroundMemory* Mem = reinterpret_cast<FLookAroundMemory*>(NodeMemory);

	// 끝점 체류 중이면 타이머만 깎고 대기
	if (Mem->PauseTimer > 0.0f)
	{
		Mem->PauseTimer -= DeltaSeconds;
		return;
	}

	// 현재 Yaw 를 목표 Yaw 로 보간
	const FRotator Current = Pawn->GetActorRotation();
	const float NewYaw = FMath::FixedTurn(Current.Yaw, Mem->TargetYaw, RotationSpeed * DeltaSeconds);
	Pawn->SetActorRotation(FRotator(Current.Pitch, NewYaw, Current.Roll));

	// 목표에 충분히 가까워지면 다음 단계로
	const bool bReached = FMath::Abs(FRotator::NormalizeAxis(Mem->TargetYaw - NewYaw)) < 1.0f;
	if (!bReached)
	{
		return;
	}

	Mem->PauseTimer = PauseAtEnd;

	switch (Mem->Phase)
	{
	case EPhase::ToLeft:
		Mem->Phase = EPhase::ToRight;
		Mem->TargetYaw = Mem->BaseYaw + SweepAngle;
		break;

	case EPhase::ToRight:
		++Mem->SweepsDone;
		if (Mem->SweepsDone >= SweepCount)
		{
			Mem->Phase = EPhase::ToCenter;
			Mem->TargetYaw = Mem->BaseYaw;   // 마지막에 정면 복귀
		}
		else
		{
			Mem->Phase = EPhase::ToLeft;
			Mem->TargetYaw = Mem->BaseYaw - SweepAngle;
		}
		break;

	case EPhase::ToCenter:
		Mem->Phase = EPhase::Done;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		break;

	default:
		break;
	}
}

EBTNodeResult::Type UBTTask_LookAround::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return EBTNodeResult::Aborted;  // 즉시 abort 수락
}