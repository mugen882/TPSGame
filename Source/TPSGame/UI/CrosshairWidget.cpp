#include "UI/CrosshairWidget.h"
#include "Character/PlayerCharacter.h"
#include "Components/Image.h"
#include "TimerManager.h"

void UCrosshairWidget::SetOwningCharacter(APlayerCharacter* InPlayer)
{
	if (!InPlayer)
	{
		return;
	}

	Unbind();

	OwnerCharacter = InPlayer;
	InPlayer->OnWeaponChanged.AddDynamic(this, &UCrosshairWidget::HandleWeaponChanged);
	InPlayer->OnHitConfirmed.AddDynamic(this, &UCrosshairWidget::HandleHitConfirmed);

	if (InPlayer->GetWeaponManager())
	{
		// 초기 크로스헤어 동기화.
		ApplyCrosshairFor(InPlayer->GetWeaponManager()->GetCurrentWeaponType());
	}

	if (HittedImage)
	{
		HittedImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UCrosshairWidget::HandleWeaponChanged(EWeaponType WeaponType)
{
	ApplyCrosshairFor(WeaponType);
}

void UCrosshairWidget::HandleHitConfirmed()
{
	if (!HittedImage)
	{
		return;
	}

	HittedImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HitMarkerTimer, this, &UCrosshairWidget::HideHitMarker,
			HitMarkerDuration, /*loop=*/false);
	}
}

void UCrosshairWidget::ApplyCrosshairFor(EWeaponType WeaponType)
{
	if (!CrosshairImage)
	{
		return;
	}

	if (const TObjectPtr<UTexture2D>* Found = CrosshairTextures.Find(WeaponType))
	{
		if (*Found)
		{
			CrosshairImage->SetBrushFromTexture(*Found);
		}
	}
}

void UCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bHideWhenNotAiming || !CrosshairImage)
	{
		return;
	}

	const APlayerCharacter* Player = OwnerCharacter.Get();
	if (!Player)
	{
		return;
	}

	const bool bShouldShow = Player->IsAiming();
	const ESlateVisibility Desired = bShouldShow ? ESlateVisibility::HitTestInvisible
												 : ESlateVisibility::Collapsed;

	// 매 틱 SetVisibility 호출 비용을 피하려고 상태가 바뀔 때만 적용
	if (CrosshairImage->GetVisibility() != Desired)
	{
		CrosshairImage->SetVisibility(Desired);
	}
}

void UCrosshairWidget::Unbind()
{
	if (APlayerCharacter* Player = OwnerCharacter.Get())
	{
		Player->OnWeaponChanged.RemoveDynamic(this, &UCrosshairWidget::HandleWeaponChanged);
		Player->OnHitConfirmed.RemoveDynamic(this, &UCrosshairWidget::HandleHitConfirmed);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitMarkerTimer);
	}
}

void UCrosshairWidget::HideHitMarker()
{
	if (HittedImage)
	{
		HittedImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UCrosshairWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}
