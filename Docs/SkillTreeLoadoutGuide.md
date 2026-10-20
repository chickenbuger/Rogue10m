# 스킬트리 Loadout과 무기별 회피 설정 가이드

## 결론: 스킬 Data Asset은 어디서 참조하는가?

개별 스킬은 기존처럼 Content/DataAsset/AttackSkill 아래의 URogue10mAttackSkillData 에셋으로 만든다.

개별 스킬을 Character Blueprint에 직접 하나씩 지정하지 않고, 다음 중간 프로필에서 참조한다.

- Data Asset 타입: URogue10mWeaponSkillProfileDataAsset
- 권장 위치: Content/DataAsset/SkillProfile
- Character 연결 위치:
  - BP_FirstPersonCharacter
  - Combat Component
  - Skill Loadout
  - Weapon Skill Profiles 배열

구조는 다음과 같다.

~~~mermaid
flowchart LR
    Attack["AttackSkill Data Asset"] --> Profile["WeaponSkillProfile Data Asset"]
    Dodge["DodgeSkill Data Asset"] --> Profile
    Profile --> Combat["Character Combat Component"]
    Tree["SkillTree Widget"] -->|"Drag"| Slot["Input Skill Slot"]
    Slot --> Combat
    Combat --> Execute["Mouse Input / Charge / Jump Attack"]
~~~

## 1. 기본 회피 Data Asset

Content Browser에서 Data Asset을 만들고 URogue10mDodgeSkillDataAsset을 선택한다.

권장 이름:

- DA_Dodge_Unarmed
- DA_Dodge_Dagger
- DA_Dodge_GreatSword

주요 값:

- Distance: 이동 거리. 기본값 100cm, 즉 1m
- Duration: 이동 시간
- Cooldown: 재사용 대기시간
- StaminaCost: 회피 스테미나 비용
- Icon: HUD에 표시할 이미지

E 입력은 Character에서 현재 무기의 Weapon Skill Profile을 찾은 뒤 Default Dodge Skill을 자동 사용한다.
공중 회피 차단은 기존과 동일하게 유지된다.

## 2. 무기 스킬 프로필 Data Asset

Content Browser에서 URogue10mWeaponSkillProfileDataAsset 기반 Data Asset을 만든다.

권장 이름:

- DA_SkillProfile_Unarmed
- DA_SkillProfile_Dagger
- DA_SkillProfile_GreatSword
- DA_SkillProfile_DualBlades
- DA_SkillProfile_Bow
- DA_SkillProfile_Staff
- DA_SkillProfile_Knuckle

설정 항목:

- WeaponType: 이 프로필을 사용할 무기
- DefaultSkillBindings:
  - Primary: 좌클릭 기본 스킬
  - Special: 우클릭 기본 스킬
  - ChargedPrimary: 좌클릭 차징 스킬
  - ChargedSpecial: 우클릭 차징 스킬
  - JumpPrimary: 공중 좌클릭 스킬
  - JumpSpecial: 공중 우클릭 스킬
- SkillTreeSkills: 스킬트리에 표시할 전체 후보
- InitiallyUnlockedSkills: 시작부터 Drag 가능한 스킬
- DefaultDodgeSkill: 무기 장착 시 자동 바인딩할 회피 Data Asset

DefaultSkillBindings에 들어간 스킬은 자동으로 해금되고 해당 입력 슬롯의 초기 스킬이 된다.

## 3. Character Blueprint 연결

BP_FirstPersonCharacter를 열고 Combat Component를 선택한다.

Weapon Skill Profiles 배열에 사용할 프로필을 등록한다.

예:

1. DA_SkillProfile_Unarmed
2. DA_SkillProfile_Dagger
3. DA_SkillProfile_GreatSword
4. 나머지 무기 프로필

무기 장착 상태가 바뀌면 CombatComponent가 WeaponType에 맞는 프로필을 다시 적용한다.

기존 PrimaryAttackSkill, SpecialAttackSkill 등의 필드는 이전 에셋 호환용 fallback이다.
새 콘텐츠는 Weapon Skill Profiles를 기준으로 작성한다.

## 4. 스킬트리 Widget Blueprint

