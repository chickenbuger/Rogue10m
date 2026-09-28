# Sprint#4-20 화면 비율 대응 전투 HUD

- 날짜: 2026-09-08
- 브랜치: Sprint#4-20-responsive-combat-hud
- 단계: Architect / 최종 구현 소스와 계약 정합성 갱신. 최종 실행 검증 결과는 별도 개발 결과 문서에 기록한다.
- 기준: 2026-09-08_lostark-hud-design-plan.md와 hud-layout-1920x1080.svg의 1000×160 전투 코어
- 우선순위: 화면 비율 변화에도 프레임·수치·슬롯이 함께 배치되도록 구조를 수정한다.

## 범위와 소스 근거

기존 ApplyClassThemedBottomHUD.py는 테마 프레임을 전체 폭으로 늘리면서 체력 x150, 자원 x1120, 아이덴티티 x912를 고정 배치한다. Main의 높이는 260이며, 진행도 영역은 52다. 이것이 폭 변화 시 배경과 콘텐츠 정렬이 분리되는 구조적 원인이다. BottomHUD의 C++ View 전달과 직업 테마 소유권은 유지한다.

이번 작업은 C++·Editor 생성/검증 스크립트·Editor 저장 Widget Blueprint·문서를 포함한다. 신규 전투 기능, 미니맵, 메뉴 재설계, 독립 스킬 쿨다운, 소모품 사용 연결은 포함하지 않는다. 기존 텍스처를 활용하며 생성 이미지와 픽셀 단위 일치를 완료 조건으로 삼지 않는다.

## 레이아웃 계약

```text
WBP_Rogue10mMainHUD / Canvas
└─ BottomHUDWidget: 전체 폭, 아래 anchor, 높이184
   └─ UI_BottomHUDCanvas
      ├─ UI_CombatScaleBox: 가용 폭, 높이160, 화면 아래 여백24
      │  └─ UI_CombatDesignSize: SizeBox1000×160
      │     └─ UI_CombatCanvas
      │        ├─ 직업 프레임과 배경
      │        ├─ 체력 / 현재 자원
      │        ├─ 행동5칸 / 직업 문장 / 잠긴 아이템4칸
      │        └─ 직업명·레벨 등 전투 관련 보조 표시
      └─ 경험치: 전체 폭, 아래 anchor, 높이4
```

- Main BottomHUD slot: anchors(0,1)-(1,1), alignment(0,1), offsets(0,0,0,184). 아래 alignment가 높이184를 위쪽으로 배치하므로 top offset에 -184를 중복 적용하지 않는다.
- Combat ScaleBox slot: anchors(0,1)-(1,1), alignment(0,1), offsets(24,-24,24,160). 높이160, 좌우 여백24, 화면 아래 여백24를 공통 계약으로 사용한다.
- ScaleBox: ScaleToFit / DownOnly, 자식 정렬 가로 중앙·세로 아래. 폭이 넓어져도 코어를 가로로 늘리거나 확대하지 않는다. 폭 부족으로 축소될 때도 아래 여백24를 유지한다.
- SizeBox: 디자인 폭1000·높이160. 프레임, HP, MP/스태미나, 문장, 슬롯, 관련 라벨은 같은 CombatCanvas 아래 둔다.
- 경험치는 ScaleBox 밖에 두며 전체 화면 폭을 따른다. 경험치와 장식은 입력을 차단하지 않는다.
- 행동5칸과 아이템4칸은 동일 부모 안에서 고정된 설계 좌표를 가진다. 가용 폭 부족 시 하나의 배율로 함께 줄어든다.
- 현재 Unreal DPI 정책을 그대로 사용한다. 이미 DPI가 적용된 논리 좌표에 직접 viewport 비율을 다시 곱하지 않는다. SetRenderScale 또는 매 프레임 Tick으로 비율을 보정하지 않는다.
- 1000×160, 아래24, 경험치4는 UMG 디자인 단위다. 실픽셀 값은 엔진 DPI 배율과 ScaleBox 축소 결과를 적용한 값이며, 모든 해상도에서 1000 실픽셀로 고정된다는 뜻이 아니다.

