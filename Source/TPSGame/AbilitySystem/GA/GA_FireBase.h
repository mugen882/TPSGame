#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_FireBase.generated.h"

class AWeaponBase;
class ACommonCharacter;

/*
    발사 베이스 클래스
    무기 발사 관련 처리
    쿨다운 적용

    네트워크 구조

    NetExecutionPolicy = LocalPredicted 이므로 소유 클라이언트와 서버 양쪽에서 실행된다.
    두 머신이 하는 일이 다르다.

      소유 클라 : 조준점 계산 -> 서버 전송 -> 연출만 예측 재생 (판정 없음)
      서버      : 클라 조준점 수신 -> 검증 -> 트레이스 + 데미지

    AI는 서버에서 AIController가 조종하므로 "로컬이면서 권위"다.
    조준점을 스스로 알기 때문에 전송 없이 바로 권위 발사한다.
*/

UCLASS(Abstract)
class TPSGAME_API UGA_FireBase : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_FireBase();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

    // 무기의 bInfiniteAmmo 설정을 Cost 파이프라인에 연결
    virtual bool CheckCost(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ApplyCost(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override;

    // 쿨다운 GE를 무기 FireInterval(SetByCaller)로 적용
    virtual void ApplyCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override;

    virtual const FGameplayTagContainer* GetCooldownTags() const override; // CheckCooldown에서 이 태그 여부로 쿨다운 체크

    // 발사 타이밍 분기: 플레이어=즉발, 그 외(적)=노티파이 대기
    virtual bool ShouldFireImmediately(const FGameplayAbilityActorInfo* ActorInfo) const;

    /*
        한 발 발사 파이프라인.

        로컬이면 조준점을 계산해 (원격이면) 서버로 보내고 연출을 예측 재생한다.
        권위면 조준점으로 실제 판정을 수행한다.
        머신건 연사 루프도 이 함수를 재사용한다.
    */
    void FireOnce(const FGameplayAbilityActorInfo* ActorInfo);

    ACommonCharacter* GetOwningCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;

    // 서버가 클라 조준점을 기다리는 중인지 (연사 루프가 중복 요청하지 않도록)
    bool IsWaitingForClientAim() const { return TargetDataDelegateHandle.IsValid(); }

private:
    // 로컬 머신에서 조준점을 계산한다. 서버(원격 폰)에서는 의미가 없다.
    FVector ComputeLocalAimPoint(const FGameplayAbilityActorInfo* ActorInfo) const;

    // 원격 클라 → 서버로 조준점 전송 (예측 키와 함께)
    void SendAimPointToServer(const FGameplayAbilityActorInfo* ActorInfo, const FVector& AimPoint);

    // 서버: 클라 조준점 수신 대기 시작
    void BeginWaitForClientAim(const FGameplayAbilityActorInfo* ActorInfo);
    void StopWaitForClientAim();

    // 클라이언트의 조준점을 서버가 받는다.
    void OnClientAimPointReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);
    void OnClientAimTimeout();

    /*
        서버 검증.

        클라이언트가 보낸 조준점을 그대로 믿지 않는다.
        - 사거리를 넘으면 사거리 지점으로 클램프한다(거부하지 않는 이유는
          정상 플레이에서 경계값이 자주 걸리는데 거부하면 헛방으로 느껴지기 때문).
        - 캐릭터 뒤쪽을 조준하면 거부한다.
        총구 위치는 서버 자신의 무기에서 계산한다. 클라가 보낸 값을 쓰지 않는다.
    */
    bool ValidateAimPoint(ACommonCharacter* Char, AWeaponBase* Weapon, FVector& InOutAimPoint) const;

    /*
        사격자의 지연만큼 되감을 시간을 계산한다.

        클라가 화면에서 본 적의 위치는 서버의 현재 위치가 아니다.
          서버 -> 클라 위치 복제 지연 (RTT/2)
        + 클라의 이동 보간 지연
        + 클라 -> 서버 조준점 전송 지연 (RTT/2)
        = RTT + 보간 지연

        AI는 서버에서 조준하므로 지연이 없다. 0을 반환한다.
    */
    float ComputeRewindSeconds(const FGameplayAbilityActorInfo* ActorInfo) const;

    /*
        측정 전용 트레이스. 데미지를 적용하지 않는다.

        되감은 상태와 복원된 상태에서 각각 돌려 "이 발사에 랙 보상이
        실제로 영향을 줬는가"를 로그 한 줄로 판정하는 데 쓴다.
        TPS.LagCompensation.Debug 가 켜졌을 때만 호출된다.
    */
    bool TraceProbe(ACommonCharacter* Char, AWeaponBase* Weapon, const FVector& AimPoint, FHitResult& OutHit) const;

    // 서버 권위 발사 (트레이스 + 데미지 + 연출 Cue)
    void FireAuthoritative(const FGameplayAbilityActorInfo* ActorInfo, const FVector& AimPoint);

    // 클라 예측 연출 (머즐/발사음 + 예측 탄착)
    void ExecuteFireCues(const FGameplayAbilityActorInfo* ActorInfo, const FVector& AimPoint);

    // 발사 몽타주 재생.
    // bEndAbilityOnMontageEnd
    // true(적): 몽타주 종료가 어빌리티를 끝낸다(노티파이가 발사).
    // false(플레이어): 연출용 fire-and-forget 재생만 하고 어빌리티는 즉시 종료된다.
    // 반환값: 재생할 몽타주가 있었으면 true.
    bool PlayFireMontage(const FGameplayAbilityActorInfo* ActorInfo, bool bEndAbilityOnMontageEnd);

    UFUNCTION()
    void OnFireMontageEnded();

    UFUNCTION()
    void OnFireMontageCancelled();

    // 클라 예측 연출 (머즐/발사음 + 예측 탄착)
    void PredictFireCues(const FGameplayAbilityActorInfo* ActorInfo, const FVector& AimPoint);

protected:
    // 사거리 클램프 여유. 1.0이면 정확히 FireRange까지만 허용.
    UPROPERTY(EditDefaultsOnly, Category = "Validation")
    float AimRangeTolerance = 1.1f;

    // 전방 판정 하한. -1이면 검사 안 함, 0이면 정확히 90도.
    // TPS 카메라는 캐릭터 측후방을 조준할 수 있으므로 느슨하게 둔다.
    UPROPERTY(EditDefaultsOnly, Category = "Validation")
    float AimMinForwardDot = -0.5f;

    /*
        되감기 상한.

        핑을 그대로 믿으면 조작된 클라이언트가 "1초 전에 있던 위치"를 쏘는 치트가 된다.
        정상 플레이의 지연 범위를 넘지 않도록 잘라낸다.
    */
    UPROPERTY(EditDefaultsOnly, Category = "LagCompensation")
    float MaxRewindSeconds = 0.3f;

    /*
        클라이언트의 이동 보간 지연 추정치.

        CharacterMovementComponent의 NetworkSmoothing이 만드는 지연으로,
        핑에 포함되지 않는다. 정확히 알기 어려워 상수로 둔다.
    */
    UPROPERTY(EditDefaultsOnly, Category = "LagCompensation")
    float InterpolationDelay = 0.05f;

    // 클라 조준점이 도착하지 않을 때 어빌리티가 영구히 매달리는 것을 막는다.
    UPROPERTY(EditDefaultsOnly, Category = "Validation")
    float ClientAimTimeout = 1.0f;

private:
    FGameplayTagContainer CooldownTagContainer; // { Cooldown.Fire }

    FDelegateHandle TargetDataDelegateHandle;
    FTimerHandle    ClientAimTimeoutTimer;
};
