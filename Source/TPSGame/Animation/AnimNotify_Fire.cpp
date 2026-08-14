#include "AnimNotify_Fire.h"
#include "Character/CommonCharacter.h"

void UAnimNotify_Fire::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	if (ACommonCharacter* Character = Cast<ACommonCharacter>(MeshComp->GetOwner()))
	{
		Character->OnFireNotify();
	}
}