# 무기 교체·쿨다운·문맥 메뉴 정지 영향 분석과 테스트 계획

## 범위와 기준

이 packet은 읽기 전용 C++ 분석과 요구사항/테스트 계획이다. 실제 교체·일시정지·자원 전환·세이브 이행은 구현하지 않는다. 분석 기준 HEAD는 `7806e6f1cf327641b3278140412f4b5151dc1de1`(런타임 조건 코드 f45ed772), UE5.8.3이다. 독립 복제의 `Sprint#9-4-weapon-switch-analysis`에서 문서만 작성한다. R11 및 R02/R03 자동화11건은 이전 packet의 실행 결과이며 이번 분석의 실행 결과로 재계상하지 않는다.

## 최신 계약

- 전투 여부가 아니라 행동 상태로 교체 가능 여부를 판정한다. 대기/이동/달리기는 허용, 공격/사망은 차단한다. 회피·피격 반응·시전·공중 등은 달리기 수준을 넘는 행동이므로 보수적 차단 추천으로 분리한다. 없는 피격/시전 상태나 지속시간을 임의로 만들지 않는다.
- 짧은 교체 동작, 무적 없음. 교체 모션의 정확한 시간은 미정이며 아래 0.5초 쿨다운과 같은 값이라고 가정하지 않는다.
- 성공한 교체 후 기존 스킬 쿨다운은 계속 흐른다. 적용 대상 각 deadline에 `newEnd = max(oldEnd, now + 0.5s)`를 적용한다. `oldRemaining + 0.5s`나 일괄0.5초 초기화가 아니다.
- 사전 무기별 세팅은 비전투 중 편집하고 전투 중 무기 교체는 해당 세팅을 전환한다. 교체 허용과 세팅 편집 허용은 다른 명령이다.
- 필드에서 상점/인벤토리/스킬 세팅 메뉴를 열면 게임과 런 타이머가 계속된다. 안전지대에서 해당 메뉴를 열면 둘 다 정지한다. 안전지대 입장만으로 자동 정지하지 않는다.
- 마나↔내공의 자유 전환/잔량 정책은 미정이다. 일정 레벨 이상의 고난도 미니게임으로 혼합된 상위 개념을 얻는 별도 이벤트가 있으며 런당1번, 다음 런 초기화다. 실패 생존 시 런 회복불가 마나/내공 손상 때문에 재도전 요구 조건이 미달하며 횟수 플래그만으로 막지 않는다. 평가 API는 사유를 표시하고 시작하지 않는다. 다음 런 손상 초기화다. 레벨·이름·효과·조작은 미정이다. 일반 내부 에너지 수련 미니게임은 후보 제안이다. 이번 packet에는 연결하지 않는다.

## 확인한 현재 코드와 차이

