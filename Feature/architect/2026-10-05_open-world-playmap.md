# 오픈월드 플레이맵 제작 계획

## 목표와 범위

`Sprint#5-11-open-world-playmap`에서 기존 World Partition 기본맵 `/Game/FirstPerson/Lvl_FirstPerson`을 템플릿으로 새 `/Game/Rogue10m/Maps/OpenRunMap`을 제작한다. 기존 `StartHub`의 런 포탈을 새 맵에 연결한다. 설계된 1,800m × 1,800m의 Landscape, 완만한 탐험 길, 지역 조우, 고정된 북쪽 목표 공간을 제공한다. 원본 기본맵·구매 에셋·기존 전투 및 마을 미커밋 작업을 보존한다. 커밋은 하지 않는다.

지역은 남쪽 입장 야영지 `(0,-65000)`, 서쪽 숲 `(-50000,-25000)`, 중앙 강 교차로 `(0,5000)`, 동쪽 폐풍차 `(50000,-5000)`, 서북 바위 고원 `(-35000,42000)`, 북쪽 보스 유적 `(15000,68000)`cm다. 전체 경계는 ±90000cm이며 Z는 실제 지형에 맞춘다. 보스 지역은 처음부터 접근 가능하게 두고 전체 순회를 강제하지 않는다. 직행 약1,338m와 선택 탐험을 구분한다.

이번 범위는 플레이 환경·지역 전투·스트리밍·허브 이동이다. 새 NPC 서비스, 보스 전용 공격 패턴/승리 시스템, 아이템·영구 성장 시스템을 임의로 추가하지 않는다. 보스용 기존 데이터와 전투 구현은 읽어 확인한 범위에서 재사용하고 기능 한계를 결과에 명시한다.

## 조사와 기술 선택

- MapDataLibrary가 이미 OpenRunMap 경로와 1800m 크기를 정의한다.
- 보유 Stylized_Village에는 Landscape 자동 지형 재질·레이어, 나무·바위·석재/목재 모듈·풍차·물 메시가 있다. 별도 높이맵/절벽/완성 교량 팩은 확인되지 않아 새로운 높이 데이터를 작성한다.
- 공식 MCP에는 Landscape 생성 도구가 없고 native `ALandscapeProxy::Import`, `ULandscapeSubsystem::ChangeGridSize`는 기본 Python에 노출되지 않는다. Epic Landscape 에디터와 같은 생성 경로를 제한된 프로젝트 authoring helper로 노출한다. 엔진 파일은 수정하지 않는다.
- 63quads × 16components + 1 = 1009×1009 높이 샘플, XY scale 180000/1008cm, Zscale256. Landscape를 WP streaming proxy로 분할하고 실제 런타임 로드·해제·재진입을 검사한다.
- 1.8km 전체를 상시 탐색하지 않도록 동적 Navigation과 플레이어 주변 invoker 방식의 한정된 설정을 선택한다. 실제 에디터 API를 확인한 뒤 기존 플레이어 자산을 보존하는 전용 설정 경로를 사용한다.
- 공식 MCP로 도구 검색·설정/에셋 검사·맵 로드·렌더 캡처·최종 저장을 직렬 실행한다. 대량 제작과 callback 기반 PIE 검증은 기존 Epic Python Remote Execution을 사용한다. 임의 Python을 MCP 도구로 소개하지 않는다.

## Ultrawork Packets

| 패킷 | 목표 | 수정 위치 | 완료 조건 | 검증 | 롤백 경계 |
| --- | --- | --- | --- | --- | --- |
| 1 | native Landscape 생성 지원 | Source/Rogue10m/Editor, Build.cs | 에디터 한정·대상맵/파일 범위 제한 helper 빌드·반영 | Editor 빌드, 프로젝트 파일 생성, native 결과 검사 | 신규 helper와 editor dependency |
| 2 | Landscape·지형/길/지역 제작 | 신규 OpenRunMap 및 World/OpenWorld 콘텐츠 | 1800m 지형·교량·랜드마크·지역 전투 저장/재로드 | 실제 collision trace·스폰·보행·렌더 검사 | 새 맵/전용 콘텐츠만 분리 |
| 3 | streaming·Navigation·허브 연결 | 신규 맵 설정과 전용 지원, StartHub 포탈 인스턴스 | 주변 지형/환경 로드, distant unload, AI 이동, 포탈 후 20분 런 | 실제 PIE 이동·재진입·조우 및 타이머 검사 | 전용 설정 및 포탈 목적지만 복원 |
| 4 | 독립 검토·기록 | Feature/doc, DevLog, SprintChangeLog, WorldStructure | 결과·검증·한계 및 재현 경로 기록 | 보존 기준선·hash·AST·diff·CheckGeneratedChanges | 이번 문서 추가분 |

