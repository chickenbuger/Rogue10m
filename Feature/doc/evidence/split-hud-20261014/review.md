# 좌우 분리 전투 HUD 독립 소스 리뷰

- 검토일: 2026-10-14
- 역할: Architect / Unreal Reviewer
- 방식: 승인 시안, 현재 소스, 착수 전 소스 스냅샷과 UE 5.8 엔진 구현을 읽어 비교했다. 소스·에셋 변경, 빌드, 프로젝트 실행, Editor/MCP 호출은 하지 않았다.
- 결론: 현재 검토 범위에서 미해결 finding은 없다. Python 전역 경로 복원과 실제 캡처에서 발견한 아이덴티티 비율 글자 겹침은 모두 해결을 재확인했다. `split02` 실제 캡처 3장과 런타임 7조건·1,334검사·실패 0 및 기존 저장 67개 바이트 보존 결과를 확인했다. 최종 Editor 재열기·저장 에셋 검증은 별도 담당자의 증거를 따른다.

## 검토 범위

주 검토 대상 3개는 `Source/Rogue10m/UI/Widgets/Rogue10mHudWidgetParts.h`, 같은 경로의 `.cpp`, `Scripts/Editor/ApplySplitCombatHUD.py`다. 보조 자료로 Bottom HUD `.h/.cpp`, Main HUD `.cpp`, Run HUD의 자원 View, Compact/ClassThemed 적용 도우미와 새 Split 런타임 QA fixture를 읽었다.

승인 시안은 `approved-concept.png`와 원본 `C:/Users/PC/.codex/visualizations/2026/10/11/01a12a0a-9437-7243-849b-511a17e6f850/hud-split-20261014/rogue10m-split-hud-mh-v1.png`를 기준으로 했다. 기존 동작 비교에는 `tmp/split-hud-20261014/before/Source/` 스냅샷을 사용했다.

| 파일 | 정적 소스 리뷰 당시 SHA-256 |
| --- | --- |
| `Rogue10mHudWidgetParts.h` | `CF303A0B130E5DE8103A90EC0DA6FA6091A5E7D9507B9611826BD2628E6F4F47` |
| `Rogue10mHudWidgetParts.cpp` | `737F65BA45C719D34B99940D048B6AF9C30046FF74B48393612A7714F93E99F3` |
| `ApplySplitCombatHUD.py` | `645E6A129020468CD694D2C4BBC25CE762D0080EDD04E0CCF819FA5134DEBE0D` |

## 확인 결과

1. **Native 프레임과 원형 게이지**: `URogue10mBeveledFrameWidget::NativePaint`는 장식 윤곽만 그린다. 게임 수치·잠금·쿨다운과 데이터 연결은 기존 파트를 사용한다. 부모 스타일 tint, disabled draw effect와 할당 geometry를 전달하며, 바깥 stroke는 inset 안에 들어간다. `Points[0]`을 로컬 `StartPoint`로 복사한 뒤 Add하므로 TArray 내부 원소를 자기 Add의 인자로 전달하던 assert 원인은 제거됐다. 모든 작성 프레임은 높이 38 이상이고, 150×150 Identity 파트의 원형 게이지 중심·반경 규칙도 유지된다. 저장 thumbnail paint와 실제 화면은 실행 확인 대상이다.

2. **아이콘·CDO·게임 View 보존**: `WeaponIconOverrides`는 `UPROPERTY(EditDefaultsOnly)` soft-texture 맵이며 Split Identity Blueprint CDO에만 Unarmed/Knuckle/GreatSword를 설정한다. Staff/Dagger 등은 기존 `InIdentityView.IconTexture`를 사용한다. `IdentityView = InIdentityView`를 그대로 보존하고 별도 transient `DisplayedIconTexture`에 표시 경로를 기록하므로 IdentityType, Current/Max, Normalized와 무기 숙련 상태를 바꾸지 않는다. `GetDisplayedIconTexture()`는 선택한 표시 경로이며, 로드된 실제 brush의 증거로 대체할 수 없다. QA는 `UI_IdentityIcon`의 resource object도 확인한다.

