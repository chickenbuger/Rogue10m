# Cook-Fix-E 격리 사본

이 문서와 `DERIVATIVE.sha256.json`이 이 사본의 기준입니다. 보존한 `ARTIFACTS.sha256.json`, 기존 README·결과·테스트는 upstream 이력이며 새 사본의 성공 증거가 아닙니다. 원본 strict release, B/B2, Culture-D/UTC-D, run01–04, 원본 Build 로그를 수정하지 않았습니다.

Production 변경은 두 파일입니다. `strict_primitives.py`의 `argument_plan`, `exact_cooker_command`, `postflight`만 바꾸고, `internal_smoke.py`의 HELPER_SHA와 관련 설명만 새 helper에 맞춥니다. wrapper `32A95E82FB7C68485FD90086D0F185E583BAC86A9B28287B2E69F1367C4CFBD9`, native validator `985500591447274B367B97A89257D1CA992BA7891890D220608D7B13B383CF98`, policy는 그대로입니다. `cook-fix-e.patch` 하나가 UTC-D에서 변경된 두 production 파일만 담습니다.

## 정규 CLI 교정

- 기존 UbtArgs에 `-Rebuild`를 한 번 추가합니다. `-nocompileeditor`, Game `Rogue10m`, Win64 Development와 기존 로컬 executor 제한은 유지합니다. top-level `-clean`은 사용하지 않습니다.
- UAT `-CookOutputDir=<outputs.cook>/Windows`를 사용합니다. `outputs.cook` 자체는 기존 6개 형제 출력 중 하나로 남습니다.
- AdditionalCookerOptions에 `-FOOBASEDIR="<outputs.cook_user>"`를 넣습니다. 기존 UserDir와 MCP/Python 격리는 유지합니다. 새 로그는 `cook_user/Build/Windows/FileOpenOrder/EditorOpenOrder.log`입니다.

설치 UE5.8 근거: CookCommand.Automation.cs:85–87은 outputdir를 그대로 전달하고, CookOnTheFlyServer.cpp:7285–7326은 단일 CookByTheBook override에 플랫폼 디렉터리를 자동 추가하지 않습니다. Stage CopyBuildToStagingDirectory.Automation.cs:1438–1456은 leaf가 Windows가 아니면 Windows를 추가합니다. 따라서 UAT 인수는 명시적인 Windows leaf를 사용합니다. cooker 전용 `[Platform]` 토큰을 UAT CookOutputDir에 전달하는 방법은 쓰지 않습니다.

ProjectParams.cs:743과 BuildProjectCommand.Automation.cs:90은 Editor agenda를 제외합니다. :130–135,173–174는 UbtArgs를 cooked Game에 전달합니다. TargetDescriptor.cs:132–133과 BuildMode.cs:196–201은 같은 UBT 프로세스에서 `-Rebuild` target을 공식 clean 후 build합니다. 설치 엔진 read-only 경로와 Editor 이름은 이 Game clean 대상에서 제외되지만 공통 SourceFileCache.bin 등 공식 Intermediate 정리는 발생합니다. Editor6 보존은 실제 전후 raw SHA로 판정합니다.

top-level `-clean`은 CookCommand.Automation.cs:156–159,773–787에서 CookOutputDir와 무관하게 프로젝트 Saved/Cooked/Windows를 삭제하므로 금지합니다. `-KeepFileOpenLog=false`는 bool 값으로 해석되지 않아 disable이 아닙니다. Core IPlatformFileOpenLogWrapper.cpp:30–75가 지원하는 FOOBASEDIR를 사용합니다.

## 실제 실행의 필수 증거

새 capture에는 현재 모든 Source/Config/Content/Build/Scripts/Plugins와 이미 존재하는 Game 제품의 정확한 snapshot을 넣습니다. run04가 바꾼 Build/Windows/FileOpenOrder/EditorOpenOrder.log도 현재 바이트 그대로 새 capture에 포함하며 복구·삭제·ignore하지 않습니다. 현재 internal validate는 prior_game_products의 정확 일치를 허용합니다. strict release의 old Game 부재 조건을 이 internal call graph에 잘못 적용하면 안 됩니다.

