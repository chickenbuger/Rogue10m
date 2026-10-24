# Map Maker Live Editor QA — Result

- 작업: `Sprint#9-1-map-maker-live-qa`, 2026-10-25.
- 기준: `3fbfc3ea`, 원본 맵 1,260파일 초기 SHA 및 dirty maps/content 0.
- [계획](../architect/2026-10-25_map-maker-live-qa.md), [가이드](../../Docs/WorldMapMakerGuide.md).

## 실제 화면 재현

1. Window 메뉴의 World Map Maker가 실제 GUI에서 열린다. 현재 WP/streaming/256m/700m 표시가 정상이다.
2. 약 356px 폭에서 좌표·개수가 잘려 읽기 어렵다. [화면](evidence/map-maker-live-qa-20261025/panel-narrow-before.png).
3. 남쪽 구역의 Load + focus는 기존 -8.47도 pitch를 유지해 카메라를 Y=-94847cm로 보낸다. 실제 화면이 경계 벽에 가려진다. [카메라](evidence/map-maker-live-qa-20261025/focus-camera.json), [화면](evidence/map-maker-live-qa-20261025/focus-wall-before.png).
4. 구역 로드 뒤 청록 상태가 유지되며 다른 구역을 선택할 수 있다. 변경 없는 맵의 저장 버튼은 파일을 쓰지 않고 완료한다. 초기와 이 검사 후 dirty 패키지는 0개다.

## 수정 / 재검증

- 격자를 400px 가독 폭과 축소 금지 가로 스크롤로 보완했다. 약 362px의 기존 사용자 창에서 [왼쪽](evidence/map-maker-live-qa-20261025/panel-narrow-fixed-left.png)·[오른쪽 끝](evidence/map-maker-live-qa-20261025/panel-narrow-fixed-right.png)을 실제 드래그해 글자와 모든 열 접근을 확인했다. 새 독립 창 기본 크기는 560×900이며 저장된 사용자 레이아웃은 유지한다.
- 첫 포커스 -70도는 [야영지에서 개선](evidence/map-maker-live-qa-20261025/focus-camp-fixed.png)됐으나 최외곽 `(3,-4)`에서 [다시 가림](evidence/map-maker-live-qa-20261025/focus-edge-70.png)을 확인했다. Reviewer/Architect로 돌아가 벽 상단과 외곽 셀 거리를 검토한 뒤 -80도로 보완했다. 현재 월드의 활성 viewport만 조작하며 pilot/lock은 거절한다.
- 공식 Live Coding은 RPC 제한 시간을 넘겼고, 완료 후 UBT 성공과 별개로 런타임 패치 LNK2011/LNK1120 실패를 확인했다. [실패 구분 기록](evidence/map-maker-live-qa-20261025/livecoding-result.json). 재요청 없이 정상 종료 후 [정식 빌드](evidence/map-maker-live-qa-20261025/build-final.log) 115.11초 성공으로 복구했다.
- [최종 회귀](evidence/map-maker-live-qa-20261025/automation-final/result.json): 57.22초, 실제 3개 Success, 실행기/에디터 exit0, 자동화 실행 전후 원본 1,260파일 SHA 변경0. 실제 변경의 저장·재로드는 격리된 WP fixture에서 검증하고, 최종 GUI 조작은 아래에 별도 기록한다.

## 최종 GUI / 회귀 검증표

