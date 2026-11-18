# R11 공격 자원 합산 및 원자적 결제 결과

동일 자원 비용 행을 각각 검사하던 경로를 자원별 합계 검사로 바꿨다. 예를 들어 스태미나 100에서 비용 60+60인 공격은 이제 거부되며 체력/마나/게이지/쿨다운/공격 사용 횟수도 변하지 않는다. 체력은 총비용보다 커야 하며 다른 자원은 정확한 잔액까지 사용할 수 있다.

기본 비용을 충족한 뒤 남는 마나로 선택적 아이덴티티 발동을 계획한다. 기본 마나 10+발동 5일 때 잔액 9는 공격 거부, 10/14는 기본 공격만, 15는 발동 포함으로 처리한다. 실제 결제는 합계로 한 번 수행한 후 게이지와 발동 쿨다운을 확정한다. 입력 사전검사도 같은 합산 타입을 사용하고, 수락 중 attribute 콜백의 재진입 공격은 거부한다. 음수/비유한값/합계 overflow/알 수 없는 자원은 거부한다.

## 보존과 범위
- 독립 물리 clone: `C:/Users/PC/Documents/Codex/2026-10-27/task/Rogue10m-r11`, 브랜치 `Sprint#9-2-atomic-attack-costs`.
- 기준 SHA: `e32e5abe672dd9554b3f51e90b672f6039077d29`. 원본 `D:/Project/Rogue10m`의 HEAD/브랜치/clean 상태와 기존 저장 98개 SHA-256 불변.
- Content/Config/SaveVersion/저장 필드/사용자 슬롯은 변경하지 않았다. 복제 SaveGames 파일 수 0.
- 설계 README/overview/티켓 및 성장 설계 일부/조건 스키마·예시는 부모 대화의 텍스트 인계로 받았다. ZIP 전체 다운로드·무결성 확인은 미완료이며 수행했다고 주장하지 않는다.
- R02/R03 조건 평가, R05 저장 이행, 신규 기술/랭크 활성화, 생산 맵 반영은 아직 구현하지 않았다. 다음 packet에서 별도 경계로 진행한다.

## 검증
- 원본 기준 Editor 빌드 19.27초 성공, 기존 HybridSkillTree 3/3 성공.
- 복제 Editor 빌드: 사전검사 API 누락의 첫 컴파일 실패 후 기존 API를 보존하여 43.06초 성공. 테스트 fixture 수정 후 최종 증분 빌드 8.36초 성공. UE 5.8.3 CL58210709, MSVC14.44, SDK10.0.22621.
- 신규 자동화 4/4 Success, errors=0, warnings=0: [보고서](evidence/atomic-attack-costs-20261026/AttackResources_fixturefixed/report/index.json).
- 기존 숙련 자동화 3/3 Success, errors=0, warnings=0: [보고서](evidence/atomic-attack-costs-20261026/HybridSkillTree_final/report/index.json).
- [원본 저장/HEAD/clean 보존](evidence/atomic-attack-costs-20261026/original-preservation.json).
- 최초 신규 자동화는 추상 Character를 직접 생성한 fixture 오류로 exit3. 실제 기본 Blueprint Character 및 null 검사로 수정한 뒤 통과했다. [실패 로그](evidence/atomic-attack-costs-20261026/AttackResources/automation.log)는 보존한다.
- `Scripts/CheckGeneratedChanges.ps1`, `git diff --check` 통과.

재실행: `Scripts/VerifyAttackResourceCosts.ps1 -Suite AttackResources -RunLabel <새이름>`, 이후 `-Suite HybridSkillTree`. 이전 증거 경로는 덮어쓰지 않는다. 테스트는 BeginPlay/GameInstance 없이 별도 월드에서 실제 ExecuteAttackSkill과 GAS attribute 콜백을 사용한다.

## 검토와 한계
새 Tick/네트워크/저장 경로는 추가하지 않았다. 기존 내공 순환 할인과 공유 쿨다운 계산을 유지하며 수락 시 캡처하는 공격 효과를 사용한다. 원자성은 게임 스레드에서 전 비용 사전검증 후 결제하는 논리 transaction이다. GAS의 개별 attribute 알림을 동시에 통지하는 기능이나 저장 트랜잭션을 뜻하지 않는다.

NullRHI 자동화에서 다단 공격의 실행 1개/결제 1회 경계는 검사했지만, 실제 다중 대상·피해 pulse·시각 효과·20분 런·Shipping/cook·성능은 검증하지 않았다. 성능 향상 수치를 주장하지 않는다.

롤백은 이 packet의 코드/테스트/문서 변경만 되돌린다. 원본 세이브·숙련 원장·맵 복원 작업은 필요하지 않다. 원본으로 자동 병합하거나 push/PR/배포/유튜브 업로드하지 않는다.
