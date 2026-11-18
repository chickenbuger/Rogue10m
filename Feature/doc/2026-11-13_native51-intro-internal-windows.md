# Native51·초급 NPC 포함 Windows 내부 빌드

새 D 내부 Development 패키지는 정규 Build/Cook/Stage/Archive·보존 검사·IoStore identity를 통과했고, 실제 EXE가 시작 메뉴를 렌더했다. 두 행의 실제 EXE/PDB 샘플은 row48/Info32/VersionInfo16 및 소비 루프48로 일치한다. 게임 창의 Computer Use 접근 승인이 시간 초과되어 프로필·NPC·종료/재시작 조작은 미실행이다. 실패한 B, 중단된 C, 이전 E5를 보존하며 릴리스 완료로 표시하지 않는다.

## 실제 준비한 입력

정상 공유 프로젝트에서 NPC 처치 귀속 수정의 Editor 빌드와 Native51B 51/51을 확인한 동일 DLL CA647B1E…2CB9175를 사용한다. Source/Config368개는 해당 시험과 일치한다. 초급 DA1·Director 외부 Actor1·folder 외부 Object3을 실제 저장하고 cold reload 및 Editor-game의 F 수락·회수2·골드40 한 번·중복 거부·새 RunId 초기화를 관찰했다. 이 실제 게임은 packaged EXE가 아니다.

이후 전체 입력10466개를 새로 캡처했다. 기존10460개는 불변, 신규 콘텐츠5개와 Editor import가 만든 캐시1개만 추가됐다. Content9526개, 보호 저장2루트 및 이전 E5 payload50파일의 SHA를 대조했다. 캐시는 원본 source와 bytecode 일치를 별도 검토했고 삭제·수정하지 않았다.

Native51Smoke 전달 ZIP SHA9E933B61…12F590/33파일을 Feature/implementation/native51-internal-smoke-20261113B에 정확히 복사했다. helper·policy·validator·PS wrapper를 검토했고 Root의 별도 오프라인47개는 실패0/skip0이었다. 기존 strict/B/B2/E5 도구·봉인은 바꾸지 않았다.

## 현재 동결 상태와 출력

입력은 내부 산출물 생성을 위한 현재 체크포인트로 고정했으며 완성도 인증이 아니다. 지형1800×1800m·미축소, 일반 Hub→Compact 포털 없음/기본맵 미전환, 월드 한글 표식 네모·기존 회색 재질/누락 의존은 그대로 명시했다. 물리 루트·15분 성장·완주·UI7·패키지 저장/실패/재시작은 미검증이다.

명시적 cook6맵은 L_Menu, StartHub, OpenRunMap, FiveVillageGraybox20261110, WorldLayout20261112, WorldLayoutCompact20261113A다. 실제 Hub의 기존 원본 목적지와 새 후보 둘 다 누락하지 않는다. IoStore 확인 요구에는 이6맵과 새 IntroSupply DA도 포함했다.

소유 Temp 영역은 C:/Users/PC/AppData/Local/Temp/R10Native51IntroSmoke20261113B, 실행 출력은 그 아래 run01의 evidence/archive/cook/stage/cook_user/qa_user 6개다. 이전 E5 source/persistent archive·프로젝트·엔진·보호 저장과 겹치지 않는다. 실제 호출 PS7.5+ 자신의 PID/시작UTC ticks/실행파일 SHA를 기록한 동일 세션만 wrapper를 실행한다. 별도의 root-startup 승인 파일 또는 metadata/clone QA 승인과 무관하다.

- [계획](../architect/2026-11-13_native51-intro-internal-windows.md)
- [최종 NPC 실제 검증](2026-11-13_compact-intro-quest-vertical-slice.md)
- [콘텐츠 입력 동결](evidence/npc-intro-compact20261113B/internal-smoke-content-checkpoint.json)
- [전체 보존 결과](evidence/npc-intro-compact20261113B/post-game-preservation.json)
- [별도 오프라인47](evidence/npc-intro-compact20261113B/native51-smoke-offline-result.json)

실제 실행 결과와 IoStore·전후 보존은 후속 결과에 기록한다. 모든 결과는 내부 미검수 빌드이며 release_eligible=false와 UI7/visual UNVERIFIED를 유지한다. 실패는 원문·출력·lease와 함께 보존하며 성공으로 바꾸지 않는다. 거절된 root-startup-approval-A.json 생성과 의존 metadata/clone QA의 추가 실행·재시도·우회는0이다.

## 실제 빌드와 시작 실패 — 후속 진단 결과

