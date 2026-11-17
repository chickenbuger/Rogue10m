# World Map Maker commandlet의 Python startup 정적 감사

2026-11-13. 이 자료는 향후 Temp 물리 사본에서 실행할 PythonScript commandlet의 사전 검토다. 실제 commandlet과 `import unreal`은 실행하지 않았다. Source/Config/Content/Build/Scripts/Plugins, 기존 capture·seal·증거는 수정하지 않았다. 파일 생성은 이 감사 폴더 안으로 한정했다. DevLog·Sprint 문서·Git도 변경하지 않았다.

**현재 결론은 `PENDING_FINAL_SNAPSHOT_RECHECK / NOT_APPROVED`다.** Root의 별도 host evidence로 Documents/Epic Python 부재와 UE_PYTHONPATH=null 관찰은 확인했다. 나머지 환경, 새 UserDir, 최종 clone·명령행·mount 순서는 실행 직전 재확인이 필요하다. 이 미확인을 빈 배열이나 `true`로 대신할 수 없다. 메뉴 자료 등록만으로 GUI 입력 금지를 위반한다고 판단하지 않는다. 아래 ControlRig 검토는 registry 등록과 실제 창·선택·입력 실행을 구분한다.

## 자료와 사용법

- `collect_startup_inventory.py`: stdlib로 파일만 읽는 수집기. 스캔한 Python을 import/실행하지 않는다. 출력은 이 수집기 아래의 **새 디렉터리**에만 허용하며 기존 결과를 덮어쓰지 않는다. host 환경변수와 Documents/UserDir는 조회하지 않는다.
- `observed-final/inventory.json`: 최종 관찰값, 원본 raw SHA, 설정 행, import/호출 AST 위치. 경로 존재는 현재 설치·프로젝트 관찰값이며 최종 clone의 사실로 승계하지 않는다.
- `observed-final/runtime-inputs.UNRESOLVED.json`: 실행 전 Root가 **다른 장소의 사본에서** 채울 미확인 항목. 이 동결 파일 자체는 수정하지 않는다. `startup_safe=false` 유지가 기본이다.
- `review-source-addenda.json`: 메뉴·progress·프로젝트 commandlet 분기 검토에 추가로 읽은 C++의 raw pin.
- `host-context-review.json`: Root가 별도로 수집한 실제 host evidence의 원문과 SHA, 관찰 시점, 남은 unknown과 snapshot 후 재확인 필요 표시. 수집기 자체의 환경 조회와 구분한다.
- `verify_inventory.py`, `verification.json`: lexical 파서의 문자열/주석/후행 쉼표 반례, 실행 금지·unknown 보존, 기록한 기존 파일의 SHA 재확인. 통과는 오프라인 자료 검증이며 UE startup 실행 승인이 아니다.
- `observed-v1/`, `observed-v2/`: 수집기 보완 전 관찰을 보존했다. v1은 strict JSON으로 33개 UE descriptor의 후행 쉼표를 읽지 못했고 플랫폼 확장도 빠졌다. v2는 이를 보완했으며 final은 embedded boot 후보까지 추가했다. 최종 판단에는 `observed-final`만 사용한다.
- `DELIVERABLES.sha256.json`: manifest 자신을 제외한 이 폴더의 모든 최종 파일 목록·크기·SHA. 최종 인계 후 이 패킷은 수정하지 않는다.

일반 Python으로 수집할 때의 명령이다. **UE 실행 명령이 아니다.** 동결 후 재수집은 이 폴더를 다른 새 감사 위치로 물리 복사한 뒤 그 안에서 한다.

```powershell
& 'D:/Program Files/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -I -B -S '<audit-copy>/collect_startup_inventory.py' --project-root '<final-clone>' --engine-root 'D:/Program Files/UE_5.8' --out-dir '<audit-copy>/fresh-observation'
```

