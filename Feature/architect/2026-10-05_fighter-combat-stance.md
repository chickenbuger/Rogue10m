# 격투가 전투 대기 자세 계획

## 목표와 범위

2026-10-05 요청은 기존 일반 Idle과 구분되는 격투가 전투 자세를 준비하는 것이다. 일반 상태에서는 손을 내린 기존 Idle을 유지하고, 맨손·너클 전투 활동이 시작되면 두 주먹을 올리는 가드 자세로 연결한다. 공격 후 잠깐 가드를 유지하다가 자연스럽게 일반 Idle로 돌아간다. 기존 강화 공격, 피해·타수·콤보·차징 기준, 머리 카메라 안정화와 V 키의 3인칭 확인 기능을 보존한다.

작업 시작 브랜치는 `Sprint#5-8-fighter-arm-swing`이고 이전 미커밋 작업이 있다. 구현은 해당 상태를 보존한 `Sprint#5-9-fighter-combat-stance`에서 진행한다. 현재 바이트를 체크포인트로 삼고, Git HEAD를 기준으로 이전 작업을 되돌리지 않는다. 계획 담당은 본 문서만 작성한다.

## 확인한 실제 경로

- `ARogue10mStylizedCharacter::GetAnimationPlaybackMesh()`는 숨긴 Manny `AnimationSourceMesh`를 반환한다. 이 메시의 `ABP_Common_Unarmed` 결과를 Appearance Mesh의 Retarget AnimBP가 평가한다. 머리 카메라는 이 Appearance Mesh를 계속 따른다.
- `ABP_Common_Unarmed`는 원본 `ABP_Unarmed`를 복제한 AnimBP다. 별도 C++ CommonAnimInstance는 없다. 기본 이동 상태 머신은 원본 `BS_Idle_Walk_Run`과 `MM_Idle`을 사용한다.
- 기존 수정 이후 공통 AnimGraph는 이동 FootIK를 `DefaultSlot` 앞에서 평가한다. 공격이 끝난 뒤 FootIK가 공격 다리를 다시 덮어쓰지 않도록 이 순서를 보존한다.
- `Rogue10mFistAnimInstance`에 기존 CombatReady와 가드 계산이 있으나 현재 Appearance 전투 대기를 바꾸는 경로가 아니다. 해당 legacy 값만 조정하고 적용 완료라고 판단하지 않는다.
- `CombatComponent::ExecuteAttackSkill`의 자원·쿨다운 검증 성공 이후 지점은 모든 맨손·너클 공격에 공통이다. `StartChargeEffect`는 VFX가 없어도 전투 자세 시작 이벤트를 받을 수 있어야 한다.
- `CharacterAnimationComponent`는 이미 이동·점프·착지·회피 이벤트를 받고 Tick이 꺼져 있다. 전투 활동 시각과 차징 상태도 이 컴포넌트에 두고, native AnimInstance의 기존 애니메이션 갱신에서 읽는다. 별도 Tick이나 타이머를 추가하지 않는다.

## 자세와 전환 설계

### 표현 기준

왼 주먹을 약간 앞에 두고 오른 주먹은 턱 아래에 남기는 복싱 가드를 기본으로 한다. 양팔이 화면 중앙을 가리지 않도록 주먹은 눈높이 아래에 배치하고, 팔꿈치는 몸 쪽에 둔다. 무협지 권사 느낌은 과한 전신 흔들림보다 단단한 발 지지와 공격 준비 자세로 표현한다.

기존 authored 공격 `A_Unarmed_LeftJab`의 첫 닫힌 주먹 가드 포즈를 출발점으로 삼아 공격 시작·회수와의 불연속을 줄인다. 2초 루프 시퀀스는 별도 `FighterStance` 폴더에 만들고 일반 Idle과 기존 공격 시퀀스는 덮어쓰지 않는다. 작은 상체 호흡을 추가하되 발·골반의 지지 위치를 고정하고 머리 component 회전은 보정한다. 루프 첫·마지막 포즈를 일치시킨다.

### 기본 동작

| 상황 | 동작 |
|---|---|
| 전투 활동 이전 | 손을 내린 기존 일반 Idle |
| 사용 가능한 맨손·너클 공격 성공 | 즉시 전투 준비 상태, 공격 자체가 가드보다 우선 |
| 유효한 차징 시작 | 버튼을 유지하는 동안 전투 준비 상태 유지 |
| 공격·차징 종료 | 정지 상태에서 가드로 복귀하고 기본 3초 유지 |
| 유지 시간 종료 | 약 0.45초 블렌드로 일반 Idle 복귀 |
| 전투 준비 중 이동·질주·점프 | 기존 이동·공중 애니메이션 우선; 발을 고정하는 가드 루프 중단 |
| 시간 내 정지·착지 | 아직 전투 준비 상태면 가드 재개 |
| 전투 중 회피 | 회피 몽타주 우선, 전투 준비 시간을 갱신하고 완료 후 가드 복귀 |
| 무기 변경·사망·입력 취소·조종 해제·차단 메뉴·컴포넌트 종료 | 가드와 전투 준비 상태 정리 |

