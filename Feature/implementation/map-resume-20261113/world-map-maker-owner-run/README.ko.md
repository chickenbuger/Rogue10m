# Root 전용 비 GUI OwnerDriver — UNEXECUTED

이 폴더만 새로 작성했다. 기존 E201 fixture·입력 예시·manifest와 공유 Source/Config/Content/Scripts는 수정하지 않았다. 실제 UE/UBT/UAT/Cook/Game/MCP/GUI·프로세스 조회·prepare 실행은 0이다. 기본 명령은 plan이며 파일 쓰기와 프로세스 생성도 없다. Windows run05가 끝나고 Root가 독점 엔진 사용권을 넘기기 전에는 `--execute`를 쓰지 않는다.

## 고정 근거와 실행 전 입력

- 물리 clone: `C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/Project`. 상위 `physical-clone-receipt.json` SHA `8AF525EA9D9A0132A3742D9D0E1D6EDEDAD24183B1366572DA53D4C3A063CA18`. 9,896파일/5,089,364,691B, Editor6/BuildId55116800을 확인한 receipt다. WP duplicate나 Editor 성공을 뜻하지 않는다.
- 기존 fixture SHA는 `E201B2F5A1E3816639D0809E5977F2F51247977D6CCE3570D2C2658BBF993D13`으로 고정했다.
- 기존 startup audit14(자체 manifest+13 entries)의 manifest SHA `1ADF625535F9DDEF28916AB4922C5298071ADF20987ACB028DB6832656609E49`; host 관찰 SHA `7D35ADAE8CCAE0CAA52ADABD5D2C9466503E5DCF7C12803AF73DEF4FE7E32A7E`. 역사적 관찰이며 실행 허가가 아니다. 실제 기록된 source/INI/descriptor/Python/부트 후보 및 namespace를 매번 다시 검사한다.
- **run05 종료 뒤 새 OriginGuard가 필수**다. E201의 `--build-origin-guard <Temp/new-name.json>`을 일반 Python으로 Root가 실행하고 출력 SHA를 새 config에 넣는다. 전체 현재 Source/Config/Scripts/Content/Plugins/Build/Binaries/uproject/Save와 default user Save를 보호한다. 현재 Game binaries 값도 이 guard에 포함된다. 이전 capture03/05 seal의 값을 무단으로 최신 hash로 대체하지 않는다.
- Root가 run05 후 생성한 `origin-after-win05.json` SHA `78521AA53F6522EF5C1AE630B3B0B4237D2EC3611247A1D4C15A550AC9DC81E7`과 현재 exe/PowerShell pin을 넣은 `config.origin-after-win05.UNEXECUTED.json`도 제공한다. 실행 flag는 false, lease는 빈 값이다. Root가 Temp 사본에서 구체 실행을 승인해야 하며 이 예시는 freshness를 자동 보증하지 않는다.
- 새 config 사본에 정확한 Cmd exe·PowerShell exe SHA, 독점 `owner_lease`, `root_exclusive_qa_execution_authorized=true`, 새 `OwnerRun_<ASCII>` sibling, fresh `User` 또는 `User_<ASCII>` sibling을 기록한다. 기존 template 자체는 그대로 둔다. 최초 UserDir는 부재여야 한다. 첫 실행 전에 config를 확정하며 reserve 뒤 config 변경은 새 session을 요구한다.

물리 receipt의 모든 clone 파일/hash·single link·source와 별도 file identity·immutable namespace를 검증한다. Source/Content/Config/uproject/Editor6은 fresh OriginGuard 안 값과 물리 receipt가 같아야 한다. 원본 Build file-open 로그는 run05에서 바뀔 수 있으므로 예전 물리 receipt의 source Build hash로 판정하지 않고 **fresh OriginGuard 전체 Build hash**로 보호한다. clone Build는 물리 receipt와 계속 같아야 한다. stale guard/새 shared 변경은 자동 보정하지 않고 차단한다. full hash가 많아 실행 단계의 검증에는 시간이 걸릴 수 있다.

## 단계별 사용

아래 `python`은 Root가 지정한 표준 Python이다. `driver`는 이 폴더의 frozen 사본, `config`는 Temp의 Root 작성 사본이다. 이 문서의 명령은 실행하지 않았다.

```text
python <driver> --config <config>
python <driver> --config <config> --phase reserve --execute
python <driver> --config <config> --phase startup
```

기본 plan은 engine/process queries/writes=0이다. `reserve`만 새 session과 유일 GUID candidate를 만든다. 정확한 `/Game/Rogue10m/Maps/QA/MapMakerMoveReload_<32hex>` 아래 map/BuiltData/externalActors/externalObjects 네 대상의 부재를 확인한다. 원본·CandidateA·이전 partial target을 재사용하지 않는다.