| 검증 항목 | 실제 조작과 확인 결과 | 근거 |
| --- | --- | --- |
| 좁은 패널 | 약 362px 창에서 가로 스크롤을 양 끝까지 드래그했다. 좌표·개수가 읽히고 8개 열에 모두 접근한다. | [왼쪽](evidence/map-maker-live-qa-20261025/panel-narrow-fixed-left.png), [오른쪽](evidence/map-maker-live-qa-20261025/panel-narrow-fixed-right.png) |
| 남동 외곽 포커스 | `(3,-4)`에서 최종 pitch -80°. 카메라 Y=-86,763.94cm·Z=27,079.81cm로 프레이밍되며 경계 벽 뒤로 가려지던 문제를 해소했다. | [화면](evidence/map-maker-live-qa-20261025/focus-edge-final.png), [카메라](evidence/map-maker-live-qa-20261025/focus-camera-edge-final.json) |
| 야영지 포커스 | 남쪽 야영지 구역에서도 pitch -80°로 지면과 배치 위치를 내려다본다. | [화면](evidence/map-maker-live-qa-20261025/focus-camp-final-placement.png), [카메라](evidence/map-maker-live-qa-20261025/focus-camera-camp-final.json) |
| 팔레트·지형 배치 | Content Browser의 기존 Engine Cube를 사용하고 UI에 Height 125cm / Yaw 45° / Scale 20을 입력했다. 실제 배치 버튼으로 `(12,800,-64,000,3,609.155)`cm에 큐브 1개가 생겼고 yaw 45°·scale 20을 확인했다. | [배치 화면](evidence/map-maker-live-qa-20261025/panel-placed.png), [읽기 결과](evidence/map-maker-live-qa-20261025/live-placed.json) |
| 실제 Undo / Redo | 키보드 Ctrl+Z로 큐브가 0개·dirty 패키지 0개가 되고, Ctrl+Y로 같은 액터 1개와 위치·회전·크기가 복원됐다. | [Undo](evidence/map-maker-live-qa-20261025/live-undo.json), [Redo](evidence/map-maker-live-qa-20261025/live-redo.json) |
| 이름 필터 선택 | `OW_MM_Cube` 필터를 입력하고 영역 선택 버튼으로 해당 액터 1개를 선택했다. | [화면](evidence/map-maker-live-qa-20261025/filter-selection.png) |
| 선택 그룹 이동 | 목적지 `(1,-3)` 선택 후 실제 Move 버튼으로 X=38,400cm·Y=-64,000cm로 이동했다. 기존 Z·yaw 45°·scale 20을 유지했다. | [화면](evidence/map-maker-live-qa-20261025/panel-moved.png), [읽기 결과](evidence/map-maker-live-qa-20261025/live-moved.json) |
| 네이티브 도구 연결 | Landscape와 World Partition 버튼으로 각각 기본 도구를 열었다. 지형 sculpt/paint는 실행하지 않았다. | [두 도구 화면](evidence/map-maker-live-qa-20261025/native-landscape-partition.png) |
| Simulate 보호·복귀 | Simulate 중 편집 버튼이 비활성화되고 Stop Play 메시지가 표시됐다. 종료 후 정상 편집 상태로 돌아왔다. | [보호 상태](evidence/map-maker-live-qa-20261025/simulate-guard.png), [종료 후](evidence/map-maker-live-qa-20261025/after-simulate.png) |
| 다른 맵 보호 | blank map에서는 편집을 차단했다. 원래 OpenRunMap을 다시 로드하자 정상 복귀했다. | [다른 맵](evidence/map-maker-live-qa-20261025/wrong-map-guard.png), [복귀](evidence/map-maker-live-qa-20261025/cleanup-restored.png) |
| 변경 없는 맵 저장 | clean 상태에서 저장 버튼을 눌러 정상 완료를 확인했다. 이 조작은 dirty 변경의 디스크 저장 시험으로 계산하지 않는다. | [화면](evidence/map-maker-live-qa-20261025/save-clean.png) |
| 변경 저장·독립 재로드 | 별도 NullRHI 에디터의 고유 WP fixture에서 OFPA 배치·이동·Undo/Redo·저장·맵 재로드 후 위치/크기/메시 참조 보존을 검증했다. `EditorIntegration`, `EnvironmentPolicy`, `GridBounds` 모두 Success다. | [실행기](evidence/map-maker-live-qa-20261025/automation-final/result.json), [원본 보고서](evidence/map-maker-live-qa-20261025/automation-final/report/index.json) |

