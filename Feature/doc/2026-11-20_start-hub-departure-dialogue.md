# 시작 마을 관문과 대화 UI

2026-11-20 · Sprint#14-3 · UE 5.8

## 북쪽 관문

1. 중앙 우물에서 북쪽 관문까지 이어진 길을 따라간다. 관문 앞에서 `F 대화하기`가 나타난다.
2. 화면 하단 카드의 `마을에 남기`에 처음 포커스가 놓인다. `F` 또는 `Esc`로 닫고 준비를 이어갈 수 있다.
3. `탐험 출발`을 확인하면 입장 재화를 저장하고 수령한 출발 보급을 이관한다. 필드가 로드된 후 20분 시계가 시작된다. 저장에 실패하면 같은 창에서 재시도할 수 있다.

카드는 화면 하단 중앙에 두고 이름, 대사, 선택 두 개, 입력 안내 순으로 읽힌다. 관문과 접근로가 카드 위에 남는다. 기본 선택은 색과 테두리로 강조한다. 화면은 720p에서도 `마을에 남기`와 `탐험 출발`이 한눈에 들어오도록 구성한다. ImageGen은 실제 관문과 마을 스크린샷을 참고해 기와 지붕, 문지기, 옅은 안개를 제안했다. [북쪽 관문 시안](evidence/start-hub-departure-dialogue-20261120/north-gate-dialogue-concept.png)

## 길잡이 아린 UI 기획

아린과 플레이어가 화면 왼편에 보이는 상태에서 오른편 카드로 대화한다. 이름 아래에 짧은 대사를 한 줄 쓰고, 첫 화면에는 세 가지 선택만 둔다. 관문과 같은 짙은 녹회색 바탕, 금속빛 테두리, 밝은 한글을 사용한다. [아린 시안](evidence/start-hub-departure-dialogue-20261120/arin-dialogue-concept.png)

| 첫 선택 | 동작과 상태 |
| --- | --- |
| 출발 보급품 받기 | 미수령 시 포커스를 주고 기존 지급 검증으로 연결한다. 수령 후 `보급품 수령 완료`를 보여주고 다음 활성 항목에 포커스를 준다. 지급 수량과 재수령 조건은 기존 데이터를 따른다. |
| 장비와 물약 | 회복 물약, 장착 무기·방어구 강화, 영구 특성 강화로 연결한다. 현재 선택한 기능의 비용, 보유량, 효과를 표시한다. 이번 탐험 골드와 로비 재화에는 각각 이름을 붙인다. |
| 탐험 난이도 | 쉬움·보통·어려움을 고르고 다음 탐험에 적용할 난이도를 표시한다. 적 체력·피해·경험치 배율은 선택 상세에서 확인한다. |

대화 진입은 `F`, 선택은 방향키와 `Enter` 또는 마우스로 한다. 하위 화면의 `Esc`는 이전 메뉴로, 첫 화면의 `Esc`는 마을 조작으로 돌아간다. 보급을 받지 않았거나 난이도를 변경하지 않아도 북쪽 관문을 사용할 수 있다. 아린의 실제 안내 대사는 관문 앞에서 `F`로 출발한다는 문구로 맞췄다. 세 항목으로 묶는 새 패널은 구현용 기획이다.

두 시안은 Codex 내장 ImageGen으로 만들었다. 이미지의 배경, 장식, 입력 표기는 디자인 참조이며, 구현 화면의 지형, 모델, 현행 키 바인딩은 게임에서 확인한다. [최종 생성·편집 프롬프트](evidence/start-hub-departure-dialogue-20261120/imagegen-prompts.md) · [작업 계획](../architect/2026-11-20_start-hub-departure-dialogue.md)

## 구현 파일

