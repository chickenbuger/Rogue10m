# Windows 검증 후 gameplay 통합 계획

기획→개발→검증 순서로 각 패킷을 현재 공유 소스에 실제 반영한다. Windows ArchiveD는 이전 통과 체크포인트로 보존한다. Source/Content 변경은 검증 전 완성으로 표기하지 않는다.

## 패킷과 소유권
1. 몬스터 자동 전리품: 정확한 수령자 인벤토리 우선, 기존 stack metadata 유지, 초과/실패분만 바닥/지연 전달. no-duplicate 및 아이템 총량, invalid receipt quarantine, 시체 수명 밖 재시도/저장 Write–Read를 확인한다.
2. 무기 스킬 V2: 비용 결제 후 쿨다운 보존, G04/G10 이동·종점 타격, 몽타주 중단/피격 시 이동과 미발생 타격 취소. 피격 면역은 추가하지 않는다. 20 DA 복제는 제작/검증과 실제 production 프로필 연결을 구분한다.
3. 다섯 마을 travel fixture: 새 Source test 한 파일을 설치해 정식 Editor build에 포함한다. 실제 Hub overlap, CharacterMovement 이동, 방문/재방문, Abandon receipt 및 귀환을 검증한다. 종료 시 경제 변화 없는 fixture임을 명시하고 victory/full-run 완료로 표현하지 않는다.

세 패킷의 실제 적용 전 파일별 hash/원본은 tmp/gameplay-integration-20261111/PreApplyA에 보존했다. Source 변경이 서로 겹치지 않는지 확인 후 순차 적용, UHT/정식 Editor 빌드는 한 번 묶고 재시작한다. 공유 Editor/build는 통합 담당이 독점 조정한다. 다른 Work의 staged 패치는 별도 검토 없이는 적용하지 않는다.

## 완료 기준
- 현재 dirty tree 기준 git apply --check, engine-free tests, UHT/Editor build.
- 신규 auto-loot5/기존 loot8/저장 Write–Read, 신규 TechniqueMotion9와 비용·무기·카탈로그 회귀. 세이브 전체 hash guard와 독립 QA 이름을 사용한다.
- 실제 travel direct/revisit 두 경로와 고유 RunId/receipt/Hub 복귀 검증. 강제 teleport나 seeded visit을 통과로 세지 않는다.
- Source/asset 완료 단계를 구분하고 필요한 실제 3인칭 영상/프레임을 확인한다. 대기중 VFX 가독성/다른 기획 미완은 별도 유지한다.
- 통과한 새 체크포인트로 Windows Development package를 갱신하고 strict content 및 실제 cold 메뉴 검사를 다시 수행한다.

## 되돌리기
공유 Git reset/clean은 사용하지 않는다. 통합 담당이 수정한 정확한 hunk/새 파일만 식별하여 scoped backup과 비교한다. 다른 작업이 겹친 파일은 통째로 덮어쓰지 않고 재조율한다. Binary asset은 Editor만 사용한다. 실패한 빌드/실행/자산 보고서는 보존한다.
