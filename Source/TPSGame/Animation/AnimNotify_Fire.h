#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_Fire.generated.h"

/*
	애님노티파이로 발사 이벤트 받아 발사
*/

UCLASS(meta = (DisplayName = "Fire"))
class TPSGAME_API UAnimNotify_Fire : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};