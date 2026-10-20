# Unreal MCP 플러그인 설정 계획

## 요청과 확인 결과

사용자 요청에 따라 Rogue10m의 Unreal 플러그인을 직접 설정하고 실제 연결을 검증한다. 작업 브랜치는 `Sprint#5-35-unreal-plugin-setup`이다.

- `ModelContextProtocol`, `AllToolsets`와 기존 editor/UMG/GAS/AI 도구는 이미 활성화되어 있다.
- 프로젝트 기본 설정은 auto-start=True, port=8001, path=/mcp, tool-search=True이며 기존 Codex URL도 일치한다.
- 현재 Saved 사용자 설정에는 MCP override 섹션이 없고 실행 중인 에디터도 확인되지 않았다. Saved 파일은 직접 수정하지 않는다.
- 설치 엔진의 `AllToolsets`는 `LiveCodingToolset`을 의존하지 않는다. 해당 플러그인은 존재하고 사전 빌드 DLL이 있지만 기본 비활성이므로 Editor 한정으로 명시 활성화한다.
- 인증 문제로 의도적으로 꺼 둔 내장 `AIAssistant`와 다른 플러그인·포트·Codex 설정은 유지한다.

## 작업 패킷

| Packet | 목표 | 수정 위치 | 완료 조건 | 검증 | 복구 경계 |
| --- | --- | --- | --- | --- | --- |
| P1 Architect / Implementer | 실제 누락된 live compile provider 보완 | Rogue10m.uproject | LiveCodingToolset Editor 한정 활성, 기존 항목 유지 | JSON·원본 diff·descriptor/DLL 확인 | 추가한 plugin 항목만 제거 |
| P2 Builder | 프로젝트 실행과 자동 연결 확인 | 새 에디터 프로세스, 증거 폴더 | 명시적 MCP 시작 플래그 없이 8001 도구 discovery 및 읽기 호출 성공 | 공식 MCP initialize/list/describe/call, 실제 settings/plugin 조회 | 이 작업이 시작한 프로세스만 정상 종료 가능 |
| P3 Reviewer / Doc Writer | 위험 검토와 결과 기록 | Feature/doc, Docs/UE58MCPSetup.md, SprintChangeLog, DevLog | 검증·한계·한국어 로그/Notion 후보 | 독립 설정 검토·생성 경로/문서 검사 | 이 작업의 추가 문서/문단만 복원 |

## 실행 원칙과 한계

- 기존 에디터가 발견되면 새 프로세스를 중복 실행하지 않고 프로젝트/포트 소유권을 먼저 확인한다.
- 플러그인 검증은 게임 에셋 변경 없이 `/Engine/Maps/Entry`에서 수행하며 PIE·실제 Live Coding compile은 실행하지 않는다.
- 네이티브 Unreal 도구가 이 Codex 세션에 노출되지 않으면 기존 `Scripts/InvokeUnrealMCP.py`로 같은 공식 서버 프로토콜을 사용한다.
- 코드/Build.cs 변경은 없고 DLL이 이미 있어 불필요한 전체 빌드를 피한다. 시작 시 모듈 문제를 확인하면 관련 빌드만 수행한다.
- 게임 소스/생산 에셋/저장 파일 및 기존 미커밋 변경을 보존한다. 커밋·푸시는 실행하지 않는다.

## 실제 설정 확인 후 보완

에디터의 실제 `Live Coding` 환경설정에서 `bEnabled=False`를 확인했다. 플러그인 활성화만으로 compile 도구가 동작 가능한 설정이 되지 않으므로, `ConfigSettingsToolset.SetSectionProperties`로 이 활성값만 True로 바꾸고 저장/재조회한다. 사용자 설정 원본을 임시 폴더에 백업하며 Saved 파일 직접 편집은 하지 않는다. 컴파일을 트리거하지 않는다.

독립 저장 검토에서 API 성공과 메모리 True에도 Saved에는 False override 제거만 남고 엔진 기본값은 False임을 확인했다. 지속성을 분명히 하기 위해 프로젝트의 tracked `Config/DefaultEditorPerProjectUserSettings.ini`에 LiveCoding bEnabled=True를 추가하고, 이 작업이 시작한 에디터만 정상 재기동한 뒤 실제 API로 재조회한다. 기존 MCP 키는 그대로 보존한다.
