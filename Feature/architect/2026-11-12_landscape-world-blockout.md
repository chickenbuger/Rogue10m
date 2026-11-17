# Landscape 기반 월드 배치 첫 제작

## 목적과 범위

2026-11-12 저장한 [제작 기준](../../Docs/WorldMapProductionGuide.md), [좌표 그래프](../../Docs/References/WorldMap/world-layout-20261112.json), [획득 목록](../../Docs/References/WorldMap/acquisition-catalog-20261112.json), [콘셉트 지도](../../Docs/References/WorldMap/rogue10m-world-map-concept-20261112.png)를 실제 UE5.8 공간으로 옮긴다. 순서는 시작 마을 연결 → 보스 직행로 → 큰 마을 3곳 → 작은 거점 5곳 → 사냥터 5곳이다.

기존 OpenRunMap과 구형 FiveVillageGraybox를 재생성하지 않는다. 새 World Partition 맵 `/Game/Rogue10m/Maps/Production/WorldLayout20261112`에서 Landscape·충돌·건물·도로를 작성한다. 기존 StartHub, 아린·장비·보스 관련 작업 중 변경은 보존한다. 기존 포털의 목적지를 덮어쓰지 않고 새 제작 필드 연결을 따로 둔다.

## 선택한 방식

- Landscape: 1009×1009 표본, 1800m 사방, 16×16 컴포넌트, 4×4 스트리밍 프록시, Z 스케일 256. 산과 평지·길 주변 지면을 담당한다.
- Static Mesh: 건물·마탑·시장·초소·바위·나무·보스 바닥. 동굴과 수직 절벽은 후속 단계에서 별도 메시로 만든다.
- 배치 좌표: 기획의 미터 좌표를 `worldXYcm=(15000,32550)+localXYm×100`으로 변환한다. S=(15000,32550), B 입구=(15000,62550). 기존 보스 지지 바닥 상단 9600cm에 접속한다.
- 중심 직행로는 보행 폭 최소 8m, 목표 경사 12° 미만. 마을 중심에 건물을 놓아 통행을 막지 않는다. 입구까지 60초는 실제 CharacterMovement로 별도 검증하며 계산 거리만으로 합격 처리하지 않는다.
- 퀘스트·비급·직업 NPC·일반 몬스터 15종은 이번 공간에 위치를 대응시킨다. 마커를 실제 NPC·드랍·AI 구현으로 보고하지 않는다. 상위 비급의 퀘스트 직접 보상 금지 규칙을 유지한다.

## 자산 재사용

`Docs/FabAssetReuseGuide.md`를 기준으로 이미 프로젝트에 들어 있는 Stylized_Village 모듈(목재/회벽/기와/석조/풍차/시장 소품/수목/바위), 기존 StartVillage 지면 머티리얼, 기존 Landscape AutoLayer를 우선 재사용한다. 새 구매·외부 팩 도입은 없다. 로컬 존재·로드 성공·배치 성공을 기록하며 최신 Fab 계정 소유 확인과 혼동하지 않는다. 벤더 원본은 수정하지 않는다. 현 단계에서는 건물 실루엣과 동선 확인을 우선하며 내부·장식 밀도·충돌 세부 조정은 후속 제작 대상이다.

## Ultrawork Packets

| Packet | 목표/영역 | 완료 조건 | 검증 | 롤백 경계 |
|---|---|---|---|---|
| ULW-1 | 맥락 확인·보존 / MCP·기존 맵 | 현재 맵·포털·보스·dirty 상태 확인, 기존 작업 백업 | MCP 읽기·해시 | 기존 자산 미변경 |
| ULW-2 | 지형 / 새 전용 native helper·높이 생성기 | WP Landscape와 연결 경사·평탄 마을 대지 생성 | Editor 빌드·terrain inspect·높이/폭 수치 | 새 helper·후보 맵만 |
| ULW-3 | 동선·마을 / 새 Editor author | 29개 간선, 3큰마을·5거점·5사냥터 배치 | 배치 보고·캡처·충돌 trace | 후보 맵/own prefix만 |
| ULW-4 | 시작 연결·플레이 / StartHub의 새 포털 | 원래 마을 서비스 유지, 새 필드 정상 스폰·보스 접근 | 저장 후 재로드·PIE·실측 | 새 포털만, 백업 보관 |
| ULW-5 | 검토·기록 / evidence·doc·DevLog | 구현과 미구현 구분, 검증 실패 포함 | diff review·CheckGeneratedChanges | 이번 변경만 선택 stage |

## 경계와 Exit Gate

새 Landscape import는 정확한 프로젝트·맵·R16 경로만 받는 별도 에디터 전용 helper로 제한한다. 기존 native helper의 OpenRunMap 보호를 완화하지 않는다. 새 UFUNCTION이 있으므로 실행 중 에디터를 정상 종료하고 정규 빌드한다. 저장은 후보 맵·그 외부 액터와 명시적인 새 프로젝트 자산으로 제한한다. 같은 실패를 두 번 반복하면 원인 검토로 돌아간다.

빌드, 실제 에디터 작성, 시각 검토, 저장·재로드, 통행 검증이 끝나기 전 완료·통합을 주장하지 않는다. 선행 통합 미검증 커밋과 대규모 기존 dirty 작업은 이번 검증으로 인증하지 않는다. 관련 변경만 커밋·push하고 develop/test/main 승격 조건이 충족되지 않으면 사유를 기록한다.
