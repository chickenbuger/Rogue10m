# 평탄한 필드·자연 외곽·마을 경계 제작 결과

2026-11-17 · Sprint#9-32 · **수치 검증을 통과한 후보 저장 체크포인트, 시각 인수·독립 cold 검증 진행 중**.

[기획서](../architect/2026-11-17_natural-world-boundaries.md)에 따라 기존 Crop 후보에 평탄한 플레이 구역과 외곽 능선, 마을 경계·소품·도로 전이를 저작했다. 구조 검사49/49와 정적 캡슐 질의544/544를 통과했고, native 선택 저장은741개 패키지를 대상으로 성공했다. **프로젝트 뷰포트 이미지의 MCP 전송이 자동 승인 검토에서 거절돼 별도 동의 응답을 기다리고 있다. 실제 화면·근접/원거리 연속성·AAA 수준의 미술 완성도를 검증했다고 보고하지 않는다.**

## 실제 변경과 보존 범위

대상은 `/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A`다. Landscape·충돌 컴포넌트 각각45개와1,012.5×562.5m 영역,21지점·29개 양방향 도로·중앙300m 좌표를 유지했다. 새 `NaturalBoundary20261117` additive 편집 레이어를 추가해 내부를 Z9600cm로 평탄화하고,13점 폐곡선 외곽에 최대33m 높은 능선을 만들었다. 목표 격자의 폐곡선551개 표본은 Z11498.576~12893.184cm로, 평면보다 약19.0~32.9m 높았다. 기획의24~33m 방향과 실제 최소 표본의 차이를 보존한다. 물 plane이나 강·동굴 시스템을 추가한 작업은 아니다.

568×316의179,488개 목표 높이를 전체 비교했고 목표 composite BLAKE3는 `1fa4f2c33df67f184bf962b9a792cae5b46c7326ecb8604789d85baccba7d143`, 독립 uint16LE SHA256은 `90003d3cea808922f12e56a16eda8439662c8c7e53fad2833996ec48d24143ed`다. 기존 레이어의 raw RG 높이·paint·B/A 및 native 원본 레이어 해시를 보존했으며 편집 레이어 수는2개다. 이전의 전체 composite 높이 불변 검사를 재사용하지 않고 새 목표 높이와 원래 레이어 보존을 각각 검사했다.

기존 액터 경로1,390개를 보존했다.672개 액터의 높이를 재배치하고20개 건축 그룹은 같은 수직 차이를 적용했다. 별도 충돌 수정으로 T 기록촌 도서관8개 부품을 함께 서쪽200cm 이동했다. 나머지 배치 XY·회전·스케일·재질 참조를 유지하고, 도서관도 부품 사이 상대 배치와 Z를 유지했다. 원래 계획 파일은 보존하고 [한정 수정 계획](evidence/landscape-natural-boundary20261117/author-plan-gate-reflow.json)에 이8개 예외만 기록했다.

도로29개는 새 평면에 맞춰 투영 위치를 조정했다. 폭8m 안에서 새 `M_RoadFeather`가 최소6m 갈색 중심을 유지하고 양옆0.7~1m에 smoothstep과 약한 world noise를 적용한다. 길이 UV의 끝 fade는 추가하지 않았다. 기존 도로 메시663개는 숨김·NoCollision 상태로 유지한다. 재질의 컴파일·노드/연결 검사는 통과했지만 자연스러운 실제 색 전이는 아직 화면으로 인수하지 않았다.

새 배치는10개 그룹·22개 ISM 컴포넌트·708개 인스턴스다. H는 목재·청록, M은 낮은 목책·황토, T는 석재·청색을 사용하며 각각 반경38/38/37m에4/4/5개의 열린 출입구를 둔다.13개 출입구의 실제 부품 내부 폭은14m다. 외곽에는13개 숨김 Pawn 전용 차단을 두고, W/A/R에는 선택 탐험을 암시하는 소품3곳을 배치했다. 소품에 신규 보상·픽업·퍼즐 기능을 연결하지 않았다.

## 프로젝트 소유 재질과 퀘스트 복제

신규 소유 경로는 `/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117`다. 자산은 도로 재질1개, 퀘스트 복제1개, ISM용 표면 재질5개로 총7개다. 기존 프로젝트 자원과 Engine 기본 Cube/Cylinder를 재사용했으며 신규 Fab 반입은 없다.

