# Unreal MCP 설치 스킬 최신 원본 보완 결과

## 결과

설치된 `C:/Users/PC/.codex/skills/unreal-engine-mcp-codex`를 Epic 최신 원본과 비교해 Markdown 9개를 보완하고 실제 설치 경로에 반영했다. 비교 기준은 2026-10-19 KST에 조회한 main의 `a6aa73ada02a9fb1f5415bec491f9942c519b297` (3.1.1, 2026-09-17 UTC)이다.

작업 브랜치는 `Sprint#5-34-unreal-skill-upstream-sync`이며 기존 미커밋 변경은 보존했다. 게임 C++·설정·에셋, 실제 MCP config와 포트, runner 구현은 변경하지 않았다. 커밋·푸시는 하지 않았다.

## 완료한 Ultrawork Packets

| Packet | 결과 | 검증 |
| --- | --- | --- |
| S1 Architect | 최신 commit 고정, 원문 10파일과 설치본 20파일 대조 | SHA256·원본 manifest·독립 감사 |
| S2 Implementer | 본문과 참조 8개의 누락·오래된 절차 보완 | 공식 원문/로컬 엔진 소스 대조 |
| S3 Builder / Reviewer | candidate 검사·6사례 독립 행동 검토·설치 | 구조/참조/구문 PASS, 20파일 해시 확인, 설치본 validator PASS |
| S4 Doc Writer | 계획·결과·증거·Sprint·한국어 일일 로그 | 문서 링크/공백·기존 변경 상태 대조 |

## 파일별 변경

아래 경로는 모두 개인 설치 스킬 폴더 기준이다.

| 파일 | 보완 내용 |
| --- | --- |
| `SKILL.md` | direct/proxy 탐색, native dispatch 제한, 독립 호출만 병렬 허용, 반복 project skill 발견, endpoint 보존, compiler 결과 확인, 소유 process tree만 종료 |
| `references/setup-codex.md` | AllToolsets 또는 선택 provider 필수, 실제 설치 경로 기준 실행, Codex config 생성 위치·기존 파일 보존, 선택형 proxy 지원 경계, Saved 직접 편집 금지 우선 |
| `references/operations.md` | proxy status/캐시와 실제 연결 구분, list_changed 후 catalog 갱신, 공유 proxy 보존, process/port/project 확인, probe/runner 성공 해석 제한 |
| `references/live-editor-workflow.md` | 의존/동일 상태 호출 순차, compile 상태 해석, batch의 비트랜잭션 성격, 승인된 checkpoint와 불명 변이 재시도 방지 |
| `references/security.md` | 기존 변경을 보존하는 복구점과 명시적 커밋 승인 |
| `references/toolset-authoring.md` | async SetValue/SetError, 동기 오류 반환, converter 사용 조건, Python 필수 타입·등록, 테스트 helper/재검색 |
| `references/unreal-agent-skills.md` | docstring/instructions 정의, 정확한 class path, Python transient 수정 경계, Create/Update 입력과 Blueprint 명시 저장 |
| `references/version-matrix.md` | 실제 테스트 이력과 기대 호환성 구분, provider/proxy/schema 검사와 live read 증거 |
| `references/upstream.md` | 원본 commit/version/date·파일 대응표·각색/유지 범위 |

8개 runner/probe/validator 스크립트, `agents/openai.yaml`의 자동 호출 정책·8001 endpoint, `triggers.md`, MIT 고지는 원본 바이트를 유지했다. MIT 고지는 최신 Epic 원본과도 일치한다. Claude 전용 hook이나 plugin 설치 명령은 가져오지 않았다.

## 검증

- skill-creator `quick_validate.py`: candidate와 실제 설치본 모두 통과.
- 설치 스킬 전용 validator 함수: 통과. 원본 저장소용 plugin/package 전체 검사는 설치 폴더 구조와 달라 실행 대상으로 삼지 않았다.
- 로컬 참조 15개, JSON/TOML/Python 예제 3개, 기존 Python script AST, YAML 구문, 파일 집합·비문서 불변성 검사 통과.
- 독립 검토 6사례: 프록시 복구, provider 누락, 호출 동시성, 불명 timeout, Agent Skill 수정/저장, 테스트·컴파일 완료 판정. 공유 프로세스 종료 문구 모호성 1건 수정 후 재확인하여 잔여 발견사항 없음.
- 설치 직전 원본 해시를 재검사해 동시 변경을 방지했고, 설치 후 20파일 SHA256을 기대값과 대조했다. 실제 파일 수도 20개이다.
- `Scripts/CheckGeneratedChanges.ps1` 통과. 기존 게임 에셋 변경 경고는 이 작업 전 미커밋 상태에 포함되어 있으며 이 작업에서 수정하지 않았다.

## 적용·복구와 한계

설치본은 실제 개인 스킬 경로에 반영됐다. config/메타데이터 변경이 없어 이 작업에서 Codex나 Unreal을 재시작하지 않았다. 이후 스킬 본문을 읽으면 보완한 지침을 사용한다.

원본 20파일 백업과 candidate는 Git 제외 로컬 폴더 `D:/Project/Rogue10m/tmp/unreal-skill-sync-20261019/`에 보존했다. 복구 시 현재 설치본의 동시 변경 여부를 먼저 확인하고, 이번에 바꾼 9개만 `before/`에서 복원한다.

로컬 UE 5.8에는 선택형 `Extras/Proxy`가 없다. 따라서 proxy 실행 인자·라이브 연결·PIE·게임 빌드·성능·실제 품질 향상은 검증하지 않았다. 이번 결과는 스킬 지침 동기화와 정적/행동 검토이며 실제 엔진 동작을 보증하지 않는다.

## 관련 자료

- [계획](../architect/2026-10-19_unreal-skill-upstream-sync.md)
- [파일별 diff](evidence/unreal-skill-sync-20261019/skill-update.diff)
- [원본 manifest](evidence/unreal-skill-sync-20261019/upstream-manifest.json)
- [구조·참조 검사](evidence/unreal-skill-sync-20261019/validation.json)
- [독립 검토](evidence/unreal-skill-sync-20261019/review.md)
- [설치 해시 확인](evidence/unreal-skill-sync-20261019/installation.json)
- [설치본 구조 검사](evidence/unreal-skill-sync-20261019/post-install-validation.json)
- [Epic 비교 revision](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin/tree/a6aa73ada02a9fb1f5415bec491f9942c519b297)
- [한국어 일일 로그](../../DevLog/20261019.txt)
