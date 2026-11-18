# 다섯 마을 런 연결 우선 구현 결과

2026-11-11 / **신규 C++ 소스 반영·독립 상태 테스트 완료, UE 통합 빌드·활성화 대기**.

다섯 마을 후보를 생산 런으로 연결할 전용 포털·GameMode와 방문/재방문 기록을 현재 프로젝트에 추가했다. 기존 소스와 맵은 교체하지 않았다. 공유 UE·빌드는 지팡이/통합 담당자가 계속 사용하므로 실행·접속하지 않았다. 현재 플레이에서 새 입구가 이미 보이거나 실제 여행이 완료된 상태는 아니다.

## 변경 파일

| 신규 파일 | 목적 |
| --- | --- |
| `Source/Rogue10m/Core/Rogue10mFiveVillageGameMode.h/.cpp` | 생산 GameMode 상속, 정상 portal StartRun 흐름, PlayerState 관측 Component 한 번 등록 |
| `Source/Rogue10m/World/Rogue10mFiveVillageRunPortal.h/.cpp` | 기존 입구 보존, 후보 경로+새 GameMode URL을 쓰는 별도 입구와 비충돌 프레임 |
| `Source/Rogue10m/World/Rogue10mFiveVillageRouteDataAsset.h/.cpp` |5개 마을·반경·보스 접근 데이터 스키마와 기본 정의·유효성 검사 |
| `Source/Rogue10m/World/Rogue10mFiveVillageProgressModel.h` | UE와 독립적인 방문·재방문·첫 접근 상태 모델 |
| `Source/Rogue10m/Components/Rogue10mFiveVillageProgressComponent.h/.cpp` | PlayerState 소유, timer/런 전이·관측 API. 저장·보상·gate 없음 |
| `Source/Rogue10m/Tests/Rogue10mFiveVillageRuntimeTests.cpp` | 후속 UE Automation2개: 상속·포털/3인칭 기본값·데이터 거부 |
| `Scripts/Tests/FiveVillageProgressModel20261111.cpp` | 독립 C++ 상태/경계/직행·재방문 회귀 |
| `Scripts/Editor/ActivateFiveVillageRunEntry20261111.py` | 에디터 담당자가 기존 포털 옆에 새 입구만 추가·저장·재로드하는 실행 코드 |

## 검증과 한계

- 독립 MSVC C++17/W4/WX 테스트 **52개·실패0**,7만 회 재진입 스트레스 통과. [검증 기록](evidence/five-village-runtime-20261111/portable-tests.json).
- 독립 UE API/수명 검토에서 포털 배치 축1건을 수정했다. localY 뒤쪽 배치에서 localX 옆 배치로 바꾸고 통과 공간 capsule 검사를 추가했다. 최종 검토에서 추가 critical/medium은 발견하지 못했다.
- 활성화 Python의 AST와 인계/빌드 guard2개가 Unreal import 전에 차단됨을 확인했다. 실제 활성화는 실행하지 않았다.
- UE C++ 컴파일·Automation2개·실제 PlayerState timer·정상 Hub 여행·시계·귀환은 **미실행**이다. 독립 상태 테스트를 이 결과로 대체하지 않는다.
- 3인칭 설정·카메라·생산 Pawn/컨트롤러·기존 포털·세이브·무기·빌드 설정은 변경하지 않았다.1인칭 작업은 보류했다.

파일별 해시와 실행/미실행 상태는 [implementation-validation.json](evidence/five-village-runtime-20261111/implementation-validation.json), 검토 근거는 [review.md](evidence/five-village-runtime-20261111/review.md)에 기록했다.

## 통합 담당자 인계

새 클래스 빌드/재시작 → Automation `Rogue10m.FiveVillage` → 단독 Hub에서 활성화 함수 → 정상 프로필 포털 여행/1200초/3인칭/기록·초기화 → 결과/귀환 순서로 확인한다. [전용 README](evidence/five-village-runtime-20261111/README.md)에 실제 호출과 검증 조건이 있다.

보스용 NavMesh와 실제 보스 데이터 배치, 의뢰·상점·던전·마을 pause, cook/패키지 여행,60초/15분·물리 입력은 남아 있다. 이번 신규 입구는 탐험 QA용이며 보스 없는 현재 후보를 완성 런으로 표현하지 않는다. 사용권 대기는 상위 담당자의 직접 지시에 따른 것이고, 도구 승인이 거부된 작업이나 불명확한 에디터 동작은 없다. 커밋·푸시하지 않았다.

## 다섯 마을 별도 런 입구 실제 활성화 (UTC2026-11-11, 여행 검수 대기)
- 새 GameMode/PlayerState 방문 Component/전용 Portal/경로정의 source를 실제 UHT·Editor 빌드에서 통과시켰다. 최종 통합 빌드28.15초 성공이다.
- 현재 DLL의 Rogue10m.FiveVillage RuntimeDefaults/RouteDefinition2개가 정확경로·queue·종료0·원본저장/DLL보존으로 통과했다. 이2개는 여행 검사가 아닌 기본값/스키마 검사다.
- Hub.umap과 외부Actor/Object709개를 SHA일치 백업했다. 단독 clean Hub Editor PID29248에서 지면/캡슐통과 검사 후 새 입구를(600,4270,180)에 추가·저장했다.
- L_Menu→StartHub 실제 재로드에서 새 Portal1개·기존 Portal1개와 원래 목적지/위치/옵션 불변을 확인했다. 후보/기존 OpenRunMap/L_Menu 파일SHA는 동일이다.
- 디스크 변화는 Hub.umap수정1개와 새 Portal/폴더 외부패키지3개뿐이다. 기존709개 중 Hub.umap 외 패키지 변경은 없다.
- 정상 프로필 진입·방문/재방문·타이머·정산·귀환·새 런 초기화는 후속 실제 travel fixture에서 검증한다. 보스/NavMesh/서비스/의뢰·60초/15분 실측·cook/full run은 미완료다.
- Notion 요약 후보: 다섯 마을 후보로 가는 별도 입구를 Hub에 실제 배치하고 저장재로드를 확인했다. 기존 입구는 보존했고 정상 런 여행 검수는 별도 진행한다.
- 근거: Feature/doc/evidence/five-village-runtime-20261111/{VillageUnitA,EditorActivation1111A}, entry-activation-*.json.