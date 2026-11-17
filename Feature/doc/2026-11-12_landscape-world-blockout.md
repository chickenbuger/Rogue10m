# Landscape 월드 배치 첫 제작

저장한 기획·지도·획득 동선을 실제 UE5.8 공간으로 옮겼다. 기존 시작 마을에서 별도 포털로 새 필드에 입장하며, 기본 장비 보행으로 보스 입구까지 **48.537초**를 확인했다. 이 체크포인트의 범위는 **공간 배치와 통행의 첫 제작**이다. 신규 NPC·대사·퀘스트·비급 지급·15종 AI·중간/최종보스 전투 완성을 뜻하지 않는다.

## 실제 결과

- 맵: `/Game/Rogue10m/Maps/Production/WorldLayout20261112`
- Landscape/World Partition: 1800m 사방, 1009² 높이, 256개 지형·충돌 컴포넌트, 16개 공간 로딩 프록시, 편집 레이어1개. 별도 맵을 경유한 저장본 재로드에서도 구성 확인.
- 큰 마을: 수렵 회관·오두막2 / 정적 풍차·상점·시장 부스3 / 외관 마탑·작은 야외 서고.
- 작은 거점5곳: 평탄대 안에 작은 건물·깃발·소품. 사냥터5곳: 수목·바위 등 첫 장식과 기획 위치 대응.
- 도로29개, 통행 폭8m를 확보한 배치. 일반 몬스터15·NPC12·비급23·무기3의 **편집 전용** 위치 표식53개에 중간보스4·최종보스1의 별도 예약5개를 더했다. 보스 입구와 지지 바닥은 실제 저작했으나 보스 액터는 배치하지 않았다.
- StartHub에는 새 포털·문틀·표지6개만 외부 액터로 추가했다. 기존 포털2개·아린은 유지하며 `StartHub.umap` 자체는 작업 전 백업과 해시가 같다.

## 검증과 증거

| 항목 | 결과와 범위 |
|---|---|
| 정규 Editor 빌드 | 성공2회. 새 UFUNCTION 추가는 종료 후 UHT/정규 빌드, OFPA 가드 수정도 정규 빌드 |
| 지형 | 256 collision/16 proxy/Edit Layer/원점·스케일·1.8km bounds 확인. 생성 후 저장·에디터 재시작·재로드 통과 |
| 순수 기하 | 21노드·29간선·비교차·인코딩 검증. 건물 모서리 고도차0cm, 충돌 건물의8m 길 침범0 |
| 정상 입장 | 격리된 QA 저장 슬롯, 메뉴 UI의 정상 캐릭터 생성 콜백→StartHub→새 포털→필드. 프로필 복원과 `StartRun=1` 로그 확인 |
| 기본 보행 | Pawn.AddMovementInput으로 실제 CharacterMovement 사용. 620cm/s 기본 장비 속도 그대로, 48.537초, 접지 이탈0프레임. 물리 키보드 입력·전체 게임 플레이 완료 아님 |
| 타이머 | Hub는 Waiting/1200초, 포털 후 Running/1200초 시작·감소. 보행 끝에도 정상 감소 |
| 전체 길 검사 | 최초 PhysicsA의3431개 검사 중20개 실패를 원본 보존. 긴 직선 캡슐의 지면 접촉과 정확한 타일 경계 ray 누락을 구분해 후속 세부 검사 진행 |
| 화면 | 첫 도로 메시가 지형에 잘려 보이는 문제 확인. 지형·충돌을 바꾸지 않는 지면 투영 도로로 보정 진행 |

주요 증거: [지형 검사](evidence/landscape-world-blockout-20261112/terrain-inspection.json), [배치 기록](evidence/landscape-world-blockout-20261112/scene-author.json), [실제 보행](evidence/landscape-world-blockout-20261112/runtime-walk.json), [시작 연결](evidence/landscape-world-blockout-20261112/start-hub-entry.json), [최초 검사 실패](evidence/landscape-world-blockout-20261112/PhysicsA/physics-validation.json). 실패 후보와 성공 결과를 섞어 통과로 보고하지 않는다.

## 자산·소스와 보존

기존 Stylized_Village의22개 메시 경로(건축·풍차·가구·수목·바위), StartVillage의 기존 지면/배너 재질6개, 기존 Landscape 재질·Auto Layer를 실제 로드·배치했다. 벤더 원본을 수정하거나 팩을 새로 가져오지 않았다. Fab 가이드의 과거 보유 카드 기록과 이번 로컬 로드·현재 맵 사용을 구분한다. 이번에 로그인된 Fab 계정의 소유를 새로 재확인한 것은 아니다.

새 제작 도구는 `Rogue10mWorldLayoutAuthoring.h/.cpp`, `WorldLayoutTerrain20261112.py`, `AuthorWorldLayoutScene20261112.py`, `ManageWorldLayout20261112.py`, `RepairWorldLayoutRoadVisuals20261112.py`, `ValidateWorldLayout20261112.py`, `PlaytestWorldLayout20261112.py`다. 기존 OpenRunMap 전용 helper와 옛 생성 스크립트는 수정·재실행하지 않았다. 새 helper는 정확한 후보/높이 파일을 제한하고 이미 지형이 있으면 재Import하지 않는다. Import 직후 외부 proxy를 무관한 dirty 맵으로 오인한 문제를 후보의 외부 패키지2종만 허용하도록 고쳤다.

기존 tracked dirty68개와 StartHub 외부 파일을 별도 작업 백업에 보관했다. 검증 중 다른 채팅이 공용 Controller 소스와 Sprint 문서를 바꾼 것은 덮어쓰거나 이번 커밋에 포함하지 않는다. 게임·포털·보행 결과는 이 작업의 실제 로드된 Editor DLL과 당시 작업 공간 기준이다. 선행 전투·저장 WIP나 전체 현재 소스를 이 체크포인트의 맵 검사로 인증하지 않는다.

## 남은 제작

전체 삼각형 단위로 횡경사13.664°·최대 합성 경사15.095°가 있는 지점은 전역12° 목표를 위해 추가 보정이 필요하다. 표본 도로 종경사11.880° 통과와 구분한다. T 마탑 평탄대는 건물/길 여유를 위해 반경34m이며 H/M35m와 다르다.

지형 페인트·식생 밀도·길 가장자리·마을 내부·사냥터별 생태 표현·중간보스 공간 세부 저작은 후속 작업이다. 실제 NPC/대화/상점/수련/퀘스트/드랍/스킬북 획득, 15종/4중간/1최종의 플레이 조우, 동적 NavMesh·AI·15분 성장 동선·3회 이상 전체 주행·패키징은 아직 완료하지 않았다. 시작 마을의 새 포털이 곧 전체 새 월드 콘텐츠 완성을 뜻하지 않는다.

기획 근거: [제작 기준](../../Docs/WorldMapProductionGuide.md), [실행 계획](../architect/2026-11-12_landscape-world-blockout.md), [콘셉트/실제 자료 구분](../../Docs/References/WorldMap/README.md).
