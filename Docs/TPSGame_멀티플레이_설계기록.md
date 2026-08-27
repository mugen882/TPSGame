# TPSGame 코옵 멀티플레이 — 설계 결정 기록

싱글플레이로 완성된 UE5 TPS 포트폴리오에 코옵 멀티플레이 계층을 얹는 작업의 기록.
"무엇을 만들었나"보다 **"왜 그렇게 결정했나"**와 **"무엇이 틀렸고 어떻게 알았나"**를 남기는 데 목적이 있다.

- 엔진: UE 5.5.4 (소스 빌드)
- 대상: 2~4인 코옵, 데디케이티드 서버
- 기반: GAS (GameplayAbilitySystem)

---

## 1. 출발점

싱글플레이 코드베이스는 아키텍처가 정리된 상태였다. 갓클래스는 컴포넌트로 분해되어 있었고(`UWeaponManagerComponent` / `UPlayerAimComponent` / `UEnemyCombatComponent`), GAS 기반 전투와 Behavior Tree + EQS 기반 AI가 동작했다.

문제는 **네트워크 개념이 코드에 단 한 줄도 없었다**는 것이다. 전체 5,000여 줄에서 `HasAuthority()`가 한 번, `Replicated` UPROPERTY가 0개, RPC가 0개였다.

이것은 "네트워크 미구현" 상태가 아니라 **모든 것이 암묵적으로 클라이언트 권위로 짜인** 상태였다. 무기 액터가 각 머신에서 독립적으로 스폰되고, 히트스캔 트레이스가 어느 머신에서든 실행되고, 데미지가 로컬에서 직접 적용되었다.

### 마일스톤 구성

| | 범위 | 상태 |
|---|---|---|
| M0 | GAS 네트워크 기반 (어트리뷰트 복제, 초기화 경로, ReplicationMode) | 완료 |
| M1 | 무기 교체 서버 권위화 | 완료 |
| M2a | 탄약을 GAS 어트리뷰트로 이관 | 완료 |
| M2b-1 | 발사 서버 권위화 (TargetData, 서버 검증) | 완료 |
| M2b-2 | 발사 연출을 GameplayCue로 이관 | 완료 |
| M2b-3 | 머신건 연사를 발당 어빌리티 활성화로 재설계 | 완료 |
| M3 | 사망/리스폰 동기화 | 예정 |
| M4 | 랙 보상 (서버 되감기) | 예정 |
| M5 | 데디케이티드 서버 운영 검증, 대역폭 측정 | 예정 |

각 마일스톤은 **독립적으로 검증 가능**하도록 잘랐다. M2는 원래 하나였으나 탄약(M2a)과 발사 권위화(M2b)를 분리했고, M2b는 다시 판정(b-1) / 연출(b-2) / 연사(b-3)로 쪼갰다. 한꺼번에 하면 버그 발생 시 원인 분리가 불가능하기 때문이다.

---

## 2. 설계 결정 기록

### D1. ASC를 PlayerState가 아닌 Character에 유지

**맥락** — GAS 표준 관례는 플레이어의 ASC를 `APlayerState`에 두는 것이다. 리스폰 시 어트리뷰트와 이펙트가 유지되고, 캐릭터 파괴와 무관하게 살아남는다.

**결정** — `ACommonCharacter`에 그대로 둔다.

**근거**
- 적(`AEnemyCharacter`)이 같은 베이스 클래스를 쓰는데 PlayerState가 없다. 이관하면 플레이어와 적의 초기화 경로가 갈라진다.
- 웨이브 디펜스 코옵에서는 리스폰 시 어트리뷰트를 재초기화하면 충분하다.
- 기존 코드 전체가 ASC를 아바타에서 찾는 것을 전제로 한다.

**트레이드오프** — 리스폰 간 지속되어야 할 상태(누적 점수, 영구 버프)가 생기면 재검토 필요.

---

### D2. `InitAbilityActorInfo`를 세 경로로 분리

**맥락** — 기존 코드는 `PossessedBy()`에서만 ASC를 초기화했다. `PossessedBy`는 **서버에서만 호출된다.**

결과적으로 클라이언트에서는 ActorInfo가 영영 초기화되지 않았고, 같은 블록에서 하던 어트리뷰트 변경 델리게이트 바인딩도 되지 않아 **클라이언트 HUD가 체력 변화를 전혀 감지하지 못했다.**

**결정** — 공용 `InitAbilityActorInfoAndBind()`를 분리하고 세 곳에서 호출한다.

| 경로 | 대상 |
|---|---|
| `PossessedBy()` | 서버 |
| `OnRep_PlayerState()` | 플레이어 클라이언트 |
| `BeginPlay()` | PlayerState가 없는 캐릭터(AI) |

**검증** — 로그로 실제 호출 순서를 확인한 결과:

```
[CL|Simulated |Client] BP_EnemyCharacter    (from BeginPlay)         BOUND
[CL|Autonomous|Client] BP_PlayerCharacter_0 (from BeginPlay)         BOUND
[CL|Autonomous|Client] BP_PlayerCharacter_0 (from OnRep_PlayerState) skip
```

- 적에게는 `OnRep_PlayerState`가 **아예 오지 않는다.** BeginPlay 경로가 없었다면 적 체력바가 클라이언트에서 영원히 갱신되지 않았을 것이다.
- 플레이어도 `BeginPlay`가 `OnRep_PlayerState`보다 먼저 왔다.

**교훈** — 초기화 콜백 순서에 의존하지 말고 **여러 진입점을 멱등하게** 만든다.

---

### D3. ASC ReplicationMode를 역할별로 분리

기존 코드는 `SetReplicationMode()` 호출이 없어 기본값 `Full`로 동작하고 있었다.

| 대상 | 모드 | 근거 |
|---|---|---|
| 플레이어 | `Mixed` | 소유 클라만 자기 GE 상세(남은 시간, 스택)를 알면 된다 |
| 적 / 기타 | `Minimal` | GameplayTag와 Cue만 복제. 어트리뷰트는 AttributeSet이 별도로 내린다 |

---