새 입력 키나 별도 방어 판정은 추가하지 않는다. 준비 자세는 시각적 상태이며 피해 경감·패링·타격 판정을 만들지 않는다. 기본 대상은 `ERogue10mWeaponType::Unarmed`와 `Knuckle`이다.

### 확정 연결안

공통 AnimBP의 부모를 신규 `URogue10mFighterCombatStanceAnimInstance`로 바꾼다. 연결은 `기존 이동 FootIK → TwoWayBlend(이동 결과, 신규 가드 SequencePlayer) → DefaultSlot`이다. Native alpha는 이 TwoWayBlend의 가중치로 전달한다. 공격·회피는 기존 Slot에서 최종 우선권을 가진다. 가드는 몽타주 재생을 점유하지 않으므로 루프 재시작으로 공격을 끊지 않는다. 기존 이동 상태 머신, FootIK 설정, DefaultSlot 속성과 순서를 보존하고 해당 입력 연결만 좁게 교체한다.

CharacterAnimationComponent는 성공한 전투 활동의 종료 예정 시각과 차징 유지 여부를 소유한다. 신규 native AnimInstance는 이 상태를 읽어 정지·접지 상태에서만 전투 가드 alpha를 올리고, 이동·질주·점프 때는 기존 이동 포즈로 매끄럽게 돌린다. 인터페이스는 전투 활동 기록, 차징 시작·종료, 상태 초기화로 나누며 실제 C++ 함수명은 구현 결과 문서에 기록한다. QA에서 준비 상태와 실제 native alpha를 구분해 읽을 수 있는 getter를 노출한다. 유지 3초, 진입 0.20초, 복귀 0.45초와 정지 속도 기준을 UPROPERTY로 조정 가능하게 한다. 공격 지속 시간은 실제 몽타주 길이를 `AttackSpeedMultiplier * AnimationPlayRate`로 나누어 포함하며, 재입력·장시간 차징이 유지 시간을 올바르게 갱신해야 한다.

새 가드 시퀀스는 AnimBP의 SequencePlayer 데이터로 연결한다. 기존 Motion Data Asset이나 공격 DA를 바꾸지 않는다. 이동 중에도 전신 고정 가드를 재생하는 식으로 범위를 넓히지 않는다. 추후 이동 중 가드가 필요하면 발 이동을 유지하는 상체 레이어와 전투 이동 클립을 별도 설계한다.

## 안정성 계약

- Appearance Mesh와 기존 머리 카메라 경로를 유지한다. 별도 FirstPersonMesh를 새로 활성화하지 않는다.
- 기존 기본 pitch -20도, 머리 회전 반영 0.15와 최대 회전 8도 설정을 보존한다.
- 새 루프에서 렌더 뼈 local translation·scale을 원본 가드와 같게 두어 팔·다리 길이를 보존한다. 손목과 닫힌 손가락 포즈를 비틀림 없이 유지한다.
- IK helper 뼈는 실제 손·발 위치에 맞춘다. stretch를 허용하거나 임계값을 완화해 검사를 통과시키지 않는다.
- 정지 가드의 발·골반 지지 위치, 루프 경계, 진입·복귀의 프레임 연속성을 실제 포즈로 확인한다. 블렌드 중 손·발과 머리 이동도 검사한다.
- 차징 효과 사용 여부와 가드 상태를 분리한다. 파티클을 끈 현재 모드에서도 유효한 차징은 가드로 연결된다.
- 실패한 공격, 메뉴에서 차단된 입력, 지원하지 않는 무기로 가드를 새로 활성화하지 않는다. 전투 준비 종료·사망·조종 해제·EndPlay에서 timestamp와 차징 상태를 정리한다.
- 기존 강화 공격 10개 시퀀스, 관련 몽타주·DA·프로필은 이번 허용 저장 목록에서 제외한다. 공통 AnimBP와 새 가드 시퀀스만 저장한다.
- 변경 전 현재 Content의 기존 uasset 4,186개 SHA-256을 기록한다. 기존 파일의 허용 변경은 `ABP_Common_Unarmed.uasset` 하나이며 신규 가드 시퀀스 이외의 기존 바이너리를 저장하지 않는다.

## Ultrawork Packets

### P1 — 전투 대기 루프 제작

