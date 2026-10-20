# Sprint 변경 이력

이 문서는 Rogue10m의 Sprint별 주요 변경 사항과 검증 결과를 요약한다.
세부 설계와 구현 결과는 Feature 문서, 일자별 작업 과정은 DevLog를 참조한다.

## 기록 규칙

각 작업은 다음 형식으로 누적한다.

- 작업 식별자: Sprint#스프린트번호-작업번호
- 브랜치: Sprint#스프린트번호-작업번호-간단한-영문-설명
- Sprint 번호: `test`가 `main`에 성공적으로 병합되어 현재 Sprint가 종료된 시점에만 증가
- 작업 번호: 같은 Sprint 안에서 독립 작업이 시작될 때 증가하며, 다음 Sprint 시작 시 1로 초기화
- 상태: 계획 / 개발 중 / 빌드 완료 / QA 완료 / main 반영
- 목표: 작업이 해결하려는 문제
- 주요 변경: 플레이어가 체감하거나 구조적으로 중요한 변경
- 검증: 빌드, 테스트 또는 확인한 체크리스트
- 관련 문서: Feature/architect 및 Feature/doc 문서

작은 수정이 같은 Sprint 작업 범위에 포함되면 기존 항목에 추가한다.
별도 기능이나 독립적인 롤백 경계를 가진 작업은 다음 작업번호를 사용한다.

---

## Sprint 0 — 플레이어블 프로토타입 기반

### 주요 목표

- 10분 로그라이크 전투의 기본 플레이 루프 구성
- 전투, 장비, 스킬, HUD, 몬스터와 런 진입 구조 확보
- UE 5.8 프로젝트 Harness와 문서 흐름 도입

### 주요 변경

- 시작 허브와 전투 맵 이동 흐름
- 기본 몬스터와 플레이어 전투
- GAS 기반 Attribute 및 스킬 구조
- 공격 Data Asset과 콤보/쿨다운
- 장비, 인벤토리 초기 구조
- MainHUD와 Widget Part 브리지
- 다단 히트, 피해 숫자, 플레이어 피격 피드백
- Lazy Codex Harness와 Feature/DevLog 문서 체계

### 상태

- main 반영

### 관련 문서

- Feature/doc/2026-06-23_prototype-run-flow.md
- Feature/doc/2026-06-29_equipment-damage-feedback.md
- Feature/doc/2026-07-02_gas-skill-system.md
- Feature/doc/2026-07-11_multihit-damageable-targets.md
- Feature/doc/2026-07-12_attack-patterns-and-hit-modes.md
- Feature/doc/2026-07-12_hud-refresh-optimization.md

---

## Sprint 1 — 조작, 성장, 인벤토리, 몬스터 데이터 확장

### Sprint#1-1 — sprint-postprocess-stamina

- 상태: 빌드 완료, 통합 대기
- 목표: 달리기와 회피 조작 및 플레이어 전투 피드백 개선
- 주요 변경:
  - Shift 달리기와 스테미나 소비
  - Post Process 기반 달리기 효과
  - 방향성 Dodge와 입력 잠금
  - Space 점프, E 구르기 입력 재배치
  - 공중 구르기 차단
  - 피격 효과 지속 시간 0.5초 설정
- 검증:
  - Rogue10mEditor 빌드
  - 이동 상태와 입력 충돌 코드 검토
- 관련 문서:
  - Feature/doc/2026-07-12_sprint-postprocess-stamina.md
  - Feature/doc/2026-07-12_directional-dodge.md
  - Feature/doc/2026-07-13_jump-roll-input-remap.md

### Sprint#1-2 — progression-skill-slots

- 상태: 빌드 완료, 통합 대기
- 목표: 레벨/경험치 표시와 스킬 슬롯 상태 표현 개선
- 주요 변경:
  - 경험치 Hover 상세 정보
  - 현재 경험치 / 필요 경험치 소수점 3자리 표시
  - 정수 레벨 표시
  - WBP_Progression을 LevelExperience UI로 정리
  - 좌/우 공격, 차징, Space 스킬 슬롯 배치
  - 해금 및 콤보 진행 상태에 따른 슬롯 비활성화
- 검증:
  - Rogue10mEditor 빌드
  - Widget Blueprint 변수명 확인
- 관련 문서:
  - Feature/doc/2026-07-12_experience-hover-level.md
  - Feature/doc/2026-07-12_level-experience-skill-slots.md

### Sprint#1-3 — grid-inventory-windows

- 상태: 빌드 완료, 통합 대기
- 목표: 확장 가능한 NxM 인벤토리와 메뉴 창 기반 마련
- 주요 변경:
  - 기본 10x10 인벤토리
  - NxM 아이템 배치 및 충돌 검사
  - 가방 아이템 기반 추가 컨테이너
  - 아이템 Data Asset과 월드 드롭 Mesh
  - 인벤토리, 장비, 스킬트리 UserWidget 부모 클래스
  - 메뉴 창 토글과 입력 모드 처리
- 검증:
  - Rogue10mEditor 빌드
  - 그리드 배치/이동/제거 API 검토
- 관련 문서:
  - Feature/doc/2026-07-13_grid-inventory-and-windows.md
  - Docs/GridInventoryAndMenuWindowsGuide.md

### Sprint#1-4 — monster-data-regen-ui-cleanup

- 상태: 빌드 완료, 통합 대기
- 목표: 몬스터 데이터화, 공용 자원 회복, HUD 단순화
- 주요 변경:
  - Monster Data Asset 타입 추가
  - 이름, 레벨, 경험치, Vitals, Mesh, AnimBP, AI/공격 설정 데이터화
  - 플레이어/몬스터 공용 초당 자원 회복 컴포넌트
  - 기본 체력/스테미나/마나 회복량 각각 초당 0.1
  - MonsterInfo를 LV N : 이름 + HP 형태로 변경
  - MainWidget MiniMap 임시 비활성화
  - 아이템 획득 피드를 이미지 | 수량으로 변경
  - 현재 프로젝트 구조 Mermaid 문서 추가
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - CheckGeneratedChanges 통과
  - git diff --check 통과
- 관련 문서:
  - Feature/doc/2026-07-13_monster-data-regen-and-hud-cleanup.md
  - Docs/CurrentProjectArchitecture.md

---

## 다음 Sprint 작성 템플릿

### Sprint#N-M — short-description

- 상태: 계획
- 목표:
- 주요 변경:
  -
- 검증:
  -
- 관련 문서:
  - Feature/architect/YYYY-MM-DD_feature-name.md
  - Feature/doc/YYYY-MM-DD_feature-name.md
### Sprint#1-5 — skill-tree-drag-drop

- 상태: 빌드 완료, 에디터 에셋 설정 대기
- 목표: 하드코딩 스킬 경로 제거와 스킬트리 기반 Loadout 구성
- 주요 변경:
  - 무기별 Weapon Skill Profile Data Asset
  - 기본 1m Dodge Skill Data Asset
  - 무기 변경 시 기본 회피 자동 바인딩
  - 스킬트리 해금 스킬 Drag 시작
  - 좌/우 클릭 및 차징 슬롯 Drop 장착
  - E 회피 슬롯 자동 표시 및 Drop 차단
  - 기존 숫자키 소비 아이템 슬롯 유지
- 검증:
  - UnrealHeaderTool 성공
  - Rogue10mEditor Win64 Development 빌드 성공
  - Tick 추가 없음
- 관련 문서:
  - Feature/architect/2026-07-13_skill-tree-drag-drop-loadout.md
  - Feature/doc/2026-07-13_skill-tree-drag-drop-loadout.md
  - Docs/SkillTreeLoadoutGuide.md
### Sprint#1-6 - equipment-p-key

- 상태: 구현 및 Editor 빌드 완료
- 목표: 장비창 입력을 P로 변경하고 메뉴 Widget 및 회피 Data Asset 설정 위치 명확화
- 주요 변경:
  - 장비창 토글 키 B에서 P로 변경
  - 메뉴 창 가이드 및 입력 의사 코드 갱신
  - InventoryWindowWidgetClass 및 SkillTreeWindowWidgetClass 설정 절차 정리
  - 무기별 Weapon Skill Profile의 DefaultDodgeSkill 참조 경로 정리
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - CheckGeneratedChanges Harness 경로 검사 통과
- 관련 문서:
  - Feature/architect/2026-07-14_equipment-p-key-and-editor-setup.md
  - Feature/doc/2026-07-14_equipment-p-key-and-editor-setup.md
  - Docs/GridInventoryAndMenuWindowsGuide.md
  - Docs/SkillTreeLoadoutGuide.md
### Sprint#1-7 - character-assets-menu-widgets

- 상태: C++ 빌드 완료, Editor 에셋 생성 대기
- 목표: 캐릭터 기본 전투 Data Asset과 필수 메뉴 UserWidget 에셋 구성
- 주요 변경:
  - 맨손 기본 회피 Data Asset 생성 정의(100cm / 0.16초 / 쿨타임 0.45초 / 스테미나 10)
  - 기존 맨손 공격 6종을 묶는 Weapon Skill Profile 생성 및 Combat Component 연결 자동화
  - 인벤토리 10x10 그리드/용량 표시 골격
  - 장비 슬롯 컨테이너 골격
  - 스킬 트리 엔트리/목록/Drag & Drop 안내 골격
  - 메뉴 WBP 생성과 PlayerController 클래스 연결 자동화
- 검증:
  - Unreal Python `py_compile` 성공
  - Rogue10mEditor Win64 Development 빌드 성공
  - 열린 Editor Remote Execution 활성화 후 `.uasset` 생성 검증 예정
- 관련 문서:
  - Feature/architect/2026-07-14_character-assets-and-menu-widgets.md
  - Feature/doc/2026-07-14_character-assets-and-menu-widgets.md
  - Scripts/Editor/CreateCharacterAssetsAndMenuWidgets.py
#### Sprint#1-7 추가 문서 - AI 개발 세팅 참조

- Unreal Python, Remote Execution, AI 플러그인 설정 정리
- Editor 스크립트 실행 및 외부 연결 예시 추가
- Hot Reload, 에셋 저장, 동시 Editor 실행 관련 안전 규칙 추가
- 관련 문서: `Ai 개발 세팅 참조/README.md`
### Sprint#1-8 - plugin-cpp-cleanup

- 상태: 구현 및 Editor 빌드 완료
- 목표: 사용하지 않는 Editor 플러그인 자동 활성화와 C++ 설정·모듈 의존성 정리
- 주요 변경:
  - ModelingToolsEditorMode, StateTree, GameplayStateTree 프로젝트 자동 활성화 제거
  - StateTreeModule, GameplayStateTreeModule 의존성 제거
  - Main HUD 비활성 미니맵 바인딩·클래스 설정·갱신 함수 제거
  - Build.cs 빈 설정과 샘플 주석 제거
  - Python __pycache__ 및 bytecode Git 무시 규칙 추가
  - 사용 중인 GAS, AI, UMG/Slate, AI Assistant/MCP 플러그인 유지
- 검증:
  - Unreal 프로젝트 파일 생성 성공
  - Rogue10mEditor Win64 Development 빌드 성공
  - CheckGeneratedChanges 및 git diff --check 통과
- 관련 문서:
  - Feature/architect/2026-07-14_plugin-cpp-cleanup.md
  - Feature/doc/2026-07-14_plugin-cpp-cleanup.md
### Sprint#1-9 - menu-window-widget-setup

- 상태: 구현, Editor 에셋 설정 및 빌드 완료
- 목표: InventoryWindowWidget과 SkillTreeWindowWidget 누락 경고 해결 및 실제 WBP 연결
- 주요 변경:
  - WBP_InventoryWindow, WBP_EquipmentWindow, WBP_SkillTreeEntry, WBP_SkillTreeWindow 검증·컴파일
  - BP_FirstPersonPlayerController에 세 Menu Window Widget Class 지정
  - SkillTreeWindow에 WBP_SkillTreeEntry 지정
  - 명시 Class 누락 시 기본 WBP를 불러오는 Soft Class fallback 추가
  - 메뉴 WBP 생성·복구용 Editor Python 스크립트 추가
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - Editor CDO ClassProperty 재조회 성공
  - 저장 후 dirty package 0건
  - CheckGeneratedChanges 및 git diff --check 통과
- 관련 문서:
  - Feature/architect/2026-07-14_menu-window-widget-setup.md
  - Feature/doc/2026-07-14_menu-window-widget-setup.md
  - Docs/GridInventoryAndMenuWindowsGuide.md
### Sprint#1-10 - content-asset-naming

- 상태: 구현, Editor 에셋 참조 정리 및 빌드 완료
- 목표: Widget, DataAsset, FirstPerson 콘텐츠의 UE 5.8 명명 규칙 위반 정리
- 주요 변경:
  - `UW_Rogue10mMainWidget` → `WBP_Rogue10mMainHUD`
  - `ABP_FP_Copy` → `ABP_FirstPerson`
  - `CtrlRig_FPWarp` → `Rig_FirstPersonWarp`
  - Main HUD C++ Soft Class 및 유지보수 스크립트·가이드 경로 갱신
  - 재실행 가능한 Editor Python 이름 변경·구 소스 정리 절차 추가
  - DataAsset의 `DA_` 접두사와 World Partition `Lvl_FirstPerson`은 검토 후 유지
- 검증:
  - Widget 21개, DataAsset 10개, FirstPerson 7개 전수 감사 결과 위반 0건
  - 구 경로 3개 부재, 신규 경로 3개 존재
  - Character/Anim Blueprint/Control Rig 및 PlayerController/Main HUD 참조 확인
  - Object Redirector 0건, dirty package 0건
  - Rogue10mEditor Win64 Development 빌드 성공
- 관련 문서:
  - Feature/architect/2026-07-14_content-asset-naming.md
  - Feature/doc/2026-07-14_content-asset-naming.md
  - Docs/WidgetBlueprintHUDGuide.md
### Sprint#1-11 - stone-fist-identity

- 상태: 데이터 구성, Editor 자산 연결 및 C++ 빌드 완료
- 목표: 권 아이덴티티의 기본 전투·회피·2단 점프를 데이터 주도 구조로 설계하고 플레이어 기본 프로필로 연결
- 주요 변경:
  - `T_Identity_StoneFist` 대표 이미지와 권 스킬 아이콘 6종 반영
  - 잽 → 스트레이트 2타 콤보, 우클릭 차징 권압, 점프 내려찍기 Data Asset 생성
  - 일반 우클릭과 좌클릭 차징 미바인딩
  - 권보 Dodge Data Asset과 `Knuckle` Skill Profile 생성
  - CharacterData 기본 무기 타입 및 Profile 최대 점프 횟수 추가
  - BP_FirstPersonCharacter CombatComponent에 CharacterData/권 프로필 연결
  - Data Asset과 GAS의 책임 경계 및 후속 전용 Ability 마이그레이션 정의
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - Editor Remote Execution 자산 로드·속성·바인딩·콤보 참조 확인
  - 공중 회피 차단 및 `MaxJumpCount=2` 코드 경로 확인
- 관련 문서:
  - Feature/architect/2026-07-14_stone-fist-identity.md
  - Feature/doc/2026-07-14_stone-fist-identity.md
### Sprint#1-12 - menu-widget-layout

- 상태: 구현, Editor 자산 재컴파일 및 빌드 완료
- 목표: Parts UI 스타일을 기준으로 Inventory, Equipment, SkillTree 메뉴를 실제 확장 가능한 레이아웃으로 재구성
- 주요 변경:
  - 공통 Canvas/Frame/Content 메뉴 계층과 다크 패널/금색 강조 스타일
  - Inventory 10x10 UniformGrid, NxM ItemCanvas, BagTab, Capacity 영역
  - Equipment 캐릭터 프리뷰, 7개 장비 슬롯, 능력치 영역
  - SkillTreeEntry 아이콘/이름/설명/잠금 상태 카드
  - SkillTree ScrollBox/WrapBox, 필터, 스킬 포인트 영역
  - WBP 4종과 PlayerController/SkillTreeEntry 클래스 참조 재컴파일·저장
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - Editor Remote Execution 클래스 재로드와 참조 확인
  - Tick 추가 없음
- 관련 문서:
  - Feature/architect/2026-07-14_menu-widget-layout.md
  - Feature/doc/2026-07-14_menu-widget-layout.md

### Sprint#1-13 - menu-designer-layout

- 상태: 구현, Editor 자산 직접 배치 및 빌드 완료
- 목표: 메뉴 UI를 C++ Slate fallback이 아닌 Widget Blueprint Designer 소유 구조로 전환
- 주요 변경:
  - Inventory Canvas/Frame/Grid/NxM ItemCanvas/하단 돈·무게 직접 배치
  - GridSize X×Y에 맞춘 WBP_InventoryCell UserWidget 자동 생성
  - BagTab/Capacity/Hint 제거 및 Item Data Asset UnitWeight 기반 총 무게 계산
  - Equipment 프리뷰/장비 슬롯 7종/능력치 영역 직접 배치
  - SkillTreeEntry 아이콘/이름/설명/잠금 상태 UserWidget 구성
  - SkillTreeWindow 필터/ScrollBox/WrapBox/스킬 포인트 영역 직접 배치
  - Overlay 제거, Root Canvas 형제 배치와 ZOrder 사용
  - 네이티브 `RebuildWidget()` 제거 및 필수 `BindWidget` 계약 적용
  - Editor Python 필수 이름·Overlay 0개 검증 추가
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 메뉴 WBP 4종과 WBP_InventoryCell Compile/Save 성공
  - InventoryCellWidgetClass 연결 및 제거 대상 위젯 0개 확인
  - 필수 위젯 누락 0개, Overlay 0개
- 관련 문서:
  - Feature/architect/2026-07-15_menu-designer-layout.md
  - Feature/doc/2026-07-15_menu-designer-layout.md
  - Docs/GridInventoryAndMenuWindowsGuide.md

### Sprint#1-13 - NxM Inventory Cell/Item/BagTab 상호작용

- 목표: 인벤토리 좌표 셀, NxM 아이템, 가방 탭을 분리된 UserWidget으로 구현하고 회전·충돌 프리뷰를 제공한다.
- 주요 변경: 회전 상태 저장, 회전 footprint 기반 경계/AABB 충돌, Canvas 좌표 스냅, 잡은 셀 오프셋, R키 90도 회전, 녹색/적색 프리뷰, 가방 탭 전환.
- 자산: `WBP_InventoryCell`, `WBP_InventoryItem`, `WBP_BagTab`, `WBP_InventoryWindow`.
- 검증: Rogue10mEditor Development 빌드 성공, 열린 Editor WBP 컴파일/저장 및 클래스 할당 확인.
- 상태: 구현 및 로컬 검증 완료. PIE에서 아이템 Data Asset별 NxM 이동/회전 체감 QA 필요.
- 관련 문서: `Feature/architect/2026-07-15_menu-designer-layout.md`, `Feature/doc/2026-07-15_menu-designer-layout.md`, `Docs/GridInventoryAndMenuWindowsGuide.md`.
### Sprint#1-13 - menu-widget-contract-and-folders

- 상태: 구현 및 로컬 검증 완료
- 목표: BagTab 바인딩 오류와 UserInterfaceSettings EditCondition 오류를 해소하고 메뉴 자산 구조 및 Inventory Cell 시인성을 정리한다.
- 주요 변경:
  - `UI_BagSizeText` C++ 필수 바인딩 및 BagTab GridSize 인자 제거
  - `WBP_InventoryCell`에 0.5 패딩 기반 어두운 외곽선 적용
  - 메뉴 자산을 `Inventory`, `Equipment`, `SkillTree` 기능별 폴더로 이동
  - PlayerController SoftClassPath, 내부 WidgetClass, Editor 자동화 경로 갱신
  - 이동 후 누락된 `SkillTreeEntryWidgetClass` 재지정
  - UserInterfaceSettings Font DPI 기본값 명시 및 전체 Editor 모듈 재빌드
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - UE5.8 commandlet 메뉴 WBP 7종 컴파일 및 내부 클래스 참조 검사 성공
  - `UI_BagSizeText`, `LogEditCondition`, `bUseCustomFontDPI` 오류 재발 없음
- 관련 문서:
  - `Feature/architect/2026-07-15_menu-designer-layout.md`
  - `Feature/doc/2026-07-15_menu-designer-layout.md`
  - `Docs/GridInventoryAndMenuWindowsGuide.md`
### Sprint#1-13 - inventory-grid-visual-balance

- 상태: 구현 및 로컬 WBP 검증 완료, PIE 시각 재확인 필요
- 목표: Inventory Cell과 창 내부 구획의 시각적 균형을 마비노기/Diablo II식 NxM Grid 기준으로 개선한다.
- 주요 변경:
  - 44×44 Cell에 1px 다크 경계와 차콜 Fill 적용
  - 448×448 `UI_InventoryGridFrame` 추가
  - Grid/ItemCanvas 중심 `(0, 16)` 통일
  - BagTab 136×36, 14pt 및 어두운 배경 적용
  - Title/Tab/Grid/BottomInfo 중첩 제거와 대칭 여백 확보
- 검증:
  - UE5.8 메뉴 WBP 7종 commandlet 컴파일 성공
  - `UI_InventoryGridFrame` 필수 Designer 위젯 확인
  - Inventory/SkillTree 내부 WidgetClass 참조 확인
  - Python 오류 및 Overlay 0개
- 관련 문서:
  - `Feature/architect/2026-07-15_menu-designer-layout.md`
  - `Feature/doc/2026-07-15_menu-designer-layout.md`
  - `Docs/GridInventoryAndMenuWindowsGuide.md`

### Sprint#1-13 - prototype-inventory-items

- 상태: 구현, Editor Data Asset 생성 및 C++ 빌드 검증 완료
- 목표: NxM 인벤토리 배치와 회전 QA에 사용할 1x1, 2x3, 4x3 프로토타입 아이템 준비
- 주요 변경:
  - 프로토타입 Item Data Asset 3종 생성
  - 아이콘 미지정 시 사용하는 InventoryTint 속성 추가
  - 빈 기본 인벤토리에 시작 아이템을 한 번만 자동 배치
  - bAddPrototypeStartingItems, PrototypeStartingItems로 에디터 조정 가능
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - Unreal Python 에셋 생성 및 InventorySize 재검증 성공
- 관련 문서:
  - Feature/architect/2026-07-15_prototype-inventory-items.md
  - Feature/doc/2026-07-15_prototype-inventory-items.md

### Sprint#1-14 - project-warning-cleanup

- 상태: 프로젝트 측 경고 수정 및 독립 commandlet 검증 완료
- 목표: 삭제된 몬스터 컴포넌트 직렬화 경고와 GameplayCue 전체 콘텐츠 검색 경고를 제거하고 엔진 자체 경고를 분리한다.
- 주요 변경:
  - `BP_BaseMonster`와 배치 External Actor에서 삭제된 `Rogue10mVitalsComponent` 참조 제거
  - 현재 GAS Ability System 및 Vital Regeneration 컴포넌트 유지 확인
  - `GameplayCueNotifyPaths=/Game/GameplayCues` 설정 추가
  - `ValidateWarningFixes.py` 회귀 검증 추가
  - `r.MotionVectorSimulation`은 UE 5.8 엔진 측 Render Thread Safe 플래그 문제로 판정하고 프로젝트 렌더 설정은 유지
- 검증:
  - 구형 Vitals 문자열 참조 0건
  - 새 UE5.8 commandlet 검증 성공, 0 errors / 0 warnings
- 관련 문서:
  - `Feature/architect/2026-07-16_project-warning-cleanup.md`
  - `Feature/doc/2026-07-16_project-warning-cleanup.md`
  - `DevLog/20260716.txt`

### Sprint#1-14 - inventory-item-footprint

- 상태: 구현, C++ 빌드, WBP 재구성 및 독립 commandlet 검증 완료. PIE 시각 QA 필요.
- 목표: 단일 10×10 인벤토리에서 1×1·2×3·4×3 아이템의 셀 점유 크기와 아이콘 원본 종횡비를 정확히 유지한다.
- 주요 변경:
  - Grid Entry와 DragDrop Operation의 회전 상태 및 R키 회전 처리 제거
  - Item Data Asset의 원본 `InventorySize`만 배치·충돌 footprint로 사용
  - Designer SizeBox 기본값을 44×44로 명시하고 Canvas 슬롯과 런타임 `UI_InventoryItemSize`를 `Width×44`, `Height×44`로 동기화
  - Preview Border를 전체 footprint에 Fill하고 아이콘은 4px inset `ScaleBox(ScaleToFit)`로 분리
  - `SetBrushFromTexture(Icon, true)`로 텍스처 실제 크기를 반영해 세로형·가로형 아이콘 종횡비 보존
  - 아이콘 있는 아이템은 크기 문자열을 숨기고 비장비 스택 수량만 우측 상단 표시
  - 아이콘 없는 프로토타입은 `InventoryTint`와 `W×H` fallback 표시
  - Inventory Window의 BagTab 컨테이너와 클래스 참조를 제거하고 컨테이너 0으로 고정
  - 제거된 탭 공간에 맞춰 Grid Frame/Grid/ItemCanvas 중심을 `(0,-12)`로 통일
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - `WBP_InventoryItem`, `WBP_InventoryWindow` 재구성·컴파일·저장 성공
  - 새 commandlet에서 메뉴 WBP 7종 컴파일 및 클래스 참조 검증 성공
  - 최종 `WBP_InventoryItem`에 IconScale 포함, `WBP_InventoryWindow`에 BagTabContainer 없음
  - 최신 독립 검증 로그의 Blueprint/Python 오류 0건
  - 런타임 회전 코드 검색 0건
- 관련 문서:
  - `Feature/architect/2026-07-15_inventory-item-footprint.md`
  - `Feature/doc/2026-07-15_inventory-item-footprint.md`
  - `Docs/GridInventoryAndMenuWindowsGuide.md`

### Sprint#1-14 - inventory-item-icon-scale

- 상태: 구현 및 C++ 빌드 검증 완료
- 목표: Item Data Asset마다 인벤토리 아이콘의 표시 크기를 원본 비율을 유지한 채 조절한다.
- 주요 변경:
  - `InventoryIconScale` Data Asset 속성 추가, 기본값 `1.0`, 범위 `0.1~2.0`
  - `ScaleToFit` 이후 중앙 Pivot 기준 균일 Render Scale 적용
  - 확대 아이콘이 인접 셀을 침범하지 않도록 footprint 경계 클리핑
  - 점유 크기·배치·충돌은 기존 `InventorySize` 기준 유지
- 검증: Rogue10mEditor Win64 Development 빌드 성공
- 관련 문서:
  - `Feature/architect/2026-07-15_inventory-item-footprint.md`
  - `Feature/doc/2026-07-15_inventory-item-footprint.md`

### Sprint#1-14 - starter-item-icons

- 상태: 아이콘 생성, Data Asset/Texture Import, C++ 빌드, WBP 재구성 및 독립 commandlet 검증 완료. PIE 시각 QA 필요.
- 목표: 포션과 기본 장비를 실제 이미지로 제작해 M×N 인벤토리와 장비창에서 데이터 기반으로 표시한다.
- 주요 변경:
  - 다크 판타지 스타일 스타터 아이콘 6종 제작 및 투명 배경 처리
  - 1칸당 256px 기준으로 1×1, 1×3, 2×2, 2×3 원본 캔버스 구성
  - UI Texture 6개와 Item Data Asset 6개 생성
  - Data Asset 기반 `FRogue10mItemStack` 변환과 시작 아이템 배치
  - 포션 시작 수량 5개 및 장비 5종 프로토타입 자동 장착
  - `OnEquipmentChanged` Delegate와 장비창 슬롯 아이콘 7개 추가
  - 장비 슬롯에도 `ScaleBox(ScaleToFit)`와 `InventoryIconScale` 적용
- 검증:
  - PNG 투명 모서리와 M×N 해상도 검사 성공
  - Unreal Python Import 시 Texture/Data Asset 크기·참조 검증 성공
  - Rogue10mEditor Win64 Development 빌드 성공
  - UE5.8 commandlet 메뉴 WBP 전체 컴파일 성공, 0 errors / 0 warnings
  - 열린 Editor에서 Data Asset 6개, PrototypeStartingItems 9개, 장비 아이콘 7개와 누락 0개 확인
- 관련 문서:
  - `Feature/architect/2026-07-15_starter-item-icons.md`
  - `Feature/doc/2026-07-15_starter-item-icons.md`
  - `Feature/architect/2026-07-15_inventory-item-footprint.md`
  - `Feature/doc/2026-07-15_inventory-item-footprint.md`

### Sprint#1-14 - inventory-item-tint-layer-order

- 상태: 수정 및 Editor/commandlet 검증 완료
- 목표: InventoryTint Border가 실제 아이콘과 수량을 덮지 않도록 렌더 계층을 고정한다.
- 주요 변경:
  - `UI_InventoryItemPreviewBorder` GridSlot Layer 0
  - `UI_InventoryItemIconScale` GridSlot Layer 1
  - `UI_InventoryItemQuantityText` GridSlot Layer 2
  - `ValidateMenuWidgetAssets.py`에 Layer 회귀 검사 추가
