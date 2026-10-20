# Unreal MCP 플러그인 직접 설정 결과

## 완료 결과

Rogue10m의 실제 플러그인과 에디터 환경설정을 보완하고, 정상 재시작 후 공식 MCP 읽기 호출로 연결과 설정 지속성을 확인했다. 작업 브랜치는 `Sprint#5-35-unreal-plugin-setup`이다.

- `Rogue10m.uproject`: 기존 플러그인을 보존하고 `LiveCodingToolset`을 Editor 대상 한정으로 활성화했다.
- `Config/DefaultEditorPerProjectUserSettings.ini`: LiveCoding bEnabled=True 기본값만 추가했다. 기존 MCP 자동 시작·8001·/mcp·도구 검색 설정은 그대로다.
- Editor Preferences의 Live Coding 활성값을 공식 설정 API로 True 저장하고 재조회했다. Saved 파일은 직접 편집하지 않았다.
- 기존 `AIAssistant=False`와 Codex 서버 URL을 유지했다. 다른 플러그인이나 별도 프록시는 설치하지 않았다.

## 패킷과 검증

| Packet | 결과 | 검증 |
| --- | --- | --- |
| P1 Architect / Implementer | 기존 MCP/AllToolsets 확인, 누락 LiveCoding provider와 지속 기본값 보완 | JSON snapshot 대조, 기존 MCP 키 보존, DLL/BuildId/의존성 검토 |
| P2 Builder | 소유 에디터 시작 및 정상 재기동, 자동 MCP 연결 | initialize, tools/list, list_toolsets, describe_toolset, 실제 call_tool 읽기 |
| P3 Reviewer / Doc Writer | 독립 설정·저장 지속성 검토, 가이드/로그 | 재기동 응답 대조·문서/생성 경로 검사 |

최초 PID14280 및 재시작 PID42032 모두 `/Engine/Maps/Entry`에서 PIE 없이 실행했다. `-ModelContextProtocolStartServer`나 포트 override는 지정하지 않았다. 최종 PID42032가 `127.0.0.1:8001` 리스너를 소유했다.

실제 검증 값:

| 항목 | 확인 결과 |
| --- | --- |
| MCP/AllToolsets/LiveCodingToolset 플러그인 | 활성 True |
| 기존 내장 AIAssistant | 비활성 False 유지 |
| MCP auto-start / port / path / tool-search | True / 8001 / /mcp / True |
| native meta-tools / toolsets | 3 / 53, CompileLiveCoding 스키마 포함 |
| Live Coding 기능 활성 | False→True, 정상 재시작 후에도 True |
| PIE / 현재 레벨 | False / /Engine/Maps/Entry |

현재 채팅에는 native Unreal 도구가 노출되지 않아 기존 `Scripts/InvokeUnrealMCP.py`로 같은 공식 HTTP 서버와 프로토콜을 이용했다. 이는 서버 연결 성공과 구분해 기록한다. Codex 기본 도구 목록 갱신이 필요하면 기존 MCP 연결을 재연결하면 되며 새 서버 등록은 필요 없다.

## 독립 검토와 지속성 보완

기존 `AllToolsets`에 LiveCoding provider가 없음을 설치 descriptor에서 확인했다. 사전 빌드 DLL 두 개와 엔진 BuildId 55116800이 일치했고, 프로젝트·플러그인·모듈이 모두 Editor 대상 제한을 갖는다. 이번 설정만을 위해 C++ 빌드나 프로젝트 파일 재생성은 필요하지 않았다.

API 저장 성공만으로 지속성을 확정하지 않았다. Saved 비교에서 False override 삭제만 남고 엔진 기본값이 False인 것을 독립 검토자가 발견했다. 프로젝트의 tracked 기본값을 추가한 뒤 소유 에디터를 정상 재시작하고 True를 다시 읽어 우려를 해소했다. 기존 ini 키와 비관련 Saved 설정은 보존했고 미해결 검토 항목은 없다.

## 유지한 범위와 한계

게임 C++·모듈·Build.cs·생산 에셋을 직접 변경하지 않았고 PIE·게임 저장·실제 Live Coding compile·빌드·패키징은 실행하지 않았다. 이전 미커밋 작업을 유지했고 커밋·푸시는 하지 않았다. 현재 에디터는 MCP 연결이 가능한 상태로 실행 중이다.

첫 연결 시 샌드박스 소켓 차단은 localhost 읽기 권한 확장으로 처리했다. 기동 중 연결 거부는 포트 준비를 기다려 해결했으며, 설정 변경 호출의 불명 재시도나 강제 종료는 하지 않았다. 시작 로그에 나온 일반 엔진 경고를 게임 전체 QA 통과로 해석하지 않는다.

복구가 필요하면 이 작업에서 추가한 LiveCodingToolset 항목과 프로젝트 LiveCoding 기본 섹션을 제거하고, 에디터 설정 API로 기능 활성값을 이전 False로 변경한다. 다른 미커밋 변경을 포함한 프로젝트 파일 전체를 덮어쓰지 않는다. 원본 snapshot은 Git 제외 `tmp/unreal-plugin-setup-20261019/`에 보존했다.

## 관련 자료

- [계획](../architect/2026-10-19_unreal-plugin-setup.md)
- [설정 가이드](../../Docs/UE58MCPSetup.md)
- [독립 검토](evidence/unreal-plugin-setup-20261019/review.md)
- [도구 목록](evidence/unreal-plugin-setup-20261019/toolsets.json)
- [MCP 재시작 설정](evidence/unreal-plugin-setup-20261019/restart-mcp-values.json)
- [Live Coding 재시작 설정](evidence/unreal-plugin-setup-20261019/restart-livecoding-values.json)
- [정상 재시작](evidence/unreal-plugin-setup-20261019/editor-restart.json)
- [최종 연결](evidence/unreal-plugin-setup-20261019/connection-final.json)
- [일일 로그](../../DevLog/20261019.txt)