`-I -B -S`는 이 일반 Python 수집기 자신의 환경·site 실행·pycache 생성을 제한한다. 향후 UE embedded interpreter의 실제 sys.path를 측정하거나 해당 startup을 실행한 것으로 해석하지 않는다. 수집기의 AST 신호는 검토 위치를 찾는 보조 자료다. 호출 그래프·조건 평가·동적 import의 완전한 증명이 아니며 `deferred_function_body`도 startup에서 그 함수를 호출하면 실행될 수 있다.

## 관찰한 범위

최종 결과는 plugin descriptor 884개, Python 원문 469개(AST parse 미해결 0), `init_unreal.py` 21개, ini 297개, 관련 설정 행 5개, `.pth` 1개, 기본 소스 pin 15개다. 원문이 strict JSON이 아닌 descriptor 33개는 문자열을 보존하면서 주석·후행 쉼표만 lexical 정규화했다. raw 파일 pin은 그대로 남겼다. `StreamingADA.uplugin`의 `IsExperimentalVersion` 중복 키(true/false)도 보존했으며 임시 last-value parsing을 UE 실제 resolution 증명으로 사용하지 않는다. descriptor parse 실패는 최종 0이다.

현재 프로젝트 `Content/Python`, 엔진 `Engine/Content/Python`, 프로젝트 `Plugins`는 없다. `.uproject`의 `AdditionalPluginDirectories`는 비어 있다. 엔진·프로젝트 Plugins와 설치된 Platforms 하위 plugin/config 후보를 함께 조사했다. Saved/User 설정, 명령행 override, 외부 추가 경로, 최종 mount 집합·순서는 미관찰이다.

`Config/DefaultEngine.ini:117–118`은 Python settings의 `bRemoteExecution=True`를 명시한다. 현재 읽은 Config에는 `StartupScripts`와 `AdditionalPaths`가 없다. 이것이 최종 merged config의 빈 배열을 증명하지는 않는다. 엔진 `BaseEditor.ini:31–34`의 `DisablePythonForCommandlet` 목록은 CookShaders/Cook/GatherText/GatherTextFromAssetsWorker이며 PythonScript 전용 startup 제거 수단이 아니다. 최종 clone의 remote/MCP 설정 override와 실제 적용 여부는 Root의 별도 검토 대상이다. 이 감사는 MCP를 호출하지 않았다.

## 설치 소스로 확인한 경로·실행 순서

아래 cpp/h는 `Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/` 기준이다. line은 수집한 설치본 기준이며 raw pin이 바뀌면 재검토한다.

