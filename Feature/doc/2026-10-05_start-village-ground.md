# 시작마을 바닥 텍스처 개선 결과

`Sprint#5-12-start-village-ground`에서 StartHub의 잔디·흙길 늘어짐을 수정하고 광장과 준비 구역에 돌바닥을 적용했다. 모든 바이너리는 Unreal Editor에서 저장했다. 기존 미커밋 작업을 보존했고 커밋·푸시는 하지 않았다.

## 원인과 적용

기존 생성기는 잔디 65회·흙길 6회 반복을 요청했지만 StarterContent 부모에 지원되는 반복 파라미터가 없어 실제 적용되지 않았다. 120m 바닥과 비균일하게 확대한 길의 메시 UV가 텍스처를 늘렸다. 원본 잔디·자갈 텍스처의 최종 조회 크기는 2048²이며 LOD bias는 0이었다.

마을 전용 `M_StartVillage_GroundDetail`에서 Absolute World Position의 XY를 `TileSizeCm`으로 나눠 색상과 노멀 텍스처에 같은 좌표를 공급한다. 완만한 18m 색상 변화와 전용 틴트·거칠기를 추가했다. 기존 잔디·흙길 인스턴스를 이 부모로 연결해 메시 크기와 방향에 관계없이 일정한 반복 크기를 사용한다.

| 바닥 | 전용 인스턴스 | 최종 반복 크기 |
|---|---|---|
| 잔디 | MI_StartVillage_Grass | 180cm |
| 흙·잔자갈 길 | MI_StartVillage_EarthPath | 90cm |
| 광장·준비 구역·포탈 패드 | MI_StartVillage_Cobblestone | 120cm |

돌 재질은 중앙 광장·포탈·준비 구역 4곳의 총 6개 바닥 슬롯에만 적용했다. 외부 액터와 컴포넌트를 `modify()`한 뒤 현재 레벨을 저장해 OFPA 연결을 보존했다. 원본 구매/StarterContent 에셋과 그래픽 전역 설정은 수정하지 않았다.

생성기는 새 전용 부모와 기존 튜닝을 재사용하며, 기존 부모에 반복 파라미터가 없으면 경고와 실제 미적용 상태를 기록한다. 읽기 전용 사전 검사에서 재질 타입·전체 파라미터·부모·텍스처·양수 반복 크기를 확인한 후에만 마을 정리를 시작한다. 이번에는 전체 마을 생성기를 실행하지 않았다.

## 검증

- native 재질 컴파일 오류 배열이 비었고 공식 MCP MaterialTools 컴파일도 성공했다. MCP 배선 조회로 World XY→물리 반복→색상/노멀과 최종 색상·노멀·거칠기 출력을 확인했다.
- 공식 MCP로 StartHub와 전용 재질 4개를 명시적으로 저장했고 5개 모두 dirty=false였다. 기본맵으로 이동해 마을을 해제한 뒤 StartHub를 다시 열었다.
- `reload-validation.json` 43/43 PASS. 기존 677개 액터의 클래스·위치·회전·스케일·메시·충돌·포탈 목적지를 기준선과 비교해 보존을 확인했다. 6개 바닥 재질 슬롯·3종 프로필·텍스처와 미저장 패키지 없음도 확인했다. 포탈은 OpenRunMap 및 런 시작 설정을 유지한다.
- 최종 텍스처 조회 크기는 잔디/자갈 색상·노멀 2048², 돌 색상·노멀 1024²다. 최초 돌 텍스처의 32² 조회는 준비 중 임시 값으로 보이며 원본 해상도로 해석하지 않았다.
- Content 6290개 전체 크기·SHA256 비교 PASS. StartHub·기존 인스턴스 2개·바닥 외부 액터 6개만 변경됐고 나머지 6281개는 같은 바이트다. 새 에셋 2개는 전용 master와 돌 인스턴스다. OpenRunMap과 포탈 패키지도 같은 바이트다.
- 잔디·길·광장 전후와 동서 길·마을 전체의 실제 MCP PNG를 육안 검토했다. 큰 얼룩과 방향성 늘어짐이 사라졌다. 전후 월드 카메라 위치는 같지만 이미지 높이가 895/777로 달라 정확한 픽셀 비교는 아니다. 가까운 화면 하단 일부는 여전히 부드럽게 보인다.
- 생성기 사전 검사도 실제 재질에 읽기 전용으로 실행해 통과했다. 독립 검토에서 사전 검사 순서를 보강했고 Python AST·diff·하네스 경로 검사도 수행했다.

C++와 Build.cs를 변경하지 않아 Editor 타깃 재빌드는 생략했다. 재질 범위 작업으로 PIE 전투·포탈 이동 재실행, 패키징과 성능 측정은 수행하지 않았다.

## 변경 파일과 근거

신규 조사·수정·검사: `Scripts/Editor/InspectVillageGround.py`, `ImproveVillageGround.py`, `ValidateVillageGround.py`, `Scripts/CaptureVillageGround.py`, `Scripts/AuditVillageGroundContent.py`. 생성기 재사용·사전 검사: `Scripts/Editor/CreateStartVillage.py`.

계획: `Feature/architect/2026-10-05_start-village-ground.md`. 근거: `Feature/doc/evidence/village-ground-20261005/`의 `before-state.json`, `application.json`, `reload-validation.json`, `preservation.json`, `generator-preflight.json`, 공식 MCP 저장·컴파일·배선 응답과 최종 PNG. 에디터는 StartHub의 개선된 바닥이 보이는 카메라로 열어 두었다.

![최종 시작마을 바닥](D:/Project/Rogue10m/Feature/doc/evidence/village-ground-20261005/after-village-ground.png)