- `Rogue10mRunPortal.h/.cpp`가 범위 진입, 대화 소유, 확인 및 저장 후 이동을 처리한다. `Rogue10mPlayerController.h/.cpp`는 대화 포커스와 이동·시점 입력을 관리한다. `Rogue10mTutorialWidget.h/.cpp`에 관문용 하단 카드와 두 선택을 추가했다. `Rogue10mTutorialDataAsset.h`의 아린 안내 세 문구에 관문 `F` 입력을 반영했다.
- `M_StartVillage_PortalGlow.uasset`와 `M_StartVillage_PortalSurface.uasset`은 대기 상태의 문양과 막을 옅게 만든다. `SV_PortalDestinationSign`과 `SV_PortalPrototypeSign`의 외부 액터 패키지를 저장해 영문 자동 입장 안내를 숨겼다. `PolishStartHubDeparture.py`로 지정된 네 패키지와 재로드를 검사한다. [패키지와 실제 설정](evidence/start-hub-departure-dialogue-20261120/editor-validation.json)
- 네이티브 검증 파일 `Rogue10mTownRunRuntimeTest.cpp`, `Rogue10mLegacyPortalTravelTest.cpp`, `Rogue10mBasicRunPlaytest.cpp`, `Rogue10mLobbyUpgradeRoundTripRuntimeTest.cpp`, `Rogue10mRunCompletionRuntimeTest.cpp`, `Rogue10mRunLearningRuntimeTest.cpp`와 에디터 검증 `ValidateOpenWorld.py`, `ValidateTutorialNPC.py`를 관문 확인 흐름에 맞췄다.
- `WorldMapProductionGuide.md`와 `world-layout-20261112.json`에 북쪽 관문 동선을 반영했다. 변경 이력과 검증 화면은 `SprintChangeLog.md`, `DevLog/20261120.txt`에 기록했다.

## 검증

- 맵 사전 로딩 변경을 포함한 UE 5.8 Development Editor 빌드를 통과했다. 별도 에디터 프로세스에서 지정한 네 패키지를 저장한 뒤 관문 위치, 이동 대상, 숨긴 표지, 아린 안내와 머티리얼 설정을 다시 읽었다. 문양은 실제 렌더에서 여전히 밝던 첫 결과를 보고 불투명도를 0.07로 낮췄다. 포탈 막은 0.34에서 0.06으로 조정했다.
- 1280×720 실제 게임에서 관문에 닿아도 대기 중인 상태, `F` 진입, 기본 선택의 `Enter` 취소, 다시 `F`로 대화를 연 후 방향키와 `Enter`로 출발하는 흐름을 확인했다. 사전 로드한 `OpenRunMap`을 재사용하고 입장 재화와 인벤토리, 보급 영수증 및 난이도를 유지했다. 렌더링 실행에서 60개 조건을 통과했다. [실제 게임 화면](../../DevLog/20261120_StartHubDeparture/north-gate-dialogue.png)
- 이전 버전 저장을 사용하는 합성 `Legacy1` 검증은 관문까지 실제 보행 입력, 기존 저장 복원, 정상 시계 시작을 포함해 17개 조건을 통과했다. 기존 아린 화면은 키보드·마우스·서비스·저장을 포함한 59개 조건을 통과했다.
- 마지막 두 프로세스 검사에서 출발 및 저장 59개, 게임 재시작 19개를 통과했다. 미완료 런 정리 후 입장 시점 재화와 영구 투자가 보존되고 새 런의 성장과 장비가 초기화되며, 아린 보급을 1회 받을 수 있었다. 최종 격리 검증의 로그 오류는 0개였다. [검증 기록](evidence/start-hub-departure-dialogue-20261120/runtime-validation.json)

첫 검증기는 자동 무기 장착과 런 경험치로 영구 특성을 살 수 있다고 가정해 출발 전에 멈췄다. 검증용 프로필에서 실제 가방의 무기를 장착하고, 기존 최고 레벨을 보존하며 별도 로비 예산을 준비했다. 출발 후 재시작은 중단된 런을 정리하고 새 준비 단계로 들어가는 규칙에 맞춰 비교하는 값과 검증 항목을 갱신했다.