3. **비동기 교체**: 아이콘 경로 변경 시 `SetBrushFromTexture(nullptr)` 후 `SetBrushFromSoftTexture()`를 호출한다. UE 5.8 `Engine/Source/Runtime/UMG/Private/Components/Image.cpp`에서 전자는 기존 streaming handle을 무조건 취소하며, RequestAsyncLoad 완료 콜백은 요청 경로를 대조해 중단된 이전 콜백을 버린다. 이 경로에서 이전 클래스 이미지가 늦게 덮어쓰는 새 위험은 찾지 못했다. 로드 중 fallback 표시와 로드 완료 후 표시 전환은 기존 0.1초 View 갱신을 통해 처리한다. 실제 cold-load와 클래스 연속 전환은 QA 확인 사항이다.

4. **좌우 반응형·legacy·마나 흐름**: 공통 scale은 `min(1, 가용 폭 / (560 + 600), 가용 높이 / 270)`이며 가용 폭에서 좌우 margin 24씩과 중앙 gap 96을 뺀다. 두 패널의 slot과 bounds가 같은 scale을 사용한다. Main은 논리 `MyGeometry.GetLocalSize()` 변화에 맞춰 갱신하고 UMG DPI를 한 번 상속하므로 별도 viewport DPI를 다시 곱하지 않는다. 획득 알림은 실제 오른쪽 패널 위 24에 둔다. `bUseSplitCombatLayout=false`의 기존 중앙 배치와 HP 색은 유지한다. Split HP는 복사한 View의 FillColor만 초록으로 바꾸며, Wizard이면서 `ManaView.bVisible`인 경우 마나 View와 실제 파랑 FillColor를 전달한다. 그렇지 않으면 기존 스태미나 View를 전달한다.

5. **저장 범위·맵 보존**: Python 적용 대상은 새 Texture 2개, Split 파트 6개와 기존 Bottom/Main Blueprint 2개로 한정된다. 시작 시 무관한 dirty content/map package가 있으면 중단한다. 기존 Compact/ClassThemed 파트는 수정하지 않으며 `save_dirty_packages()`를 호출하지 않는다. 개별 Texture/Widget 저장만 수행한다. Main은 소유하는 Bottom 자식만 같은 바인딩 이름·최신 Blueprint 클래스로 재생성해 새 CDO 설정을 전달하며, 다른 위젯을 포함한 기존 이름 집합도 보존 검사한다. 소스에서 맵 변경·저장 호출은 찾지 못했다. 적용 중 예외는 앞서 저장한 신규 에셋을 남길 수 있으므로 이 작업은 원자적 트랜잭션이 아니다. 최종 saved-asset 목록의 실행 증거와 기존 맵·에셋 manifest 비교는 QA 담당자의 확인이 필요하다.

## 검토 중 해결된 항목

**P2 — 같은 Editor Python 세션의 Compact 후속 적용 경로 오염**: Split 빌더가 `compact.QUICK_PATH`와 `compact.ACQUISITION_PATH`를 변경하고 원래 checked setter만 복원하면, 이후 Compact 적용·롤백이 Split 경로로 저장할 수 있었다. 최종 파일은 원래 두 경로도 저장하고 `finally`에서 복원한다. 정상 완료와 예외 모두 복원되는 구문을 재확인했다. 현재 미해결 finding으로 남기지 않는다.

## 남은 실제 QA 확인

- 1280×720·1920×1080·울트라와이드·800×600 및 ApplicationScale 변경에서 실제 패널 geometry, 중앙 여백과 로그/획득 알림 비중첩을 확인한다. 매우 좁은 창의 최소 가독성은 bounds 검사만으로 판정하지 않는다.
- 아이템 행은 별도로 약 `208 / 324` 축소된다. 원래 14pt 키 글자가 공통 scale 1에서도 약 9pt로 보이므로, 작은 화면에서 수량·쿨다운·키 식별이 가능한지 실제 캡처로 판단한다.
- 네 클래스의 표시 경로와 실제 UImage brush, 불러오기 전 fallback, 실제 자원 비율 0·중간·1, 마나 enabled/disabled를 확인한다. 정적 검토만으로 화면 품질이나 테스트 통과를 주장하지 않는다.
- 변경된 프레임의 Editor thumbnail 저장, Blueprint CDO 값 보존, 두 기존 HUD 외 원본 에셋/맵 보존과 부분 실패 후 재적용을 확인한다.

