# 몬스터 AI 타깃 수명 정리 — 최초 인계 및 후속 통합

## 2026-11-11 현재 상태

아래의 독립 인계 기록 이후 컨트롤러 수정이 공유 작업 트리에 통합됐다. 테스트는 concrete HumanMale BP와 PlayerState/ASC 초기화로 보완됐고, 과거 Engine_BossTargetB에서 실제5개 성공 및 BossNaturalB에서 자연AI112검사 실패0을 확인했다. 첫 Engine_BossTargetA의 추상 native 클래스 생성 실패는 보존한다. 옛 테스트 후보를 복원하지 않는다.

현재 DLL은 과거 B DLL과 달라 이 결과를 새 현재 바이너리의 합격으로 표현하지 않는다. 이번 재개 작업에서는 UE·UBT를 실행하지 않았다. 최신 소스/증거 비교와 새 검사 도구는 `Feature/doc/2026-11-11_monster-ai-resume-readiness.md`를 참조한다. 아래 미적용·미실행 표기는 최초 인계 시점의 이력이다.

## 최초 인계 결과 (2026-11-10)

공유 프로젝트를 수정하지 않고 AIController의 소멸 타깃 정리 패치와 실제 UE 객체를 사용하는 자동화 테스트를 별도 작업 공간에 준비했다. 통합·Editor 빌드·신규 UE 테스트 실행은 사용권 인계 후의 작업이다. 커밋·푸시하지 않았다.

## 실제 현재 구현

- `Enemy/AI/Rogue10mMonsterAIController.cpp`는 기존 BT/BB 또는 네이티브 fallback을 사용한다. fallback BT는 Decision → Wait(0.25초)다.
- 시야·피격 인지, 거리 제한, 기억 만료, 네비게이션 추적과 순찰이 이미 있다. `BB_Monster`는 TargetActor·HasTarget 등 상태 키를 사용한다.
- 인간형 보스는 `Rogue10mHumanoidAnimInstance`와 DefaultSlot 몽타주를 opt-in으로 사용한다. 피해·예고·회복 시간은 기존 AttackPatternComponent가 결정한다. 애니메이션은 authoritative 타이밍에 맞춰 재생된다.
- 기존 Human3P_E1 증거에는 일반 기본/파동/장판 약 1.0/1.6/1.9초, 분노 기본/파동/장판 약 1.0/1.2/1.425초의 예고가 있다. 이동 및 Alt 회피 fixture에서 피해0을 관측했다. 최신 코드의 새 실행 결과나 실제 사람 난이도 판정은 아니다.
- 기존 Human3P_E1은 451검사 실패0·18전투+피격/사망·22PNG 검토 결과이며, 전종 자연 AI와 전체 필드 플레이는 미인증이라고 명시한다. 그 한계를 유지한다.

## 확인한 원인

UE5.8 `BlackboardKeyType_Object.cpp`는 object 키를 `FWeakObjectPtr`로 저장하고 `WeakObjPtr.Get()`을 반환한다. 타깃이 제거되면 object 키 조회는 null이 될 수 있지만 별도 bool 키는 자동으로 바뀌지 않는다.

기존 컨트롤러는 `Target && !IsValidCombatTarget(Target)`에서만 ClearTarget을 호출한다. 따라서 TargetActor가 이미 null이면 HasTarget·IsInAttackRange·WasDamaged 정리를 건너뛴다. ClearTarget에는 BB 정리뿐 아니라 공격 취소, 포커스와 이동 정리도 포함된다. 기존 MonsterChaseEffects는 중단/재개·거리 제한을, BossReadability는 살아 있는 타깃 획득을 검사하지만 소멸 타깃을 검사하지 않는다.

## 수정

`Target || HasTarget` 상태를 확인한 뒤 유효하지 않은 타깃을 기존 ClearTarget으로 정리한다. 타깃을 원래 갖지 않은 순찰에서는 정리 함수를 반복 호출하지 않으므로 진행 중인 순찰 이동이 중단되지 않는다. 새 Tick·상태 소유권·별도 에셋·게임플레이 튜닝은 추가하지 않는다.

## 최초 인계 당시 검증 구분

| 구분 | 상태 |
| --- | --- |
| 현재 소스 및 UE5.8 약한 참조 구현 검토 | 완료 |
| 독립 코드 패치 작성 | 완료 |
| 신규 실제 UObject/BB/AIController 자동화 테스트 작성 | 5개 준비, 실행 전 |
| 패치 적용/역적용 검사와 변경 경로 검사 | 16개 정적 준비 검사 통과, verification_Review02/verification.json |
| 공유 AI 기준 SHA 보존 대조 | 컨트롤러 원본 SHA 일치, 정적 검사에 포함 |
| 신규 UE C++ 전체 컴파일 | 미실행: 에디터·UBT 사용권 인계 전 |
| 신규 UE 자동화/기존 자연 추적 회귀 | 미실행: 통합·빌드·사용권 인계 후 |
| 실제 입력·전체 맵 플레이 | 미실행 |

## 최초 인계 당시 통합 파일과 실행 순서

- 런타임 변경: `Source/Rogue10m/Enemy/AI/Rogue10mMonsterAIController.cpp`.
- 새 테스트: `Source/Rogue10m/Tests/Rogue10mMonsterAITargetLifecycleTests.cpp`.
- 문서: 이 결과, 짝 설계, 당일 DevLog, SprintChangeLog. 당일 로그와 changelog는 공유 추가분을 보존하면서 이번 항목만 병합한다.
- `tmp/work-boss-ai-target-loss-20261110/`의 source/docs 분리 패치와 `README.md`를 인계한다. 원본 컨트롤러 SHA가 달라졌으면 다시 검토한다.
- 독점 사용권 획득 후 Editor 빌드 → `Rogue10m.MonsterAI.TargetLifecycle` → 기존 HumanoidAnimation/MonsterAttackGeometry → `VerifyHumanoidMonsterAI20261110.ps1` 자연 추적 회귀 순서로 검증한다. 기존 저장 네임스페이스를 쓰지 않는다.
- 실패하면 수정한 조건과 신규 테스트만 롤백한다. 맵·무기·AttackPatternComponent·보스 DA는 이번 패치에 포함하지 않는다.

## 별도 후속 제안

시야를 잃은 동안 UpdateCombatBehavior가 현재 타깃 위치로 LastKnownLocation을 계속 갱신하고 actor goal을 추적하는 현 구현은 향후 별도 검토할 수 있다. 마지막으로 본 위치로 수색하도록 바꾸려면 이동·공격·기억 정책과 실제 가림/네비게이션 회귀가 필요하다. 이번 작은 수명 정리 패치에는 포함하지 않았다.
