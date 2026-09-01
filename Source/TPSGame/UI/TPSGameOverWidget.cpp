#include "UI/TPSGameOverWidget.h"
#include "Character/TPSPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UTPSGameOverWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RestartButton)
	{
		RestartButton->OnClicked.AddDynamic(this, &UTPSGameOverWidget::HandleRestart);
	}

}

void UTPSGameOverWidget::HandleRestart()
{
	// 실제 재시작은 서버가 수행한다. 컨트롤러가 RPC로 넘긴다.
	if (ATPSPlayerController* PC = GetOwningPlayer<ATPSPlayerController>())
	{
		PC->RequestRestart();
	}

	// 중복 클릭 방지
	if (RestartButton)
	{
		RestartButton->SetIsEnabled(false);
	}
}

void UTPSGameOverWidget::SetResult(bool bVictory)
{
	if (!ResultText) return;

	ResultText->SetText(bVictory
		? NSLOCTEXT("TPS", "Victory", "적을 모두 처치했습니다")
		: NSLOCTEXT("TPS", "Defeat", "목숨을 모두 소진했습니다"));

	ResultText->SetColorAndOpacity(FSlateColor(
		bVictory ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor(1.f, 0.4f, 0.4f)));
}