### D4. `Damage` 메타 어트리뷰트는 복제하지 않는다

**맥락** — `Damage`는 캐릭터의 상태가 아니라, GE에서 `PostGameplayEffectExecute`로 피해량을 넘기기 위한 1회성 운반 수단이다. 값을 읽은 즉시 0으로 리셋된다.

**근거** — 값이 0이 아닌 구간은 GE 실행 콜스택 안에서만 존재한다. NetUpdate 시점에는 이미 0으로 돌아와 있으므로, 복제를 걸어도 **클라이언트는 영원히 0만 받는다.**

이것은 대역폭 절약 문제가 아니라 **범주 오류**다. 순간 이벤트를 상태 채널로 보내려는 시도이기 때문이다. 더 나쁜 것은 `ReplicatedUsing`이 붙어 있으면 다음에 코드를 읽는 사람이 "클라이언트가 이 값을 관측할 수 있다"고 착각하고, 그 위에 `OnRep_Damage`에서 데미지 숫자를 띄우는 코드를 짜게 된다는 점이다.

**클라이언트에 피해량을 알려야 한다면** GameplayCue의 `RawMagnitude`를 쓴다.

---

### D5. 어트리뷰트에 `REPNOTIFY_Always`

기본값 `REPNOTIFY_OnChanged`는 도착한 값이 로컬 값과 다를 때만 `OnRep`을 호출한다. 클라이언트가 어빌리티를 예측 실행하며 어트리뷰트를 미리 바꿔놓은 경우, 서버 값이 도착해도 **이미 같은 값이라 `OnRep`이 생략**된다. 그러면 ASC가 "서버 값이 도착했다"는 사실 자체를 모르게 되어, 예측으로 적용해둔 변경을 서버 확정값 기준으로 정리하는 절차가 생략된다.

---

### D6. 무기 액터를 복제하지 않는다 ★

**맥락** — 초기 M1 계획은 무기 액터에 `bReplicates = true`를 켜고, `TMap`을 복제 가능한 슬롯 배열로 바꾸고, `CurrentWeapon` 포인터를 복제하는 것이었다.

그런데 M2 설계를 먼저 진행하면서 **탄약이 무기 액터를 떠나 어트리뷰트로 간다**는 결론이 나왔다. 그러자 질문이 바뀌었다 — 무기 액터에 복제할 상태가 남아 있는가?

**남은 것은 전부 BP CDO 설정값이었다.** `MaxAmmo`, `FireInterval`, `FireRange`, `BaseDamage`, VFX 레퍼런스. 모든 머신이 이미 동일하게 갖고 있는 값들이다.

**결정** — 무기 액터는 각 머신이 로컬로 스폰하고, **"어떤 종류를 들고 있는가"만 `EWeaponType` 1바이트로 복제**한다.

**효과** — 4인 코옵 기준 액터 채널 12개(캐릭터당 무기 3자루) → 0개.

**성립하는 이유** — 서버의 무기 액터와 클라이언트의 무기 액터는 별개 객체지만 상관없다. 권위 로직(트레이스, 데미지)은 전부 서버가 자기 무기로 수행하고, 클라이언트 무기는 순수하게 시각·설정 용도이기 때문이다.

**한계** — 무기 줍기/버리기를 추가하면 진짜 복제 액터로 되돌려야 한다.

**교훈** — **M2를 먼저 설계한 것이 M1을 절반 이하로 줄였다.** 설계 순서와 구현 순서를 일치시킬 필요가 없다.

---

### D7. 무기 교체를 GameplayAbility로

**맥락** — 기존 `UWeaponManagerComponent::EquipWeapon()`이 몽타주 재생, 상태 태그, 발사 취소를 직접 처리했다. 두 가지가 네트워크에서 깨진다.

1. `AddLooseGameplayTag(State.Swapping)`은 **복제되지 않는다.**
2. `AnimInstance->Montage_Play()`는 로컬 재생이라 **다른 클라이언트에 보이지 않는다.**

**결정** — `UGA_SwapWeapon`을 만들고 `NetExecutionPolicy = LocalPredicted`로 설정한다.

| 문제 | GAS의 해결 |
|---|---|
| 상태 태그 비복제 | `ActivationOwnedTags` — 어빌리티가 실행되는 머신(소유 클라 + 서버)에 적용. 태그를 검사하는 곳이 정확히 그 두 머신이므로 충분 |
| 몽타주 비복제 | `UAbilityTask_PlayMontageAndWait` — 시뮬레이티드 프록시까지 복제 |
| 클라→서버 전달 | `TryActivateAbilitiesByTag`가 `ServerTryActivateAbility`를 자동 전송하고, 클라는 응답을 기다리지 않고 예측 실행한다. 서버는 검증 후 `ClientActivateAbilitySucceed`/`Failed`로 예측 키를 확정하거나 폐기시킨다. **커스텀 RPC 불필요** |

**무기 종류 전달 방식** — 무기별로 BP 자식을 만들고 각각 `TargetWeaponType`과 `InputTag`를 다르게 설정했다. **"어떤 스펙이 활성화되었는가" 자체가 무기 종류를 실어 나른다.**

**데디케이티드 서버 대비** — 실제 교체 타이밍은 몽타주 노티파이가 보내는 GameplayEvent로 받되, **몽타주 종료를 폴백으로 둔다.**

---

### D8. 탄약을 GAS 어트리뷰트로 이관 ★

**맥락** — 기존 `AWeaponBase::CurrentAmmo`(int32)가 `Fire()` 안에서 감소했다. 어빌리티는 클라이언트와 서버 양쪽에서 실행되므로 탄약이 두 벌로 갈라진다.

**선택지**

| | 예측 | 무기별 독립 | 비용 |
|---|---|---|---|
| A. 무기 액터에 `Replicated int32` | ✗ | ✓ | 가장 쉬움 |
| B. `Ammo` 어트리뷰트 1개 | ✓ | ✗ | 무기 교체 시 풀 공유 |
| **C. 무기별 어트리뷰트 1개** | ✓ | ✓ | 어트리뷰트 1개 + Cost GE 1개 |