정규 Game/Cook/Stage/Archive는 종료0, BuildCookRun418.71초였다. source Game·staged·archive EXE는99E843D7…AD15969로 일치했다. payload50파일/1,951,832,646B/SHA2454269E…6B9F1A4이며 별도 D:/Project/Rogue10m/tmp/InternalWindows20261113Native51IntroB_UNVERIFIED/Windows에 물리 복사했다. 공식 UnrealPak0과 봉인 validator0으로6맵+새DA7개 identity, Export2252개/생성WP셀158개를 확인했다. 기존10466입력·Editor6·저장2루트의 대조도 통과했다.

실제 bootstrap은 Runtime01에서 창/로그가 없었고 Runtime02에서777006으로 끝났다. [Epic UE5.8 종료코드](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/ECrashExitCodes?lang=en-US)의 CrashDuringStaticInit 분류이며 원래 예외 코드는 아니다. 일반 무창 실패2회에서 단순 재실행을 중단하고 Architect/Reviewer로 복귀했다. 실제 메뉴·프로필·QA routing/UI7은 미도달이다. DELIVERY.json은 전달 당시 미실행 기록으로 보존하고 README에 실제 시작 실패를 추가했다.

정확한 소유 Game EXE를 Win32 debug API로 별도 시작해0.297초에 원래 쓰기 AV0xc0000005를 확보했다. 해당 PID27692는 스스로 종료했으며 강제 종료·다른 프로세스 연결·권한 변경은 없다. 일치하는 동봉 PDB(GUID1fb36454-19bd-488e-b1e7-6337f82e59ea/age1)와 캡처 stack/PE unwind 데이터만 사용해7프레임을 복원했다. raw stack 후보와 실제 unwind 프레임은 분리했다.

fault는 TDeferredRegistry의AddRegistration→RegisterCompiledInInfo→AttackSkillData의generated compiled-in 등록 initializer→CRT다. 실제 binary의 producer 배열은40B인데 consumer loop는48B씩 읽는다. 두 번째 항목의 Info를0으로 오독한 뒤 movups[rsi+0x10],xmm0에서 쓰기 실패한다. 원본 generated array는 정상2행/non-null Info 주소다. 사용자 ResourceCost 데이터나 NPC/맵/세이브를 변경할 근거가 아니다.

설치 UBT의 -NoLiveCoding은 bWithLiveCoding=false compile override였다. 프로젝트 WITH_RELOAD0(40B)와 설치 engine consumer의reload1(48B)을 다르게 만들었다. 현재Win64/x64/Development/Game의기본bWithLiveCoding은true다. 후속FixF는 별도 도구 파생에서 UBT compile override만 제거하고, Runtime/Cooker의 -NoLiveCoding과 기존 strict/B/B2/E5·UI7/승인 guard를 유지한다. 실제 재빌드 후 macro/ABI·시작 성공은 아직 미검증이다.

[실제 EXE/PDB 스택과 ABI 원문](evidence/npc-intro-compact20261113B/StartupFailureB/abi-mismatch.json), [시작 실패 구분](evidence/npc-intro-compact20261113B/package-startup-failure-review.json), [실제 IoStore](evidence/npc-intro-compact20261113B/internal-iostore-final.json), [전달본 manifest](evidence/npc-intro-compact20261113B/internal-windows-delivery.json).

## 종료 체크포인트: FixF 준비·C 실행 중단

FixF의 원본33파일 보존과 변경6개를 검토했고 현재manifest47파일의 해시를 Root가 다시 확인했다. helper0CDDDF8F…891D6E0C/core5ACCE0EB…102EA192다. 최종49/49 합성 검사·PS parse0·독립리뷰Critical/Medium0이다. 새 C 전체capture는기존과같은inputSHA C4BDCE58…CD61CD/mapsSHA CAEC9853…0800A4로10466파일이일치했고 Source/Content 수정은없었다.

C 입력을6762A669…488174C로봉인하고실제소유PS7 PID11792의세션을호출했다. 그러나대기호출이‘aborted by user’로중단된뒤cell157과session50569를찾을수없었고소유PS/validator도없었다. 원문journal은21:14:05UTC의pre_sdk_immutable_validation/running에남아있다. sdk_exit/buildcookrun_exit가null이고 C의SDK/UAT/Gamebuild는시작하지않았다. 원문을성공/실패로덮어쓰지않고 별도[C 중단 관찰](evidence/npc-intro-compact20261113B/C-interruption-observation.json)과바이트복사로보존했다.

