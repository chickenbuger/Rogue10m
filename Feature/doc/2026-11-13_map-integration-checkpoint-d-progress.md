# 체크포인트 D 진행 기록 — 비GUI 보존 조사와 내부 Windows 빌드 준비

체크포인트 C 이후 구현과 내부 빌드를 계속하라는 후속 지시에 따른 진행 기록이다. 완료 범위는 아래 실제 실행만 포함한다. Computer Use/GUI 입력 및 입력 API 우회는 계속0회다.

## 실제 확인한 작은 체크포인트

- 재개 시 Source·Config366개와 DLL이 C 때와 동일하고 branch도 유지됐다. 다른 UE·빌드·게임 프로세스가 없었다.
- C에서 실제 로드한 actor 기록을 유지9×5 XY 영역(-33750,22500)..(67500,78750)cm와 대조했다. StaticMesh1282·TargetPoint53·PlayerStart의 bounds는 모두 완전히 안에 있다. outside0/intersecting0이며 충돌/전체참조 인증은 아니다.
- 공식 Python commandlet으로 후보의 on-disk Asset Registry 그래프를 조회했다. 후보1419패키지 모두 asset record가 있었고 원본 World/원본 external package로 향하는 참조0, 후보 밖에서 들어오는 package referencer0, 공유 의존 자산39개를 관찰했다. 후보 패키지 전후 SHA불변·dirty0이다. soft subobject/native 비반영 필드/동적 조회의 완전성까지 증명하지 않는다.
- 공식 Turnkey VerifySdk가 exit0·8.50초로 끝났고 Win64 SDK10.0.22621.0이 Valid다. 설치/업데이트·Game 빌드·Cook은 아직 요청하지 않았다.
- 전체 Content/Build/uproject9524파일의 새 읽기 전용 SHA 기준을 확보했다. 기존 editor/packaged SaveGames458파일의 private 해시 인벤토리도 Temp에 보존했다. 실제 save 파일을 복사·변경하지 않았다.

## 진행 중과 미완료

별도 staged helper v3는 raw EditLayer B/A를 uint16LE로 정규화하고 alpha/flags 분포·GUID/brush·PCG/spline/foliage·live reference·전체 ActorDesc 로드 여부를 조사하도록 준비 중이다. 아직 실제 Source에 설치하거나 컴파일·실행하지 않았다. v2의 RG/paint/metadata semantic 경로는 보존한다.

검토에서 `TObjectPtr::Get()`의 지연 resolve, FReferenceFinder의 soft/custom serialization 누락, foliage native instance/base 참조의 비반영 필드를 확인했다. 각 조사 plane의 unknown/unsupported/잘림을 명시하고 조사만으로 crop 안전 판정을 하지 않는다. 실제 데이터를 받은 뒤 현재 단일층·객체 구성에 한정된 공개 API 매핑 가능성을 판단한다.

후보 A는 여전히1800×1800m·1360actor이며 축소/QuestDirector·DA 배치/기본맵 전환0이다. 기존 strict release와 B/B2 봉인·ArchiveE는 그대로다. 별도 이름/물리 출력의 **미검증 내부 Development smoke package** 준비만 허용된 새 범위로 삼으며, 동일 최종 DLL/입력 검증 후 정규 Game Win64·Cook·IoStore·archive의 실제 결과를 기록한다. UI·플레이 QA 미완료 표시는 유지한다.

## 근거

- [재개 Source/DLL](evidence/map-resume-20261113/checkpoint-d-resume-preflight.json)
- [실제 actor bounds 대조](evidence/map-resume-20261113/actor-retention-bounds-review-d.json)
- [Asset Registry 검토](evidence/map-resume-20261113/AssetDependencyD/reviewed-summary.json), [실행](evidence/map-resume-20261113/asset-dependency-d-exit.json)
- [SDK 실행](evidence/map-resume-20261113/InternalSmokeSdkD/result.json)
- [새 콘텐츠 입력 기준](evidence/map-resume-20261113/BaselineD/content-build-project-sha256.json)
- [읽기 전용 API 위험 검토](../implementation/map-resume-20261113/terrain-dependency-v3/review/2026-11-13_readonly-terrain-dependency-api-review.md)
- [진행 packet 계획](../architect/2026-11-13_terrain-dependencies-smoke-package.md)