**결정** — C. 성립하는 결정적 조건은 `GA_FireRifle` / `GA_FireMachineGun` / `GA_FireRocket`이 **이미 별도 클래스**라는 점이었다. 각자 자기 `CostGameplayEffectClass`를 가지면 끝난다.

**효과** — 기존 코드의 `CommitAbility()` 한 줄이 그대로 두 가지를 처리하게 되었다.

- `CheckCost` → 탄약 부족이면 **어빌리티가 활성화조차 되지 않음** (기존 `CanFire()` 대체)
- `ApplyCost` → 예측 차감

**무기와 어트리뷰트 연결** — `virtual FGameplayAttribute GetAmmoAttribute()`로 무기가 자기 탄약 풀을 알려준다. 이 switch는 4곳(`TryFire`, `GA_Reload`, `BTTask_FireWeapon`, `InitAmmoAttributes`)에 복사될 뻔했다.

> **⚠️ 시점 주의** — M2a 시점에서는 예측이 실제로 동작하지 않았다. `GA_FireBase`가 아직 `LocalOnly`라 서버가 발사를 알지 못했고, 서버 탄약이 변하지 않으니 복제도 발생하지 않았다. 클라의 로컬 변경이 교정되지 않고 남아 **겉보기에만 정상**이었다. 예측은 M2b-1에서 `LocalPredicted`로 올린 뒤에야 실제로 켜졌다. M2a는 "예측 가능한 형태로의 리팩터링"이었고, M2b-1이 스위치를 켠 작업이다.

---

### D9. 탄약은 `COND_OwnerOnly`, 체력은 `COND_None`

체력은 적 머리 위 체력바 때문에 모두가 봐야 한다. 탄약은 **누구도 남의 것을 볼 필요가 없다.**

부수 효과로 적 캐릭터는 소유 커넥션이 없으므로 탄약이 네트워크로 아예 나가지 않는다. 4인 코옵 기준 탄약 트래픽이 1/4로 줄어든다.

---

### D10. `MaxAmmo`는 어트리뷰트로 만들지 않는다

무기 BP CDO의 설정값이라 모든 머신이 이미 동일하게 알고 있다.

부작용으로 `PreAttributeChange`에서 탄약 상한 클램프를 할 수 없게 되었다. AttributeSet이 `WeaponManager`를 들여다보면 계층이 뒤집히므로, **하한(0)만 AttributeSet에서 막고 상한은 값을 아는 쪽(재장전 시점)에서 처리**하도록 나눴다.

---

### D11. 재장전은 예측하지 않는다

- `Instant` GE의 Override는 예측 롤백이 까다롭다.
- 재장전 몽타주가 1초 이상 도는데 마지막에 왕복 지연 RTT/2(50ms)를 기다리는 것은 체감되지 않는다.

**예측은 실제로 필요한 곳에만 쓴다.**

또한 충전에 GE를 쓰지 않고 `SetNumericAttributeBase`를 직접 호출했다. "MaxAmmo로 채운다"는 동작에 GE가 더해주는 것(수정자, 스택, 지속시간)이 없기 때문이다.

단, `GA_Reload`의 `NetExecutionPolicy`는 `LocalPredicted`로 올려야 했다. 기본값 `LocalOnly`에서는 **서버가 재장전 사실을 전혀 알지 못해** 탄약을 채울 수 없기 때문이다.

---

### D12. 적도 탄약을 사용한다 (Cost 면제하지 않음)

설계 초안에서는 "적은 Cost를 면제하자"고 했으나, 코드를 확인하니 **적도 탄약을 소모하고 재장전한다.** 면제하면 적이 영원히 재장전하지 않아 전투 리듬이 바뀐다.

**대신 리로드 트리거 위치를 옮겨야 했다.** Cost가 붙으면 탄약 0일 때 어빌리티가 활성화되지 않으므로, 몽타주 노티파이(`OnFireNotify`)에 있던 재장전 코드가 도달 불가능한 죽은 코드가 된다. `BTTask_FireWeapon`으로 이동했다.

**교훈** — 게이트를 앞단으로 옮기면 그 뒤에 있던 코드가 조용히 죽는다. Cost/Cooldown 같은 활성화 조건을 추가할 때는 **그 어빌리티가 실행되어야만 도달하던 코드**를 함께 점검해야 한다.

---

### D13. 데디케이티드 서버를 M5가 아닌 M1부터

**당초 계획** — M1~M4는 리슨 서버(PIE)로 개발하고, M5에서 데디케이티드 서버 빌드.

**변경** — 개발 초기부터 PIE `Play As Client`(데디케이티드 서버)를 기본 환경으로.

**근거** — 리슨 서버는 호스트가 서버이면서 동시에 로컬 플레이어라, **서버 코드가 "로컬 플레이어가 있어서" 우연히 동작하는 것을 가려준다.** 실제로 전환 즉시 세 가지가 드러났다(F2, F5, F6).

**주의** — PIE의 데디케이티드 서버는 완전한 데디케이티드가 아니다. `Run Under One Process`면 서버 월드가 에디터 프로세스 안에서 돌아 렌더링 리소스가 살아 있고, `WITH_EDITOR` 코드가 존재하며, 에셋 쿠킹이 다르다. **일상 개발은 PIE, 마일스톤마다 패키징된 Server 타겟으로 스모크 테스트**하는 리듬이 맞다.

---

### D14. 클라이언트는 "조준점"을 보내지 "히트 결과"를 보내지 않는다 ★

**맥락** — 조준점은 카메라 기준으로 계산된다(`UPlayerAimComponent::ComputeAimPoint`). 데디케이티드 서버에는 로컬 플레이어도 카메라도 없으므로 서버가 스스로 원격 클라이언트의 조준점을 알아낼 방법이 없다.

**선택지**

| | 지연 체감 | 검증 가능성 |
|---|---|---|
| 히트 결과를 보낸다 | 좋음 | **불가** — 클라가 명중을 선언 |
| **조준점만 보낸다** | 나쁨(랙 보상 전까지) | 가능 |

