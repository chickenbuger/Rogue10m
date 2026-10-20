# Unreal MCP 플러그인 설정 검토

## 독립 설정 리뷰

- 이전 snapshot과 현재 `.uproject`를 JSON으로 비교한 결과 `LiveCodingToolset`의 Enabled=True, TargetAllowList=[Editor] 한 항목만 추가되었다. 해당 항목을 제외하면 모든 값이 같다.
- 플러그인 추가 시점까지 MCP 프로젝트 기본 설정은 SHA256까지 같았고 `AIAssistant=False`도 유지되었다. 이후 지속성 보완으로 LiveCoding bEnabled=True 기본 섹션만 추가하며 기존 MCP 키를 보존한다.
- 플러그인 자체 EditorOnly/NoRedist 및 두 모듈의 Editor/TargetAllowList 제한이 프로젝트 항목과 일치한다.
- 의존성 ToolsetRegistry는 기존 MCP 의존성으로 활성화되며 사전 빌드 DLL 두 개와 엔진 BuildId 55116800이 일치한다. 이 변경에 코드 빌드·projectfiles 재생성은 필요하지 않다.
- 읽기 전용 의존성 감사에서 기존 MCP+AllToolsets의 재귀 플러그인 descriptor 79개가 설치되어 있고 explicit false 의존성과 충돌하지 않음을 확인했다.

## 실제 에디터 검증

- 새 에디터 PID 14280, `/Engine/Maps/Entry`, 명시적 MCP 시작/포트 override 인자 없음.
- 같은 PID가 127.0.0.1:8001에서 리슨하고 공식 MCP initialize/discovery/read-only call에 응답했다.
- native meta-tool 3개, toolset 53개; LiveCodingToolset과 CompileLiveCoding 스키마 노출.
- ModelContextProtocol/AllToolsets/LiveCodingToolset=True, AIAssistant=False, PIE=False.
- MCP 메모리 설정 auto=True, port=8001, path=/mcp, search=True.
- Live Coding 기능값 False를 확인한 뒤 공식 설정 API로 True 저장 및 재조회. 컴파일 미실행.

## 지속성 보완

독립 Saved 비교에서 API 변경은 LiveCoding의 bEnabled=False 한 줄 제거뿐이고 비관련 변경은 없었다. 엔진 기본값은 False이고 프로젝트 기본값은 없었으므로 메모리 True만으로 재시작 지속성을 확정하지 않았다. 프로젝트 tracked 기본 설정에 LiveCoding bEnabled=True를 추가한 뒤 소유 에디터를 정상 재기동·재조회하여 검증한다. Saved 파일은 직접 편집하지 않는다.

후속 독립 확인: tracked 기본 설정은 빈 줄+LiveCoding 섹션+bEnabled=True 3줄 추가뿐이고 기존 값은 같다. 정상 재시작 14280→42032 뒤 실제 응답에서 LiveCoding=True, MCP auto=True/8001//mcp/search=True, PIE=False, Entry 맵과 PID42032의 loopback 리스너를 확인했다. 지속성 검증 PASS이며 관련 우려는 해소되었다. 미해결 검토 항목은 없다.

## 검증 과정과 경계

첫 로컬 소켓 조회는 샌드박스에서 WinError10013으로 차단되었다. localhost 읽기 호출을 권한 확장하여 실행했다. 에디터 초기화 중 연결 거부 1회를 확인한 뒤 포트 소유 PID 준비를 기다려 성공했다. 같은 변이 호출을 재실행하지 않았고 에디터/프록시를 강제 종료하지 않았다.

라이브 MCP 연결과 설정은 확인했지만 실제 Live Coding compile, 게임 빌드, PIE, 패키징은 실행하지 않았다. 현재 채팅의 native 도구 노출과 공식 서버 직접 호출을 구분한다. 필요한 경우 Codex의 기존 연결을 재연결한다.
