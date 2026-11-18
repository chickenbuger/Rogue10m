# Hub → Compact 후보 입구 저장·재로드

최종 확인(2026-11-14 00:03 UTC): `CheckGeneratedChanges.ps1` exit0 및 범위 한정 `git diff --check` exit0이다. 기존 dirty 에셋과 LF/CRLF 경고는 유지했으며 이번 신규 에셋 6개는 Unreal API로 작성·저장했다. [프로세스 관측](evidence/hub-compact-entry20261113A/final-process-observation.json)에는 기존 D bootstrap14384/Game54244만 남고 새 검증 commandlet·Editor·빌드 프로세스는 없다. 해당 게임 창의 입력·재접근·종료는 수행하지 않았다.

정상 공유 프로젝트의 StartHub에 **후보 입구 1개와 표시 Actor 5개를 추가해 저장했고, 새 프로세스의 정밀 재로드 검사를 통과했다.** 기존 포털 3개는 보존했다. 최종 재해시에서 이전10466개 입력은 모두 동일했고, 신규6개만 늘어 총10472개다. 보호 저장·Editor6·D의두물리payload도 동일하다. 실제 플레이어의 포털 통과와 새 Windows 패키지 반영은 아직 검증하지 않았다.

## 실제 구현

새 포털은 `CompactCandidate20261113A_RunPortal`이며 위치는 `(-1440,4270,180)cm`다. 기본 native `Rogue10mRunPortal`을 사용하고 목적지는 `/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A`다. timer=true, additional options는 빈 값이므로 기존 `StartRun=1` 경로를 사용한다. FiveVillage GameMode 강제 지정이나 지원 맵 가드 변경은 없다.

기존 OpenRunMap·FiveVillageGraybox·WorldLayout 포털의 대상, 옵션, 위치·회전·크기·flags는 저장 전후와 재로드 후 동일하다. 기본 맵, 기존 Hub umap, 후보 PlayerStart·GameMode·Director·의뢰 DA는 변경하지 않았다. 신규 Actor는 항상 로드되며 editor-only=false, DataLayer 없음이다. Actor label과 고유 tag로 찾을 수 있고 사용자가 에디터에서 직접 옮기거나 설정을 바꿀 수 있다.

Engine Cube와 기존 프로젝트 WarmStone/PortalGlow 재질로 기둥 2개·상단·표면을 구성하고 `COMPACT CANDIDATE` 표식을 추가했다. 새 Fab import·구매·vendor 원본 수정은 없다. 신규 6개 외부 Actor 패키지만 생성했고, 후속 수정도 이번 소유 패키지 5개로 제한했다.

## 실제 검사와 발견한 문제

읽기 검사에서 Hub 685개 Actor의 전체 로드를 확인했다. 두 월드 CDO는 1200초, prototype override=false, 자동 시작=false였다. 후보의 PlayerStart는 `(15000,32550,5112)cm`, 기존 Director는 IntroSupply DA를 참조하며 지원 맵은 정확한 후보였다. 클래스 이름 `FirstPerson`은 유지된 기존 명칭이며 이번 작업에서 카메라를 변경하지 않았다.

첫 지면 query는 준비되지 않은 메쉬 때문에 None이었고, 이후 설치 엔진의 공식 StaticMesh compilation 완료 barrier를 사용했다. HitResult 바인딩 오류도 native-break `to_tuple()`으로 바로잡았다. 통과한 검사에서는 지면 3점 모두 Z=0/normalZ=1, 반경42·반높이96cm의 **Visibility** 캡슐 접근 query가 비어 있었다. 실제 Pawn 이동이나 포털 overlap의 증거는 아니다.

첫 작성 시도는 신규 descriptor를 첫 저장 전에 요구해 저장 전 중단됐다. UE5.8은 신규 descriptor를 `OnObjectPreSave`에서 등록하므로, 공식 `is_package_external()/get_package()`로 직접 소유권을 검증하는 방식으로 교정했다. 정확한 6개 Package 객체, 이전 manifest·현재 디스크에 없는 Hub 외부 경로, dirty 객체/이름 집합의 일치를 요구한 뒤 그 패키지만 저장했다.

