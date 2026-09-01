#include "UI/TPSHUDWidget.h"
#include "Character/PlayerCharacter.h"

#include "UI/AmmoWidget.h"
#include "UI/HealthBarWidget.h"
#include "UI/VignetteWidget.h"
#include "UI/CrosshairWidget.h"
#include "Game/TPSGameState.h"
#include "Components/TextBlock.h"

void UTPSHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToGameState();
}

void UTPSHUDWidget::InitFor(APlayerCharacter* Player)
{
	if (!Player)
	{
		return;
	}

	OwnerCharacter = Player;

	// 각 자식 위젯이 자기 필요한 델리게이트/ASC에 바인딩하도록 플레이어 참조만 넘긴다.
	if (Crosshair)
	{
		Crosshair->SetOwningCharacter(Player);
	}
	if (Ammo)
	{
		Ammo->SetOwningCharacter(Player);
	}
	if (HealthBar)
	{
		HealthBar->SetOwningCharacter(Player);
	}
	if (Vignette)
	{
		Vignette->SetOwningCharacter(Player);
	}
}

void UTPSHUDWidget::BindToGameState()
{
	ATPSGameState* GS = GetWorld()->GetGameState<ATPSGameState>();
	if (!GS)
	{
		// GameState 복제 전. 다음 틱에 재시도.
		GetWorld()->GetTimerManager().SetTimerForNextTick(
			this, &UTPSHUDWidget::BindToGameState);
		return;
	}

	GS->OnLivesChanged.AddDynamic(this, &UTPSHUDWidget::HandleLivesChanged);
	GS->OnEnemiesChanged.AddDynamic(this, &UTPSHUDWidget::HandleEnemiesChanged);

	// 바인딩 전에 이미 값이 와 있을 수 있다 (M1의 F4와 같은 형태)
	HandleLivesChanged(GS->GetRemainingLives());
	HandleEnemiesChanged(GS->GetRemainingEnemies());
}

void UTPSHUDWidget::HandleLivesChanged(int32 NewLives)
{
	if (!LivesText) return;

	LivesText->SetText(FText::FromString(FString::Printf(TEXT("남은 목숨 %d"), NewLives)));

	// 마지막 목숨이면 눈에 띄게. 팀 공용 자원이라 전원이 같은 경고를 본다.
	LivesText->SetColorAndOpacity(
		NewLives <= 1 ? FSlateColor(FLinearColor::Red) : FSlateColor(FLinearColor::White));
}
void UTPSHUDWidget::HandleEnemiesChanged(int32 NewCount)
{
	if (!EnemiesText) return;

	EnemiesText->SetText(FText::FromString(FString::Printf(TEXT("남은 적 %d"), NewCount)));
}
