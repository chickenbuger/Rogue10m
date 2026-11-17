# World Map Maker 비 GUI 회귀 준비 — UNEXECUTED

2026-11-13. 현재 원본 Source/Config/Content/Scripts, CandidateA, 사용자 Save를 변경하지 않았다. UE/UBT/UAT/Cook/Game/MCP/GUI를 실행하지 않았다. 이 폴더의 코드·입력 예시·정적 검토만 준비했다. 현재 capture03/seal/run04와 병행하여 실행하면 안 되며, Root가 단독 엔진 사용권을 확보한 뒤 아래 조건을 충족해야 한다.

## 존재하는 도구와 검증 경계

실제 도구는 `Source/Rogue10mEditor/Private/MapMaker/Rogue10mMapMakerModel.{h,cpp}`의 비반영 C++ `Rogue10mMapMaker::FModel`, `SRogue10mMapMaker`, `Rogue10mEditorModule.cpp`의 `Rogue10m.WorldMapMaker` 탭 등록이다. Editor 모듈과 canonical Editor DLL은 존재하지만, 이번에는 탭·Widget 생성 및 패널 선택/입력 경로를 사용하지 않는다. 모듈 StartupModule은 `IsRunningCommandlet()`이면 탭 등록 전에 반환한다.

FModel은 `/Game/Rogue10m/Maps/OpenRunMap`만 허용하며, `MoveSelectionToCell`은 GEditor 선택에 의존한다. 테스트 전용 맵 예외·GIsAutomationTesting·Root hardcoded guard를 변경하거나 흉내 내지 않는다. `SaveMap`은 정확한 맵/외부 package만 저장한다. 기존 `Rogue10m.MapMaker.EditorIntegration`과 `Scripts/RunMapMakerTests.py`는 Slate/선택/UndoRedo를 포함하므로 현재 금지 범위에서 실행하지 않는다. 과거 통과 기록은 이번 cold reload 증거가 아니다.

이 fixture는 공식 `EditorActorSubsystem.set_actor_transform`과 `EditorLoadingAndSavingUtils.save_packages`의 **기저 Actor API 회귀**다. 그룹 평균 XY를 25,600cm 셀 중심으로 옮기고 Z·회전·스케일을 유지한다. FModel 코드 자체, 패널·마우스·키보드·실제 게임·저장 실패 UI 검증으로 표시할 수 없다. 1인칭 기능은 다루지 않는다.

## 현재 실행 차단 조건

- Root의 현재 packaging과 겹치지 않는 독점 QA 엔진 사용권이 아직 없다. 예시 config의 `root_exclusive_qa_execution_authorized=false`를 유지한다. 향후 이 flag는 독립 clone에서의 QA 실행 허가만 뜻하며 원본 입력 seal은 계속 보존한다.
- 완전 물리 QA 프로젝트와 새 WP duplicate가 아직 없다. 원본 또는 기존 CandidateA에서 테스트할 수 없다.
- Python startup 경로의 감사 영수증이 없다. PythonScript commandlet는 fixture 전에 `RunStartupScripts()`를 실행한다. Config `StartupScripts`/`AdditionalPaths`, 엔진·활성 플러그인·프로젝트 mount의 `Python/init_unreal.py`, 격리 UserDir의 Epic Python 경로, `UE_PYTHONPATH`를 확인해야 한다. 원본 Config에는 `bRemoteExecution=True`가 있으므로 아래 commandline override로 비활성화한다. 프로젝트 `Content/Python`과 엔진 `Content/Python`은 이번 좁은 파일 검사에서 없었지만, 전체 startup 경로 부재의 증거로 확대하지 않는다.
- 실제 WP 여부, descriptor와 외부 package의 완전 독립성, commandlet에서의 공개 subsystem 가용성, 대상 Actor의 이동 적격성은 아직 미검증이다. 공개 API가 없거나 StaticMeshActor 0이면 실패·차단으로 기록한다. 임의 성공이나 외부 Actor asset 무맥락 로드로 대체하지 않는다.