첫 cold 검사에서는 장식 충돌이 다시 켜진 실제 결함을 발견했다. `set_collision_enabled()`만으로는 메쉬 기본 충돌 상속을 끄지 못했다. 새 6개 파일을 다시 물리 백업한 후, 소유한 Mesh 4개에 `set_collision_profile_name('NoCollision')`을 적용해 `use_default_collision=false`를 저장했다. 표식 색도 위치 인수의 BGRA 혼동을 명시적 RGB 키워드로 교정했다. 수정 전 캡슐은 장식을 맞혔고 수정 후에는 비었다. 이전 프로젝트 파일이나 기존 포털은 수정하지 않았다.

최종 `cold03` 새 프로세스 PID47416은 exit0/RED0이다. 저장 직후의 Actor 전체 기록과 일치하고 descriptor는 691개다. 메쉬·재질, 영구 NoCollision 프로필과 상속 해제, 표식의 font/text/size/정렬/RGBA, 실제 외부 Package와 DataLayer 조건을 읽어 확인했다. 새 장식이 로드된 상태의 캡슐 query도 비었다. 실제 렌더링 검수는 null RHI 검사로 대체하지 않았다.

## 보존과 실행 경계

제작 전 전체 입력 10466개가 D 시점과 같았고, Source/Config·Hub/후보의 2512개 파일과 보호 SaveGames 2루트를 물리 백업했다. 첫 작성 실패 후 전체 입력도 다시 같음을 확인했다. 기존 D archive와 전달본은 각각 별도 물리 payload로 보호했다. 후속 보정 전 신규 6개 파일의 별도 백업도 남겼다.

현재 실행 중인 D bootstrap14384/Game54244는 **이번 입구 추가 전 버전**이다. 같은 게임창의 앱 접근 승인 시간 초과는 유지하며 이번 작업에서 CUA 호출·창 입력·접근 재요청·다른 API를 통한 창 입력 우회·새 게임·공유 빌드·강제 종료는 0이다. 모든 명령은 별도 UserDir·null RHI의 정상 Source 프로젝트 콘텐츠 API 작업이었다.

기존 거절된 `root-startup-approval-A.json` 생성 및 의존 metadata/clone QA는 별도 승인 대기를 유지했다. 해당 파일이나 거절된 런처를 재시도하지 않았다. 각 실패 로그와 phase별 실제 실행 script를 보존했다.

## 남은 검증

정상 프로필 부착 → 신규 포털 overlap → 동일 RunId·1200초 시작 → NPC/F/회수·보상 → 귀환·새 Run·재방문과 실제 save IO는 미실행이다. 현재 D에는 이 변경이 없으므로 D에서 새 포털 검증을 했다고 표시하지 않는다. 맵은 여전히1800×1800m이며 축소·60초 직행·15분 성장·완주·렌더링/UI7 검수도 남았다.

기존 포털 코드는 progression이 미부착이면 baseline 저장을 건너뛸 수 있고, 대상 로드 실패 복구가 별도 구현돼 있지 않다. 이번에는 이 코드를 바꾸지 않았으며 실제 정상 프로필의 travel 검증에서 확인해야 한다.

- [계획과 검토](../architect/2026-11-13_hub-compact-entry.md)
- [저장 결과](evidence/hub-compact-entry20261113A/author02-result.json)
- [소유 패키지 교정](evidence/hub-compact-entry20261113A/collision01-result.json)
- [실제 정밀 cold reload](evidence/hub-compact-entry20261113A/cold03-result.json)
- [물리 백업](evidence/hub-compact-entry20261113A/physical-checkpoint.json)

최종 입력·세이브·D 보존 검사는 [final-result.json](evidence/hub-compact-entry20261113A/final-result.json)에 기록했다. 실제4개통과phase는 inspect03·author02·collision01·cold03이며 각각 exit0/RED0이다. 원래 Source/Config368개, 모든이전콘텐츠, 기존 Hub umap과Actor, 후보/의뢰DA·PlayerStart가 동일하다. 물리백업2512개와보정전신규6개백업도검증했다. AST5/5·범위diff검사는통과했으며커밋·푸시는하지않았다.