## 하위 위젯 정합성

ApplyResponsiveCombatHUD.py는 전용 하위 위젯6개와 BottomHUD/MainHUD2개, 총8개 Widget Blueprint만 명시적으로 생성·저장한다. 전용 경로는 /Game/Widget/Parts/Responsive이며 WBP_ResponsiveVitalBar, WBP_ResponsiveExperienceLine, WBP_ResponsiveQuickSlot, WBP_ResponsiveIdentity, WBP_ResponsiveIdentityBar, WBP_ResponsiveLogLine을 포함한다. 기존 공유 하위 위젯의96×116 문장이나34 높이 체력 바를 축소된 부모에 그대로 넣지 않도록 전용 내부 트리를 사용한다. 생성기는 ApplyClassThemedBottomHUD.py의 공통 Editor 도우미를 재사용한다.

전투 코어는 어두운1000×160 배경과1 단위 금속색 상·하단 선을 사용한다. 기존1920×208 전체 폭 테마 그림을 늘려 쓰는 대신 기존 T_HUD_GothicMedallion 텍스처를76×76 문장에 재사용하고 직업별 색을 적용한다. 새 그림 생성이나 텍스처 재수입 없이 동일한 중앙 geometry를 유지한다.

현재 View는 공격4개와 회피1개, 잠긴 아이템4개를 전달한다. 전용 슬롯은56×56, 간격8이며 Designer와 런타임 모두 행동5개·아이템4개를 따른다. 코어 내 HP는(24,14,352,20), 현재 자원은(624,14,352,20), 문장은(462,43,76,76), 행동 그룹은(24,64,312,56), 아이템 그룹은(664,64,248,56)에 놓인다. 컴팩트 레이블은 LMB/RMB/LMB+/RMB+/E이며 원래 입력 View와 전투 바인딩은 유지한다. 레벨은 코어 하단에, 경험치선은 별도 root에 표시한다.

## 주변 정보 계약

| 요소 | Main Canvas 배치 | 표시 범위 |
| --- | --- | --- |
| 대상 정보 | 상단 중앙 anchor(0.5,0), alignment(0.5,0), offsets(0,24,420,68) | 기존 MonsterInfoWidget 데이터 사용 |
| 남은 시간 | 우측 상단 anchor(1,0), alignment(1,0), offsets(-24,24,140,32) | 런 타이머 View 연결 |
| 시스템 로그 | 좌측 하단 anchor(0,1), alignment(0,1), offsets(24,-208,360,48) | 최근2줄, 하단 코어 위 여백 확보 |
| 획득 알림 | 우측 높이65% anchor(1,0.65), alignment(1,0), offsets(-24,0,260,72) | 최근3줄 |

로그 전용 행은 높이24, 줄바꿈 없이 말줄임표로 잘림을 처리한다. C++ MainHUD는 동일한 anchor·alignment·offset을 적용하고 BottomHUD의 디자인 크기와 여백 값을 사용한다. BottomHUD의 ApplyResponsiveLayout은 부모 ScaleBox와 SizeBox의 배치를 설정하며 수동 viewport 배율을 추가하지 않는다.
## Ultrawork Packets

