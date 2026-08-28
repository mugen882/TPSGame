#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LagCompensationComponent.generated.h"

class UCapsuleComponent;

/*
	랙 보상용 히트박스 스냅샷 한 프레임.

	캡슐만 저장하는 이유:
	이 프로젝트는 ECC_Weapon 채널을 CapsuleComponent만 Block한다.
	부위별 판정이 없으므로 본 트랜스폼을 전부 저장할 필요가 없다.
	캐릭터당 프레임 하나가 약 40바이트로 끝난다.
*/
USTRUCT()
struct FTPSHitboxFrame
{
	GENERATED_BODY()

	/*
		스냅샷을 찍은 시각. UWorld::GetTimeSeconds()이며 머신마다 기준이 다르다.
		기록도 소비도 서버 안에서만 일어나므로 절대 기준을 맞출 필요가 없다.
		(클라이언트와 시각을 주고받아야 한다면 GameState의 ServerWorldTimeSeconds를 써야 한다)
	*/
	float Time = 0.f;

	FVector  Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float    HalfHeight = 0.f;
	float    Radius = 0.f;
};

/*
	랙 보상 (서버 되감기)

	문제:
	클라이언트가 화면에서 본 적의 위치는 서버의 현재 위치가 아니다.
	  - 서버 -> 클라 위치 복제 지연 (RTT/2)
	  - 클라의 이동 보간/스무딩 지연
	  - 클라 -> 서버 조준점 전송 지연 (RTT/2)
	그래서 서버가 "현재" 히트박스로 트레이스하면, 움직이는 적을 정조준해도 빗나간다.
	클라이언트는 적의 진행 방향 앞을 조준해야 맞는다.

	해결:
	서버가 각 캐릭터의 히트박스 위치를 시간과 함께 보관해두고, 발사 판정 시
	사격자의 지연만큼 되감아 그 시점의 히트박스로 트레이스한다.

	이 구조가 가능한 이유는 D14에서 클라이언트가 "히트 결과"가 아니라 "조준점"을
	보내도록 했기 때문이다. 서버가 판정을 하고 있어야 되감을 대상이 존재한다.
*/
UCLASS(ClassGroup = (TPS), meta = (BlueprintSpawnableComponent))
class TPSGAME_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULagCompensationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/*
		RewindTime 시점의 히트박스로 캡슐을 옮긴다.
		되감기 전 상태를 내부에 보관하므로 반드시 Restore와 짝을 이뤄야 한다.
		(직접 부르지 말고 FTPSLagCompensationScope를 사용할 것)
	*/
	void RewindTo(float ServerTime);
	void Restore();

	// 되감은 위치를 디버그 드로우로 표시할지
	static bool IsDebugDrawEnabled();

private:
	// 두 스냅샷 사이를 보간해 정확한 시점의 히트박스를 만든다.
	bool SampleAt(float ServerTime, FTPSHitboxFrame& OutFrame) const;

	void RecordFrame();

private:
	/*
		링버퍼.

		MaxHistorySeconds 동안의 스냅샷만 보관한다. 되감기 한도(MaxRewindSeconds)보다
		넉넉히 잡아야 경계에서 샘플이 모자라지 않는다.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "LagCompensation")
	float MaxHistorySeconds = 1.0f;

	// 기록 주기. 프레임레이트와 무관하게 일정한 밀도를 유지한다.
	UPROPERTY(EditDefaultsOnly, Category = "LagCompensation")
	float RecordInterval = 1.0f / 60.0f;

	TArray<FTPSHitboxFrame> History;
	float LastRecordTime = -1.f;

	// 되감기 전 원래 위치. Restore가 이 값을 되돌린다.
	bool     bRewound = false;
	FVector  SavedLocation = FVector::ZeroVector;
	FRotator SavedRotation = FRotator::ZeroRotator;

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> Capsule = nullptr;
};

/*
	되감기 RAII 스코프.

	트레이스 도중 조기 반환이나 예외가 있어도 반드시 복원되도록 스코프로 감싼다.
	복원에 실패하면 캐릭터가 과거 위치에 영구히 박히므로 수동 호출은 위험하다.

	사격자 본인은 되감지 않는다. 자기 캐릭터는 클라이언트가 예측으로 직접 조종하므로
	서버와 이미 동기화되어 있고, 되감으면 오히려 자기 총구 위치가 틀어진다.
*/
struct TPSGAME_API FTPSLagCompensationScope
{
	FTPSLagCompensationScope(UWorld* World, class ACommonCharacter* ExcludeCharacter, float RewindSeconds);
	~FTPSLagCompensationScope();

	// 실제로 되감았는지 (디버그/로그용)
	bool WasRewound() const { return RewoundComponents.Num() > 0; }

private:
	TArray<TWeakObjectPtr<ULagCompensationComponent>> RewoundComponents;
};
