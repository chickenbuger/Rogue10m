# 남은 지형·전투 기능 검증과 개별 게시

현재 공유 작업은 f1574a3e와 미커밋 변경이다. 게시 기준은 origin/main(fdbe8c8c)이며, 두 상태를 섞지 않고 각 기능의 의존성을 닫은 물리 checkout에서 검증한다.

| 패킷 | 목표·범위 | 완료 조건 | 검증 | 복구 경계 |
| --- | --- | --- | --- | --- |
| P1 | 마법 명령 버퍼 header·독립 시험·실행기 | 현재 코드 6사례1701검사, 독립 리뷰 | TestMagicCommandBuffer.ps1 | 신규3파일·전용문서/증거 |
| P2 | Crop 검사 도구·reflection scanner | main 기반 UHT/Editor와 transient5회귀 | BuildEditor.ps1, Editor-Cmd | Editor helper와 fixture |
| P3 | 프로필 초기화·전리품·전투 | 기능마다 dependency closure와 현재 회귀 통과 | 개별 headless 시험 | 기능별 hunk·클래스 |
| P4 | Crop 지형·도로 | native·map·asset 전체 폐쇄 및 cold 보존/충돌 검증 | AssetRegistry, Editor-Cmd | 후보 맵 패키지와 프로젝트 소유 재질 |
| P5 | 문서·게시 | 각 완료 기능을 commit/push하고 develop→test→main 반영 | generated/diff·원격 확인 | 기능별 작업 브랜치 |

각 패킷은 새 work branch에서 진행한다. 원본 checkout과 실패 증거·사용자 저장을 보존하고 Content 링크는 만들지 않는다. 현재 호스트에 Unreal이 없으므로 테스트는 headless로 진행한다. 새 오류는 실제 AbsLog를 검사한다. 기존 제한이 남은 기능은 완료라고 하지 않고 이유와 정확한 범위를 기록한다.
