#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VignetteWidget.generated.h"

class APlayerCharacter;
class UImage;
class UWidgetAnimation;

/**
	피격 피드백 위젯 클래스
	- OnPlayerDamagedDir    : 피격 방향 인디케이터 회전 + 페이드
	데이터 바인딩은 SetOwningCharacter 에서 수행한다.
 */
UCLASS()
class TPSGAME_API UVignetteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningCharacter(APlayerCharacter* InPlayer);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> VignetteImage;

	UPROPERTY(EditAnywhere, Category = "Damage")
	float VignetteDuration = 0.15f;

	// 피격 방향을 가리키는 인디케이터. 중심 피벗 기준 회전됨.
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> DirectionalIndicator;

	UPROPERTY(EditAnywhere, Category = "Damage")
	float IndicatorDuration = 0.4f;

private:
	UFUNCTION()
	void HandlePlayerDamagedDirectional(float HitAngle);

	void Unbind();

	void HideDirectionalIndicator();

	UFUNCTION()
	void HandlePlayerDamaged();

	void HideVignette();

private:
	TWeakObjectPtr<APlayerCharacter> OwnerCharacter;

	FTimerHandle IndicatorTimer;

	FTimerHandle VignetteTimer;
};