- 검증:
  - 열린 Editor에서 Layer 0/1/2 확인 및 WBP 컴파일·저장 성공
  - UE5.8 commandlet 전체 메뉴 WBP 컴파일 성공, 0 errors / 0 warnings
- 관련 문서:
  - `Feature/doc/2026-07-15_starter-item-icons.md`
  - `DevLog/20260716.txt`

### Sprint#1-14 - inventory-item-icon-runtime-paint

- 상태: 구현, C++ 빌드, WBP 자동 검증, PIE 시각 검증 완료
- 목표: Texture가 연결됐지만 Tint만 보이던 NxM 인벤토리 아이콘을 런타임에 정상 표시한다.
- 주요 변경:
  - WBP_InventoryItem에서 Inventory Icon ScaleBox를 제거하고 Tint Border의 Image 콘텐츠로 직접 배치
  - Texture 종횡비와 NxM footprint를 이용해 44px 셀 기준 Fit 크기 계산
  - 생성 전에는 적용되지 않는 SetDesiredSizeOverride() 대신 FSlateBrush::SetImageSize()로 Brush에 크기 저장
  - 아이콘이 있는 일반 상태 Tint Alpha를 최대 0.18로 제한
  - 스타터 Texture Import를 TC_DEFAULT, TEXTUREGROUP_UI, TMGS_FROM_TEXTURE_GROUP으로 통일
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - ValidateMenuWidgetAssets.py 전체 통과
  - PIE UI 포함 캡처에서 포션, 장검, 투구, 갑옷, 부츠, 반지 아이콘 정상 표시
- 관련 문서:
  - Feature/architect/2026-07-15_starter-item-icons.md
  - Feature/doc/2026-07-15_starter-item-icons.md
  - Docs/GridInventoryAndMenuWindowsGuide.md
  - DevLog/20260716.txt

# Sprint#2-1 - 장비창 캐릭터 프리뷰 및 7부위 레이아웃

- 목표: 장비창에서 현재 플레이어 전신 외형과 7부위 장착 상태를 적절한 비율로 함께 확인한다.
- 주요 변경:
  - SceneCapture 전용 프리뷰 액터, transient 512×768 Render Target, Leader Pose 기반 메시 복제, 장비창 수명주기 연결
  - 장비 데이터와 UMG 바인딩을 무기·투구·갑옷·장갑·신발·반지·목걸이 7부위로 통일
  - 캐릭터 프리뷰 중심 CanvasPanel 직접 배치와 부위별 슬롯 크기 차등 적용
  - ScaleToFit 및 최대 1.0 아이콘 배율로 Texture 종횡비와 슬롯 경계 보존
  - WBP_EquipmentWindow 전용 재생성 및 CanvasSlot 좌표·크기 회귀 검사 추가
- 검증:
  - UE Editor 빌드 및 전체 메뉴 Widget Blueprint 컴파일 성공
  - 7개 슬롯 위치·크기 자동 검사 성공
  - PIE 장비 데이터 7개, 장비창 표시, 프리뷰 액터 1개 확인
  - 스타터 장비 5종 Texture Brush 연결 및 빈 슬롯 아이콘 숨김 확인
- 상태: 구현 및 런타임 검증 완료
- 관련 문서: `Feature/architect/2026-07-16_equipment-character-preview.md`, `Feature/doc/2026-07-16_equipment-character-preview.md`
# Sprint#2-1 - 인벤토리·장비창 동시 표시와 드래그 UI

- 목표: 인벤토리와 장비창을 동시에 표시하고 각 창을 타이틀 Drag로 이동하며, 장비창을 좌측 스탯·중앙 캐릭터 프리뷰·우측 장비 슬롯 구조로 정리한다.
- 주요 변경:
  - 인벤토리·장비창 전용 표시 그룹으로 두 창의 상호 배타 정책 제거
  - 두 전체화면 UserWidget·Canvas를 SelfHitTestInvisible로 설정해 실제 창 자식만 입력을 받고 투명 영역은 다른 창으로 입력 통과
  - 공통 `UI_WindowRoot`, `UI_WindowDragHandle`과 Canvas 이동·Viewport 경계 Clamp 추가
  - 인벤토리와 장비창에 44px 타이틀 Drag 영역 적용
  - 장비창을 980x620으로 확장하고 좌측 200x500 스탯, 중앙 340x500 프리뷰, 우측 350x500 7슬롯 Canvas 배치
  - 공격력·방어력·최대 체력·치명타 확률·공격 속도·이동 속도 Text 자리표시자 추가
  - 6개 스탯 Text를 C++ `BindWidgetOptional`로 노출해 향후 실제 수치 연결 준비
  - 슬롯을 `투구 | 목걸이`, `갑옷 | 장갑 | 무기`, `신발 | 반지` 3행으로 재정렬
  - 슬롯 크기를 갑옷 100x150, 무기 100x200, 나머지 5부위 100x100으로 통일
  - 각 장비 슬롯을 Frame > Canvas Layer > ScaleBox/Image + LocationText 계층으로 구성
  - Texture 원본 Brush 크기 복사를 끄고 긴 변 64px 정규화 및 사방 7px Stretch Anchor 적용
  - 무기·투구·갑옷·장갑·반지·신발·목걸이 위치 텍스트를 우측 상단 8px, Layer 1로 고정
  - 빈 장비 슬롯은 아이콘만 숨기고 위치 텍스트 유지
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 활성 Editor에서 두 WBP 재구성·컴파일·저장 성공
  - 7개 슬롯 계층·좌표·크기·폰트·우측 상단 Anchor·ZOrder 자동 검사 성공
  - PIE에서 인벤토리·장비창 동시 True, 양쪽 UserWidget·화면 Canvas 입력 투과와 실제 자식 Visible 확인
  - PIE에서 투구 64×64, 갑옷 42.67×64, 무기 21.33×64 정규화 Desired Size 확인
  - 새 UE5.8 commandlet 전체 메뉴 WBP 컴파일 성공, 0 errors / 0 warnings
  - PIE 마우스 상호작용은 Remote Execution 중단으로 수동 QA 필요
- 상태: 구현 및 정적·자산 검증 완료, PIE 수동 상호작용 QA 필요
- 관련 문서: `Feature/architect/2026-07-17_draggable-inventory-equipment-windows.md`, `Feature/doc/2026-07-17_draggable-inventory-equipment-windows.md`, `DevLog/20260717.txt`

# Sprint#2-1 - 장비 → 인벤토리 Drag & Drop

- 목표: 장비창의 장착 아이템을 Data Asset의 MxN 크기를 유지한 채 인벤토리의 원하는 빈 위치로 옮긴다.
- 주요 변경:
  - `Equipment` Item Drag Source와 장비 부위·Item Data·수량·클릭 셀 오프셋 Payload 추가
  - 장비창 7개 슬롯 Frame의 Drag 시작 입력 연결
  - Inventory Window DragOver/Drop의 Grid Item·Equipment Item 공통 처리
  - `TryUnequipItemToGrid()`의 위치 재검증, Grid Entry 생성, 장비 슬롯 초기화 원자적 처리
  - 성공 시 `OnInventoryGridChanged`, `OnEquipmentChanged` 동시 갱신
  - 주무기 해제 시 `Unarmed` 전환
  - 자동 장착 시작 장비의 Grid 중복 제거
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 전체 메뉴 Widget Blueprint compile/구조 검증 성공
  - commandlet 0 errors / 0 warnings
  - `git diff --check` 통과
  - 실제 마우스 Drop은 PIE 수동 QA 필요
- 상태: 구현·C++ 빌드·WBP 컴파일 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_equipment-to-inventory-drag-drop.md`, `Feature/doc/2026-07-18_equipment-to-inventory-drag-drop.md`, `DevLog/20260718.txt`

# Sprint#2-1 - 메뉴 창 호출 순서 ZOrder 스택

- 목표: 인벤토리와 장비창이 겹칠 때 가장 늦게 호출하거나 클릭한 창을 최상단에 표시
- 주요 변경:
  - `MenuWindowStack`과 `BringMenuWindowToFront()`로 메뉴 창 호출 순서 관리
  - 스택 순서에 따라 ZOrder `50 + Index`를 제한된 범위에서 재배치
  - 공통 Preview Mouse 좌클릭으로 가려진 창 승격
  - 직접 `SetWindowOpen(true)` 호출 경로도 동일하게 승격
  - 현재 최상단 열린 창을 Game and UI 입력 Focus 대상으로 선택
  - 표시 상태가 변한 창만 갱신해 인벤토리·장비창 동시 표시 순서 보존
  - Viewport에 없는 메뉴 창만 `AddToPlayerScreen()`으로 최초 등록
  - 이미 등록된 메뉴 창은 `UGameViewportSubsystem`의 기존 Widget Slot ZOrder만 갱신
  - 아이템 이동·창 클릭 시 메뉴 위젯 중복 화면 등록 경고 제거
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 전체 메뉴 Widget Blueprint commandlet 컴파일 성공
  - `ValidateMenuWidgetAssets.py` 0 errors / 0 warnings
  - 중복 등록 방지 수정 후 Rogue10mEditor Win64 Development 재빌드 성공
  - PIE 실제 겹침·클릭·Drag & Drop 수동 QA 필요
- 상태: 구현·C++ 빌드·WBP 정적 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_menu-window-z-order-stacking.md`, `Feature/doc/2026-07-18_menu-window-z-order-stacking.md`, `DevLog/20260718.txt`

# Sprint#2-2 - 인벤토리 아이템 Hover 툴팁

- 목표: 인벤토리 아이템 Hover 시 이름·아이템 정보·무게를 별도 UserWidget으로 표시
- 주요 변경:
  - `URogue10mInventoryItemTooltipWidget` C++ 부모와 `WBP_InventoryItemTooltip` 추가
  - `DisplayName`, `Description`, `UnitWeight` 기반 표시
  - 중첩 수량은 개당 무게와 총 무게를 함께 표시
  - Inventory Item의 `SetToolTip()` 연결 및 Drag Preview Tooltip 제거
  - `TSoftClassPtr` native 기본 경로로 활성 Editor의 기존 WBP 파일 잠금 회피
  - Menu Designer 빌더·좁은 Tooltip 생성 스크립트·전체 Validator 확장
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - WBP_InventoryItemTooltip 생성·저장 성공
  - 전체 메뉴 Widget Blueprint commandlet 0 errors / 0 warnings
  - 필수 Widget 3개 및 native Tooltip Soft Class 기본값 검증 성공
  - PIE 실제 Hover·화면 경계·Drag 전환 수동 QA 필요
- 상태: 구현·C++ 빌드·WBP 생성 및 정적 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_inventory-item-hover-tooltip.md`, `Feature/doc/2026-07-18_inventory-item-hover-tooltip.md`, `DevLog/20260718.txt`

# Sprint#2-3 - 인벤토리 아이템 우클릭 사용·장착

- 목표: 소비 아이템은 우클릭으로 사용하고 장비 아이템은 우클릭으로 즉시 장착·교체한다.
- 주요 변경:
  - Item Data Asset에 소비 효과용 `RestoreHealth` 설정 추가
  - 체력 회복이 실제 적용될 때만 소비 아이템 수량 1 감소
  - 빈 장비 부위 즉시 장착 및 동일 부위 기존 장비 교체
  - 기존 장비의 MxN Grid 공간을 변경 전에 확보하는 원자적 교체
  - 새 장비 원래 위치 우선, 이후 전체 컨테이너 빈 위치 검색
  - 공간 부족 시 인벤토리·장비 상태 변경 취소
  - 인벤토리·장비 Delegate 및 전투 로그 결과 갱신
  - 좌클릭 Drag & Drop과 Hover 툴팁 입력 경로 유지
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 전체 메뉴 Widget Blueprint commandlet 0 errors / 0 warnings
  - 스타터 Item Asset Python 스크립트 문법 검사 성공
  - `git diff --check` 통과
  - PIE 실제 우클릭 사용·장착·교체 수동 QA 필요
- 상태: 구현·C++ 빌드·WBP 정적 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_inventory-item-right-click-actions.md`, `Feature/doc/2026-07-18_inventory-item-right-click-actions.md`, `DevLog/20260718.txt`

# Sprint#2-4 - 장비 Hover 스탯 및 장착 장비 비교 Tooltip

- 목표: 인벤토리 장비 Hover 시 장착 증가량과 동일 부위 현재 장착 장비의 교체 차이를 함께 표시한다.
- 주요 변경:
  - `FRogue10mEquipmentStatModifiers` 6종 장비 증가량 데이터 추가
  - 부위별 현재 장착 Item Data 조회 API 추가
  - Hover 장비 아이콘·이름·설명·무게·장착 증가량 카드 구성
  - 동일 부위 장착 장비 아이콘·이름·설명·현재 증가량 오른쪽 카드 구성
  - `Hover - 현재 장착` 변화량의 증가·감소·동일 색상 비교
  - 한쪽에만 존재하는 스탯을 포함한 합집합 비교
  - 일반 300px·비교 620px Tooltip 동적 폭과 비교 패널 `Collapsed` 처리
  - 스타터 장비 5종 샘플 스탯 Data Asset 저장
  - Tooltip 계층·폭·가시성·장비 스탯 값 Validator 확장
  - 등급별 불투명 아이템 이름 색상 API 및 Tooltip 적용
  - Hover 대상·현재 장착 비교 대상·장착 해제 메뉴 이름 색상 통일
  - `장착 시 증가` 제목 런타임 및 레이아웃 생성 단계에서 숨김
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - WBP_InventoryItemTooltip 재구성·컴파일·저장 성공
  - 전체 메뉴 Widget Blueprint commandlet 0 errors / 0 warnings
  - 스타터 장비 5종 스탯 값 검증 성공
  - Python 문법 및 `git diff --check` 통과
  - 등급 이름 색상 보완 후 Editor UHT/C++ 빌드 및 전체 메뉴 commandlet 재검증 성공
- 상태: 구현·C++ 빌드·WBP/Data Asset 정적 검증 완료, PIE Hover 비교 수동 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_equipment-tooltip-stat-comparison.md`, `Feature/doc/2026-07-18_equipment-tooltip-stat-comparison.md`, `DevLog/20260718.txt`
- 보완 문서: `Feature/architect/2026-07-18_equipment-tooltip-rarity-name.md`, `Feature/doc/2026-07-18_equipment-tooltip-rarity-name.md`

# Sprint#2-5 - ???? ??? ?? ??

- ??: ???? ???? MxN ?? ?? ??????????????? ??? ?? ??? ????.
- ?? ??:
  - ERogue10mItemRarity? ??? 5??? ?? ????? ??
  - Item Data Asset ??? Inventory Background Color ?? ?? ??
  - WBP_InventoryItem? MxN ??? ??? UI_InventoryItemRarityBackground Border ??
  - ?? ?? 0????/Drag Preview 1??? 2 ??? ??
  - ??? Preview Tint? ?? ???? ?? ??? ?? ??
  - Tooltip Soft Class? Widget Blueprint ???? ????? ??
  - ??? 6?? ???????????? ?? ?? ??
  - ?? WBP/Data Asset ?? ????? 5????????? Validator ??
- ??:
  - Rogue10mEditor Win64 Development ?? ??
  - ?? Unreal Editor?? WBP_InventoryItem ?????????? ??
  - ?? ?? Widget Blueprint ?? ??
  - 5?? RGBA ??? ?? ?? ??
  - ??? 6? ?? ?? ?? ??
  - Unreal Editor Python ?? ?? ??
- ??: ???C++ ???WBP/Data Asset ?? ?? ??, PIE ?? ? ???? ?? QA ??
- ?? ??: Feature/architect/2026-07-18_inventory-item-rarity-backgrounds.md, Feature/doc/2026-07-18_inventory-item-rarity-backgrounds.md, DevLog/20260718.txt

# Sprint#2-6 - 장착 장비 Hover 및 장착 해제 메뉴

- 목표: 장비창의 장착 아이템에 Hover 상세 정보와 우클릭 장착 해제 UserWidget을 제공한다.
- 주요 변경:
  - 7개 장비 슬롯 Frame에 기존 장비 상세 Tooltip 연결
  - 이름·설명·무게·6종 장착 증가량 표시
  - 빈 슬롯 Tooltip 제거 및 장비 변경 시 이벤트 기반 갱신
  - URogue10mEquipmentSlotActionWidget과 WBP_EquipmentSlotAction 추가
  - 아이템명·장착 해제 버튼·공간 부족 결과 문구 구성
  - 우클릭 커서 위치와 Viewport 경계 보정
  - 장비창 닫기·장비 변경·좌클릭·성공 시 메뉴 자동 정리
  - 모든 Inventory Container의 MxN 첫 빈 공간 장착 해제 API 추가
  - 공간 부족 시 장비·인벤토리 상태 원상 유지
  - 기존 장비 좌클릭 Drag & Drop 경로 유지
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - WBP_EquipmentSlotAction 생성·컴파일·저장 성공
  - Action 결과 기본 Collapsed 및 필수 계층 검증 성공
  - 장비창 Tooltip/Action Soft Class Reference 검증 성공
  - 첫 빈 Grid 장착 해제 API 노출 검증 성공
  - 새 Unreal commandlet 프로세스 0 errors / 0 warnings
  - Editor Python AST와 git diff --check 통과
- 상태: 구현·C++ 빌드·WBP/API 정적 검증 완료, PIE Hover·우클릭·공간 부족 수동 QA 대기
- 관련 문서: Feature/architect/2026-07-18_equipped-item-hover-unequip-menu.md, Feature/doc/2026-07-18_equipped-item-hover-unequip-menu.md, DevLog/20260718.txt

# Sprint#2-7 - 인벤토리·장비 UI 비율 및 Drag & Drop 보정

- 목표: MxN 아이콘 비율과 Tooltip 크기를 정규화하고 인벤토리→장비 슬롯 Drop 장착 및 커서 중심 장비 Payload를 제공한다.
- 주요 변경:
  - 인벤토리 아이콘 footprint 84% Fit 및 원본 종횡비 유지
  - Texture Match Size 비활성화와 스케일 0.75~1.0 제한
  - 스타터 아이템 6종 InventoryIconScale 1.0 정규화
  - Tooltip 기본 280px·비교 580px·아이콘 52×52px
  - 장비·소비·장착 상태별 우클릭 동작 안내
  - 장비창 GridInventory DragOver·Drop·DragLeave 경로 추가
  - 부위 일치 검사와 호환·비호환 슬롯 Preview
  - 기존 TryEquipGridItem 기반 원자적 장착·교체
  - 장비 Drag Visual CenterCenter Pivot 적용
  - UE 5.8 UMG의 0.15초 Drag Decorator 보간과 전체 화면 Drag Source Geometry를 좌측 상단 비행 원인으로 특정
  - 장비 슬롯 Canvas의 MouseDown Hit-Test 경로에 투명 Drag Source 프록시 7개 사전 배치
  - MouseDown 시 동일 프록시를 Payload MxN 크기의 커서 중심 Layout으로 이동
  - Drop·Drag Cancel·클릭 종료·창 닫기 홈 Layout 복원 및 장비 갱신·Destruct 제거 수명주기 연결
  - 기존 CenterCenter Pivot과 MxN Payload 크기 유지
  - Drop·이탈·창·장비 수명주기 Preview 정리
  - Tooltip WBP 빌더와 전체 메뉴 Validator 확장
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 장비 Drag Source 프록시 추가 후 UE 5.8 UHT 통과
  - 수정된 Rogue10mEditor Win64 Development 재빌드 성공
  - 새 UnrealEditor-Cmd 프로세스에서 전체 메뉴 Widget Validator 통과
  - git diff --check 및 CheckGeneratedChanges 통과
  - WBP_InventoryItemTooltip 재생성·컴파일·저장 성공
  - 전체 메뉴 Widget Blueprint 명령형 검증 성공
  - Tooltip 폭·안내 문구 기본 가시성 검증 성공
  - 스타터 아이템 6종 InventoryIconScale 1.0 검증 성공
  - Python 문법 및 git diff --check 통과
- 상태: 구현·빌드·WBP/Data Asset 정적 검증 완료, 에디터 재시작 후 PIE 수동 상호작용 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-18_inventory-equipment-ui-polish.md`
  - `Feature/doc/2026-07-18_inventory-equipment-ui-polish.md`
  - `DevLog/20260718.txt`

## 2026-07-22 보완 - 장비 장착 해제 컨텍스트 메뉴

- 목표: 장착 장비 우클릭 시 Tooltip 중첩을 제거하고 메뉴 위치 및 외부 클릭 닫기 동작을 개선한다.
- 주요 변경:
  - 활성 Hover Tooltip 즉시 닫기 및 메뉴 종료 시 슬롯 Tooltip 복원
  - 커서 오른쪽 12px 메뉴 배치와 Viewport 경계 보정
  - 전체 화면 투명 dismiss 버튼으로 메뉴 외부 첫 클릭 소비 및 닫기
  - Action WBP 생성 스크립트와 Validator 필수 바인딩 갱신
- 검증: Rogue10mEditor 빌드 성공, 전체 메뉴 Widget Validator 0 errors / 0 warnings, Python 문법 및 diff 검사 통과
- 상태: 구현·빌드·WBP 정적 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-22_equipment-context-menu-dismiss.md`, `Feature/doc/2026-07-22_equipment-context-menu-dismiss.md`, `DevLog/20260722.txt`

## 2026-07-23 보완 - 메뉴 Python wrapper 정리

- 목표: 메뉴 Widget 자동화의 중복 진입점을 제거하고 통합 빌더·Validator 중심으로 유지보수 경로를 단순화한다.
- 주요 변경:
  - `BuildMenuDesignerLayouts.py`로 대체된 부분 생성·재생성 wrapper 6개 삭제
  - 메뉴 생성은 `BuildMenuDesignerLayouts.py`, 회귀 검증은 `ValidateMenuWidgetAssets.py`로 통합
  - Data Asset·콘텐츠 이동·경고 검증·원격 실행용 재사용 스크립트는 유지
- 검증: 남은 Python 전체 문법 검사, 삭제 파일 참조 검색, `git diff --check`, `CheckGeneratedChanges.ps1`
- 상태: 스크립트 정리 및 정적 검증 완료
- 관련 문서: `DevLog/20260723.txt`

## 2026-07-23 보완 - HUD Quick Slot USTRUCT 초기화

- 목표: Editor Automation에서 반복된 `FRogue10mHudQuickSlotView::InputSlot` 미초기화 오류를 제거한다.
- 주요 변경: `InputSlot` 기본값을 `ERogue10mAttackInputSlot::Primary`로 지정
- 원인 분리:
  - `r.MotionVectorSimulation`: UE 5.8 엔진 CVar 플래그 문제로 프로젝트 변경 없음
  - 단독 `Condition failed` 15건: UE 5.8 엔진 테스트 로그로 프로젝트 오류와 무관
- 검증: Rogue10mEditor 빌드 성공, `UObject.Class AttemptToFindUninitializedScriptStructMembers` Automation 테스트 성공, 관련 LogClass 오류 0건
- 상태: 수정 및 자동 검증 완료
- 관련 문서: `DevLog/20260723.txt`

## 2026-07-23 보완 - 시작 인벤토리 추가 갑옷

- 목표: 시작 장비 자동 장착을 유지하면서 인벤토리에 가죽 갑옷 한 개를 추가로 제공한다.
- 주요 변경: `DA_Item_LeatherArmor` 시작 참조 추가, 동일 장비 부위의 후속 자동 장착 방지
- 검증: Rogue10mEditor 빌드 성공, `CheckGeneratedChanges.ps1` 및 `git diff --check` 통과
- 상태: 구현 및 자동 검증 완료, 새 PIE 세션 수동 확인 대기
- 관련 문서: `Feature/architect/2026-07-23_starter-inventory-extra-armor.md`, `Feature/doc/2026-07-23_starter-inventory-extra-armor.md`, `DevLog/20260723.txt`

## 2026-07-23 보완 - 에픽 수호자 투구

- 목표: 에픽 등급 Head 장비 Data Asset을 제작하고 시작 인벤토리에 추가한다.
- 주요 변경: `DA_Item_GuardianHelmet` 생성, 방어력 +10·최대 체력 +20 적용, 시작 아이템 참조 및 자동 검증 목록 추가
- 에셋 정책: 기존 철제 투구 아이콘 재사용, 불필요한 기존 스타터 에셋 재저장 변경 제거
- 검증: Rogue10mEditor 빌드 성공, UnrealEditor-Cmd 생성 검증 및 전체 메뉴·아이템 Validator 통과, 생성 파일·diff 검사 통과
- 상태: 구현 및 자동 검증 완료, 새 PIE 세션 수동 확인 대기
- 관련 문서: `Feature/architect/2026-07-23_epic-guardian-helmet.md`, `Feature/doc/2026-07-23_epic-guardian-helmet.md`, `DevLog/20260723.txt`

# Sprint#2-8 - 캐릭터 기본 스탯 및 장비창 표시

- 목표: 캐릭터 기본 스탯과 장비 보너스의 소유 구조를 확립하고 최종값을 전투·생존·이동 및 장비창에 일관되게 반영한다.
- 주요 변경:
  - Character Data Asset에 체력·자원·공격력·방어력·치명타·공격 속도·이동 기본값 정의
  - 기본·장비·최종값을 공유하는 Character Stat 구조체 추가
  - 장착 장비 전체의 6종 보너스 합산 API 추가
  - 장비 변경 성공 경로를 단일 스탯 갱신 진입점으로 통합
  - 공격력의 스킬 피해 반영과 방어력의 정액 피해 감소 적용
  - 최대 체력 변경 시 현재 체력 비율 보존
  - 이동 속도와 질주 속도를 CharacterMovement에 반영
  - 장비창 6종 스탯을 `최종 (기본 + 장비)` 형식으로 표시
  - 기본 Character Data 전용 설정 모드와 Validator 추가
- 검증:
  - Rogue10mEditor Win64 Development 전체 빌드 성공
  - 리뷰 보완 후 Rogue10mCharacter 증분 재빌드 성공
  - UnrealEditor-Cmd에서 기본 스탯 10종과 장비창 바인딩 6종 검증 성공
  - 기존 스타터 장비 6종 능력치 회귀 검증 성공
  - `UObject.Class AttemptToFindUninitializedScriptStructMembers` Automation 테스트 성공
  - Python 문법, `CheckGeneratedChanges.ps1`, `git diff --check` 검사
- 상태: 구현·빌드·에셋 정적 검증 완료, 새 PIE 세션 장착·해제·피해·이동 수동 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-23_character-base-stats-equipment-window.md`
  - `Feature/doc/2026-07-23_character-base-stats-equipment-window.md`
  - `Docs/CharacterDataOwnership.md`
  - `DevLog/20260723.txt`

## 2026-07-25 보완 - 장비창 Scene Capture 중복 갱신 제거

- 목표: 실시간 캐릭터 프리뷰는 유지하면서 자동·수동 Scene Capture의 중복 렌더 경고를 제거한다.
- 원인: `bCaptureEveryFrame=true` 상태에서 `SetPreviewActive()`와 장비 변경 경로가 수동 `CaptureScene()`도 호출
- 주요 변경: 활성 프리뷰는 자동 캡처만 사용하고 수동 캡처는 `bCaptureEveryFrame=false`일 때만 허용
- 검증: Rogue10mEditor Win64 Development 빌드 성공, 수동 캡처 보호 조건, 생성물·diff 검사
- 상태: 코드 및 빌드 검증 완료, 에디터 재시작 후 PIE 경고 재발 여부 수동 확인 대기
- 관련 문서: `Feature/doc/2026-07-16_equipment-character-preview.md`, `DevLog/20260725.txt`

## 2026-07-25 보완 - 장비창 Preview Character 좌클릭 회전

