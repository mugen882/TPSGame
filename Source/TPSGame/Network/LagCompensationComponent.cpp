#include "Network/LagCompensationComponent.h"
#include "Components/CapsuleComponent.h"
#include "Character/CommonCharacter.h"
#include "Common/TPSLog.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

namespace
{
	static TAutoConsoleVariable<int32> CVarLagCompensationDebug(
		TEXT("TPS.LagCompensation.Debug"), 0,
		TEXT("1이면 되감은 히트박스를 디버그 캡슐로 그린다 (녹색=현재, 청록=되감은 위치)"),
		ECVF_Default);
}

ULagCompensationComponent::ULagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	/*
		이동 갱신 뒤에 기록해야 그 프레임의 최종 위치가 남는다.
		PrePhysics에 기록하면 한 프레임 낡은 값이 들어간다.
	*/
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	SetIsReplicatedByDefault(false);   // 서버 전용. 복제할 것이 없다.
}

bool ULagCompensationComponent::IsDebugDrawEnabled()
{
	return CVarLagCompensationDebug.GetValueOnAnyThread() != 0;
}

void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		Capsule = OwnerChar->GetCapsuleComponent();
	}

	/*
		히스토리는 서버만 기록한다.

		클라이언트에서 기록해봐야 되감기 판정은 서버에서만 일어나므로 쓸모가 없고,
		매 프레임 틱 비용만 든다.
	*/
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
		return;
	}

	// 링버퍼 용량 예약
	History.Reserve(FMath::CeilToInt(MaxHistorySeconds / RecordInterval) + 2);
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	if (!World || !Capsule) return;

	const float Now = World->GetTimeSeconds();

	// 프레임레이트와 무관하게 일정 밀도로 기록한다.
	if (LastRecordTime >= 0.f && (Now - LastRecordTime) < RecordInterval)
	{
		return;
	}
	LastRecordTime = Now;

	RecordFrame();
}

void ULagCompensationComponent::RecordFrame()
{
	const UWorld* World = GetWorld();
	if (!World || !Capsule) return;

	FTPSHitboxFrame Frame;
	Frame.Time = World->GetTimeSeconds();
	Frame.Location = Capsule->GetComponentLocation();
	Frame.Rotation = Capsule->GetComponentRotation();
	Frame.HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	Frame.Radius = Capsule->GetScaledCapsuleRadius();

	History.Add(Frame);

	// 오래된 스냅샷 폐기. 시간순으로 들어오므로 앞에서부터 지우면 된다.
	const float Cutoff = Frame.Time - MaxHistorySeconds;
	int32 RemoveCount = 0;
	while (RemoveCount < History.Num() && History[RemoveCount].Time < Cutoff)
	{
		++RemoveCount;
	}
	if (RemoveCount > 0)
	{
		History.RemoveAt(0, RemoveCount, EAllowShrinking::No);
	}
}

bool ULagCompensationComponent::SampleAt(float ServerTime, FTPSHitboxFrame& OutFrame) const
{
	if (History.Num() == 0) return false;

	// 요청 시각이 히스토리보다 오래됨 → 가장 오래된 것으로 클램프
	if (ServerTime <= History[0].Time)
	{
		OutFrame = History[0];
		return true;
	}

	// 요청 시각이 최신보다 미래 → 되감을 필요 없음
	if (ServerTime >= History.Last().Time)
	{
		return false;
	}

	/*
		두 스냅샷 사이를 선형 보간한다.

		60Hz로 기록해도 요청 시각이 정확히 스냅샷과 겹치는 일은 드물다.
		보간하지 않으면 최대 16ms만큼 어긋나고, 빠르게 움직이는 적에서는
		그것만으로 캡슐 반지름을 벗어난다.
	*/
	for (int32 i = History.Num() - 1; i > 0; --i)
	{
		if (History[i - 1].Time <= ServerTime && ServerTime <= History[i].Time)
		{
			const FTPSHitboxFrame& A = History[i - 1];
			const FTPSHitboxFrame& B = History[i];

			const float Span = B.Time - A.Time;
			const float Alpha = (Span > KINDA_SMALL_NUMBER)
				? FMath::Clamp((ServerTime - A.Time) / Span, 0.f, 1.f)
				: 0.f;

			OutFrame.Time = ServerTime;
			OutFrame.Location = FMath::Lerp(A.Location, B.Location, Alpha);
			OutFrame.Rotation = FMath::Lerp(A.Rotation, B.Rotation, Alpha);
			OutFrame.HalfHeight = FMath::Lerp(A.HalfHeight, B.HalfHeight, Alpha);
			OutFrame.Radius = FMath::Lerp(A.Radius, B.Radius, Alpha);
			return true;
		}
	}
	return false;
}

