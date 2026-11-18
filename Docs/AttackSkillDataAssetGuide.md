# 공격 스킬 Data Asset 설정 가이드

## 목적

좌클릭, 우클릭, 점프 공격, 차징 공격을 코드에 고정하지 않고 Data Asset으로 조정할 수 있게 한다.

## Data Asset 클래스

- C++ 클래스: `URogue10mAttackSkillData`
- 소스 위치: `Source/Rogue10m/Data/Rogue10mAttackSkillData.*`
- 에디터 생성 기준 위치: `Content/DataAsset`
- 공격 스킬 세부 권장 위치: `Content/DataAsset/AttackSkill`

무기별 공격 스킬 권장 위치:

- `Content/DataAsset/AttackSkill/Knuckle`
- `Content/DataAsset/AttackSkill/Dagger`
- `Content/DataAsset/AttackSkill/GreatSword`
- `Content/DataAsset/AttackSkill/DualBlades`
- `Content/DataAsset/AttackSkill/Bow`
- `Content/DataAsset/AttackSkill/Staff`

## 생성 방법

1. Unreal Editor Content Browser에서 `Miscellaneous > Data Asset`을 선택한다.
2. Data Asset 클래스 목록에서 `Rogue10mAttackSkillData`를 선택한다.
3. `Content/DataAsset` 또는 `Content/DataAsset/AttackSkill` 아래에 입력별 에셋을 만든다.

권장 에셋 이름:

- `DA_Unarmed_Primary`
- `DA_Unarmed_Special`
- `DA_Unarmed_JumpPrimary`
- `DA_Unarmed_JumpSpecial`
- `DA_Unarmed_ChargedPrimary`
- `DA_Unarmed_ChargedSpecial`
- `DA_Attack_Unarmed_Primary_Combo2`
- `DA_Attack_Unarmed_Primary_Combo3`

현재 기본 주먹 공격 Data Asset은 `Content/DataAsset`에 생성되어 있다.

자동 생성/갱신 스크립트:

- `Scripts/Editor/CreateDefaultUnarmedAttackDataAssets.py`

## 캐릭터에 지정하는 위치

`BP_FirstPersonCharacter`에서 아래 항목에 Data Asset을 지정한다.

- `Primary Attack Skill`: 좌클릭 공격
- `Special Attack Skill`: 우클릭 공격
- `Jump Primary Attack Skill`: 점프 중 좌클릭 공격
- `Jump Special Attack Skill`: 점프 중 우클릭 공격
- `Charged Primary Attack Skill`: 차징 좌클릭 공격
- `Charged Special Attack Skill`: 차징 우클릭 공격

현재 기본 연결:

- `Primary Attack Skill`: `DA_Attack_Unarmed_Primary`
- `Special Attack Skill`: 비워 둠
- `Jump Primary Attack Skill`: `DA_Attack_Unarmed_JumpPrimary`
- `Jump Special Attack Skill`: 비워 둠
- `Charged Primary Attack Skill`: 비워 둠
- `Charged Special Attack Skill`: 비워 둠

현재 기본 캐릭터는 일반 좌클릭과 점프 좌클릭만 바로 사용할 수 있다. 우클릭, 점프 우클릭, 차징 공격은 Data Asset을 지정하기 전까지 잠긴 입력으로 처리한다.

자동 연결/갱신 스크립트:

- `Scripts/Editor/AssignDefaultAttackSkillDataAssets.py`

해당 칸에 Data Asset이 없으면 그 입력은 잠긴 것으로 처리되고, 화면 전투 로그에 잠김 메시지가 표시된다.

## 조정 가능한 값

- `Skill Name`: 화면 로그에 표시될 스킬 이름
- `Skill Description`: 스킬 설명
- `Input Slot`: 해당 Data Asset이 의도한 입력 슬롯
- `Damage`: 공격 데미지
- `Attack Range`: 공격이 닿는 거리
- `Attack Trace Radius`: 공격 판정 두께
- `Attack Cooldown`: 재사용 대기 시간
- `Charge Seconds`: 차징 공격으로 인정되는 최소 누름 시간
- `Resource Costs`: 공격 실행 시 소비할 자원 목록
  - `Health`: 체력
  - `Stamina`: 스테미나
  - `Mana`: 마나
  - `Energy`: 기력 또는 별도 전투 자원