- 목표: 기존 공격 가드와 연결되는 별도 격투가 전투 대기 루프를 만든다.
- 입력: 첫 왼 잽의 안정된 가드 포즈, Manny skeleton, 기존 authored 공격·일반 Idle 해시.
- 수정 위치: 좁은 Editor 전용 authoring 코드/스크립트, 신규 `Content/Rogue10m/Animation/Combat/FighterStance` 에셋.
- 완료 조건: 신규 2초 루프 시퀀스 재로드 성공, 손목·손가락·뼈 길이·IK·머리 안정성 및 루프 경계 검사 PASS.
- 검증 명령: `./Scripts/BuildEditor.ps1 -NoHotReload` 이후 신규 Editor commandline apply/reload 검사.
- 되돌림 경계: 신규 폴더와 이번 Editor 코드 diff. 기존 Content 바이트는 전체 해시 비교로 보존한다.

### P2 — 일반 Idle과 전투 대기 전환

- 목표: 유효한 맨손·너클 공격/차징/회피 활동이 전투 준비 상태를 만들고 자연스럽게 종료되게 한다.
- 입력: P1 가드 시퀀스, 현재 공격 성공·차징·이동 이벤트, 실제 재생 메시.
- 수정 위치: CharacterAnimationComponent, 필요한 Combat/Character 이벤트 연결, 신규 native AnimInstance, 공통 AnimBP 그래프·부모를 연결하는 좁은 Editor Python 스크립트.
- 완료 조건: 정상 Idle 분리, 차징 중 유지, Slot 공격 우선, 정지 후 복귀, 유지 만료 및 무기·사망·조종 해제·메뉴·EndPlay 정리. 컴포넌트 Tick을 새로 켜지 않는다.
- 검증 명령: Editor 빌드, 실제 입력 기반 신규 전투 자세 런타임 시나리오.
- 되돌림 경계: 수정 전 현재 소스/공통 AnimBP 체크포인트와 이번 diff만 복원한다. 기존 dirty 상태를 체크포인트로 삼고 강한 공격 10개와 모든 공격 DA의 바이트 보존을 확인한다.

### P3 — 통합 기능 및 동작 검증

- 목표: 상태 전환뿐 아니라 실제 Appearance 포즈·손발·머리 카메라와 기존 공격 판정을 확인한다.
- 입력: 통합 에셋과 C++, 격리 QA save slot, 기존 강화 공격 기준 증거.
- 수정 위치: 신규 전투 자세 검증 코드/스크립트, `Feature/doc/evidence/fighter-combat-stance-20261005`.
- 완료 조건: 일반 Idle→공격→가드→복귀, 차징 유지·해제, 재입력, 이동/점프/회피, 무기 전환·메뉴 및 V 3인칭의 실제 포즈 검사 PASS. 사망·조종 해제 정리는 코드 검토로 확인하고 별도 실행 미검증 범위를 결과에 기록한다. 기존 FistCombat/StoneFist 회귀 PASS.
- 검증 명령: `./Scripts/VerifyFighterCombatStance.ps1 -Phase runtime|render|combat|stone -Label <새 이름>`. 격리 QA slot을 사용하며 label 중복을 거부한다.
- 되돌림 경계: 신규 label의 증거만 수집하며 기존 검사 결과 경로를 덮어쓰지 않는다.
- 결과 해석: NullRHI는 상태·포즈·판정 수치 검증이다. 육안 자연스러움은 그래픽 렌더를 별도로 확인해야 한다. 영상에는 일반 Idle과 전투 대기가 모두 드러나야 한다.

### P4 — 독립 검토와 기록

- 목표: 공격·카메라·기존 dirty 작업이 보존되고 이번 변경을 재현할 수 있게 한다.
- 입력: 통합 diff, Content 보존 해시, 실제 검사 결과와 렌더.
- 수정 위치: 결과 문서, `Docs/SprintChangeLog.md`의 `Sprint#5-9`, `DevLog/20261005.txt` append.
- 완료 조건: 코드 검증 1명과 동작 검증 1명의 독립 검토, 한국어 DevLog·Notion 요약 후보, 변경 및 제한 기록.
- 검증 명령: `git diff --check`, `./Scripts/CheckGeneratedChanges.ps1`, 허용 목록 밖 Content 해시 비교.
- 되돌림 경계: 이번 문서 추가분만 분리한다. 사용자 확인 없이 commit/push하지 않는다.

## 역할과 종료 조건

기획 1명, 개발 2명(루프 에셋 / 런타임 연결), 검증 2명(코드 / 실제 포즈·시야)을 사용한다. 동시 슬롯 제약에 맞춰 기획 후 검증을 순차 배치하고 모든 Editor mutation은 직렬 실행한다. 동일 실패가 두 번 반복되면 설계·검토로 돌아간다.

일반 Idle과 구분되는 가드가 실제 플레이어 Appearance Mesh에서 보이고, 기존 공격이 정상 재생하며, 손발·머리 카메라 안정성과 자동 복귀가 확인되면 완료한다. 가드 에셋 생성만으로 게임 적용 완료라고 판단하지 않는다.