- 목표: 장비창 프리뷰에서 좌클릭 가로 드래그로 캐릭터를 좌우 회전한다.
- 주요 변경: 메시 전용 Pivot, 프리뷰 영역 판정, 마우스 캡처 기반 드래그, 0.35°/px 감도 설정, 캡처 손실 정리
- 입력 호환: 타이틀 바 창 이동, 장비 슬롯 Drag & Drop, 우클릭 장착 해제 경로 유지
- 렌더 호환: 실시간 Scene Capture 유지, 카메라·조명 고정, 자동·수동 중복 캡처 보호 유지
- 검증: UE 5.8 UHT 및 Rogue10mEditor 빌드 성공, 생성물·diff 검사
- 상태: 구현·빌드·정적 검증 완료, 에디터 재시작 후 PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-25_equipment-preview-mouse-rotation.md`, `Feature/doc/2026-07-25_equipment-preview-mouse-rotation.md`, `DevLog/20260725.txt`

# Sprint#2-9 - 몬스터 경험치 보상 및 25종 로스터

- 목표: 몬스터 처치 경험치를 플레이어 성장에 연결하고 Data Asset 기반 25종 로스터를 준비한다.
- 주요 변경: 서버 권한 마지막 공격자 경험치 지급, 중복 지급 방지, 전투 로그, MonsterRank 및 공격 Fallback 수치 추가
- 콘텐츠: 일반 20종, 중간 보스 4종, 최종 보스 1종 Data Asset 생성
- 밸런스: 일반 경험치 18~140, 중간 보스 300~900, 최종 보스 3,000
- 검증: UE 5.8 UHT 및 Rogue10mEditor 빌드 성공, Unreal Python Validator 25종·20/4/1 통과, 생성물·diff 검사
- 상태: 구현·에셋 생성·빌드·정적 검증 완료, 에디터 재시작 후 PIE 처치 보상 수동 QA 대기
- 관련 문서: `Feature/architect/2026-07-25_monster-experience-roster.md`, `Feature/doc/2026-07-25_monster-experience-roster.md`, `DevLog/20260725.txt`

# Sprint#2-10 - 캐릭터 커스터마이징·생성·접속

- 목표: 게임 시작 시 3슬롯에서 캐릭터를 생성·선택하고 해당 외형으로 접속하는 로비 흐름을 제공한다.
- 주요 변경: 인간·드워프·오크 남녀 6개 아키타입 카탈로그, 이름·외형 SaveGame, 생성·선택·삭제·접속 UI, 좌클릭 회전 프리뷰 추가
- 런타임 적용: 선택 프로필을 플레이어 메시와 PlayerState 표시에 적용하고 기존 1인칭 팔·전투·장비창 프리뷰 흐름 유지
- 저장 경계: 이름·외형·선택 슬롯만 저장하며 인벤토리·경험치·스탯·월드 진행도는 후속 캐릭터 저장 기능으로 분리
- 검증: UE 5.8 UHT 및 Rogue10mEditor 빌드 성공, Unreal Python Validator에서 6개 아키타입·필수 에셋·위젯 바인딩 통과, 생성물·diff 검사
- 상태: 구현·에셋 생성·빌드·정적 검증 완료, 에디터 재시작 후 PIE 생성·저장·접속·애니메이션 수동 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-25_character-customization-lobby.md`
  - `Feature/doc/2026-07-25_character-customization-lobby.md`
  - `Docs/CharacterDataOwnership.md`
  - `DevLog/20260725.txt`

## 2026-07-26 보완 - 캐릭터 선택 후 접속 실패

- 증상: 슬롯 선택 후 `게임 접속`을 눌러도 로비가 닫히지 않고 외형 적용 실패 로그가 반복됨
- 원인: `BP_FirstPersonCharacter` CDO의 `CustomizationCatalog` 참조 누락
- 주요 변경: Character Blueprint 카탈로그 기본값 저장, C++ 기본 경로 fallback, 로비 포커스 활성화
- 회귀 방지: 에셋 생성 스크립트에서 Character CDO 참조 설정, Validator에서 메시 컴포넌트와 카탈로그 CDO 검사
- 검증: 수정 전 Validator 오류 재현, 수정 후 6개 아키타입·로비·Player Character CDO 통과, UE 5.8 Editor 빌드 성공
- 상태: 원인 수정·에셋 저장·빌드·정적 검증 완료, 에디터 재시작 후 기존 슬롯 접속 PIE QA 대기
- 관련 문서: `Feature/doc/2026-07-25_character-customization-lobby.md`, `DevLog/20260726.txt`

## 2026-07-26 보완 - 상속 기반 캐릭터 외형 및 종족별 리타기팅

- 목표: 캐릭터 생성 후 입장 시 서로 다른 Skeleton의 Leader Pose 연결로 발생하는 머리·몸·팔 왜곡을 구조적으로 제거한다.
- 주요 변경:
  - `ARogue10mStylizedCharacter` 공통 외형 부모와 Human·Dwarf·Orc 남녀 6개 자식 Character Blueprint 추가
  - 숨김 Manny `AnimationSourceMesh`와 종족 전신 `Character Mesh`를 부모·자식으로 구성
  - 종족별 프로젝트 로컬 Target IK Rig, IK Retargeter, `Retarget Pose From Mesh` AnimBP 추가
  - 카탈로그 Archetype에 CharacterClass·Retargeter·RetargetAnimClass 연결
  - GameMode 선택 클래스 결정 및 실패 안전 Spawn/Possess 교체 흐름 추가
  - 서로 다른 Skeleton Leader Pose 제거, 동일 Skeleton Hair·Facial 파츠에만 Leader Pose 적용
  - 원본 Orc Male IK Rig의 무효 `Cape` 체인은 프로젝트 로컬 복제본에서 제거
- 검증:
  - Rogue10mEditor Win64 Development 전체 빌드 성공
  - 상속/IK/AnimGraph Validator 오류 0건, 경고 0건
  - 기존 캐릭터 로비/카탈로그 Validator 오류 0건, 경고 0건
  - `CheckGeneratedChanges.ps1` 통과
- 상태: 구현·에셋 생성·정적 검증 완료, 에디터 재시작 후 6조합 이동·공격·장비창 Preview 수동 PIE QA 필요
- 관련 문서:
  - `Feature/architect/2026-07-26_inherited-character-appearance.md`
  - `Feature/doc/2026-07-26_inherited-character-appearance.md`
  - `Docs/CharacterDataOwnership.md`
  - `DevLog/20260726.txt`
## 2026-07-26 보완 - 상속 CharacterClass 접속 실패 수정

- 목표: 선택 프로필의 자식 CharacterClass를 찾지 못해 게임 접속이 중단되는 문제 해결
- 주요 변경: 공통 외형 부모 BeginPlay 조기 적용 제거, 카탈로그 고정 경로 fallback, 종족·성별 자식 Blueprint class fallback 추가
- 검증: Editor 빌드 성공, 상속 Validator 통과, 실제 PIE에서 Human Male 자식 Pawn Spawn/Possess 및 EnterSelectedCharacter 후 로비 종료 확인, 최근 오류 로그 0건
- 상태: 수정 및 자동 PIE 검증 완료
- 관련 문서: `Feature/doc/2026-07-26_inherited-character-appearance.md`, `DevLog/20260726.txt`

# Sprint#3-2 - 몬스터 영역 스포너

- 브랜치: `Sprint#3-2-monster-area-spawner`
- 목표: 지정한 Box 범위 안에서 특정 몬스터를 최대 N마리까지 생성하고 선택적으로 생존 수를 유지한다.
- 주요 변경:
  - `ARogue10mMonsterSpawner` World Actor 추가
  - Monster Class, Box 범위, 최대 생존 수, 생성 간격 에디터 설정
  - 시작 즉시 최대 수 충원과 지속 보충·일회성 생성 선택
  - 지면 트레이스와 충돌 안전 생성
  - Tick 없는 타이머, 약한 참조, 파괴 Delegate 기반 생존 수 추적
  - Authority 전용 생성과 Blueprint 수동 제어 API
- 검증:
  - UHT 통과
  - `Rogue10mMonsterSpawner.cpp` UBT 단일 파일 컴파일 성공
  - UI Preview Actor의 Unity 상수 재정의를 기능별 접두사로 수정
  - `Rogue10mEditor` Win64 Development 전체 빌드 성공
  - `CheckGeneratedChanges.ps1`, `git diff --check`
- 상태: C++ 구현 및 전체 Editor 빌드 검증 완료, 레벨 배치 PIE QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-28_monster-area-spawner.md`
  - `Feature/doc/2026-07-28_monster-area-spawner.md`
  - `DevLog/20260728.txt`

## Sprint#3-3 - 모험가 기본 직업과 스폰 후 맨손 공격

- 목표: 캐릭터 선택 후 스폰·Possess된 Pawn에 기본 직업 모험가와 좌클릭 맨손 Primary 공격을 안정적으로 부여한다.
- 주요 변경: CharacterData 직업 필드, PlayerState 모험가 기본값, PossessedBy/OnRep 기반 로드아웃 재초기화, 종족명과 직업명 분리
- 에셋: `DA_Character_Default`를 Unarmed 기본 무기로 변경하고 Unarmed Profile·Primary 주먹 공격·BP CombatComponent 참조를 검증 및 저장
- 빌드 보완: UE Unity 빌드의 Preview Actor 익명 namespace 상수 재정의 충돌 제거
- 검증: UE 5.8 Rogue10mEditor 전체 빌드 성공, 모험가/Unarmed Editor Python Validator 통과, 6개 상속 Character 및 Retarget AnimBP Validator 통과
- 상태: 구현·에셋 설정·빌드·정적 검증 완료, 에디터 재시작 후 실제 좌클릭 PIE 체감 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-28_adventurer-unarmed-spawn.md`
  - `Feature/doc/2026-07-28_adventurer-unarmed-spawn.md`
  - `Docs/CharacterDataOwnership.md`
  - `DevLog/20260728.txt`
## Sprint#3-4 - 3인 캐릭터 선택 무대

- 브랜치: `Sprint#3-4-character-selection-stage`
- 목표: 기존 캐릭터 로비 기능을 유지하면서 저장된 최대 3명의 캐릭터를 판타지 배경 앞에 동시에 표시한다.
- 주요 변경:
  - CharacterLobbyWidget의 Draft Preview 1개와 Slot Preview 3개 수명주기
  - 숨김 Manny Idle Source Mesh와 6개 종족·성별 Retarget AnimBP 적용
  - 선택 캐릭터 명도·슬롯 카드 강조와 각 프리뷰 좌클릭 회전
  - 오리지널 16:9 판타지 성채 배경과 1920×1080 WBP 무대형 레이아웃
  - 기존 생성·삭제·선택·게임 접속·외형 편집 BindWidget 유지
  - 빈 슬롯 캡처 비활성화와 로비 종료 시 Preview Actor 정리
- 검증:
  - UE 5.8 UHT 및 Rogue10mEditor Win64 Development 전체 빌드 성공
  - 기존 캐릭터 커스터마이징 Validator 통과
  - 신규 3인 선택 무대 Validator 통과
  - 최종 WBP 재로드 시 Blueprint/Python 오류 없음
- 상태: 구현·배경·WBP·빌드·에셋 정적 검증 완료, 에디터 PIE 육안 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-28_character-selection-stage.md`
  - `Feature/doc/2026-07-28_character-selection-stage.md`
  - `DevLog/20260728.txt`

### Sprint#3-4 보완 - Main Menu / Lobby / Component 경로 분리

- 목표: 캐릭터 로비 비율·상호작용을 보완하고 기본 메인 메뉴에서 로비와 인게임으로 이어지는 화면 흐름 구성
- 주요 변경:
  - `Content/Widget/Lobby/WBP_CharacterLobby`로 로비 이동
  - 기존 공용 메뉴 위젯을 `Content/Widget/Component`로 이동
  - `Content/Widget/Menu/WBP_MainMenu` 신규 생성 및 게임 시작·종료 연결
  - Slot Preview 폭 360px 제한, `UI_SelectedCharacterInfoText` 제거, 하단 선택/생성 버튼 배치
  - 캐릭터/슬롯 정보 더블클릭 접속과 빈 슬롯 캐릭터 생성 지원
  - PlayerController 흐름을 `Main Menu → Lobby → 현재 Start Map`으로 변경
- 검증: UE 5.8 Rogue10mEditor 빌드 성공, Widget Flow/Selection Stage/Customization/Inherited Character 검증 통과
- 상태: 구현·에셋 이동·자동 검증 완료, PIE 수동 체감 QA 대기
- 후속: `Start Map → World Partition Cell 오픈월드` 설계 및 구현
- 관련 문서:
  - `Feature/architect/2026-07-28_character-selection-stage.md`
  - `Feature/doc/2026-07-28_character-selection-stage.md`
  - `Docs/CurrentProjectArchitecture.md`
  - `DevLog/20260728.txt`
## Sprint#3-5 - 몬스터 Behavior Tree 전투 AI

- 브랜치: `Sprint#3-5-monster-behavior-tree-ai`
- 목표: 몬스터 이동을 홈·순찰·추격 범위로 제한하고 거리 감지 또는 피격 조건에서만 플레이어와 전투
- 주요 변경:
  - Tick 추적 제거, AIController + AI Perception + Blackboard + Behavior Tree + NavigationSystem 적용
  - Sight/피격 어그로, 기억 시간, 최대 추격 거리, 홈 복귀, 순찰 도착 대기 구현
  - 공통 `BB_Monster`, `BT_Monster`와 런타임 안전 대체 트리 추가
  - 25개 Monster Data Asset에 BT·거리 설정 연결
  - BeginPlay 데이터 적용 후 AI 재초기화로 UE Possess 순서 대응
  - 로비 `UI_StatusText` 제거, 3개 프리뷰 정면 배치, 16:9 배경 채움
- 검증:
  - UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
  - Monster Behavior Tree AI Validator 통과(Blackboard 8키, 2 Task, 25 Data Asset)
  - Character Selection Stage Validator 통과
- 상태: 코드·에셋·자동 검증 완료, NavMesh 레벨에서 순찰·피격 어그로·추격 해제 수동 PIE QA 필요
- 관련 문서:
  - `Feature/architect/2026-07-28_monster-behavior-tree-ai.md`
  - `Feature/doc/2026-07-28_monster-behavior-tree-ai.md`
  - `Docs/CurrentProjectArchitecture.md`
  - `DevLog/20260728.txt`
### Sprint#3-5 보완 - Main Menu UIOnly 포커스 오류

- 문제: `FInputModeUIOnly`가 포커스 불가능한 Main Menu `SObjectWidget`을 대상으로 지정해 PlayerController 오류 로그 발생
- 수정: `URogue10mMainMenuWidget::NativeConstruct()`에서 Main Menu 루트를 포커스 가능하게 설정
- 검증: UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
- 상태: 코드·빌드 검증 완료, PIE에서 오류 로그 미발생 확인 필요
- 관련 기록: `DevLog/20260729.txt`
### Sprint#3-5 보완 - 캐릭터 선택 슬롯 및 프리뷰 복구

- 문제: BottomBar가 버튼 입력을 차단하고, 일시정지 중 SceneCapture 갱신 실패로 Slot Preview가 흰색으로 표시됨
- 수정: 버튼 ZOrder 8, 장식/프리뷰 Hit Test 비활성, 조명·메시 활성화 후 수동 Capture, Pause 중 메시·Capture 갱신
- UI: Lobby Background Image 유지, 전체 화면 Stretch 앵커와 0 오프셋 적용
- 에디터 적용: 공식 Unreal Python Remote Execution으로 현재 PIE 종료 후 WBP 재구성
- 검증: UE 5.8 Editor 빌드 성공, 강화된 Character Selection Stage Validator 통과
- 상태: 코드·WBP·자동 검증 완료, 에디터 재시작 후 PIE에서 실제 프리뷰와 클릭 체감 확인 필요
- 관련 문서: `Feature/doc/2026-07-28_character-selection-stage.md`, `DevLog/20260729.txt`
### Sprint#3-5 보완 - 캐릭터 프리뷰 소스 로드 및 WASD 입력 복구

- 문제: Manny 프리뷰 메시의 불완전한 Soft Object Path로 프리뷰 초기화 실패
- 문제: Main Menu와 Lobby에서 이동·시점 입력 차단이 중복 누적되어 접속 후에도 WASD 비활성
- 수정: 메시 경로를 `.SKM_Manny_Simple`까지 포함한 전체 오브젝트 경로로 변경
- 수정: UI 진입 시 입력 차단 중복 방지, 접속 시 `ResetIgnoreMoveInput`/`ResetIgnoreLookInput` 적용
- 진단: 접속 직후 입력 차단 및 일시정지 상태 로그 추가
- 검증: UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
- 상태: 코드·빌드 검증 완료, 에디터 재시작 후 PIE 프리뷰/WASD 체감 QA 필요
- 관련 문서: `Feature/doc/2026-07-28_character-selection-stage.md`, `DevLog/20260729.txt`
## Sprint#3-6 - Menu Map 기반 UI와 인게임 전환

- 브랜치: `Sprint#3-6-menu-map-flow`
- 목표: 메뉴·캐릭터 선택을 별도 맵으로 분리하고 선택 캐릭터로 인게임 맵에 진입
- 주요 변경:
  - `/Game/Rogue10m/Maps/L_Menu` 신규 생성
  - `Rogue10mMenuGameMode`, `Rogue10mMenuPlayerController` 추가
  - PlayerController의 Menu World / Gameplay World 초기화 분리
  - 선택 프로필 저장 후 `Lvl_FirstPerson?StartRun=1` OpenLevel
  - `EditorStartupMap`, `GameDefaultMap`을 `L_Menu`로 변경
  - Main Menu 전체 화면 Stretch 및 제목·버튼 확대
- 검증:
  - UE 5.8 Editor 빌드 성공
  - Menu Map Flow Validator 통과
  - Widget Flow Validator 통과
  - 실제 PIE에서 `L_Menu → Lobby → Lvl_FirstPerson` 이동 성공
  - 선택 Pawn Spawn/Possess, 이동·시점 입력 및 Pause 해제 확인
- 상태: 구현·맵 자산·설정·자동 PIE 검증 완료
- 관련 문서:
  - `Feature/architect/2026-07-29_menu-map-flow.md`
  - `Feature/doc/2026-07-29_menu-map-flow.md`
  - `DevLog/20260729.txt`

## Sprint#3-6 - 로비 캐릭터 프리뷰 정면·동일 크기·투명 배경

- 목표: 캐릭터 선택 로비에서 세 캐릭터를 동일한 크기의 정면 고정 프리뷰로 표시하고 배경 위에 캐릭터만 합성
- 주요 변경:
  - 프리뷰 Yaw -90도 고정
  - 세 Slot Preview Y=15, 360×720 통일
  - SceneColor HDR/Inverse Opacity + UI Translucent 합성
  - M_CharacterPreviewTransparent 추가
  - Lobby 배경 Shade 및 Slot Stage Glow 제거
  - Lobby 드래그 회전 제거, Equipment 회전 유지
- 검증:
  - UE 5.8 Rogue10mEditor 빌드 성공
  - Character Selection Stage Validator 오류 0건
  - Character Lobby Flow Suite 오류 0건
- 상태: 구현 및 자동 검증 완료
- 관련 문서:
  - Feature/architect/2026-07-28_character-selection-stage.md
  - Feature/doc/2026-07-28_character-selection-stage.md

- 추가 런타임 검증: Menu Map PIE에서 Lobby 표시 및 프리뷰 액터 4개 생성, 관련 로드 오류 0건

## Sprint#4-1 - 로비 프리뷰-슬롯 선택 동기화

- 브랜치: `Sprint#4-1-lobby-preview-slot-selection`
- 목표: 로비의 캐릭터 프리뷰 클릭과 하단 캐릭터 슬롯 선택 상태를 일치시킨다.
- 주요 변경:
  - 프리뷰 단일 좌클릭 입력 처리 추가
  - 프리뷰 화면 좌표를 슬롯 인덱스로 판정
  - 프리뷰와 하단 슬롯 버튼이 기존 `SelectSlot()` 경로 공유
  - 프리뷰 더블클릭 즉시 접속 기능 유지
  - 생성 화면에서는 슬롯 선택 입력 제외
- 검증:
  - UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
  - `Scripts/CheckGeneratedChanges.ps1` 통과
  - `git diff --check` 통과
- 상태: 코드 및 자동 검증 완료, Menu Map PIE 수동 클릭 QA 대기
- 관련 문서:
  - `Feature/architect/2026-08-04_lobby-preview-slot-selection.md`
  - `Feature/doc/2026-08-04_lobby-preview-slot-selection.md`
  - `DevLog/20260804.txt`

## Sprint#4-2 - 고딕 전투 HUD 비주얼 개편

- 브랜치: `Sprint#4-2-gothic-combat-hud`
- 목표: 기존 HUD 배치를 유지하면서 레퍼런스와 유사한 어두운 고딕 액션 RPG 스타일을 적용한다.
- 주요 변경:
  - 흑철·청동·붉은 룬 기반의 독창적인 하단 HUD 프레임 이미지 생성
  - 투명 원본에서 좌우 바, 중앙 메달, 경험치, 슬롯 프레임 텍스처 파생
  - UI용 Texture2D 5개 임포트 및 `WBP_Rogue10mMainHUD`/HUD 파트에 적용
  - 기존 기능 위젯의 좌표·크기·ZOrder·바인딩 이름 보존
  - 재현 가능한 UMG 빌드 및 검증 스크립트 추가
- 검증:
  - 수정 Widget Blueprint 8개 컴파일 및 저장 성공
  - Gothic Combat HUD Validator 통과
  - 기존 주요 위젯 9개의 배치 값 일치
  - Texture2D 5개 및 QuickSlot 필수 바인딩 로드 성공
  - `Scripts/CheckGeneratedChanges.ps1` 통과
- 상태: 이미지 제작, Unreal 임포트, 실제 UMG 배치, 자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-05_gothic-combat-hud.md`
  - `Feature/doc/2026-08-05_gothic-combat-hud.md`
  - `DevLog/20260805.txt`
## Sprint#4-3 - Obsidian 아키텍처 결합도·응집도 연동

- 브랜치: `Sprint#4-3-obsidian-architecture-metrics`
- 목표: 현재 C++ 파일 의존 관계를 Obsidian에서 반복 생성하고 결합도·응집도로 탐색한다.
- 주요 변경:
  - `Source/Rogue10m` 내부 include 분석기와 지표 검증기 추가
  - C++ 파일 97개의 유입·유출 Wiki Link 노트 생성
  - 폴더 응집도·불안정도 표와 Mermaid 의존 그래프 생성
  - 폴더 10개를 연결한 Obsidian Canvas와 원시 `metrics.json` 생성
  - `Docs/Obsidian/ArchitectureMetrics` 대시보드 및 분석·검증·Obsidian 실행 진입점 추가
  - 범용 학습·면접 Vault 폴더와 생성 도구 제거, 지표 산출물을 프로젝트 문서 영역으로 이전
- 검증:
  - 소스 파일 97개와 생성 노트 97개 일치
  - 프로젝트 내부 include 관계 197개 추출
  - 생성 Wiki Link, Canvas JSON, 지표 범위 검증 통과
  - Obsidian 실행 스크립트 PowerShell 구문 검증 통과
- 상태: Obsidian 1.13.6 설치, Rogue10m Vault 등록, Dashboard 자동 열기, 시각화·자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_obsidian-architecture-metrics.md`
  - `Feature/doc/2026-08-12_obsidian-architecture-metrics.md`
  - `DevLog/20260812.txt`

## Sprint#4-4 - 전체 폭 경험치·레벨 바 이미지

- 브랜치: `Sprint#4-4-full-width-xp-level-bar`
- 목표: 화면 하단 전체 폭을 사용하는 고딕 경험치 트랙과 레벨 메달 이미지를 제작한다.
- 주요 변경:
  - 좌측 빈 레벨 메달과 우측 끝까지 이어지는 단일 경험치 트랙 생성
  - 크로마키 제거 후 1920×353 투명 PNG 제작
  - UI용 `T_HUD_GothicXPLevelBar` Texture2D 임포트
  - 기존 경험치 프레임과 Widget Blueprint는 변경하지 않음
- 검증:
  - 네 모서리 알파 0 및 전체 폭 가시 영역 확인
  - Texture2D 크기 1920×353 확인
  - `TEXTUREGROUP_UI`, `NeverStream=true` 확인
  - UnrealEditor-Cmd 오류 0, 경고 0, Validator 통과
- 상태: 이미지 제작, 투명화, Unreal 임포트 및 자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_full-width-xp-level-bar.md`
  - `Feature/doc/2026-08-12_full-width-xp-level-bar.md`
  - `DevLog/20260812.txt`

## Sprint#4-5 - 고딕 몬스터 정보 프레임

- 브랜치: `Sprint#4-5-gothic-monster-info-frame`
- 목표: 몬스터 조준 시 표시되는 420×68 Info 영역을 기존 HUD와 같은 고딕 스타일로 꾸밀 이미지 자산을 준비한다.
- 주요 변경:
  - 상단 이름/레벨 명패와 하단 체력 트랙을 포함한 프레임 제작
  - 추후 속성·상태 아이콘용 작은 좌우 마름모 홈 구성
  - 840×136 투명 PNG와 UI용 Texture2D 임포트
  - 기존 `WBP_MonsterInfo` 배치와 바인딩은 변경하지 않음
- 검증:
  - 네 모서리 알파 0 및 가시 영역 확인
  - Texture2D 크기 840×136 확인
  - `TEXTUREGROUP_UI`, NoMipmaps, `NeverStream=true` 확인
  - UnrealEditor-Cmd 오류 0, 경고 0, Validator 통과
- 상태: 이미지 제작, 투명화, Unreal 임포트 및 자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_gothic-monster-info-frame.md`
  - `Feature/doc/2026-08-12_gothic-monster-info-frame.md`
  - `DevLog/20260812.txt`

## Sprint#4-6 - 공통 캐릭터 애니메이션·VFX

- 브랜치: `Sprint#4-6-common-character-animation`
- 목표: 모든 종족이 같은 스킬에서 동일한 원본 모션을 사용하도록 걷기·뛰기·점프·구르기·주먹 공격과 연계 효과를 구축한다.
- 주요 변경:
  - 숨김 Manny 공통 Source AnimBP와 기존 종족별 Retarget AnimBP를 연결
  - 이벤트·타이머 기반 이동/점프/구르기 AnimationComponent 추가
  - 걷기·뛰기·점프·2단 점프·착지·구르기 Niagara 6종 구성
  - 단일·차징·3연계 주먹 Montage와 Cast/Charge/Impact Niagara 6종 구성
  - 공격/모션 Data Asset에서 Montage와 Niagara를 교체할 수 있도록 확장
  - Unarmed 1단, StoneFist 2단 점프 프로필 검증
- 검증:
  - UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
  - Unreal Asset Validator 30개 자산 통과
  - 공통 검증기: Locomotion 5종, Montage 5개, Niagara 12개, 외형 6종, 공격 8개, 점프 프로필 1/2 통과
  - `Scripts/CheckGeneratedChanges.ps1`, `git diff --check` 통과
- 상태: 코드·자산·Data Asset 구성과 자동 검증 완료, PIE 체형별 육안 QA 권장
- PIE 직접 검증:
  - Human Male에서 공통 `ABP_Common_Unarmed`, Idle Locomotion 확인
  - 기본 주먹 `AM_Punch_01`, 특수 주먹 `AM_Punch_03`, 구르기 `AM_Dodge_Roll` 런타임 재생 확인
  - 점프 이벤트와 Jump Niagara, 공격·구르기 Niagara 런타임 컴파일 확인
  - 지속 입력이 필요한 뛰기·2단 점프·차징·체인 전체 재생은 수동 입력 QA 대기
- 관련 문서:
  - `Feature/architect/2026-08-12_common-character-animation-vfx.md`
  - `Feature/doc/2026-08-12_common-character-animation-vfx.md`
  - `DevLog/20260812.txt`

## Sprint#4-8 - 묵직한 전리품 주머니 Actor

- 브랜치: `Sprint#4-8-loot-bag-actor`
- 목표: 인벤토리 월드 드롭과 몬스터 전리품이 공통으로 사용할 수 있는 묵직한 주머니 Actor Blueprint를 제공한다.
- 주요 변경:
  - `ARogue10mDroppedItem` 상속 `BP_HeavyLootBag` 생성
  - 눌린 주머니 본체, 목, 매듭, 좌우 끈의 5개 StaticMeshComponent 구성
  - 프로젝트 천·목재 머티리얼 재사용 및 금빛 이름 표시 적용
  - 공통 주머니 외형 유지를 위해 상속된 개별 아이템 메시 숨김
  - 본체만 `OverlapAllDynamic`, 장식은 `NoCollision`, Tick·동적 조명 없음
  - 재현 가능한 Unreal Editor 생성·검증 스크립트 추가
- 검증:
  - UnrealEditor-Cmd Blueprint 생성·컴파일·저장 성공
  - 새 UnrealEditor-Cmd 프로세스에서 부모·컴포넌트·메시·머티리얼·충돌·가시성 검증 `RESULT=PASSED`
  - `Scripts/CheckGeneratedChanges.ps1` 통과
  - `git diff --check` 통과
- 상태: Actor `.uasset` 제작 및 자동 검증 완료, 인벤토리/몬스터 스폰 클래스 연결은 후속 통합 작업
- 관련 문서:
  - `Feature/architect/2026-08-12_heavy-loot-bag-actor.md`
  - `Feature/doc/2026-08-12_heavy-loot-bag-actor.md`
  - `DevLog/20260812.txt`

## Sprint#4-7 - 고딕 UI 통합 적용

- 브랜치: `Sprint#4-7-gothic-ui-application`
- 목표: 준비된 고딕 HUD·인벤토리·경험치·몬스터 정보 자산을 실제 프로젝트 UMG에 적용한다.
- 주요 변경:
  - 신규 1120×1220 투명 인벤토리 외곽 프레임 제작 및 `WBP_InventoryWindow` 적용
  - 원형 레벨 장식을 보존한 1920×112 실사용 전체 폭 경험치 바 제작 및 `WBP_LevelExperiencePanel` 적용
  - 840×136 몬스터 정보 프레임을 `WBP_MonsterInfo`에 적용
  - 기존 C++ `BindWidget` 이름과 인벤토리/몬스터 배치 보존
  - `ProgressionWidget`을 화면 최하단 전체 폭, ZOrder 0으로 조정
  - 실제 조합 확인용 `WBP_GothicUIShowcase`와 1920×1080 적용 프리뷰 추가