1. `Private/PythonScriptPlugin.cpp:1139–1181`은 isolation 설정으로 CPython을 초기화한다. 기본 `bIsolateInterpreterEnvironment=true`는 일반 `PYTHON*` 환경과 user site를 제한하지만 **UE_PYTHONPATH는 별도로 추가**된다(`Private/PythonScriptPluginSettings.h:74–81`). 초기 CPython sys.path·site/customization의 최종 실측은 없다.
2. `cpp:1265–1287`은 기존 CPython sys.path 뒤에 `<FPlatformProcess::UserDir>/<EpicProductIdentifier>/Python`, `QueryRootContentPaths`가 반환한 각 mount의 `<filesystem>/Python`, `AdditionalPaths`, `UE_PYTHONPATH` 순으로 경로를 추가한다. mounted root 안의 Engine/Project/Plugin 순서를 이 코드가 고정하지는 않는다.
3. AdditionalPaths는 `ConvertRelativePathToFull`을 거친다. 상대 경로의 기본은 `FPlatformProcess::BaseDir()`다(`Core/Private/Misc/Paths.cpp:1584–1586`). UE_PYTHONPATH는 플랫폼 delimiter로 나눈 문자열을 그대로 AddSystemPath에 전달한다. 상대 항목의 실제 의미에는 실행 CWD 등이 필요하다. AddSystemPath는 존재하는 디렉터리만 추가하고 Python 문자열 동일성으로 중복을 검사한다(`Private/PyUtil.cpp:1248–1267`). 파일시스템 대소문자·alias의 물리적 중복 제거 보증은 아니다.
4. `RegisterModulePaths(cpp:2063–2079)`는 각 Python 루트에 대해 `Lib/<UBTPlatform>/<Arch>/site-packages`, `Lib/<UBTPlatform>/site-packages`, `Lib/site-packages` 순서로 `site.addsitedir`을 호출한다(`PyUtil.cpp:1187–1245`). 수집은 Win64/x64 후보를 기록했다. 실제 target/arch는 Root 확인 항목이다. content mount 추가 시에도 경로 등록이 가능하다(`cpp:2029–2035`).
5. pip 경로는 기본 `ProjectIntermediateDir()/PipInstall`, 유효한 `UE_PIPINSTALL_PATH`가 있으면 그 경로다(`PipInstall.cpp:183–192`). Windows site는 그 아래 `Lib/site-packages`다. `bRunPipInstallOnStartup=false`만으로 이 site 등록이 없어지지 않는다(`cpp:1359–1370`). pip 설치/사전 준비 자체의 파일 쓰기와 의존 모듈은 별도 검토가 필요하다. 실제 pip 경로·내용은 이번에 조회하지 않았다.
6. `RunStartupScripts(cpp:1614–1719)`는 sys.path를 **한 번 복사**하고 각 항목의 실제 `init_unreal.py`를 순서대로 모두 실행한다. 첫 파일 하나를 import하고 멈추는 방식이 아니다. 그 뒤 settings의 StartupScripts 배열, DeferredCommands, fully-initialized 표시, OnPythonInitialized broadcast 순이다. 앞 init이 sys.path를 바꾸면 후속 import 해석에 영향을 줄 수 있지만 이미 복사한 init 열거 목록은 늘어나지 않는다. restrictive permission list의 최종 값도 미확인이다.
7. init은 Public file scope로 실행한다(`cpp:1636–1641`). `__main__`의 dict에서 만든 console global을 공유하며 RunFile은 `__file__`만 설정/삭제한다(`cpp:1246–1257,1940–1965`). 초기 `__name__`은 `__main__`이므로 `if __name__ == '__main__'`을 commandlet 제외 guard로 해석하면 안 된다. 앞 Public script가 global을 바꾸는 경우는 runtime unknown이다.
8. StartupScripts는 literal Python 또는 `.py + args`일 수 있다. 파일은 먼저 주어진 이름(CWD 해석)을 검사한 뒤 sys.path에서 첫 일치를 찾는다(`cpp:1887–1912`). init Public과 달리 일반 file command의 기본 scope는 Private다(`Public/PythonScriptTypes.h:81`). 설정 문자열은 목록 이름만 수집해서는 부족하고 원문·해석된 파일·인자를 함께 검토해야 한다.
9. `PythonScriptCommandlet.cpp:43,52,65`는 force-enable, startup 실행, target script 실행을 한다. plugin 초기화 때의 `OnCommandletPreMain` callback도 startup을 호출한다(`cpp:1383–1390`). fully-initialized guard가 중복 호출을 막는다. 따라서 `-DisablePython`을 PythonScript commandlet의 startup 우회 수단으로 간주하지 않는다. `LaunchEngineLoop.cpp:4103,4110`은 PreMain을 commandlet Main보다 먼저 호출한다.
10. 일반 Python 오류는 RunFile에서 false를 반환해도 startup lambda/루프가 그 결과를 검사하지 않는다(`cpp:1641,1645,1680–1715,1961–1980`). target script 성공만으로 앞 init의 무오류·무부작용을 증명할 수 없다. native fatal/process 종료까지 계속된다는 뜻은 아니다.

## UserDir와 `.pth` 경계

