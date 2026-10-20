# 현재 상태 main 통합 계획

- 요청: 현재 작업 상태를 커밋·푸시하고, 이후 기능 개발 완료 때마다 main으로 통합한다.
- 작업 브랜치: `Sprint#5-37-main-checkpoint`.
- 범위: 기존 누적 C++·설정·Unreal 에셋·원본·도구·검증 증거·문서와 승인 규칙. 새로운 게임 기능은 구현하지 않는다.
- 원본 상태: HEAD `7d2b688`, 원격 확인 후 develop/test/main `b7ab6b6`, 기존 변경과 Studio 제거 작업을 유지한다.

## Ultrawork Packets

| 패킷 | 목표와 변경 영역 | 완료 조건 | 검증 | 되돌릴 경계 |
| --- | --- | --- | --- | --- |
| P1 | 변경 목록·크기·원격 이력·기존 검증 조사 | 게시 대상과 생성/로컬 파일 구분, 독립 검토 | git status/diff, 파일 목록·크기·경로 검사 | 읽기 전용 조사 |
| P2 | AGENTS·Git/Lazy 규칙·기록 보완, 현재 코드 검증 | 지속 승인 명시, 프로젝트 생성·Editor 빌드와 검토 결과 기록 | Build.bat -projectfiles, Scripts/BuildEditor.ps1, Scripts/CheckGeneratedChanges.ps1 | 이번 문서 변경과 스냅샷 커밋 |
| P3 | 스냅샷 커밋, develop→test→main 병합·푸시 | 원격 main과 로컬 main 일치, 누락 변경·생성 파일 없음 | staged diff 검사, git ls-remote, git status | force/reset 없이 이전 커밋 참조 보존, 필요 시 별도 revert |

## 완료 정책

현재 사용자 요청은 커밋·푸시와 향후 기능 완료 시 main 통합의 명시적 승인이다. 새로운 기능은 작업 브랜치에서 개발·검증·리뷰·기록 후 승격한다. main 통합 성공으로 Sprint5를 닫으며, 다음 기능 작업은 `Sprint#6-1-...`로 시작한다. 기존 에셋은 이번 작업에서 편집하지 않는다. 실행하지 않은 QA·패키징은 검증했다고 기록하지 않는다.
