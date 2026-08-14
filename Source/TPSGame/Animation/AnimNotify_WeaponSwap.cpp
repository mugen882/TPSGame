#include "Animation/AnimNotify_WeaponSwap.h"
#include "Character/CommonCharacter.h"

void UAnimNotify_WeaponSwap::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (ACommonCharacter* Character = Cast<ACommonCharacter>(MeshComp->GetOwner()))
    {
        Character->GetWeaponManager()->OnWeaponSwapNotify();
    }
}