- 검증:
  - 관련 Widget Blueprint 컴파일 성공
  - TextureGroup UI, NoMipmaps, NeverStream 및 해상도 검사 통과
  - 필수 바인딩 누락 0개
  - `GothicIntegratedUIValidation.txt` 결과 `PASSED`
  - Unreal Editor Widget Designer 시각 확인 완료
- 상태: 텍스처 제작, Unreal 임포트, UMG 적용, 자동 검증, 적용 프리뷰 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_gothic-ui-application.md`
  - `Feature/doc/2026-08-12_gothic-ui-application.md`
  - `DevLog/20260812.txt`

## Sprint#4-9 - 던전 보상 보물상자 Actor

- 브랜치: `Sprint#4-9-dungeon-reward-chest`
- 목표: 던전 클리어 보상 지점에 배치하거나 스폰할 수 있는 보물상자 Actor `.uasset`을 제작한다.
- 주요 변경:
  - `/Game/World/Rewards/BP_DungeonRewardChest` Blueprint Actor 생성
  - 목재 본체·둥근 뚜껑·금속 띠·잠금장치를 포함한 11개 메시 컴포넌트 구성
  - 후속 개방 연출용 `LidPivot`, 상호작용용 `RewardInteractionZone`, 보상 강조용 `RewardGlow` 추가
  - `DungeonRewardChest`, `RewardContainer` 태그와 Tick 비활성화 적용
  - 재현 가능한 Unreal Editor 생성·검증 스크립트 추가
- 검증:
  - UnrealEditor-Cmd Blueprint 생성·컴파일·저장 성공
  - 새 UnrealEditor-Cmd 프로세스에서 부모·태그·Tick·컴포넌트·메시·재질·충돌·오버랩·조명 검증 `RESULT=PASSED`
  - 오류 0건; 기존 `AdvancedPortalsSystemVFX/SM_Plane` 관련 물리 경고만 확인
- 상태: Actor `.uasset` 제작 및 자동 검증 완료, 개봉·보상 지급·중복 수령 방지·저장 연동은 후속 통합 작업
- 관련 문서:
  - `Feature/architect/2026-08-12_dungeon-reward-chest-actor.md`
  - `Feature/doc/2026-08-12_dungeon-reward-chest-actor.md`
  - `DevLog/20260812.txt`
---

## Sprint#4-10 - 고딕 전투 HUD 하단 배치 보정 (2026-08-12)

- 목표: 경험치 바를 화면 최하단 전체 폭에 붙이고 초록색으로 변경하며, 체력/스태미나를 구분하고 스킬/아이템 외측 빈 공간을 고딕 장식으로 채운다.
- 주요 변경:
  - 1920x52 전용 하단 경험치 프레임과 초록색 진행 바 적용
  - 화면 중앙 기준 체력/스태미나/메달리온/슬롯 패널 대칭 재배치
  - 5칸 기능 슬롯을 유지한 좌측 스킬 날개 및 우측 아이템 날개 Texture2D 제작/연결
  - 스태미나 런타임 색상을 황금색 계열로 정리
- 검증:
  - Editor target build 성공
  - `Saved/GothicIntegratedUIValidation.txt`: `RESULT=PASSED`
  - Unreal Editor `WBP_GothicUIShowcase` Designer 실제 표시 확인
- 상태: 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_gothic-hud-layout-refinement.md`
  - `Feature/doc/2026-08-12_gothic-hud-layout-refinement.md`

## Sprint#4-11 - 기본 이동 파티클 절제 조정 (2026-08-13)

- 브랜치: `Sprint#4-11-subtle-basic-motion-vfx`
- 목표: 과도한 걷기 파티클을 참조 영상처럼 짧고 둥근 먼지 Poof로 바꾸고 기본 동작 전체의 화면 점유와 누적을 줄인다.
- 주요 변경:
  - StarterContent 연기 원본을 UE 5.8 공식 Cascade→Niagara 변환기로 프로젝트용 소프트 먼지 Niagara로 변환
  - 걷기·뛰기·점프·2단 점프·착지·구르기 Niagara 6종을 같은 먼지 계열로 통일
  - 걷기 0.12배/0.68초, 뛰기 0.16배/0.38초 등 동작 강도별 스케일과 발생 간격 적용
  - 동작별 0.08~0.14초 방출 종료 Timer와 Niagara 시간 배율 3.0 적용
  - 좌우 발 오프셋과 첫 발자국 지연을 적용하고 공격 VFX는 보존
  - 변환된 자산의 에디터 전용 플러그인 런타임 의존성 검증 추가
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 구성 스크립트 `RESULT=COMMON_CHARACTER_ANIMATION_CONFIGURED`
  - 검증기: 이동 상태 5종, Niagara 12종, 절제 이동 VFX 6종, 1단/2단 점프 프로필 통과
  - 변환기 런타임 패키지 의존성 0개 확인
  - Human Male 실제 PIE에서 걷기·점프·구르기 입력 실행, 기존 청색 파편 반복 및 이동 VFX 누적 미발생 확인
  - 짧은 방출의 개별 먼지 프레임은 입력 자동화 타이밍상 정지 화면 분리가 제한적이며 뛰기·2단 점프는 자동 검증으로 확인
- 상태: 코드·Data Asset·Niagara 보정, 빌드, 자동 검증, 직접 PIE 기본 동작 QA 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_subtle-basic-motion-vfx.md`
  - `Feature/doc/2026-08-12_subtle-basic-motion-vfx.md`
  - `DevLog/20260813.txt`

## Sprint#4-12 - 캐릭터 파티클 구조 및 출력 정리 (2026-08-14)

- 브랜치: `Sprint#4-12-organized-character-vfx`
- 목표: 기본 이동과 맨손 공격 파티클을 용도별로 정리하고, 화면을 가리지 않는 짧고 절제된 출력으로 통일
- 주요 변경:
  - 이동 Niagara 6종을 `VFX/Character/Movement`, 맨손 공격 Niagara 6종을 `VFX/Character/Combat/Unarmed`로 분리
  - 공격 Data Asset에 시전·차징·타격 크기, 방출 시간, Niagara 시간 배율 튜닝값 추가
  - 시전 0.22~0.32배/0.10~0.16초, 차징 0.18배, 타격 0.28배/0.14초로 축소
  - 사용이 끝난 일회성 Niagara를 Timer로 비활성화하고 기존 루트 중복 자산은 복구 가능한 `tmp/legacy-character-vfx-backup-20260814`로 이동
  - 구성 및 검증 스크립트가 정리된 경로와 출력 튜닝을 생성·검사하도록 갱신
- 검증:
  - UE 5.8 Editor target 전체 및 증분 빌드 성공
  - 공용 캐릭터 애니메이션 검증기 `RESULT=PASSED`, Niagara 12종과 이동/공격 각 6종 확인
  - Python 구성·검증 스크립트 문법 검사 통과
- 상태: 구현, 자산 정리, 빌드 및 자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-14_organized-character-vfx.md`
  - `Feature/doc/2026-08-14_organized-character-vfx.md`
  - `DevLog/20260814.txt`
## Sprint#4-13 - 애니메이션 단독 표시 및 파티클 분리 (2026-08-15)

- 브랜치: `Sprint#4-13-animation-only-presentation`
- 목표: 캐릭터 기본 모션을 파티클 간섭 없이 확인하고 애니메이션·VFX를 독립 관리
- 주요 변경:
  - 공통 이동 Data Asset과 공격 스킬 Data Asset에 독립 VFX 표시 스위치 추가
  - 이동 1개 및 공격 8개 Data Asset의 파티클 표시를 OFF로 저장
  - 이동 발걸음 Timer와 점프·착지·구르기 Niagara 생성 경로 차단
  - 공격 시전·차징·타격 Niagara 생성 경로 차단, 몽타주와 전투 판정은 유지
  - 애니메이션 `/Game/Rogue10m/Animation`, 파티클 `/Game/Rogue10m/VFX` 구조를 별도 카탈로그로 정리
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 구성 스크립트 `RESULT=COMMON_CHARACTER_ANIMATION_CONFIGURED`
  - 자동 검증 `RESULT=PASSED`, `presentation=animation_only motion_vfx=off attack_vfx=off`
  - 애니메이션 상태 5종, 몽타주 5개, 종족 6종, 공격 8종 연결 확인
- 상태: 구현·자산 저장·자동 검증 완료, 파티클 OFF 게임 창 실행 및 정상 응답 확인
- 관련 문서:
  - `Feature/architect/2026-08-15_animation-only-presentation.md`
  - `Feature/doc/2026-08-15_animation-only-presentation.md`
  - `DevLog/20260815.txt`
## Sprint#4-15 - 직업별 통합 하단 전투 HUD (2026-08-19)

- 브랜치: `Sprint#4-15-class-themed-bottom-hud`
- 목표: 마법사, 전사, 권사, 도적의 하단 전투 UI를 하나의 재사용 가능한 Bottom Widget으로 통합
- 주요 변경:
  - 1920×208 직업 프레임 4종과 실제 전투 화면형 시안 4종 제작
  - `URogue10mBottomHUDWidget`, `WBP_BottomHUD` 추가
  - HP, MP/스테미나, 아이덴티티, 스킬 5칸, 아이템 5칸, 레벨/경험치를 Bottom HUD 내부로 이동
  - Main HUD의 개별 하단 자식을 제거하고 `BottomHUDWidget` 한 개만 전체 폭 260px로 배치
  - 무기/아이덴티티 기반 자동 테마 판별과 Blueprint `ThemeOverride` 제공
  - 마법사는 활성 Mana View, 나머지 직업은 스테미나를 같은 자원 위치에 표시
  - 전체 폭 52px 초록색 경험치 바를 화면 최하단에 유지
- 검증:
  - UE 5.8 Editor target 전체 및 증분 빌드 성공
  - 네 Texture2D 1920×208, UI Group, NoMipmaps, NeverStream 통과
  - Bottom HUD 필수 바인딩 14개, 스킬/아이템 각 5칸 통과
  - Main HUD `BottomHUDWidget` 1개, 이전 하단 직접 자식 0개 확인
  - `Saved/ClassBottomHUDValidation.txt`: `RESULT=PASSED`
  - `Scripts/CheckGeneratedChanges.ps1`, `git diff --check`, Python 문법 검사 통과
- 상태: 코드, 직업별 텍스처, Widget Blueprint 조립, Main 통합, 자동 검증, 적용 프리뷰 완료
- 관련 문서:
  - `Feature/architect/2026-08-19_class-themed-bottom-hud.md`
  - `Feature/doc/2026-08-19_class-themed-bottom-hud.md`
  - `DevLog/20260819.txt`


## Sprint#4-16 - 장착 무기 기반 스킬트리 자동 활성화 (2026-08-19)

- 브랜치: `Sprint#4-16-weapon-driven-skill-tree`
- 목표: 인벤토리에서 장착한 무기 타입과 활성 스킬트리·기본 입력 슬롯을 항상 일치시킨다.
- 주요 변경:
  - 무기 교체 시 동일 `WeaponType`의 `URogue10mWeaponSkillProfileDataAsset` 즉시 적용
  - 차징, 콤보, 예약 GAS 공격, 다중 타격 타이머 등 이전 무기 전투 상태 정리
  - 현재 활성 트리 밖 스킬의 입력 슬롯 지정 차단
  - 프로필이 정의되지 않은 입력 슬롯의 과거 공격 fallback 차단
  - 활성 트리 무기 타입과 스킬 소속 여부 Blueprint 조회 API 추가
  - 누락된 Bow 전용 5스킬 프로필, Montage 5개, Niagara 3개 생성 및 캐릭터 데이터 등록
  - 11개 무기 자동 전환 전용 런타임 테스트 추가
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - Data Asset Validator: `RESULT=PASSED profiles=11 attacks=55 montages=55 niagara=33 combos=11 first_person_safe=55`
  - 런타임 전환: `RESULT=WEAPON_SKILL_TREE_PASSED weapons=11 failures=0`
  - 교차 무기 스킬 지정 거부, 기본 슬롯·회피·점프 적용 확인
- 상태: 코드, 활 콘텐츠, 캐릭터 데이터 등록, 자동 전환, 빌드 및 런타임 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-19_weapon-driven-skill-tree.md`
  - `Feature/doc/2026-08-19_weapon-driven-skill-tree.md`
  - `DevLog/20260819.txt`

## Sprint#4-14 - 직업·무기별 전투 애니메이션 및 1인칭 안전 VFX (2026-08-19)

- 브랜치: `Sprint#4-14-class-combat-animation-vfx`
- 목표: 요청한 10개 전투 스타일에 종족 공통 공격 모션과 1인칭 시야를 가리지 않는 Niagara 효과를 구성
- 주요 변경:
  - 단검, 표창, 쌍단검, 장검, 대검, 쌍검, 방패, 한손검·작은방패, 마법사, 권사 프로필 제작
  - 스타일별 기본 3연계, 특수, 차징을 포함한 Montage 50개와 Attack Skill 50개 구성
  - 공격용 Niagara 30개를 애니메이션 자산과 별도 경로로 관리
  - 공통 Manny 소스와 종족별 AnimBP 연결을 통해 같은 스킬의 종족별 모션 통일
  - 로컬 1인칭 위치·회전·크기·방출시간 보정과 양손 보조 효과 지원
  - 차징 및 일회성 Niagara의 즉시 비활성화·파괴로 효과 누적 방지
  - 민첩/마법/권법 계열 선택형 2단 점프, 전사 계열 1단 점프 설정
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 요청 10종 자산 검증 `RESULT=PASSED profiles=10 attacks=50 montages=50 niagara=30 combos=10 first_person_safe=50 races=6`
  - 1인칭 튜닝 `RESULT=FIRST_PERSON_CLASS_COMBAT_VFX_TUNED styles=10 attacks=50 charge_multiplier=0.12`
  - 실제 게임 실행에서 요청 10종, 30회 연계, 10회 차징, 캡처 10장, 실패 0건
  - 쌍단검·쌍검·권사 최종 캡처 육안 검토에서 중앙 조준 영역 유지
- 상태: 코드, 자산, 종족 공통 연결, 1인칭 VFX 튜닝, 실제 게임 실행 QA 및 문서화 완료
- 관련 문서:
  - `Feature/architect/2026-08-19_class-combat-animation-vfx.md`
  - `Feature/doc/2026-08-19_class-combat-animation-vfx.md`
  - `DevLog/20260819.txt`


## Sprint#4-17 - 권사 무협 수련 스킬트리 (2026-08-19)

- 브랜치: `Sprint#4-17-martial-artist-skill-tree`
- 목표: 권갑 장착 시 활성화되는 무협 기반 권사 전용 스킬트리와 수련·토벌 해금 방식을 구현
- 주요 변경:
  - 입문·초식·절기·심법 4경지, 10개 무공 데이터 구성
  - 실제 무공 사용 횟수와 특정 몬스터 처치 횟수를 PlayerState에 기록
  - 선행 무공과 복합 조건을 만족하면 후속 무공을 연쇄 자동 해금
  - 금강불괴 해금 시 받는 피해 15% 감소, 입력 슬롯 장착은 차단
  - 권갑 프로필은 연환권 1개만 시작 해금하고 나머지 무공은 진행형으로 전환
  - 흑철·고동·옥빛 무협 프레임 Texture2D 제작 및 UMG 적용
  - 좌측 권맥 경지, 중앙 경맥형 분기 트리, 우측 비급 상세 패널과 노드 진행률 구성
  - 실제 게임 UMG 포함 1920×1080 자동 캡처 명령 추가
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 권사 전용 데이터·위젯 검증, 전체 메뉴 검증, Widget Flow 검증 오류 0건
  - 런타임 `RESULT=WEAPON_SKILL_TREE_PASSED weapons=11 failures=0`
  - 권사 후속 무공 9개 연쇄 해금, 금강불괴 15% 감소, 패시브 슬롯 차단 확인
  - `git diff --check` 통과
- 상태: 코드, 데이터 에셋, UI 텍스처, Widget Blueprint, 런타임 진행도, 자동 검증 및 실제 캡처 완료
- 관련 문서:
  - `Feature/architect/2026-08-19-martial-artist-skill-tree.md`
  - `Feature/doc/2026-08-19-martial-artist-skill-tree.md`
  - `DevLog/20260819.txt`

## Sprint#4-18 - 오버워치풍 1인칭 전투 VFX 가독성 개선 (2026-08-26)

- 브랜치: `Sprint#4-18-overwatch-inspired-combat-vfx`
- 목표: 영웅 슈터의 선명한 공격 피드백을 유지하면서 1인칭 조준점과 적 실루엣을 가리지 않는 직업별 Niagara 구성
- 주요 변경:
  - 10종 핵심 공격 50개에 `HeroShooterFP_v1` 스케일·수명·오프셋 프로필 적용
  - Primary 1·2·3, Special, Charged 단계와 무기 무게에 따른 효과 강도 계층화
  - 양손 보조 파티클의 1인칭 최대 배율을 0.70으로 분리
  - 전투 Niagara 30개에 공통 `NET_CombatReadable` Effect Type 배정
  - 실제 권사 진행 스킬 4개에 권사 Montage·Niagara presentation 연결, 피해·타격 수·해금 조건 보존
  - 권사 첫 Niagara 에디터 컴파일을 워밍업 처리하는 실게임 테스트 보완
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 핵심 자산 `RESULT=PASSED profiles=10 attacks=50 montages=50 niagara=30 first_person_safe=50`
  - 권사 진행 스킬 `RESULT=MARTIAL_ART_READABLE_VFX_PASSED skills=4 montages=4 first_person_safe=4`
  - 실게임 `RESULT=CLASS_COMBAT_RUNTIME_PASSED styles=10 combos=27 charged=9 screenshots=10 failures=0`
  - 캡처 10장 육안 QA에서 중앙 표적 영역 유지, 효과 누적 없음
  - `CheckGeneratedChanges.ps1` Harness 경로 검사 통과
- 상태: 데이터 프로필, UE 자산 저장, 권사 실사용 경로, 빌드, 자동 검증, 실제 게임 및 시각 QA 완료
- 관련 문서:
  - `Feature/architect/2026-08-26_overwatch-inspired-combat-vfx.md`
  - `Feature/architect/2026-08-26_overwatch-inspired-combat-vfx-amendment.md`
  - `Feature/doc/2026-08-26_overwatch-inspired-combat-vfx.md`
  - `DevLog/20260826.txt`

## Sprint#4-19 - 로스트아크풍 HUD 개선 기획 (2026-09-08)

- 브랜치: `Sprint#4-19-lostark-hud-design-plan`
- 목표: 현재 UI의 크기·구조 문제를 소스 기준으로 진단하고 전투 HUD 개선안을 시각화한다.
- 주요 변경:
  - 기획1·구조 검증1·시각 검증1의 독립 조사 및 통합.
  - stretch 배경과 고정 좌표 혼용, 260px 하단 높이, 52px 경험치 영역을 개선 대상으로 기록.
  - 1080p 중앙 1000×160 코어, 행동5칸·현재 잠금 아이템4칸, 경험치4px 기획.
  - 로스트아크풍 생성 콘셉트와 최종 프롬프트, 후속 개발2·검증2 패킷 작성.
- 검증: 소스/문서/생성 그림 대조. 문서 경로·공백 검사를 실행하며 C++ 빌드/실제 PIE는 후속 범위.
- 상태: 기획·콘셉트 완료, 게임 적용 전. 최신 런타임 문제 재현은 미실시.
- 관련 문서:
  - `Feature/architect/2026-09-08_lostark-hud-design-plan.md`
  - `Feature/doc/2026-09-08_lostark-hud-design-plan.md`
  - `ui-concepts/2026-09-08/rogue10m-lostark-hud-concept.png`
  - `DevLog/20260908.txt`
- 최종 문서 검사: git diff --check 및 Harness path check 통과. 정확한 치수 도면 `ui-concepts/2026-09-08/hud-layout-1920x1080.svg` 추가.

## Sprint#4-20 - 화면 비율 대응 전투 HUD 적용 (2026-09-08)

- 브랜치: `Sprint#4-20-responsive-combat-hud`
- 목표: 화면 크기·비율 변화에 대응하는 중앙 전투 HUD를 현재 UE5.8 프로젝트에 적용한다.
- 주요 변경:
  - 공통1000×160 코어, DownOnly 균등 축소, 엔진 DPI 상속, 여백24, 전체폭 XP4.
  - 행동5·아이템4의56 정사각형 슬롯과 직업 문장76 정사각형.
  - 상단 대상 정보, 우상단 타이머, 실제 시스템 로그2줄·획득 알림3줄.
  - 기존 Main/Bottom 두 자산 수정 및 전용 파트6개 생성, 구형 생성기 덮어쓰기 보호.
  - 리뷰 결과에 따라 XP 예시값 잔류와 획득 수량 표시 누락 수정.
- 검증:
  - 설치 UE5.8.2 Editor 타깃 최종 빌드 성공.
  - `RESULT=RESPONSIVE_HUD_SETUP_PASSED`, Editor 자산8개 저장.
  - `RESULT=RESPONSIVE_HUD_PASSED cases=6 failures=0`.
  - 720p·1080p·1440p·3440×1440·800×900/UI125%·1080p 복귀 실제 렌더링 검사.
  - 좁은 창 코어816.6×130.7 및 문장62.1 정사각형 측정, 직업4종·마나/스태미나·타이머·획득 수량 검사.
  - git diff --check 및 Harness 경로 검사 통과.
- 상태: 구현·Editor 적용·빌드·실제 게임 검증 완료. 패키지 빌드 및 신규 스킬 아트는 범위 밖.
- 관련 문서:
  - `Feature/architect/2026-09-08_responsive-combat-hud.md`
  - `Feature/doc/2026-09-08_responsive-combat-hud.md`
  - `Feature/doc/images/responsive-hud-20260908/`
  - `DevLog/20260908.txt`

## Sprint#4-21 - 참조 이미지 기반 금속 전투 HUD (2026-09-09)

- 브랜치: `Sprint#4-21-reference-metal-hud`
- 목표: 첨부 HUD의 검은 금속 프레임·중앙 원형 게이지·아이콘 아래 입력 표기를 실제 게임에 반영한다.
- 주요 변경:
  - 1200×208 코어, 아래56·좌우24, DPI 상속과 균등 축소 유지.
  - 생성 프레임 원본과 Native AtlasImage UV3조각으로 금속 외곽 구성.
  - 실제 자원 원형 게이지, 아이콘 없는 경우 UI 대체 그림, 실제 상태의 쿨다운·잠금 유지.
  - 전용 파트7개와 기존 Main/Bottom2개, 프레임 Texture2D1개 저장.
  - 독립 리뷰의 수평 게이지 중복 및 실제 캡처의 프레임 접합·빈 문장 문제 해결.
- 검증:
  - UE5.8.2 최종 Editor 빌드 성공.
  - `RESULT=REFERENCE_METAL_SETUP_PASSED`
  - `RESULT=REFERENCE_METAL_HUD_PASSED cases=6 failures=0`
  - 720p·1080p·1440p·울트라와이드·좁은 창/UI125%·1080p 복귀 실게임 확인.
  - 독립 최종 시각 QA3장 PASS, Python AST·공백·Harness 경로 검사 통과.
- 상태: 구현·빌드·Editor 적용·실게임 및 시각 검증 완료. 커밋·푸시 없음.
- 관련 문서:
  - `Feature/architect/2026-09-08_reference-metal-hud.md`
  - `Feature/doc/2026-09-09_reference-metal-hud.md`
  - `Feature/doc/images/reference-metal-hud-20260909/`
  - `DevLog/20260909.txt`


## Sprint#4-22 - 레퍼런스형 1인칭 주먹과 주변 HUD (2026-09-09)

- 브랜치: `Sprint#4-22-first-person-fist-presentation`
- 목표: 양손 가드·전방 펀치·복귀와 안정된 시야, 가장자리 경험치 및 상단 대상 정보를 실제 게임에 적용한다.
- 주요 변경: 전용 프레젠테이션 컴포넌트·네이티브 FistAnimInstance·DefaultSlot 기반 양팔 IK·손목 보정·타 무기 복원. XP 좌우24/아래16/높이10과 상단 대상560×68/위24. Editor Widget4개 적용.
- 검증: UE5.8.2 빌드 성공63.03초, 실제 공격3회/120프레임 실패0, HUD7조건 실패0, 독립 소스·시각 QA 및 Python/공백/Harness 검사 통과.
- 상태: 구현·Editor 적용·실게임 및 시각 검증 완료. 현재 Manny/테스트 맵으로 실제5초 GIF 보관. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-09_first-person-fist-presentation.md`, `Feature/doc/2026-09-09_first-person-fist-presentation.md`, `Feature/doc/images/first-person-fist-20260909/`, `DevLog/20260909.txt`.

## Sprint#4-23 - 양손 방향과 근접 주먹 자세 개선 (2026-09-09)

- 브랜치: `Sprint#4-23-quick-melee-hand-orientation`
- 목표: 양손이 돌아가 보이는 문제를 수정하고 오버워치 quick melee의 짧고 선명한 자세 원칙을 참고한다.
- 주요 변경: 고정 180도 회전 제거, 실제 골격 기준 손·전완 정렬, 가드 15/타격 35도, 손가락 굴곡 100/100/75도와 엄지 배치, 포즈 캐시 및 관련 런타임 검증 확장.
- 검증: UE 5.8.2 빌드 성공 40.82초, 실제 공격 3회/120프레임 실패 0, 시선 ±25도·울트라와이드 3조건, 무기 복원, 독립 소스·시각 6장 검토, GIF 및 공백·Harness 검사 통과.
- 상태: 구현·실게임 및 독립 검증 완료. 공식 글의 원칙을 참고했으며 외부 클립 직접 재생은 도구 ACL 오류로 미확인. 오른 엄지의 작은 시각 비대칭 잔여. 기존 게임플레이·카메라·HUD 자산 변경 및 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-09_quick-melee-hand-orientation.md`, `Feature/doc/2026-09-09_quick-melee-hand-orientation.md`, `Feature/doc/images/quick-melee-20260909/`, `Feature/doc/evidence/quick-melee-20260909/`, `DevLog/20260909.txt`.

## Sprint#4-24 - 몸통 기반 1인칭 근접 공격과 카메라 동조 (2026-09-09)

- 브랜치: `Sprint#4-24-body-driven-first-person-melee`
- 목표: 전신 몽타주의 체중 이동·휘두르기를 팔과 표시 카메라에 전달한다.
- 주요 변경: 골반·척추·쇄골 CS 보정 후 팔 IK, signed 3D 손 궤적과 반대손 가드, 안전한 애니메이션 snapshot→최종 POV 동조, 강도·범위·보간 설정, 비활성·무기·사망·시점 전환 초기화.
- 검증: UE 5.8.2 통합 빌드 25.95초/최종 증분 8.89초 성공, 실제 120프레임 실패 0, 시야 3조건, 추가 공격의 활성 강도 0·무기 전환·사망 상태 검사, 독립 소스 및 시각 21장 검토 통과.
- 상태: 구현·실게임·독립 검증 완료. 영상 직접 재생은 도구 오류로 미확인이고 Mixamo 파일 미적용. 전신 전체 노출 없이 어깨·팔과 POV로 체중 이동을 전달한다. 타격·HUD·바이너리 자산 및 커밋·푸시 변경 없음.
- 관련 문서: `Feature/architect/2026-09-09_body-driven-first-person-melee.md`, `Feature/doc/2026-09-09_body-driven-first-person-melee.md`, `Feature/doc/images/body-melee-20260909/`, `Feature/doc/evidence/body-melee-20260909/`, `DevLog/20260909.txt`.

## Sprint#4-25 - 기본 직업 좌우 연타·차징 어퍼컷·점프 (2026-09-10)

