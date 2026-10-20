# 남은 tmp 중복 감사와 후속 정리

작성: 2026-10-20 / `Sprint#7-1-finish-unused-work-audit`
상태: 남은 감사·후보 검토·확정4개 정리·복구 검증 완료. 최종 통합 준비.

## 해결할 미완료 항목

이전294.12MiB 최적화는 main과 원격까지 완료했다. 당시 중단된 tmp 중복 감사를 이번 후속 범위로 잡았다. 기존 도구는 파일을 모두 열거한 다음 보호 여부를 판단했고 참조 검사에서도 worktree·venv·도구 설치를 재탐색했다. 현재 Source/Config/Content나 Fab 자산을 바꾸는 작업이 아니다.

## 보존 결정

- 고유 아트와 이전 승인 기준선, FBX `.fbm` 재수입 경로는 이전 결정대로 유지한다.
- 독립 추가 조사에서 큰 tmp 영상4개와 runtime CSV ZIP은 동일한 추적 보존본이 없거나 문서 참조가 있어 유지했다.
- 2026-09-28 검토에서87개 후보를 실행/동적 경로·basename/glob 입력 등의 이유로 보존했다. 새 문자 검색에서 참조0이어도 이 결정을 자동으로 뒤집지 않는다.
- 등록된 작업 폴더와 진행 중인 무기/저장 통합 작업, 원본 녹화·이미지·재생성 도구 및 엔진 생성/캐시는 정리 대상에서 제외한다.

## 감사·검증 결과

독립 수동 감사에서 tmp34개 경로의 직하·1단계 출력51개, 1,912,473,722바이트를 비교했다. 최초 크기 필터가 배열 길이를 파일 크기로 사용한 오류를 확인해 그 결론을 무효화하고 개별 파일 크기로 재검사했다. 실제 동일 보존 쌍은2개이며 둘 다 패키징 입력이라 유지했다. 고유 ZIP/영상5개도 보존했다. 삭제 전 오류를 바로잡았고 [수정된 제한 조사](evidence/finish-unused-work-audit-20261020/manual-candidates.json)에 정확한51개 경로와 경위를 기록했다.

도구의 최종12개 회귀 검사가 통과했다. 보호 폴더 미진입, backup/baseline/checkpoint·UTF16·nested project 저작 참조 보존, 입력 무변경, suffix 선별, 경로/reparse 거부, 동시 생성된 evidence 덮어쓰기 거부, 신규 timestamp 출력 경로를 확인했다. Reviewer는 초기11개를 별도 실행했고 checkpoint 보강도 검토한다. [최종 회귀 로그](evidence/finish-unused-work-audit-20261020/fixture-final.log).

최종 실제 감사는79.828초에 종료 코드0·issues0으로 완료했다. 후보 탐색2,659파일, 크기 일치 해시1,060파일, 참조2,578파일, 단계별 보호 제외77경로, 중첩 프로젝트6개를 확인했다. 제외한 폴더의 하위 파일 수를 추정하지 않았다. 원래471개 후보는 모두 두 독립 검토의 합집합으로 검토됐고,242개 복구 checkpoint를 탐색 입구 보호로 보강한 뒤 남은229개 중225개와 참조가 있는15개는 유지했다. [최종 감사](evidence/finish-unused-work-audit-20261020/tmp-audit-final.json), [최종 판정](evidence/finish-unused-work-audit-20261020/candidate-review-final.json).

## 추가 정리

두 검토자가 모두 정리에 동의한 아린의 과거 빌드 stdout2개·감사 응답 JSON2개, **4개11,947바이트**만 삭제했다. 동일한 추적 canonical evidence3개가 남아 있고 현행 검증기는 그 경로를 사용한다. [확정 목록](evidence/finish-unused-work-audit-20261020/selection.json)에 원본·보존 경로·SHA-256·크기·판정을 기록했다.

이전 검증된 개별 파일 정리기를 재사용해 삭제 직전 해시·경로·reparse·Git 추적 상태를 확인했다.4개를 실제 복구해 바이트 동일성을 검증한 뒤 다시 정리하고 [최종 영수증](evidence/finish-unused-work-audit-20261020/verify-receipt.json)을 남겼다. [복구 안내](evidence/finish-unused-work-audit-20261020/RESTORE.md).

현재 게임 C++/설정/Unreal 자산을 이번 기능에서 수정하지 않아 전체 Editor 빌드를 반복하지 않았다. 같은 작업 폴더의 별도 보스 저장 통합 변경은 보존하며 이번 커밋에서 제외한다. 추가 삭제량은 작고 핵심 결과는 중단됐던 감사의 완료와 이후 감사의 탐색·기록 보호다. 앞선294.12MiB 정리는 유지되며 새 검증 기록의 디스크 사용량은 삭제량에 포함하지 않는다.

관련: [계획](../architect/2026-10-20_finish-unused-work-audit.md), [이전 정리](2026-10-20_unused-file-optimization.md), [Sprint](../../Docs/SprintChangeLog.md).
