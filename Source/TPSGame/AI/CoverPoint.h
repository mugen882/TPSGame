#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoverPoint.generated.h"

class UArrowComponent;

/**
 * 레벨에 수동 배치하는 엄폐 지점.
 * 위치 = 숨는 자리
 * EQS가 이 액터들을 후보로 정렬되는 커버를 선택한다.
 */
UCLASS()
class TPSGAME_API ACoverPoint : public AActor
{
    GENERATED_BODY()

public:
    ACoverPoint();

    /** 배치 시 화살표로 조정. */
    UFUNCTION(BlueprintPure, Category = "Cover")
    FVector GetCoverForward() const { return GetActorForwardVector(); }

protected:
    UPROPERTY(VisibleAnywhere, Category = "Cover")
    TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
    /** 에디터에서 엄폐방향을 시각화 */
    UPROPERTY(VisibleAnywhere, Category = "Cover")
    TObjectPtr<UArrowComponent> DirectionArrow;
#endif
};