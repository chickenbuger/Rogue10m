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
