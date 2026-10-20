# 스킬트리 Loadout과 무기별 회피 설정 가이드

2026-10-20부터 맨손·너클 격투가와 대검술사의 K 메뉴는 혼합 성장 스킬트리를 사용한다. 일반 무기 프로필·공격·회피·HUD 슬롯 연결은 아래 구조를 유지하며, 두 직업의 전수 조건과 화면은 [혼합 성장 설계의 구현 안내](HybridSkillTreeDesign.md#11-격투가대검술사-구현-안내--2026-10-20)를 따른다. 빌드·런타임 검증 결과는 [개발 결과 문서](../Feature/doc/2026-10-20_hybrid-skill-tree-runtime.md)에 별도로 기록한다.

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

DefaultSkillBindings는 해당 입력 슬롯의 초기 스킬을 정한다. 혼합 성장 트리가 관리하는 공격은 카탈로그의 전수 상태를 먼저 확인하며, 이 배열에 넣는 것만으로 숙련 조건을 건너뛸 수 없다. 시작 기본기는 무료 전수한 뒤 기본 입력을 구성한다. 다른 무기의 기존 초기 전수 흐름은 유지한다.

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

아래 WBP 구성은 기존 스킬트리의 연결 지침이다. 격투가·대검술사는 K 메뉴에서 `URogue10mHybridSkillTreeWidget`의 노드 화면을 사용하며, 다른 무기의 기존 창을 대체하지 않는다.

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
4. 격투가·대검술사는 혼합 카탈로그의 노드·실제 숙련·공용 포인트를 표시하고, 다른 무기는 기존 프로필의 SkillTreeSkills를 표시한다.
5. 해금된 스킬을 HUD 입력 슬롯으로 Drag한다.
6. QuickSlotWidget이 Drop을 받아 CombatComponent의 Loadout을 변경한다.
7. 좌/우 클릭 또는 차징 입력은 변경된 Loadout의 AttackSkill Data Asset을 실행한다.

## 이전 격투가 트리와 테스트·키 설정 — 2026-09-30 기록

당시 맨손과 너클 프로필은 기본기4와 무공10의 공용14노드(공격13·패시브1)를 K 창에 표시했다. 정상 시작은 기본기4와 연환권이며 나머지는 사용·토벌 조건을 따랐다. 이 절은 이전 창의 제작 기록이며, 현재 두 직업의 K 메뉴와 해금 조건은 다음 절을 따른다.

- **테스트 · 현재 트리 전체 해금**: 현재 세션의 모든 노드를 즉시 해금한다. 수련/토벌 횟수를 바꾸지 않으며 새 세션에서는 다시 누른다. Shipping에서는 제공하지 않는다.
- **노드 선택 → 키 지정**: 해금한 공격에 단일 키보드 키를 지정한다. Esc로 키 선택을 취소하고 지정 키 해제로 삭제한다. 키 설정은 로컬 플레이어 SaveGame에 저장된다.
- 이동·메뉴·소비품1~5 등 기존 입력과 현재 트리의 중복 키는 거부한다. 차징 기술은 표시된 시간 이상 누른 뒤 놓고, 점프 기술은 공중에서 사용한다. 패시브는 키 없이 상시 적용된다.
- 기존 마우스 입력과 HUD 드래그 장착은 계속 사용할 수 있다. 새 직접 스킬 키도 기존 GAS·자원·쿨다운 검사를 거친다.

세부 결과와 실행 검증은 `Feature/doc/2026-09-30_fighter-skill-tree.md`를 참고한다.

## 혼합 스킬트리 사용법 — 2026-10-20

K 메뉴의 격투가·대검술사 탭에서 중앙→기본기·아이덴티티→스탯·기술→최종기·공명 경로를 확인한다. 다른 직업 탭은 미리보기이며 해당 무기를 장착한 뒤 전수·활성·키 지정을 진행한다. 맨손과 너클은 하나의 격투 숙련을 공유하고 대검 숙련은 별도로 쌓인다.

- 숙련 조건을 만족한 스탯·일반 공격·심법은 0P로 자동 전수된다. 실제 몬스터에게 양수 피해를 준 공격 실행1회당 숙련 XP1을 인정하며, 다단히트·여러 대상은 중복 집계하지 않는다. 허공·무효 피해·훈련 표적·전투 테스트 인스턴스는 제외한다. 현재 M20까지이며 M12 누적385XP, M20 누적1,045XP다.
- **공용 포인트**는 `레벨 - 1 - 특성 실제 지출 - 트리 실제 지출`이다. 레벨1의 획득량은0P이며 특성과 같은 잔액을 사용한다. 숙련 XP는 소비하지 않는다.
- 격투가는 내공·투지, 대검술사는 투지·마나를 사용한다. 입문1P, 성질 선택2P이며 활성 계열은 하나다. 내공 심화는2P 또는 격투 M12·숙련 스태미나+10, 투지 심화는2P 또는 격투/대검 중 한 계열의 M12·해당 숙련 공격력+3이다. 마나 심화는2P 전용이다. 장비·특성의 스탯은 수련 대안을 대신하지 않는다.
- 투지는 공용 NodeId로 한 번만 투자한다. 마나는 격투가에서 구매·활성·공명할 수 없다. 대검 마나 발동은 실제 MP5를 추가 소비하고 MP 부족 시 게이지를 보존한다.
- **전수·성질 선택·활성 변경·부분 재분배**는 StartHub에서 살아 있고 탐험 출발 전이며 안내인 상호작용 거리 안에 있을 때만 가능하다. 프로필을 선택했다면 저장 준비도 필요하다. 창 열람에 NPC 대화를 요구하지 않는다.
- **선택 단계 부분 재분배**는 선택한 입문·심화·성질부터 유효 선행을 잃는 하위 단계·공명을 철회하고 실제 지출 P를 환급한다. 무료 숙련·스탯·기술과 상위 단계는 유지한다. 입문을 남기고 유료 심화만 환급한 뒤 수련 조건을 만족하면 심화를0P로 다시 전수할 수 있다. 성질은 다시 선택·구매한다.
- 해금한 공격을 선택해 실행 키를 지정하거나 기존 HUD 슬롯으로 Drag & Drop한다. Esc는 키 선택을 취소한다. 패시브에는 공격 키를 지정하지 않는다. 공격은 기존 GAS·자원·쿨다운 검사를 사용한다.

카탈로그 `/Game/DataAsset/SkillTree/DA_HybridSkillTreeCatalog`에는 53개 고유 노드가 있다. 대검의 분쇄참·회전참·파쇄참·대지파쇄·패왕참은 기존 동작을 사용하는 파생 AttackSkill 에셋5개이며 `Content/DataAsset/AttackSkill/HybridSkillTree/GreatSword/`에 둔다. 카탈로그는 `Rogue10mSkillTreeDataAsset`, 전수 상태는 PlayerState의 `Rogue10mSkillTreeProgressionComponent`, 임시 게이지·발동은 Character의 `Rogue10mSkillTreeCombatComponent`가 담당한다.

Save v3은 숙련·전수 경로·실제 지출·성질·활성 계열을 보관한다. v1/v2의 전수 기술과 바인딩은 legacy 기록으로 유지하되 과거 사용 횟수를 숙련 XP나 영구 스탯으로 바꾸지 않는다. 생산 저장과 분리한 QA 저장으로 복원·환급·실제 명중을 검증하며, 최종 통과 여부는 [개발 결과 문서](../Feature/doc/2026-10-20_hybrid-skill-tree-runtime.md)에 기록한다.

## 현재 제한과 확장 지점

- 공격 실행·현재 장착은 CombatComponent가 담당하며, 선택 프로필의 전수·입력 슬롯과 혼합 성장은 Progression SaveGame v3에 저장한다. 직접 실행 키는 기존 로컬 키 설정 저장 경계를 사용한다.
- 새 혼합 노드는 독립 카탈로그에서 선행 AND/OR·포인트·숙련·보상·허용 계열을 설정한다. AttackSkillData는 공격 실행 데이터로 유지하며 스탯이나 아이덴티티를 빈 공격으로 만들지 않는다.
- 격투가·대검술사 외 직업의 혼합 성장 트리와 가드·회피 숙련 보상은 이번 구현 범위에 포함하지 않는다.
- 멀티플레이 권한 검증이 필요해지면 장착 요청을 Server RPC로 전환한다.