새 owner 출력 root, 새 6개 출력 디렉터리, 새 lease/run tag/seal 및 동일 live PowerShell 세션의 새 release가 필요합니다. 기존 Prepare E와 invocation은 UTC-D/run04를 고정하므로 그대로 재사용하지 않습니다. Root가 새 사본과 새 runtime/tool pin을 가리키는 별도 준비·실행 문서를 만들고 실제 실행합니다. 이 사본은 owner release를 발급하거나 UE/UAT를 실행하지 않습니다.

postflight는 실제 BCR의 Win64/Game/Development·nocompileeditor·CookOutputDir·project/stage 값과 top-level Clean 부재, 정확히 한 Game UBT 로그의 실제 `-Rebuild`, Link 및 WriteMetadata, 새 Game/receipt 시간과 archive SHA를 검사합니다. 실제 Running cooker 행과 별도 Cook 로그의 LogInit에서 고유 outputdir·FOOBASEDIR·Windows·fileopenlog를 요구합니다. 별도 Cook 로그의 초기화 경로, 격리 로그의 존재·비어 있지 않음·새 mtime/raw pin, 새 Windows ue.projectstore/zenfs.manifest의 경로·시간·Zen identity도 요구합니다. projectstore의 auth 값은 결과에 복사하지 않고 파일 pin만 남깁니다.

**초기화 행은 full buildcookrun.log/stdout에 요구하지 않습니다.** run04 stdout에는 해당 행이 없었지만 별도 보존 `uat-bcr-logs/Cook-2026.11.13-23.52.51.txt:42`에는 실제로 있었고 :498은 LogInit입니다. 새 검사는 별도 Cook-*.txt에 적용합니다. 설치 CommandletUtils.cs:322의 NoLogTimes와 실제 로그 형식을 독립 대조했습니다.

Editor6, protected SaveGames 전 범위, 모든 입력 inventory의 byte/size/attributes/path set 보호는 그대로입니다. 수동 touch·제품 삭제/복사·manifest 이동·Saved/Cooked 재사용·오류 필터·조건 완화는 없습니다. 재링크의 출력 SHA가 이전과 같아도 실제 Link/WriteMetadata 및 새 mtime이 있으면 정상이며 SHA 차이를 새로 요구하지 않습니다.

## 검증과 한계

`evidence/offline-run03/validation.json`은 설치 Engine Python 3.11.8에서 수행한 순수 synthetic Python 검사입니다. 가짜 프로젝트·로그·exe 모양 데이터는 이 폴더의 scratch에만 생성하고 제거합니다. subprocess/GUI API는 테스트에서 호출되면 실패합니다. `helpers()`를 최초 pin 검증한 뒤 synthetic ORIGINAL 경로를 유지하기 위해 같은 helper instance를 main 경로에 재사용합니다. stable_bytes, pin, native, postflight, RED, 승인 함수를 교체하지 않습니다. 이는 bootstrap/UAT 실제 통합 실행 증거가 아닙니다.

첫 synthetic run의 Save fixture 설정 오류 2건은 `evidence/offline-run01/validation.json`에 보존했습니다. fixture만 수정했으며 production guard는 바꾸지 않았습니다. Codex Python 3.12.14는 이 호스트에서 fstat와 Path.stat의 st_ctime_ns 차이로 기존 stable-file guard에 차단되었습니다(`evidence/codex-runtime-guard.json`). 이를 완화하지 않았고 실제 owner가 사용하는 별도 pin의 Engine Python으로 검증했습니다.

재현은 writable 독립 사본에서 설치 Engine Python으로 `-B run_offline_e.py --output evidence/<새이름>`을 사용합니다. 출력 writer는 parent traversal·reparse·기존 경로와 packet 밖 출력을 거부합니다. 동결된 원본 사본에서는 파일을 추가하지 않습니다.

실제 UE/UBT/UAT/Cook/Game/MCP/GUI 실행은 0회입니다. 다음 Root 실행은 아직 미검증입니다. 1800×1800m, map crop/NPC 배치/default map 전환 미수행, UI7·visual·physical play·montage/VFX 시각 확인 UNVERIFIED, release_eligible=false를 유지합니다.