Windows `FPlatformProcess::UserDir()`는 `FOLDERID_Documents`다(`Core/Private/Windows/WindowsPlatformProcess.cpp:1406–1422`). product identifier는 `UnrealEngine`이다(`Launch/Resources/Version.h:79`, `Core/Private/Misc/App.cpp:114–116`). 따라서 대상은 **실제 Windows Documents/UnrealEngine/Python**이다. 수집기는 이 호스트 경로를 조회하지 않았다. 후속으로 Root의 `Feature/doc/evidence/map-resume-20261113/InternalSmokeD/map-maker-host-startup-context.json`을 읽었다. 이 파일은 `2026-11-13T15:25:54.8345060Z`에 Documents=`C:/Users/PC/Documents`, Epic Python 루트와 init 부재, UE_PYTHONPATH=null, 계획 clone 부재를 관찰했다. 이 source evidence의 raw SHA와 원문은 host-context-review에 보존한다. null을 임의의 빈 문자열로 바꾸지 않았고, 나머지 환경변수의 부재까지 추정하지 않았다. 실제 새 snapshot 이후 `recheckNeeded=true`다.

`-UserDir`는 ProjectUserDir/Saved/Intermediate 계열에 영향을 주지만 위 Windows Documents 함수를 바꾸지 않는다(`Paths.cpp:451–498,1940–1965`). 새 UserDir만 만들면 Epic Python path까지 격리된다는 가정은 사용할 수 없다. 기존 non-GUI README의 해당 표현은 이 정정과 함께 읽어야 한다. 기존 README는 수정하지 않았다.

`site.addsitedir`는 `.pth`를 정렬해서 처리하며 `import ` 또는 `import\t`로 시작하는 행을 실행한다(설치 CPython `Lib/site.py:194–195,216–236`). 현재 발견한 `distutils-precedence.pth`는 `_distutils_hack.add_shim()`을 조건부 import·호출한다. 이것을 실행하지 않았으며 transitive 모듈 전체를 무부작용이라고 인증하지 않는다. 초기 embedded site의 `sitecustomize`/`usercustomize`/`._pth` 후보 존재는 final inventory에 기록했다. 외부 경로와 `.pth`가 추가하는 경로·zip/native extension의 전체 동작은 아직 별도 경계다.

## init 21개 읽기 검토

`candidate`는 `.uproject` explicit true, descriptor default true, enabled dependency를 좁게 따라간 **정적 후보**다. platform/target/command-line/plugin manager의 실제 resolution이나 mount 증명은 아니다(`Projects/Private/PluginManager.cpp:421–433`). PythonScriptPlugin 자체 default는 false지만 ToolsetRegistry 의존성을 통해 후보에 들어온다.

| 플러그인 | 현재 정적 후보 | 읽은 startup 동작과 판단 |
|---|---|---|
| ControlRig | default | 5개 RigHierarchy 모듈 import 후 `.run()`, workflow provider 등록. 아래 C++ 검토상 메뉴 자료 등록·refresh 예약을 실제 GUI 입력/선택 실행과 구분한다. |
| IKRig | default | class 존재 조건 아래 workflow import/provider 등록. 등록과 workflow 실행은 구분한다. |
| ToolsetRegistry | dependency | tests import와 PythonTestRunner.create. 테스트 실행 통과나 무부작용 인증은 아니다. |
| AIModuleToolset | explicit | toolset/test import, registry.register, test runner 생성. |
| AnimationAssistantToolset | dependency | toolsets 등록, 여러 tests의 dynamic import와 runner 생성. |
| ConversationToolset | dependency | toolsets/tests import·등록·runner 생성. |
| EditorToolset | explicit | skills/toolsets/tests import, registry 등록, runner 생성. map/save 등 tool 함수는 정의·등록과 호출을 구분해야 한다. |
| NiagaraToolsets | dependency | 7개 skills 모듈 import. decorator/등록 코드까지 import 경계에 들어온다. |
| StateTreeToolset | dependency | toolsets/tests import·등록·runner 생성. |
| NearestNeighborModel | 미확정 | is_editor 조건 아래 training UObject class 정의. training import/reload는 메서드 본문. |
| NeuralMorphModel | 미확정 | 위와 같은 training class/메서드 구조. |
| VertexDeltaModel | 미확정 | 위와 같은 training class/메서드 구조. |
| DetailPoseModel | 미확정 | 위와 같은 training class/메서드 구조. |
| HairCardGenerator | 미확정 | 기본 Public __main__ 조건과 class 존재 조건 아래 controller import/instantiate 가능. 최종 활성 시 추가 검토 필요. |
| Shotgrid | 미확정 | env가 True이면 외부 bootstrap 경로를 sys.path에 넣고 import. host env/파일 미확인이므로 활성 시 허용 판단 불가. |
| MetaHumanGenerator | 미확정 | subsystem/optional content 조건 아래 cache clear·toolset 등록. |
| SequencerAnimMixerToolset | 미확정 | toolsets/tests import·등록·runner 생성. |
| USDPregen | 미확정 | `prepare_python_module()` 직접 호출. 조건이 맞으면 엔진 plugin Python site에 DLL→pyd copy, sys.path 추가. **엔진 쓰기 경로이므로 실제 활성/조건이 있으면 원본 보존 계약과 충돌해 중단 대상.** |
| USDImporter | 미확정 | 설치 init은 0 bytes. USDPregen과 다른 파일이다. |
| MovieRenderPipeline | 미확정 | executor 예제 모듈 import. decorator/callback 정의 포함. |
| CameraCalibrationML | 미확정 | 여러 calibration 모듈을 top-level import, is_editor 조건 class 정의. transitive dependencies 미인증. |

