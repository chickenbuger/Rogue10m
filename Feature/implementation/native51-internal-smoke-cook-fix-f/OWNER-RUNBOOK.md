# 메인 통합 소유자 인계

이 패킷의 현재 상태는 UNSEALED/NOT_EXECUTED다. 최종 콘텐츠가 편집 중이므로 실제 전체 capture나 content freeze를 만들지 않았다. 공유 Editor/빌드/콘텐츠 사용권은 메인 통합에만 있다. 별도 승인 대기인 root-startup-approval-A.json·metadata/clone QA는 이 도구의 범위 밖이며 생성·재시도·이름/경로/런처 변경 우회를 하지 않는다.

## 근거와 재사용 경계

기존 E5는 정규 Win64 Development Game/Cook/Stage/Archive0와5맵 IoStore identity가 확인된 내부 artifact다. E5는 NPC 처치귀속 수정 전의597E…Game EXE다. 이후 Native51B는 CA647B1E2264CB5DBC5025F81D2C28E10E12CDE5BF889F552033F8B1D2CB9175 DLL과 Source/Config368에 연결되며 새8+기존NPC18+G08/VFX25의51개 성공이다. raw report40 clean+11경고 사례,19events를 보존한다. 전체 로그 경고42줄은 시작4+실제19+controller echo19이고 RED0다.

NativeD도 실제 영어 culture18인수·43개/RED0를 확인했다. 과거 NativeA의15오류 미승인 표시는 이 새 자료에 재사용하지 않는다. Native51은 startup_review를 거부하며 raw 전체 Error/Fatal/Ensure0를 요구한다. 별도 root-startup 승인 거절 문제와 혼동하지 않는다.

실제 Cook-Fix-E helper D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF는 그대로다. 공식 UBT `-Rebuild`(최상위UAT clean 없음), CookOutputDir=<cook>/Windows, FOOBASEDIR=<cook_user>, 실제 file-open-order/Windows Zen metadata를 확인한다. 새 derivative는 실제 archivedirectory 및 staged/Game/archive EXE 동일 SHA도 추가한다. PS7.5와 JSON DateKind String/UTC 세션 검증을 유지한다.

## 1. 새 전체 입력 capture

메인에서 NPC/DA/Director 작업을 마치고 실제 저장·최종 입력 상태를 확인한 뒤 새 독립 물리 경로에 캡처한다. 원본과 엔진, 보존E5의 persistent/source archive, 이전 출력에 겹치면 안 된다.

```powershell
& $Python '.\internal_smoke.py' --capture --root 'D:\Project\Rogue10m' --engine 'D:\Program Files\UE_5.8' --owner-area $NewOwnerArea --capture-output $NewCapture --maps $ExactFiveOrReviewedCookMaps --powershell $PowerShellExe
```

Source/Config/Content/Build/Scripts/Plugins/uproject 전체의 raw SHA·파일 집합·빈 폴더와 선택 맵 .umap/유도 WP 외부 패키지 전체집합을 저장한다. Editor6·설치 도구·기존 Game products·원본/build-root/기본 사용자 SaveGames도 고정한다. 발견한 Native51B/build/preservation 참조는 관측 pin으로 연결하지만 capture는 항상 UNSEALED다. UI/시각/저장 routing을 통과로 만들지 않는다.

현재 Source/Config 또는 DLL이 Native51B에서 달라졌으면 새 정규 Editor/같은 suite 증거와 검토한 정책이 필요하다. content-only 추가도 새로운 전체 input/map manifest와 freeze가 필요하다. 과거9521 Content 또는10460 입력 수를 최종값으로 고정하지 않는다.

## 2. 최종 콘텐츠 freeze와 입력 seal

`content-freeze.UNSEALED.json`의 별도 새 데이터 기록에는 schema=R10-native51-content-freeze-v1, status=frozen_for_internal_smoke, 정확한 project/active_destination/input_manifest_sha256/map_manifest_sha256/editor_dll_sha256, recorded_utc/recorder, content_state와 실제 저장/배치 근거 pins가 필요하다.