21:17UTC현재Editor6·보호SaveGames2루트는새로대조해불변이며현재Game EXE는여전히실패B99E843…15969다. 중단뒤전체10466파일을다시해시하지는않았고,마지막전체입력일치근거는Ccapture와pre-SDK이전plan검사다. 현재UE/UBT/Game프로세스0,Root강제종료0,새실행/lease삭제/재시도0이다. 새C의실제macro/ABI/시작성공은미검증이다.

재개때에는외부Codex현재변경과프로세스를다시확인하고기존C출력/lease를보존한새소유출력버전에서시작해야한다. 최소offlineABI검사기는 tmp/PackageAbiVerification20261113C/verify_package_abi.py에준비돼있고새EXE/PDB/definitions pins를받아48/48일치확인후bootstrap/저장routing/실제NPC/UIQA로이어간다. 현재아티팩트를플레이가능완성품으로배포하지않는다. 공유에디터/빌드사용권은이Work에배정된채였으나현재실행중프로세스는없으며상위담당의재조율이필요하다.

## 명시 재개 D — 입력 일치와 새 소유 빌드 시작

사용자의 재개 지시에 따라 C의 PID11792 부재와 UE/UBT/Game 0을 확인했다. C journal/lease/출력은 보존하고 D 전용 capture01/run01을 만들었다. 전체 10466개 입력 SHA C4BDCE58…CD61CD와 maps SHA CAEC9853…0800A4가 직전 실제 콘텐츠 체크포인트와 다시 일치했다. Source/Config368·Content9526, 기존 Editor DLL CA647B1E…2CB9175를 유지한다.

D 입력 seal은 707AADD5A877930CFCE5749459023CAF53AEDCA5817777139DBBDCB8FB01A01F다. FixF의 동결된 helper/core를 그대로 사용하여 실제 PS7 소유 PID55740에서 정규 Game/Cook/Archive 실행을 요청했다. 현재는 사전 불변 검사 단계이며 실제 빌드 성공·최종 48/48·startup·저장 routing은 아직 관찰하지 않았다. D 전용 실행기는 C와 독립 경로만 사용한다.

후속 검증은 최종 EXE/PDB와 UBT 강제 include의 정확한 정의 파일을 묶은 뒤 producer/consumer 48바이트를 확인하고, 새 QA UserDir에서 프로필 생성 전 native screenshot 경로와 저장 backend를 확인한다. 그다음 실제 메뉴·NPC·취소/종료·재시작을 관찰한다. 별도 거절된 startup 승인/metadata-clone QA는 계속 중단한다.

## D 실제 빌드와 샘플 ABI 검토

SDK0, 정규 BuildCookRun0/431.85초, Stage/Archive 및 wrapper 사후 보존 검사0이다. 전체 소유 실행은21:31:43~21:56:13UTC, 실제 BCR은21:41:49부터 수행했다. 새 Source/Stage/Archive EXE는 BA99204285DE4AB0C64AE394BC698B0359C211B4D9662628422A7C2A3AAB4443으로 동일하다. 실제 EXE는341,861,376B, source/stage/archive PDB407,343,104B의 SHA는919BDF0745E2A3E23C8AC9CECD98CAE584BFA8E06CA746C515F3D1E88D919BC5다. PE RSDS와 native LLVM PDB summary는 GUID528f931b-9b9d-408b-afee-eb60a890b1a7/age1로 일치한다.

최종 Game SharedDefinitions SHA FB987E41…347D4D5E에서 UE_GAME1/IS_MONOLITHIC1/WITH_EDITOR0/WITH_LIVE_CODING1/CONSTINIT0/DEVELOPMENT1을 확인했다. 실제 생성 unity1→UHT source, module/PCH 강제include와 link response를12개 파일로 고정했다. generated 파일 직접 수정은0이다. Source/Config368·Content9526을 포함한10466 입력과 Editor6·보호 저장2루트 보존을 strict postflight에서 확인했다.

실제 PDB 전역→배열1365D(96B)→const10DCF→forward10DCC→element64763(48B)·fields64762를 추적했다. VersionInfo10B9D→6855E는16B(Size0/Hash8)다. 두 실제 Info 전역10BB5는32B(싱글턴0/8, reload16)이고 전체 저장 공간이 writable section 내부다. raw Version Size/Hash는48/3203274777과8/1238498172다. 14개 native query는모두0/빈stderr이고 동일한 실제 PDB SHA를 참조한다.