| 접점 | 확인한 현재 구현 | 필요한 변경/주의 |
|---|---|---|
| `Character/Rogue10mCharacter.cpp:629` `SetEquippedWeaponType` | PlayerState setter 호출 후 Combat 변경 알림. 사용자 행동 gate가 없다. | 플레이어용 `TryRequestWeaponSwitch`(제안 API)와 초기화/복원용 commit 경로를 분리한다. 로드/스폰에 무조건 행동 gate를 걸지 않는다. |
| `Core/Rogue10mPlayerState.cpp:34` | 무기 enum 대입과 숙련 변경 알림 | 이 low-level setter 하나에 모든 정책을 넣으면 복원·기존 테스트를 막는다. 사용자 요청 경계가 먼저다. |
| `InventoryComponent.cpp:313`, `:1130` 및 unequip 경로 | 슬롯 또는 grid 내용을 먼저 바꾼 뒤 Character setter 호출 | 거부할 요청은 인벤 변경 전에 검사해야 한다. 슬롯/grid/장비해제 등 모든 플레이어 진입점에서 원자성·GUID·강화 보존 검증. persistence 전용 복원 경로와 구분한다. |
| `CombatComponent.cpp:637`, `:797` | 변경 때 공격 visual/콤보/대기 능력/활성 hit timer 취소, cooldown source 참조 reset | ‘공격 중 요청 거부’가 아니라 공격을 취소하는 현재 동작이다. 먼저 요청을 거부해야 하며 원래 공격·자원·타이머가 그대로여야 한다. cleanup 함수는 복원/종료에도 쓰므로 전역 의미를 바꾸지 않는다. |
| `CombatComponent.cpp:680~729` `ApplyActiveWeaponProfile` | 현재·기억 binding 맵 reset 후 프로필 기본 세팅 재구축 | 무기별 사용자가 편집한 런 세팅 보관이 필요하다. 현재 기본 프로필 전환을 사전 세팅 복원 완료라고 볼 수 없다. 새 저장 버전은 이 packet에서 도입하지 않는다. |
| `CombatComponent.cpp:896~910`, `:1715~1741` | 공유 공격 start/end timestamp 하나와 source 참조. combo에서는 시작 시각이 뒤로 밀릴 수 있다. | 교체가 숫자 deadline을 지우지는 않지만0.5 floor도 없다. 시작 전 getter가0을 반환할 수 있으므로 getter 값만 저장해 복원하면 남은 제약을 잃는다. 절대 deadline과 clock domain을 보존해야 한다. |
| `CombatComponent.h:271~291` | 일반 공격, bound charge, combo, 활성 execution 상태가 분산 | `IsAttackChargeHeld` 하나나 공유 cooldown 하나로 공격/회복 상태를 대표할 수 없다. 공격 수락부터 동작 종료까지 명시적 busy 경계를 제공해야 한다. 장수명 projectile 수명 전체를 공격 동작으로 간주하지 않는다. |
| `BasicBrawlerComponent.h:17~34` | Unarmed용 Idle/Charging/Attacking snapshot | 첫 제품 무기인 Knuckle/GreatSword 전체의 공통 행동 상태는 아니다. 그대로 범용 교체 gate로 쓰지 않는다. |
| `Ability/Rogue10mGameplayAbility_Attack.cpp:12~27` | 공격 요청 실행 뒤 바로 EndAbility | GAS ability active 여부만으로 실제 montage/지연 hit/후딜을 판정할 수 없다. |
| `Character.cpp:333~388`, `:164~210` | 회피 flag는 private, 피격은 체력·feedback 경로 | 회피 공개 read-only query 접점은 필요하다. 선택한 플레이어 C++ 경로에서 범용 hit-react/casting busy 상태는 확인하지 못했다. 별도 권위 있는 상태/태그 계약이 필요하다. Blueprint 전체를 검사했다는 뜻은 아니다. |
| `Character.cpp:214~242`, `PlayerController.cpp:165~174` | 이동·공격·달리기·E회피·1~5소모품·메뉴·검사 입력 | 전용 교체 hotkey/action을 확인하지 못했다. 새 물리 키를 임의로 확정하지 않고 Enhanced Input action과 reserved-key 충돌 검토 후 배치한다. |
| `SkillBindingComponent.cpp:191~224`, `:244~295` | skill asset path별 키 저장, active 트리 레이아웃 변화 때 재바인딩/hold 취소 | 무기별 스킬 슬롯 세팅과 키 설정은 별개다. 비전투 편집 gate를 assign/unassign와 키 편집 진입점에 정합하게 적용한다. 기존 키 저장을 런 기술 사용권으로 오해하지 않는다. |
| `SkillTreeCombatComponent.cpp:27~44` | identity/track/property 변경 시 gauge/순환효과 reset, 공유 proc cooldown 보존 | 기존 호환 동작이다. 새 마나/내공 전환 잔량 정책으로 확정하거나 혼합 이벤트로 확장하지 않는다. |
| `PlayerController.cpp:1087~1145` | 일반 패널 열기 때 이동 정지·visibility/input mode 처리. 위치 기반 pause 호출 없음 | 메뉴에 의한 플레이어 입력 차단과 월드 일시정지를 분리한다. 필드 메뉴는 몬스터·시간이 진행되어야 한다. |
| `PlayerController.cpp:110`, `:426`, `:458`, `:499`, `:1012` | pause는 held input 취소, 메인 메뉴/로비/이동 등 별도 경로에서 true/false 설정 | 단일 ‘닫으면 무조건 unpause’는 다른 pause 소유권을 깨뜨린다. 메뉴 pause reason 소유권과 중첩 패널 집계를 설계해야 한다. |
| `Core/Rogue10mGameState.cpp:22~36`, `:69~85` | 월드 TimerManager의1초 tick으로 런 잔여시간 감소 | 안전 메뉴에서 실제 월드와 런 timer 둘 다 멈추는지 검증. 타이머만 수동 보정하거나 필드에서 전체 pause시키는 구현을 피한다. |