## ControlRig·GUI·선택·console/map/save를 구분한 판단

ControlRig init:6–16은 다섯 모듈의 `run()`을 호출한다. 예를 들어 `RigHierarchy/add_controls_for_selected.py:228–235`는 owner 해제→setup_menus→refresh_all_widgets다. setup은 메뉴 entry UObject와 callback을 등록한다. 그 안의 selected rig 조회, EditorDialog, hierarchy 수정, skeleton commit 코드는 callback의 실행 경로에 있다. **이 callback을 startup에서 직접 invoke한 증거는 없다.** callback/메뉴 명칭만으로 실제 선택 조작이나 데스크톱 입력을 수행했다고 판단하지 않는다.

`UToolMenus::RefreshAllWidgets`는 flag를 설정하고 next tick을 요청한다(`ToolMenus.cpp:3072–3079`). `SetNextTickTimer:3048–3057`은 bound delegate가 있을 때만 예약 요청을 전달한다. 기본 binding 설치는 `Editor/UnrealEd/Private/EditorEngine.cpp:1147–1159`의 Slate 초기화 및 IsToolMenuUIEnabled 조건 안에 있으며 `ToolMenus.cpp:535–542`는 commandlet에서 false를 반환한다. 따라서 **정상 commandlet 초기화의 기본 경로에서는 timer binding을 설치하지 않고 refresh는 flag만 변경한다.** 임의 다른 모듈이 별도 binding을 추가할 가능성까지 없다고 보증하지는 않는다. `HandleNextTick:3081–3113`도 기존 GeneratedMenuWidgets의 유효 widget만 갱신하며, 비어 있으면 갱신 callback은 실행하지 않는다. `ExtendMenu`/`AddMenuEntryObject`는 메뉴 자료를 만들거나 연결하며 창·탭을 직접 열지 않는다. `SetupAllEditorMenus:90–95`에서 호출하는 hierarchy의 CreateContextMenu/CreateDragDropMenu도 ExtendMenu/AddDynamicSection으로 lambda를 등록한다. `ToolMenu.cpp:206–210`은 Construct delegate를 저장하고 그 자리에서 실행하지 않는다. ToolMenusModule의 StartupModule/ShutdownModule은 빈 body다. 정확한 추가 분기 검토와 pin은 addenda에 남긴다. 이 registry 초기화를 commandlet 전체 금지 사유로 삼지 않는다. 검토한 startup 경로에서 InputKey/마우스·키보드 injection, 메뉴 execute callback의 직접 호출, map load/save 직접 호출은 찾지 못했다. 이는 모든 native 모듈의 sideeffect0 보증이 아니다.

