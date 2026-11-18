# 동일 프로필 다음 판 연결 검증 결과

Sprint#9-26-next-run-validation에서 **정상 보스 승리 → 마을 귀환 → 같은 프로필의 다음 필드 → 포기 취소·확정 → 저장 후 마을 귀환**을 실제 게임 프로세스의 프로그램 입력으로 확인했다. Sprint#9-24에서 실행하지 못했던 연속 경로의 검증을 마쳤으며, develop/test/main 병합은 수행하지 않았다.

## 실제 실행과 상태 경계

`BasicRun1103P`는 2026-11-03 14:22:05~14:41:14 UTC에 실행됐다. Driver wall 1129.537초, 엔진/실행기 exit0, `full_boss_completion=1`, `next_run_connection_complete=1`, 요약 검사 오류0이다. 녹화 OFF, 사람의 물리 입력은 미실행이다. Controller InputKey/DoAim, Slate 문자 입력과 실제 UMG delegate를 사용하는 자동 전략이며, 사람의 보스 난이도 평가와 구분한다.

선택·부착 프로필은 전체 흐름에서 `99D987EA4EBC50855D4592A606242C5A` 하나였다.

| 경계 | RunId·관측 |
|---|---|
| 첫 정상 런 A | `A8441CA54289B7C7A7AB8F86BD24E2E2`. 대검 UI 장착·자연 LV3/대검 숙련3·마나 입문 1P 결제, 실제 보스 처치·승리 저장·귀환 |
| 첫 승리 후 Hub와 다음 필드 B | `A46080424E89674A92A255ADC7C76163`. Hub가 새 ID를 만들고 정상 도보 포털 입장이 같은 ID를 유지. 첫 승리 결과는 `returned_victory`/`Victory`로 고정 |
| 다음 판 포기 후 Hub C | `427BA59D48DA66A00FAD2B8111630062`. A/B와 다르며 같은 프로필로 `WaitingToStart` 복귀 |

다음 필드는 실제 `OpenRunMap`/Running, 총 시간1200초였다. 샘플의 남은 시간1200→1197초 진행을 확인했다. 레벨1·XP0, Brawler/GreatSword 숙련XP0, 기술 투자0, 이번 판 `I.Mana.Entry` 미학습, 해당 Discovery 유지, 너클 시작, 첫 런 대검/GUID 소실을 검사했다. 첫 승리 후 Hub, 다음 필드, 포기 귀환에서 초기 상태와 비교했다.

기존 공격 슬롯 Primary/Special/ChargedPrimary/ChargedSpecial의 **유효 자산 또는 빈 슬롯과 `IsAttackSkillUnlocked`**가 초기 안정 Hub와 일치했다. 이 비교는 새 Ctrl/Q/E/R 4슬롯 구현을 뜻하지 않는다. 공개 `CaptureCombat()`에 대검 기술 목록 등이 진단값으로 남는 사실도 기록했으며, raw private legacy cache/전체 내부 원장의 초기화를 인증하지 않는다.

설정 메뉴에서 실제 활성 `AbandonCancelButton` delegate를 실행해 확인 창만 닫히고 런·프로필·Discovery·재화가 유지됨을 확인했다. 다시 포기를 확정하고 결과 화면의 귀환 버튼으로 저장·마을 복귀했다. 포기 보상0과 필드 입장 기준 Gold/Crystals/로비 earned·available의 복원을 비교했다. 저장 성공 영수증은 관측했지만 이번 P에서 저장 실패나 재시도는 발생하지 않았다. 실패 복구 경로의 새 실전 검증으로 세지 않는다.

보스전의 Alt 회피는 요청8/수락8/미관측0, 플레이어 HP 감소 관측1회였다. 이는 피해 회피율이나 사람에게 충분한 회피 시간의 증거가 아니다. 첫 승리 상태를 유지하는 `first_run_full_gate_frozen`, 취소 동작의 `next_run_cancel_action_verified`, 다음 필드/귀환 슬롯 비교는 모두1이다.

## 실패 보존과 증거 I/O 수정

앞선 `BasicRun1103O`는 자연 성장·학습 후 보스 이동 중 wall389.908초에 `evidence_write_failed`로 중단됐다. 보스/다음 판 완료0, 엔진 exit0·실행기 exit1이며 원본 O 로그·소스·사진을 보존했다. O는 정확한 파일명/Win32 오류를 남기지 않아 당시 원인을 확정하지 않는다.

별도 Windows 파일 계약 시험에서는 독점 append writer와 관찰 reader의 공유 충돌 Win32 32를 재현했다. QA writer에 `FILEWRITE_AllowRead`, 관찰기에 `FileShare.ReadWrite`와 메모리 스냅샷 파싱을 적용하고 파일명·플랫폼 오류 진단을 보강했다. 공유 append100회·101행의 누락/중복0, 실제 UE `Rogue10m.BasicRunEvidence.SharedReaderAppend` 자동화1/0·경고0·미실행0을 확인했다. 요약 파서28/28·AST오류0은 합성 입력 검사이며 게임 실행 횟수에 합산하지 않는다. Editor 빌드는42.00초·18.73초·7.77초 성공했다.

## 보존과 검증 한계

O 시작360개 세이브는 SHA256 변경/누락0, O의 허용 QA2개만 추가해362개였다. P 전후362→364개도 기존 변경/누락0·미확인 신규0, 허용 QA2개만 추가됐다. 저장 삭제·복원·속성 변경은 하지 않았다. 이번 packet은 기존 맵·애니메이션/에셋·제품 전투 수치·저장 스키마를 변경하지 않는다.

P의 실제 PNG45장과 O의31장 모두 전체 디코딩·SHA256 검사를 마쳤다. P의 승리/다음 필드/실제 취소/포기 정산/최종 마을 사진5장은 Root가 직접 열어 검토했고, 원본 바이트로 기능별 폴더에 보존했다. 사진의 UI 상태와 수치 검사를 구분하며 새 모션 품질 통과를 주장하지 않는다. Driver가 관측한 애니메이션 phase를 정확한 접촉/GPU 화면 시각으로 해석하지 않는다. 공격 회복 phase/파일은 미확정이며 회피 회복 캡처는 큐가 바빠 누락됐다. 사람 직접 플레이, 새 애니메이션 품질, 정상20분 시간초과 재검증, 패키징 완료는 이번 결과에 포함하지 않는다.

증거는 `DevLog/BasicRunPlayQA/BasicRun1103P/`의 `session.json`, `save-comparison.json`, `Driver/summary.txt`, `events.csv`, `samples.csv`, `next-run-skill-state.csv`, `screenshots.csv`와 원본 PNG다. O 실패는 같은 부모 폴더의 `BasicRun1103O/`에 있다. NativeIO 보고서는 `Feature/doc/evidence/next-run-validation-20261103/NativeIO1103A/report/index.json`이다.

## 다음 개발

사용자의 최신 요청을 우선해 보스 공격 전조·판정·이동 여유를 측정하고 날지 않는 보스 후보를 검토한다. Fab 보유/무료 에셋 확인, 정면 시야·중앙 크로스헤어, 테스트 모드 몬스터 히트박스/플레이어 공격 범위 표시, 처치 아이템 드롭을 각각 작은 packet으로 구현·검증한다. 기존 4개 액티브 슬롯·무기 프리셋, Discovery 정보 UI, 필드 성장/경제와 패키징 계획도 유지하며 미정 수치는 임의로 확정하지 않는다.
