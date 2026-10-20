# 보스 정산 Save v3 통합 보완

2026-10-20. 작업 브랜치 `Sprint#7-2-boss-integration-validation`, 게임 검증 기준 `ddc79e5a`. 병행 감사 작업의 `Sprint#7-1` 번호를 보존하도록 이 보스 작업을7-2로 조정한다.

상태: 초기 저장 누락의 실제 재현·수정·Editor 빌드·렌더된 여섯 프로세스 검증·특성 효과 회귀·독립 정적 검토를 완료했다. [설계](../architect/2026-10-20_boss-save-v3-integration.md)의 V1 재현, V2 수정, V3 검증·기록을 마쳤으며 최종 범위 검사·커밋·로컬 승격·공개 게시를 준비한다. 기존301검사는 스킬트리 병합 전 결과로 보존하고, 이번 최신 Save v3의386개 검사와 구분한다. 현재 작업의 커밋·main 반영·원격 게시 완료는 아직 기록하지 않는다.

## 문제와 수정

최종 보스의 마지막 피해 안에서 사망·승리·정산 저장이 실행된 뒤, 명중 함수가 복귀하며 숙련1XP와 파생 상태를 갱신했다. 결과창의 저장 완료 표시 직후에는 초기 저장과 메모리가 달랐고, 귀환 재저장이나1초 자동저장 전까지 공백이 생겼다.

승리/패배와 종료 후 피해 차단은 즉시 확정한다. 정산 컴포넌트는 종료 당시 Pawn·프로필GUID를 고정하고 UObject 약한 다음틱 타이머로 명중 처리가 끝난 뒤 전투 취소·저장·결과 화면을 확정한다. 대기 중 귀환을 거절하고 저장 시 동일 Pawn/월드/프로필·기존 CanRetrySave를 확인한다. EndPlay는 예약 핸들을 제거한다. 저장 실패는 기존 오류 화면과 재시도로 이어진다.

게임 보상·SaveGame 버전·스킬트리 경제·바이너리 에셋은 변경하지 않았다. C++ 변경은 RunCompletionComponent cpp/h와 QA cpp 세 파일이다. 현재 일반 저장과 별도 무기 worktree는 보존한다.

## 수정 전 재현

QA를 먼저 보강하고 게임 코드는 기존 상태로 Editor 빌드했다(99.68초 성공). 실제 StartHub 포탈→배치된 아자토르→플레이어 마지막 공격을 사용했다. 공개 API로 숙련7XP·경험치100으로 얻은1P의 투지 입문 투자·활성 투지를 준비하여 빈 원장 비교를 피했다.

`v3red1`에서 정산 saved=1 로그는 UTC13:12:17.592, 디스크 비교는13:12:17.738이다. 약146ms 뒤 최초 저장의 기존4도메인 그룹과 전체 SkillTree 비교가 실패했다. 실제 마지막 공격의 숙련+1 검사는 통과했다. 총49검사·실패2로 [수정 전 결과](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3red1_victory/write-results.txt)를 보존했다. 게임 exit0이어도 FAILED 마커와 assertion으로 실행기는 exit1로 거절했다. 수정 전 재현을 성공 검증 수치에 합치지 않는다.

## 수정 후 검증

Editor 빌드180.25초 성공([원문](evidence/boss-save-v3-integration-20261020/editor-build-green.log)). 기존 XGE 라이선스 경고에 따라 standalone 빌드를 사용했으며 C++ 오류는 없다. [독립 검토](evidence/boss-save-v3-integration-20261020/review.md)는 약한 타이머/정리, 종료 가드, 원래 Pawn·프로필, 실패 재시도를 확인했다. 실행 중 공격 참조가 제거되는 확정 결함을 고쳤다는 주장은 하지 않는다.

렌더된 D3D11 1280×720의 여섯 프로세스를 `ValidateRunCompletion.ps1 -Case All -Label v3green1`로 실행해 모두 통과했다. 각 게임 프로세스와 전체 실행기의 종료 코드는 0이다. QA는 최초 결과 저장, 포탈/마을 이동, Enter 재저장, 독립 Read와 FailureRead에서 디스크 프로필·현재 저장 버전·Growth/Inventory/Combat/Town/전체 SkillTree를 비교한다. 실패 잠금 동안의 초기 디스크는 의도한 prekill 상태이므로 성공과 같은 비교를 강요하지 않고 재시도 뒤 확인했다.

