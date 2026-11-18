## 경로·시간 예산 독립 도구 통합 (2026-11-10)

- 목표: 확정된 검증 도구를 프로젝트에서 바로 실행할 수 있게 하고 최신 보스룸 도착 약15분 기준을 유지한다.
- 변경: 새 `Tools/RouteBudget/` 디렉터리에 CLI·47개 회귀·실제 입력/fixture 분리·실행별 증거·새 정적 검사·출처 기록 추가.
- 검증: 회귀47개·정적 검사199개 통과,90개 방향별 경로·미방문32개 조합, 보고서 바이트 재현, 원본 실행 전후 해시 일치, 앞선 실행 증거8개 보존.
- 상태: 프로젝트 도구 통합 완료. 실제 게임 경로 구현·지형·동작·풀런 검증은 미실시. 기존 파일 무변경, 에디터/빌드/커밋/푸시 없음.
- 관련 문서: `Tools/RouteBudget/README.md`, `ARCHITECTURE.md`, `VALIDATION.md`, `DEVLOG_CANDIDATE.txt`; 기존 `Feature/architect/2026-11-10_five-village-route-plan.md`, `Feature/doc/evidence/five-village-route-spec-20261110/route-plan.json`.
- 불일치: 기존 처치15분 문서는 그대로 보존하며 입구15분 목표와 구분한다.

공유 Docs/SprintChangeLog.md 수정 금지 범위에 따라 이 폴더에만 후보를 기록했다.