## 보존과 실패 처리

기존 Content 전체 파일의 크기와 SHA256을 기준선으로 저장한다. StartHub에서는 포탈 목적지에 필요한 액터/맵 파일만 허용한다. 신규 맵 템플릿 정리 체크포인트를 저장하고, 재실행은 소유 태그만 처리한다. 체크포인트 없는 불완전 맵의 기존 액터는 임의 정리하지 않는다. 모호한 mutation timeout은 재시도하지 않는다.

`.uasset`·`.umap`은 에디터 API로만 생성·저장한다. C++ reflection 변경 후 이번 작업용 에디터의 미저장 상태를 확인하고 종료·빌드·재시작한다. 서로 다른 에이전트가 에디터를 병렬 호출하지 않는다. 같은 실패가 두 번 반복되면 Architect/Reviewer 검토로 돌아간다.

## 완료 기준

- Landscape의 실제 크기·partition·collision과 저장/재로드 확인.
- 안전한 PlayerStart와 주요 길·교량·목표 구역에 실제 이동 가능.
- 시작마을 포탈→OpenRunMap→1200초 런, 지역 적과 실제 조우.
- 런타임 지형/환경 streaming의 로드·해제·재진입 검증.
- 실제 에디터 화면, 원본 보존 근거, 현재 구현 한계를 한국어 기록.

## 실제 검사에 따른 설계 보완

- 미저장 Landscape는 WP actor descriptor가 아직 없어 `GetCompleteBounds`가 빈 경계를 반환했다. 저장 지형을 폐기하지 않고 `GetLoadedBounds`·실제 프록시 경계를 함께 검사하고, 공식 `LoadLastLoadedRegions` 경로를 제한된 에디터 함수로 노출했다.
- 기본 `WorldPartition`의 스트리밍 플래그는 Python에서 비공개다. 반복된 프로퍼티 접근을 중단하고 native `IsStreamingEnabled` 기반 검사로 통일했다.
- PCG 개별 액터 저장은 새 외부 패키지를 인식하지 못했다. 생성 완료를 증거로 확인하고 공식 Epic Python Remote Execution의 scoped `save_current_level` 체크포인트로 보존했다. PCG 실행 결과 자체는 모든 타일에서 빈 메시지 배열이었다.
- 실제 지역 전투에서 지면 스폰이 캐릭터 캡슐 중심을 바닥에 놓아 실패했다. `MonsterSpawner.cpp`에서 캡슐 반높이+2cm를 반영하고 소유 스트리밍 레벨을 명시했다.
- 고정 보스가 먼 지형 미로드 상태에서 낙하했다. 유적의 실제9600cm 평면과 같은 높이에 상시 로드되는 숨김 충돌 바닥을 추가했다.
- 실제 추적 결과 가까운 적이 공격 범위 밖에 멈췄다. 독립 검토에서 최초 도달 검사에도 목표·이동자 양쪽 반경이 가산되는 것을 확인했다. `FAIMoveRequest`의 양쪽 reach-radius inclusion을 끄고 중심 간 목표 거리를 공격 범위의80% 안으로 제한했다. 원본 몬스터 Data Asset과 공격 수치는 변경하지 않는다. 짧은 사거리2종을 공격 범위+5cm에서 검사한다.
- 직행 경로 최종 길이는1357.2m·226.2초(600cm/s), 최대 경사8.13°다. 교량은18m×120m와24m 진입 경사다.
- 실제 최종 캡처에서 원본 물 재질의 거품이 강 전체를 흰색으로 표시했다. 원본을 수정하지 않고 맵 전용 자식 Material Instance의 거품 강도와 두 물 색상을 조정해17개 수면 패널에 적용했다. 충돌은 변경하지 않는다.

## Exit Gate

패킷1~4의 명시 범위를 완료했다. 최종 Editor 빌드, Landscape·식생 저장/재로드, 실제 PIE99항목, 공식 MCP 캡처·명시적 저장과 기존 Content 전체 SHA256 보존을 확인했다. 결과와 검증의 실질 범위·공용 C++ 수정의 회귀 한계는 `Feature/doc/2026-10-05_open-world-playmap.md`에 기록한다. 보스 처치 승리 연결·전체20분 종료·패키징/성능/온라인 검사는 별도 작업이다.
