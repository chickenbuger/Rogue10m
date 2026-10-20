# Unreal MCP 스킬 독립 검토

- 대상: 설치 전 candidate 20파일 중 변경 Markdown 9개와 before/upstream 비교.
- 범위: 문서·스키마 사용 지침의 정적 검토 및 가상 요청 판단. 에디터·서버·네트워크 호출이나 실제 게임 테스트는 실행하지 않았다.
- 비교 기준: Epic 3.1.1, `a6aa73ada02a9fb1f5415bec491f9942c519b297`.

## 역할별 확인

- 기존 설치 감사: Codex TOML·8001 의존성·CLI/timeout/log runner·불명 변이 재시도 금지·MIT 고지 보존 확인.
- 저작 지침 비교: Epic의 async 완료/오류, Python 타입/등록, 테스트 재검색, Agent Skill 정의/저장 계약 보완.
- 로컬 엔진 읽기: Codex config writer, LiveCodingToolset, AgentSkill 구현을 확인했다. 이 설치 엔진에는 Extras/Proxy가 없어 프록시 지침을 조건부로 한정했다.

## 독립 행동 검토 6사례

| 가상 상황 | 확인한 판단 |
| --- | --- |
| proxy status=true인데 배우 목록 실패 | status/캐시와 실제 도달성을 구분하고 기존 연결·catalog 복구 후 읽기 호출로 확인 |
| 8001 응답, ModelContextProtocol만 활성화 | 검증된 endpoint/기존 config 보존, AllToolsets 또는 의도한 provider 보완 |
| 같은 Blueprint 변수 추가와 컴파일의 병렬 요청 | 결과 의존 호출은 순차, 지원되는 독립 읽기만 안전한 중첩 허용 |
| 여러 에셋 변이 timeout 후 재실행 요청 | 재생 전 읽기 검사, 부분 적용 확인; batch의 transaction/rollback 보장 없음 |
| Python transient / Blueprint dirty Agent Skill | Python 원본 수정·reload; Blueprint는 별도 save와 정확한 class path 재조회 |
| runner exit0/JSON success, CompileStillActive | 테스트 report/실행·실패 수 확인 전 통과 금지; 컴파일 완료 전 변이 보류 |

## 발견사항과 수정

독립 검토에서 기존 timeout 문구가 공유 editor/proxy 종료까지 허용하는 것으로 읽힐 수 있다는 잔여 모호성 1건을 발견했다. SKILL.md에서 종료 대상을 현재 headless 작업이 직접 시작한 process tree로 제한하고, live MCP timeout에는 공유 editor/proxy 종료 권한이 없으며 읽기 확인과 operations 복구 절차를 따르도록 수정했다. 수정 후 구조·참조·예제 구문 검사를 다시 통과했다. 독립 검토자가 수정된 문장을 다시 읽고 모호성 해소 및 잔여 발견사항 없음을 확인했다.

새로 생긴 확정 API 오류나 중요한 안전장치 퇴행은 발견하지 못했다. 이 결과는 지침 판단 검토이며 실제 MCP/PIE 동작이나 품질 향상을 증명하지 않는다.
