# 보스 정산 Save v3 통합 독립 검토

검토일: 2026-10-20. 작업: `Sprint#7-2-boss-integration-validation`. 기준 main: `ddc79e5a8270afb1b40660e969771ed920057816`. 병행 감사의7-1을 보존하도록 보스 작업 번호를 조정했다.

## 검토 결론과 현재 상태

수정 전 실제 게임에서 최초 정산 저장과 메모리의 불일치를 재현했다. 종료 판정은 즉시 유지하면서 저장·표시를 명중 처리 이후로 지연하는 수정은 원인에 맞는다. 수정한 컴포넌트 두 파일의 정적 검토에서는 추가 차단 결함이나 범위를 넘는 변경을 발견하지 못했다.

이 문서 작성 시 수정 후 빌드와 여섯 프로세스 검증은 책임 작업자가 진행 중이다. 정적 검토 결과를 수정 후 게임 검증 성공으로 해석하지 않는다. 이 검토에서는 Source·Git·Editor 프로세스를 변경하지 않았다.

## 수정 전 실제 재현

[Write 결과](../boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3red1_victory/write-results.txt)와 [실제 게임 로그](../boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_v3red1_victory/write.log)를 확인했다.

- 최초 정산 로그는 `2026-10-20 13:12:17.592 UTC`에 `victory=1`, `boss_xp=3000`, `boss_gold=300`, `saved=1`을 기록했다.
- CoreTicker의 최초 디스크 조회는 `13:12:17.738 UTC`로, 저장 성공 로그에서 146ms 뒤였다. 이 조회는 귀환이나 명시적 재저장을 실행하지 않았고 1초 자동저장보다 앞선 경계를 관측했다.
- Write 49개 검사 중 2개가 실패했다. 기존 Growth/Inventory/Combat/Town의 디스크 대 메모리 그룹 비교와 전체 SkillTree 원장 비교가 불일치를 검출했다. 저장 프로필과 현재 SaveGame 버전 비교는 통과했다. 그룹 실패를 네 도메인 모두의 개별 실패라고 확대하지 않는다.
- 실제 플레이어 마지막 공격이 숙련 경험치를 정확히 1 올리는 검사는 통과했다. 따라서 숙련을 지급하지 못한 문제가 아니라 최초 저장 순서에 공백이 있음을 뒷받침한다.
- 해당 게임 프로세스의 종료 코드는 0이었지만 결과는 `RUN_COMPLETION_FAILED`, `failures=2`였다. 실행기는 성공 marker 부재와 실패 결과를 확인하여 종료 코드 1로 거절했다. 프로세스 종료 코드만으로 QA를 통과시키지 않았으며, 실패 검사를 제거하거나 비교를 완화하지 않았다.

[수정 전 실행기 기록](runtime-red-launcher.log)과 [수정 전 빌드 로그](editor-build-red.log)는 별도 보존한다.

## 원인과 변경 범위

`CombatComponent::ApplyAttackDamage`는 `ApplyDamage`를 호출한 뒤 확인된 명중의 숙련 XP와 관련 통지를 처리한다. 최종 보스는 `ApplyDamage` 안에서 사망하면서 승리 정산을 호출했고, 기존 정산은 그 호출 스택 안에서 즉시 `SaveNow`와 저장 성공 화면을 실행했다. 따라서 마지막 명중의 숙련 XP·자동 해금보다 앞선 스냅샷이 최초 결과 저장에 남을 수 있었다. 이후 자동저장이나 귀환 전 재저장은 이 차이를 가릴 수 있다.

검토 대상 Source는 다음 3개다.

| 파일 | 변경 목적 |
| --- | --- |
| `Source/Rogue10m/Components/Rogue10mRunCompletionComponent.h` | 대기 상태·다음 틱 타이머·원래 Pawn과 프로필 GUID, 수명 종료 정리 선언 |
| `Source/Rogue10m/Components/Rogue10mRunCompletionComponent.cpp` | 즉시 종료 확정과 지연 저장 경계 분리, 대기 중 귀환 차단과 동일 프로필 검증 |
| `Source/Rogue10m/Tests/Rogue10mRunCompletionRuntimeTest.cpp` | 실제 Save v3 원장 준비·전체 비교, 마지막 숙련 1XP와 최초 디스크 저장 관측 |