| 최종 사례 | 검사 수 | 결과 | 근거 |
| --- | ---: | --- | --- |
| 승리·최초 Save v3·마지막 명중 숙련·저장·마을 귀환 | 74 | 실패0·exit0 | [Write](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3green1_victory/write-results.txt) |
| 승리 저장의 별도 프로세스 복원 | 16 | 실패0·exit0 | [Read](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3green1_victory/read-results.txt) |
| 실제 저장 실패·귀환 차단·복구 후 재시도 | 76 | 실패0·exit0 | [Failure](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3green1_failure/failure-results.txt) |
| 저장 재시도 결과의 별도 프로세스 복원 | 16 | 실패0·exit0 | [FailureRead](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3green1_failure/failureread-results.txt) |
| 시간 초과·패배 정산·마을 귀환 | 52 | 실패0·exit0 | [Timeout](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3green1_timeout/timeout-results.txt) |
| 실제 치명적 피해·패배 정산·마을 귀환 | 51 | 실패0·exit0 | [Death](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3green1_death/death-results.txt) |
| 렌더된 핵심 QA 합계 | 285 | 여섯 프로세스·전체 실행기 통과 | [실행기 원문](evidence/boss-save-v3-integration-20261020/runtime-green-launcher.log) |
| 기존 특성 효과 회귀 | 101 | 실패0·exit0 | [Effects](evidence/trait16-runtime-20261011/Rogue10m_Town_QA_boss_v3_green1/effects-results.txt) |
| 최종 성공 검사 합계 | 386 | 실패0 | [검증 결과 요약](evidence/boss-save-v3-integration-20261020/validation-results.json) |

정상 승리의 최초 저장을 귀환·명시적 재저장·1초 자동저장 전에 관측했으며, 마지막 실제 공격의 숙련1XP와 전체 원장이 디스크에 존재했다. 유료 투지 입문·활성 계열·누적 숙련을 포함한 원장을 준비해 빈 데이터의 형식적 비교를 피했다. 보상·원장이 중복 피해와 실패 재시도로 늘지 않는 기존 검사를 유지했다. [승리 화면](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3green1_victory/run-result.png)과 [저장 실패 화면](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3green1_failure/run-result.png)을 직접 열어 결과·보상·오류·재시도 안내의 가독성도 확인했다. 수정 전49개·실패2의 예상 실패는 이386개 성공 검사에 합산하지 않았다.

## 검증 범위

기존 통제 fixture처럼 순간이동·AI정지·준비 중 임시 무적·보스1HP 사전 피해·시간 단축을 사용했다. 실제 마지막 전투 타격과 포탈/UMG Enter/플랫폼 저장/마을 재로드를 검사했다. 전체20분 밸런스·Shipping·D3D12·네트워크·장시간 안정성 검증은 별도다. 이번 작업의 원본 Content/Config 변경은0이며 별도 무기 worktree를 보존했다. 콘텐츠/설정 tree와 수정 Source 세 파일·모듈 해시를 [검증 기준](evidence/boss-save-v3-integration-20261020/validation-scope.json)에 준비했으며 책임 작업자가 최종 범위·해시·생성 경로를 재대조한 뒤 커밋·develop→test→main 승격·공개 게시를 진행한다. 해당 결과는 실제 SHA와 원격 확인 뒤 추가하며 현재는 게시 준비 상태다.

검증 중 병행 감사 작업이 공유 체크아웃과 main을 전진시킨 사실을 확인했다. 그 변경은 문서·감사 스크립트 범위이며 Source/Config/Content가 같아 이번 게임 검증 기준은 유지한다. 책임 작업자는 해당 작업의 종료 후 별도 게시 인덱스와 명시한40파일로 커밋을 구성하고 main 참조를 compare-and-set으로 승격할 예정이다. 공유 브랜치·인덱스와 감사 기록을 덮어쓰지 않는다. 이 절차의 실제 실행·성공 결과는 아직 기록하지 않는다.
