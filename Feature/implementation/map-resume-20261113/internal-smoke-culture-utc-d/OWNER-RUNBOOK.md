# 통합 소유자 실행 안내

목표는 내부 검사용 Windows Development artifact다. 별도 smoke schema와 release를 사용하며 UI7=UNVERIFIED, visual_review=UNVERIFIED, release_eligible=false가 계획·실행 journal·성공/실패 결과에 고정된다. 실제 GUI/입력/시각/런 플레이 검증을 만들지 않는다. 현재 맵1800×1800m·미축소·NPC 미배치·기본맵 미전환 상태를 기록한다.

## 지금의 차단 사항

실제 NativeTestsA의 raw report는37 clean+6 warning cases,43 Success다. 현재 DLL180DA46E668B12C82DCF3A85B4EB51DB27DB14DBF3E7131FADE312CBB56CB76D와 Source/Config366이 일치한다. 그러나 첫 Test Started 전에 `LogAutomationTest: Error: Condition failed`15행이 있다. test/shutdown 단계 red는 없으며 전체 red0라고 보고하면 안 된다.

`startup-red-review.NOT_APPROVED.json`은 정확한15행과 로그 SHA, LowLevelTestAdapter/LaunchEngineLoop/UnifiedErrorTests/StructuredLogTests 근거 pin을 담는다. emitter와 앞선 영문 로그 예제는 확인했지만 의도된 negative test 또는 지역화 원인으로 확정하지 않았다. 기존 파일의 status만 바꿔 통과시키지 않는다. 통합 소유자가 원인과 영향·내부 artifact 수용 범위를 검토한 **별도 새 파일**을 봉인해야 한다. 새 사용자 결정이 필요한 승인 절차를 추가하는 것이 아니라 현재 증거의 기술 검토를 요구한다.

## 1. 최종 입력 캡처

UE/빌드 사용권은 메인 통합 Work에만 있다. 소유자가 작업을 멈춰 동결한 뒤 아래 CLI를 실행한다. 이 Work는 이 명령으로 실제 원본 전체를 캡처하거나 실행하지 않았다.

```powershell
& $Python '.\internal_smoke.py' --capture --root 'D:\Project\Rogue10m' --engine 'D:\Program Files\UE_5.8' --owner-area $OwnerArea --capture-output $NewCapture --maps $ExactCookMaps --powershell $PowerShellExe
```

NewCapture는 기존 owner-area 아래의 새 경로이며 기존 부모가 있어야 한다. Source/Config/Content/Build/Scripts/Plugins와 유일한 .uproject의 모든 파일·raw SHA·길이·빈 폴더·선택 root 존재 여부를 캡처한다. 선택한 맵별 `.umap` 및 유도한 __ExternalActors__/__ExternalObjects__ 전체집합·부재 여부를 별도 manifest로 기록한다. 설치 UAT/UBT 트리와 engine/validator/runtime 도구 pin도 캡처한다.

원본·build root·LOCALAPPDATA의 SaveGames를 각각 전후 비교한다. 중복 root는 하나로 묶는다. UserDir는 출력6개의 별도 cook_user와 qa_user를 사용한다. 기존 Game EXE/.target은 삭제하지 않고 prior_game_products로 캡처한다. 새 Game link/metadata·타임스탬프·archive inner EXE 동일 SHA를 나중에 요구한다.

캡처 결과는 항상 UNSEALED/NOT_EXECUTED다. 후보/출력 경로의 공백 및 shell 제어 문자·reparse/hardlink를 거부한다. 엔진 실행 경로의 공백은 지원한다. 입력이 바뀌면 다시 동결·캡처하고 Source/Config 또는 DLL이 바뀌면 같은 root의 새 native43 증거가 필요하다. 기존 Native43는 정확히 D:\Project\Rogue10m에서 실행한 것이며 다른 root의 실행으로 경로 치환해 승격하지 않는다.

## 2. smoke seal의 검토값

캡처한 smoke.UNSEALED.json의 별도 사본에서 다음을 채운다. pin은 `{path:절대경로,sha256:대문자 SHA256,size:선택 바이트수}`다.