새 보상·SaveGame 버전·성장 경제·콘텐츠를 도입하는 변경은 아니다. 기존 원장의 비교와 저장 시점을 보완한다.

## 컴포넌트 경계 검토

- 최초 terminal phase와 `bSettled`는 즉시 유지한다. `GameState`의 Running 전용 종료 가드가 타임아웃·플레이어 사망·보스 사망의 뒤늦은 결과 변경을 막는다. GameMode의 패배→`Die()`와 Character의 `FinishRun(false)` 재진입도 기존 dead/terminal 가드로 끝난다.
- 종료 당시 `ResultCharacter`와 연결된 `ResultProfileId`를 보존한다. 저장 시 Controller의 현재 Pawn이 원래 Pawn인지, 같은 World인지, 연결 프로필 GUID가 같은지 확인하고 기존 `CanRetrySave`의 선택·연결·저장 프로필 일치 검사를 사용한다. 다음 틱 전에 다른 Pawn이나 프로필로 바뀐 경우 해당 새 프로필을 이전 런의 정산으로 저장하지 않는다.
- `SetTimerForNextTick(this, &ThisClass::FinalizeSettlement)`의 UObject 바인딩은 객체 수명에 묶인다. 타이머 handle을 보관하고 `EndPlay`에서 제거하며 pending 상태도 해제한다. callback은 RunState의 유효성과 동일 World를 다시 확인한다.
- 다음 틱 callback에서 전투 취소→`SaveSettlement`→pending 해제→결과 UI 순서를 사용한다. 마지막 명중의 동기 처리 스택이 반환된 뒤 저장하며, 성공 여부가 확정되기 전에 저장 완료 화면을 표시하지 않는다.
- `ReturnToTown`은 pending 동안 거절한다. 저장 실패 후에는 pending을 해제하고 오류 화면을 제공해 기존 `SaveNow` 재시도를 사용할 수 있다. 저장 재시도에는 보상 재지급 경로가 없다.
- 선택적 정리로 pending 설정을 `bSettled` 설정 직후에 배치하면 외부 메서드 호출 전부터 상태 불변조건이 더 분명해진다. 현재 확인한 호출 경로에서 이 배치로 인한 실제 재진입 결함은 찾지 못했으므로 차단 항목으로 보지 않는다.

## 실행 중 공격 참조에 대한 정정

`CancelCombatVisuals`는 `CancelPendingAttackHits`로 이어지며, 이 함수는 `ActiveAttackExecutions`에서 일부 항목을 제거한다. 그러나 제거 조건에는 `HitStartDelaySeconds > 0`과 `CompletedPulses == 0`이 모두 필요하다. 현재 `ExecuteAttackHitPulse`는 `ApplyAttackDamage` 전에 `CompletedPulses`를 증가시킨다. 그러므로 검토한 보스 최종 피해 경로에서 실행 중인 `Execution&`나 `Execution*`가 이 취소로 제거된다고 확정할 수 없다.

확인한 `CancelBoundSkillInput`은 보류 입력 참조와 모니터 타이머를 정리하고, Hybrid 변경 통지는 기술·능력치·표시를 갱신한다. 이 경로에서 실행 중 공격 맵 전체를 초기화하는 동작도 찾지 못했다. 전투 취소와 결과 UI를 다음 틱으로 옮긴 것은 명중 처리 중 재진입을 줄이고 저장 경계를 모으는 설계 선택이다. 확정된 메모리 참조 해제 결함을 수정했다는 주장으로 기록하지 않는다.

## 수정 후 반드시 확인할 사항

실제 게임에서 귀환·자동저장 전 최초 완료 저장 파일의 Save v3와 전체 SkillTree가 메모리와 같은지 확인해야 한다. 마지막 명중의 숙련 1XP가 해당 파일에 있어야 하며, 보상과 원장이 중복 피해·저장 실패·귀환 재시도로 늘지 않아야 한다. 기존 여섯 프로세스의 승리/저장 실패/시간 초과/사망 및 별도 프로세스 복원도 다시 통과해야 한다.

이 항목들의 수정 후 실제 결과는 진행 중이므로 이 문서에는 성공 수치나 Green 완료를 기록하지 않는다.

후속: 위 진행 상태는 정적 검토 작성 시점의 기록이다. 이후 실제 검증의 최종 결과와 게시 준비 상태는 [보스 정산 Save v3 통합 결과](../../2026-10-20_boss-save-v3-integration.md)를 참조한다.