**결정** — 조준점만 보내고 서버가 트레이스한다. `FTPSTargetData_AimPoint`(`FVector_NetQuantize100`)를 `FScopedPredictionWindow` 안에서 `ServerSetReplicatedTargetData`로 전송.

**근거** — 히트 결과를 받으면 서버가 검증할 대상이 사실상 없어진다. 조준점만 받으면 "클라가 조준한 시점의 적 위치 ≠ 서버의 현재 위치" 문제가 생기는데, **그것이 정확히 M4 랙 보상이 풀 문제다.** 이 선택을 해야 M4에 할 일이 생긴다.

**서버 검증** — `ValidateAimPoint`가 사거리 초과를 클램프하고 후방 조준을 거부한다. 총구 위치는 **서버 자기 무기에서** 계산하며 클라 전달값을 쓰지 않는다.

여기에 `LocalPredicted`가 자동으로 딸려주는 방어선이 있다. `ServerTryActivateAbility`를 받으면 서버가 `CanActivateAbility` / `CheckCost`를 **처음부터 다시** 돌린다. 클라의 판정 결과를 전달받지 않기 때문에, 탄약 없이 쏘거나 재장전 중 쏘는 조작 요청이 여기서 걸린다.

**부수 조치** — 조준점이 유실되면 `InstancedPerActor` 어빌리티가 활성 상태로 매달려 **그 무기로 영영 발사할 수 없게 된다.** 1초 타임아웃을 넣었다.

---

### D15. GameplayCue를 `IGameplayCueInterface`로 구현 ★

**맥락** — M2b-1까지 연출은 `AWeaponBase::PlayFireCosmetic()`이 호출된 머신에서만 실행되었다. 적 발사는 서버에서만 실행되므로 데디케이티드 서버에서는 **아무에게도 보이지 않았고**, 플레이어 발사도 자기 화면에만 보였다.

**선택지**

| | 등록 방식 |
|---|---|
| `UGameplayCueNotify_Static` 파생 | `/Game` 애셋 스캔 + 애셋 이름에서 태그 유도 |
| **`IGameplayCueInterface` 구현** | 함수 이름을 리플렉션으로 조회 |

**첫 번째를 먼저 시도했고 실패했다.** 스캔 대상이 되도록 BP 자식(`GC_Weapon_Fire`)을 만들고 `Gameplay Cue Notify Paths`에 `/Game`을 추가했지만 태그가 등록되지 않았다. 원인은 네이티브 태그의 초기화 순서다 — `UE_DEFINE_GAMEPLAY_TAG`로 만든 태그의 등록 시점보다 CDO 생성이 먼저 일어나면 생성자에서 지정한 `GameplayCueTag`에 무효 태그가 박히고, 그러면 `DeriveGameplayCueTagFromAssetName()`이 "이미 태그가 있다"고 판단해 이름 기반 유도까지 건너뛴다. BP 자식은 부모의 잘못된 값을 상속받으므로 애셋 이름을 규칙대로 지어도 소용이 없었다. 생성자의 태그 지정을 제거해 이름 기반 유도에 맡기는 것도 시도했으나 실행되지 않았다.

애셋 스캔·이름 규칙·초기화 순서 어디에도 의존하지 않는 두 번째 방식으로 전환했고, Notify 클래스와 BP, `/Game` 스캔 경로 설정은 모두 제거했다.

**결정** — `ACommonCharacter`가 `IGameplayCueInterface`를 구현한다.

```
GameplayCue.Weapon.Fire   →  void GameplayCue_Weapon_Fire(EGameplayCueEvent::Type, const FGameplayCueParameters&)
GameplayCue.Weapon.Impact →  void GameplayCue_Weapon_Impact(...)
```

**함수 이름이 곧 태그다.** 애셋 스캔·이름 규칙·초기화 순서에 전혀 의존하지 않는다.

**부수 이점** — Cue가 하는 일이 결국 `GetCurrentWeapon()->ShowMuzzleFlash()`라, 캐릭터가 직접 처리하는 편이 자연스럽고 클래스 2개가 통째로 사라졌다. 무기별 Cue 애셋을 만들 필요도 없다 — Cue 핸들러가 대상 액터에서 현재 무기를 찾아 그 BP 설정을 그대로 쓴다.

---

### D16. 예측 키로 Cue 중복 재생을 막는다

**맥락** — 사격자는 예측으로 이미 연출을 재생했다. 서버가 멀티캐스트하면 두 번 보게 된다.

**결정** — GAS의 내장 메커니즘에 맡긴다.

```cpp
// UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_Implementation
if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
{
    InvokeGameplayCueEvent(...);
}
```

멀티캐스트 RPC는 **모든 클라이언트에 도착하지만, 재생 여부는 수신 측이 각자 결정한다.** 사격자는 "내가 만든 키"임을 알아보고 건너뛴다.

성립 조건은 **서버가 클라의 예측 키를 그대로 실어 보내는 것**이다. 이 스코프를 빼면 무효 키가 나가고 모든 클라에서 `IsLocalClientKey()`가 false가 되어 사격자가 두 번 본다.

적은 예측 키가 없는 것이 맞다 — 아무도 예측하지 않았으므로 전원이 재생해야 한다.

---

### D17. 판정과 연출의 책임을 분리

**맥락** — Cue를 예측 키와 함께 실행해야 하는데, **무기는 예측 키를 알지 못한다.**

**결정** — `FireInternal`이 `FHitResult& OutHit`을 반환하도록 바꾸고, Cue 실행은 키를 가진 어빌리티가 담당한다.

| | 역할 |
|---|---|
| `AWeaponBase::FireAuthoritative(Aim, Ctrl, OutHit)` | 서버 판정. 명중 지점 반환 |
| `AWeaponBase::TracePredictedImpact(Aim, OutHit)` | 판정 없는 예측 트레이스 |
| `UGA_FireBase::FireAuthoritative` | 서버 Cue 실행 |
| `UGA_FireBase::PredictFireCues` | 클라 예측 Cue 실행 |

