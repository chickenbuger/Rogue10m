# World Map Maker 사용 가이드

현재 플레이맵 `/Game/Rogue10m/Maps/OpenRunMap`을 Unreal Editor 안에서 구역별로 편집하는 도구다.

## 채택 방식

이미 사용하는 **World Partition**을 유지한다. 현재 RuntimeHashSet은 256m 런타임 셀·700m 로딩 범위를 사용한다. Landscape, 외부 액터(OFPA), 기존 PCG 식생과 네이티브 도구를 그대로 활용한다. 별도 청크 파일이나 스트리밍 매니저를 만들지 않는다.

툴의 8×8 격자는 **편집 작업 영역**이다. 원점 기준 256m 간격이며, 가장자리 영역은 1.8km × 1.8km 맵 경계에서 잘린다. 이 좌표는 Unreal의 런타임 셀 ID가 아니다. 구역을 고른다고 런타임 로딩 범위나 몬스터 스폰 규칙이 바뀌지 않는다.

## 열기

1. C++ 모듈 빌드 후 Unreal Editor를 연다. 이미 실행 중이었다면 새 모듈을 읽도록 에디터를 다시 연다.
2. Content Browser에서 `Content/Rogue10m/Maps/OpenRunMap`을 연다. 기본 시작맵은 메뉴 맵이므로 플레이맵을 직접 열어야 한다.
3. 상단 **Window > World Map Maker**를 선택한다. 콘솔 명령 `Rogue10m.MapMaker.Open`으로도 열 수 있다.
4. 패널은 다른 에디터 탭과 같이 도킹하거나 별도 창으로 사용할 수 있다. 새 독립 창의 기본 크기는 560×900이며 기존 사용자 레이아웃은 유지한다. Play/Simulate 중에는 편집 버튼이 잠긴다.

## 구역을 열고 편집하기

1. 위가 북쪽(+Y)인 격자에서 구역을 클릭한다. 금색이 선택 영역, 청록색이 이 패널이 로드한 영역이다. 각 칸 숫자는 **현재 로드된 편집 가능 환경물** 수이며 전체/미로드 액터 수가 아니다. 좁은 패널에서는 격자 하단 가로 스크롤로 양쪽 끝 열을 볼 수 있다. 좌표/개수가 잘리지 않도록 셀 폭을 유지한다.
2. **Load + focus**로 해당 구역을 로드하고 뷰포트를 이동한다. Perspective에서는 경계 벽에 시야가 가리지 않도록 높은 각도에서 내려다본다. 여러 구역을 연속 로드할 수 있다. **Focus**는 시점만 이동한다. 활성 레벨 뷰포트가 없거나 카메라를 piloting/lock 중이면 먼저 해당 상태를 해제한다. Orthographic 모드는 기존 방향으로 native zoom을 적용한다.
3. 필요하면 레이블 필터를 입력하고 **Select environment in area**를 누른다. 기존 Details 패널/뷰포트 기즈모에서 선택한 환경물의 위치·회전·크기·메시를 편집한다.
4. 환경물 묶음을 다른 영역으로 옮기려면 선택을 유지한 채 목적지를 고르고 **Move selection here**를 누른다. 그룹 중심의 XY가 목적지 중앙으로 이동하며 상대 위치와 Z는 유지된다. 지형 높이에 자동 재투영하지 않으므로 이동 후 접지 상태를 확인한다.
5. **Release tool areas**는 이 패널이 만든 로더만 해제한다. 사용자가 World Partition 창에서 직접 열어둔 영역은 유지된다. 패널 종료/맵 변경 때도 툴 소유 로더를 정리한다.

보호 대상은 지형·게임플레이·경계·보스 바닥·연결된 계층·움직임 잠금 액터다. 자동 환경물 선택은 프로젝트 소유 태그와 허용 환경 폴더를 가진 네이티브 StaticMeshActor에 한정된다. 보호 액터가 섞인 그룹은 일부만 이동하지 않고 전체 거절한다. 스포너·보스·Blueprint 기능 액터는 기존 World Outliner/Details에서 계약을 확인하며 편집한다.

## 기존 에셋 배치하기

1. Content Browser에서 기존 **Static Mesh 한 개**를 선택한다.
2. 패널에서 **Use selected mesh**를 누르고 에셋 이름을 확인한다.
3. `Height offset (cm)`, `Rotation (degrees)`, `Uniform scale`을 설정한다. 높이 오프셋 ±10,000cm, 균일 크기 0.01~100을 지원한다.
4. 구역을 로드한 후 **Place mesh at area center**를 누른다. 로드된 Landscape 충돌을 기준으로 메시 바닥을 맞춘다. 다리·강 수면·다른 소품 위로 스냅하지 않는다. Landscape 충돌이 없거나 메시 경계가 월드 밖으로 나가면 배치를 거절한다.
5. 새 액터는 `OpenWorld/MapMaker` 폴더와 프로젝트 소유 태그를 가지며, 선택 상태에서 Details로 추가 조정한다. 원본 메시 에셋은 수정하지 않는다.

배치·그룹 이동은 네이티브 **Ctrl+Z / Ctrl+Y**로 실행 취소/다시 실행할 수 있다. Landscape 도형/페인트는 **Landscape tools**, 상세 공간 로드 관리는 **World Partition** 버튼에서 기존 Unreal 도구를 사용한다. 기존 PCG 식생은 해당 PCG 액터/그래프 파라미터를 Details에서 편집한다. PCG 생성 결과를 이 툴의 개별 소품으로 변환하지 않는다.

## 저장

**Save map + external actors**는 현재 `OpenRunMap`과 그 외부 액터 패키지의 변경을 저장한다. 패널 밖에서 편집한 같은 맵 변경도 포함한다. 다른 맵이나 메시·재질 원본의 변경은 저장하지 않는다. 콘텐츠 에셋을 직접 편집했다면 해당 에셋의 기존 저장 기능을 사용한다. 자동 저장이나 저장 없는 배치 확정은 하지 않는다.

## 개발 검증

```powershell
.\Scripts\BuildEditor.ps1 -NoHotReload
& 'D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' Scripts/RunMapMakerTests.py
.\Scripts\CheckGeneratedChanges.ps1
```

테스트는 별도 unattended Editor 프로세스에 새 WP/Landscape fixture를 만들고 격자·보호 정책·다른 맵/PIE 거절·영역 소유권·배치·Undo/Redo·이동·OFPA 저장/재로드를 확인한다. 실행 전후 원본 OpenRunMap 및 외부 패키지 SHA256을 대조한다. 완료 결과와 실행 한계는 [기능 결과](../Feature/doc/2026-10-24_world-map-maker.md)를 참조한다.

공식 근거: [Epic World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine), [Epic Data Layers](https://dev.epicgames.com/documentation/unreal-engine/world-partition---data-layers-in-unreal-engine). 새 런타임 Data Layer는 이번 툴에서 자동 생성하지 않는다.