void ULagCompensationComponent::RewindTo(float ServerTime)
{
	if (bRewound || !Capsule) return;

	FTPSHitboxFrame Frame;
	if (!SampleAt(ServerTime, Frame))
	{
		return;   // 되감을 데이터가 없음. 현재 위치로 판정한다.
	}

	SavedLocation = Capsule->GetComponentLocation();
	SavedRotation = Capsule->GetComponentRotation();
	bRewound = true;

	/*
		콜리전을 실제로 과거 위치로 옮긴다.

		같은 프레임 안에서 즉시 복원하므로 다른 시스템에 영향이 없고
		기존 LineTraceSingleByChannel을 그대로 재사용할 수 있다.
	*/
	Capsule->SetWorldLocationAndRotation(Frame.Location, Frame.Rotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (IsDebugDrawEnabled())
	{
		const UWorld* World = GetWorld();
		// 현재 위치(녹색) vs 되감은 위치(청록)
		DrawDebugCapsule(World, SavedLocation, Frame.HalfHeight, Frame.Radius,
			SavedRotation.Quaternion(), FColor::Green, false, 2.0f);
		DrawDebugCapsule(World, Frame.Location, Frame.HalfHeight, Frame.Radius,
			Frame.Rotation.Quaternion(), FColor::Cyan, false, 2.0f);
	}
}

void ULagCompensationComponent::Restore()
{
	if (!bRewound || !Capsule) return;

	Capsule->SetWorldLocationAndRotation(SavedLocation, SavedRotation, false, nullptr, ETeleportType::TeleportPhysics);
	bRewound = false;
}

// ---------------------------------------------------------------- Scope

FTPSLagCompensationScope::FTPSLagCompensationScope(UWorld* World, ACommonCharacter* ExcludeCharacter, float RewindSeconds)
{
	if (!World || RewindSeconds <= 0.f) return;

	const float TargetTime = World->GetTimeSeconds() - RewindSeconds;

	/*
		사격자를 제외한 모든 캐릭터를 되감는다.

		사격자 본인은 클라이언트가 예측으로 조종하므로 서버와 이미 동기화되어 있고,
		되감으면 오히려 자기 총구 위치가 과거로 가서 트레이스 시작점이 틀어진다.

		최적화 여지: 총구~조준점 구간 근처만 되감도록 좁히면 캐릭터가 많을 때 비용이 준다.
		             지금은 캡슐 이동 두 번이라 전수로 돌려도 부담이 없다.
	*/
	for (TActorIterator<ACommonCharacter> It(World); It; ++It)
	{
		ACommonCharacter* Char = *It;
		if (!Char || Char == ExcludeCharacter) continue;

		/*
			아군은 되감지 않는다.

			히트스캔이 아군에게 들어가지 않으므로(ApplyDamageEffect의 팀 필터)
			되감아도 판정에 영향이 없다.
		*/
		if (!ACommonCharacter::AreHostile(ExcludeCharacter, Char)) continue;

		ULagCompensationComponent* Comp = Char->FindComponentByClass<ULagCompensationComponent>();
		if (!Comp) continue;

		Comp->RewindTo(TargetTime);
		RewoundComponents.Add(Comp);
	}
}

FTPSLagCompensationScope::~FTPSLagCompensationScope()
{
	for (TWeakObjectPtr<ULagCompensationComponent>& Comp : RewoundComponents)
	{
		if (Comp.IsValid())
		{
			Comp->Restore();
		}
	}
}