클라가 판정 없는 별도 트레이스를 도는 것은 낭비처럼 보이지만, **연출은 즉시 / 판정은 권위**를 분리하는 것이 슈터의 표준 구조다. 그러지 않으면 탄착이 왕복 지연(RTT) 뒤에 보인다.

---

### D18. 로켓만 Cue 대신 액터 복제

**결정** — `AProjectile`에 `bReplicates = true` + `SetReplicateMovement(true)`, 폭발은 `Multicast_PlayImpactFX`.

**근거**
- 히트스캔과 달리 비행 시간이 있어 **"날아가는 모습" 자체가 연출**이다. Cue로 흉내내는 것보다 액터를 복제하는 편이 간단하고 정확하다.
- VFX/사운드 에셋이 투사체 BP 설정이라 Cue 파라미터에 억지로 실어야 하는데, 액터가 이미 모든 머신에 존재하니 그럴 이유가 없다.

**부수 조치**
- `OnHit`에 `HasAuthority()` 게이트 — 클라 사본도 자체 이동 중 충돌 이벤트를 받는다. 없으면 데미지 이중 적용이나 서버보다 먼저 사라지는 문제가 생긴다.
- 즉시 `Destroy()`하면 방금 보낸 멀티캐스트가 유실될 수 있어 숨김 + 0.2초 수명으로 처리.
- `GetImpactVFX()` / `GetImpactSound()`를 가상 함수로 분리 — 로켓은 `ExplosionVFX`를 따로 갖는다.

---

### D19. 연사를 "어빌리티 1회 + 타이머"에서 "발당 활성화"로 ★

**맥락** — 머신건은 어빌리티를 한 번 활성화한 뒤 `FTimerManager` 루프로 연사를 돌렸다. 싱글플레이에서는 자연스러운 구조지만 네트워크에서는 **모든 발사가 단일 예측 키를 공유**하게 된다. 그 결과 세 가지가 동시에 깨졌다.

| 항목 | 원인 |
|---|---|
| Cost가 첫 발만 적용 | `CommitAbility`가 활성화 1회에만 호출됨 |
| 쿨다운이 GAS 밖 | `ApplyCooldown`을 비워두고 타이머가 발사 간격을 대신함 |
| 조준점 짝짓기 불가 | TargetData 슬롯이 `(SpecHandle, PredKey)` 쌍당 하나뿐이라 계속 덮어써짐 |

**뿌리가 하나였다.** M2b-2에서는 조준점을 캐시하고 서버 루프가 발사를 주도하게 하는 응급 대응으로 넘겼지만, 그것은 증상 완화였다.

**결정** — **발사 1회 = 어빌리티 활성화 1회.**

- 타이머 루프 제거. 어빌리티는 한 발 쏘고 즉시 종료한다.
- 입력 홀드 반복은 `APlayerCharacter::Tick`이 담당한다.
- 발사 간격은 쿨다운 GE(`FireInterval`을 SetByCaller로 주입)가 강제한다.

**효과** — 발당 예측 키가 생기면서 Cost·쿨다운·조준점이 전부 라이플과 같은 경로를 타게 되었다. `UGA_FireMachineGun`이 **156줄에서 39줄로 줄었고, 클래스에 생성자 하나만 남았다.**

**매 틱 활성화 시도의 비용** — 쿨다운 중의 시도는 `CanActivateAbility`에서 태그 검사만으로 걸러지고 서버로 RPC가 나가지 않는다. 사실상 무료다.

**교훈** — 싱글플레이에서 자연스러웠던 "1회 활성화 + 내부 루프" 패턴이 네트워크에서는 예측 단위를 뭉개버린다. **예측의 단위와 게임플레이의 단위를 일치시켜야** GAS의 기계장치가 작동한다.

---

### D20. 예측이 불가능한 무기는 예측을 포기한다

**맥락** — 머신건은 `FMath::VRandCone`으로 난수 퍼짐을 적용한다. 클라이언트가 아무리 정확히 트레이스해도 서버와 다른 지점을 맞춘다.

**선택지**

| | 비용 |
|---|---|
| 예측 키를 난수 시드로 공유 | `CurrentSpread` 누적값까지 동기화해야 함 |
| **예측을 포기하고 서버 결과만 표시** | 사격자가 RTT만큼 늦게 봄 |

**결정** — `virtual bool SupportsPredictedImpact()`를 두고 무기가 스스로 답하게 한다.

```
라이플  (true)  : 서버가 예측 키를 실어 Cue를 보낸다 -> 사격자는 건너뛰고 나머지만 재생
머신건  (false) : 키 없이 보낸다 -> 사격자 포함 전원이 서버 탄착 하나만 재생
```

**근거** — 퍼짐 무기는 "즉시 보이지만 틀린 위치"보다 "늦게 보이지만 맞는 위치"가 낫다. 연사 중에는 탄흔이 연속으로 생겨 지연이 잘 드러나지도 않는다.

머즐과 발사음은 여전히 예측하므로 **입력 반응성은 유지된다.** 예측을 포기한 것은 탄착 하나뿐이다.

**부수 효과** — 같은 화면에서 라이플은 즉시, 머신건은 RTT 후 탄흔이 뜬다. `NetEmulation.PktLag 500`을 걸면 이 차이가 그대로 보여, **예측하는 무기와 안 하는 무기의 대비를 눈으로 보여주는 데모**가 된다.

---

## 3. 실제로 밟은 함정

문서화 가치가 가장 높은 부분. 전부 실제로 겪었고, **전부 에러 없이 조용히 실패했다.**

### F1. `PossessedBy`는 서버 전용

→ D2 참조. 클라이언트 HUD 체력바가 절대 움직이지 않았다.

### F2. `OnPossess`도 서버 전용

`ATPSPlayerController::OnPossess()`에서 HUD를 생성하고 있었다. 리슨 서버에서는 호스트가 서버이자 로컬 컨트롤러라 동작했지만, **원격 클라이언트에는 그때도 HUD가 없었다.** 데디케이티드로 전환하자 전원 HUD가 사라져서 발견했다.