## 원본 보존과 종료

실제 GUI에서 만든 임시 큐브의 액터·폴더 패키지 2개만 dirty인 것을 [정리 직전 읽기](evidence/map-maker-live-qa-20261025/live-before-cleanup.json)로 확인했다. 이 임시 변경을 저장하지 않고 `new_blank_map(False)`로 폐기한 뒤 원본 OpenRunMap을 다시 로드했다. [최종 읽기](evidence/map-maker-live-qa-20261025/live-final.json)는 로드된 액터 1,243개, 맵 메이커 액터 0개, dirty map/content 각 0개다. 에디터는 정상 종료했고 `git status --short -- Content`도 변경 0개였다.

첫 정리 시도는 에디터가 반환한 상대 프로젝트 경로를 경로 가드가 거절해 변경 실행 전에 멈췄다. `Path.resolve()`로 경로를 정규화하고 보호 조건을 다시 통과한 뒤 위 정리를 완료했다.

자동화 실행 전후 원본 1,260파일 해시 변경 0은 위 실행기 결과에서 확인했다. 실제 GUI 종료 후에도 [최종 증거 검사](evidence/map-maker-live-qa-20261025/final-checks.json)가 성공했다. 원본 1,260파일 SHA 변경 0, Content 상태 공백, 최종 액터 1,243개·QA 소품 0개·dirty map/content 각 0개를 확인했고 배치·키보드 Undo/Redo·이동·양 구역 -80° 포커스의 읽기 assertion도 통과했다. 코드 독립 리뷰에서 차단 결함은 남아 있지 않다.

## 범위와 게시 상태

- 이번 결과는 실제 렌더된 Editor 패널과 주요 입력을 검증했다. 전체 20분 런·장시간 스트리밍 성능·cook/Shipping·실제 Landscape sculpt/paint 변경은 검증하지 않았다.
- 생산맵에 임시 큐브를 디스크 저장하지 않았다. 변경 저장·재로드의 근거는 격리 fixture이고, 생산맵의 저장 버튼 검사는 clean 상태의 성공 경로다.
- 최종 자동화는 일반 성공 2개와 경고를 동반한 성공 1개다. `EditorIntegration`의 네이티브 경고 4개(참조 수집 fallback 1개·fixture/HLOD AssetRegistry 재인식 3개), 최초 Live Coding 실패·-70° 외곽 재현 증거를 보존한다. UBT 성공을 런타임 패치 성공으로 해석하지 않는다.
- [Editor 원문 로그](evidence/map-maker-live-qa-20261025/editor-final-live.log)의 frame 0에는 기존 startup Smoke 검사 오류 `Condition failed` 15개가 있다. 수정 전과 회귀 전 실행에도 같다. [설치된 엔진 소스 검토](evidence/map-maker-live-qa-20261025/review.md)에서 시작 시 Smoke 검사와 오류 출력 경로를 확인했지만 개별 15개 검사의 이름·정확한 원인은 미확정이며 엔진 수정은 하지 않았다. 위 오류 0은 선택한 맵 메이커 자동화 3개 보고서의 결과다.
- GUI·정식 빌드·회귀·[독립 리뷰](evidence/map-maker-live-qa-20261025/review.md) 완료. 기능 커밋 `8ca0523309866aed5aa48fc4b435c02845241d8a`에 명시한 94파일만 포함했다. 생성 경로·공백·JSON 50개·Python 2개·문서 링크 38개·DevLog 추가 전용 검사가 통과했다. 원격 변경을 다시 확인한 뒤 `develop -> test -> main`을 fast-forward하고 네 브랜치를 일반 atomic push했다. [직접 원격 SHA 대조](evidence/map-maker-live-qa-20261025/publication.json)가 모두 일치했다. 이 완료 기록도 소스/콘텐츠 변경 없는 문서 체크포인트로 같은 흐름에 반영한다.
