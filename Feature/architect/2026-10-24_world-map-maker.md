# World Map Maker — Architect / Sprint#8-1

## Scope Gate

현재 플레이맵 `/Game/Rogue10m/Maps/OpenRunMap`은 이미 World Partition RuntimeHashSet(256m 셀, 700m 로드 범위)이다. 1.8km × 1.8km Landscape와 기존 포탈·런·저장·보스 계약을 유지한다. 별도 청크 런타임이나 맵 재생성은 도입하지 않는다. 에디터 전용 C++ 모듈의 도킹 패널에서 **편집 작업 영역**을 관리한다. 작업 격자는 런타임 셀 ID와 다른 개념이며 화면에 명시한다.

근거: `Docs/WorldStructure.md`, `Source/Rogue10m/Editor/Rogue10mOpenWorldAuthoring.cpp`, `Feature/doc/evidence/open-world-20261005/reload.json`, `connection.json`, 로컬 UE5.8 WorldPartition/LoaderAdapter API. Epic [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine), [Data Layers](https://dev.epicgames.com/documentation/unreal-engine/world-partition---data-layers-in-unreal-engine).

## Ultrawork Packets

| Packet / role | Goal / touched area | Completion / validation | Rollback boundary |
| --- | --- | --- | --- |
| M1 Architect | 조사·모듈 경계·현재 WP 유지 | 현재 맵·기존 보호 구조·편집 격자 계약 확인 | 이 계획 문서 |
| M2 Implementer / model | `Source/Rogue10mEditor/Private/MapMaker` 모델: 256m 작업격자, 개별 소유 로더, focus/select, Content Browser StaticMesh 배치, 선택 환경물 이동, 맵 저장 | 음수/경계·다른맵/PIE 거절·Undo·저장/재로드 자동화 | 새 editor 모듈, 원본 콘텐츠 무변경 |
| M3 Implementer / UI | Window 메뉴의 World Map Maker 도킹 패널, 8×8 격자/상태/필터/메시팔레트/높이·회전·크기/로드·해제·선택·이동·저장, native Landscape/WP 진입 | 빌드 및 실제 Slate 생성/핵심 작업 검증 | 새 UI 파일과 모듈 등록 |
| M4 Builder / Reviewer | projectfiles, BuildEditor, 좁은 Unreal Automation, 독립 Unreal 리뷰 | 실패 수 0, 저장된 원본 맵 불변, 생성경로 검사 | fixture는 임시 맵 복제본만 변경 |
| M5 Doc Writer / Exit | 사용 가이드·결과·한국어 DevLog·Sprint 기록, 명시 범위 commit 및 develop→test→main | 검증·원격/조상/변경 범위 확인 후 일반 push | 원본 변경을 버리거나 force push하지 않음 |

## Implementation contracts

- Editor 전용 모듈 `Rogue10mEditor`로 Slate/UnrealEd 의존성을 런타임과 분리한다. Tick 없이 버튼/에디터 이벤트 기반으로 갱신한다.
- 작업 격자는 월드 원점 기준 25,600cm, 좌표 -4..3의 8×8, 실제 편집 범위 ±90,000cm로 가장자리를 자른다. XY 반열린 경계와 마지막 +90,000cm 처리를 명시하고 테스트한다.
- `FLoaderAdapterShape`를 WP의 `CreateEditorLoaderAdapter`로 소유한다. 툴이 만든 핸들만 해제한다. 다른 사용자 영역을 전역 언로드하지 않는다. 맵 변경/패널 종료 시 정리한다.
- 지원 맵/Editor world/PIE 가드를 모든 읽기·변경 진입에서 확인한다. 테스트는 별도 prefix의 복제 맵만 허용하며 생산 콘텐츠에 저장하지 않는다.
- 편집 선택/이동은 프로젝트 소유 태그가 있는 환경 `AStaticMeshActor`만 지원한다. 보스 바닥·Gameplay·Terrain·Boundary 계열과 비소유/시스템 액터는 보호한다. 게임플레이 액터는 기존 Details/Outliner에서 별도로 편집한다.
- 팔레트는 Content Browser의 기존 `UStaticMesh`를 받아 배치한다. 새 아트/플러그인 구매·반입·팩 수정은 없다. `Docs/FabAssetReuseGuide.md` 확인; 팩 현재 보유/라이선스를 재검증했다고 주장하지 않는다.
- 배치는 지형 충돌 trace 성공을 요구한다. 명시 높이·yaw·균일 scale을 적용하고, `FScopedTransaction`/Modify/컴포넌트 및 package dirty를 사용한다. 그룹 이동은 상대 위치와 높이를 보존하며 유효 작업 범위를 벗어나면 전체 거절한다.
- 저장 버튼은 **현재 OpenRunMap과 그 외부 액터의 변경 전체**를 명시 저장한다. 다른 맵/콘텐츠의 Save All은 호출하지 않는다. PCG·Landscape는 기존 전용 에디터 모드로 처리한다.
- 모듈/맵의 자동 변경·자동 저장·런타임 streaming 설정 변경은 없다. 변경 전 존재하던 보스 Save v3 파일·공유 문서·검증 자료는 보존한다.

## Validation plan

`Scripts/BuildEditor.ps1`, UE Automation `Rogue10m.MapMaker`, `Scripts/CheckGeneratedChanges.ps1`, `git diff --check`. 실제 맵의 임시 복제 fixture에서 공간로드 소유권·static mesh 배치·Undo/Redo·이동·OFPA 저장·독립 재로드를 확인한다. UI 생성은 Slate가 있는 에디터 경로로 검증하고 미실행 검증은 완료로 보고하지 않는다.
