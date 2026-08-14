#include "UI/VignetteWidget.h"
#include "Character/PlayerCharacter.h"
#include "Components/Image.h"

void UVignetteWidget::SetOwningCharacter(APlayerCharacter* InPlayer)
{
	if (!InPlayer)
	{
		return;
	}

	// 재빙의 등으로 중복 바인딩되지 않도록 이전 구독부터 정리
	Unbind();

	OwnerCharacter = InPlayer;
	InPlayer->OnPlayerDamagedDir.AddDynamic(this, &UVignetteWidget::HandlePlayerDamagedDirectional);
	InPlayer->OnPlayerDamaged.AddDynamic(this, &UVignetteWidget::HandlePlayerDamaged);

	if (DirectionalIndicator)
	{
		DirectionalIndicator->SetRenderOpacity(0.0f);
	}

	if (VignetteImage)
	{
		VignetteImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UVignetteWidget::HandlePlayerDamagedDirectional(float HitAngle)
{
	if (!DirectionalIndicator)
	{
		return;
	}

	DirectionalIndicator->SetRenderTransformAngle(HitAngle);
	DirectionalIndicator->SetRenderOpacity(1.0f);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			IndicatorTimer, this, &UVignetteWidget::HideDirectionalIndicator,
			IndicatorDuration, /*loop=*/false);
	}
}

void UVignetteWidget::Unbind()
{
	if (APlayerCharacter* Player = OwnerCharacter.Get())
	{
		Player->OnPlayerDamagedDir.RemoveDynamic(this, &UVignetteWidget::HandlePlayerDamagedDirectional);
		Player->OnPlayerDamaged.RemoveDynamic(this, &UVignetteWidget::HandlePlayerDamaged);  // 추가
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IndicatorTimer);
		World->GetTimerManager().ClearTimer(VignetteTimer);  // 추가
	}
}


void UVignetteWidget::HideDirectionalIndicator()
{
	if (DirectionalIndicator)
	{
		DirectionalIndicator->SetRenderOpacity(0.0f);
	}
}

void UVignetteWidget::HandlePlayerDamaged()
{
	if (!VignetteImage)
	{
		return;
	}

	VignetteImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			VignetteTimer, this, &UVignetteWidget::HideVignette,
			VignetteDuration, /*loop=*/false);
	}
}

void UVignetteWidget::HideVignette()
{
	if (VignetteImage)
	{
		VignetteImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UVignetteWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}
