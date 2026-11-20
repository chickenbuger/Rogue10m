# 인벤토리 시안 실제 적용

StartHub와 OpenRunMap에서 사용하는 실제 `I` 창에 시안의 왼쪽 고딕 프레임, 정사각형 격자, 아이템 점유 영역, 한 줄 골드·무게 표시, 오른쪽의 장착 비교 카드를 반영했다. 게임에 연결된 아이템 이미지와 시안의 프레임에서 만든 투명 텍스처를 사용한다. [시안](evidence/inventory-concept-20261120/inventory-concept.png) · [적용 계획](../architect/2026-11-20_inventory-concept-implementation.md)

## 화면과 조작

- 기본 가방을 58×58 논리 셀 100개로 그린다. 실제 빈 칸의 경계가 720p에서도 보이도록 선에 논리 폭 2px를 준다. 아이템은 데이터에 지정된 전체 영역에 배경·아이콘을 그린다. 가죽 갑옷은 2×3, 철제 장검은 1×3이다.
- 투구 위에 포인터를 올리면 수호자의 투구와 장착 중인 철제 투구가 한 줄에 표시된다. 두 카드에는 장비 수치가 표시되며, 작은 글씨에 실제 설명·무게와 교체 시 증감을 담았다. 켜진 화면의 공간에 맞춰 카드가 이동한다.
- 드래그하는 동안 격자에서 놓을 영역을 강조한다. 가방과 장비창은 `I`·`P`로 각각 열고 닫는다. Pawn이 바뀌면 두 창이 현재 캐릭터의 인벤토리에 다시 연결되고 이전 장비 메뉴와 프리뷰를 정리한다.
- 검증 화면의 월드와 아이템 아이콘, 게임 HUD는 실제 게임의 표시다. 시안의 장식, 제목과 키 안내, 점유 영역과 비교 구성이 적용된 위치를 확인할 수 있다. [대표 게임 화면](../../DevLog/20261120_InventoryConceptUI/inventory-comparison.png) · [720p 화면](evidence/inventory-ui-20261120/inventory-comparison-720.png)

## 구현 산출물

| 파일 | 적용 내용 |
| --- | --- |
| `WBP_InventoryWindow`, `WBP_InventoryCell`, `WBP_InventoryItem`, `WBP_InventoryItemTooltip` | 프레임·셀·아이템 영역과 실제 가방 위에 표시되는 비교 카드 |
| `T_UI_InventoryConceptFrame` | ImageGen으로 승인 시안에서 분리한 가방용 투명 프레임 |
| `Rogue10mMenuWindowWidgets`, `Rogue10mInventoryItemHover`, `Rogue10mPlayerController` | 아이템 표시, 호버, 드롭 영역, 현재 인벤토리 연결 |
| `ApplyInventoryConcept.py`, `BuildInventoryConceptTooltip.py`, `ValidateMenuWidgetAssets.py` | 기존 인벤토리 Blueprint 적용과 바인딩 검사 |
| `VerifyInventoryUI.ps1`, `Rogue10mInventoryUIRuntimeTest.cpp` | 격리된 StartHub에서 실제 Slate 경로·렌더·저장 복원 검사 |

[프레임 원본](evidence/inventory-ui-20261120/T_UI_InventoryConceptFrame.png) · [프레임 생성 프롬프트](evidence/inventory-ui-20261120/T_UI_InventoryConceptFrame.prompt.txt)

## 검증

| 검사 | 결과 |
| --- | --- |
| UE 5.8 Editor 빌드 | 최신 통합판 `122cb740` 기반 11개 빌드 작업 성공 |
| 저장된 위젯 재컴파일 | 필수 바인딩, 1162×1353 텍스처, 580×580 격자와 2px 셀 테두리 정상, 로그 오류 0건 |
| StartHub 1920×1080, 1280×720 | 각각 79개 통과. 100셀과 6개 아이템의 실제 그리기 영역, 투구 수치, `I`·`P`, 드래그 허용·거부, 우클릭 장착·해제·포션을 검증했다. |
| PNG 가시성 | 두 해상도 모두 빈 영역에서 가로·세로 내부선 각각 9개를 확인했다. |
| 새 프로세스 저장 복원 | 8개 통과. 옮긴 장검, 소비 수량, 해제한 투구와 골드를 복원했다. |

세 게임 실행 로그의 오류는 0건이다. 조작 검증은 렌더된 Slate 위젯에 프로그램으로 키와 포인터 이벤트를 전달한 결과다. 별도 QA 프로필과 작업 체크아웃을 사용했고, 각 실행에서 테스트 외 저장의 해시가 일치했다. 공유 편집기의 기존 오류 4건을 기록했으며 작업 시작 경계 이후 신규 오류는 0건이다. [검증 요약](evidence/inventory-ui-20261120/verification.json) · [초기 실패와 수정](evidence/inventory-ui-20261120/initial-checks.md)
