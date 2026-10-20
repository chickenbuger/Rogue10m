# 보스 정산 Save v3 통합 보완 계획

기준: main `ddc79e5a8270afb1b40660e969771ed920057816`. 게시 작업: `Sprint#7-2-boss-integration-validation`. 사용자 요청: 남은 작업이 있으면 진행. 검증 시작 때7-1로 만든 브랜치는 동시 감사의7-1과 중복되어 게시 전에7-2로 정리했다. main 병합 전에 시작한 작업이므로 Sprint7은 유지한다.

## 확인한 공백과 범위

보스 종료 301검사는 스킬트리 병합 전 기준이었다. 이후 Save v3에 SkillTree 원장이 추가됐지만 보스 QA는 Growth/Inventory/Combat/Town만 비교했다. 최신 원장과 디스크 저장 버전을 포함해 검증한다.

최종 명중의 ApplyDamage 안에서 몬스터 사망→승리 정산→SaveNow가 먼저 실행되고, 복귀 후에 명중 숙련1XP/자동 해금이 추가된다. 결과창이 저장 성공을 먼저 표시하므로 자동저장/귀환 전의 초기 저장과 메모리에 차이가 생길 수 있다. 최초 종료 판정은 즉시 고정하고, 명중 처리가 완료된 다음 틱에 정산 저장·결과창을 확정하는 경계를 검토한다. 대기 중 귀환 호출도 차단한다.

게임 보상·SaveGame 버전·스킬트리 경제·콘텐츠는 변경하지 않는다. 기존 프로젝트 에셋을 그대로 사용하므로 신규 Fab 반입과 바이너리 편집은 없다. 공유 작업은 시작 시 깨끗했고 분리된 무기 worktree는 건드리지 않는다.

## ULW 패킷

| 패킷 | 목표/영역 | 완료 조건 | 검증 | 되돌리기 경계 |
| --- | --- | --- | --- | --- |
| V1 Architect/Reviewer | 소스 순서와 QA 누락 감사, 원인 재현 | 새 v3 검사에서 초기 저장 누락 확인 또는 동등한 명확한 증거 | Editor 빌드 + Victory | 설계/QA 변경 |
| V2 Implementer/Builder | RunCompletion 저장 확정 순서와 대기 가드, QA SkillTree·디스크 비교 | 마지막 명중 숙련까지 초기 완료 저장에 존재 | BuildEditor.ps1, ValidateRunCompletion.ps1 -Case All | RunCompletion cpp/h·QA cpp |
| V3 Reviewer/Doc Writer | 6프로세스·스냅샷·화면/회귀 기록, main 게시 | 실패0 또는 구체적 미해결 이유 기록, 변경 범위 검토·로그·공개 확인 | CheckGeneratedChanges.ps1, diff --check, fetch/FF/atomic push | 문서·증거·해당 작업 커밋 |

렌더된 독립 QA 프로세스에서 새 접두사 저장만 사용한다. 기존 저장 파일 바이트는 편집하지 않고 실제 플랫폼 저장 API/읽기 잠금으로 확인한다. 기존 시간 단축·AI 정지·보스1HP 마무리 fixture 한계는 유지하며 전체20분 밸런스나 Shipping 인증으로 확대하지 않는다.