추가 cooldown 도메인: Character의 회피 `NextDodgeAllowedTime`, Inventory의 quick-slot별 end time(:556~569), SkillTreeCombat의 proc end time이 별개다. ‘모든 쿨다운’ 적용 목록과 HUD 표시는 다음 구현 diff에서 명시해야 한다. 단일 공격 timestamp만 고치고 모든 스킬 쿨다운 유지가 끝났다고 주장할 수 없다. 임의의 제외 목록이나 새 수치 정책을 추가하지 않는다.

선택한 runtime C++ 검색에서는 generic SafeZone/SafeArea/Sanctuary provider를 확인하지 못했다. 마을 서비스는 기존 StartHub/WaitingToStart 경계지만, 이것만으로 필드 안전지대를 대표할 수는 없다. 안전 맵/영역을 판정하는 명시적 read-only provider(제안)를 먼저 정하고 `GameplayMenuOpen && SafeContext`일 때만 해당 메뉴 소유 pause를 건다. 생산 맵에 volume을 임의 배치하지 않는다.

## 제안 구현 묶음 — 이번에는 미구현

1. **W1 행동 gate와 요청 transaction**: Character h/cpp, Combat h/cpp 및 필요시 작은 transient switch component(제안 경로). 이동/대기/달리기 허용, 공격/사망 및 보수적 busy 상태 거부. 소유 무기/장비 유효성 재검증 후 commit. 인벤은 commit 전에 바꾸지 않는다. 결과 API는 거부 사유를 제공하고 UI/hotkey가 공유한다.
2. **W2 무기별 런 세팅과 입력 전환**: Combat h/cpp, SkillBinding h/cpp, Inventory 사용자 장착/해제 경로. 현재 무기 세팅을 런 메모리에 보관하고 목표 무기의 이미 학습한 스킬만 검증·적용한다. Discovery를 Learned로 환산하지 않는다. 신규 hotkey/모션 에셋은 실제 선택 후 별도 검증한다. 저장 snapshot은 아직 확장하지 않는다.
3. **W3 cooldown deadline 보존과 floor**: 공유 공격·기술별/무기별 상태 표현을 먼저 확정. 원래 deadline은 계속 감소하고 성공 commit 때만 `max(oldEnd, now+0.5)` 적용. 교체 준비/거부/중복 요청이 기존 쿨다운·자원·무적 값을 바꾸지 않게 한다. 지연 시작 cooldown과 HUD source도 함께 검증한다.
4. **M1 문맥 메뉴 pause**: PlayerController h/cpp, 안전 문맥 provider(제안), 필요시 UI close 입력. 안전지대 입장만으로 pause하지 않는다. 중첩 인벤+장비창, 설정/상점/스킬 세팅, result/lobby pause 소유권, 여행/죽음/종료 cleanup을 처리한다. pause 중에도 닫기 입력이 동작해야 하며 공격은 발동하지 않아야 한다.

위 목록은 조건 기반보다 넓은 런타임 변경이다. 이번 제출은 영향·테스트 계획 보고까지이며 실제 수정 전 별도 packet으로 진행한다. 새 세이브/경제/마나·내공 전환은 범위 밖이다.

## 테스트 계획 — 미실행

