#include "AI/BTTask_FireWeapon.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AbilitySystemComponent.h"

#include "Character/EnemyCharacter.h"
#include "AI/EnemyCombatProfile.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Common/AIBlackboardKeys.h"
#include "Common/TPSLog.h"

UBTTask_FireWeapon::UBTTask_FireWeapon()
{
    NodeName = TEXT("Fire Weapon");
    bNotifyTick = true;
    bNotifyTaskFinished = true;  // 안전하게 정리 콜백 받기
}

EBTNodeResult::Type UBTTask_FireWeapon::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FBTFireWeaponMemory* Mem = reinterpret_cast<FBTFireWeaponMemory*>(NodeMemory);
    Mem->bBursting = false;
    Mem->RemainingBurst = 0.f;
	Mem->bBurstCoolingDown = false;
	Mem->RemainingBurstCooldown = 0.f;

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon) return EBTNodeResult::Failed;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AICon->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;

    UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
    const UEnemyCombatProfile* Profile = Enemy->GetCombatProfile();
    if (!ASC || !Profile) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(TargetActorKey)) : nullptr;
    if (!Target)
    {
        // 타겟 없으면 발사 안 함, 시퀀스는 살림
        return EBTNodeResult::Succeeded;
    }

    const float Dist = FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());
    if (Dist > Enemy->GetAttackRange())
    {
		UE_LOG(TPSLog, Warning, TEXT("BTTask_FireWeapon: Target out of range (%.1f > %.1f)"), Dist, Enemy->GetAttackRange());
        // 사거리 밖 → 발사 안 함
        return EBTNodeResult::Succeeded;
    }

    const FGameplayAbilitySpecHandle Handle = Enemy->GetFireAbilitySpecHandle();
    if (!Handle.IsValid()) return EBTNodeResult::Succeeded;

    // 발사 시도. active/쿨다운이면 이번엔 패스
    if (!ASC->TryActivateAbility(Handle))
    {
		UE_LOG(TPSLog, Warning, TEXT("BTTask_FireWeapon: TryActivateAbility failed"));
        return EBTNodeResult::Succeeded;
    }

    if (Profile->bBurstFire)
    {
        Mem->bBursting = true;
        Mem->RemainingBurst = Profile->BurstDuration;
    }
    // 단발/버스트 모두 InProgress로 대기 → TickTask에서 종료 판정
    return EBTNodeResult::InProgress;
}

void UBTTask_FireWeapon::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    FBTFireWeaponMemory* Mem = reinterpret_cast<FBTFireWeaponMemory*>(NodeMemory);

    AAIController* AICon = OwnerComp.GetAIOwner();
    AEnemyCharacter* Enemy = AICon ? Cast<AEnemyCharacter>(AICon->GetPawn()) : nullptr;
    UAbilitySystemComponent* ASC = Enemy ? Enemy->GetAbilitySystemComponent() : nullptr;
    if (!ASC)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

	// 쿨다운 진행 중(사격 멈춘 채 BurstCooldown 만큼 대기)
	if (Mem->bBurstCoolingDown)
	{
		Mem->RemainingBurstCooldown -= DeltaSeconds;
		if (Mem->RemainingBurstCooldown <= 0.f)
		{
            if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
                BB->SetValueAsBool(InBurstCooldownKey, false);

			Mem->bBurstCoolingDown = false;
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
		return;
	}

    const FGameplayAbilitySpecHandle Handle = Enemy->GetFireAbilitySpecHandle();

    // 버스트 진행 중
	if (Mem->bBursting)
	{
		Mem->RemainingBurst -= DeltaSeconds;
		if (Mem->RemainingBurst <= 0.f)
		{
			ASC->CancelAbilityHandle(Handle);   // 사격은 즉시 중단
			Mem->bBursting = false;

			// 프로파일에서 쿨다운 값을 읽어 쿨다운 단계로 전환
			const UEnemyCombatProfile* Profile = Enemy->GetCombatProfile();
			const float Cooldown = Profile ? Profile->BurstCooldown : 0.f;

			if (Cooldown > 0.f)
			{
				Mem->bBurstCoolingDown = true;
				Mem->RemainingBurstCooldown = Cooldown;

                if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
                    BB->SetValueAsBool(InBurstCooldownKey, true);
			}
			else
			{
				FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);  // 쿨다운 0이면 바로 종료
			}
		}
		return;
	}

    // 단발: fire ability(몽타주)가 끝나면(EndAbility로 IsActive=false) 완료
    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
    if (!Spec || !Spec->IsActive())
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}

EBTNodeResult::Type UBTTask_FireWeapon::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FBTFireWeaponMemory* Mem = reinterpret_cast<FBTFireWeaponMemory*>(NodeMemory);
    if (AAIController* AICon = OwnerComp.GetAIOwner())
    {
        if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AICon->GetPawn()))
        {
            if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
            {
                // 버스트든 단발이든 진행 중이면 취소
                ASC->CancelAbilityHandle(Enemy->GetFireAbilitySpecHandle());
            }
        }
    }
    Mem->bBursting = false;
    return EBTNodeResult::Aborted;
}