최종 빌드·적용·런타임 QA·시각 리뷰 결과는 이후 증거 파일에 기록한다.

## 실제 캡처 시각 리뷰 — split01

`Rogue10m_Town_QA_split01/runtime-contract.txt`의 마지막 결과는 `cases=7 assertions=1330 failures=0`이며, `save-preservation.txt`는 기존 저장 파일 65개의 바이트 보존을 통과했다고 기록한다. 독립 시각 검토는 `_01_1920x1080`, `_02_2560x1440`, `_04_800x600` 실제 캡처 3장을 원본 크기로 열어 수행했다.

- 승인 시안의 좌측 생명력·직업 자원·레벨·EXP와 우측 스킬·아이템·원형 아이덴티티 분리를 확인했다. 세 화면에서 중앙 플레이 영역이 열려 있으며, HUD가 서로 겹치거나 화면 밖으로 잘리는 현상은 보이지 않는다. HP·스태미나 수치, 지팡이의 파란 마력 표시, 두 줄 HOLD 입력, 잠금, 시스템 로그와 획득 알림도 식별할 수 있다. 무기명 대검·지팡이·너클은 읽을 수 있다.
- **P2 — 아이덴티티 비율 글자의 원형 테두리 겹침**: 1920×1080의 `72%`와 2560×1440의 `100%` 하단 glyph가 금색 원형 테두리에 가려진다. 800×600에서는 같은 위치의 `72%` 하단을 더 구분하기 어렵다. 위젯 bounds 통과는 글자와 custom NativePaint 테두리의 실제 겹침을 검증하지 않으므로, 이 항목은 자동 QA 통과와 별도로 남긴다. 비율을 원 내부의 안전한 위치로 올리고 필요한 경우 무기명도 함께 조정한 뒤 새 캡처로 재확인해야 한다. 구현 담당자에게 전달했으며 이 리뷰에서는 소스·Editor를 변경하지 않았다.

이 시각 리뷰 시점의 미해결 항목은 위 P2 표시 결함 1건이다. 기능·저장 계약의 자동 QA 결과와 실제 표시 품질을 구분한다.

## 수정본 재확인 — split02 / P2 해결

같은 해상도의 `Rogue10m_Town_QA_split02/SplitCombatHUD_01_1920x1080.png`, `_02_2560x1440.png`, `_04_800x600.png` 3장을 원본 크기로 다시 열었다. 무기명 위치 y99/높이24, 비율 위치 y123/높이18과 아이콘 1.12배 표시 수정 이후 대검 `72%`, 지팡이 `100%`, 너클 `72%`의 하단 glyph가 금색 원형 테두리에 가려지지 않고 읽히는 것을 확인했다. 무기명과 비율도 서로 겹치지 않는다. **split01에서 발견한 P2 표시 결함을 해결로 판정한다.**

중앙 여백, 좌우 앵커, HP·스태미나/마력·EXP, 스킬 5개·아이템 4개와 획득 알림 분리는 유지된다. 800×600의 글자는 작지만 이번에 확인한 무기명·비율이 잘리거나 원형 윤곽에 가려지는 현상은 없다. 새 표시 회귀는 발견하지 못했다.

실제 결과 파일에서 `RESULT=SPLIT_COMBAT_HUD_PASSED cases=7 assertions=1334 failures=0`과 `PASS existing_save_bytes_preserved count=67`을 읽어 확인했다. 이 수정본의 독립 시각 리뷰에는 미해결 finding이 없다. 검토자는 문서만 갱신했으며 소스·에셋·Editor·빌드·실행에 손대지 않았다.
