# 오픈월드 플레이맵 제작 결과

`Sprint#5-11-open-world-playmap`에서 `/Game/Rogue10m/Maps/OpenRunMap`을 제작하고 저장했다. 기본맵의 환경·시스템과 보유 Stylized Village 에셋을 사용했다. 시작마을의 기존 포탈은 새 맵으로 연결했다. 실제 PIE 99항목을 통과했으며, 현재 결과는 탐험과 지역 전투가 가능한 초기 플레이맵이다.

## 공간과 콘텐츠

| 항목 | 저장된 구성 |
| --- | --- |
| 지형 | 1,800m × 1,800m Landscape, 높이 약21.3~140m |
| 분할 | 1009² 높이 샘플, 256 지형/충돌 컴포넌트, 공간 로드 프록시16개 |
| 지역 | 남쪽 야영지, 서쪽 숲, 중앙 강 교차로, 동쪽 폐풍차, 서북 바위 고원, 북쪽 보스 유적 |
| 길 | 약12m 폭, 중앙 직행·서쪽·동쪽 탐험 경로, 장식 패치711개 |
| 강과 교량 | 폭60m 강, 폭18m·길이120m 교량3개와24m 진입 경사 |
| 식생 | 공식 Epic PCG 그래프1개, 공간 로드 타일183개, 보유 나무3종2,806개 |
| 지역 전투 | 기존 몬스터 데이터를 참조하는 자식 Blueprint6종, 지역 스포너5개와 고정 보스1개 |
| 입장 | 남쪽 PlayerStart, 약80m 안전 공터, 야영지 집 외관3채와 보급 소품 |
| 목표 공간 | 북쪽 석조 유적과 높이55m 탑, 열린 진입부, 상시 로드 보스 접지 바닥 |

최초 공간 제작기에서 소유 액터1,029개를 배치했다. 이 수치는 이후 추가한 PCG 타일183개, 강 끝 패널2개와 보스 접지 바닥을 포함한 최종 총액터 수가 아니다. 실제 나무 수는 ISM/HISM548개 컴포넌트의 인스턴스를 집계했다.

북쪽 목표는 처음부터 접근 가능하다. 중앙 직행 경로의 수학적 길이는1,357.2m, 기본600cm/s 기준 약226.2초이며 최대 경사는8.13°다. 이 시간은 전 구간을 실제로 걸어 측정한 결과가 아니다. 교량과 각 지역은 별도 실제 보행으로 검사했다.

## 월드 설정과 게임 연결

World Partition RuntimeHashSet에256m 셀과700m 로드 범위를 설정했다. Landscape 프록시와 PCG 타일은 공간 로드 대상이며, 플레이어가 먼 지역으로 이동하면 이전 영역이 해제된다.

맵 전용 `URogue10mOpenWorldNavigationSystem`과 Tick이 없는 `ARogue10mOpenWorldBootstrap`을 배치했다. 플레이어 주변60m에서 동적 내비게이션을 생성하고80m 밖에서 제거한다. 타이머로 현재 빙의된 Pawn을 확인하고 약한 참조로 invoker를 등록·정리한다. 프로젝트 전체 Navigation 설정과 기존 캐릭터 에셋은 변경하지 않는다.

메뉴→StartHub 대기→기존 포탈 overlap→OpenRunMap 흐름이다. 포탈은 `StartRun=1` 이동 옵션을 붙이며 새 맵에서1200초 런이 시작된다. OpenRunMap을 직접 PIE로 열면 이 옵션이 없으므로 현재 GameMode는 런을 자동 시작하지 않는다. 실제 입장 흐름은 StartHub에서 플레이하고 북쪽 포탈을 통과하여 확인할 수 있다.

실제 QA에서 드러난 공용 C++ 문제 두 가지를 함께 수정했다. 몬스터 지면 스폰에 캡슐 반높이+2cm를 더하고 소유 스트리밍 레벨을 명시했다. AI는 `FAIMoveRequest`에서 이동자·목표 양쪽 도달 검사 반경을 제외하고 중심 간 목표 거리를 공격 범위의80% 이내로 제한했다. 최초 도달 검사와 최종 도달 검사 모두 전투와 같은 중심 거리 계약을 사용한다. 원본 몬스터 Data Asset과 피해량·공격 범위는 유지했다. 이 공용 수정은 다른 맵에도 적용되며, 기존 모든 맵의 조우를 재검사한 것은 아니다.

