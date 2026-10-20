# UE 5.8 MCP Setup

## 적용 상태

- 프로젝트 엔진 연결을 UE 5.8로 변경했다.
- 엔진 경로 기준: `D:\Program Files\UE_5.8`
- `Rogue10m.uproject`에서 다음 실험 플러그인을 활성화했다.
  - `ModelContextProtocol`
  - `MCPClientToolset`
  - `AllToolsets` (도메인 도구 제공)
  - `LiveCodingToolset` (Editor 대상 한정, 2026-10-19 보완)

## 플러그인 위치

- Unreal MCP: `D:\Program Files\UE_5.8\Engine\Plugins\Experimental\ModelContextProtocol\ModelContextProtocol.uplugin`
- MCP Client Toolset: `D:\Program Files\UE_5.8\Engine\Plugins\Experimental\Toolsets\MCPClientToolset\MCPClientToolset.uplugin`
- Live Coding Toolset: `D:\Program Files\UE_5.8\Engine\Plugins\Experimental\Toolsets\LiveCodingToolset\LiveCodingToolset.uplugin`

## 기본 서버 설정 메모

### 내장 AI Assistant와 외부 MCP 구분 (2026-09-30)

`Rogue10m.uproject`의 `AIAssistant`는 현재 비활성화되어 있다. Epic 내장 AI 창에서 Cloudflare 인증 스크립트 차단과 `IsAvailable: canceled`가 반복되어 해당 에디터 웹 연결을 사용하지 않도록 설정했다. 이 변경은 인증 서비스 자체의 수정이 아니다.

`ModelContextProtocol`, `MCPClientToolset`, `AllToolsets`와 개별 에디터·UMG·GAS 도구는 활성 상태다.

내장 Epic AI 창이 필요하면 프로젝트 파일의 `AIAssistant`에서 `Enabled`를 `true`로 되돌리고 에디터를 재시작한다. 웹 인증 호환 문제는 다시 발생할 수 있다.

### 서버 설정

`ModelContextProtocol` 플러그인의 서버 설정은 `EditorPerProjectUserSettings` 성격이다.
이 프로젝트는 `Config/DefaultEditorPerProjectUserSettings.ini`에 서버 자동 시작과 포트의 프로젝트 기본값을 기록한다. 개인 `Saved/Config/`의 `EditorPerProjectUserSettings.ini` 값이 이를 덮어쓸 수 있으므로 실제 적용값도 확인한다.

- 기본 URL 경로: `/mcp`
- 프로젝트 기본 포트: `8001`
- 프로젝트 자동 시작 기본값: 켜짐
- 도구 검색 기본값: 켜짐

에디터 환경 설정의 Model Context Protocol 섹션에서 서버 설정을 확인한다. 에디터 설정 API에서 `bAutoStartServer=True`, `ServerPortNumber=8001`을 확인했다. 2026-10-05에 명시적 MCP 시작 명령이나 포트 지정 인자 없이 에디터를 재시작해 8001 자동 시작과 공식 MCP 연결을 검증했다. 근거는 `Feature/doc/evidence/start-village-20261005/editor-restart.log`와 `restart-current-level.json`이다.
외부 MCP 클라이언트를 연결할 때는 로컬 개발 환경에서만 포트를 열고, 필요한 경우 승인된 도구만 노출한다.

### 직접 설정·실제 연결 확인 (2026-10-19)

현재 설치 엔진의 `AllToolsets`는 `LiveCodingToolset`을 포함하지 않으므로 프로젝트에 Editor 대상 한정 항목을 별도로 추가했다. 필요한 DLL과 엔진 BuildId가 일치해 코드 재빌드 없이 에디터를 시작했다. `ModelContextProtocol`, `AllToolsets`, `LiveCodingToolset`이 실제 에디터에서 활성화되었고 `AIAssistant`는 기존 비활성을 유지했다.

명시적인 MCP 시작/포트 인자 없이 프로젝트를 `/Engine/Maps/Entry`에서 실행해 `127.0.0.1:8001/mcp` 자동 시작을 확인했다. 포트 소유자는 해당 에디터 프로세스이며 loopback 주소만 리슨했다. 도구 검색은 meta-tool 3개와 도구 묶음 53개를 반환했고 `CompileLiveCoding` 스키마도 노출되었다. 실제 설정 API 조회에서 auto-start=True, port=8001, path=/mcp, search=True, PIE=False를 확인했다.

Editor Preferences > General > Live Coding의 기능 활성값은 꺼져 있어 공식 `ConfigSettingsToolset`의 설정 API로 `bEnabled=True`를 저장하고 재조회했다. `Saved/` 파일을 직접 편집하지 않았다. 실제 Live Coding 컴파일·빌드·PIE는 실행하지 않았다.

설정 API가 기존 Saved의 False override만 제거하는 것을 확인해, 프로젝트 `Config/DefaultEditorPerProjectUserSettings.ini`에 `[/Script/LiveCoding.LiveCodingSettings]`의 `bEnabled=True`도 명시했다. 이 작업이 시작한 에디터만 정상 종료·재기동했고, 시작/포트 override 인자 없이 Live Coding=True 및 기존 MCP 네 가지 설정이 유지됨을 실제 API로 확인했다.

현재 Codex 채팅에는 native Unreal meta-tool이 노출되지 않아 기존 `Scripts/InvokeUnrealMCP.py`로 같은 공식 HTTP 서버를 initialize → discovery/schema → read-only call 순서로 검증했다. Codex의 기본 도구 목록이 갱신되지 않은 경우 기존 MCP 연결을 재연결하면 된다. 별도 서버나 프록시를 추가하지 않았다.

검증 자료: [설정 결과](../Feature/doc/2026-10-19_unreal-plugin-setup.md), [재시작 MCP 설정](../Feature/doc/evidence/unreal-plugin-setup-20261019/restart-mcp-values.json), [재시작 Live Coding](../Feature/doc/evidence/unreal-plugin-setup-20261019/restart-livecoding-values.json).

## Rogue10m에서 우선 활용할 작업

- C++ 클래스와 Widget Blueprint 바인딩 상태 점검
- Data Asset 참조 누락과 미사용 에셋 탐색
- 공격/아이템/몬스터 데이터 초안 생성 보조
- 빌드 로그와 PIE 오류 분석
- DevLog와 Feature 문서 요약 자동화
- 에셋 생성 요청 시 이미지 `png`, 음성 `wav` 기본 형식 확인

## 검증 기록

- 프로젝트 파일 재생성: 성공
- UE 5.8 `Rogue10mEditor Win64 Development` 빌드: 성공
- `CheckGeneratedChanges.ps1`: 통과
- 참고 경고: Android SDK 설정이 없어 프로젝트 파일 생성 중 Android 플랫폼이 스킵되었다.
- 참고 경고: Variant 템플릿 삭제분에 대한 바이너리 에셋 경고가 표시되었으나, 이전 최적화 작업의 의도된 삭제 항목이다.