- `Enable Combo`: 해당 공격 이후 콤보 입력을 받을지 여부
- `Combo Window Open Seconds`: 공격 실행 후 콤보 입력을 받기 시작하는 시간
- `Combo Window Close Seconds`: 공격 실행 후 콤보 입력을 마감하는 시간
- `Combo Input Slot`: 다음 콤보를 실행할 입력 슬롯
- `Next Combo Skill`: 콤보 입력 성공 시 실행할 다음 공격 Data Asset
- `Attack Montage`: 공격 애니메이션 몽타주
- `Skill Icon`: 스킬트리와 공격 정보 UI에서 사용할 공격 아이콘 텍스처
- `Icon Label`: 아이콘 텍스처가 없을 때 표시할 짧은 임시 라벨
- `Icon Tint`: 아이콘 배경과 강조색
- `Attack Effect`: Niagara 또는 Particle 효과 에셋
- `Draw Debug Attack`: 이전 데이터 에셋 호환용 필드
- `Debug Color`: 디버그 선과 판정 구체 색상

## 화면 디버그 표시

실제 공격 쿼리와 타격 결과를 확인하려면 전투 테스트 도구의 공간 디버그를 켠다. `Scripts/VerifySpatialDebug.ps1`로 테스트 세션에서 쿼리 표시와 종료 후 정리를 검증할 수 있다.

## 콤보 설정

콤보는 Data Asset에서 다음 스킬을 직접 연결하는 방식으로 관리한다.

기본 흐름:

1. 현재 공격 Data Asset에서 `Enable Combo`를 켠다.
2. `Combo Window Open Seconds`와 `Combo Window Close Seconds`로 추가 입력 가능 시간을 정한다.
3. `Combo Input Slot`에 다음 콤보를 받을 입력을 지정한다.
4. `Next Combo Skill`에 다음 공격 Data Asset을 지정한다.

현재 기본 주먹 좌클릭 콤보는 다음 체인으로 준비되어 있다.

- `DA_Attack_Unarmed_Primary`
- `DA_Attack_Unarmed_Primary_Combo2`
- `DA_Attack_Unarmed_Primary_Combo3`

`DA_Attack_Unarmed_Primary`와 `DA_Attack_Unarmed_Primary_Combo2`는 좌클릭 추가 입력을 받도록 설정되어 있고, `DA_Attack_Unarmed_Primary_Combo3`은 마무리 공격으로 콤보 연결을 끊는다.

## 권 스킬트리 잠금 규칙

현재 프로토타입 스킬트리는 권을 기본 해금 무기로 사용한다.

- 권 숙련도는 기본값 1이다.
- 단검, 대검, 쌍검, 활, 지팡이는 숙련도 0이며 스킬트리에 접근할 수 없다.
- 무기를 얻거나 기본 스킬서 또는 NPC를 통해 스킬을 습득하면 해당 무기 숙련도를 1로 올리고 스킬트리를 열 계획이다.
- 권 스킬트리의 기본 제공 스킬은 `DA_Attack_Unarmed_Primary`, `DA_Attack_Unarmed_JumpPrimary`이다.
- 콤보 2타와 3타는 테스트용으로 앞 스킬이 해금되면 다음 스킬을 클릭해 해금할 수 있다.
- 스킬트리에서 잠긴 공격 Data Asset은 캐릭터에 연결되어 있거나 콤보 체인에 연결되어 있어도 실행되지 않는다.
- 현재 권 트리는 좌클릭 라인과 점프 좌클릭 라인을 나누어 표시한다.
- 좌클릭 라인은 `기본 권격 -> 연속 권격 2 -> 연속 권격 3 -> 권 마무리` 순서로 확장한다.
- 점프 좌클릭 라인은 `도약 권격 -> 공중 연계 -> 낙하 권격` 순서로 확장한다.

각 스킬 노드는 공격 정보 아이콘 영역을 가진다. 실제 아이콘 텍스처가 없으면 Data Asset의 `Icon Label`과 `Icon Tint`를 기준으로 임시 아이콘을 표시한다.

## 공격 쿨타임 UI

HUD는 화면 6시 방향 퀵 슬롯 위에 현재 공격 정보를 표시한다.

- 표시 대상은 현재 기본 공격 또는 콤보 대기 중인 다음 콤보 공격이다.
- 콤보 입력 가능 시간이 열리면 아이콘과 이름이 즉시 다음 콤보 스킬로 바뀐다.
- 쿨타임 오버레이와 남은 시간은 캐릭터의 공유 공격 쿨타임 값을 기준으로 표시한다.
- 콤보 공격은 개별 쿨타임을 따로 쓰지 않고 콤보 루트 스킬의 쿨타임을 공유한다.
- 콤보 중에는 다음 입력 가능 시간이 닫히기 전까지 쿨타임이 바로 흐르지 않는다.
- 마지막 공격이 콤보를 더 이어갈 수 없거나, 다음 입력 가능 시간이 끝나면 첫 공격 상태로 돌아가며 공유 쿨타임이 시작된다.

## 자원 소모 설정

공격 Data Asset의 `Resource Costs` 배열에서 공격에 필요한 자원을 설정한다.