| 패킷·담당 | 목표·입력 | 수정 영역 | 완료 조건 | 검증 명령·방법 | 롤백 경계 |
| --- | --- | --- | --- | --- | --- |
| P0 기획1 | 기존 기획과 현재 생성기에서 비율 계약 확정 | 본 설계문서 | 공통 계층·치수·DPI·검증 기준 공유 | 소스와 기존 기획 대조 | 본 신규 문서 |
| P1 개발1 | 런타임 HUD와 슬롯 크기를 새 계층에 맞춤 | Source/Rogue10m/UI/Widgets | 기존 View 바인딩 유지, 프리뷰와 런타임 치수 정합 | Scripts/BuildEditor.ps1 | 이번 C++ 변경 부분 |
| P2 개발2 | Editor 생성기를 수정하고 UE5.8에서 자산 재생성 | Scripts/Editor와 해당 Widget Blueprint | 단일 BottomHUD, 공통 ScaleBox, XP4, 행동5·아이템4, 위젯 컴파일 성공 | UE5.8 Editor Python으로 ApplyResponsiveCombatHUD.py 실행 | 이번 생성기 변경과 Editor 저장한 대상 자산; 기존 미커밋 변경 보존 |
| P3 검증1 | 계층·좌표·DPI·해상도 대응 검증 | HUD validator와 검증 결과 | 4개 목표 해상도에서 비율 유지·경계 내 배치 증거 확보 | UE5.8에서 ValidateResponsiveCombatHUD.py 및 Rogue10m.TestResponsiveHUD 런타임 geometry 검사 | validator와 이번 검증 문서 |
| P4 검증2 | C++/Editor 통합 회귀 검토 | 변경 diff, 상태 확인 | UObject·바인딩·입력·직업별 geometry·중복 HUD 위험 검토 | 빌드 로그, git diff --check, Scripts/CheckGeneratedChanges.ps1 | 검토 문서 |
| P5 통합·문서 | 빌드·적용 결과와 한계 정리 | Feature/doc, DevLog, Docs/SprintChangeLog.md | 한국어 일지·변경 요약·실제 검증 결과 기록 | 문서 대조 및 Git 상태 확인 | 이번 추가 문서와 append 부분 |

기획1·개발2·검증2의 역할을 단계별로 교대 운영한다. 공유 파일은 한 담당만 수정하고 Editor 자산 저장 및 빌드는 메인 조율 아래 직렬 실행한다. 동일 실패가2회 반복되면 Architect/Reviewer로 돌아가 원인을 재확인한다.

## 검증과 Exit Gate

1. UE5.8 설치 엔진과 현재 프로젝트를 사용해 Editor 타깃을 빌드한다. 스크립트 정적 검사만으로 엔진 적용 완료를 선언하지 않는다.
2. 변경 생성기를 Editor에서 실행하고 저장된 BottomHUD/MainHUD 및 수정한 하위 위젯이 컴파일되는지 확인한다. 생성기가 쓰는 UE API는 설치 엔진의 실제 API와 성공 로그로 확인한다.
3. 1280×720,1920×1080,2560×1440,3440×1440에서 전투 코어가 중앙이며 프레임·콘텐츠 상대 위치가 유지되는지 확인한다. 가능하면 4:3도 추가한다. 중앙 오차 목표는2 실픽셀 이하이며, 화면 밖 배치·슬롯 중첩은0건을 목표로 한다.
4. 측정 단계는 분리해서 기록한다: 저장된 UMG 계층 검사 / DPI 기반 수학적 계산 / Editor 렌더 / 실행 중 게임 viewport. 실제 게임 실행은 PIE 또는 Editor 실행 파일의 -game 모드로 구분해 기록한다. UMG 구조 검사만으로 게임 플레이 화면 검증 완료라고 쓰지 않는다.
5. 마법사 마력 전환, 나머지 직업 스태미나, 체력·경험치 갱신, 잠금 슬롯, 쿨다운·긴 입력명, 대상 정보 및 메뉴와의 충돌을 확인한다. 확인 못한 상태는 미검증으로 남긴다.
6. 이전 기획의720p 최소 글자 크기 및110% 사용자 배율은 별도 접근성 목표다. 이번에 compact 프리셋/배율 설정을 구현하지 않았다면 충족했다고 주장하지 않는다. 현재 DPI 보존하에서 텍스트 실제 크기를 기록해 다음 개선 판단에 사용한다.
7. GeneratedChanges와 diff whitespace 검사를 수행한다. 에셋은 Unreal Editor를 통해서만 저장하고, 생성/캐시 산출물을 직접 편집하지 않는다.
8. 사용자가 별도로 승인하지 않은 커밋·푸시는 하지 않는다. 기존 대량 미커밋 변경을 정리하거나 되돌리지 않는다.

## 관련 문서

- Feature/architect/2026-09-08_lostark-hud-design-plan.md
- ui-concepts/2026-09-08/hud-layout-1920x1080.svg
- Docs/LazyCodexHarness.md
- Docs/UnrealEngineeringRules.md