| ID | 시나리오 | 수용 조건 |
|---|---|---|
| W01 | 대기·걷기·달리기, 적 근처 전투 플래그 on/off | 행동이 허용 상태면 교체 가능. 단순 in-combat 이유로 막지 않음. |
| W02 | 공격 windup/active/recovery/차징/콤보, 동일 frame 요청 | 거부. 원래 몽타주·hit timer·자원·execution/use count·장비 불변. |
| W03 | 사망·회피·피격 반응·시전·공중 | 사망/공격은 확정 차단. 나머지는 채택한 보수적 busy 계약대로 차단; 없는 상태를 montage 아무 것이나로 추정하지 않음. |
| W04 | 이미 발사된 장수명 projectile 뒤 정상 이동 | projectile 수명만으로 계속 교체를 잠그지 않음. 기존 캡처 효과/소유 무기 변경으로 피해가 재증폭되지 않음. |
| W05 | 슬롯/grid/해제/hotkey/API 각각 거부·성공 | 모든 사용자 경로 같은 gate. 실패는 GUID·강화·인벤 위치·무기·세팅 불변. 복원/초기화 경로는 별도 계약으로 정상 작동. |
| W06 | 너클 A세팅→대검 B세팅→너클 | 정확한 사전 세팅/키 복귀, 중복 바인딩·유령 held input 없음. 전투 중 편집은 거부해도 준비된 세팅 전환은 가능. |
| W07 | 교체 중 피격/치명타/종료 | CanBeDamaged·충돌·방어 규칙에 교체 무적 없음. 취소/종료 이후 늦은 commit이 장비를 덮지 않음. |
| W08 | 연타/동일 무기/목표 장비가 사라짐 | 중복 transaction·자원 소비·임의 floor 누적·분실 없음. 소유권을 commit에서 다시 확인. |
| C01 | 성공 시 기존 잔여0/0.2/0.5/1.2초 | 새 잔여0.5/0.5/0.5/1.2초.1.7초로 더하지 않음. |
| C02 | 교체 왕복·반복, 긴 기존 쿨다운 | 기존 deadline이 계속 흐르며 초기화·재시작·매번0.5 더하기 없음. 적용 대상 모든 skill에 동일식. |
| C03 | combo의 미래 시작 시각, getter는 현재0인 경우 | 보관된 end timestamp를 잃지 않으며 floor가 남은 제약을 단축하지 않음. |
| C04 | 무기별 다른 기술/공유 cooldown/HUD, proc·회피·소모품 도메인 | 명시한 모든 대상 deadline 유지·표시. 적용 제외를 숨기거나 단일 공유타이머만으로 전체 통과 주장하지 않음. |
| M01 | 필드에서 메뉴 열고 실제 world/AI/런 timer 관측 | 플레이어 입력만 정책대로 차단, 게임·런 timer 계속. |
| M02 | 안전지대에서 메뉴 열기/닫기 | 열려 있는 동안 게임·런 timer 둘 다 정지, 닫으면 소유한 pause만 해제. |
| M03 | 안전지대 진입/이탈, 메뉴 닫힘 | 입장 자체는 자동 정지하지 않음. 미확인 문맥을 안전으로 가정하지 않음. |
| M04 | 인벤+장비 두 창, 메뉴간 전환 | 마지막 대상 메뉴가 닫힐 때만 메뉴 pause 해제. 한 창 닫기로 다른 창 pause가 풀리지 않음. |
| M05 | pause 중 닫기 키/마우스, held 공격키 | UI 닫기 가능, resume 후 유령 공격/중복 발동 없음. |
| M06 | 결과/메인로비/여행/죽음 중 메뉴 close | 기존 결과·저장 실패 gate/다른 pause 소유권 훼손 없음. |
| R01 | R11 4 + 조건4 + 기존숙련3 | 기존11건 재실행, 새 기능 실행 결과와 별도로 보고. |
| R02 | 실제 격리 QA 프로필/입력/렌더링 | 너클/대검 교체 모션, HUD, 피격 가능, 메뉴시간 관측. QA 슬롯만 사용. 미실행이면 명시. |
| P01 | 원본/세이브/생산맵 보존 | 원본 clean·98개 해시 동일, Content/SaveVersion 변경 없음 또는 별도 승인 범위와 정확히 일치. |