- status=sealed, engine_build_id=실제 engine/project .modules와 Editor.target의 같은 값. target=Rogue10m, platform=Win64, configuration=Development.
- editor_build={result:pin,log:pin}. 현재 근거는 PacketIntegrationA/editor-build-uht-fix-result.json 및 editor-build-uht-fix.log다. raw exit0·DLL·시간·성공 로그와 실제 Editor6/target/module mapping을 검사한다. 관측한6개 pin을 fresh compile provenance 전체로 과장하지 않는다.
- native43={process,result,reviewed,report,log,selection,startup_review} 각 pin. 실제6개 근거 경로/해시는 results/checkpoint-c-read-only.json.native_refs 또는 native43-retained-a-preflight.json.native_evidence에 있다. startup_review는 검토한 새 파일이어야 한다.
- startup_review schema=R10-native-startup-review-v1, status=reviewed_exact_engine_startup, log_sha256, 순서·timestamp 포함 exact_red_lines, nonblank reviewer/reason, source_evidence pins를 요구한다. test/shutdown red, 순서 위반, 다른 오류/로그/소스는 이 검토로 허용하지 않는다.
- active_destination=/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A. cook_maps와 required_iostore_packages에 정확히 포함한다. 나머지 메뉴/허브/필드 맵도 소유자가 명시한다. smoke_map_selection_final=true는 이번 cook 목록의 동결이며 기본맵 변경을 뜻하지 않는다.
- run_tag(영문숫자20자 이내), owner_area, 기존 독립 output_root와 그 바로 아래의 새6개 evidence/archive/cook/stage/cook_user/qa_user. 기존 출력/lease를 삭제하거나 덮어쓰지 않는다.
- zen_project는 알고 있는 정확한 값 또는 null이다. null이면 실제 cook의 단일 Zen oplog identity를 사후 관측값으로 기록하며 `zen_presealed=false`로 남긴다. 실제 unique project/cooker 명령과 입력 pin을 별도로 검증한다. 다른 후보의 Zen 상수를 복사하지 않는다.

packaging_status=NOT_EXECUTED와 고정 scope/acceptance_flags/known_limitations를 유지한다. 이 seal의 SHA를 별도로 기록한다. status만 바꾸거나 내부 manifest와 파일을 같이 고쳐도 기존 외부 SHA에는 통과하지 않는다.

## 3. 소유 세션 release와 실행

통합 담당의 현재 PowerShell PID/start UTC ticks/executable/SHA를 release에 기록한다. 별도 owner-release.UNSEALED.json 사본은 schema=R10-internal-smoke-release-v1, status=released_for_internal_smoke, 정확한 seal_sha256/project/owner_id/run_tag/active_destination, scopes=[Win64 Development internal smoke], 활성 UTC window를 요구한다. project_build_outputs_authorized=true, gui_forbidden=true, release_eligible=false도 필수다. 실제 실행은 프로젝트 Binaries/Intermediate/Saved와 엔진 진단 경로에 쓸 수 있다.

```powershell
$ownProcess = [Diagnostics.Process]::GetCurrentProcess()
$ownIdentity = @{pid=$PID;start_utc_ticks=$ownProcess.StartTime.ToUniversalTime().Ticks;executable=$ownProcess.MainModule.FileName;sha256=(Get-FileHash -LiteralPath $ownProcess.MainModule.FileName).Hash}
$ownProcess.Dispose()
& '.\Invoke-InternalSmoke.ps1' -Seal $SmokeSeal -SealSha256 $SealSHA -Release $OwnerRelease -ReleaseSha256 $ReleaseSHA -OwnerId $OwnerId -PythonPath $Python
```

기본은 계획만 출력한다. 같은 세션에서 -Execute를 추가해야 SDK VerifySdk와 정규 Game BuildCookRun을 시작한다. 새 계획 두 번·현재 소유 세션·exclusive lease·새 출력6개·매 phase 입력/도구/Editor/보호 저장 재해시를 요구한다. native UAT/UBT mutex·local UBA4 제한을 유지한다. lease는 전역 OS잠금이 아니므로 메인 통합의 실행 창 조율도 필요하다.

프로세스 종료·tree kill·GUI/마우스/키보드·배포·게임 실행 경로는 없다. 자식 PID를 관측했다고 주장하지 않는다. 실패/중단/만료 시 결과와 lease를 남기며 UAT exit!=0, source/config/content 변경, stale Game/archive mismatch는 성공으로 기록하지 않는다. 만료 후 재발급으로 기존 plan을 자동 승인하지 않는다.

## 4. artifact 결과와 IoStore

postflight.json은 internal_smoke_artifact_built_UNVERIFIED 상태다. 신선한 Game link/.target, 실제 local4 flags, 정확한 cooker project/UserDir/Python·MCP 차단, 입력/Editor/저장 불변 및 payload SHA를 요구한다. 실제 UAT branch는 이번 Work에서 실행하지 않았다. 예상 로그 형식과 다르거나 Cook Error가 나오면 출력을 보존하고 검토한다. Native 시작 오류의 검토가 UAT/cook 오류를 자동 허용하지 않는다.

postflight의 next_iostore는 소유자가 official UnrealPak CLI를 실행할 구조화 인수다. wrapper는 이 도구나 게임을 자동 실행하지 않는다. 소유자는 tool/container 전후 pin과 exit/log/session을 기록한 뒤 다음으로 정확한 ExportBundleData package identity를 확인한다.

```powershell
& $Python -I -B -X utf8 '.\internal_smoke.py' --seal $SmokeSeal --seal-sha256 $SealSHA --release $OwnerRelease --release-sha256 $ReleaseSHA --owner-id $OwnerId --verify-iostore $EvidenceCsv
```

이 검사는 postflight의 동일 plan 및 payload hash와 CSV exact map identity를 대조한다. 결과는 iostore_csv_identity_only_UNVERIFIED이며 실제 listing 프로세스 귀속·UI·시각·저장 routing·게임 플레이 승인을 만들지 않는다. 모든 GUI/키보드·마우스 검수와 입력 API 우회는 현재 금지 상태로 남는다.