## Root가 만드는 독립 프로젝트와 영수증

예정 경로는 `C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/Project`다. capture03의 Content/Config/Source/Build/uproject와 canonical Editor6을 물리 복사한다. 원본 Saved나 사용자 설정은 복사하지 않고 `.../R10MMQA20261113A/User`를 별도 UserDir로 쓴다. project Plugins는 capture03 기준 없으며 필요 엔진 플러그인은 고정 설치본을 쓴다. junction/symlink/reparse point/hardlink는 금지다. 고정 Editor6 전부와 엔진 BuildId를 대조하며 컴파일·project generation은 하지 않는다.

1. Root가 canonical capture03 manifest/hash와 복사본 파일/hash를 대조한다. 복사 전후 원본·default Save 보호 hash를 남긴다. Source/Config/Content/Build/uproject와 Editor6이 완전하게 복사되어야 `physical_snapshot_verified=true`를 기록할 수 있다.
2. GUID 32자리로 `/Game/Rogue10m/Maps/QA/MapMakerMoveReload_<GUID>`를 새로 선택한다. 아래 네 대상은 모두 부재여야 한다: `Content/Rogue10m/Maps/QA/<name>.umap`, `<name>_BuiltData.uasset`, `Content/__ExternalActors__/Rogue10m/Maps/QA/<name>`, `Content/__ExternalObjects__/Rogue10m/Maps/QA/<name>`.
3. 아래 official duplicate commandlet를 clone에서만 실행한다. 시작 직전 부재/경로/lease를 재검사한다. 내부 builder는 기존 destination package를 삭제하는 경로가 있으므로 부재 guard가 필수다. `-Rename`은 넣지 않는다. 실패·timeout·partial target은 그대로 보존하며 같은 target으로 재실행하지 않는다.
4. Root가 정확한 command/own PID+creation time/path/exit0/로그, source hash 변경 0과 destination namespace를 검증한다. `copy-receipt.UNEXECUTED.json`의 새 실행용 사본을 만들고 status를 `independent_wp_copy_verified`, 사실로 확인한 필드만 갱신한다. Editor6 SHA 6개 및 startup audit 근거를 넣고 전체 JSON SHA를 config에 pin한다. 예시를 단순 true로 바꾸는 것은 검증이 아니다.
5. `map_maker_api_fixture.py --build-origin-guard <Temp/origin-guard.json>`을 일반 Python으로 실행하면 UE import 없이 원본 `Source/Config/Scripts/Content/Plugins/Build/Binaries/uproject/Saved/SaveGames`와 `%LOCALAPPDATA%/Rogue10m/Saved/SaveGames` hash를 저장한다. 출력 SHA를 config에 pin한다. 이 보호 hash 범위와 clone 보호 hash는 향후 실행 때 수 GB를 읽으며, 이번 준비 중에는 실행하지 않았다.

## 정확한 commandlet 인자

다음은 **향후 Root 전용 인자 예시**이며 이 문서는 실행하지 않았다. Root의 own-process launcher는 hidden 시작, exact executable/project/creation identity, timeout, stdout/stderr/engine log, 종료 확인을 기록해야 한다. 공유 프로세스를 종료하지 않는다. 기존 Scripts wrapper를 수정할 필요가 없다.

```text
Executable: D:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe
WorkingDirectory: C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/Project
Arguments (각 행은 하나의 argv):
C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/Project/Rogue10m.uproject
/Game/Rogue10m/Maps/OpenRunMap
-run=WorldPartitionBuilderCommandlet
-Builder=WorldPartitionRenameDuplicateBuilder
-NewPackage=/Game/Rogue10m/Maps/QA/MapMakerMoveReload_<GUID32>
-SCCProvider=None
-NoSourceControl
-Unattended
-NullRHI
-NoSound
-NoSplash
-NoLiveCoding
-UserDir=C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/User
-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False
-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:bRemoteExecution=False
-abslog=<fresh absolute duplicate-engine.log>
```