- 브랜치: `Sprint#4-25-basic-brawler-inputs`
- 목표: 기본 Unarmed의 왼손→오른손 연계, 오른손 차징 어퍼컷, 실제 머리/카메라 동조와 일반 점프를 구현한다.
- 주요 변경: 전용 입력 상태 컴포넌트, 기본 공격 Data Asset 4개, 프로필 연결, 본 기반 FP 포즈와 점프 표현, 취소/쿨다운 보호.
- 검증: UE 5.8.2 최종 빌드 8.57초, Editor 자산 적용 통과, 기본 직업 480프레임 실패 0, 기존 너클 120프레임/시야 3조건 실패 0. 독립 소스·시각 검토와 10초 GIF/자산 해시/경로 검사 통과.
- 상태: 구현·통합·회귀·독립 시각 검증 완료. 원본 YouTube 영상 재생·직접 비교는 반복된 접근 오류로 미완료이며 영상 파일 제공 또는 도구 복구 대기(blocked). 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-10_basic-brawler-inputs.md`, `Feature/doc/2026-09-10_basic-brawler-inputs.md`, `Feature/doc/evidence/basic-brawler-20260910/`, `DevLog/20260910.txt`.
## Sprint#4-26 - 자연스러운 기본 격투가 모션 (2026-09-10)

- 브랜치: `Sprint#4-26-natural-brawler-motion`
- 목표: 준비·타격·회수의 리듬과 몸·머리·카메라 연결을 자연스럽게 다듬는다.
- 주요 변경: 현재 위치에서 연타 연결, 빠른 신전과 곡선 회수, 가드/반대손 비대칭, 상체 선행·머리 후행, 기본 Unarmed 카메라 임계감쇠, 두 어퍼컷 rate1.5와 실제 타격 약0.422초, 30fps 캡처/카메라 경계검사.
- 검증: UE5.8.2 빌드40.23초, Editor 자산 적용, 기본480프레임/너클120프레임·시야3조건 실패0, 독립 소스·시각 검증, 실제10초 GIF 검증 통과.
- 상태: 구현·실게임 검증 완료. 원본 유튜브 직접 재생은 컴퓨터 플러그인 초기화 ACL 오류로 미완료. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-10_natural-brawler-motion.md`, `Feature/doc/2026-09-10_natural-brawler-motion.md`, `Feature/doc/evidence/natural-brawler-20260910/`, `DevLog/20260910.txt`.

## Sprint#4-27 - 팔만 보이는 1인칭 격투 시야 (2026-09-12)

- 브랜치: `Sprint#4-27-arms-only-first-person`
- 목표: 아래를 볼 때 몸통·하체가 보이지 않도록 팔만 표시하고 기존 격투 동작과 머리 기반 카메라 동조를 유지한다.
- 주요 변경: Editor에서 생성한 팔 전용 메시, 전체 뼈89·재질2 유지와3LOD 검증, 메시·재질·애니메이션·숨김 본·렌더 설정 복원, 실제 최소피치와 -89도 스트레스 검증. 회귀 테스트의 외부 시야 입력 분리.
- 검증: UE5.8.2 통합빌드118.35초/최종20.00초 성공, 자산 저장 및 원본해시보존, 팔전용390·기본480·너클120프레임/시야3조건 실패0. 독립 소스·시각14장 검증, 공백·Harness 경로 검사 통과.
- 상태: 팔 전용 구현·Editor 적용·실게임 회귀 완료. 고릴라 암즈 비교 방향은 기록했으며 원본 영상 직접 비교는 computer-use 초기화 오류로 미완료. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-12_arms-only-first-person.md`, `Feature/doc/2026-09-12_arms-only-first-person.md`, `Feature/doc/evidence/arms-only-20260912/`, `Feature/doc/images/arms-only-20260912/`, `DevLog/20260912.txt`.

### Sprint#4-27 참고 자료 보완 — 사용자 녹화본 확인 (2026-09-12)

- 사용자 제공 갈브레나 1인칭 녹화본을 로컬에서 직접 분석해 브라우저 접근 실패로 남았던 해당 자료의 시각 확인을 보완했다.
- 공격별 팔 진입·회수와 연계 자세, 강공격·착지 카메라 반응, 명중 표현의 비교 방향을 기록했다.
- 검증: 전체 흐름 대표 프레임 및 공격/이동 구간8fps 대조, 서브에이전트 독립 확인. 오디오와 엔진 내부 카메라 원인은 미확인.
- 상태: 해당 녹화본 비교 완료. 신규 게임 기능 구현은 없음. 고릴라 암즈 등 다른 원본 영상 비교는 별도 미완료.
- 관련 문서: `Feature/doc/2026-09-12_recorded-galbreena-reference.md`, `Feature/doc/evidence/recorded-reference-20260912/`, `DevLog/20260912.txt`.

### Sprint#4-27 참고 자료 보완 — 단계별 타격감 영상 (2026-09-12)

- 목표: 두 번째 녹화본의 공격 효과·적 반응·카메라·UI·락온 설명을 현재 격투와 대조한다.
- 주요 기록: 기존 명중 VFX/피해 숫자는 재사용하고, 적 반응·명중 전용 카메라 충격·접촉 정보·체력 감소 강조를 같은 명중에 맞추는 설계 제안. 기존 팔 궤적 참고와 연결.
- 검증: 주 담당의 실제 프레임/자막 관찰, 서브에이전트의 읽기 전용 소스 대조, 문서 공백 검사. 오디오 및 실제 Data Asset VFX 활성 여부는 미확인.
- 상태: 참고 분석 완료. 신규 게임 효과 구현은 없음.
- 관련 문서: `Feature/doc/2026-09-12_impact-animation-reference.md`, `Feature/doc/evidence/impact-reference-20260912/`, `DevLog/20260912.txt`.

## Sprint#4-28 - 레퍼런스 기반 기본 복싱 4동작 (2026-09-12)

- 브랜치: `Sprint#4-28-reference-boxing-combos`
- 목표: 사용자 녹화 분석을 토대로 좌클릭 왼잽→오른잽, 우클릭 짧게 스트레이트/0.85초 모아 놓으면 오른훅을 실제 게임에 구현한다.
- 주요 변경: 네 절차형 동작과 준비·회수 연결, 충전 해제 팔꿈치 연결, 스트레이트 상체 전진, 훅 반대가드 분리. 전용4스킬·프로필 Editor 적용. 확정 명중에서 적 메시 움찔·플래시 및 실행당1회 카메라 충격, 기존 Niagara 재사용. 시작 무기 소유권과 팔 전용 시야 유지.
- 검증: UE5.8.2 최종빌드20.20초 성공; 기본480·명중280·팔시야390·너클120프레임/시야3조건 실패0. 독립 소스·시각 검토 통과. 실제 손 도달 대비 피해 최대2프레임 차이(30fps), 골격 길이 보존. 실제게임10초MP4와 증거 보관, diff/하네스 경로 검사 통과.
- 상태: 구현·Editor 적용·실게임 검증 완료. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-12_reference-boxing-combos.md`, `Feature/doc/2026-09-12_reference-boxing-combos.md`, `Feature/doc/evidence/boxing-combos-20260912/`, `Feature/doc/images/boxing-combos-20260912/`, `DevLog/20260912.txt`.

## Sprint#4-29 - Obsidian 소스 전용 그래프 (2026-09-22)

- 브랜치: Sprint#4-29-obsidian-source-graph
- 목표: DevLog·README를 제외하고 .h/.cpp 파일 사이의 내부 include 관계를 탐색한다.
- 주요 변경: 소스 노트 경로 필터·설정 백업, 실행기 통합, 역방향·대시보드 링크 제거, 소스 121개/관계 292개 재생성, 유출 링크 검증.
- 검증: 생성·링크·지표 검사, PowerShell 4개 구문 및 그래프 설정 보존 검사 통과. Unreal 변경이 없어 엔진 빌드 생략.
- 상태: 구현·파일 검증 완료. 실행 중인 Obsidian 화면 반영 미확인, 필요 시 그래프 검색창 필터 수동 입력. 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-09-22_obsidian-source-graph.md, Feature/doc/2026-09-22_obsidian-source-graph.md, DevLog/20260922.txt.

### Sprint#4-29 보완 — Feature 문서 제외
- 목표·변경: 소스 전용 그래프 검색에 Feature 경로 제외를 명시하고 생성기·실행 설정·사용 안내를 일치시켰다.
- 검증·상태: 필터 저장 및 소스 121개/관계 292개 생성·검증 통과. 현재 앱 화면 반영 미확인.
- 관련 문서: Feature/doc/2026-09-22_obsidian-source-graph.md, DevLog/20260922.txt.

### Sprint#4-29 보완 — 기능 노드와 관련 소스 연결
- 목표: Feature 작업 문서를 숨기면서 전투·인벤토리·스킬 등 기능별 관련 소스는 표시한다.
- 주요 변경: 기능 노드 10개, 기능-소스 연결 164개, 검토 가능한 JSON 분류 규칙과 생성기, Files/Functions 결합 필터 및 금색 기능 그룹.
- 검증: 소스 121개/include 292개 보존, 기능 매핑·링크·연결 수 검사, 미분류 0개, PowerShell 5개 구문과 그래프 설정 검사 통과.
- 상태: 구현·파일 검증 완료. 현재 앱 화면 반영 미확인. 기능 연결은 호출 관계가 아닌 탐색용 분류다. 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-09-22_obsidian-source-graph.md, Feature/doc/2026-09-22_obsidian-source-graph.md, DevLog/20260922.txt.

### Sprint#4-29 보완 — Feature 본문 근거 연결
- 목표: 기존 Feature 설계·결과 본문을 참고하여 기능 노드와 소스를 연결한다.
- 주요 변경: 파일명 패턴 분류를 문서 주제·본문 참조 해석으로 교체, 클래스 정의와 축약명/파일 쌍 해석, 문서 경로·행별 근거 기록, 기능 10개/연결 397개 재생성.
- 검증: 170개 문서 검사·157개 주제 문서 사용, 모든 연결의 실제 근거 확인, 소스 121개/include 292개 보존, 참조 해석 회귀 검사 통과.
- 상태: 구현·생성 검증 완료. 현재 런타임 호출이 아닌 설계/과거 결과를 포함한 문서 연관도. 실행 중 앱 화면 반영은 미확인. 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-09-22_obsidian-source-graph.md, Feature/doc/2026-09-22_obsidian-source-graph.md, DevLog/20260922.txt.

## Sprint#4-30 - Martelo 원본 동작 미리보기 (2026-09-22)

- 브랜치:`Sprint#4-30-martelo-motion-preview`
- 목표:사용자 FBX의 실제 동작과 현재 팔 전용 1인칭 적용 시 차이를 확인한다.
- 주요 변경:SDK 기반65뼈/40샘플 분석,사선·측면 골격 GIF/MP4,현행 절차형 포즈와 리타깃 필요성 비교 문서.
- 검증:원본 Import/평가 성공,왼발 주도 발차기 수치·대표6프레임 직접 확인,서브에이전트 독립 코드·수치 검토.
- 상태:원본 동작 분석·미리보기 완료. 실게임 리타깃/공격 연결은 수행하지 않음. 런타임/Content 변경과 UE 빌드·커밋·푸시 없음.
- 문서:`Feature/architect/2026-09-22_martelo-motion-preview.md`,`Feature/doc/2026-09-22_martelo-motion-preview.md`,`DevLog/20260922.txt`.


## Sprint#4-31 - Martelo 실제 게임 1인칭 미리보기 (2026-09-22)

- 브랜치: `Sprint#4-31-martelo-first-person-preview`
- 목표: 사용자 FBX를 실제 플레이어의 팔 전용 메시에서 재생한 게임 화면을 제공한다.
- 주요 변경: Mixamo→Manny 프리뷰 자산, 재사용 가능한 Editor 생성·포즈 검증, 독립 게임 촬영 명령, 원래 속도·0.5배속 MP4.
- 검증: UE 5.8.2 빌드 성공, 소스 40시점×65뼈 위치 보존, 새 Editor 자산 재사용 성공, 최종 게임 209프레임·실패 0, 공용 자산 네 파일 해시 보존, 소스·화면 검토 및 변경 경로 검사 통과.
- 상태: 실제 적용 미리보기 완료. 고정 카메라·블렌딩 없는 전환이며 팔 전용 설정상 발차기는 보이지 않는다. 기존 공격 입력·피해 판정에 연결하지 않았다. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-22_martelo-first-person-preview.md`, `Feature/doc/2026-09-22_martelo-first-person-preview.md`, `DevLog/20260922.txt`.


## Sprint#4-32 - 팔·몸통·다리가 보이는 전신 1인칭 (2026-09-22)

- 브랜치: `Sprint#4-32-full-body-first-person`
- 목표: 팔 전용 캐릭터를 몸통·다리까지 보이는 1인칭으로 변경하고 실제 게임 화면을 제공한다.
- 주요 변경: 전체 Manny/머리 가림, 캡슐 기준 전신 부착, 일반 월드 투영으로 재질 가림 우회, 전신 아래보기 -85도, 카메라·손 목표 및 맨손 전용 가드 보정, 팔 전용 옵션 유지.
- 검증: UE 5.8.2 빌드 22.86초 성공. 기본 격투 480·너클 120·팔 전용 390·전신 시연 269프레임 모두 실패 0. 너클 상하/울트라와이드 추가 3시점과 실제 화면 검토. 공용 자산 네 파일 보존 및 변경 경로 검사 통과.
- 상태: 구현·통합 검증 완료. 실제 전신 아래보기와 Martelo 원래 속도/0.5배속 촬영. Martelo 입력 매핑·피해 및 보행/발 접지 애니메이션은 추가하지 않음. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_full-body-first-person.md`, `Feature/doc/2026-09-22_full-body-first-person.md`, `DevLog/20260922.txt`.


## Sprint#4-33 - 머리 움직임을 따르는 전신 1인칭 카메라 (2026-09-22)

- 브랜치: `Sprint#4-33-head-attached-camera`
- 목표: 달리기·점프·공격 시 머리에 붙은 듯 움직이고 실제 머리 회전을 따라 후방도 보는 카메라.
- 주요 변경: 숨김 이전 native/SingleNode 머리 포즈 추종, 눈 위치 회전 보정, 실제 이동 기반 상체 흔들림, 연속 yaw/반강도 각도 경계, 입력·팔 전용 모드 보존.
- 검증: UE 5.8.2 빌드 27.01초 성공. 후방 회전 1,446샘플, 기본480·너클120/시야3·팔390·최종 시연419프레임 실패 0. 독립 소스/시각 검토, 공용 자산 및 경로·diff 검사 통과.
- 상태: 구현·통합 검증 완료. 약14초 실제 영상 제공. Martelo는 원본 머리 yaw 약39도이며 180도 후방은 별도 임시 포즈 검증. 발 보행·접지와 공격 입력 매핑 추가 없음. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_head-attached-camera.md`, `Feature/doc/2026-09-22_head-attached-camera.md`, `DevLog/20260922.txt`.


## Sprint#4-34 - Boxing.fbx 기반 권사 자세와 공격 (2026-09-22)

- 브랜치: `Sprint#4-34-boxing-reference-stances`
- 목표: 실제 복싱 동작과 여러 격투 자세를 참고하여 권사 가드·전신 공격을 개선한다.
- 주요 변경: 활성 스택 왼발 전진/왼손 직선 분석 도구, 기본 복싱·롱가드·골반 회전형 프리셋, 반대손 보호·팔꿈치 모음·골반 참여·지지발 보정과 이동/공중 보간, 비교 촬영.
- 검증: UE5.8.2 빌드30.86초 성공. 세 자세 각480·너클120/시야3·팔전용390프레임 실패0. 실제 손 정점/피해 차이 최대1프레임. 독립 소스/시각 검토, 원본과 공유자산4파일 보존, 경로·diff 검사 통과.
- 상태: 구현·통합 검증 완료. 세 자세 각각6.67초 실게임 영상 및 원본 골격 영상. 기존 C++ 절차형 공격 보완이며 FBX 직접리타깃·선택UI·지형IK 추가 없음. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_boxing-reference-stances.md`, `Feature/doc/2026-09-22_boxing-reference-stances.md`, `DevLog/20260922.txt`.

## Sprint#4-35 - Boxing 원본 직접 적용과 머리 카메라 미리보기 (2026-09-22)

- 브랜치: `Sprint#4-35-boxing-direct-head-preview`
- 목표: Boxing.fbx 원본을 플레이어 체형에 적용해 머리 카메라로 보는 실제 결과를 제공한다.
- 주요 변경: 격리된 Mixamo→Manny 리타깃 자산·검증 스크립트, 원본 프레임0 눈 위치와 감쇠 없는 머리 추종 촬영 fixture, 정면/아래40도·UI제외 비교 영상.
- 검증: 최종 UE5.8.2 빌드7.86초 성공. 정면278·최종보조278프레임 실패0. 머리추종 오차0cm/0.000002도·제한도달0, 원본 타격13프레임 일치·손/골반/발 궤적보존. 독립 소스/시각검토, 원본/공유자산4파일 보존 및 경로·diff 검사 통과.
- 상태: 직접 적용 미리보기 완료. 낮은 원본 가드로 정면 손 가독성은 낮고 보조 시점에서 동작 확인 가능. 기존 공격 입력/피해·일반 카메라 기본값 변경 없음. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_boxing-direct-head-preview.md`, `Feature/doc/2026-09-22_boxing-direct-head-preview.md`, `DevLog/20260922.txt`.

## Sprint#4-36 - 일반 Idle과 복싱·권사 공격 연결 (2026-09-22)

- 브랜치: `Sprint#4-36-relaxed-idle-martial-combat`
- 목표: 손을 내린 평상시 Idle과 복싱·권사 공격을 자연스럽게 연결한다.
- 주요 변경: 전신 맨손 전용 Idle/전투 가중치, 손·팔꿈치 경로 IK와 손목·손가락 보간, 최초 타격/콤보 준비 충돌 방지, 대기/복귀/재입력 검증과15초 실제 게임 영상.
- 검증: UE5.8.2 빌드11.69초 성공. 시연453·재입력513·기존격투480·너클120/시야3·팔전용390프레임 실패0. 손 이동최대16.937cm·팔 길이오차0, 피해오차1프레임이내. 독립 소스/시각 검토, 공용 자산4파일 보존 및 경로·diff 검사 통과.
- 상태: 구현·통합 검증 완료. 새 Mixamo 미리보기는 브라우저 초기화 오류로 직접 재생 불가하여 기존 Boxing.fbx 분석을 참고했으며 신규 에셋 리타깃은 하지 않았다. 기존 입력/피해/카메라 기본값 유지. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_relaxed-idle-martial-combat.md`, `Feature/doc/2026-09-22_relaxed-idle-martial-combat.md`, `DevLog/20260922.txt`.


## Sprint#4-37 - 머리 카메라 기본 시선 30도 조정 (2026-09-22)

- 브랜치: `Sprint#4-37-forward-head-framing`
- 목표: 선호한40도 구도에서 시선을10도 올리고 실제 시작 시선과 자유 입력을 함께 유지한다.
- 주요 변경: 로컬 전신 수평 스폰 최초 -30도 적용, 캐릭터별 처리 상태, 입력·무기 전환 유지 검사, 원본40/30도 비교 영상.
- 검증: UE5.8.2 최종 빌드11.22초 성공. 초기90·원본촬영278프레임 및 머리 회귀1,446합성샘플 실패0. 독립 소스/시각 검토, 공용자산4파일 및 생성 경로·diff 검사 통과.
- 상태: 구현·통합 검증 완료. 실제 시작 방향 변경이며 고정 하향 카메라가 아니다. 비교 원본의 눈 위치 보정은 촬영용이며 게임HUD·애니메이션·FOV는 유지. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_forward-head-framing.md`, `Feature/doc/2026-09-22_forward-head-framing.md`, `DevLog/20260922.txt`.


## Sprint#4-38 - Boxing 기본 공격 연결 착수 (2026-09-23, 09-26 정리)

- 브랜치: `Sprint#4-38-boxing-basic-attack`
- 목표: Boxing 미리보기 동작을 기본 공격에 연결한다.
- 주요 변경: 기존 First Person Mesh 경로의 원본 동작 연결 및 production 원본 클립 준비.
- 검증: 당시 Editor 빌드 완료. 실행 검증은 사용량/도구 제약으로 완료하지 못했다.
- 상태: 후속 사용자 요청에 따라 표시 경로를 Sprint#4-39의 Appearance 기반 실제 몽타주 방식으로 대체. 이 항목 자체의 런타임 검증 완료로 기록하지 않는다. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-23_boxing-basic-attack.md`, `Feature/doc/2026-09-26_appearance-head-camera.md`.

## Sprint#4-39 - Appearance Mesh 머리 카메라 (2026-09-26)

- 브랜치: `Sprint#4-39-appearance-head-camera`
- 목표: 실제 외형의 머리에 카메라를 두고 원본 전신 모션을 본다.
- 주요 변경: Appearance raw head pose 카메라, 기존 Source/Retarget ABP 유지, First Person Mesh 표시·평가 비활성화, 실제 좌우 Boxing 몽타주 및 좌클릭 연결, 실제 시야 기준 공격·외형 기준 이펙트.
- 검증: UE5.8.2 최종 빌드9.40초 성공. 애셋 제작·저장, 게임570프레임·cache570샘플 실패0. 위치0cm/회전0.00000382도 오차. 공유애셋7개 보존, 소스/시각 검토·생성경로·diff 검사 완료. 실제19초 영상.
- 상태: 지정 범위 QA 완료. 원본 스트레이트 머리 회전으로 정면 표적을 빗맞히는 동작은 독립 collision 질의로 확인했으며 그대로 유지한다. 최초 모든공격명중 가정의 실패로그 보존. 큰 시야 회전·팔 가림이 있고 발 전체 가독성은 제한. 생명주기/외형교체/멀티플레이 개별실행 검증 제외. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-26_appearance-head-camera.md`, `Feature/doc/2026-09-26_appearance-head-camera.md`, `DevLog/20260926.txt`.


## Sprint#4-40 - V키 3인칭 전신 확인 (2026-09-28)

- 브랜치: `Sprint#4-40-third-person-inspection`
- 목표: 현재 외형과 모션을 V키로 3인칭에서 확인하고 다시1인칭으로 복귀한다.
- 주요 변경: 최종화면POV 관찰전환,300cm거리·캡슐중심0cm높이,12cm벽스윕,머리/외형파츠 가림복원,UI입력차단. 실제머리카메라·공격기준과FP비활성 유지.
- 검증: 최종UE5.8빌드12.43초 성공. 실제V입력왕복3회,280프레임실패0. 1인칭135/3인칭145샘플,머리위치0cm/회전0.00000418도오차,좌우잽각12재생샘플,벽10샘플300→128cm와복귀,UI차단검사. 독립소스/시각검토·경로·diff검사완료.9.33초실행영상.
- 상태: 지정범위QA완료. 공격중정확한토글순간/사망/외형교체/재빙의/네트워크 개별실행 제외. 최초몽타주회복구간검사오류와물리마우스간섭실패로그보존. 관찰시점과공격조준은시차가있다. 커밋·푸시없음.
- 관련 문서: `Feature/architect/2026-09-28_third-person-inspection.md`, `Feature/doc/2026-09-28_third-person-inspection.md`, `DevLog/20260928.txt`.


## Sprint#4-41 - 미사용 임시 파일 정리 (2026-09-28)

- 브랜치: `Sprint#4-41-unused-file-cleanup`
- 목표: 현재 참조와 복구 가능성을 확인하여 불필요한 파일을 정리한다.
- 주요 변경: 재생성 PNG2,781개와 중복본50개, 총2,831개/약2.55GB 삭제. 검증된 출력 폴더10개의 PNG Git 제외 규칙, 읽기 전용 감사 도구와 삭제·복구 매니페스트 추가.
- 검증: 원본·스크립트로 전 프레임 RGB 재생성 일치, 최종 영상 스트림·프레임 수 확인. 보존 의존성2,858개 전후SHA256일치, 삭제 영수증과 대상 부재 확인. 경로/diff검사. 게임 코드·애셋 변경 없어 UE 재빌드 생략.
- 상태: 정리 완료. 참조가 남은 기존FP코드·콘솔도구·원본·애셋·백업과 대응 최종 영상이 없는278프레임 보존. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_unused-file-cleanup.md`, `Feature/doc/2026-09-28_unused-file-cleanup.md`, `DevLog/20260928.txt`.

## Sprint#4-42 - 캐릭터 애니메이션 왜곡 안정화 (2026-09-28)

- 브랜치: `Sprint#4-42-animation-stability`
- 목표: 몸 흔들림과 손발 변형의 최초 발생 단계를 확인하고 기술적 왜곡을 수정한다.
- 주요 변경:6종 실제root 분리·추가IK 비활성·보조체인 오매핑 해제, Source FootIK→공격Slot 순서 교정, 오른잽 하체 보존·가슴CS 회전·손IK 동기화, 전용 훅 진입blend0.10초. 생성기 재발 방지와 단계별 감사·runtime 기록 도구 추가.
- 검증: UE5.8 빌드8.40초, 통합450프레임/실패0, V키280프레임/실패0. 연결길이비1.000 복원, 왼잽 오른발step150.812→17.183도, 원본→Source 발회전 오차179.214→0.0001도 미만. 걷기187.74cm·점프89.99cm·착지 확인. 독립리뷰2건, 공유원본SHA 보존, AST/diff/Harness경로검사,15초 실행영상 검증.
- 상태: 확인된 기술적 왜곡 수정 및 지정범위 QA완료. 원본 훅의 빠른 큰 회전은 유지. Human Male 실제검증, 다른5종 개별영상·경사지 접지·네트워크·전체무기/회피 회귀 제외. 카메라 감쇠·입력/피해/차징 변경 없음. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_animation-stability.md`, `Feature/doc/2026-09-28_animation-stability.md`, `DevLog/20260928.txt`.


## Sprint#4-43 - 머리를 포함한 전신 그림자 복원 (2026-09-28)

- 시작 브랜치: `Sprint#4-43-full-body-shadow`. 공유 체크아웃의 후속 작업 브랜치 전환은 보존.
- 목표: Appearance 머리 카메라에서 머리를 가리면서 전신 그림자는 온전히 유지한다.
- 주요 변경: raw pose 기반 shadow-only Poseable Mesh, LeaderPose 얼굴·머리카락 동기화, 원본 CastShadow 보존, V/비활성 복원과 오래된 proxy 정리. 별도 FP 메시 비활성 유지.
- 검증: UE5.8 빌드7.28초 성공, 그림자390프레임/실패0·포즈330샘플/위치오차0cm·공격91샘플/4종, 동일자세 전후 머리 실루엣 확인, V 회귀280프레임/실패0. 독립 소스/런타임 검토, diff·Harness 경로 검사.
- 상태: 현재 Human Male/VSM 범위 완료. RT 그림자·반사, 전 외형·파츠 교체의 개별 실행, GPU 성능 측정 제외. 이번 애셋 편집·커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-28_full-body-shadow.md`, `Feature/doc/2026-09-28_full-body-shadow.md`, `DevLog/20260928.txt`.

## Sprint#4-44 - Rogue10m 개인 스튜디오 (2026-09-28)

- 브랜치: `Sprint#4-44-personal-studio`
- 목표: 개발 일정과 현재 상황, 개선·수정 내역을 한곳에서 확인하고 관리한다.
- 주요 변경: 실제 Sprint/DevLog/Feature 연결, 현재 브랜치 현황, 주간 일정, 작업 추가·수정·삭제 및 로컬 파일 저장, 검색·원문 조회·일정 백업, 실행기와 반응형 한국어 대시보드.
- 검증: Node 구문·통합 테스트 4건, Edge 실제 UI 5개 뷰와 일정 CRUD/재접속/백업/검색/원문 열기, 390px 모바일 및 200% 글자 확대, 브라우저 오류 0건. 실제 서버 HTTP 200과 기록 연결 확인. 게임 코드·에셋 변경 없어 UE 빌드 생략.
- 상태: 로컬 구현 및 지정 범위 QA 완료. 다른 PC 접속·외부 호스팅은 미제공. 문서의 과거 검증과 현재 실행 상태를 구분한다. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_personal-studio.md`, `Feature/doc/2026-09-28_personal-studio.md`, `Studio/README.md`, `DevLog/20260928.txt`.

## Sprint#4-45 - 스튜디오 일정 검토·실행 연결 (2026-09-28)

- 브랜치: `Sprint#4-45-studio-task-execution`
- 목표: 등록된 개발 일정을 검토하고 적절한 순서로 실제 작업한 뒤 결과를 스튜디오에 반영한다.
- 주요 변경: 현재 채팅 5분 heartbeat, 시작 시각·선행 작업·우선순위 대기열, 단일 실행 claim과 파일 잠금, 진행 로그·산출물·검증 결과, 일시정지·중단·재검토, 실행 중 편집 차단, 15초 화면 갱신.
- 검증: Node 9개 검사, 독립 프로세스 중복 claim 차단, 기존5뷰 회귀 및 새 실행 UI 경로·문서 열기·390px 모바일 통과. 예약 ACTIVE 확인. 기존 등록 작업을 실제 조사·기획·검증·기록 경로로 처리.
- 상태: 실행 연결 및 지정 범위 QA 완료. PC/Codex 앱 구동 필요. 중단은 안전한 작업 경계에서 반영하며 커밋·푸시·배포는 자동 수행하지 않는다.
- 관련 문서: `Feature/architect/2026-09-28_studio-task-execution.md`, `Feature/doc/2026-09-28_studio-task-execution.md`, `Studio/EXECUTION.md`, `Studio/README.md`.

## Sprint#4-46 - 무기별 애니메이션 기획 작성 (2026-09-28)