기본 신규 Data Asset은 `Health`, `Stamina` 항목을 0 비용으로 가진다. 비용을 0보다 크게 설정하면 공격 실행 전 현재 캐릭터 자원을 확인하고, 부족하면 공격을 실행하지 않는다.

현재 연결 상태:

- `Health`: 체력 비용. 현재 체력이 비용 이하이면 사용하지 않는다.
- `Stamina`: 스테미나 비용. 현재 스테미나가 부족하면 사용하지 않는다.
- `Mana`: 마나 비용. 마나 표시 여부와 별개로 데이터는 사용할 수 있다.
- `Energy`: 향후 기력 컴포넌트 연결을 위한 예약 값이다. 현재 0보다 큰 비용을 넣으면 미연결 로그가 표시된다.

## 확장 방향

- 실제 피해 타이밍은 이후 Animation Montage Notify로 옮기는 것이 좋다.
- 무기 타입별로 Data Asset을 나눠 장비 무기와 연결한다.
- `Attack Effect`는 추후 Niagara 모듈 연결 후 공격 위치 또는 명중 위치에 스폰한다.

## 공격 형태와 피해 방식 설정

`Attack Shape`과 `Hit Mode`는 독립적으로 선택한다.

- 직선 박스: `Attack Range`, `Box Half Width`, `Box Half Height`
- 투사체 경로: `Attack Range`, `Attack Trace Radius`, `Hit Count`
- 부채꼴: `Attack Range`, `Arc Angle Degrees`
- 원형: `Attack Range`, `Circle Forward Offset`

피해 방식별 `Hit Count` 의미:

- 단타: 항상 1회이며 `Hit Count`를 사용하지 않는다.
- 연속 공격: 공간을 다시 검사할 횟수
- 적중 후 다단히트: 최초 적중 대상에게 피해를 적용할 횟수

`Hit Interval`은 연속 공격과 다단히트의 피해 간격이다. 플레이어 공격에서는 공격속도 배율에 따라 짧아진다.

몬스터는 `BP_BaseMonster`의 `Attack Skill Data`에 같은 Data Asset을 지정한다. 비워 두면 기존 몬스터 단타 설정을 사용한다.

## 최소·최대 피해 범위

모든 공격자는 AttributeSet에서 다음 비율을 가진다.

- `MinDamageRatio`: 기본값 `0.9`
- `MaxDamageRatio`: 기본값 `1.1`

스킬은 캐릭터 비율을 기본으로 사용하며 다음 값으로 추가 보정한다.

- `MinDamageRatioMultiplier`: 기본값 `1.0`
- `MaxDamageRatioMultiplier`: 기본값 `1.0`

최종 계산식:

```text
최종 최소 비율 = 캐릭터 최소 비율 × 스킬 최소 보정
최종 최대 비율 = 캐릭터 최대 비율 × 스킬 최대 보정
최종 피해 = 스킬 기본 피해 × Random(최종 최소 비율, 최종 최대 비율)
```

최소·최대 결과가 역전되면 자동으로 정렬하며 음수 비율은 0으로 제한한다. 연속 공격과 다단히트는 각 타수마다 피해량을 독립적으로 계산한다.

예시: 기본 피해가 100이고 캐릭터 범위가 90~110%, 스킬 보정이 80~120%이면 최종 피해 범위는 72~132이다.

## 치명타

캐릭터 Attribute 기본값:

- `CriticalChance`: `0.0` — 기본 치명타 확률 0%
- `CriticalDamageMultiplier`: `1.5` — 치명타 피해 150%

스킬 Data Asset 보정:

- `CriticalChanceBonus`: 캐릭터 치명타 확률에 더하는 값. `0.1`은 10%p 증가
- `CriticalDamageMultiplierBonus`: 캐릭터 치명타 피해 배율에 더하는 값. `0.2`는 150%를 170%로 변경

피해 범위 랜덤 계산 후 치명타를 판정하며, 치명타가 발생하면 계산된 피해에 최종 치명타 배율을 곱한다. 연속 공격과 다단히트는 각 타수마다 독립적으로 치명타를 판정한다.

### 장비·패시브·액티브 연결

- 장비: 장착 중 Infinite Gameplay Effect로 치명타 Attribute 변경
- 패시브: 패시브 습득 시 Infinite Gameplay Effect 적용, 스킬 제거 시 Effect 해제
- 액티브 버프·디버프: Duration Gameplay Effect로 일시 증가 또는 감소
- 개별 공격 스킬: Data Asset의 치명타 보너스 필드 사용

치명타 확률은 최종적으로 0~100%로 제한한다. 치명타 피해 배율은 음수가 되지 않도록 제한한다.
