#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/WeaponManagerComponent.h"
#include "CrosshairWidget.generated.h"

class APlayerCharacter;
class UImage;
class UTexture2D;

/*
	크로스헤어 위젯 클래스
	- OnWeaponChanged : 무기별 크로스헤어 텍스처 교체
	- OnHitConfirmed : 히트마커 표시
*/
UCLASS()
class TPSGAME_API UCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningCharacter(APlayerCharacter* InPlayer);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleWeaponChanged(EWeaponType WeaponType);

	UFUNCTION()
	void HandleHitConfirmed();

	void ApplyCrosshairFor(EWeaponType WeaponType);
	void Unbind();

	void HideHitMarker();

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> HittedImage;

	// 무기 타입별 크로스헤어 텍스처.
	UPROPERTY(EditAnywhere, Category="Crosshair")
	TMap<EWeaponType, TObjectPtr<UTexture2D>> CrosshairTextures;

	// 조준하지 않을 때 크로스헤어를 숨길지 여부
	UPROPERTY(EditAnywhere, Category="Crosshair")
	bool bHideWhenNotAiming = false;

	// 히트마커 표시 시간
	UPROPERTY(EditAnywhere, Category = "HitMarker")
	float HitMarkerDuration = 0.1f;

	TWeakObjectPtr<APlayerCharacter> OwnerCharacter;

	FTimerHandle HitMarkerTimer;
};
