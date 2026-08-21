#include "Animation/AnimNotify_WeaponSwap.h"
#include "Character/CommonCharacter.h"
#include "Common/TPSGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Common/TPSLog.h"

/*
    교체 몽타주의 "실제로 손에 무기가 바뀌는" 프레임에서 호출된다.

    노티파이는 몽타주가 재생되는 모든 머신에서 발생하지만,
    이벤트를 실제로 소비하는 것은 어빌리티가 활성화된 머신(소유 클라 + 서버)뿐이다.
*/
void UAnimNotify_WeaponSwap::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    AActor* Owner = MeshComp->GetOwner();
    if (Owner->GetLocalRole() == ROLE_SimulatedProxy) return;

    if (!MeshComp) return;

    ACommonCharacter* Character = Cast<ACommonCharacter>(MeshComp->GetOwner());
    if (!Character) return;

    UE_LOG(TPSLog, Warning, TEXT("Notify | %s | %s"),
        TPSNetDebug::NetModeToString(MeshComp->GetOwner()->GetNetMode()),
        TPSNetDebug::NetRoleToString(MeshComp->GetOwner()->GetLocalRole()));

    FGameplayEventData Payload;
    Payload.EventTag = TAG_Event_WeaponSwap;
    Payload.Instigator = Character;
    Payload.Target = Character;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, TAG_Event_WeaponSwap, Payload);
}