- 브랜치: `Sprint#4-46-weapon-animation-plan`
- 목표: 스튜디오에 등록된 무기별 애니메이션 기획 작업을 실제로 실행한다.
- 주요 변경: 무기12유형의 대기·기본·특수·차징 동작, 공용/총기 대체 모션 교체 요구, 최신 Appearance 카메라·리타깃·그림자 계약, 데이터 연결, 제작 패킷과 QA 기준 문서.
- 검증: 실제 enum12개와 기획12행 일치, 근거 경로7개 존재, 생성기11종/5슬롯 및 맨손 전용 입력 대조, 기존 구현과 신규 제안 구분 확인. 문서 작업이므로 UE 빌드 생략.
- 상태: 기획 작성 완료. 신규 애니메이션·게임 코드·에셋 적용 및 전무기 실게임 QA는 수행하지 않음. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_weapon-animation-plan.md`, `Feature/doc/2026-09-28_weapon-animation-plan.md`, `DevLog/20260928.txt`.

## Sprint#4-47 - 스튜디오 즉시 에이전트 작업 흐름 (2026-09-28)

- 브랜치: `Sprint#4-47-studio-agent-workflow`
- 목표: 예정 등록을 계기로 AI가 역할을 배분하고 진행 중·검증 대기·완료를 관리한다.
- 주요 변경: 기존 heartbeat 중지, HTTP 저장 즉시 dispatcher 실행, 기획·최대2개 독립 구현·빌드·독립 리뷰·문서·최종 리뷰, 실제 Codex CLI, 상태/역할 UI, 중단 자식 종료 대기, 실행 잠금과 실패 복구, 완료 근거 강화.
- 검증: Node19건, Edge5개화면과 작업 흐름/모바일/글자확대 통과. 실제 격리 CLI는 Windows deny-read ACL 오류로 blocked·일시정지·완료 미진입 확인. 구문/diff 검사. 게임 변경 없어 UE 빌드 생략.
- 상태: 구현 및 모의 통합 QA 완료. 실제 무인 성공 실행은 환경 오류로 미검증. 사용자가 오류 시 확인 필요로 중지를 선택하여 자동 승인 확장 미적용, 라이브 엔진 확인 필요·일시정지 유지. 기존 완료 일정 보존. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_studio-agent-workflow.md`, `Feature/doc/2026-09-28_studio-agent-workflow.md`, `Studio/EXECUTION.md`, `DevLog/20260928.txt`.

### Sprint#4-47 보완 — 샌드박스 복구 (2026-09-29)

- 목표: 실제 에이전트의 읽기 명령을 막던 Windows 샌드박스 오류를 복구한다.
- 변경: 사용자 승인으로 22바이트 NUL 손상 권한 상태를 백업 보존하고 정상 샌드박스 초기화로 재생성. 승인 정책과 오류 시 중지 유지. 엔진 안내를 복구 완료로 변경.
- 검증: 일반 샌드박스 파일 읽기 및 실제 Studio 실행기 read-only 에이전트 성공, 종료 코드0. 유효한 상태 JSON과 백업 해시 일치 확인.
- 상태: 샌드박스 복구 완료. 기존 작업의 확인 필요 기록·전체 일시정지는 유지하며 실제 개발 작업 재실행 및 전체 완료 검증은 이번 범위에 포함하지 않았다.
- 관련 문서: `DevLog/20260929.txt`, `Feature/doc/2026-09-28_studio-agent-workflow.md`.


## Sprint4 종료 — 로컬 main 반영 (2026-09-29)

- 사용자 main 저장 요청에 따라 누적 작업1,332개 파일을 `8c9bb8a`로 저장했다.
- 목표: UI·전투·애니메이션·Appearance 카메라·그림자·Studio 및 개발 증거를 복구 가능한 상태로 보존한다.
- 검증: UE5.8 Editor 빌드 성공, Studio 루트 실행19개 테스트와 구문 검사 통과, staged diff와 Harness 경로 검사 통과. 개별 기능의 QA 한계는 기존 항목을 유지한다.
- 상태: 작업 브랜치 → develop → test → main 로컬 fast-forward 반영 완료. 후속 사용자 명시 승인으로 GitHub origin의 develop/test/main atomic push 성공(aa941d4), 원격 대기 해소. 다음 신규 작업은 main 병합 완료에 따라 `Sprint#5-1-<작업명>`부터 시작한다.
- 관련: `Feature/doc/2026-09-29_sprint4-main-save.md`, `Feature/doc/evidence/main-save-20260929/changed-files.tsv`, `DevLog/20260929.txt`.

## Sprint5

### Sprint#5-1 — 맨손·너클 애니메이션 제작과 1인칭 안정화 (2026-09-29)

- 브랜치: `Sprint#5-1-unarmed-knuckle-animation`
- 목표: 무기별 기획서의 맨손·너클 동작을 제작하고 머리 흔들림·정면 가독성·손발 본 정상성을 실제 게임에서 검증한다.
- 주요 변경: 맨손 4종·너클 5종 전용 Sequence/Montage 18개와 실제 DA 9개 연결. 오른잽/스트레이트 구분, IK 팔꿈치·양손 연타 연속성 보정, 오른손 타격 높이·어깨 방향 교정, 진입 블렌드 완화. 머리 회전 기여 15%·최대 8도, 초기 시선 -20도. 공유 원본·피해량·해금·히트 수 유지, 기존 너클 지연 0초만 접촉에 맞게 조정. 구형 생성기 4개에 새 연결 덮어쓰기 방지.
- 검증: UE Editor 빌드, 9클립·14접촉·48원본 해시, 6체형 각 840프레임, 자세 711,486행 구조 검사 PASS. Human Male 9종 실제 피해 14회 및 지연/연타 간격 검사 PASS. 초기 시야 90·V 관찰 280·Appearance 카메라 570프레임 회귀, Harness 경로·diff·Python 문법 검사 통과. 실제 실행 영상과 수치 증거 저장.
- 상태: 구현 및 명시 범위 QA 완료, 작업 브랜치 보존. 새 커밋·푸시 없음. 너클 장비 모델/모든 방어구 관통, 경사면·네트워크·사망/재빙의·극단 공격속도·RT 그림자는 미검증. Sprint5는 main 병합 전으로 유지한다.
- 관련 문서: `Feature/architect/2026-09-29_unarmed-knuckle-animation.md`, `Feature/doc/2026-09-29_unarmed-knuckle-animation.md`, `Feature/doc/evidence/fist-combat-20260929/`, `DevLog/20260929.txt`.

## Sprint#5-2 - 스튜디오 Git 조회 및 중단 안내 교정 (2026-09-29)

- 브랜치: `Sprint#5-2-studio-review-diagnostics`
- 목표: 기획 중단을 검증 실패로 오해하는 표시와 Windows sandbox Git 소유권 오류를 해결한다.
- 주요 변경: 현재 저장소로 한정한 명령별 Git 조회 지침, 경로 인용, 역할별 중단 안내, 기존 구현 우선 확인. 전역 신뢰·ACL·승인 정책 유지.
- 검증: Node22건 통과, 실제 read-only 역할의 Git status/branch 모두 종료0 및 브랜치 일치, diff 검토. 게임 변경 없어 UE 빌드 생략.
- 상태: Git 조사 및 단계별 안내 수정 완료. 기존 맨손·너클 구현 보존. 자동 작업 전체 완료는 별도 검증 대상. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-29_studio-review-diagnostics.md`, `Feature/doc/2026-09-29_studio-review-diagnostics.md`, `DevLog/20260929.txt`.

## Sprint#5-3 - 스튜디오 맨손·너클 작업 인계 및 완료 검증 (2026-09-29)

- 브랜치: `Sprint#5-3-studio-task-completion`
- 목표: 반복 확인 필요 작업을 기존 구현 인계·실제 검증·독립 검토 후 완료로 연결한다.
- 주요 변경: 원 작업 완료와 활성 충돌 없음 확인, 기존 미커밋 구현 인계 확정, 중복 제작 없이 스튜디오 검증 진행 기록. 기존 구현만으로 확인을 반복 요구하지 않는 coordinator 지침 보완.
- 검증: UE빌드 성공4.09초, 새 HumanMale 9종840프레임/실패0·실제14회 피해, 자세118,581행 구조 PASS/비유한0·머리회전 최대7.0021도. V280·카메라570프레임 회귀 실패0. 독립 소스/실행자료 리뷰와 원본48개 SHA 일치.
- 상태: 등록 요청 범위 완료 검증. 기존 게임 소스·에셋 보존, 커밋·푸시 없음. 나머지5체형은 기존 증거 확인이며 이번 재실행 아님. 장비별 관통·네트워크 등 기존 미검증 한계 유지.
- 관련 문서: `Feature/architect/2026-09-29_studio-fist-completion.md`, `Feature/doc/2026-09-29_studio-fist-completion.md`, `Feature/doc/evidence/studio-fist-completion-20260929/`, `DevLog/20260929.txt`.

## Sprint#5-4 - 격투가 스킬트리와 테스트 해금·키 설정 (2026-09-30)

- 브랜치: `Sprint#5-4-fighter-skill-tree`
- 목표: 맨손/너클 격투가 스킬을 기존 K 스킬트리에 연결하고 테스트 전체 해금·스킬별 키 설정을 제공한다.
- 주요 변경: 공용14노드(공격13·심법1), 세션 테스트 해금과 Shipping 제외, Enhanced Input 키 지정/해제·로컬 SaveGame·충돌 검사, 차징/공중/쿨다운/GAS 검증·메뉴/정지/무기/키 변경 취소, 실제 위젯·설명 겹침 개선, Editor 구성 및 전투 값 보존 검사.
- 검증: UE5.8 Editor 빌드10.44초 성공, Editor 두 프로필14노드/위젯 PASS, 실제 게임107항목/13개 공격 발동·몽타주 실패0, 새 프로세스 키 복원2항목 PASS, 기존11무기 전환 PASS, 9종840프레임·실제14회 피해 PASS, 자세118,581행 구조PASS/비유한0·머리회전7.0021도. 실제1920×1080 캡처와 독립 교차 리뷰, Python 문법·diff 검사.
- 상태: 구현 및 명시 범위 검증 완료. 기존 미커밋 동작·카메라·Studio 변경을 보존했으며 커밋·푸시 없음. 전체13개 공격의 모든 상황별 피해·장비관통·온라인 복제와 영구 수련 저장은 범위 밖.
- 관련 문서: `Feature/architect/2026-09-30_fighter-skill-tree.md`, `Feature/doc/2026-09-30_fighter-skill-tree.md`, `Docs/SkillTreeLoadoutGuide.md`, `Feature/doc/evidence/fighter-skill-tree-20260930/`, `DevLog/20260930.txt`.

## Sprint#5-5 - 내장 AI Assistant 웹 인증 오류 처리 (2026-09-30)

- 브랜치: `Sprint#5-5-editor-ai-assistant-error`
- 목표: 사용자 로그의 Cloudflare iframe 스크립트 차단·IsAvailable 취소 오류를 프로젝트에서 처리한다.
- 주요 변경: 프로젝트 AIAssistant 플러그인만 비활성화하고 내장 Epic AI 창의 웹 연결을 중지. Studio CLI·Unreal MCP·AllToolsets·게임 기능 설정 보존, 적용·복원 안내 추가.
- 검증: 프로젝트 JSON·필수 플러그인 및 단일 설정 변경 확인, UE Editor 빌드120.41초 성공. 실제 새 Editor 시작·정상 종료0, AIAssistant 마운트/로그0건, MCP·AllToolsets·Python 마운트 확인. 관련 diff 검사 통과.
- 상태: 요청된 AI Assistant 오류 경로 처리 완료. 인증 서비스 자체의 수정은 아니며 내장 Epic AI 창은 비활성화된다. 변경 전부터 있던 엔진 초기화 Condition failed15건은 범위 밖으로 기록. 기존 변경 보존, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-30_editor-ai-assistant-error.md`, `Feature/doc/2026-09-30_editor-ai-assistant-error.md`, `Docs/UE58MCPSetup.md`, `Feature/doc/evidence/editor-ai-assistant-20260930/`, `DevLog/20260930.txt`.


### Sprint#5-6 — close-combat-animation-preparation

- 상태: 애니메이션 적용 준비·정적 QA 완료, 게임 통합·화면·표적 판정 미검증
- 목표: 보유 Close Combat의 훅·어퍼컷·킥·가드·방향 회피를 Manny Preview로 검토하고 안정화 결과 보존
- 주요 변경: 원본33패키지 동일 바이트 설치, Manny11시퀀스와 리그3개 생성, 새 Preview root lock, 준비/감사/분석 스크립트와 후보 문서
- 검증: Unreal exit0, 983포즈 구조·root lock·DA11계약 PASS, 기존5,630파일·VaultCache121파일 SHA256 보존, AST·Harness·diff 검사
- 한계: 직선 킥의 큰 회전·회피 덤블링/긴 스텝 보류, Rolls and Dodges 로컬 미확인, 패리와 타깃 판정 미검증
- 관련 문서: Feature/architect/2026-10-02_close-combat-animation-preparation.md, Feature/doc/2026-10-02_close-combat-animation-preparation.md


## Sprint#5-7 — 격투가 스킬별 애니메이션 연결 (2026-10-02)

- 목표: 현재14노드의 실제 스킬마다 의미에 맞는 동작을 연결하고 안정화/밸런스 보존.
- 변경: 기존9개 유지, 유운수·금강쇄·무극패왕권 새 몽타주/파생 클립6개 및 DA3개의 몽타주/지연 필드만 적용. 태산압정은 소스 부족·기존 연결 유지, 금강불괴는 상시패시브.
- 검증: Editor14노드 전체 필드/에셋/root/limb PASS. 실제13개 키 발동 확인. 늦은 유운수 몽타주1항목 미통과. 기존9개 실제 정면타깃·머리카메라840프레임 회귀 PASS.
- 상태: 가능한12개 적합 연결 저장 완료. 실제 렌더·새3개 개별 접촉 미검증, 태산압정 소스 필요. 셰이더 준비 제한 후 NullRHI 최소검사 사용. commit/push 없음.
- 문서: Feature/architect/2026-10-02_fighter-skill-animation-binding.md, Feature/doc/2026-10-02_fighter-skill-animation-binding.md.


### Sprint#5-7 후속 검사 완료

- 유운수 미통과는 정상 블렌드 아웃 후의 늦은 활성 검사였다. 에셋 변경 없이 실제 수락 실행의 몽타주 가중치/시간 진행 관측으로 보강.
- 최종 전체검사120항목/13개키실행 PASS. 새3개 production DA는135cm 정면타깃 각피해1회·한 프레임 이내 시간오차·기존카메라상한 PASS.
- 검사 C++2파일/실행 스크립트만 변경, 에셋·전투/카메라/밸런스 보존. 승룡권은 기존 Knuckle_Uppercut 유지.
- 실제 렌더/스킨 접촉 미검증과 태산압정 소스 부족 유지. 근거: 스킬별 연결 결과 문서 후속진단 및 followup-validation.json. commit/push 없음.

## Sprint#5-8 — 격투가 팔 휘두르기 강화 (2026-10-05)

- 브랜치: `Sprint#5-8-fighter-arm-swing`
- 목표: 기본 맨손·너클·금강쇄 팔 움직임을 더 크게 만들고 타격/손발/머리 시야 안정성을 보존한다.
- 주요 변경: opt-in AuthorFistSwing, 후방 준비·회수/훅 원호/어퍼컷 낙폭 강화, 접촉 주변 샘플 키 양팔 보호, 금강쇄 FK 기반 IK 재동기화, 시퀀스10개 Editor 저장, 기준선·재로드·실제 게임 검사 도구와 영상.
- 검증: Editor 빌드 성공, 원시120Hz10종 PASS, 실제841프레임/118,581포즈·14회피해 PASS, V580/260샘플 PASS, StoneFist3종1,234검사 PASS, 렌더840프레임 PASS. 실제Appearance 손 경로1.03~9.62% 증가, 카메라전후7.002도 유지. 허용10개외 Content4,205개 같은바이트 확인. 독립 코드/실행 검토 PASS.
- 상태: 요청 범위 적용·저장·실행 검증 완료. 원시/실제 속도 수치 구분, HUD/모션블러로 미세 손가락·발 전체 육안 확인 제한. 기존 미커밋 작업 보존, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_fighter-arm-swing.md`, `Feature/doc/2026-10-05_fighter-arm-swing.md`, `Feature/doc/evidence/fighter-arm-swing-20261005/`, `DevLog/20261005.txt`.
## Sprint#5-9 — 격투가 전투 대기 자세 (2026-10-05)

- 브랜치: `Sprint#5-9-fighter-combat-stance`
- 목표: 기존 손을 내린 일반 Idle과 구분되는 맨손·너클 가드를 실제 Appearance Mesh에 연결한다.
- 주요 변경: 기존 공격 시작 포즈의 2초 호흡 루프, native AnimInstance 부모와 FootIK→자세 블렌드→DefaultSlot 연결. 공격·차징·회피 후 3초 유지, 진입 0.20초/복귀 0.45초. 이동·공중 우선, 무기·메뉴·사망·조종 해제 정리. 기존 FootIK 준비기의 새 연결 인식 보강.
- 검증: Editor 빌드, 재로드 원시241포즈, 통합640프레임·90,381포즈·렌더640프레임 PASS. 평균 외형 손 높이24.90cm 상승, 실제 발 높이 차이2.9345cm, 카메라최대7.0144도. FistCombat9종·피해14회·V580/260샘플 및 StoneFist3종1,234검사 PASS. 독립 코드·모션·선택 화면 검토, Python문법·Harness·diff 검사 통과.
- 보존: 기존4,186개 uasset 중 공통AnimBP1개만 수정, 신규가드1개 생성, 나머지4,185개 같은SHA256. 기존 강화공격10종·DA·몽타주·카메라 설정 보존.
- 상태: 요청 범위 구현·저장·실행 검사 완료. 21.33초 무음 영상 첨부. 실제 1인칭 오른손·3인칭 오른발 일부는HUD에가려짐; 이동가드레이어·별도 사망/재빙의 실행·전체체형/장비관통·온라인 미검증. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_fighter-combat-stance.md`, `Feature/doc/2026-10-05_fighter-combat-stance.md`, `Feature/doc/evidence/fighter-combat-stance-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-10 — 기본맵 기반 시작 마을 (2026-10-05)

- 브랜치: `Sprint#5-10-start-village`
- 목표: 기본 플레이맵을 복제하고 보유 Stylized Village 에셋으로 시작 마을을 제작하여 메뉴·준비 구역·전투 포탈을 연결한다.
- 주요 변경: 신규 StartHub 120m × 120m, 집 외관 10채·우물 광장·네 방향 길·준비 구역 4곳·남쪽 PlayerStart·북쪽 포탈·외곽 BlockAll 경계. 신규 소유 액터 667개와 머티리얼 12개. 메뉴 이동 목적지 StartHub·StartRun 옵션 제거, 포탈 후 기존 전투맵 런 시작. 공식 MCP JSON-RPC 클라이언트와 생성·검사 도구, 자동 시작·8001 포트 기본값.
- 검증: UE5.8 Editor NoHotReload 빌드·CheckGeneratedChanges PASS. 메뉴 정적·실제 PIE PASS. 허브/포탈 50항목 PASS, 시작점 실제 접지·허브 1200초 대기·2초 보행 약 11.50m·실제 native 포탈 overlap·전투 1200→1198초 확인. Python AST 검사와 Unreal API 노출 리뷰 완료.
- 보존: 기존 Content 4,302개 크기·수정 시각 동일, 선정 70개 SHA256 동일. 신규 Content 720개는 새 맵·전용 콘텐츠·해당 외부 액터/객체 범위 안에 있음. 기존 전투·애니메이션 미커밋 작업 보존.
- 상태: 공간 제작·저장·메뉴 연결·실제 런타임 검사 완료. 최종 전체/플레이어 화면과 독립 배치 검토, 명시적 서버 시작 없는 재기동·8001 자동 연결·StartHub 재로드 및 공식 MCP 저장 확인 완료. NPC 서비스·실내·대규모 오픈월드 확장·성능/패키징/온라인 검사는 미구현 또는 범위 밖. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_start-village.md`, `Feature/doc/2026-10-05_start-village.md`, `Feature/doc/evidence/start-village-20261005/`, `Docs/UE58MCPSetup.md`, `DevLog/20261005.txt`.

## Sprint#5-11 — 오픈월드 플레이맵 (2026-10-05)

- 브랜치: `Sprint#5-11-open-world-playmap`
- 목표: 기본 World Partition 맵과 보유 지형·환경 에셋으로 실제 탐험·전투 가능한 플레이맵을 제작하고 시작마을 포탈에 연결한다.
- 주요 변경: 신규 OpenRunMap 1800m × 1800m Landscape·256지형/충돌 컴포넌트·16공간 프록시,6개 지역·711길 패치·교량3개·야영지·풍차·북쪽 유적. 공식 PCG 그래프1개·183타일·나무2806개. 맵 전용 동적 Navigation invoker·256m 셀/700m 로드, 지역 스포너5개·몬스터 자식 Blueprint6종·고정 보스 접지. StartHub 포탈 목적지와20분 런 연결, 전용 강 재질과17패널 저장. PCGPrimitives 활성화, editor 한정 Landscape helper와 재현/QA 도구.
- 공용 수정: 몬스터 지면 스폰에 캡슐 반높이+2cm 및 소유 스트리밍 레벨 적용. AI 도달 검사에서 이동자·목표 양쪽 반경을 제외한 중심 거리 계약으로 공격 범위 밖 정지 방지. 원본 전투 데이터는 보존.
- 검증: 최종 Editor NoHotReload 빌드14.59초 성공, 프로젝트 파일 생성·재로드 PASS. 실제 포탈·1200초 런·지역/교량 보행·내비게이션·야영지 unload/reentry·보스 접지·native 몬스터 추적/피해99항목 PASS. 별도 짧은 사거리2종 경계 회귀30항목 PASS, 플레이어 정지 상태 실제 접근·피해 확인. 공식 MCP 실제 화면4개·명시적9패키지 저장·10패키지 미저장 상태false. Python AST13개·프로젝트 JSON·diff·CheckGeneratedChanges PASS.
- 보존: 기준선 Content5022개 전체 크기·SHA256 검사, StartHub 맵/포탈 외부 액터2개만 허용 변경, 나머지5020개 동일. 신규1268개는 OpenRunMap·전용 콘텐츠·해당 외부 액터/객체 경로 안에 있음. 기존 미커밋 작업 보존.
- 상태: 초기 플레이맵 제작·저장·명시 범위 검증 완료. 보스 처치 승리 연결·상세 미술/NPC/실내·전체20분 종료·전체 경로 연속 순회·다른 맵의 공용 AI/스폰 회귀·패키징/성능/온라인은 후속 범위. Sprint5 유지, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_open-world-playmap.md`, `Feature/doc/2026-10-05_open-world-playmap.md`, `Feature/doc/evidence/open-world-20261005/`, `Docs/WorldStructure.md`, `DevLog/20261005.txt`.

## Sprint#5-12 — 시작마을 바닥 텍스처 개선 (2026-10-05)

- 브랜치: `Sprint#5-12-start-village-ground`
- 목표: 잔디·흙길 텍스처의 뭉개짐과 방향성 늘어짐을 수정하고 광장 바닥 질감을 개선한다.
- 주요 변경: 마을 전용 World XY 재질, 잔디180cm·흙길90cm·돌120cm 반복. 기존 인스턴스2개 부모 교체, 광장·준비 구역·포탈 바닥6개 돌 재질 연결. 생성기 튜닝 재사용·읽기 전용 사전 검사, 조사/수정/캡처/재로드/Content 감사 도구.
- 검증: native·공식 MCP 재질 컴파일 및 배선 PASS, 명시5패키지 저장·dirty=false, 맵 해제·재로드43항목 PASS. 기존677액터 배치·메시·충돌·포탈 보존, Content6290개 전체 SHA256에서 허용9개 외6281개 동일·신규2개. 실제 전후·동서 길·마을 화면 검토, 생성기 실제 사전 검사·Python AST·diff·하네스 경로 검사.
- 상태: 요청 범위 적용·저장·재로드 검증 완료. 원본 에셋과 그래픽 전역 설정 보존. 재질 전용 변경으로 C++ 빌드·PIE 전투 재검사·패키징/성능 측정 미실행. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_start-village-ground.md`, `Feature/doc/2026-10-05_start-village-ground.md`, `Feature/doc/evidence/village-ground-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-13 — 제작된 오픈월드 탐험 지도 (2026-10-05)

- 브랜치: `Sprint#5-13-open-world-map`
- 목표: 완성된 OpenRunMap을 실제 배치와 일치하는 한국어 지도로 제공한다.
- 주요 변경: 실제R16 지형 음영·10m등고선,6지역·3길·유한60m강·교량3개·조우/보스·입장 표시, 별도StartHub 포탈 삽입도·방위/500m축척. 2200×2550PNG·좌표/근거JSON·재현 렌더러와 문서.
- 검증: 독립 좌표 검토, 제작POI/교량 일치·PCG183타일 완료, R16기준5점 양자화오차1cm이내·라벨범위/비중첩·실제PNG 육안 검토, Python AST·PNG무결성·diff 검사.
- 상태: 지도 제작 완료. 수목/건물은 지도 기호이며 개별 인스턴스 정사영 캡처가 아니다. Unreal 콘텐츠·C++·설정 변경과 빌드/PIE 실행 없음, 기존 작업 보존·커밋/푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_open-world-map.md`, `Feature/doc/2026-10-05_open-world-map.md`, `Feature/doc/evidence/open-world-map-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-14 — 기본 몬스터와 시작마을 안내 NPC (2026-10-05)

- 브랜치: `Sprint#5-14-open-world-monsters`
- 목표: 오픈월드 기존 6종에 Idle·이동·기본 공격을 구현하고 시작마을에 간단한 대화와 기본 아이템 지급 NPC를 배치한다.
- 주요 변경: 타이머 기반 기본 근접 공격 Profile/Component와 opt-in AI 연결, 보유 6종 모델·5개 동작 클립 및 전용 DataAsset/Profile 12개 연결. 원본 능력치·보상·스포너 정책 보존. 길잡이 아린의 F 대화·기존 회복 포션 2개 지급·중복 방지·만석 재시도, 지급 스택의 실제 잔여 수량만 OpenLevel 포탈 뒤로 전달. NPC native 카드 RebuildWidget 표시 수정, 전용 BP/DA·StartHub 액터 저장 및 검사 도구.
- 검증: Editor NoHotReload 빌드 111.69초 및 표시 보완 70.11초 성공. 저장 재로드 100/100, 최종 실제 몬스터 PIE 219/219와 NPC PIE 57/57 통과. 6종 Idle·이동·native 추적/피해·회피/사망 취소, 물약 실제 사용·실제 포탈 후 잔여 1개·1200초 런 확인. 실제 F 열기/지급/닫기와 한글 카드 화면 2개, 독립 코드·화면 검토·문법/JSON/diff/하네스 검사.
- 보존: 기준선 Content 6,292개 전체 SHA256에서 허용 기존 10개 변경·신규 15개, 6,282개 동일. 기존 마을 677액터·원본 25종·구매 모델/클립/재질·다른 미커밋 작업 보존.
- 상태: 요청 범위 제작·저장·최종 집중 검사 완료, 에디터 StartHub 대기. 세션 단일 플레이어 보급이며 특성 강화·보스 특수 패턴/승리 목표·전체 20분·네트워크·패키징/출시 성능은 후속 범위. Sprint5 유지, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_open-world-monsters.md`, `Feature/architect/2026-10-05_start-hub-tutorial-npc.md`, `Feature/doc/2026-10-05_open-world-monsters.md`, `Feature/doc/2026-10-05_start-hub-tutorial-npc.md`, `Docs/OpenWorldMonsterDesign.md`, `Feature/doc/evidence/open-world-monsters-20261005/`, `DevLog/20261005.txt`.

### Sprint#5-14 추가 — 길잡이 아린 시각 콘셉트

- 목표: 시작마을의 대화·출발 보급 역할에 맞는 NPC 이미지를 제작한다.
- 주요 변경: imagegen 내장 도구로 친근한 성인 안내인·초록 여행복·가죽 가방·물약 두 병·밝은 기와 마을의 전신 콘셉트 1024×1536 PNG를 생성하고 프로젝트에 사본·프롬프트를 저장했다.
- 검증: 인물·손·물약·전신 구도 육안 검토, PNG 형식/크기와 원본·사본 SHA256 일치, diff 검사.
- 상태: 이미지 제작 완료. 시각 콘셉트와 문서만 변경했으며 엔진 에셋·C++·설정·3D 모델·메시 교체는 이번 범위에 없음. 빌드/PIE 재실행과 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_start-hub-tutorial-npc.md`, `Feature/doc/2026-10-05_start-hub-tutorial-npc.md`, `Feature/doc/evidence/start-hub-tutorial-npc-image-20261005/`, `DevLog/20261005.txt`.

### Sprint#5-14 후속 — 아린 FBX 제작·적용 준비

- 목표: 기존 아린 콘셉트를 실제 리깅된 FBX로 제작하고 StartHub NPC에 적용한다.
- 준비한 변경: 동일 외형의 흰 배경·무그림자 정면 전신 T포즈 참조와 프롬프트, A1~A4 패킷, 새 Skeleton용 Idle 임포트/리타깃 경로 문서. 기존 NPC와 엔진 콘텐츠는 유지했다.
- 검증: 실제 NPC Skeleton 일치 조건·기존 FBX/IK Retarget 코드 조사, 참조 PNG/원본 사본 해시·육안·diff/하네스 검사. Tripo 공식 인증·리깅·FBX 명세 확인.
- 상태: 참조 준비 완료, TRIPO_API_KEY 연결 대기. 유료 생성 태스크·잔액 조회·실제 FBX·새 메시·맵 적용은 미실행이며 완료로 처리하지 않는다. 빌드/PIE는 실제 적용 시 수행. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_arin-npc-fbx.md`, `Feature/doc/2026-10-05_arin-npc-fbx.md`, `Feature/doc/evidence/arin-npc-fbx-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-15 — 하단 전투 HUD 시안 적용 (2026-10-05)

