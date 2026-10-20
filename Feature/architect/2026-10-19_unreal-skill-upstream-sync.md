# Unreal MCP 스킬 원본 동기화 계획

## 범위와 기존 상태

- 사용자 요청: 설치된 `C:/Users/PC/.codex/skills/unreal-engine-mcp-codex`와 Epic 최신 원본을 비교하고 누락된 절차를 Codex에 맞게 보완한다.
- 작업 브랜치: `Sprint#5-34-unreal-skill-upstream-sync`. 기존 미커밋 변경을 보존한다.
- 비교 기준: Epic `unreal-engine-skills-for-claude-code-plugin` main, `a6aa73ada02a9fb1f5415bec491f9942c519b297` (2026-09-17 UTC), 확인일 2026-10-19 KST.
- 원본 범위: `unreal-mcp`, `create-toolset`, `unreal-skill`, setup/operations, README, hook/manifest와 MIT 고지.
- 수정 대상은 개인 설치 스킬의 Markdown과 프로젝트 기록이다. 게임 C++/설정/에셋, 실제 MCP 연결 설정, runner 구현은 변경하지 않는다.

## Ultrawork Packets

| Packet | 목표 | 수정 위치 | 완료 조건 | 검증 | 되돌릴 경계 |
| --- | --- | --- | --- | --- | --- |
| S1 Architect | 최신 원본 고정과 누락 비교 | 임시 원문/해시, 이 계획 | 커밋과 파일별 보완 근거 확보 | 원문·설치본 대조 및 독립 감사 | 읽기 자료/계획만 제거 가능 |
| S2 Implementer | Codex에 맞춘 절차 보완 | 설치 스킬의 SKILL.md와 references | 도구 제공 플러그인, 프록시 복구, dispatch/동시성 및 저작 지침 반영 | quick_validate, 참조·차이 검토 | 설치 전 원본 20파일과 해시 보존 |
| S3 Builder / Reviewer | 정적·행동 검증 후 설치 | 임시 candidate → 사용자 설치본 | 구조 검사와 독립 시나리오 검토, 설치본 해시 일치 | validator, 링크 검사, 시나리오 검토, CheckGeneratedChanges | 변경 파일만 원본 백업으로 복원 |
| S4 Doc Writer | 재현 가능한 동기화 기록 | Feature/doc, SprintChangeLog, DevLog | 수정 목록·검증·한계·Notion 후보 기록 | 문서 링크/공백 및 기존 변경 보존 확인 | 이 작업 추가 문단/파일만 복원 |

## 적용 원칙

- Codex TOML과 기존 사용자 포트/의존성 메타데이터, 명령행 실행 라우터, runner 스크립트, MIT 고지를 보존한다.
- 최신 원본의 독립 호출 병렬 허용을 반영하되, 같은 에디터 상태나 에셋을 바꾸는 호출과 의존 호출은 순차 실행한다.
- 원본 Claude 전용 `/plugin`, `.mcp.json` 설치 절차는 Codex의 실제 지원 설정으로 번역하고 미확인 프록시 옵션은 만들지 않는다.
- 프로젝트 지침의 Saved 직접 편집 금지 및 커밋 승인 규칙을 우선한다.
- 설치 디렉터리는 작업공간 밖이므로 candidate 검증 후 해당 파일 복사에만 권한 확장을 사용한다. 관련 없는 설정을 바꾸지 않는다.

## 검증과 한계

- 스킬 본문/참조만 변경하므로 UE 빌드·PIE·서버 재시작을 실행하지 않는다.
- 정적 검증은 실제 MCP 연결·빌드·게임 품질 향상을 증명하지 않는다. 연결 상태는 실제 읽기 전용 Unreal 호출로만 판단하도록 지침을 보완한다.
- 요청 없는 커밋·푸시는 실행하지 않는다.
