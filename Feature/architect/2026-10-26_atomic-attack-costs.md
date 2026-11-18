# R11 공격 자원 합산 및 원자적 결제

## 인계와 보존
부모가 README, overview, 티켓/성장 설계 일부, 조건 스키마/예시를 대화 텍스트로 인계했다. ZIP 전체를 읽거나 해시 검증한 것은 아니다. Library 다운로드는 파일별 1회 실패 후 재시도하지 않았다. 최신 구현 지시를 적용하며 R05 저장 이행과 생산 맵은 변경하지 않는다.

기준 e32e5abe672dd9554b3f51e90b672f6039077d29, UE 5.8.3 CL58210709. 원본 D:/Project/Rogue10m은 clean. 기존 저장 98개 해시 동일. 원본 Editor 빌드와 기존 HybridSkillTree 3건 통과. 작업은 --no-hardlinks 독립 물리 clone의 Sprint#9-2-atomic-attack-costs에서 수행한다. 원본 저장파일을 작업 복제에 가져오지 않는다.

## Packet R11
- 목표: 동일 자원 행 합산 후 전 자원 검증, 기본 비용+선택적 마나 proc의 단일 결제. 기본 부족 시 실패/무차감, proc만 부족 시 기본 공격 허용.
- 변경: CombatComponent, SkillTreeCombatComponent, 새 자원 예산 타입과 자동화 테스트.
- 경계: 기존 Health는 총비용보다 커야 함. 음수/NaN/무한/알 수 없는 자원 비용은 거부. 순환 스태미나 할인은 합산 전 일관되게 적용. 실패는 게이지/쿨다운/공격 실행/사용 횟수에 영향 없음.
- 원자성: 게임 스레드에서 사전 검증 후 각 자원을 한 번 결제하는 논리 transaction. 재진입 공격을 거부한다. GAS 개별 attribute 알림을 하나의 전역 알림으로 바꾸지는 않는다.
- 완료: 실제 수락 경로를 쓰는 중복 비용, 여러 자원 실패, 체력 경계, proc fallback, exact 합산, 재진입, 다단 실행 snapshot 테스트와 기존 3건 통과. 복제 Editor 빌드 성공.
- 롤백: 이 packet의 소스/문서 diff만 되돌리거나 독립 복제를 보관/폐기. 기존 해금 원장/세이브/맵은 변경하지 않음.

## R02/R03 후속
R11 검증 후 별도 packet으로 판단한다. MageRank는 Mana identity와 분리하고 새 필수 AND를 기존 OR 배열로 흉내 내지 않는다. 현 checkpoint에서는 신규 저장 도메인/실제 두 기술 활성화는 도입하지 않는다.