먼 지형이 아직 로드되지 않았을 때 고정 보스가 낙하하는 현상은 유적의 실제 평면9600cm와 같은 높이에 숨김 BlockAll 바닥을 상시 로드하여 처리했다. 최종 화면에서 강이 흰색으로 보인 현상은 원본 `MI_Water`를 부모로 하는 새 `MI_OpenWorldRiver`에 거품 강도0과 물 색상을 적용해 수정했다. 강의17개 패널에만 연결했고 지형·교량 충돌은 변경하지 않았다. 현재 강 수면은 시각 표현이며 새 수영 시스템은 없다.

## 실제 검증

| 검사 | 결과와 증거 |
| --- | --- |
| Editor 빌드 | 최종 NoHotReload 빌드 성공,14.59초. `build-editor-center-distance.log` |
| 프로젝트 파일 | Editor dependency 변경 후 생성 성공 |
| Landscape 저장/재로드 | 1.8km 실제 경계, 지형/충돌256개, 공간 프록시16개. `reload.json` |
| 식생 저장/재로드 | 타일183개·인스턴스2,806개 재로드 확인. `reload.json` |
| 식생 배치 | 길·강·공터 제외 위반0, 최대 지면 투영 차이10.391cm·평균0.329cm. `vegetation.json` |
| 포탈과 런 | StartHub 대기 후 실제 native overlap 이동, 새 맵1200초 활성 런 시작. `runtime.json` |
| 접지·이동 | PlayerStart 착지,6개 지역과 야영지 재진입, 교량3개에서 진입/중앙/출구 표본 및 지역 보행 |
| 내비게이션 | 실제 invoker 위치·반경, 스트리밍 완료 대기, 플레이어 주변 경로 완성 및 이동 |
| 스트리밍 | 북쪽 보스 지역에서 야영지 sentinel 해제, 야영지 복귀 후 다시 로드 |
| 보스 접지 | 진입 전에도 고정 보스가 유적 바닥에서 유지됨 |
| 몬스터 전투 | 실제 지역 스폰, 피해 감지→플레이어 표적→추적150.87cm, native 공격으로 플레이어135→134HP |
| 근접 경계 회귀 | 별도 PIE30항목 PASS. Sporeling155→119.14cm, VerdantSlime125→95.72cm 접근, 각각 플레이어 피해1. `ai-boundary.json` |
| 타이머 진행 | 검사 중1200→1154초,46초 감소 확인 |
| 종료 처리 | 검사 소유 PIE 종료 확인 |
| 최종 저장 | 공식 MCP로 새 맵·전용 에셋9경로만 저장 성공, StartHub 포함10패키지 미저장 상태false. `final-save.json` |
| 원본 보존 | 기존 Content5,022개 전체 크기·SHA256 검사, 포탈 관련2개만 허용 변경, 나머지5,020개 동일. `preservation.json` |
| 화면 | 공식 MCP의 실제 전체·야영지·교량·유적 PNG4개를 육안 확인 |

`runtime.json`의99개 검사가 모두 통과했다. 이동 검사는 각 영역으로 검사 Pawn을 이동한 뒤 실제 착지·보행·스트리밍·Navigation을 관측한다. 전체1.8km를 연속 횡단한 플레이 테스트는 아니다. 이동 중에는 검사 Pawn의 피해를 막았고, 단일 몬스터 추적 검사에는 다른 AI를 일시 정지하고 재생을 잠시 멈췄다. 몬스터에게1피해를 주어 기존 감지 경로를 활성화했으며 플레이어에게 직접 피해를 적용하지 않았다. 이러한 처리는 검사 소유 PIE에서만 수행하고 콘텐츠에는 저장하지 않았다.

PCG 생성183회 모두 실행 메시지 배열이 비어 있었다. 새 외부 액터를 MCP 개별 저장 도구가 인식하지 못한 문제는 실행 성공 증거를 확인하고 Epic Python Remote Execution의 현재 레벨 저장으로 처리했다. 모호한 생성 요청을 중복 실행하지 않았다. 이후 공식 MCP의 명시적 에셋 저장과 재로드를 확인했다.

독립 검토에서 이동 완료 반경에 목표 반경도 가산되는 경계 문제를 발견했다. 양쪽 반경을 제외한 최종 C++ 빌드 이후99항목 통합 검사를 재실행했다. 별도30항목 검사는 Sporeling·VerdantSlime을 공격 범위+5cm에 놓고 플레이어 이동0cm, 기존 잘못된 도달 임계값 안의 시작 거리, 실제 범위 안 접근·피해를 확인했다. 검사에서 Unreal Array를 Python list와 직접 합친 초기 오류는 list 변환으로 수정했으며 실패 증거와 정리된 PIE 상태를 보존했다.

