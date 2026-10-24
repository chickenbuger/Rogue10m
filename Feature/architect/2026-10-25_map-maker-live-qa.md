# Map Maker Live Editor QA — Sprint#9-1

## Scope Gate

사용자 요청: 실제 Unreal Editor에서 맵 메이커를 열어 화면과 주요 기능을 검증하고 발견한 문제를 수정한다. 기준 `3fbfc3ea`의 NullRHI 검사 한계를 보완한다. 실제 화면·사용 조작은 root만 수행하고 다른 에이전트는 코드/검증 계약을 독립 검토한다.

| Packet / role | Goal / touched area | Exit / validation | Rollback boundary |
| --- | --- | --- | --- |
| L1 Architect | 실행 중 에디터/프로젝트·MCP·원본 상태 확인, 작업 브랜치 | 정확한 프로젝트 read-only call, 초기 파일/dirty/loader 증거 | 원본 파일 무변경 |
| L2 QA | 실제 패널 열기, 크기/스크롤/격자·영역 load/focus/release·필터·팔레트·배치·Undo/Redo·이동·native tools·PIE 상태 | 실제 screenshot/상태 증거, 모델/씬 readback으로 조작 확인 | QA 배치만 Undo, 원래 맵 저장 없음 |
| L3 Implementer/Builder | 발견 문제만 소스 수정, 필요한 빌드/재시작 | BuildEditor 및 원래 좁은 Automation, 동일 실제 조작 재검증 | 수정 editor 파일만 |
| L4 Reviewer/Doc Writer | 독립 검토, 한국어 DevLog·결과·Sprint·공개 게시 | 원본 보존·명시 파일 체크·main 승격/push | 무관한 작업 폐기/force push 없음 |

## Execution contracts

- 사용자가 실제 에디터 열기를 명시 요청했으므로 실행 중 인스턴스가 없으면 해당 프로젝트를 GUI 에디터로 시작한다. 현재 사용자 세션이 있으면 식별과 dirty 상태부터 읽는다.
- UI 조작은 computer-use의 @oai/sky, 현재 상태 확인과 자산 동작은 공식 Unreal MCP 도구를 우선한다. 세션에 네이티브 MCP 도구가 없으면 기존 `Scripts/InvokeUnrealMCP.py`로 동일 loopback endpoint/discovery/dispatch를 사용한다.
- 원본 OpenRunMap의 파일/외부 패키지 SHA와 dirty/loader 상태를 먼저 기록한다. 실제 맵에서 배치는 QA 소품만 생성하고 Undo로 되돌린다. 저장 버튼은 변경 없는 맵 또는 엄격한 별도 QA 맵에서 검사한다. 생산 맵 저장/오염은 하지 않는다.
- 외부 에셋 추가 없이 기존 StaticMesh를 재사용한다. 가이드의 소유 라이브러리 확인을 수행했다고 주장하지 않는다. QA 에셋이 필요하면 Unreal Editor API로만 생성/저장/정리한다.
- 에디터 공유 상태 변경은 순차 수행한다. 실패/타임아웃은 재조회 후 진단한다. 같은 실패 두 번이면 Architect/Reviewer로 돌아간다.
- 증거에는 실제 수행한 UI 조작과 코드/자동화 조작을 구분한다. 실행하지 않은 검사를 성공으로 기록하지 않는다.

## Reviewer → Architect 재진입

- 첫 고도 보완(-70도)은 야영지에서 통과했지만, 최외곽 `(3,-4)`에서 경계 벽 가림이 다시 재현됐다. 반복하지 않고 독립 Reviewer의 경계 기하 검토로 돌아갔다.
- 외곽 셀 중앙은 ±83,400cm, 벽 평면은 ±89,500cm로 최소 수평 거리 6,100cm다. 벽 상단은 Z=31,000cm다. -80도의 중앙 시선은 벽을 지날 때 최소 `8,000 + 6,100*tan(80°) ≈ 42,595cm`로 벽 상단을 넘는다. yaw/다른 모서리에도 대칭으로 성립한다. 최종 -80도로 수정하고 같은 셀과 야영지에서 실제 화면·카메라를 재검증한다.
- 공식 Live Coding은 90초 RPC timeout 이후 상태를 재조회했다. UBT는 128.63초 성공했지만 에디터의 런타임 패치 링크는 LNK2011/LNK1120으로 실패했다. 요청을 재실행하지 않고 정상 종료 후 BuildEditor 전체 빌드로 전환했다. 패치 성공을 주장하지 않는다.

## Exit Gate 실행 기록

- 최종 정식 빌드와 세 자동화 검사가 통과했다. -80도에서 같은 남동 외곽과 야영지를 실제 렌더 화면·카메라 readback으로 확인했다.
- 실제 패널의 임시 Cube 배치·숫자 입력, Ctrl+Z/Ctrl+Y, 이름 필터 선택, 다른 구역으로 이동을 수행했다. native Landscape/WP 화면과 Simulate·잘못된 맵 가드도 확인했다.
- 실행 취소 검증 후 다시 실행/이동한 QA 변경은 검증 전 baseline이 clean이고 dirty 두 패키지가 QA 소품과 폴더뿐임을 재확인한 뒤 `new_blank_map(False)`와 원본 재로드로 폐기했다. 이 경로는 저장하지 않고 원본을 복원하기 위한 정리다. 생산 맵 편집 저장은 하지 않았다.
- 도구 종료 후 원본 액터 1,243개, QA 소품 0개, dirty map/content 0개이며 에디터도 정상 종료했다. `final-checks.json`에서 원본 1,260파일 SHA 변경 0과 Content Git 변경 0을 확정했다.