Python phase는 위 공통 인자 중 positional map/Builder/NewPackage를 제거하고 다음을 쓴다. fixture/config 경로에 공백이 없는 현재 경로를 사용하고, `-Script="..."`의 바깥 큰따옴표가 실제 native commandline에 남아 script와 인자가 한 문자열이 되도록 Root launcher에서 검증한다. `-DisablePython`은 쓰지 않는다. Python 플러그인은 기본 비활성 설치이므로 `-EnablePlugins=PythonScriptPlugin`을 명시한다. Root에서 실제 전체 argv/quoting을 검토한 후 실행한다.

```text
-run=pythonscript
-EnablePlugins=PythonScriptPlugin
-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:bIsolateInterpreterEnvironment=True
-Script="D:/Project/Rogue10m/Feature/implementation/map-resume-20261113/world-map-maker-nongui-review/map_maker_api_fixture.py --config <Temp/config.json> --phase discover --execute --owner-handoff <ROOT_EXCLUSIVE_LEASE>"
```

`discover`는 정확한 새 맵을 열고 disk descriptor의 native StaticMeshActor 경로/GUID/label/package만 출력한다. 선택·Actor 로드·이동·저장은 하지 않는다. 결과가 없거나 descriptor package가 새 candidate namespace 밖이면 차단한다. 출력 후보 중 Root가 명시적으로 1~16개 이동 actor와 별도 witness를 골라 config에 기록한다. 발견 단계 결과는 이동 성공이 아니다.

`move`는 새 process에서 같은 인자를 `--phase move`로 바꾼다. 공개 `WorldPartitionBlueprintLibrary.load_actors([Guid])`로 해당 Actor와 witness를 현재 clone editor world에 로드한다. 이 API는 void/Python None이므로 반환값으로 성공을 판단하지 않는다. 실제 `get_all_level_actors()`의 path/GUID/external package를 descriptor와 대조한다. 참조 Actor가 재귀 로드될 수 있으나 exact map 밖 dirty package는 허용하지 않는다. 직접 region-load UFUNCTION은 없고 필요시 공식 `get_intersecting_actor_descs(Box)`→`load_actors(Guid[])` 조합이 가능하다. `unreal.load_asset(externalActor)`나 selection loader를 사용하지 않는다.

일반 숨김 Editor는 허용하지 않는다. 하위 WP loader의 SlowTask가 대화상자를 요청할 수 있으며 commandlet에서는 `!IsRunningCommandlet()` 조건으로 그 대화상자를 억제한다. fixture도 exact UnrealEditor-Cmd 실행 파일과 단일 `-run=pythonscript`, `-Unattended`, `-NullRHI`, 단일 exact UserDir를 검사한다. 이 근거는 임의 startup code의 UI 부재까지 대신 증명하지 않으므로 startup audit도 별도로 필요하다.

`cold_verify`는 move process의 정상 종료를 Root launcher가 `WaitForExit` 결과와 `HasExited`로 확인한 뒤 완전히 새 process에서 실행한다. `move-exit-receipt.UNEXECUTED.json`의 새 사본에 move-result의 PID+creation FILETIME, exact exe/project, exit0, UTC 종료 시각을 기록하고 pin한다. phase를 `cold_verify`로 바꾸며 `--move-exit-receipt <receipt> --move-exit-sha256 <SHA>`를 추가한다. move와 cold의 config/fixture SHA 및 clone 보호 hash가 동일해야 한다. GC/같은 process map 재로드는 cold 증거가 아니다.

## 정확한 변경 범위와 판정

실행 전 모든 actor가 exact native StaticMeshActor, candidate persistent world의 독립 외부 package, `Rogue10m.OpenWorld` tag, 허용 OpenWorld folder, 이동/level unlock, attachment 없음, 보호 label 아님을 확인한다. 진짜 외부 package를 확인할 수 없으면 차단한다. 새 QA 후보 이외의 actor, 원본 또는 CandidateA는 이동/저장하지 않는다.