프로젝트 `Source/Rogue10mEditor/Private/Rogue10mEditorModule.cpp:22–25`는 IsRunningCommandlet이면 탭 크기·spawner·menu·`Rogue10m.MapMaker.Open` 등록 전 반환한다. `OpenMapMaker()`의 TryInvokeTab 및 SpawnMapMaker의 Slate panel 생성은 이 commandlet startup에서 호출하지 않는다. 앞으로도 GUI panel/tab/선택 입력 callback, console로 tab을 여는 명령, InputKey/Slate 입력 우회는 QA 범위에 넣지 않는다.

EditorToolset의 `asset.py:348 eas.save_asset`, `scene.py:456 AssetTools.save_assets` 등은 등록된 tool 함수 본문이다. startup의 등록이 그 tool 호출을 증명하지 않으며, QA가 이 함수를 호출하도록 추가하는 것도 이번 감사가 허용한 일이 아니다. Root가 검토한 fixture의 clone 한정 명시 API 작업과 원본/엔진 쓰기 경로를 따로 판정해야 한다.

Python startup의 FScopedSlowTask progress도 창 생성과 feedback update를 구분한다. `SlowTask.cpp:170–175`의 MakeDialogDelayed는 지연값만 저장하고, MakeDialog의 `!IsRunningCommandlet()` 조건(:234)이 commandlet의 progress dialog 생성을 막는다. ForceRefresh는 feedback context로 요청을 전달하므로 전체 no-op으로 확대하지 않는다. 이는 사용자 입력을 합성하는 코드라는 뜻도 아니다. 추가 feedback 분기 검토는 addenda에 기록한다.

## Root가 실행 전에 별도로 채울 항목

1. fresh physical clone의 정확한 경로·완전한 입력 seal·실행 파일과 Editor6 pins·Save/SourceConfig/Content 보존 증거. 기존 Capture03는 최신 seal로 간주하지 않는다. 이 자료는 새 capture05를 자동 승인하지 않는다.
2. 실제 host 환경변수의 존재와 값, Documents Epic Python 경로의 존재·원문·SHA 검토. 없는 값은 관찰한 부재로 기록하고 미관찰 null과 구분한다. `UE_PYTHONPATH`, `UE_PIPINSTALL_PATH`, Python isolation 및 Shotgrid/bootstrap 항목을 생략하지 않는다.
3. fresh UserDir의 실제 경로·부재/새 생성 상태와 최종 config hierarchy/override. remote/MCP 자동 시작을 비활성화하는 최종 argv와 적용 근거. 표준 -Unattended/-NullRHI만으로 startup 경로나 외부 Python code가 사라진다고 가정하지 않는다.
4. 최종 enabled/mounted plugin 후보와 mount 순서, 초기 CPython path, AdditionalPaths/CWD/BaseDir 해석, pip/site/.pth 및 그 추가 경로, StartupScripts·DeferredCommands·OnPythonInitialized callback. known init 21개 외의 startup 경로가 생기면 새 원문을 검토한다.
5. 위 사실과 검토한 fixture/정확 argv를 별도 evidence에 pin한다. 실제 데스크톱 입력/창·탭 열기·선택 조작 callback 또는 원본·엔진 저장을 startup에서 직접 실행하는 새 경로가 확인되면 실행하지 않고 구체 경로를 보고한다. 단순 메뉴 자료 등록은 이 차단 사유와 구분한다.

이 패킷은 Python startup의 정확한 파일·설정·경로와 알려진 import 경계를 다룬다. 임의 native 모듈 전체, 실제 headless 실행, 렌더, World Map Maker 패널, 물리 입력 또는 게임 회귀 검증으로 확대하지 않는다. 3인칭 유지·1인칭 보류·피격 취소·면역 보류 계약은 변경하지 않았다.