- 브랜치: 작업 시작 시 `Sprint#5-15-compact-combat-hud` 생성·전환. 공유 체크아웃은 병행 작업에서 이후 `Sprint#5-14-open-world-monsters`로 전환됨.
- 목표: 승인된 imagegen 시안의 낮은 검정·청동 HUD를 실제 UMG에 적용하고 경험치·레벨·획득 알림을 정리한다.
- 주요 변경: 1200×196 코어·하단20·양옆24, 패널 내부 경험치, 실제 무기명·잠금 기호·준비 강조·HOLD 두 줄, Compact 5파트와 Main/Bottom 연결. 좁은 화면에서 획득 카드가 코어 위로 이동. HUD C++·Editor 재현/검증·프레이밍 보호·실제 게임 QA 추가.
- 검증: UI 전체 Editor 빌드28.42초, 최종 별도 모듈 빌드25.40초 성공. 7패키지 validator PASS·공식 MCP dirty=false. 실제 StartHub 6해상도/DPI단계 922검사 PASS·실패0, PNG6개 크기/시각과 일반/좁은 화면 육안 확인. 독립 C++/UMG 리뷰·AST·PowerShell parse·diff·CheckGeneratedChanges·Harness 경로 PASS.
- 보존: 기존 UI112개 중 Main/Bottom만 수정, 나머지110개 SHA256 동일. 신규 Compact5개. 기존 전투/애니메이션/맵 작업과 일반 수량0 로그 필터 유지.
- 상태: 적용·저장·게임 재로드·검증·기록 완료. 패키징/온라인/장시간 성능 검사 범위 밖. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_compact-combat-hud.md`, `Feature/doc/2026-10-05_compact-combat-hud.md`, `Docs/CompactCombatHUDGuide.md`, `Feature/doc/evidence/compact-hud-20261005/`, `DevLog/20261005.txt`.

### Sprint#5-14 후속 — Apollo 아린 정적 3D·FBX 제작 및 적용 검사

- 목표: 사용자가 지정한 Apollo 스킬로 아린을 실제 FBX로 제작하고 검증을 통과하면 시작마을 NPC 외형에 적용한다.
- 주요 변경: 프로젝트 전용 Python·실제 Apollo API/stdio MCP·공식 체크섬 검증 portable Blender, 단일 이미지2.5D FBX·정사각3뷰 참조·원본 ApolloHullAdapter의 실제 입체 GLB·공식 MCP 업로드/preview/export/download·검사/포장 스크립트6개와 정적 입체 FBX·GLB·설명 패키지.
- 검증: 닫힌 표면·양의 체적·단일 연결 등11/11, 최종 FBX 실제 재수입 높이1.7m·24,880삼각형·뼈0·Action0, 원본GLB 보존 SHA256·manifest·ZIP CRC, 실제 정면/측면/사선·독립 검토·AST·하네스 경로. 두 팔 자세 리깅 검사는 최대 늘어남7.38/6.34배로 실패했고 사선 얼굴 중복·망토 투영 늘어짐을 확인했다.
- 상태: 정적 입체 초안 FBX 전달 완료. 게임용 리깅·Idle·NPC 적용은 실제 품질 불합격으로 미완료이며 기존 안내 NPC·대화·지급을 유지했다. 업로드 score1.0/ready를 NPC 품질 승인으로 해석하지 않는다. CPU local_draft 출처와 실패 근거 동봉, C++/엔진 콘텐츠 변경·빌드/PIE 재실행·커밋/푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_arin-npc-fbx.md`, `Feature/doc/2026-10-05_arin-npc-fbx.md`, `Feature/doc/evidence/arin-apollo-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-16 — Blender MCP 아린 입체 모델·NPC 적용 (2026-10-07)

- 브랜치: `Sprint#5-16-blender-arin-npc`에서 개발 후 사용자 품질 개선 지시에 따라 `Sprint#5-17-arin-quality-refinement`로 전환.
- 목표: 사용자 지정 Blender MCP를 실제 설정하고 리깅된 아린 FBX를 시작마을 NPC에 적용한다.
- 주요 변경: 프로젝트 전용 Blender4.5.14/Python/MCP addon·loopback9877·Codex blender 항목, 재현 시작/호출 도구. 실제 입체175메시/42,764삼각형·23본·2초 Idle·BLEND/FBX/GLB. 전용 UE19에셋과 guide 외형 참조·BP/외부 액터 저장.
- 검증: 실제 MCP ping/version/scene/python/viewport, FBX18/18, 실제 NPC PIE64/64, 맵 해제·StartHub 재로드와 외형/보호설정 검증 PASS. 실제 마을 정면/얼굴/사선 캡처. Content6312기준 신규19·변경3·삭제0·나머지6309크기/수정시각 동일. 대화·정확한 물약2개·만석/중복/소모 후 포탈 잔여1개 유지.
- 상태: 스타일화된 v1 제작·적용·저장·집중 검사 완료. 단색15재질·UV/텍스처/LOD/얼굴·손가락 리그 없음. Blender 재수입은 live 소유 프로세스였으며 이후 v1 원본 fresh-process 검사에서는 새 품질 계약의 UV/PBR 기준 미달을 사실대로 기록. 품질 개선용 동결 기준선으로 보존. C++ 빌드/패키징/성능/네트워크 미실행, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-07_blender-arin-npc.md`, `Feature/doc/2026-10-07_blender-arin-npc.md`, `Feature/doc/evidence/arin-blender-20261007/`, `Scripts/Blender/README.md`, `DevLog/20261007.txt`.

## Sprint#5-17 — 아린 Blender 품질 개선 (2026-10-07)

- 브랜치: `Sprint#5-17-arin-quality-refinement`.
- 목표: 다섯 Blender 스킬·imagegen 참조·반복 검증으로 실제 아린 모델을 개선하고 UE용 UV/PBR·LOD·FBX를 적용한다.
- 주요 변경: 다섯 스킬 설치와 v1 동결, Q1~Q6·imagegen 참조·독립 critic. 실제 얼굴/눈/입·두피/땋기·손/손톱·연결 바지·외투/앞치마 변형 개선. 174편집 부위·4런타임 메시·51,634삼각형·23제작 본·2초 Idle, 4개 UV/PBR atlas·16PNG(Cloth4k/나머지2k). BLEND·결합/분리 FBX·GLB 및 실제 렌더/GIF 전달. UE 전용 24에셋·기존 guide 외형 연결·3LOD 저장.
- 검증: 새 Blender 프로세스 3포맷 검사·22개 최종 FBX 자세 렌더 검토, 실제 2초 Idle 13프레임. 최종 NPC PIE67/67·GUI 맵 해제/재로드·별도 UE Cmd 프로세스 저장 참조 검사 통과. 실제 UE 9화면에서 LOD1/2의 초기 의상 패치·홍채 손실을 발견해 실제 축소 비율1/.25/.125→1/.85/.65·경계 잠금으로 수정하고 재검토 통과. 최종 UE 정점40,211/35,681/28,851·각4sections·LOD0/골격 보존. 텍스처 임시32px는 배치 속성 설정+공식 컴파일 완료 후 실제2k/4k 검증. Python AST24개·diff·CheckGeneratedChanges 통과.
- 보존: Content6,331개 전체 SHA256에서 기존 안내인 DA/BP/소유 외부 액터3개만 변경, 신규RefinedV2 24개·삭제0·나머지6,328개 동일. 대화·물약2개·중복/만석 재시도·실제 소모/포탈 잔여1개 유지. 에디터 검토용 카메라·강제LOD·백그라운드 갱신 설정 복원.
- 상태: 스타일화된 안내 NPC 품질 개선·최종 적용·저장·집중 검증 완료. 근접 얼굴/헤어의 단순화·목 음영 경계·고정 망토 한계와 개별 손가락/얼굴 리그·보행/전투·Manny 호환·패키징/성능/온라인 미검증을 명시. C++ 변경/빌드 없음. Sprint5 유지, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-07_arin-quality-refinement.md`, `Feature/doc/2026-10-07_arin-quality-refinement.md`, `Feature/doc/evidence/arin-quality-20261007/`, `DevLog/20261007.txt`.

## Sprint#5-18 — 사용자 FBX·이미지 기반 아린 v3 개선 (2026-10-09)

- 브랜치: `Sprint#5-18-arin-reference-refinement`.
- 목표: 첨부 판타지 모험가 FBX와 전신 이미지의 얼굴·헤어·후드·자수 의상·장비를 실제 아린 모델에 반영하고 기존 시작마을 NPC 기능을 보존한다.
- 주요 변경: R1~R6 패킷·도너 조사와 원본/v2 동결, 도너 메시·UV·2K 색 atlas를 기반으로 헤어/팔보호대/부츠 국소 수리·재질 팔레트·피부 마스크·팔/무릎 가중치 개선. 최종45,205정점·90,214삼각형·1메시/1재질·23제작 본·최대3영향·170cm·2초 Idle, 실제4K PBR 네 장과 BLEND·결합/분리FBX·GLB·실제GIF. 전용 RefinedV3 새9에셋·guide 외형/Idle 연결·3LOD 저장. 임시32px resource 문제는 검사한7개 에셋을 재가져오기/삭제 없이 복구하고 실제4K gate 뒤 누락 재질2개를 준비했다.
- 검증: fresh Blender3포맷과 최종FBX8뷰·canonical 동등형상27뷰/clay·native packed4PBR SHA,13실제 Idle 샘플/12프레임2000ms, source/engine 독립 검토. 실제UE9화면 강제/예측LOD0·1·2/1section 유지·카메라와 임시 설정 복원. LOD정점110,688/100,176/81,219·목표1/.85/.65·경계 잠금·LOD0/골격 보존, 정확한 UE LOD삼각형 수는 미노출로 명시. 실제 NPC PIE67/67, GUI 맵 해제·재로드, 별도 UE Cmd 프로세스 저장 검사 exit0, 최종 출처/원본 보존 해시23개·신규9Python AST·diff/harness 경로 통과.
- 보존: Content6,355개 전체 SHA 감사에서 기존 guide DA/BP2개만 변경, 나머지6,353개 동일·새9개·삭제0·범위 밖 변경0. 소유 외부 액터 패키지는 저장했으나 SHA 동일. 기존 대화·포션2개·중복/만석 재시도·실제 소모와 포탈 잔여1개 유지. 사용자 첨부 원본과 v2 여섯 산출물의 SHA 동일.
- 상태: 아린 v3 모델 개선·시작마을 적용·저장·집중 검증 완료. 원본2K 얼굴/헤어 세부·약한 목 측면 음영·열린 의상 경계와 비다양체 표면·개별 얼굴/손가락 리그 부재를 기록한다. 보행/전투·Manny 호환·천 시뮬레이션·성능/패키징은 미검증이다. C++ 변경/빌드 없음. Sprint5 유지, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-09_arin-reference-refinement.md`, `Feature/doc/2026-10-09_arin-reference-refinement.md`, `Art/NPC/Arin/ReferenceRefined/README.md`, `Feature/doc/evidence/arin-reference-refinement-20261009/`, `DevLog/20261009.txt`.

## Sprint#5-19 — 아린 기본 걷기·대화 애니메이션 (2026-10-09)

- 브랜치: `Sprint#5-19-arin-walk-talk-animation`.
- 목표: 기존 아린 v3에 기본 Walk/Talk를 제작·적용하고 실제 이동·대화에 따른 자동 전환을 제공한다.
- 주요 변경: Blender 30fps·Walk1.2초/Talk3초와 별도 BLEND·FBX2개, C++ 전용 애니메이션 컴포넌트, Guide DataAsset의 클립·속도 설정, 같은 프레임의 타이머 중복 호출 보호, 기존 골격에 새 UE클립2개와 Guide DA참조 저장.
- 레퍼런스: Mixamo의 공개 걷기·대화 프리뷰. 적용 클립은 직접 제작한 키프레임이며 다운로드한 Mixamo FBX는0개.
- 검증: 최종 Editor 빌드 성공·재시작, fresh FBX Walk37/Talk91 전체 자세, 원본28/독립FBX8/실제UE6화면, 실제 PIE159/159(기존67 전부·신규23단계), 임시5FPS 저프레임 검사 후120FPS 복원, 새 Editor-Cmd PID51740 저장 재로드 통과.
- 보존: 기존 Content6363개 SHA동일·Guide DA1변경·신규클립2개·삭제/범위밖변경0. 기존 아린9패키지·Art59파일·대화/포션2/중복·만석/포탈잔여1 유지.
- 상태: 완료. 커밋·푸시 없음. Talk는 몸짓이며 얼굴 립싱크·개별 손가락·AI순찰·지형발IK·의상물리·전환블렌딩은 포함하지 않는다.
- 관련 문서: [설계](../Feature/architect/2026-10-09_arin-walk-talk-animation.md), [결과](../Feature/doc/2026-10-09_arin-walk-talk-animation.md), [사용 안내](../Art/NPC/Arin/Animation/README.md), [통합 검증](../Feature/doc/evidence/arin-walk-talk-20261009/tutorial-runtime.json), [일일 로그](../DevLog/20261009.txt).

## Sprint#5-20 — 이동 중 상체 공격·하체 보행 분리 (2026-10-09)

- 브랜치: `Sprint#5-20-moving-upper-body-attack`.
- 목표: 플레이어가 걷는 중 공격할 때 다리가 전신 공격 몽타주로 고정되는 문제를 해결한다.
- 주요 변경: 실제 수락 스킬/몽타주·지상 속도에 따른 native 상체 조건, 보행과 몽타주 결과의 별도 캐시, spine_01/ik_hand_root 상체 마스크, 정지0.12초 전신 복귀. 회피 weighted tail·점프/공중 유지, 이동 시 정지 가드 해제. 기존11노드·DefaultSlot·몽타주·Data Asset 보존 후 새9노드 추가. Editor 전용 제작/읽기 감사와 기존 가드·FootIK 검증 계약 갱신.
- 검증: Editor 최종 빌드·프로젝트 재생성, 컴파일 오류/경고0, fresh 재로드와 legacy guard/FootIK 감사 통과. 원본 그래프 대조1100프레임 하체474실패→수정1100프레임·258066검사 실패0. 실제 렌더1100·가드640·기존 전투840프레임 실패0. 맨손/너클9종에서 source/retarget 하체 보행 최대차0.000002415도·source 위치차0cm, 실제 체력16펄스·타이밍 유지.
- 보존: Content6366개 중 기존 Common AnimBP1개만 변경·6365개 SHA 동일·추가/삭제0. 기존 아린·월드·HUD와 미커밋 작업 유지. StartHub Editor·PIE 종료·dirty0·120FPS 복원.
- 상태: 구현·저장·집중 QA 완료. 현재 맨손/너클 경로에 집중 검증. 커밋·푸시 없음.
- 관련 문서: [설계](../Feature/architect/2026-10-09_moving-upper-body-attack.md), [결과](../Feature/doc/2026-10-09_moving-upper-body-attack.md), [검증 비교](../Feature/doc/evidence/moving-upper-body-attack-20261009/runtime-comparison.json), [일일 로그](../DevLog/20261009.txt).

## Sprint#5-21 — 1인칭 카메라 하향·공격 전방 점검 (2026-10-09)

- 브랜치: `Sprint#5-21-lower-camera-forward-attacks`.
- 목표: 1인칭 카메라를 조금 낮추고 현재 플레이어 공격의 전방 방향과 실제 피해를 점검한다.
- 주요 변경: native EyeOffset.Z 4→-3cm로 약7cm 하향. 초기 pitch=-20도·조준·3인칭 검사·몽타주·피해 시점 유지. 읽기 전용 전체 공격 감사와 실제 타격손/앞뒤 대상 피해/동일 자세 카메라 비교·1인칭 캡처 도구 추가.
- 검증: 최종 Editor 빌드 성공. 74DA/13Profile 참조·타이밍 이상0, 현재 맨손/너클23공격·16시퀀스120Hz 방향 조사 이상0. 실제13종+회전2건=15건의 runtime/render 각각1,572프레임·4,978/4,979검사·실패0, 체력21펄스·15접촉/2idle화면. 카메라 동일 자세7.000000cm·독립 오차0. 기존 머리 카메라570·초기 시선90·3인칭280·이동 상하체1,100프레임 회귀 실패0.
- 보존: Content6,366파일 전부 SHA 동일·추가/삭제/변경0, 집중207패키지/타격 데이터/프로필/몽타주 동일. 기존 아린·월드·HUD·미커밋 작업 유지. 여섯 Blueprint 기본값 확인, StartHub Editor·PIE 종료·dirty0·120FPS 복원.
- 상태: 구현·집중 QA·독립 리뷰 완료. 실제 손 방향/전투 범위는 현재 맨손·너클·무투가 경로다. 원형 내려찍기의 후방 피해는 의도된 범위 판정이다. 커밋·푸시 없음.
- 관련 문서: [설계](../Feature/architect/2026-10-09_lower-camera-forward-attacks.md), [결과](../Feature/doc/2026-10-09_lower-camera-forward-attacks.md), [원본 감사](../Feature/doc/evidence/camera-forward-attacks-20261009/audit.json), [일일 로그](../DevLog/20261009.txt).

## Sprint#5-22 — Walking.fbx 기반 보행 손바닥 보정 (2026-10-09)

- 브랜치: `Sprint#5-22-walking-palm-reference`.
- 목표: 첨부 Walking.fbx의 손바닥 방향을 참고해 현재 플레이어의 맨손·너클 보행을 개선한다.
- 주요 변경: Blender에서 65본·30FPS 원본을 해부학적 손 평면으로 분석했다. 원본 Walk/Jog 16개를 복제해 손목→중지 축의 제한된 회전만 적용하고 새 BlendSpace로 연결했다. 모든 위치·스케일, 손가락·하체·IK 원본 채널과 27표본·Idle·타이밍·마커·공격 그래프를 보존했다. 첫 보조 IK 보정의 실행 중 이동 문제를 발견해 원본 IK를 복원하고 더 엄격한 검증을 추가했다.
- 검증: UE5.8 Editor 빌드27.97초 성공. native101,953키쌍·16클립 검사 통과, 보호 위치/스케일 오차0·회전0.000116414도 이내. 실제 render650프레임·95,970검사·9화면과 여섯 외형 각각650프레임·95,976검사 실패0. 독립 보행 대조220프레임에서 보조 손 IK 위치 변화0cm, 손바닥 평균 방향16조합 모두 개선. 이동 공격1,100프레임·258,066검사·피해16건, 전방/카메라1,572프레임·15건·피해21펄스 회귀 통과.
- 보존: 기존 Content6,366개 중 Common AnimBP1개만 변경·나머지6,365개 SHA 동일, 신규17·삭제0. 제공 FBX SHA 동일. 새 에디터 재로드에서 그래프/표본/키 검사 통과, 여섯 카메라(8,0,-3)cm·pitch-20도 유지. StartHub·PIE 종료·dirty0·120FPS 복원.
- 상태: 적용·저장·집중 QA·독립 정적/기하/시각 리뷰·문서 완료. 전체 Mixamo 보행을 이식하지 않고 손 방향을 반영했으며 모든 프레임의 참조 일치를 주장하지 않는다. 커밋·푸시 없음.
- 관련 문서: [설계](../Feature/architect/2026-10-09_walking-palm-reference.md), [결과](../Feature/doc/2026-10-09_walking-palm-reference.md), [독립 실행 검토](../Feature/doc/evidence/walking-palm-reference-20261009/reference/runtime-review-v2.json), [일일 로그](../DevLog/20261009.txt).

### Sprint5 운용 기록 — 스튜디오 활성화와 후속 개발 검토 (2026-10-11)

- 목표: 개인 개발 스튜디오를 활성화하고 남은 개발 후보를 제시한다.
- 변경: 기존 실행기로 로컬 서버 시작, 개발 일정 화면 열기 요청, 최신 결과 문서 기반 6개 후속 후보 정리 및 한국어 개발 기록 추가.
- 검증: 실제 API 응답에서 ready·paused=false·진행/실행대기0·등록2건 모두 completed 확인.
- 상태: 활성화·목록 검토 완료. 후보는 미등록이며 게임 구현·빌드/PIE·브랜치 변경·커밋/푸시 없음. 기존 게임·Studio 작업 보존.
- 관련 문서: `Feature/doc/2026-10-11_studio-backlog-review.md`, `DevLog/20261011.txt`.

## Sprint#5-23 — 아자토르 보스 전용 패턴 (2026-10-11)

- 브랜치: Sprint#5-23-azathor-boss-patterns.
- 목표: 기존 아자토르에 예고가 있는 정면/범위 특수 공격과 체력 단계 패턴을 구현하고 실제 검증한다.
- 주요 기록: 사용자 요청으로 스튜디오 예정에 높은 우선순위·자동 실행으로 등록. 기존 미커밋 작업과 기본 보스 능력치 보존, 실제 빌드/PIE/재로드/독립 검토를 작업 범위에 명시.
- 검증: API 저장 확인·기존2건 보존·새 작업 ID a359620e-46c8-409f-a2e2-6e991387d1da. dispatcher active와 engine busy, 기획 에이전트 running 및 실제 세션 시작 확인.
- 상태: 확인 필요(blocked·일시정지). 기획 에이전트가 PATH의 python 부재로 중단했다. 설치된 Python 3.12.14의 절대 경로와 작업 메모를 보완했지만 샌드박스 읽기 전용 MCP 확인은 WinError 10013으로 실패했다. 사용자가 허용한 동일 확인 명령 하나를 승인 심사에 제출해 실제 실행했으나 이번에는 WinError 10061(연결 거부), 종료 코드 1로 실패했다. 현재 UnrealEditor 프로세스 0개이며 기본 MCP 설정은 자동 시작·8001/mcp다. PIE/미저장 상태 미확인·구현 패킷 미확정·게임 구현/빌드/QA 미실행. 단일 명령 허용을 지속 실행 설정 변경이나 자동 재검토 권한으로 확대하지 않았다. 커밋·푸시 없음.
- 후속 지시: 사용자가 에디터 실행·재검토를 요청한 뒤 다른 채팅의 Studio 제거와 충돌을 확인했고, "스튜디오 없이 아자토르 직접 개발"을 선택했다. 아래 상태가 현재 개발 상태이며 위 blocked는 제거 전 초기 Studio 실행의 기록이다.
- 현재 변경: 직접 개발로 보스 opt-in·기본→정면→기본→영역 순환·고정 직사각형/원형 경고·HP50% 강화·특수 쿨타임·범위/높이/가림/단일 타격·취소/사망/EndPlay 정리를 C++ 컴포넌트에 구현했다. 전용 프로필/데칼 4개를 새로 만들고 기존 아자토르 DA의 참조 한 곳만 연결했다. 기본 HP10000·피해70·EXP3000·이동240과 기존 애니메이션·다른5종 보존.
- 현재 검증: Editor 빌드68.49초 성공, 실제 PIE22사례·480확인 실패0·55.688초, 강화 예고/피해/회복 시간의 관측 오차 구간까지 검증. 주황 직사각형·보라 원·붉은 강화 실제 화면3개 확인. 새 에디터 재로드와 콘텐츠 기존6296개 동일·기존DA1변경·4추가·삭제0 및 독립 리뷰 통과. 마지막 OpenRunMap·PIE없음·dirty0·원래 CPU제한/FPS 복원.
- 현재 상태: 구현·저장·집중 QA·독립 검토·문서 완료. 첫 화면 확인의 GPU 메모리 오류는 기록 후 에디터 재실행·재로드·slow-motion 화면 확인으로 복구했다. 장시간 GPU 안정성·북부 이동/전체 AI 전투·물리 회피·패키징·네트워크·최종 밸런스는 별도 검증이다. 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-11_azathor-boss-patterns.md, Feature/doc/2026-10-11_azathor-boss-patterns.md, Feature/doc/evidence/azathor-boss-patterns-20261011/, DevLog/20261011.txt. 제거 전 일정·실행 이력은 Studio 제거 문서의 로컬 백업에 보존되어 있다.

### Sprint5 운용 기록 — 개인 게임 개발 스튜디오 제거 (2026-10-11)

- 목표: 사용하지 않는 개인 개발 스튜디오와 전용 실행·예약 연결을 제거한다.
- 주요 변경: 실행 작업 없음 확인 후 서버 PID28988 종료, heartbeat rogue10m 삭제, Studio 전체 및 BAT·PowerShell 실행기 삭제(추적 파일21개), 현재 MCP 문서의 개인 Studio 설명 정리. 기존 앱·일정·실행 이력75파일을 Git 제외 로컬 ZIP에 백업·검증했다.
- 검증: 실제 프로세스/포트 소유권 및 종료, 4317 리스너 없음, 자동화 삭제 응답과 설정 부재, ZIP 전항목 경로·크기·SHA256 일치, 제거 경로 부재·활성 참조 감사, CheckGeneratedChanges 종료 코드0.
- 상태: 제거 완료. 과거 기록·게임 코드·에셋 보존. 성장·스킬·인벤토리 일정 추가는 이 채팅에서 미실행. 다른 채팅의 직접 개발과 공유 브랜치를 유지했고 UE 빌드/PIE·커밋/푸시 없음.
- 관련 문서: `Feature/architect/2026-10-11_remove-personal-studio.md`, `Feature/doc/2026-10-11_remove-personal-studio.md`, `DevLog/20261011.txt`. 로컬 백업: `tmp/studio-removal-20261011/studio-backup.zip`.

## Sprint#5-24 — 마을 NPC 서비스와 캐릭터 진행 영구 저장 (2026-10-11)

- 작업 브랜치: `Sprint#5-24-npc-services-persistence`의 별도 작업 폴더. 원본 동시 작업의 현재 `Sprint#5-26-monster-test-ui` 브랜치 보존.
- 목표: 아린의 물약 구매·주무기/방어구 강화·활력/힘 특성·다음 탐험 난이도를 실제 게임에 추가하고 성장·수련/토벌·스킬 진행·인벤토리를 프로필별로 영구 저장한다.
- 주요 변경: Data Asset 기본값과 PlayerState 전용 특성 컴포넌트, 프로필 GUID SaveGame, 변경 이벤트 1초 자동 저장·포탈/종료 저장, 전체 인벤토리와 아이템 GUID/강화/퀵슬롯, 수령 기록과 프로필 전환 격리, 난이도 및 XP/골드 보상. 복원 실패는 롤백하고 기존 파일을 보존한다.
- 검증: 격리 최종 Editor 빌드80.86초·원본 통합 빌드29.64초 성공. 원본 서비스 Write60/Read25와 실제 포탈/런 Write42/Read15 실패0. 오류 저장27검사 실패0·원본 바이트 동일. 720p/1080p 실제 입력/화면 각9검사 통과, 최신 원본 F8 입력과 함께 쓰는 NPC 메뉴9검사 추가 통과. 독립 리뷰·생성 출력 검사·scoped diff 검사 통과.
- 보존: 기존 미커밋 작업·보스와 테스트 UI 변경 유지. 기능 Source34개 반영 후32개 해시 일치·BasicMonster2개 동시 테스트 인스턴스 변경 별도 대조. 본 기능의 바이너리 에셋·모듈/Build.cs 변경 없음, Studio 제거 유지.
- 상태: 구현·원본 반영·빌드·실제 기능/화면 검증·독립 리뷰·문서 완료. 로컬 단일 플레이 저장을 지원하며 현재 런·현재 HP/MP/스태미나·월드 드롭 액터는 저장 대상이 아니다. 커밋·푸시 없음.
- 관련 문서: [설계](../Feature/architect/2026-10-11_town-services-persistence.md), [결과](../Feature/doc/2026-10-11_town-services-persistence.md), [사용 가이드](TownServicesPersistenceGuide.md), [독립 리뷰](../Feature/doc/evidence/town-services-persistence-20261011/review.md), [개발 일지](../DevLog/20261011.txt).

### Sprint5-26 — 현재 위치 몬스터 테스트 UI (2026-10-11)