처음 ISM을 배치했을 때 엔진이 기존 목재·석재·천 재질5개의 instancing 사용 플래그를 자동 변경해 원본 패키지가 dirty가 됐다. native 저장 가드는 이5개 외부 패키지 때문에 **저장 호출 전에 거절했고 실제 저장하지 않았다**. 가드를 완화하지 않고 `M_Natural_DarkTimber`, `M_Natural_WarmStone`, `M_Natural_Banner_Teal/Ochre/Blue`를 복제해 ISM 플래그를 켠 뒤 새22개 ISM에만 할당했다. 원래5개 패키지는 디스크에서 재로드했으며 원본 파일 해시·그래프가 같고 플래그false·clean 상태임을 확인했다. [복구 기록](evidence/landscape-natural-boundary20261117/author-live/ism-material-repair.json)과 거절된 첫 저장 기록은 보존했다.

퀘스트는 기존 DataAsset을 유지하고 `DA_IntroSupply_Flat`만 복제해 layout origin Z를9600cm로 바꿨다. SupportedWorld·회전·전체5개 quest row·선택 이야기 텍스트를 원본과 대조했다. Director의 참조만 후보 전용 복제본으로 바꿨으며 보상·진행 규칙을 추가하지 않았다.

## 구조·충돌·보행 검증

[49개 구조 검사](evidence/landscape-natural-boundary20261117/author-live/validation-owned-materials.json)는 목표 높이 전체 일치, 원본 레이어 보존,45개 등록된 지형/충돌, 허용 액터 변경,708개 인스턴스의 실제 transform·재질·공개 충돌 설정,13개 문 폭과 퀘스트 복제를 포함한다. loaded-reflection 검사는 유지하되 native/custom serializer·미로드·전역 transitive 안전성은 인증하지 않는다.

[정적 캡슐544개 검사](evidence/landscape-natural-boundary20261117/author-live/capsule-validation-gate-reflow.json)는 반경34/42cm·반높이96cm로 외곽13구간과 모서리, 출입구13곳의3개 횡방향 경로, 지면 지지점51곳을 질의했다.52개 높은 차단 질의는 정확한 외곽 backup actor의 hit를 확인했다. 점프 높이는 기본 JumpZ420cm/s와 중력980cm/s² 가정으로 만든90/180cm 참조값이다. **실제 점프 실행·물리 키보드·런타임 중력 측정 결과는 아니다.**

처음544개 중6개 T/E17 왼쪽 접근 질의는 도서관 `Post11`에 초기 관통했다. 반경34cm의 관통 깊이는8.2cm였다. 문 자체14m 검사는 통과했으나12m 안쪽 접근이 기존 건물에 닿았으므로 전체8개 부품을 서쪽200cm 이동했다. 원래 질의·폭·합격 조건을 유지한 재검사에서544/544가 통과했다. 실패 결과는 삭제하거나 합격으로 재분류하지 않았다.

| 정상 CharacterMovement 중앙 S→B | 측정 시간 | 결과 |
| --- | --- | --- |
| 1회차 | 48.5345초 | 60초 이내, 공중 관측0 |
| 2회차 | 48.5447초 | 60초 이내, 공중 관측0 |
| 3회차 | 48.5395초 | 60초 이내, 저장 체크포인트 이후 통과 |

보행은 기존 `CropWalkQA` 프로필의620cm/s·기본1200초 런·시간배율1 조건에서 렌더링된 PIE에 `AddMovementInput`을 제공한 시험이다. 처음2회는 미저장 지형에서,3회차는 저장 체크포인트 이후 실행했다. 세 회차의 wall 시간은48.531/48.547/48.531초다. 속도·위치·런 시간 덮어쓰기와 무적은 사용하지 않았다. [3회 요약](evidence/landscape-natural-boundary20261117/runtime-walk/summary.json)과 [저장 후 실제 캐릭터 값](evidence/landscape-natural-boundary20261117/runtime-walk/kinematics-3.json)에서 캡슐34/96cm·JumpZ420·최대 점프2회·hold0·실제 중력−980cm/s²를 확인했다. 실제 점프를 실행한 결과는 아니다. 정상 Hub 포털·사람 플레이·15분 성장·20분 완주·전체 전투·NavMesh 인수를 포함하지 않는다. 외곽 선택 경로 보행과 독립 cold·최종 파일 범위 검사는 후속 결과를 기다린다.