클라이언트 측 대응 훅은 **`AcknowledgePossession()`** 이다. F1과 정확히 같은 형태의 버그가 컨트롤러 계층에서 반복된 것이다.

### F3. `AGameMode`와 `AGameState`는 짝이다

`AGameModeBase` → `AGameMode`로 승격했으나, World Settings의 `GameStateClass`가 `GameStateBase`로 저장되어 있었다.

`AGameStateBase`는 `AGameState`의 **부모**이므로 `GetGameState<AGameState>()` 캐스팅이 null을 반환한다. 결과적으로 **`MatchState`가 클라이언트에 영원히 전달되지 않는다.** 경고도 에러도 없다.

C++ 생성자의 기본값보다 **에디터에 저장된 값이 우선**하고, 우선순위는 Project Settings < BP Class Defaults < World Settings 순이다.

### F4. 복제가 스폰보다 먼저 도착할 수 있다

`OnRep_CurrentWeaponType()`이 `Weapons` TMap이 비어 있는 상태에서 호출되면 조용히 무시된다. 그런데 `CurrentWeaponType`은 다시 바뀌지 않으므로 **`OnRep`이 재발생하지 않는다.**

증상은 엉뚱한 곳에서 나타났다 — 무기 교체뿐 아니라 **재장전도 되지 않았다.** `GA_Reload`가 무기 null 검사로 즉시 종료했기 때문이다.

해결: `SpawnWeapons()` 말미에 이미 받아둔 값을 한 번 더 반영. 어느 쪽이 먼저 오든 **마지막에 실행된 쪽이 상태를 확정**하게 만든다.

### F5. 데디케이티드 서버에는 카메라가 없다

`UPlayerAimComponent::ComputeAimPoint()`가 `FollowCamera` 기준으로 조준점을 계산한다. 리슨 서버에서는 호스트에게 실제 카메라가 있어 호스트 조준은 정상 동작했다.

데디케이티드에서는 서버에 로컬 플레이어가 아예 없다. `GA_FireBase`가 `LocalOnly`인 동안에는 표면화되지 않았을 뿐, **M2b-1에서 `LocalPredicted`로 올리는 순간 서버가 원점을 향해 발사하게 된다.** 이것이 D14(TargetData)가 필요한 이유이며, 리슨 서버에서는 이 필요성이 보이지 않는다.

### F6. 연출이 전부 로컬 호출

`ShowMuzzleFlash()`, `PlayFireSound()`, `PlayImpactEffect()`가 `AWeaponBase::Fire()` 안에서 직접 호출된다. 적의 발사 어빌리티는 서버에서만 실행되므로, 데디케이티드 서버에서는 **이펙트가 아무 데서도 보이지 않는다.**

→ D15에서 GameplayCue로 해결.

### F7. Loose 태그는 복제되지 않는다

`AddLooseGameplayTag()`는 호출된 머신에만 적용된다. `State.Dead`, `State.Swapping`이 모두 해당한다.

M1에서 교체 상태는 어빌리티의 `ActivationOwnedTags`로 옮겨 해결했다. `State.Dead`는 M3 과제로 남아 있다.

### F8. `FScopedPredictionWindow`는 서버 전용이다

생성자에 `IsNetSimulating() == false` 검사가 있어 **클라이언트에서는 아무 일도 하지 않고 빠져나온다.**

Cue를 예측 키와 함께 실행하려 했는데 클라에서 `ScopedPredictionKey`가 세팅되지 않았고, 그 결과 (a) 예측 재생이 안 되고 (b) 서버 멀티캐스트가 도착해도 자기 키로 인식해 건너뛰어 **사격자 화면에서 연출이 아예 사라졌다.**

키를 대입만 하는 경량 스코프 구조체(`FTPSScopedCueKey`)를 따로 만들어 해결했다. 단, `ServerSetReplicatedTargetData`는 키 생성·등록이 필요하므로 **원래의 `FScopedPredictionWindow`를 유지해야 한다.** 두 곳의 목적이 다르다.

### F9. 판정을 서버로 옮기면 그에 딸린 클라 피드백이 조용히 끊긴다 ★

M2b에서 **같은 형태의 문제를 다섯 번 반복해서 겪었다.**

| 증상 | 원인 |
|---|---|
| 로켓 폭발이 안 보임 | `AProjectileRocket::HandleImpact` 오버라이드가 자체 `Explode()`에서 로컬 스폰 |
| 히트마커가 안 뜸 | `OnHitConfirmed.Broadcast()`가 서버 전용이 된 `FireInternal` 안에 있음 |
| 방향성 비네트가 안 나옴 | `LastDamageInstigator`가 `PostGameplayEffectExecute`(서버 전용)에서만 설정됨 |
| 적 발사 연출이 안 보임 | F6 |
| **적 발사 몽타주가 안 보임** | **`bHoldingAim`이 BT(서버 전용)에서만 설정되어 클라 AnimBP의 Layered Blend 가중치가 0으로 고정** |

**공통 패턴** — 서버 권위화의 부작용은 권위 로직 자체가 아니라 **그 주변 연출·피드백**에서 나타난다. 그리고 전부 조용히 실패한다.

해결 방식도 각각 달랐다. 무엇을 누구에게 보내느냐가 다르기 때문이다.

| 대상 | 방식 | 근거 |
|---|---|---|
| 발사/탄착 연출 | GameplayCue | 전원이 봐야 함 |
| 로켓 폭발 | 액터 멀티캐스트 | 에셋이 투사체 BP에 있고 액터가 이미 복제됨 |
| 히트마커 | Client RPC | **사격자만** 봐야 함. Cue를 쓰면 남의 명중까지 표시됨 |
| 가해자 정보 | `Replicated` + `COND_OwnerOnly` | 상태값이며 피격자만 필요 |
| 적 조준 자세 | `Replicated` + `COND_None` | 전원이 봐야 하는 상태값 |

**교훈** — 판정 코드를 서버로 옮길 때는 "이 함수가 부수적으로 하던 일이 무엇인가"를 전수 조사해야 한다. 특히 **파생 클래스의 오버라이드**를 놓치기 쉽다 — 로켓 폭발이 그 사례였고, `Projectile.cpp`만 보고 `ProjectileRocket.cpp`의 존재를 확인하지 않은 것이 원인이었다.