## 후속 실제 실행 — v3/v4와 최종 native43

v3 소유helper2파일을백업·사전해시후설치했고C4456 변수shadow2곳이름만수정하여정규Editor재빌드exit0·16.66초였다. 공식commandlet은rawBA179488개/중복4832를완전수집했고packed22139..45181/2065종/flags네종을관측했다. 전체descriptor1360/1360로드와모든object/selection/transaction/texture/layer/dirty불변을확인했다. PCGSubsystem1, foliage/spline/brush0을관찰했으나native/lazy/opaque14항목은미지원이라reflection완료false·crop안전false를유지했다.

v4 ExportCandidateRetainedTiles API를추가해정규빌드exit0·21.63초, SourceCPP2DE929A9...C432144B/헤더F402E599...8F684D71, DLL92CB712BBC20F8299B9474FE704306089005C9760D4A1EC4E0E99BDDEEB49490을확보했다. 실제8채널×45tile=360BIN/1843200B와canonical1794880B를수집했으며Source/DLL/패키지manifest의실제SHA를연결했다. 후보1419패키지와semantic은불변이었다. 독립QA 원본검사기/정책2파일을짧은실제Temp에그대로복사해읽기실행한결과numericStaticPass=true·실패0·미확인19·종료3이다. 이전프로젝트출력범위/길어진sandboxTemp경로실패도보존했으며검사기를완화하지않았다.

같은최종v4 DLL의NativeTestsC는exit0·105.61초,43/43(37clean+6warning12개),전체로그RED0,Source/Config366내용·집합불변이었다. 정확18인수/영어culturetail/startupRED0을강제한별도Culture-D validator 실제사전검증도통과했다. 원래A15오류는승인하지않고기록을유지한다. UI·시각·실제몽타주·입력·완주미검증은같다.

최종Root입력과선택5맵(메뉴/허브/OpenRunMap/기존prototype/1800m후보A),외부패키지·Editor6·보호저장을실제호스트Temp의독립owner영역에캡처했다. 내부전용sealSHA E663DC78E226103E8F34B86F240E649A4A16F30DCEBFF539BD808186E13A9CAA. 기본맵·NPC배치·지형크기변경0,release_eligible=false와UI7/visualUNVERIFIED를유지하며실제Game/Cook전소유세션plan검증진행중이다.

근거: [v3 bounded결과](evidence/map-resume-20261113/TerrainDependencyD/result.json), [raw실자료](evidence/map-resume-20261113/RawInventoryD/result.json), [독립QA](evidence/map-resume-20261113/RawInventoryD/independent-qa-host-shortpath/result.json), [v4빌드](evidence/map-resume-20261113/TerrainRawTileBuildD/result.json), [최종native사전검증](evidence/map-resume-20261113/NativeTestsC/internal-smoke-native-preflight.json), [startup probe검토](evidence/map-resume-20261113/StartupCultureProbeD/review.json).


## 내부 실행기 UTC 차단과 보존 수정

첫 실행은 owner_release_validation에서 중단됐고 UAT실행0·lease0·여섯출력폴더0을실제확인했다. PS7.6.9/KST+09의 ConvertFrom-Json이 UTC문자열을DateTime으로자동변환한뒤 [string]cast가Z/소수초를잃어DateTimeOffset.Parse가+09로재해석했다. 원래13:55Z..17:55Z가13:55+09..17:55+09로해석되어만료오판했다. 실패release/구seal/run01/로그는보존했다.

새 internal-smoke-culture-utc-d 사본은 #requires7.5와 Read-PinnedJson의-DateKind String 두줄만바꿨다. 실제KST·enUS/koKR/deDE와만료/미래/역전/scope/PID/시작ticks/exe/SHA/owner/seal/schema반례34개와독립리뷰를통과했고가드를제거하지않았다. wrapperSHA32A95E82FB7C68485FD90086D0F185E583BAC86A9B28287B2E69F1367C4CFBD9; culture nativevalidator9855...CF98·strict helper는그대로다.