숫자0.5 검사는 순수 deadline 단위 테스트로 먼저 수행하고, 실제 world time·메뉴 pause·입력/몽타주 검증을 별도 통합 테스트로 수행한다. 이번 문서 작성은 위 테스트 실행이나 무기 교체 기능 구현 완료가 아니다.


## 최신 실패 손실 범위 확정

일반 돌파/융합 실패 생존 손실은 이번 런까지만이며 다음 런 초기화한다. 로비 강화·재화·Discovery를 손상시키지 않는다. 융합 실패 사망은 가능성이지 확정 결과가 아니다. 최초 융합 보상은 저장 프로필의 해당 융합 최초1회 큰 로비 재화로 확정됐고 금액만 미정이다. 반복 성공/콜백/저장 재시도 중복 지급을 금지한다. 실패 생존 시 재도전은 이번 판 회복불가 손상으로 조건 미달이며 다음 판 초기화한다. 이번 분석에 미니게임·페널티·보상 구현은 포함하지 않는다.


### 숙련 자동 승급 확정
사용자 답변으로 숙련100 도달 시 자동 승급이 확정됐다. 특정 행동이나 수동 버튼 gate는 없다. 초과 숙련 처리 등 수치 세부와 행동 달성 후 목록 공개는 미정이다. 이번 읽기 전용 분석에서 숙련 공식/승급 코드는 수정하지 않았다.


### 획득처·미래 확장 추가 기록
대검은 고정 장소·상점·퀘스트에서 획득 가능하다. 첫 버전은 너클/대검이며 미래 무기 확장 가능 구조를 지향한다. 상점 기본 등급, 퀘스트 더 희귀한 등급, 드롭/특정 히든 피스는 매우 좋은 등급 기회다. 모든 드롭 최고 등급이나 확률·표를 확정하지 않는다. 이번에는 규칙 기록/영향 분석만 수행했다.


미래 무기 확장 영향: 현재 WeaponSkillProfiles 배열과 무기별 세팅 분리는 재사용 접점이다. 그러나 SkillTreeData의 숙련 enum은 Brawler/GreatSword, ProgressionRequirementTypes의 ID 검증은 두 트랙/Mage, ProgressionComponent::Validate는 Masteries 개수2로 제한한다. 따라서 현재 기반을 ‘새 무기가 자동 지원되는 구조’로 주장하지 않는다. 확장 시 ID/트랙 매핑·프로필·조건 검증·snapshot 제한/호환성 테스트를 별도 변경해야 하며 기존 enum 순서나 저장 형식을 이번 분석에서 변경하지 않았다.


## 장비별 방어 규칙 추가

장착 장비에 따라 기본 회피/가드 스킬을 제공하고 방패를 장착하면 패링을 연다. 방패 슬롯과 대검/너클 동시장착 허용은 질문 중이다. 임의 보조 슬롯, 모든 무기에 대한 패링, 가드/패링 중 교체 허용 정책을 추가하지 않는다. 이번 packet은 요구사항 기록만이며 새 방어 스킬/슬롯/전투 동작은 구현하지 않았다.


## Ctrl/Q/E/R4슬롯 입력·HUD 추가 영향 분석

기준 런타임 코드는 그대로다. 사용자 확정4슬롯은 ‘학습한 모든 기술에 임의 단축키’와 다르다. 무기별 현재 런 Learned 중 직접 장착한4개만 해당 슬롯으로 실행하며, 기본 좌/우클릭은 별도다. Discovery는 사용권이 아니다.