## 저장·실패 기록과 남은 인수

소스의 editor 전용 Foliage 의존성과 native component-map 좌표 단위 오류를 보완한 정규 Editor 빌드를 통과했다. 이후 구조 검사와 충돌을 통과한 후보를 native 가드로741개 패키지만 저장했고 반환은true, 남은 dirty는0이다. SaveAll은 사용하지 않았다. [저장 결과](evidence/landscape-natural-boundary20261117/author-live/save-result.json)는 디스크 변경 범위·독립 재로드·시각 인수를 대신하지 않는다.

초기 좌표 단위 guard 실패, ActorFactory의 기존 root 처리, Python에서 제외된 BlueprintSetter, Rotator 위치 인수 순서, dirty 목록 정렬 표현, 도서관 접근6개 실패와 원본 재질 자동 플래그·저장 거절을 각 원문으로 보존했다. 첫 좌표 guard 실패는 지형 수정 전에 중단됐다. 별도 저장 거절도 `save_called=false`였다. 정상 재개에서 과거 실패를 소급해서 바꾸지 않았다.

첫 PIE에서 StylizedCharacter의 `SKEL_Hu_M`이 오래된 `/Engine/EngineMeshes/Humanoid` 의존성을 찾지 못하는 LoadErrors1줄이 재발했다. 기존과 같은 vendor import 문제이며 이번 제작에서 원본을 수정하지 않았다. [해당 로그 검사](evidence/landscape-natural-boundary20261117/runtime-walk/first-pie-log-check.json)와 이전 오류를 함께 유지하며 전체 로그 오류0으로 보고하지 않는다. 파생 재질 복구·선택 저장 구간의 [로그 검사](evidence/landscape-natural-boundary20261117/author-live/material-save-log-check.json)는 기존 오류11줄을 보존하고 해당 경계 이후 신규 오류0줄을 확인했다.

현재 [수치 체크포인트](evidence/landscape-natural-boundary20261117/author-live/numeric-checkpoint.json)는 `saved=true`, 구조49·정적 캡슐544 통과, `visual_reviewed=false`, 최종 인수false다. 중앙3회 통과에 이어 선택 경로 보행, 독립 cold·파일 범위·보호 저장 대조와 이미지 전송 동의 후의 시각 검토·독립 리뷰 결과를 기다린다. [fresh fetch 후 게시 의존성 검사](evidence/landscape-natural-boundary20261117/publication-dependency-check.json)의 `origin/main`(`024ecfb7`)에는 후보맵·NPC Director·Quest DataAsset 헤더가 없고 기존 Crop native 도구는 있다. 이 결과만 독립 게시하면 필요한 기반이 빠지므로 의존성 범위와 미완료 인수를 정리한 뒤 커밋·게시·승격 결과를 기록한다.

## 참고한 AAA 제작 원칙

[Ghost of Tsushima 환경 제작 설명](https://blog.playstation.com/2020/07/09/crafting-the-world-of-tsushima/)의 자연 색·지역 정체성·환경 유도를 마을 실루엣과 색 표식에 적용하는 방향을 정했다. 바람·동물 안내 기능을 구현한 것은 아니다. [Horizon Forbidden West 월드 제작 설명](https://blog.playstation.com/2021/11/22/horizon-forbidden-west-an-authentic-world/)의 문화와 목적에 맞는 재료·소품, 멀리서 읽히는 정착지 구분을 세 마을과 선택 소품에 맞춰 해석했다. 다른 게임의 진행·보상 체계를 복제하지 않았고, 이 참고가 실제 화면 품질의 검증 근거가 되지는 않는다.

정확한 기계 판독 상태와 좌표는 [신규 제작 카탈로그](../../Docs/References/WorldMap/landscape-natural-boundary20261117.json)에 기록한다. 일반15종·중간보스4종·최종보스1종, 탐색 비급과 무기별 입력·마법 주문 규칙은 기존 카탈로그를 따른다.