프로젝트/맵/Editor/native 입력의기존동결manifest를보존하고새runtime pin·run02·새정확한livePSrelease만발급했다. 새내부sealSHA774C599EABDA0B82640A2DAEE7A70A01E83109E8A72A69CBD96234B394B263B1. 새실행기에서첫plan과UTC소유검사를통과한후두번째전체불변성검증진행이관찰됐다. 이기록시점에는아직Game/Cook성공을주장하지않는다.

추가Landscape기준: raw EditLayer와현재합성층의export를구분했다. CollapseAllLayers는실행하지않았고금지상태다. raw360tile/heightmap읽기성공을레이어/brush/수정가능성/저장재로드전체보존으로확대하지않는다. 참고원시자료와별개19개보존gate미확인은그대로다.


## 2026-11-13 추가 패킷: Game 테스트 guard 및 내부 빌드 재검증

- 목표: 실제 Game Win64 Development 빌드에서 발견한 Editor 전용 montage fixture의 컴파일 범위를 바로잡고, 새 DLL 회귀 후 별도 내부 빌드를 재시도한다.
- 변경 범위: `Source/Rogue10m/Tests/Rogue10mAuthoredContactSequenceTests.cpp` 첫 줄의 `WITH_EDITOR` 조건 추가, native 검사 실행기의 `NativeTestsD` 출력 이름 추가. 기존 23개 Editor 테스트와 assertion은 유지한다.
- 완료 조건: 정규 Editor 빌드 성공, 새 DLL의 정확한 native 43개 및 전체 로그 RED 0 확인, 새 입력 봉인, 정규 Game/cook/package 결과 기록. 실패 시 완료로 주장하지 않는다.
- 검증: `Scripts/BuildEditor.ps1 -NoHotReload`, `Scripts/RunReviewedNativeTests20261113.py --execute --label NativeTestsD`, 별도 UTC 수정 wrapper의 strict 내부 smoke 절차.
- 복구 경계: `GameGuardFixD/`에 수정 전 두 파일과 SHA를 보관한다. 사용자 WIP나 원본/후보 Content를 되돌리지 않는다. 기존 실패 출력과 seal도 보존한다.
- 현재 사실: run02 UAT exit 6, Game 컴파일 오류 1개, Cook 미시작. 실패 후 입력 10,456개·Editor 6제품·저장 2root 불변. guard 수정 뒤 Editor exit 0, 12.2146초. NativeTestsD는 실행 중이다.
- 지형: 1800m 후보이며 축소 미실행. 단일 Edit Layer raw와 혼합 결과를 구분하고 Collapse All Layers는 사용하지 않는다. height/paint/B/A export만으로 편집 구조·브러시·참조 보존 완료를 선언하지 않는다.
- 도구 제한: Computer Use 및 GUI 입력·우회 0회 유지. UI/시각/물리 입력과 완주 검증은 미검증이다.


## 추가 진행: guard 수정 후 회귀와 새 내부 봉인

실제 Editor 빌드가 종료 코드 0, 12.2146초로 통과했다. DLL SHA256은 `0AB7748B6C89334185031CD285A2160F66B94146BF4D0EA14B1D5DD78BD8E90C`다. 이 DLL의 NativeTestsD는 110.922초에 종료했고 정확한 43개가 성공했다. 37개는 경고가 없고 6개에는 기존 정책과 일치하는 12개 경고가 있다. 전체/시작 로그 RED는 0개이며 Source/Config 366개 및 DLL이 실행 전후 일치했다. 기존 startup15는 승인하거나 감추지 않았다.

새 capture03의 전체 입력 10,456개를 이전 capture01과 대조했다. 변경은 검토한 테스트 CPP guard와 native 출력 D 라벨 두 파일뿐이며, 나머지 입력 및 보호 저장 2개 루트는 일치한다. 입력에는 Content 전체 9,521개가 포함된다. 앞선 tracked Content 2,139개 보존과 전체 Content 범위를 혼동하지 않는다.

