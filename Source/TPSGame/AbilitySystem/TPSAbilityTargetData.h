#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "TPSAbilityTargetData.generated.h"

/*
	클라이언트 → 서버 조준점 전달용 TargetData

	배경:
	조준점은 카메라 기준으로 계산된다(UPlayerAimComponent::ComputeAimPoint).
	데디케이티드 서버에는 로컬 플레이어도 카메라도 없으므로, 서버가 스스로
	원격 클라이언트의 조준점을 알아낼 방법이 없다. 서버에서 GetAimPoint()를
	부르면 의미 없는 값이 나온다.
	그래서 조준점만 클라이언트가 계산해 보내고, 판정(트레이스/데미지)은
	서버가 수행한다.

	"조준점"을 보내지 "히트 결과"를 보내지 않는 것이 핵심 설계 선택이다.
	히트 결과를 보내면 지연 체감은 좋아지지만 클라이언트가 명중을 선언하는
	구조가 되어 조작에 무방비해진다.

	대신 "클라이언트가 조준한 시점의 적 위치"와 "서버의 현재 적 위치"가
	어긋나는 문제가 생기는데, ULagCompensationComponent가 사격자의 지연만큼
	히트박스를 되감아 해결한다. 서버가 판정을 수행하고 있어야 되감을 대상이
	존재하므로, 이 선택이 랙 보상의 전제이기도 하다.

	FVector_NetQuantize100: 1cm 정밀도로 압축 직렬화. 조준점에는 충분하다.
	실측 기준 발당 약 28바이트로, 같은 세션의 ServerMovePacked 대비 1% 수준이다.
*/
USTRUCT()
struct TPSGAME_API FTPSTargetData_AimPoint : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

public:
	FTPSTargetData_AimPoint() = default;
	explicit FTPSTargetData_AimPoint(const FVector& InAimPoint)
		: AimPoint(InAimPoint) {}

	UPROPERTY()
	FVector_NetQuantize100 AimPoint = FVector::ZeroVector;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FTPSTargetData_AimPoint::StaticStruct();
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FTPSTargetData_AimPoint(%s)"), *AimPoint.ToString());
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		AimPoint.NetSerialize(Ar, Map, bOutSuccess);
		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FTPSTargetData_AimPoint> : public TStructOpsTypeTraitsBase2<FTPSTargetData_AimPoint>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