### F10. Cue 실행과 Cue 처리는 다른 경로다

`ACommonCharacter::ExecuteFireCue()`(실행)와 `ACommonCharacter::GameplayCue_Weapon_Fire()`(처리)가 같은 클래스에 있어 혼동하기 쉽다.

- 전자는 우리 코드가 직접 호출한다.
- 후자는 **우리 코드에서 부르는 곳이 한 군데도 없다.** GameplayCueManager가 리플렉션으로 호출한다.

그래서 Cue가 실제로 동작하는지는 **핸들러에 로그를 넣어보기 전에는 알 수 없었다.** 실행 쪽 로그만으로는 "호출은 되는데 아무 일도 안 일어나는" 상태를 구분할 수 없다.

### F11. `OnRep`은 CurrentValue 재계산 전에 호출된다

예측하는 클라이언트에서 Instant Cost GE는 BaseValue가 아니라 애그리게이터 모디파이어로 존재한다(GAS가 롤백 가능하도록 Infinite처럼 다룸). 서버 값이 도착하면 네트워크 스택이 BaseValue를 먼저 덮어쓰고, `GAMEPLAYATTRIBUTE_REPNOTIFY`가 예측 모디파이어를 제거하며 CurrentValue를 재계산한다. `OnRep` 본문은 그 사이에 실행되므로 CurrentValue가 낡은 값이다. 어트리뷰트 로그는 BaseValue로 찍어야 의미가 통한다.

### F12. 복제가 정상이어도 애니메이션은 안 보일 수 있다

적 발사 몽타주가 클라이언트에서 보이지 않았다. 서버·복제·수신까지 모두 정상이었고, 실제로 끊긴 곳은 **AnimBP의 Layered Blend Per Bone 가중치**였다.

`BTService_UpdateAim`이 사거리 판정으로 `SetHoldingAim()`을 호출하는데, Behavior Tree는 서버에서만 돈다. `bHoldingAim`에 복제 지정자가 없어 클라이언트에서는 영원히 false였고, 그 값에서 파생된 `AimAlpha`가 0이라 상체 슬롯이 통째로 무시되었다. 몽타주는 정상 재생 중이었지만 최종 포즈에 반영되지 않았다.

F7(Loose 태그 비복제)과 같은 뿌리지만, **발현 지점이 C++이 아니라 애니메이션 그래프**라 코드만 봐서는 찾을 수 없었다.

**추적 순서** — 각 단계를 로그로 참/거짓 확정하며 좁혔다.

| 단계 | 확인 방법 | 결과 |
|---|---|---|
| 서버가 재생하는가 | `PlayFireMontage` 진입 로그 | O |
| 취소되지 않는가 | `OnCancelled` 분리 후 경과 시간 | 취소 없음 |
| 클라가 수신하는가 | 시뮬 프록시에서 `Montage_IsPlaying` | O |
| 포즈에 반영되는가 | `GetSlotMontageLocalWeight` | **X (0.00)** |

**교훈** — 네트워크 계층이 정상임을 먼저 증명하면 탐색 범위가 절반으로 줄어든다. 복제 여부 → 수신 여부 → 적용 여부를 순서대로 확정할 것. 특히 **애니메이션은 "재생 중"과 "보이는 중"이 다르다.**

### F13. 서버의 TargetData 델리게이트 안에서는 예측 키 스코프가 이미 열려 있다

ServerSetReplicatedTargetData_Implementation이 브로드캐스트 전에 클라이언트의 예측 키로 FScopedPredictionWindow를 연다. 그 안에서 실행하는 GameplayCue는 명시적으로 스코프를 열지 않아도 그 키를 실어 보낸다. 사격자에게 보여야 하는 Cue라면 무효 키로 덮어써야 한다. "스코프를 안 열었으니 키가 없다"는 전제가 성립하지 않는다.

**실제 증상** — 머신건 탄착이 사격자 화면에만 뜨지 않았다. 예측하지 않는 무기라 사격자도 서버 Cue를 봐야 하는데, 열려 있던 클라 예측 키 때문에 사격자만 정확히 건너뛰었다. 다른 클라이언트에서는 정상이었으므로 자기 화면만 보고 테스트하면 "Cue가 아예 안 온다"로 오진하기 쉽다. `FTPSScopedCueKey`로 무효 키를 덮어써 해결.

---

## 4. 검증 방법

### 네트워크 로그 접두사

모든 네트워크 관련 로그에 실행 머신 정보를 붙였다.

```
[SV|Authority |Dedicated] BP_PlayerCharacter_C_0
[CL|Autonomous|Client   ] BP_PlayerCharacter_C_0
[CL|Simulated |Client   ] BP_EnemyCharacter_C_3
```

`HasAuthority()` / `GetLocalRole()` / `GetNetMode()` 세 가지를 함께 찍는다. 출력 로그 필터에 `[SV` 또는 `[CL`을 넣으면 즉시 분리된다.

### 파이프라인 전체 확인

M2b-1 완료 시점의 정상 로그. 발사 한 번에 두 머신이 각자의 가지를 탄다.

```
[CL] ActivateAbility
[CL] CommitAbility 후 탄약=9
[CL] FireOnce bLocal=1 bAuth=0
[CL] 조준점 전송 V(...)
[SV] ActivateAbility            ← 서버 독립 재검증 통과
[SV] CommitAbility 후 탄약=9    ← 서버도 Cost 적용
[SV] FireOnce bLocal=0 bAuth=1
[SV] 조준점 수신
[SV] FireAuthoritative hit=...
```

**어디서 끊기는지가 원인을 가른다.** `[SV] ActivateAbility`가 없으면 서버가 활성화에 실패한 것이고, `조준점 수신`이 없으면 TargetData가 도달하지 못한 것이다.

### 예측 동작 확인

```
NetEmulation.PktLag 200
```