기본 config의 `author_owner_reference=true`는 첫 이동 Actor에 **새 QA 전용 owner 참조 하나**를 별도 witness로 명시적으로 만든다(원래 owner가 null일 때만). 이는 실제 참조 보존 증거를 만들기 위한 추가 변경이며 FModel 동작이라고 주장하지 않는다. 기존 owner를 덮어쓰지 않는다. 원래 owner 참조만 검사하려면 실행 전에 false로 설정하고 move/cold 동안 config를 유지한다.

후보 내부에서 이동 Actor의 XY 위치와 선택적 owner 한 필드만 바뀐다. exact dirty subset(이동 Actor package, 새 map, 새 BuiltData)만 save하며 witness 또는 다른 package가 dirty면 저장 전에 실패한다. 정체성/GUID·회전/스케일/Z·mesh/material·owner·tag/folder와 witness 기록을 move 직후 및 cold process에서 비교한다. actor의 모든 native/reflected 참조를 전수 증명하는 검사가 아니며 다른 hidden/native dependency 보존으로 확대하지 않는다.

시작 marker는 배타 생성되며 실패/timeout 때 남긴다. phase 결과나 시작 marker가 있으면 재실행하지 않는다. 실패한 clone을 지우거나 자동 rollback하지 않는다. 원본/기본 Save/clone의 candidate 외 파일 보존 검사가 실패하거나 확인 불가면 성공을 취소한다. 최종 허용 표현은 `underlying_actor_api_cold_readback_passed`뿐이며 현재 모든 phase는 **UNEXECUTED**다.

## 정적 근거

설치 UE5.8의 아래 공개 헤더/구현을 읽었다. generic Editor6은 프로젝트 native 테스트를 새로 빌드하지 않고 사용한다.

- `WorldPartitionBlueprintLibrary.h:118-159`, CPP의 GetEditorWorld/LoadActors: public reflected descriptor/Guid loader, 현재 EditorWorld 사용. GetActorDescs는 성공 시 Python Array[ActorDesc], 실패 시 None이며 loaded 목록은 아니다.
- `EditorActorSubsystem.h`, CPP SetActorTransform → `EditorElementSubsystem.cpp` → `ActorElementEditorWorldInterface.cpp`: 선택이 없는 transform 경로, PostEditMove/dirty 통지.
- `FileHelpers.h`: reflected load_map/get_dirty_*_packages/save_packages. WithDialog/SaveAll 미사용.
- `WorldPartitionRenameDuplicateBuilder.cpp`: NewPackage와 Rename flag, 기존 destination 삭제 경로.
- `WorldPartitionActorLoaderInterface.cpp:163-164`와 `SlowTask.cpp:234`: 로드 진행 대화상자 요청과 commandlet 억제 조건.
- `PythonScriptCommandlet.cpp`: Script 문자열 수동 파싱 및 RunStartupScripts. `PythonScriptPlugin.cpp`: mount/user/additional/UE_PYTHONPATH의 시작 script 경로.

프로젝트 파일 SHA와 이번 전달물 SHA는 `DELIVERABLES.sha256.json`에 기록한다. 독립 읽기 리뷰는 최종 fixture `E201B2F5A1E3816639D0809E5977F2F51247977D6CCE3570D2C2658BBF993D13`에서 추가 Critical/Medium 없음이었다. 이것은 UE 실행·컴파일 또는 패널 검증 결과가 아니다.

이번 준비 중 실행한 검증은 다음 순수 Python 명령뿐이다. offline guard/namespace/CLI/cold-exit 검사 8개가 통과했고 기본 plan은 `UNEXECUTED`, `unreal_imported=false`를 반환했다. 실제 출력은 `offline-tests.txt`에 보존했다.

```powershell
& '.venv/Scripts/python.exe' -B 'Feature/implementation/map-resume-20261113/world-map-maker-nongui-review/test_fixture_offline.py'
& '.venv/Scripts/python.exe' -B 'Feature/implementation/map-resume-20261113/world-map-maker-nongui-review/map_maker_api_fixture.py' --config 'Feature/implementation/map-resume-20261113/world-map-maker-nongui-review/config.UNEXECUTED.json'
```
