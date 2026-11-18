# 몬스터 AI 타깃 수명 정리 패치 계획

## 범위와 기준

- 기준 작업 공간: `D:/Project/Rogue10m`, `Sprint#9-29-humanoid-boss-npc`, HEAD `51f2def0`와 2026-11-10의 현재 수정본. 공유 작업은 덮어쓰지 않는다.
- 독립 작업 공간: `tmp/work-boss-ai-target-loss-20261110/candidate`, 별도 Git 작업 브랜치 `Sprint#9-29-ai-target-loss-patch`. 실제 Content 폴더나 링크를 만들지 않는다.
- 에디터·UBT 사용권은 인계받지 않았다. 이번 산출물은 미적용 패치와 자동화 테스트 준비이며, 빌드·런타임 검증 완료로 표현하지 않는다. 커밋·푸시하지 않는다.

## 현재 구현과 기존 검증

- AIController의 C++ BT task가 결정과 순찰/추적/공격을 실행한다. 저장 BT/BB가 유효하면 사용하고, 없으면 네이티브 임시 BT/BB를 만든다. fallback 결정 주기는 0.25초다.
- BB 키는 TargetActor, HomeLocation, PatrolLocation, LastKnownLocation, HasTarget, IsInAttackRange, WasDamaged다. 시야/피격 인지와 귀환 거리·기억 만료 정책이 있다.
- 인간형 보스는 opt-in HumanoidAnimInstance와 DefaultSlot 몽타주를 사용한다. 일반 몬스터는 기존 프로필을 유지한다. CharacterMovement의 기본 지상 이동과 기존 AI 계약은 보존한다.
- 공격 위치·방향·시간은 시작 시 고정한다. 보스 예고 중 HP 감소가 이미 표시된 예고 시간을 줄이지 않는다. 피해 적용에는 캡슐 포함 범위와 가림 검사가 있다.
- 기존 Human3P_E1 증거는 18전투+피격/사망, 451검사 실패0이다. 기본/파동/장판 일반 예고는 약 1.0/1.6/1.9초, 분노 시 약 1.0/1.2/1.425초다. 해당 프로그램 입력 fixture에서 이동·Alt 회피 시 피해0을 관측했다. 물리 입력 난이도·전종 자연 추적·필드 전체 플레이 인증은 아니다.
- 현재 Enemy/AI 컨트롤러에는 공유 미커밋 변경이 없다. AttackPatternComponent/Profile, 인간형 애니메이션 및 보스 DA는 다른 작업의 변경이 있어 수정 대상에서 제외한다.

## 선정한 독립 개선

타깃 액터가 제거되면 UE Blackboard object 키의 약한 참조는 null을 반환할 수 있지만 HasTarget은 자동으로 false가 되지 않는다. 현재 ExecuteBehaviorDecision은 `Target && !IsValidCombatTarget(Target)`일 때만 ClearTarget을 호출한다. 이 때문에 null이 된 타깃과 HasTarget/공격 범위/피격 플래그가 어긋나고, 타깃 소멸 시 공격 정리가 누락된다.

이 조건에 HasTarget 상태를 포함해 기존 ClearTarget 경로를 재사용한다. 타깃을 가진 적이 타깃을 잃으면 공격·포커스·이동 및 관련 BB 상태를 함께 정리한다. 원래 타깃이 없는 순찰 중에는 ClearTarget을 반복 호출하지 않는다. 예고 시간·피해·애니메이션·보상·에셋은 바꾸지 않는다.

## Ultrawork Packets

| Packet | 목표 | 수정 위치 | 완료 조건 | 검증 명령 | 롤백 경계 |
| --- | --- | --- | --- | --- | --- |
| Architect | 현재 구현·변경·기존 증거 구분 | 이 문서 | 원인과 범위 확인 | 정적 소스/엔진 검토 | 문서만 제거 |
| Implementer | 소멸 타깃 상태 정리 | Enemy/AI/Rogue10mMonsterAIController.cpp | 기존 ClearTarget으로 수렴, 순찰 유지 | 별도 검토 및 diff | 조건 변경 한 곳 복원 |
| Tester | 실제 UE 객체 회귀 준비 | Tests/Rogue10mMonsterAITargetLifecycleTests.cpp | 소멸/누락/유효/중단/무타깃 이동 시나리오 | 에디터 사용권 인계 후 Automation RunTests Rogue10m.MonsterAI.TargetLifecycle | 신규 테스트 파일 제거 |
| Reviewer/Doc Writer | 적용 전 검사·인계 | 결과 문서, DevLog, 패치 | patch check·경로검사·공유 기준 SHA 일치 | 전달 폴더의 VerifyPatch.ps1 | 독립 산출물만 제거 |

## 인계 후 실행할 검증

1. 최신 컨트롤러와 기준 SHA를 대조하고 패치를 검토·적용한다. 충돌이 있으면 자동 덮어쓰지 않는다.
2. 에디터·UBT 단독 사용권을 받은 뒤 `Scripts/BuildEditor.ps1`을 실행한다.
3. 격리 QA save 옵션을 가진 NullRHI 프로세스에서 `Rogue10m.MonsterAI.TargetLifecycle`을 실행한다. 신규 테스트는 GameInstance를 생성하지 않아 사용자 저장을 읽거나 쓰지 않는다.
4. 기존 HumanoidAnimation, MonsterAttackGeometry와 실제 맵 MonsterChaseEffects 회귀를 실행한다. 기존 실행기 `Scripts/VerifyHumanoidMonsterAI20261110.ps1`는 이번에 실행하지 않는다.
5. 필요 시 실제 자연 추적·회피를 렌더링해 확인한다. 기존 공격 모션 시각 검증을 이번 정적 패치의 검증으로 재사용하지 않는다.