MSVC bounded disassembly에서 initializer가 정확한 배열주소0x151173DA0과count2를 consumer0x1423265B0에 전달한다. 같은 RSI 행에서 register/name/info/version을 읽고 정확한 UScriptStruct registry에 전달한 후0x30 증가·count 감소·동일루프로 backedge를 확인했다. 독립 읽기 리뷰도 이 샘플 연결을 확인했다. 기존 byte fingerprint 양성만으로 수락하지 않았다.

검증 범위는 AttackSkillData의두행과두 Info다. 전체 generated ABI·모든PCH 캐시·CRT 전체연결·registry 마지막 쓰기명령은인증하지 않는다. 초기 collector의 CRT는 중복 심볼/범위한계로 미확인, registry는primary unwind구간만 담았다. startup/UI/routing은 별도 실제 관찰 대상이며 이 정적검토가 대신하지 않는다.

근거: evidence/npc-intro-compact20261113B/D-abi-reviewed.json, D-compile-provenance.json, internal-windows-D-owner-result.json; raw typed/disassembly는 tmp/PackageAbiVerification20261113C/D-native-types-01 및 D-disassembly-01에 보존한다.

## D 실제 시작 메뉴·경로 검사와 UI 접근 차단

공식 UnrealPak 목록0과 기존 봉인 identity validator0으로6맵+새의뢰DA7개, export2252개·WP셀158개를확인했다. 새전달경로는 D:/Project/Rogue10m/tmp/InternalWindows20261113Native51IntroD_UNVERIFIED/Windows이며 payload50개/1,957,690,950B/SHA CF46302DB0FE392ED7BCC3F57E426F468D461FF9C1906463C45778FF9DC137FA다. DELIVERY.json은생성당시미실행이력을그대로보존하고후속관찰은별도증거에남긴다.

22:02:40UTC 실제bootstrap14384가새innerGame54244를실행했다. 실제DX12Game로그는L_Menu로드와GameEngine객체를확인하고, native shot showui가QA User/Saved/Screenshots/Windows/ScreenShot00000.png를만들었다. 원본PNG를열어 ROGUE10M·게임시작·게임종료 버튼이있는실제메뉴를확인했다. 이는Editor-game이아닌새packagedEXE관찰이다. 원본PNG의바이트복사/해시는 PackagedDVisual/01-native-menu-startup.png에보존했다.

실제GameEngine.GameScreenshotSaveDirectory는해당QA경로와일치한다. GetIni의Final Value(s) 아래PlatformFeatures에는SaveGameSystemModule값이없고, 설치Windows/generic소스fallback과일치한다. 다만프로필쓰기전경로probe이며실제save IO/프로필저장routing통과로세지않는다. QA SaveGames는0파일이고프로필생성0이다.

게임창9964506을inventory에서확인한뒤 cua.getApp({windowId:9964506})을요청했으나 도구가301초후 Computer Use app approval timed out을반환했다. 앱접근승인시간초과이며자동승인심사거절로표현하지않는다. GUI입력0,재시도0,CLI/다른API로GUI입력우회0이다. 이후권한관련메시지조회도1회시도했지만user abort로결과가없어새승인근거를얻지못했다. 프로필·NPC·F·보상·종료취소/확인·정상종료·재시작은새패키지에서미실행이다. 이전Editor-game수직흐름으로대신통과시키지않는다.

게임프로세스는관측시14384/54244가살아있었고강제종료하지않았다. 이후빌드/Editor/중복Game을시작하지않고접근승인해결을기다린다. 계속하려면현재같은패키지창의정상접근이필요하다. 별도거절된root-startup-approval-A.json/metadata-cloneQA는그대로별도승인대기이며이번재시도0이다.

마지막전체재해시에서10466입력·Editor6·보호저장2루트(프로젝트458/Local0파일)·이전E5와실패B및새Dpayload가모두동일했다. C의원문/복사6쌍도각SHA일치,lease는현재존재와pin을기록하고재사용하지않았다. 별도Clease의이전SHA불변까지새로증명했다고주장하지않는다. AST9/9,범위diff check0,CheckGeneratedChanges0이다. 기존WIP의binary/CRLF경고는변경하지않았다. commit/push0이다.

근거: evidence/npc-intro-compact20261113B/D-runtime01-checkpoint.json, D-C-preservation.json, internal-D-iostore-final.json, internal-windows-D-delivery.json, D-runtime01-startup-log-snapshot.log. 남은맵1800m·Hub→Compact정상연결·월드한글표식·물리경로/성장/완주와UI7/저장실패는그대로미완이다.
