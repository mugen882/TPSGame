#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_WeaponSwap.generated.h"

/*
	애님노티파이 이벤트로 무기교체 이벤트 받아 무기 교체
*/

UCLASS(meta = (DisplayName = "WeaponSwap"))
class TPSGAME_API UAnimNotify_WeaponSwap : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};