`--phase startup`처럼 `--execute` 없는 native phase plan은 정확한 argv, Windows quoting 결과, 5줄 runner 본문/hash, 코드 hash와 startup surface/hash를 출력한다. **승인 파일을 생성하지 않는다.** Root는 `approval.UNEXECUTED.json`의 Temp 사본에 실제 검토 결과를 기록한다. `approved=true`와 네 review flag는 Root가 실제 final argv·plugin plan·probe 이전 startup 부작용·UI/Actor startup 금지 준수를 확인한 경우에만 설정한다. plan의 config SHA는 reservation 또는 config 자체의 SHA로 확인한다. phase/config/code/argv/wrapper/surface/원본 guard 및 고정 근거 pin이 모두 일치해야 한다.

```text
python <driver> --config <config> --phase startup --execute --approval <Root-startup-approval.json> --approval-sha256 <SHA>
```

`startup_probe.py`는 Actor/world API를 호출하지 않는다. `PluginBlueprintLibrary.get_enabled_plugin_names`, `Paths`, `SystemLibrary` getter와 **이미 로드된 CDO만** `find_object(..., follow_redirectors=False)`로 찾아 설정을 읽는다. 로드·설정 수정·패널·Widget·선택·입력은 없다. CDO나 getter가 없으면 unverified다. enabled plugin 목록은 native module 로드 목록의 증거가 아니다. 네이티브 commandlet 전역의 public Python getter는 확인되지 않아 unverified로 남기며, exact Cmd exe·`-run=pythonscript`·Unattended getter/NullRHI CLI 증거와 구분한다.

probe 이전에 `RunStartupScripts()`가 실행된다. 따라서 probe 결과로 startup 사전 승인을 대신할 수 없다. `expected_metadata`가 없거나 실제 getter 값과 다르면 metadata를 남기고 실패하며 다음 단계는 차단한다. Root가 관찰을 참고해 새 session/fresh User sibling의 **새 승인**을 직접 준비할 수 있으며, 기존 실패 session을 지우거나 재시도하지 않는다. 비교 expected_metadata의 정확한 6개 키는 다음과 같다: `enabled_plugins`, `python_settings`, `python_user_settings`, `mcp_auto_start_server`, `UE_PYTHONPATH`, `init_unreal_files_on_observed_sys_path`. 값의 형식은 probe의 metadata 출력과 같아야 한다. 빈 배열/기본값으로 모르는 값을 메우지 않는다.

첫 bootstrap은 Root가 Source/Config, 실제 Cook285 mount와 알려진9 init, host 환경, final argv를 검토해 **metadata 획득만** 구체 승인할 수 있다. `scope_notes`에 그 근거와 허용한 registry registration/refresh-flag 초기화 범위를 기록한다. 이것들은 사용자 GUI 입력과 구분하며 전역 startup 무부작용으로 확대 인증하지 않는다. `no_ui_or_actor_startup_actions_approved`는 registry 초기화까지 금지하는 뜻이 아니라 실제 UI 조작·Actor 작업을 허용하지 않는 phase 범위다. `expected_metadata=null`인 bootstrap은 관찰 후 unverified로 멈추는 것이 정상이다.

startup이 정상 종료하고 metadata가 Root 기대와 일치하면 Root는 실제 metadata SHA와 **현재 startup surface**를 검토한 새 duplicate 승인 JSON을 작성한다.

```text
python <driver> --config <config> --phase duplicate
python <driver> --config <config> --phase duplicate --execute --approval <Root-duplicate-approval.json> --approval-sha256 <SHA>
```

duplicate는 official `WorldPartitionBuilderCommandlet`/`WorldPartitionRenameDuplicateBuilder`/`-NewPackage`만 사용하고 `-Rename`은 없다. 내부 builder는 기존 destination을 삭제할 수 있어 native 시작 직전에도 네 대상 부재를 다시 검사한다. 새 target namespace 밖 clone immutable 파일과 전체 원본/Save를 비교한다. exit0·새 map/external actor 결과·보존 검사가 통과한 경우에만 E201이 요구하는 별도 `duplicate/copy-receipt.json`을 만든다. 이것은 자동 startup 승인이나 Actor 적격성 판정이 아니다. namespace의 실제 descriptor 검사는 discover의 E201 guard가 수행한다.

이후 동일 방식으로 Root가 각 phase plan→새 phase 승인→실행을 진행한다.

```text
python <driver> --config <config> --phase discover
python <driver> --config <config> --phase discover --execute --approval <Root-discover-approval.json> --approval-sha256 <SHA>
```

