#include "TPSGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead, "State.Dead");			// 캐릭터 사망 상태
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Reloading, "State.Reloading");	// 캐릭터 재장전중
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Aiming, "State.Aiming");		// 캐릭터 조준중
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Swapping, "State.Swapping");	// 캐릭터 무기 교체중
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Fire, "Ability.Fire");		// 발사(TAG_Input_Fire_Rifle, TAG_Input_Fire_RocketLauncher, TAG_Input_Fire_MachineGun과 함께 부여된 태그로 발사 취소시 사용)
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Reload, "Input.Reload");					// 재장전 입력
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Fire_Rifle, "Input.Fire.Rifle");			// 소총 발사 입력
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Fire_RocketLauncher, "Input.Fire.RocketLauncher");	// 로켓런처 발사 입력
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Fire_MachineGun, "Input.Fire.MachineGun");	// 기관총 발사 입력
UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Fire, "Cooldown.Fire");		// 발사 쿨다운(쿨다운 지속시간 만큼 유지)
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Damage");			// 데미지
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_CooldownDuration, "Data.CooldownDuration");	// 쿨다운 지속시간