content_state는 status=OWNER_RECORDED_FINAL, map_extent_m=[실측 또는 검토된 정수2개], npc_placement와 default_map 설명을 담고 seal의 content_state와 정확히 같아야 한다. 이 파일은 입력 동결 기록이며 Engine 시작/metadata/clone QA 승인 파일이 아니다. 이번 패킷은 실제 frozen 기록을 생성하지 않았다.

입력 seal은 schema=R10-native51-smoke-seal-v1이다. capture의 필수 null을 검토값으로 채운 별도 파일에서 status=sealed, smoke_map_selection_final=true, 고유 run_tag/output_root/출력6개를 정한다. packaging_status=NOT_EXECUTED, release_eligible=false와 UI7/visual UNVERIFIED를 유지한다. native51 refs는 process/result/reviewed/report/log/selection/build_result/launcher/preservation9개 pin이며 build_result는 editor_build.result와 같은 pin이어야 한다.

Native51 정책은 정확한51 selection JSON·new fixture source·build receipt·launcher와 연결된다. 각11사례의 raw warning 전체 wrapper(시간·파일·행 포함)와19순서·actor suffix를 대조하며, 유효한 world/actor32hex GUID쌍만 정규화한다. source/runtime이 바뀐 새 실행을 이 과거B 정책에 자동 끼워 맞추지 않는다.

preserved_artifacts의 첫 pin은 실제 persistent-delivery05.json이고 그 delivery_manifest도 검증한다. output_root/capture는 두 E5 archive의 내부·같은 경로·상위 경로를 모두 거부한다. 기존 output/Game products를 삭제하지 않고 prior_game_products를 검증하며 정규 Rebuild의 fresh link/metadata를 요구한다.

## 3. 별도 소유 세션 release

실제 실행은 메인이 코드 리뷰 후 자기 PowerShell 세션에서만 한다. release schema=R10-native51-smoke-release-v1, status=released_for_native51_smoke, scopes=[Win64 Development Native51 internal smoke]다. 정확한 seal SHA/project/run_tag/map/owner_id, 현재 PID/start UTC ticks/exe/SHA와 유효 UTC window가 필요하다. project_build_outputs_authorized=true, gui_forbidden=true, release_eligible=false는 이 build 실행의 범위를 표시한다. 실제 release는 이 Work에서 발급하지 않았다.

```powershell
& '.\Invoke-InternalSmoke.ps1' -Seal $ReviewedSeal -SealSha256 $SealSHA -Release $OwnerRelease -ReleaseSha256 $ReleaseSHA -OwnerId $OwnerId -PythonPath $Python
```

기본은 계획만 출력한다. -Execute를 더해야 새 lease/출력6개·SDK verify·정규 Game/cook/stage/archive를 시작한다. 같은 입력/도구/Editor/저장/보존 pin을 phase마다 재검사한다. native UAT/UBT mutex와 메인의 실행 창 조율이 필요하다. 프로세스를 종료하거나 GUI/게임을 실행하는 경로는 없고 자식 PID 관측을 주장하지 않는다. SDK/UAT 실패, source/content 변화, stale Game, FOO/cook/stage/archive 불일치는 실패로 남는다. 이전 출력과 lease를 보존한다.

## 4. 결과 범위

postflight 성공은 internal_smoke_artifact_built_UNVERIFIED다. exact new output chain·freshGame·payload·입력/Editor/저장 불변을 확인한다. next_iostore의 공식 CLI는 메인이 별도 소유 실행 증거를 남기며 호출한다. `--verify-iostore`는 같은 plan/payload와 정확한 ExportBundleData map identity를 대조하고 listing 프로세스 귀속이나 실제 UI/저장 routing을 대신하지 않는다.

Native51 CPP 증거와 실제 map Director/loot/충돌/run GUID/저장/포털 동작을 구분한다. 새 artifact도 실제 UI·시각·물리 플레이·저장 routing은 UNVERIFIED다. release 승인·배포·기존 E5 업데이트는 이 도구의 동작이 아니다.