discover 결과를 보고 Root가 `selection.UNEXECUTED.json`의 Temp 사본에 정확한 candidate, 1~16 Actor path, 별도 witness, destination cell과 선택적 owner-reference authoring 여부를 직접 기록한다. 자동 Actor 선택은 없다. Root move/cold 승인에는 같은 selection SHA도 넣는다.

```text
python <driver> --config <config> --phase move --selection <selection.json> --selection-sha256 <SHA>
python <driver> --config <config> --phase move --execute --selection <selection.json> --selection-sha256 <SHA> --approval <Root-move-approval.json> --approval-sha256 <SHA>
python <driver> --config <config> --phase cold_verify --selection <selection.json> --selection-sha256 <SHA>
python <driver> --config <config> --phase cold_verify --execute --selection <selection.json> --selection-sha256 <SHA> --approval <Root-cold-approval.json> --approval-sha256 <SHA>
```

move/cold는 동일한 selected-config bytes를 쓴다. move의 실제 PID+creation FILETIME·exact executable·exit0를 기록한 `exit-receipt.json`을 E201 cold wrapper에 pin하며, 완전히 새 native process를 띄운다. 같은 process map reload는 cold 증거가 아니다. E201의 existing partial marker/config/저장 경계는 완화하지 않았다. 패널/FModel 검증으로 표시하지 않는다.

## launcher와 startup 표면 보호

각 Python native phase의 Temp `runner.py`는 정확히 5줄이다. `sys.argv=[fixture_or_probe, ...]`를 Python literal로 고정하고 `runpy.run_path(..., run_name='__main__')`를 호출한다. native `-Script=<runner.py>`에는 공백·따옴표·내부 인자가 없다. 프로젝트/엔진 경로는 `subprocess.Popen`의 argv list와 Windows quoting으로 전달하며 shell을 쓰지 않는다.

모든 native argv에 Cmd/Unattended/NullRHI/NoLiveCoding, 새 UserDir, remote=False/MCP auto=False/isolate=True, pip startup=False와 developer mode=False를 넣는다. Python 플러그인은 CLI로 활성화하며 project 파일은 수정하지 않는다. `ForcePipInstallOnInit`, `-DisablePython`, 임의 추가 argv를 받아 넣는 기능은 없다.

각 phase 승인과 launch 직전 검사는 UserDir 전체, clone Saved/Config, Intermediate/PipInstall, 기록된 외부 startup 파일/namespace, UE/Python/user 환경의 fingerprint를 대조한다. 종료 후 표면도 별도 보존한다. 이전 metadata만으로 달라진 설정을 자동 승인하지 않는다. static inventory의 `Lib` 제3자 site-packages/native 구현은 전수 source 인증 범위가 아니며 별도 Root 감사 경계다.

native 시작 직전 pinned PowerShell을 hidden으로 실행해 UE/UBT/UAT/Game/ShaderCompileWorker/UnrealPak/IoStore와 해당 cmd/dotnet wrappers의 active 여부를 필터된 CIM 목록으로 확인한다. 조회 실패/unknown/timeout이면 launch하지 않는다. Root 독점 lease는 별도이며 이 순간 확인이 다른 운영자의 향후 launch를 잠그는 OS mutex라고 주장하지 않는다.

native는 hidden·stdin null·stdout/stderr/engine log 별도 파일, PID/creation/exe SHA/argv/종료 receipt를 기록한다. **timeout이나 identity 불확실 시 kill/terminate하지 않는다.** process와 partial target/evidence를 그대로 두고 session 후속 단계를 차단한다. 정상 root 종료가 child 전체 종료를 보증한다는 주장은 하지 않는다. 다음 단계의 idle guard도 다시 확인한다. 실패 session/target을 자동 삭제·복구하거나 재사용하지 않는다.

## 완료한 정적 검증과 한계

순수 Python mock 검사 9개 통과: 기본 plan 무변경, 무승인 차단, 5줄 argv wrapper, noRename, startup surface 승인 변경 차단, native/query timeout kill0, getter-only probe, 기존 E201 hash 보존. 실제 audit 기록 입력·Python namespace 재검사도 읽기 전용으로 통과했다. UE/실제 process query는 수행하지 않았다. 독립 리뷰에서 발견한 UserDir overlay gap을 보강한 뒤 추가 Critical/Medium은 없었다.

현재 reserve/startup/duplicate/discover/move/cold는 전부 UNEXECUTED다. 최종 전달물 hash는 이 폴더의 `DELIVERABLES.sha256.json`에 있으며 Root 실행 전 코드/입력/승인/원본 guard가 달라지면 실행을 차단하는 것이 정상이다.