### 스킬 엔트리

새 Widget Blueprint를 만들고 부모를 URogue10mSkillTreeEntryWidget으로 지정한다.

필요한 선택적 변수명:

- UI_SkillNameText: TextBlock
- UI_SkillIconImage: Image

권장 이름:

- WBP_SkillTreeEntry

해금된 엔트리는 좌클릭 Drag가 가능하다.
잠긴 엔트리는 낮은 투명도와 비활성 상태로 표시되고 Drag할 수 없다.

### 스킬트리 창

스킬트리 창 Widget Blueprint의 부모를 URogue10mSkillTreeWindowWidget으로 지정한다.

필요한 변수명:

- UI_SkillListContainer: VerticalBox, WrapBox 또는 다른 PanelWidget
- SkillTreeEntryWidgetClass: WBP_SkillTreeEntry 지정

스킬트리 창을 열 때 현재 무기 프로필의 SkillTreeSkills를 다시 구성한다.

## 5. 스킬 슬롯 Drop

기존 WBP_QuickSlot의 부모가 URogue10mQuickSlotWidget이면 별도 Blueprint Drop 그래프는 필요하지 않다.

스킬트리 엔트리를 다음 슬롯으로 드롭하면 해당 입력에 장착된다.

- 좌클릭
- 우클릭
- 좌클릭 차징
- 우클릭 차징

다섯 번째 E 회피 슬롯은 무기 프로필에서 자동 결정되므로 Drop을 받지 않는다.

숫자키 1~5는 소비 아이템 슬롯 입력으로 유지된다. 공격 스킬 입력과 충돌하지 않는다.

## 6. 런타임 흐름

1. Character가 무기를 장착한다.
2. CombatComponent가 WeaponType에 대응하는 Weapon Skill Profile을 찾는다.
3. 기본 공격 스킬과 기본 회피를 적용한다.
4. SkillTree 창은 프로필의 SkillTreeSkills를 표시한다.
5. 해금된 스킬을 HUD 입력 슬롯으로 Drag한다.
6. QuickSlotWidget이 Drop을 받아 CombatComponent의 Loadout을 변경한다.
7. 좌/우 클릭 또는 차징 입력은 변경된 Loadout의 AttackSkill Data Asset을 실행한다.

## 격투가 트리와 테스트·키 설정 (2026-09-30)

맨손과 너클 프로필은 기본기4와 무공10의 공용14노드(공격13·패시브1)를 K 창에 표시한다. 정상 시작은 기본기4와 연환권이며 나머지 무공은 기존 수련·토벌 규칙으로 열린다.

- **테스트 · 현재 트리 전체 해금**: 현재 세션의 모든 노드를 즉시 해금한다. 수련/토벌 횟수를 바꾸지 않으며 새 세션에서는 다시 누른다. Shipping에서는 제공하지 않는다.
- **노드 선택 → 키 지정**: 해금한 공격에 단일 키보드 키를 지정한다. Esc로 키 선택을 취소하고 지정 키 해제로 삭제한다. 키 설정은 로컬 플레이어 SaveGame에 저장된다.
- 이동·메뉴·소비품1~5 등 기존 입력과 현재 트리의 중복 키는 거부한다. 차징 기술은 표시된 시간 이상 누른 뒤 놓고, 점프 기술은 공중에서 사용한다. 패시브는 키 없이 상시 적용된다.
- 기존 마우스 입력과 HUD 드래그 장착은 계속 사용할 수 있다. 새 직접 스킬 키도 기존 GAS·자원·쿨다운 검사를 거친다.

세부 결과와 실행 검증은 `Feature/doc/2026-09-30_fighter-skill-tree.md`를 참고한다.

## 현재 제한과 확장 지점

- 장착 상태는 현재 플레이 세션의 CombatComponent에 보관된다.
- 영구 저장이 필요하면 이후 SaveGame 또는 PlayerState 저장 모델로 이동한다.
- 스킬 포인트, 선행 스킬, 레벨 조건은 SkillTreeSkills의 해금 규칙 확장 지점이다.
- 멀티플레이 권한 검증이 필요해지면 장착 요청을 Server RPC로 전환한다.
