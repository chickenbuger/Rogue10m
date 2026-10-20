# 에디터 내장 AI Assistant 인증 오류 처리

브랜치: `Sprint#5-5-editor-ai-assistant-error`

## 요구와 원인

사용자가 제공한 `LogAIAssistant`의 Cloudflare iframe 스크립트 차단과 `IsAvailable: canceled` 오류를 직접 처리한다. UE 5.8.2의 실험적 Editor 전용 AIAssistant 플러그인이 내장 CEF 브라우저에서 Epic 웹 앱을 여는 경로다. 마지막 환경 설정 메시지는 연결 성공 전체를 보장하지 않는다.

## 수정 범위

프로젝트의 `AIAssistant` 활성값만 false로 바꾼다. 내장 Epic AI 창은 사용할 수 없게 되지만, 별도 Codex CLI 기반 Studio와 ModelContextProtocol·AllToolsets 설정은 보존한다. 엔진 코드, 웹 보안 제한, ACL, 캐시와 게임 소스·바이너리 에셋은 수정하지 않는다. 전체 Cloudflare 인증 서비스를 고쳤다는 의미가 아니다.

## 완료 조건과 검증

- 프로젝트 JSON에서 AIAssistant만 비활성화되고 나머지 설정은 동일하다.
- UE Editor 빌드가 변경된 플러그인 구성으로 성공한다.
- 실제 에디터를 새 프로세스로 시작해 프로젝트와 MCP 플러그인을 로드하고 정상 종료한다. 새 로그에 AIAssistant 마운트·초기화·JavaScript 오류가 없어야 한다.
- 결과와 복원 방법을 Feature/doc, 한국어 DevLog, SprintChangeLog 및 MCP 안내에 기록한다.

복원 경계: 필요하면 프로젝트 파일의 AIAssistant 활성값만 true로 바꾸고 에디터를 재시작한다. 기존 미커밋 작업은 보존하며 커밋·푸시는 하지 않는다.