- 목표: 넓은 맵을 이동하지 않고 일반 몬스터와 아자토르를 현재 위치에서 바로 시험한다.
- 주요 변경: 별도 C++ UMG 창/테스트 컴포넌트, F6 및 안내 버튼, 실제6종 안전 지면 소환·최대6마리·소유 대상 정리, 기본/정면/영역·HP40% 강화·초기화·자동 AI·회복·무적, pause/focus/캐릭터 전환 복원, 테스트 대상 보상 차단. F8 에디터 단축키 충돌을 피함. 바이너리 에셋·맵·원본 데이터·스튜디오 변경 없음.
- 검증: 최종 Editor 빌드9.86초 성공; F6 실제 게임1280×720 80확인·실패0, 1920×1080 추가80확인·실패0; 실제 Slate hit-test 마우스 클릭·키/ESC·피해·보상·수명/안전 경계·화면; 새 Editor PIE 창/pause/blocking·공식 MCP 캡처; 최종 OpenRunMap·PIE없음·dirty0, CheckGeneratedChanges/diff --check 통과. 초기 실패와 수정 이력 보존.
- 상태: 구현·집중 QA·검토·문서 완료. 작은 에디터 뷰포트는 F11 또는 새 에디터 창 사용 권장. Shipping 패키징·멀티플레이·장시간 GPU 안정성은 별도. 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-11_monster-test-ui.md, Feature/doc/2026-10-11_monster-test-ui.md, Feature/doc/evidence/monster-test-ui-20261011/, DevLog/20261011.txt.

### Sprint5-28 — 몬스터 시야 추적과 종별 Niagara (2026-10-11)

- 목표: 일반5종과 아자토르의 플레이어 인식→추적→공격을 보강하고 공격·실제 명중을 종별 효과로 표시한다.
- 주요 변경: 주변360도 시야·정지/재개·리시 재인식 차단·이동 Actor 경로 유지, F6 자연 인식과 짧은 사거리 테스트 보정, 종별 CPU burst6개+보스 특수2개 제작 및 프로필6개 연결, AutoRelease 완료 참조 정리, 다중 Editor 프로젝트 경로 보호.
- 검증: 원본 통합 빌드32.42초 성공, 최종 격리110 및 원본110검증 실패0, 기존 F6 UI80검증 실패0, Niagara8개 컴파일/스택 오류·경고0, 실제 종별/보스 화면, 생성 경로·diff·Python 문법 검사 통과. 초기 실패와 수정 기록 보존.
- 상태: 구현·한정 원본 통합·빌드·실제 플레이 QA·검토·문서 완료. 다른 채팅의 특성·저장 변경 보존. 로컬 단일 플레이 검증; 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-11_monster-effects-chase.md, Feature/doc/2026-10-11_monster-effects-chase.md, Feature/doc/evidence/monster-effects-chase-20261011/review.md, DevLog/20261011.txt.

## Sprint#5-27 — 특성 16종·강화 UI 실제 적용 (2026-10-11)

- 목표: 설계한 16종을 실제 전투에 적용하고 아린의 독립 특성 강화창·영구 저장·무료 재분배를 제공한다. 인벤토리·스킬·성장은 각각 별도 관리한다.
- 주요 변경: 카탈로그 Data Asset/CDO16종, 실제 비용·누적 보너스 투자 기록, 16종 능력치/조건부 효과, v1→v2 보존 전환과 손상 거절, 쓰기 실패 재시도, 네이티브 UMG 필터·상세·입력 복구, 공식 Editor 임포트 UI Texture1개.
- 검증: 최종 Editor 빌드9.76초 성공. 효과100·저장125+227·UI260·기존 서비스60+25/런42+15, 총854확인·실패0. 실제 Slate 키·마우스·16카드 스크롤·재분배 모달·저장 재시도, 독립 검토, 생성 경로·공백 검사, 새 Editor 클래스3개/16종/Texture 재로드·dirty0 확인.
- 브랜치: 최초 Sprint#5-27-trait16-runtime에서 시작. 동시 몬스터 작업이 공유 원본을 Sprint#5-28-monster-effects-chase로 전환하여 현재 브랜치와 다른 변경을 보존했다.
- 상태: 구현·빌드·실제 단일 플레이 QA·독립 검토·문서 완료. 초기 실패 증거 보존. Shipping cook·멀티플레이·장기 밸런스는 별도. 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-11_trait16-runtime.md, Feature/doc/2026-10-11_trait16-runtime.md, Docs/TraitEnhancementGuide.md, Feature/doc/evidence/trait16-runtime-20261011/, DevLog/20261011.txt.

## Sprint#5-29 — 플레이어 시야에 맞춘 상체 애니메이션 (2026-10-13 완료)

- 브랜치: `Sprint#5-29-view-aligned-upper-body`. 10월11일 시작한 작업을13일 최종 검증했다.
- 목표: 현재 외형 메시의 상체·팔 애니메이션이 플레이어의 시야각을 따라 움직이고 하체·손목·손가락과 머리 카메라의 회전 계약을 보존한다.
- 주요 변경: native AnimInstanceProxy 최종 포즈 보정, pitch75도·잔여 yaw45도 제한, 척추25%·양팔 나머지75% 분배, 지수 보간, 목/머리 원본 회전 복원과 FK/IK 손 동기화. 독립 원본 대조·실제 입력·외형 리타깃 QA, 기존 이동 원본 검사용 중립 조준 선택과 회귀 QA 저장 경로 격리.
- 검증: 최종 Editor 빌드8.23초 성공. 집중 통합/실제 렌더 각각21상황·1,575프레임·460,488검사·실패0, 원본 대조459,385검사·실패0, 부정 대조는 조준 관련4종만2,456건 검출. 이동1,100/가드640/카메라1,572/맨손 전투840프레임 회귀 모두 실패0. 20초 주요 공격·52.5초 전체 실행영상과 독립 소스/실제 화면 검토 완료.
- 보존과 복원: Content6,359개 크기/mtime·추가·삭제 변경0, Common AnimBP SHA 동일. 초기 QA 저장 접두사 오류로 변경된 진행도는 작업 전 전체 SHA가 같은 원본 바이트로 Unreal API를 통해 복원했고 생산 저장3개 SHA를 재확인했다. 일회 복구 도구는 Source에서 제거했다. 기존 OpenRunMap·PIE 종료·dirty0과 새 native 기본값 확인.
- 상태: 구현·빌드·범위 내 QA·독립 검토·문서 완료. 현재 인간 남성 LOD1 검증이며 극단 상향 V회피의 근접 클리핑·고각 머리 위치 변화는 한계로 기록했다. 기존 미커밋 작업을 보존했고 커밋·푸시는 없다.
- 관련: [설계](../Feature/architect/2026-10-11_view-aligned-upper-body.md), [결과](../Feature/doc/2026-10-11_view-aligned-upper-body.md), [독립 QA](../Feature/doc/evidence/view-aligned-upper-body-20261011/qa-review.md), [최종 소스 리뷰](../Feature/doc/evidence/view-aligned-upper-body-20261011/final-source-review.md), [일일 로그](../DevLog/20261013.txt).

## Sprint#5-30 — 마우스 기준 머리 안정화·안전 모션·속도 연동 (2026-10-13)

- 브랜치: `Sprint#5-30-mouse-head-stable-locomotion`.
- 목표: 걷기2도·달리기4도·공격3도 안으로 머리/카메라 잔여 회전을 제한하고, 상체 시선·하체 보행·공격/이동 속도를 일치시킨다. 스핀·뒤돌아차기·구르기를 활성 경로에서 제외한다.
- 주요 변경: mouse head native proxy와 평가 직전 제한 재캡처, 포즈/basis snapshot·반복 camera read·작은 위치 잔여, Movement→Source→Appearance 순서, 비용 전 금지 모션 gate, DA35개와 회피 안전 연결, 파생sequence7+montage33, grounded BlendSpace rate getter와 발자국 cadence. 원본 첫root 높이·게임플레이 수치·섹션/Notify 보존.
- 검증: 최종 Editor 빌드56.43초. 실제 통합/렌더 각각24상황·1800프레임·489428기록 검사·실패0. 비활성 대조21상황은 머리 관련3151건 예상 검출. Fist840프레임·9스킬14실제 피해/머리 그림자/V, Moving1100프레임·258033검사·9스킬16피해 회귀 통과. fresh74DA/13프로필·unsafe active0, 독립source20파일·PNG9장 검토. 25초/60초 실행영상.
- 보존: source455개 중14개 수정·7개 추가·삭제0. Content6359개 중 의도한37개 변경·40개 추가·다른 변경/삭제0, 생산 저장3개 SHA/목록 동일. OpenRunMap·PIE종료·dirty0·native2/4/3기본값 확인.
- 상태: 구현·빌드·범위 내 수치/렌더·독립 검토·문서 완료. 초기 실패 증거 보존. 인간 남성LOD1 단일 플레이 범위, 기본1인칭 HUD 손 가림·모든 target 손가락/지면접촉 불인증을 명시했다. 기존 미커밋 작업 보존, 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-13_mouse-head-stable-locomotion.md, Feature/doc/2026-10-13_mouse-head-stable-locomotion.md, Feature/doc/evidence/mouse-head-stable-locomotion-20261013/qa-review.md, Feature/doc/evidence/mouse-head-stable-locomotion-20261013/final-source-review.md, DevLog/20261013.txt.

## Sprint#5-31 — 대검·맨손 아이덴티티 이미지 (2026-10-14)

- 목표: 중앙 원형 HUD와 어울리는 대검·맨손 상징을 투명 PNG 원본으로 제작한다.
- 주요 변경: built-in imagegen 자산별 생성1회·여백 편집1회, 흑철/황동/붉은 대검과 돌주먹/금빛/비취 기운, SourceArt 원본·정확한 프롬프트 보존. .gitignore에서 최종 PNG2개만 예외 처리.
- 검증: 최종2개1254×1254 RGBA/alpha0~255, 본체 높이75.76%·72.65%, 실제 원본 독립 기술·시각 검토 PASS, 파일SHA·프롬프트·Git제외안됨 확인. Unreal source/Texture 변경이 없어 빌드 미실행.
- 상태: 이미지 제작·원본 보존·검토·문서 완료. 실제 HUD import/binding와68px 실행 배치는 별도. 기존 작업 보존, 커밋·푸시 없음.
- 브랜치: Sprint#5-31-identity-image-assets.
- 관련: Feature/architect/2026-10-14_identity-images.md, Feature/doc/2026-10-14_identity-images.md, Feature/doc/evidence/identity-images-20261014/file-qa.json, Feature/doc/evidence/identity-images-20261014/visual-qa.md, DevLog/20261014.txt.


## Sprint#5-32 — 좌우 하단 전투 HUD 적용 (2026-10-14)

- 브랜치: Sprint#5-32-split-combat-hud. 기존 Sprint#5-31 원본과 미커밋 작업 보존.
- 목표: 승인한 몬스터헌터 참고 imagegen 시안대로 좌 생존/LV/EXP·우스킬/아이덴티티/잠금4를 실제 게임에 적용하고 중앙 시야를 확보한다.
- 주요 변경: native 반응형 두코어·고정gap96·초록HP 표시·마력교체보존·획득피드위배치, beveled native frame, 최신맨손/대검2Texture 표시override, 신규Split6파트/기존MainBottom2연결, 기존Compact모드복구flag.
- 검증: 최초 Editor 빌드45.12초·프레임수정후최종링크3.88초성공. 최종7조건·1,334확인·실패0, PNG7장·기존.sav67개byte보존. UI80기존중78개동일/2변경/8신규/삭제0, rawPNG2동일, 저장계약·새Editor재로드·dirty0·PIE종료, 독립정적/3해상도시각리뷰·생성경로·공백/문법검사통과.
- 상태: 구현·저장·검증·재로드·문서 완료. 발견한frame배열assert/비율glyph테두리겹침과검증API문제수정, 실패증거보존. 인벤토리·스킬·성장·특성관리/저장스키마·밸런스·맵보존. Shipping/네트워크/장기GPU·아주작은글자별도. 커밋·푸시없음.
- 관련: Feature/architect/2026-10-14_split-combat-hud.md, Feature/doc/2026-10-14_split-combat-hud.md, Docs/SplitCombatHUDGuide.md, Feature/doc/evidence/split-hud-20261014/, DevLog/20261014.txt.


### 사용자 승인 커밋·푸시 준비 (2026-10-14)

- 범위: HUD 네이티브6개·전용10에셋·원본PNG2개·공통 작성도우미와 검증/문서/증거. 그 외 기존 미커밋 기능은 제외한다.
- 보완: 통합 QA의 별도 진행 저장 의존성을 조건부 컴파일·실행 전 검사로 명시한다. 에디터는 종료하지 않고 임시 standalone에서만 오류 종료한다.
- 추가 검증: HEAD 추적 파일+스테이징 HUD 소스7개만 있는 독립 Editor 빌드와 최종 QA 파일 재컴파일·링크 성공. 독립 범위/종료 보호 검토 미해결0.
- 상태: 사용자 승인에 따라 이 작업 브랜치의 커밋과 일반 원격 푸시를 진행한다. Sprint는 main 병합 전이므로5를 유지한다.
- 파일 목록/검증: Feature/doc/evidence/split-hud-20261014/commit-files.txt, commit-clean-build.log, commit-clean-build-final.log.

## Sprint#5-33 — 중앙 확장형 혼합 성장 스킬트리 설계 (2026-10-18)

- 브랜치: `Sprint#5-33-hybrid-skill-tree-design`. 기존 Sprint#5-32 체크아웃과 미커밋 작업을 유지하며 새 로컬 브랜치로 전환했다.
- 목표: 사용자 참조 이미지의 중앙→아이덴티티·기본기→핵심 기술 구조와 격투 숙련·레벨업 포인트 혼합 성장 규칙을 설계한다.
- 주요 변경: D1~D4 패킷, 기존 초기5기술 보존과9잠금 무공의 숙련/스탯 경로 제안, 아이덴티티 입문1P·일부 심화2P 또는 숙련 성장·성질2P·양부모 공명, 특성 공유 포인트 원장, 일회 보상·활성1계열·재분배/저장 이전 정책. 대표 정권 경로 클릭 구조도와 참조/검토 증거.
- 검증: 독립 조사·최종 설계 검토 미해결0, 단계/선행·스탯/XP·포인트 산술, 구조도19노드·736/320/다크/터치 선택·요약·넘침/겹침/스크립트 오류0, 원본 참조SHA일치, Harness 경로 통과. 문서/로그 공백·링크와 기존 변경 보존 확인.
- 상태: Pre-production 설계·검토·문서 완료. 실제 C++/원형UMG/노드DataAsset/숙련XP/공유 원장/새 저장·밸런스 검증은 후속 Production. 소스·설정·Unreal 바이너리·생산 저장 변경 없음, Editor 빌드 미실행. 커밋·푸시 없음, Sprint5 유지.
- 관련: [설계](HybridSkillTreeDesign.md), [계획](../Feature/architect/2026-10-18_hybrid-skill-tree-design.md), [결과](../Feature/doc/2026-10-18_hybrid-skill-tree-design.md), [검토](../Feature/doc/evidence/hybrid-skill-tree-20261018/review.md), [일일 로그](../DevLog/20261018.txt).

### imagegen 화면 시안 추가 (2026-10-18)

- 목표: 사용자 후속 요청에 따라 확정 성장 규칙을 실제 화면 형태의 이미지로 보여준다.
- 주요 변경: D5 패킷, built-in 참조 생성1회·상태/글자 교정1회, 흑철·황동 방사형 트리의 최종PNG와 두 프롬프트 보존. LV4/M12·내공 심화0P·잔액0P·활성내공·미래기술/공명잠금 표시.
- 검증: 최종 이미지 시각 검토·PNG 치수·생성 원본과 복사SHA 일치. 첫 출력의M9 스태미나/비활성입문 상태·일부 기술명 교정. 일부 중간 노드는 대표 표현으로 생략함.
- 상태: 화면 시안 제작 완료. 실제 원형UMG/공유 원장/숙련 시스템 구현은 후속 범위. 기존 브랜치 유지, 커밋·푸시 없음.
- 관련: [최종 이미지](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-mockup-v2.png), [생성 프롬프트](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-prompt-v1.txt), [교정 프롬프트](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-prompt-v2.txt).

### 격투가 아이덴티티 내공·투지 확정 (2026-10-18)

- 목표: 사용자 확정에 따라 격투가는 내공·투지만 사용하도록 설계와 최신 이미지 시안을 정리한다.
- 주요 변경: 격투가의 마나 표/성질/산식 제거, 직업 허용 목록 검증 명시, built-in 편집1회로마나 가지를 제거한v3 PNG·프롬프트 보존. 포인트/숙련 혼합 해금과활성1계열 정책 유지.
- 검증: 상세 설계와v3 이미지의내공·투지 두계열 대조, 기존성장/잔액/수련0P유지, 생성원본/복사SHA동일, 문서공백·링크/생성경로검사.
- 상태: 설계·최신화면시안수정완료. 기존HTML·v2는변경전기록, 실제런타임구현후속. 기존브랜치유지·커밋/푸시없음.
- 관련: [설계](HybridSkillTreeDesign.md), [현재v3 이미지](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-mockup-v3.png), [현재 편집 프롬프트](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-prompt-v3.txt), [일일 로그](../DevLog/20261018.txt).

## Sprint#5-34 — Unreal MCP 설치 스킬 Epic 최신 원본 보완 (2026-10-19)

- 브랜치: `Sprint#5-34-unreal-skill-upstream-sync`. 기존 Sprint#5-33과 미커밋 작업 보존, main 병합 전이므로 Sprint5 유지.
- 목표: 설치된 개인 Codex Unreal MCP 스킬과 Epic 최신 원본을 비교하고 누락 지침을 Codex 방식으로 반영한다.
- 주요 변경: Epic 3.1.1/a6aa73ad 기준 Markdown 9개 보완. AllToolsets/provider, proxy status·catalog·복구, 독립 호출 병렬/의존 순차, native dispatch, toolset async/타입/테스트, Agent Skill class path·저장, 실제 설치 경로와 Saved/커밋 규칙, 원본 대응표.
- 검증: candidate/설치본 공식 구조 검사·설치 전용 검사, 참조15·예제3·스크립트 AST/YAML/MIT 대조, 독립6사례·timeout 모호성1건 수정/재확인, 설치20파일 SHA256·파일 수 확인, 생성 경로/문서 검사 통과.
- 상태: 비교·보완·개인 설치 반영·정적/행동 검토·문서 완료. 원본20파일 로컬 백업 보존. 8001·자동 호출·runner8·MIT·기존 프로젝트 변경 유지. 실제 엔진에는 proxy가 없어 라이브 MCP/PIE/빌드·품질 향상은 검증하지 않음. 커밋·푸시 없음.
- 관련: [계획](../Feature/architect/2026-10-19_unreal-skill-upstream-sync.md), [결과](../Feature/doc/2026-10-19_unreal-skill-upstream-sync.md), [검토](../Feature/doc/evidence/unreal-skill-sync-20261019/review.md), [일일 로그](../DevLog/20261019.txt).

## Sprint#5-35 — Unreal MCP 플러그인 직접 설정 (2026-10-19)

- 브랜치: `Sprint#5-35-unreal-plugin-setup`. 기존 Sprint#5-34와 미커밋 작업 보존, main 병합 전이므로 Sprint5 유지.
- 목표: 현재 설치 스킬에 필요한 Unreal 플러그인과 에디터 설정을 직접 보완하고 실제 연결을 확인한다.
- 주요 변경: LiveCodingToolset Editor 한정 활성, 공식 설정 API의 Live Coding 활성·저장, tracked LiveCoding 기본값 추가. 기존 MCP/AllToolsets·8001 자동 시작/search·AIAssistant 비활성·Codex config 보존.
- 검증: DLL/BuildId 55116800·의존성·Editor 제한 독립 검토, 실제 loopback8001/필수3플러그인/53toolset·3meta/CompileLiveCoding schema, settings·PIE/Entry 조회, 소유 에디터 정상 재시작 후 LiveCoding=True와 MCP4설정 유지, 문서/생성 경로 검사.
- 상태: 플러그인·기능 활성·지속성·실제 MCP 연결·독립 검토·문서 완료. 에디터 실행 중, compile/게임 빌드/PIE/패키징 미실행. native 도구 목록은 필요 시 Codex 기존 연결 재연결. 커밋·푸시 없음.
- 관련: [계획](../Feature/architect/2026-10-19_unreal-plugin-setup.md), [결과](../Feature/doc/2026-10-19_unreal-plugin-setup.md), [가이드](UE58MCPSetup.md), [검토](../Feature/doc/evidence/unreal-plugin-setup-20261019/review.md), [일일 로그](../DevLog/20261019.txt).

## Sprint#5-36 — Fab 보유 에셋 우선 재사용 원칙 (2026-10-19)

- 브랜치: `Sprint#5-36-fab-owned-asset-reuse`. 기존 미커밋 작업과 Sprint5 번호 유지.
- 목표: 사용자 보유 Fab 에셋을 우선 활용하도록 실제 목록과 프로젝트 자원을 대조하고 개발 지침에 저장한다.
- 주요 변경: Fab 내 라이브러리 최신순 일부/Combat/Monster 카드30개와 로컬 주요8팩 조사. AGENTS 재사용 규칙, 영역별 후보·기존 계약 보존·선별 반입/검증 가이드, 출처/현재 상태 구분과 부분 조사 기록.
- 검증: 실제 런처 화면 확인·독립 로컬 조사/문서 검토, JSON/문서 링크·기존 로그/프로젝트 변경 보존·생성 경로 검사.
- 상태: 조사·상시 지침·활용 가이드·기록 완료. 전체 보유 목록·버전/라이선스·실제 적용은 미검증. 구매/다운로드/설치/import·게임 소스/설정·빌드/PIE 변경 없음. 사용자 런처 입력 감지 후 추가 조작 중단, 커밋·푸시 없음.
- 관련: [가이드](FabAssetReuseGuide.md), [계획](../Feature/architect/2026-10-19_fab-owned-asset-reuse.md), [결과](../Feature/doc/2026-10-19_fab-owned-asset-reuse.md), [보유 카드 기록](../Feature/doc/evidence/fab-owned-assets-20261019/launcher-observations.json), [일일 로그](../DevLog/20261019.txt).

### Sprint#5-36 후속 — 기본 애니메이션·VFX 편집 우선 (2026-10-19)

- 목표: 기본 동작·효과 제작을 보유 에셋 수정/편집 중심으로 구체화한다.
- 주요 변경: AGENTS와 활용 가이드에 파생본 편집 기본 경로, 애니메이션/효과별 조정 범위와 원본·카메라·판정 보존, 새 제작이 필요한 경우의 이유 기록을 추가했다.
- 검증/상태: 문서 검토·경로/공백 검사, 선호 반영 완료. 실제 에셋·소스·설정·빌드/PIE 변경 없음, 같은 작업 브랜치·기존 변경 보존, 커밋·푸시 없음.
- 관련: Docs/FabAssetReuseGuide.md, Feature/architect/2026-10-19_fab-owned-asset-reuse.md, Feature/doc/2026-10-19_fab-owned-asset-reuse.md, DevLog/20261019.txt.

## Sprint#5-37 — 현재 상태 main 통합과 완료 후 자동 병합 (2026-10-20)

- 브랜치: `Sprint#5-37-main-checkpoint`. 원격 확인 후 기존 누적 변경을 유지하며 통합 전용 작업 브랜치를 만들었다.
- 목표: 현재 상태를 main에 커밋·푸시하고, 이후 기능 완료마다 main까지 통합하는 사용자 상시 승인을 저장한다.
- 주요 변경: 누적 Sprint5 게임/에셋/도구/설계/증거의 스냅샷, AGENTS·Git/Lazy 규칙 보완, P1 조사·P2 검증·P3 게시 패킷과 한국어 기록. 이번 통합에서 새로운 게임 동작은 구현하지 않는다.
- 검증: 원격 브랜치 일치/조상 관계·파일 크기/고신뢰 자격 증명·생성 경로 조사, 프로젝트 파일 생성 성공(16.47초). Editor 빌드·스테이징 검사와 원격 게시 결과는 완료 전에 아래에 추가한다.
- 상태: 현재 검증·게시 진행 중. main 병합 성공 후 Sprint5 종료, 다음 기능은 `Sprint#6-1-...`.
- 관련: [계획](../Feature/architect/2026-10-20_main-checkpoint.md), [결과](../Feature/doc/2026-10-20_main-checkpoint.md), [증거](../Feature/doc/evidence/main-checkpoint-20261020/), [일일 로그](../DevLog/20261020.txt).

### Sprint#5-37 검증 완료

- 프로젝트 생성16.47초·전체 Editor 빌드144.03초/77 actions 성공, 격리 Write/Read 통합 QA 실패0·exit0, 생성 경로/활성 파일 공백 검사 통과. 기존 증거 원문 공백 보존과 캐릭터 교체 실패 저장 연결 후속 점검을 결과 문서에 명시했다. 스냅샷 커밋·승격·원격 게시 준비 완료.

### Sprint5 체크포인트 로컬 main 반영

- 스냅샷: `207c595`(Source103개·전체6084개 변경). 별도 인덱스로 검증 후 병행 구현9개를 제외하고 작업→develop→test→main fast-forward 완료.
- 상태: 로컬 main 통합 완료, Sprint5 체크포인트 종료. 신규 브랜치는 Sprint#6-1부터, 이미 만들어진 Sprint#5-38과 미완성 스킬트리 코드는 보존한다.
- 원격: chickenbuger/Rogue10m의 사용자/소유자·admin/push 권한·공개 상태를 확인했으나, 자동 승인 검토가 공개 자료 전송의 별도 승인을 요구해 push를 차단했다. 구체적 목적지/자료 범위를 제시하여 사용자 승인 대기. 원격 성공으로 보고하지 않는다.

## Sprint#5-39 — 보스 처치·런 정산·시작마을 복귀 (2026-10-20)

- 브랜치: `Sprint#5-39-boss-run-completion`. Sprint5 main 병합 전에 생성한 번호를 유지한다. 병행 스킬트리와 원본을 보존하는 별도 작업 폴더에서 개발했다.
- 목표: 아자토르의 실제 사망을 승리·기존 보상 정산·진행 저장·결과 화면·StartHub 귀환으로 연결하고 일반 사망/시간 초과도 종료한다.
- 주요 변경: R1 GameState 최초 종료 가드와 GameMode 소유 RunCompletionComponent·사망 연결, R2 네이티브 결과 UI/입력 차단·저장 실패 재시도·옵션 없는 마을 이동, R3 격리 QA/빌드/독립 검토와 기존 특성 fixture의 종료 계약 보완, R4 결과·DevLog·최종 통합 준비. 기존 보스 XP3,000/골드300을 한 번만 지급하며 새 보상·SaveGame 스키마·스킬트리 경제·원본 바이너리는 변경하지 않는다.
- 검증: 콘텐츠 고정 Editor 빌드90.27초 성공. 렌더된 실제 게임 Write53·Read12·Failure58·FailureRead12·Timeout33·Death32, 합계200검사·실패0·여섯 프로세스 exit0. 기존 Trait Effects 회귀101검사·실패0·exit0, 총301검사 통과. 실제 StartHub 포탈→OpenRunMap 저장 보스→플레이어 좌클릭 마지막 피해→결과창 Enter→StartHub 및 별도 프로세스 복원. Windows 읽기 공유 잠금의 실제 저장 실패·귀환 차단·재시도 보상 불변, 전체 Growth/Inventory GUID·골드/Combat/Town 스냅샷 일치. 실제 승리/실패/시간 초과 화면과 독립 코드 검토 완료.
- 보존과 한계: Write/Read 사이 공유 콘텐츠 변경을 검출한 뒤 연결을 제거하고207c595의6,417콘텐츠 파일·트리890c83ca26eb5ac410bbd9b0d34e406c9a3c44e3를 고정했다. 비교 완화 없이 재검증, 원본 소스·스킬트리·에셋 유지. 순간이동·AI정지·준비 중 임시 무적·1HP 보스 마무리·1초 타임아웃 fixture 범위이며 전체20분/밸런스/Shipping/D3D12/장시간GPU/네트워크는 미검증이다.
- 상태: 구현·빌드·핵심 QA·특성 회귀·독립 정적 검토·문서 완료. 최종 경로 검사·안전한 원본 통합·커밋·로컬 승격은 준비 중이다. 이번 기능의 push는 미시도이며 기존 체크포인트 채팅의 공개 전송 승인 대기 상태가 이어진다. main/원격 반영 완료로 보고하지 않는다.
- 관련: [설계](../Feature/architect/2026-10-20_boss-run-completion.md), [결과](../Feature/doc/2026-10-20_boss-run-completion.md), [최종 실행](../Feature/doc/evidence/boss-run-completion-20261020/frozen-runtime-launcher.log), [콘텐츠 고정](../Feature/doc/evidence/boss-run-completion-20261020/content-freeze.json), [독립 검토](../Feature/doc/evidence/boss-run-completion-20261020/review.md), [일일 로그](../DevLog/20261020.txt).

- Sprint#5-39 최종 통합: 필수 경로·67파일 범위·공백·PowerShell 구문 검사 통과 후 기능 커밋 `c680f1ba689d643956c954023d306c45404c8a54`, develop→test→main 로컬 fast-forward 완료. 원격 fetch와 조상 관계·체크아웃·이전 해시 대조 완료. 원본5-38 브랜치/HEAD 및 병행 스킬트리 보존을 Source14개 독립 대조로 확인. [통합 결과](../Feature/doc/evidence/boss-run-completion-20261020/local-integration.json). 최종 기록 문서는 별도 커밋으로 같은 경로로 승격한다. 공개 전송 승인은 계속 대기 중이며 push 미시도다.
