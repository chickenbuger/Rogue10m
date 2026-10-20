# 내장 AI Assistant 웹 인증 오류 처리 결과

브랜치: `Sprint#5-5-editor-ai-assistant-error`

## 변경과 효과

사용자가 제공한 Cloudflare iframe 스크립트 차단·`IsAvailable: canceled` 메시지의 실행 주체는 UE 5.8.2의 실험적 Editor 전용 AIAssistant 플러그인이다. 사용자 직접 수정 요청에 따라 `Rogue10m.uproject`의 해당 플러그인만 `Enabled: false`로 변경했다. Epic 내장 AI 창이 로드되지 않아 이 프로젝트에서 해당 웹 연결·JavaScript 오류가 발생하지 않는다. Cloudflare 인증 서비스 자체를 수정한 것은 아니다.

Codex CLI를 사용하는 개인 Studio는 이 내장 플러그인에 의존하지 않는다. 프로젝트의 ModelContextProtocol, MCPClientToolset, AllToolsets, 개별 Editor/UMG/GAS/AI 도구와 GameplayAbilities 설정을 보존했다. 엔진 소스, 웹 보안 제한, ACL, 캐시와 게임 에셋은 수정하지 않았다.

## 실제 검증

- 프로젝트 JSON 파싱 및 필수 플러그인 활성값 확인: `PROJECT_PLUGIN_CONFIG_PASSED`.
- Git 변경 검토: 프로젝트 파일의 변경은 AIAssistant 활성값 한 곳이다. 관련 diff 검사 통과.
- `Scripts/BuildEditor.ps1`: `Result: Succeeded`, 120.41초. 플러그인 구성 변경에 따른 Editor 대상 재컴파일·링크 성공.
- 새 `UnrealEditor.exe` 프로세스에서 실제 Editor 시작·프로젝트 로드·`QUIT_EDITOR` 정상 종료: 종료 코드 0.
- 새 로그의 AIAssistant 마운트·`LogAIAssistant` 메시지: 0건. ModelContextProtocol·AllToolsets·PythonScriptPlugin 마운트 확인.
- 새 로그의 프로젝트 Blueprint/Rogue10m 오류와 Fatal 오류: 0건.

실제 Editor 시작 검증은 숨김 창·`-unattended -nosplash -nosound -nullrhi -ExecCmds="QUIT_EDITOR"`로 수행했다. 소스·게임 기능의 재검증을 새로 수행했다는 의미는 아니다. 시작 시 엔진의 `LogAutomationTest: Error: Condition failed` 15건은 남아 있으며, 변경 전 사용자의 실제 에디터 로그에도 같은 15건이 존재한다. 이를 이번 AI Assistant 오류 해결이나 모든 오류 제거와 혼동하지 않는다.

증거: `Feature/doc/evidence/editor-ai-assistant-20260930/build.log`, `editor-startup.log`. 검증용 프로세스는 정상 종료했으며 커밋·푸시는 수행하지 않았다. 기존 미커밋 게임·Studio 작업은 보존했다.

## 적용과 복원

프로젝트를 다음에 열면 내장 Epic AI 창이 비활성화된 구성이 적용된다. Codex 스튜디오와 외부 MCP는 계속 사용한다. 내장 창이 필요하면 프로젝트 파일의 AIAssistant 활성값만 true로 되돌리고 에디터를 재시작한다. 웹 인증 호환 문제는 다시 발생할 수 있다.

관련: `Feature/architect/2026-09-30_editor-ai-assistant-error.md`, `Docs/UE58MCPSetup.md`.
