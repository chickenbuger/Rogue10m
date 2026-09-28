# Sprint#4-20 화면 비율 대응 전투 HUD 구현 결과

- 날짜: 2026-09-08
- 브랜치: Sprint#4-20-responsive-combat-hud
- 엔진: 설치된 UE 5.8.2, Changelist 56702186
- 상태: 구현·UE 자산 적용·빌드·실제 게임 6조건 검증 완료

## 적용한 동작

전투 HUD의 장식·체력·자원·문장·슬롯을 1000×160 공통 좌표계로 묶었다. 화면 아래 중앙에 놓고 좌우·아래 24 여백을 유지한다. 가용 폭이 부족할 때 전체를 같은 비율로 축소하며, 넓은 화면에서는 가로로 늘리지 않는다. 엔진 ShortestSide DPI를 그대로 상속하므로 여기의 치수는 실제 픽셀이 아닌 UMG 디자인 단위다.

하단 높이는184, 전투 코어는160이며 XP는 축소 영역 밖에서 전체 폭의4단위 선으로 표시한다. 행동5칸과 잠긴 아이템4칸은 모두56 정사각형, 간격8로 통일했다. 문장은76 정사각형이다. 실제 입력 LMB/RMB/LMB+/RMB+/E를 유지하고 원래 입력 설명은 툴팁에 남긴다.

상단 중앙 대상 정보420×68, 우상단 타이머140×32, 좌하단 로그 최대2줄, 우측 획득 알림 최대3줄을 화면 가장자리에 고정했다. 빈 System/TextBlock 미리보기는 제거하고 실제 메시지와 획득 수량을 표시한다. 타이머는 기존 런 활성 상태에 따라 표시한다.

어두운 반투명 중앙 패널, 얇은 금속선, 기존 직업 문장의 색 변화로 기획의 분위기를 반영했다. 새로운 스킬 아이콘이나 생성 시안의 던전·보스 아트는 추가하지 않았다. 현재 데이터에 아이콘이 없는 스킬은 키 레이블만 보인다.

## 수정 영역

- C++: BottomHUDWidget, MainHUDWidget, HudWidgetParts의 헤더·구현 총6파일. 새 Tick 없이 기존 갱신 경로 사용.
- Editor 적용·검증: ApplyResponsiveCombatHUD.py, ValidateResponsiveCombatHUD.py.
- 구형 생성기 보호: ApplyClassThemedBottomHUD.py, BuildGothicCombatHUD.py에서 새 계층 덮어쓰기 차단.
- Editor 저장 자산8개: 기존 MainHUD/BottomHUD와 Responsive 폴더의 VitalBar, ExperienceLine, IdentityBar, QuickSlot, Identity, LogLine.
- 실제 게임 검사: Source/Rogue10m/Tests/Rogue10mResponsiveHUDRuntimeTest.cpp.

기획1·개발2·검증2의 역할을 단계별 교대로 운영했다. 빌드와 Editor 자산 저장은 메인이 직렬 실행했다. 리뷰에서 발견한 XP 초기 예시값37% 잔류는0으로, 획득 수량 누락은 본문·툴팁의 수량 표기로 수정했다.

## 검증

- Editor 타깃: Scripts/BuildEditor.ps1 -NoHotReload 최종 성공(32.44초).
- 설치 UE Editor에서 적용 및 저장 자산 계층·바인딩·anchor·초기값 검증: RESULT=RESPONSIVE_HUD_SETUP_PASSED, 자산8개 저장.
- 실제 게임의 6조건: 1280×720, 1920×1080, 2560×1440, 3440×1440, 800×900/UI배율125%, 1920×1080 복귀.
- 검사 항목: 실제 viewport 크기, 중앙정렬, 경계, 일정한 가로세로 비율, 정사각 문장, 슬롯 수·간격·중첩, 전체폭 XP, 직업4종 테마, 마나/스태미나, 타이머, 획득 수량.
- 최초 검사에서 마나 활성화 및 런 시작을 생략한 테스트 조건 오류를 발견했다. 테스트 프로세스에만 명시적 상태를 설정한 최종 실행에서 모두 통과했다. 실제 게임 규칙을 바꾸지 않았다.
- CheckGeneratedChanges.ps1의 Harness 경로 검사와 git diff --check 통과. 기존 대량 미커밋 에셋에 대한 일반 경고는 유지된다.

