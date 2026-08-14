#include "Animation/AnimNotify_WeaponSwap.h"
#include "Character/CommonCharacter.h"

void UAnimNotify_WeaponSwap::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    if (ACommonCharacter* Character = Cast<ACommonCharacter>(MeshComp->GetOwner()))
    {
        if (UWeaponManagerComponent* WM = Character->GetWeaponManager())
        {
            WM->OnWeaponSwapNotify();
        }
    }
}