강 재질은 캡처 직후의 성공만으로 저장을 판정하지 않았다. 재시작에서 외부 액터의 재질이 원복된 것을 발견해 `actor.modify()`·`component.modify()`로 해당 OFPA 패키지를 변경 표시한 뒤 저장했다. 맵을 해제·재로드하여17개 패널 모두 새 재질을 참조하고 미저장 패키지가 없는 것을 확인했다.

## 도구와 보존 범위

공식 Unreal MCP는 직렬 JSON-RPC 클라이언트로 도구 검색·스키마·에디터 Agent Skills·PCG 작성/실행·맵 로드·캡처·저장에 사용했다. 네이티브 MCP 도구가 Codex 세션에 직접 노출되지 않아 `Scripts/InvokeUnrealMCP.py`를 사용했다. 대량 배치와 native Landscape 생성 함수 호출, callback 기반 PIE는 Epic Python Remote Execution으로 보완했다.

원본 `/Game/FirstPerson/Lvl_FirstPerson`, 구매 에셋, 기존 전투·애니메이션 콘텐츠는 같은 SHA256으로 보존했다. 기존 Content의 허용 변경은 다음2개다.

- `Content/Rogue10m/Maps/StartHub.umap`
- `Content/__ExternalActors__/Rogue10m/Maps/StartHub/4/6L/9ETBXYOM6DWLFQ0NWB8O0H.uasset`

새 콘텐츠1,268개는 OpenRunMap, `World/OpenWorld`와 해당 맵의 외부 액터/객체 경로 안에 있다. `.uasset`·`.umap`은 에디터 API로만 생성·저장했다. 기존 미커밋 소스·전투 작업을 보존했고 새 커밋·푸시는 없다.

## 재현과 작업 문서

- 계획: `Feature/architect/2026-10-05_open-world-playmap.md`
- 구조: `Docs/WorldStructure.md`
- 지형/공간: `Scripts/Editor/OpenWorldLayout.py`, `Scripts/Editor/CreateOpenWorld.py`
- PCG: `Scripts/BuildOpenWorldPCG.py`, `Scripts/OpenWorldWoodlandToolScript.py`, `Scripts/Editor/FinalizeOpenWorldVegetation.py`
- 연결/접지/수면: `Scripts/Editor/FinalizeOpenWorld.py`, `Scripts/Editor/ProtectOpenWorldBoss.py`, `Scripts/Editor/PolishOpenWorldWater.py`
- 실제 PIE: idle Editor에서 `Scripts/Editor/ValidateOpenWorld.py`를 Remote Execution으로 실행하고 `runtime.json`의 최종 상태·99항목·PIE cleanup을 확인한다. 콜백 실행이라 초기 명령의 종료0만으로 통과 판정하지 않는다.
- 근접 경계: `Scripts/Editor/ValidateOpenWorldAIBoundary.py`, 최종 `ai-boundary.json`의30항목·두 사례·PIE cleanup을 확인한다.
- 화면: `Scripts/CaptureOpenWorld.py`
- 보존: `Scripts/VerifyOpenWorldContent.py`
- 증거: `Feature/doc/evidence/open-world-20261005/`

`CreateOpenWorld.py`는 별도 제작 패스를 시작하는 생성기다. 완성된 맵에 재실행하면 소유 환경을 다시 만들므로 단순 검증 용도로 실행하지 않는다. 식생·수면·보스 접지 패스도 이후 다시 적용해야 한다. 롤백은 신규 맵/전용 콘텐츠, 전용 native helper, 공용 스폰/AI 수정, 기존 허브 포탈 목적지로 구분한다. 실제 기본맵과 구매 콘텐츠는 롤백 대상으로 삭제하지 않는다.

## 남은 범위

보스는 기존 전투 데이터를 사용하는 고정 조우다. 처치→승리 런 종료 연결과 전용 패턴은 구현하지 않았다. NPC 서비스·집 실내·지역별 상세 미술 배치도 후속 작업이다. 항공 전체 화면에서는 나무의450~650m culling 때문에 먼 숲이 표시되지 않으며, 지상 캡처와 재로드 인스턴스 집계로 식생을 확인했다.

전체20분 종료, 모든 길의 연속 순회, 모든 몬스터의 전투, 패키징/쿠킹, 성능 목표와 온라인 복제는 이번 검증에 포함하지 않았다. 특히 공용 AI·스폰 수정의 기존 맵 회귀와 패키지 내 동적 WP Navigation은 후속 QA가 필요하다. 검증 실패 파일도 보존해 최종 통과 결과와 구분했다.