이번 검증은 Editor 실행 파일의 실제 -game 렌더링이다. 패키지 빌드·메뉴 전체 재설계·접근성 최소 글자 크기 검증은 범위에 포함하지 않았다. 720p에서는 엔진 DPI에 의해 글자도 줄어들며, 작은 글자 가독성을 위한 별도 UI 배율 설정은 후속 과제다.

## 재실행

1. Scripts/BuildEditor.ps1 -NoHotReload.
2. Editor Python에서 Scripts/Editor/ApplyResponsiveCombatHUD.py 실행(배치 재생성이 필요할 때).
3. Scripts/Editor/ValidateResponsiveCombatHUD.py 실행.
4. 별도 테스트 게임에서 Rogue10m.TestResponsiveHUD 실행. 테스트가 런·자원 상태와 화면 크기를 바꾸고 6장 캡처한 뒤 해당 프로세스를 종료하므로 일회용 검증 세션에서 사용한다.

자동 테스트의 엔진 시작 옵션에 -NoRemoteShaderCompile을 사용해 로컬 셰이더 컴파일을 선택했다. 프로젝트 설정은 변경하지 않았다.

## 변경 보존과 증거

기존 Widget31개 해시를 작업 시작 기준과 비교했으며 이번에 변경한 기존 자산은 MainHUD와 BottomHUD 두 개뿐이다. 신규 전용 파트6개를 추가했다. 기존 미커밋 코드·에셋을 되돌리지 않았고 커밋·푸시하지 않았다.

- 설계: Feature/architect/2026-09-08_responsive-combat-hud.md
- 적용 로그: tmp/responsive-hud/setup-editor-final.log
- 기존 자산 백업·해시: tmp/responsive-hud-backup-20260908/

## 최종 실제 실행 결과

RESULT=RESPONSIVE_HUD_PASSED cases=6 failures=0

| 화면 조건 | 논리 화면 크기 | 전투 코어 크기 | 문장 크기 | 결과 |
| --- | --- | --- | --- | --- |
| 1280×720 | 1921.9×1081.1 | 1000×160 | 76×76 | 통과 |
| 1920×1080 | 1920×1080 | 1000×160 | 76×76 | 통과 |
| 2560×1440 | 1920×1080 | 1000×160 | 76×76 | 통과 |
| 3440×1440 | 2580×1080 | 1000×160 | 76×76 | 통과 |
| 800×900, UI125% | 864.6×972.7 | 816.6×130.7 | 62.1×62.1 | 통과 |
| 1920×1080 복귀 | 1920×1080 | 1000×160 | 76×76 | 통과 |

축소 조건에서 좌우24 여백을 유지하며 문장과 슬롯이 함께 줄어든다. 기본4해상도만으로는 엔진 DPI가 논리높이를1080 부근으로 맞춰 DownOnly 분기가 실행되지 않으므로, 좁은 창과 UI125%를 별도로 검사했다. 수치는 실제 위젯의 캐시 geometry를 루트 논리 좌표로 변환해 측정했다.

- 실제 캡처6장: Feature/doc/images/responsive-hud-20260908/
- 최종 실행 로그: Feature/doc/evidence/responsive-hud-20260908/runtime-verified.log
- 실제 Editor 적용 로그: Feature/doc/evidence/responsive-hud-20260908/setup-editor-final.log
- 첫1080p 육안 확인: 우상단 진행 타이머, 좌하단 실제2줄 메시지, 아래 중앙 코어 확인. 과거 System/TextBlock 패널 잔류 없음.
- 대상 정보는 현재 타깃 유무에 따라 숨겨지므로, 저장 계층 및 anchor를 검사했고 모든 대상 상태를 실전 조작한 것으로 주장하지 않는다.
- 로그에는 기존 엔진/플러그인 초기화 진단도 포함되어 있다. HUD 전용 성공 표식과 실제 렌더링 검사를 기준으로 결과를 판단했다.

독립 시각 검증자는 최종1080p·3440×1440·800×900 캡처3장을 확인했다. 새 잘림·겹침은 발견하지 못했고, 타이머 여백·로그2줄 분리·과거 미리보기 제거를 확인했다.
