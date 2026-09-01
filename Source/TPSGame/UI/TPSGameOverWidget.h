#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TPSGameOverWidget.generated.h"

/*
	게임오버 결과 화면.
*/
UCLASS()
class TPSGAME_API UTPSGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 승패에 따라 문구와 색을 바꾼다. 컨트롤러가 표시 직전에 호출한다.
	void SetResult(bool bVictory);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> RestartButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ResultText;

	UFUNCTION()
	void HandleRestart();
};