| 관찰 대상 | 기대 동작 | 의미 |
|---|---|---|
| 탄약 카운터 | 즉시 감소 | 예측 O |
| 머즐 / 발사음 | 즉시 재생 | 예측 Cue O |
| **라이플** 탄착 | 즉시 재생 | 예측 O (퍼짐이 없어 서버와 일치) |
| **머신건** 탄착 | ~400ms 후 재생 | 예측 포기, 서버 결과만 (D20) |
| 적 체력바 | ~400ms 후 감소 | 서버 권위 |
| **자기 머즐이 두 번 번쩍임** | **없어야 함** | 예측 키 중복 방지 |

**같은 발사 한 번에서 항목별로 반응 시점이 다른 것**이 예측 파이프라인이 동작한다는 증거다.

중복 재생 확인은 지연을 크게(`PktLag 500`) 걸어야 한다. 지연이 작으면 두 재생이 겹쳐 구분되지 않는다.

**라이플과 머신건을 번갈아 쏘면 예측 유무의 대비가 한 화면에 드러난다.** 같은 벽에 쐈을 때 라이플은 즉시, 머신건은 왕복 후 탄흔이 뜬다. 예측이 무엇을 사주고 무엇을 포기하게 하는지 보여주는 가장 직관적인 데모다.

### 마일스톤별 통과 조건

| | 확인 |
|---|---|
| M0 | 클라이언트에서 적 체력바 감소 / 클라 HUD 체력바 반응 / 클라 사망 시 레벨 리로드 안 됨 |
| M1 | 양쪽 화면에서 무기 교체가 보임 / 교체 중 피격 시 태그 정상 해제 / 빠른 연타 시 무시 |
| M2a | 설정값과 정확히 일치하는 발수만큼 발사 (이중 차감·미차감 검출) / 재장전 몽타주가 다른 화면에서 보임 |
| M2b-1 | 사격자/타 클라 **양쪽** 화면에서 적 체력 감소 |
| M2b-2 | `[CL\|Simulated]` 프록시에서 Cue 실행 / 적 발사 연출이 전원에게 보임 / 로켓 비행이 클라에 보임 / 히트마커·방향성 비네트 |
| M2b-3 | 홀드 시 `FireInterval` 간격 연사 / **탄약이 발당 1씩 감소** / 탄약 소진 시 자동 재장전 / 라이플은 탄착 즉시·머신건은 RTT 후 (각 1회) |

---

## 5. 남은 작업

### M3 — 사망/리스폰
`State.Dead`를 복제되는 GE로 전환(F7), GameMode 주도 리스폰, `UDifficultySubsystem`을 GameInstance에서 GameState로 이관(현재 머신별로 독립이라 난이도가 갈릴 수 있음)

### M4 — 랙 보상
적 트랜스폼 히스토리 링버퍼, RTT 기반 되감기, 지연 200ms 환경에서 전/후 비교 영상

세 시스템이 각자의 시간축을 갖고 있다는 것이 난점이다. 클라가 화면에서 본 적의 위치는 서버의 현재 위치가 아니라 복제 지연과 보간을 거친 과거 위치다. 서버는 조준점 하나만 받으므로 "언제 시점의 적을 보고 쐈는지"를 모른다.

**이것이 가능한 이유는 D14에서 서버가 트레이스를 하도록 만들었기 때문이다.** M2b-1 이전이었다면 되감을 대상 자체가 없었다.

### M5 — 운영 검증
패키징된 Server 타겟, 원격 접속, `NetworkProfiler` 대역폭 측정

---

## 6. 회고

**설계 순서와 구현 순서를 분리한 것이 가장 효과적이었다.** M2 설계를 먼저 내려가 본 덕에 M1의 범위가 절반 이하로 줄었다(D6). 하위 결정이 상위 구조를 바꿀 가능성이 있으면 설계를 먼저 훑어보는 편이 낫다.

**개발 환경을 실제 배포 형태에 가깝게 앞당긴 것도 배당이 컸다.** 데디케이티드 서버로 전환하자마자 세 개의 잠복 버그가 드러났다(F2, F5, F6). M5에서 발견했다면 M2~M4의 설계 결정 일부를 되돌려야 했을 것이다.

**네트워크 버그는 대부분 조용히 실패한다.** 이 문서에 기록된 13개 함정 중 컴파일 에러나 런타임 경고를 낸 것은 **하나도 없다.** 전부 "그냥 동작하지 않는" 형태였고, 로그 접두사와 마일스톤별 통과 조건이 없었다면 원인 추적에 훨씬 오래 걸렸을 것이다.

**서버 권위화의 진짜 비용은 판정 코드가 아니라 그 주변에 있었다.** F9가 이를 가장 잘 보여준다. 트레이스와 데미지를 서버로 옮기는 것 자체는 명확한 작업이지만, 그에 딸려 있던 연출·히트마커·비네트가 모두 조용히 끊겼고 각각 다른 방식(Cue / 멀티캐스트 / Client RPC / 복제 프로퍼티)으로 복구해야 했다. **무엇을 누구에게 보내는가**에 따라 답이 갈린다.

**응급 대응은 반드시 회수해야 한다.** M2b-2에서 머신건 연사를 살리려고 조준점 캐시와 서버 루프 주도 발사를 넣었지만, 그것은 증상 완화였을 뿐 예측 단위가 뭉개진 근본 문제는 그대로였다. M2b-3에서 구조를 바꾸자 그 우회 코드와 함께 연사 전용 코드가 통째로 사라졌다(D19). **임시 대응을 커밋 로그에 "M2b-3에서 걷어냄"이라고 명시해둔 것이 회수 시점에 근거가 되었다.**

**기존 아키텍처 정리가 배당을 줬다.** 갓클래스를 컴포넌트로 분해해둔 덕에 카메라 의존 코드가 `UPlayerAimComponent` 한 곳에 격리되어 있었고(F5), 데디케이티드 서버에서 위험한 패턴(`GetFirstPlayerController` 계열) 사용이 0건이었다. 싱글플레이 단계의 설계 품질이 멀티플레이 이관 비용을 직접적으로 낮췄다.
