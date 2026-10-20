# 과거 제작 파일과 중복 산출물 최적화 결과

작성: 2026-10-20 / 브랜치: `Sprint#6-2-unused-file-optimization`
상태: 파일 정리·무결성·복구·독립 검토 완료. 최종 Git 통합은 아래에 기록한다.

## 결과

검토한7개 원본345,395,090바이트를 정리하고36,988,290바이트의 압축본3개를 남겼다. **순감소308,406,800바이트(294.12MiB, 약308MB)**다. 새 도구·기록의 소량 크기를 제외한 대상 파일 기준 수치다.

| 작업 | 파일 | 원본 크기 | 보존 방법 |
| --- | ---: | ---: | --- |
| 중복 Blender 백업 정리 | 4 | 107,360,840바이트 | 별도 과거 제작 후보에 동일 크기·SHA-256 사본 존재 |
| 120Hz 검증 JSON 무손실 압축 | 3 | 238,034,250바이트 | 원본 bytes 그대로 gzip, 압축 해제 전량 해시 대조 |

과거 실패·성공 로그와 비교 이미지, 최종 아린 편집/애니메이션 원본·FBX/GLB·입력 이미지를 보존했다. `.blend1` 중 승인 기준선과 고유 버전은 남겼다. `.fbm` 텍스처는 중복이어도 FBX 재수입 경로라 보존했다. 현재 Source/Scripts/Config의 동적·도구 연결을 조사했으며 사용하지 않는다고 입증된 소스는 없었다. Content/Fab 자산과 엔진 생성/캐시 파일, 진행 중인 worktree는 변경하지 않았다.

## 도구 호환과 검증

`EnhanceFighterArmSwing.py`는 기존 일반 JSON을 먼저 읽고 없으면 `.json.gz`를 읽는다. 압축된 baseline도 존재 검사에 포함해 과거 기준선 재생성을 차단한다. 실패를 숨기거나 새 baseline으로 넘어가지 않는다. 실제 Unreal 에셋을 다시 굽거나 과거 검증 결과를 재생성하지 않았다.

- 독립 조사와 Reviewer가 원본7개·보존 사본4개의 크기·SHA-256을 재계산해 모두 확인했다.
- apply/verify에서 압축3개를 전량 해제하고 원본 크기·SHA-256 동일, 삭제 집합과 보존 집합 교집합0, 범위 이탈·reparse0을 확인했다.
- **원본7개를 실제 복구하고 모든 해시를 재검증한 뒤 다시 정리했다.**
- 실제 reader/main을 AST로 추출해 오프라인 검사7개를 통과했다. 일반/압축 UTF8·일반 우선·누락/손상 오류·압축 baseline 덮어쓰기 차단을 확인했다. 실제 gzip3개도 각10클립으로 파싱했다.
- C++/Config/Content 변경이 없어 전체 Editor 빌드와 PIE를 반복하지 않았다. 최종 생성 경로·공백·Python 구문·문서·스테이징 검사는 최종 통합 기록에 덧붙인다.

## 작업 패킷

U1 참조/중복 조사, U2 확정 파일 정리/검증기 호환, U3 무결성/실제 복구/독립 리뷰, U4 기록과 Git 통합으로 진행했다. 전체 tmp 감사는 보존 worktree·도구 탐색 비용 때문에 종료했고 후보 목록이나 삭제로 사용하지 않았다. 이번7개 밖의 미사용 판정은 하지 않는다.

## 복구와 한계

[확정 목록](evidence/unused-file-optimization-20261020/selection.json), [최종 확인](evidence/unused-file-optimization-20261020/verify-receipt.json), [실제 복구](evidence/unused-file-optimization-20261020/restore-receipt.json), [압축본 읽기](evidence/unused-file-optimization-20261020/reader-after.json), [독립 검토](evidence/unused-file-optimization-20261020/review.md), [복구 명령](evidence/unused-file-optimization-20261020/RESTORE.md).

현재 작업 폴더의 디스크 부담을 줄이는 변경이다. 게임 FPS/런타임 메모리/패키지 크기 개선을 측정하거나 주장하지 않는다. 과거 Git 이력은 유지하므로 최초 clone 크기는 같은 양만큼 줄지 않는다. 아트 원본과 증거는 [복구 안내](evidence/unused-file-optimization-20261020/RESTORE.md)로 바이트 동일하게 복구할 수 있다.

관련: [계획](../architect/2026-10-20_unused-file-optimization.md), [Sprint](../../Docs/SprintChangeLog.md), [개발 일지](../../DevLog/20261020.txt).

## 최종 파일 검사

[파일별 변경 목록](evidence/unused-file-optimization-20261020/commit-files.txt)에32개를 고정했다. Python6개 구문, JSON8개 보고서 파싱, 상대 문서 링크10개, 일일/Sprint 로그의 기존 내용 보존, Source/Config/Content 변경0을 확인했다. 필수 생성 경로 검사도 통과했다. [최종 검사 보고서](evidence/unused-file-optimization-20261020/final-checks.json)를 참조한다.