준비 도구의 최초 공유 SHA 이후 보완이 진행 중인 상태에서 provisional run03 준비를 먼저 시작한 사실을 기록했다. 그 결과는 보존하되 실행 승인하지 않았다. 독립 검토를 마친 읽기 전용 frozen 준비기 `PrepareInternalSmokeE-62D22D3B6C25.frozen.py`로 별도 run04를 새로 준비했다. frozen 작성기의 SHA는 `62D22D3B6C2527AFD03664035A1979C81F5E60050EDDF3F72B1AC72E24FBA541`이며 실행 전후 일치한다. strict runtime과 기존 실패 출력은 수정하지 않았다.

최종 run04 seal SHA는 `1634A6756D4BE05EFC397441F93695CE192B3D9343DDAB94C60BCC215FE85BF4`다. 2026-11-13 14:42 UTC에 소유 세션에서 내부 Windows 빌드 wrapper를 시작했다. 이 문단 작성 시점은 pre-lease 입력 검증 단계이며 UAT/Cook 성공을 주장하지 않는다. 출력은 `C:/Users/PC/AppData/Local/Temp/R10InternalSmoke20261113D/run04`로 이전 실행과 분리했다. UI7/시각/물리 입력은 미검증, release_eligible=false다.

World Map Maker는 Source의 `SRogue10mMapMaker`와 private `FModel`, `Rogue10m.WorldMapMaker` 탭 등록을 확인했다. FModel의 OpenRunMap 한정·GEditor 선택 의존 때문에 현재 금지된 GUI/선택 조작을 호출하지 않는다. 독립 물리 QA 프로젝트에서 공식 Actor API 이동·저장·다른 프로세스 재로드를 검증하는 준비를 병렬로 진행 중이다. 이는 패널 조작 검수와 다르며 아직 실행하지 않았다.

NPC F 회귀는 현재 예약키 거부·입력 소비·대화 중 차단 경로와 과거 NpcRegF9 92검사의 정확한 증거 위치를 정리했다. 과거 DLL 결과와 NativeD43을 현재 물리 F 검증으로 바꾸지 않는다. 입력 API/Slate를 사용하는 기존 fixture도 현재 실행 금지다. 마우스 동시 기술·차징과 대화 교차, 포커스 복구는 미검증이다.

지형은 여전히 1800m다. raw 8채널·360tile의 수치 검사 통과와 19개 보존 pending을 구분한다. 편집 레이어 GUID·B/A 복원·native/opaque 참조·normal halo·충돌/cold reload의 전체 보존 전에는 crop/원본 교체를 하지 않는다. Collapse All Layers와 Computer Use/GUI 우회 실행은 0회다.

근거: [NativeD production 검증](evidence/map-resume-20261113/NativeTestsD/internal-smoke-native-preflight.json), [실제 Editor 빌드](evidence/map-resume-20261113/GameGuardFixD/result.json), [새 캡처 비교](evidence/map-resume-20261113/InternalSmokeD/capture03-comparison.json), [run04 준비](evidence/map-resume-20261113/InternalSmokeD/prepared04.json), [crop 19개 계획](../implementation/map-resume-20261113/crop-pending19-review/pending19-evidence-plan.md), [F 입력 회귀 메모](../implementation/map-resume-20261113/npc-f-interaction-regression-review.md).


### run04 실제 종료와 보존 진단

Game fresh link/metadata와 Cook2323은 완료했으나 CookOutputDir의 Windows 하위 경로 불일치로 UAT staging exit1이다. Archive/IoStore 완성은 미완료다. 전체10456 입력을 다시 해시한 결과 변경은 생성 Build/Windows/FileOpenOrder/EditorOpenOrder.log 1개뿐이다. Source/Config/Content/Scripts/uproject, Editor6, 보호저장2root는 일치한다. old seal은 invalid로 보존하고 생성로그도 별도 복사했다. [실제 진단](evidence/map-resume-20261113/InternalSmokeD/run04-input-diagnostic.json). 원본 strict/B/B2를 수정하지 않는 별도 내부 derivative의 정규 출력 경로 교정을 준비한다. Source 변경 없는 재시도에서 fresh link를 유지하기 위해 UBT -Rebuild를 검토했고, 최상위 UAT -clean은 Saved/Cooked 삭제 경로 때문에 제외한다. 현재 추가 UE/빌드는 없다.
