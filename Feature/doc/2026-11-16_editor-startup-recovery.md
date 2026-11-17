# Unreal Editor 실행 복구와 Landscape 작업 재개

2026-11-16 · Sprint#9-32 · 정상 Editor 실행 후 live 확인 진행

사용자가 에디터가 열리지 않아 점검·계속 진행을 요청했고, Visual Studio로 프로젝트 활성화 중이라고 알려줬다. 사용자 VS를 중단하거나 종료하지 않고 최초 종료 로그와 호스트의 빌드 상태를 확인했다.

## 확인한 원인

- 최신 `Saved/Logs/Rogue10m.log`의 실행 인자는 `-game`이다. 정상 에디터 실행 세션이 아니다.
- 첫 종료 요청은 21:07:50.878 KST `Window closed during preload`, 이어 `UGameEngine::Tick.ViewportClosed` / status0이다.
- ToolsetDefinition/AgentSkill Python 오류는 그 이후에 발생했다. 이 로그의 최초 종료 원인으로 보지 않았다. 11/16 신규 crash 디렉터리도 없었다.
- 21:11:51 프로젝트 파일 생성은8.07초에 성공했다.
- 21:12:18 VS의 Rogue10mEditor Win64 Development 빌드는 `Unable to build while Live Coding is active`로3.04초에 실패했다.
- 호스트 PID21224는 정확한 프로젝트의 `UnrealEditor.exe ...Rogue10m.uproject -game`이며, 창 handle0·종료 준비 로그가 있지만 프로세스가 남아 있었다.

일반 sandbox의 Get-Process 결과는 호스트 전체 프로세스를 보여주지 못했다. 앞선 ‘프로세스 없음’ 관찰을 실제 호스트 전체의 부재로 확대하지 않으며, 이번에는 호스트 직접 읽기로 VS2개·잔여게임·MSBuild를 구분했다. MCP 연결 거절 자체는 당시 실제 결과다.

## 실제 복구

복구 전 게임·UBT 로그와 프로세스 정보를 `evidence/landscape-production-20261116/editor-recovery/`에 보존했다. 실행 경로·프로젝트·-game 인자·창 없음·정상 종료 준비를 재확인한 PID21224만 정리했다. 사용자 VS와 다른 앱은 종료하지 않았다.

다른 Editor/UBT/컴파일러가 없음을 확인한 후 `Scripts/BuildEditor.ps1 -NoHotReload`를 실행했다. 결과는 **Succeeded / exit0 / 169.49초 /110 actions**다. XGE 라이선스 경고는 standalone 실행 안내였고 컴파일·링크는 성공했다. 프로젝트 C++ 소스를 수정하거나 generated 파일을 직접 편집하지 않았다.

`Scripts/OpenLandscapeEditor20261116.ps1`을 추가해 기존 후보 맵을 `-game` 없이 `-NoLiveCoding`으로 실행했다. 새 PID11088이며 프로젝트·플러그인·캐시를 로딩 중이다. 이 시점에는 live editor identity·dirty·층·Landscape 상태는 아직 확인 중이다. 실행기를 성공했다는 이유로 에디터 준비나 지형 완성을 선언하지 않는다.

## 다음 검증

정상 에디터가 준비되면 공식 MCP의 프로젝트·맵·PIE/compile/dirty 상태와 프로젝트 Agent Skills를 먼저 조회한다. 후보 전체 물리 백업 및 보호 입력 SHA를 만든 후 native 외곽 선택·높이/paint/layer/GUID/참조 기준을 확보한다. 과거 거절된 metadata clone QA와 별도 startup 승인 파일은 이번 정상 제작 작업으로 우회하지 않는다.

## 실제 live 확인과 선택 준비 완료

정상 에디터 창을 확인하고 공식 MCP `get_current_level`에서 정확한 후보 경로를 받았다. 프로젝트 Agent Skills를 조회했으며 이번 native Landscape 외곽 편집에 직접 대응하는 별도 프로젝트 스킬은 없었다. 최초 도구 호출의 fully-qualified 이름은dispatcher에서거절됐고, 설치UE소스·이전성공인자확인후짧은도구이름으로교정했다. 실패응답도보존했다.

후보1,423파일·18,272,864바이트를 새 물리 경로 `tmp/LandscapeProduction20261116A/before`에 복사·SHA검증했고, Source/Config/원본3맵·프로젝트Production데이터3,947파일·44,924,472바이트를 보호 해시로 기록했다. 후보의dirty는빈상태다. 지형 영역 로드 후 render/collision256개·proxy16개·edit layer1개·원본lattice계약을확인했다. 유지179,488표본의height/paint/layer기준과액터transform기록을저장했다.

전역 의존 검사에는 미지원 reflected property1,156건이있어`success=false`로남겼다. truncation은없다. 독립리뷰는미지원형식과실제의존을구분했다. 후보한정추가검사는registry초기scan완료,맵+terrain패키지18개에7종reference옵션전체활성,외부referencer0을확인했다. loaded액터1361/컴포넌트1986에서PCG·foliage·spline·water·brush구성은0이다. 별도native검사의PCG owner1은PCGSubsystem이며구현된graph/component가아니다. 전역opaque/native/custom참조인증으로확대하지않는다.

정식LandscapeManage/Select에서외곽211개만선택했고,유지45개는미선택이다. nativehelper는preview에의도적으로success=false를돌려주므로첫wrapper의과도한success조건은교정했으며원문preview·traceback을보존했다. 실제selection=true호출은success=true/selected211·dirty불변이다. 실제UI캡처 `live-before/native-selection.png`를저장했다.

현재지형삭제/저장0이며사용자삭제확인대기다. Computer Use의앱내로컬삭제직전확인규칙을근거로정확한후보·211개·유지치수·후속검증을제시했다. 원본백업·기존WIP는그대로이며,전역검사실패·Undo/Redo·coldreload·실제보행미검증을유지한다.