| 근거 | 현재 구현 | 필요한 후속 변경 |
|---|---|---|
| `Character.cpp:236` | E가 DoDodge에 직접 바인딩 | 새 E 액티브와 동시 발동/입력 소비 충돌. 방어 키 결정 전 임의 대체 키나 신규 슬롯을 만들지 않는다. |
| `SkillBindingComponent.cpp:121~159` | IsUsableKey가 modifier 키 거부. E는 Legacy 예약 및 실제 Pawn key binding 검사에 걸림 | Ctrl 독립 슬롯에 맞춘 제한적 입력 정책 필요. E를 예약 목록에서 지우는 것만으로 기존 회피 바인딩은 없어지지 않는다. Q/R도 C++ 목록에 없다는 이유로 모든 IMC에서 비어 있다고 판단하지 않는다. |
| `SkillBindingComponent.cpp:299~310` | 어떤 Ctrl/Shift/Alt 등이 눌려 있어도 SkillPressed 거부 | Ctrl 자신이 눌린 상태의 독립 입력을 blanket modifier 검사로 거부하지 않도록 재설계. Ctrl+다른 키를 임의 조합 스킬로 정의하지 않는다. 동시 입력·busy 거부는 실제 공격 gate와 별도 검증. |
| `HybridSkillTreeWidget.cpp:690` | KeySelector의 SetAllowModifierKeys(false) | 실행 코드만 허용해도 UI에서 Ctrl 선택 불가. 고정4슬롯 장착 UI와 기존 자유 키 선택 UI의 역할을 명확히 분리한다. |
| `SkillBindingComponent.h:20~25`, cpp:191~224 | skill asset path→임의 FKey 저장, 고정4개 제한 아님 | 무기별 런4슬롯과 입력 선호 저장은 별도. 기존 저장파일을 자동 삭제/변환하지 않는다. 과거 임의 키가4슬롯 밖 추가 실행권을 주지 않도록 adapter/비활성 경계 계획. |
| `AttackSkillData.h:18~27` | Primary/Special/Jump/Charged 의미의 기존6 enum | 이 enum을 Ctrl/Q/E/R 순서로 재사용/재정렬하면 데이터·콤보·애니메이션 계약 손상. 별도 논리 액티브 슬롯 ID4개(제안)와 기존 공격 의미를 분리한다. |
| `RunHUD.cpp:205~262`, `RunHUD.h:60~75` | 기존4칸은 좌/우클릭·좌/우차징, InputSlot 필드와 공유 cooldown 사용 | 신규 Ctrl/Q/E/R4칸과 좌/우 기본공격을 별도 view로 제공. 기존 ‘4칸’을 이름만 바꾸거나 마우스 공격을4개 중 둘로 세지 않는다. 새 cooldown/floor 표시와 actual equipped preset이 같은 source를 사용. |
| `HudWidgetParts.cpp:551~571` | drop이 기존 AssignSkillToInputSlot과 InputSlot에 연결 | 드래그/장착/해제는 새로운4슬롯 대상임을 검증. 기존 좌클릭 기본 공격 경로를 실수로 덮지 않도록 SlotKind/별도 view 타입을 검토. Blueprint 반사 필드 호환도 점검. |
| `SkillBindingComponent.cpp:244~295` | Enhanced Input 동적 action 생성/변경 시 이전 generation 제거, pressed-until-release 무시, paused=false | 재사용 가능한 접점. 슬롯/무기 전환 시 held 취소·action 중복 제거·진행 중 클릭/charge의 유령 발동을 검증한다. 실제 IMC 에셋/키 충돌은 live inspection에서 확인해야 하며 이번에는 에셋을 읽거나 수정하지 않았다. |

후속 범위는 W2의 Combat/SkillBinding 런 프리셋 모델과 Character E 입력, RunHUD h/cpp, HudWidgetParts, HybridSkillTreeWidget/기존 MenuWindowWidgets의 편집 UI까지 확대된다. 별도 타입 이름/물리 Ctrl 좌우 매핑/방어 키/슬롯 교체 UX는 아직 구현하지 않는다. 제안한 새 enum은 기존 enum 변경을 뜻하지 않는다.

