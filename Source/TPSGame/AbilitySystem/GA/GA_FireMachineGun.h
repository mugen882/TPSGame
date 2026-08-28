#pragma once
#include "CoreMinimal.h"
#include "AbilitySystem/GA/GA_FireBase.h"
#include "GA_FireMachineGun.generated.h"

/*
    머신건 발사 클래스

    연사 전용 로직이 전부 사라졌다.

    이전에는 어빌리티 1회 활성화 + FTimerManager 루프로 연사를 구현했다.
    그 구조에서는 모든 발사가 단일 예측 키를 공유하기 때문에
      - CommitAbility가 한 번만 호출되어 Cost가 첫 발에만 적용되고
      - 쿨다운을 GAS 대신 타이머가 대신하며
      - TargetData 슬롯이 덮어써져 조준점을 발당으로 짝지을 수 없었다.

    이제 발사 1회 = 어빌리티 활성화 1회다.
    입력 홀드 반복은 APlayerCharacter::Tick이 담당하고,
    발사 간격은 쿨다운 GE(FireInterval을 SetByCaller로 주입)가 강제한다.
    덕분에 라이플과 완전히 같은 경로를 타며, 이 클래스에는
    "어떤 입력 태그로 활성화되는가"만 남았다.
*/

UCLASS()
class TPSGAME_API UGA_FireMachineGun : public UGA_FireBase
{
    GENERATED_BODY()
public:
    UGA_FireMachineGun();
};