### 추가 입력 테스트 계획 —7건, 미실행

- I01 Ctrl 단독 Started/Completed/Canceled가 정확히 해당 슬롯으로 전달. Ctrl 조합을 새 스킬로 만들지 않고 좌/우 alias 정책이 정해지면 중복 실행 검사.
- I02 E 액티브와 회피가 동시에 발동하지 않음. 방어 키 미확정 상태에서는 구현 완료 주장 금지. Q/R의 실제 활성 IMC 충돌도 검사.
- I03 여러 기술 학습 후 장착4개만 실행, 미장착·Discovery-only·과거 임의 키로5번째 활성 실행 불가.
- I04 좌클릭 기본/우클릭 특수는4슬롯 밖에서 동작. 기존 차징/점프 의미·애니메이션 계약 보존.
- I05 너클4개↔대검4개 정확한 프리셋 복원. 누른 채 교체/메뉴/키 해제 시 유령 입력·action generation 중복·잘못된 쿨다운 적용 없음.
- I06 비전투 편집 허용과 전투 중 준비 세팅 전환을 구분. 필드 메뉴 진행/안전 메뉴 pause·닫기 입력과 충돌 없음.
- I07 HUD4개 키·아이콘·부족 조건·잠금/빈 슬롯·드래그/해제·cooldown floor 표시가 실제 슬롯과 일치. 실제 렌더링/해상도 검증은 별도.

### 보스 패턴별 대체 대응 검증 기준 —3건, 미실행

패턴별 기록 양식: 패턴 ID/전조/기본 대응 경로(이동·기본 공격·현재 장비 기본 방어)/필요 공간·충돌·도달 타이밍/특정 기술의 편의/무기·학습 세팅/결과·증거. 수치 창은 실측 후 기록한다. 현재 방패가 없는 초기 너클 캐릭터에게 패링을 보편 기본 대응으로 강제하지 않는다.

- B01 특정 기술 미보유와 초기 너클 기준으로 각 패턴에 가능한 기본 대응 경로가 존재하는지 실제 입력/장소에서 검증. 기술 부재로 무조건 실패·소프트락이 되는 패턴은 거부한다.
- B02 해당 기술 보유 시 대응이 쉬워지는 선택 이점을 검증하되, 기술 미보유 대체 경로를 삭제하지 않는다. 무조건 무피격 가능이나 임의 성공률을 요구로 만들어 내지 않는다.
- B03 패턴 연속/위치 경계/필드 충돌을 포함한 보스 전체 흐름에서 대체 경로가 연결되는지 검사. 필수 퀘스트·필드 보스 키·특정 기술을 숨은 입장/대응 gate로 만들지 않는다.

앞선21건에 입력7건/보스3건을 더해 총31개 계획 시나리오다. 모두 미실행이며 이번 변경은 문서뿐이다.


## 최종 범위 정리

현재 너클/대검에는 방패 보조 슬롯/동시장착/패링을 추가하지 않는다. 향후 한손검·둔기 및 방패 범위에서 별도 검토한다. 현재 기본 회피/가드 방향과 특정 기술 없는 보스 대체 대응 기준은 유지한다.

융합 실패/최초 보상 정책은 규칙 문서만 갱신했다. 프로필별 해당 융합 최초1회 로비 보상의 안정적인 영수증·저장 재시도 멱등성 검증은 향후 보상 packet이며 이번31개 교체/입력/메뉴/보스 계획의 실행 결과가 아니다. 실제 세이브 이행은 없다.

이번 변경은 문서/검사 증거뿐이다. 다음 실제 구현 후보는 W1 행동 gate와 원자적 요청 경계부터이며, 이후 W2 무기별 액티브4슬롯/입력/HUD, W3 cooldown floor, M1 안전 문맥 메뉴 pause를 각각 독립 검증한다. 방어 키·일부 상태의 명시적 busy 계약·실제 safe provider/에셋 매핑을 구현 전에 확인한다. 마나/내공 이벤트/보상/저장과 방패/패링은 별도 후속이다.
