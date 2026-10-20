# 격투가 팔 휘두르기 강화 계획

## 목표와 범위

2026-10-05 요청은 현재 격투가 공격의 팔 움직임을 조금 더 크게 만드는 것이다. 팔을 당기는 준비, 훅의 좌우 원호, 어퍼컷의 아래에서 위로 올라가는 궤적을 강화한다. 기존 기본 공격 입력, 판정 시간, 피해량, 타수, 콤보 창, 재생 속도와 카메라 안정화를 유지한다. 머리 카메라는 Appearance Mesh의 실제 공격 포즈를 계속 사용한다.

조사 시작 기준 작업 트리는 `Sprint#5-7-fighter-skill-animation-binding`이며 기존 미커밋 작업이 있다. 구현은 같은 현재 상태를 보존해 생성한 `Sprint#5-8-fighter-arm-swing` 작업 브랜치에서 진행한다. 변경 전 파일과 실제 에셋 상태를 별도 증거 폴더에 기록하고 이번 파일만 추적한다. 이 계획 단계는 읽기 전용 조사와 본 문서 작성만 수행했다.

확정 범위는 맨손/너클 authored 9개와 금강쇄 파생 1개, 총 10개 동작이다. 유운수·무극패왕권·태산압정은 유지한다. 기존 접촉 목표/시간, 장타력, 재생 속도, 몸통 turn 및 head 보정은 보존하고 팔 준비·원호·follow-through만 강화한다. 기존 명령에 opt-in 모드를 추가하고 `Scripts/Editor/EnhanceFighterArmSwing.py`에서 baseline/apply/reload 감사를 수행한다.

## 확인한 실제 재생 경로

- `Rogue10mCombatComponent::ExecutePendingAttackSkill`에서 실제 DA의 `AttackMontage`를 `AttackSpeedMultiplier * AnimationPlayRate`로 `Character::PlayCommonMontage`에 전달한다.
- `GetAnimationPlaybackMesh()`는 애님 인스턴스가 있는 `GetMesh()`를 반환한다. Appearance Camera 활성 상태에서는 숨긴 FirstPersonMesh의 별도 재생을 생략한다. 따라서 이전 FirstPerson 표현 전용 `Rogue10mFistAnimInstance`의 팔 보정값만 바꾸는 작업은 현재 Appearance Mesh의 공격 개선 근거가 되지 않는다.
- 네 기본 맨손 공격과 다섯 너클 공격은 `FistAuthored/A_*`, `AM_*`에서 재생된다. Editor 전용 `Rogue10mFistCombatAuthoring.cpp`는 원본 왼 잽의 가드 포즈에서 120Hz로 해당 클립을 굽는다.
- 유운수와 무극패왕권은 각각 Close Combat 왼 훅과 기존 charged punch를 별도로 파생한 `FighterSkills` 클립이다. 기존 authored 궤적 조정 범위에 자동으로 포함되지 않는다.
- 금강쇄 `A_Fighter_IronChain_TwoPunch`는 `A_Knuckle_HookFlurry`의 두 권을 별도 베이크한 클립이다. HookFlurry 재생성만으로 이미 저장된 금강쇄는 갱신되지 않는다. 같은 source-time map을 사용해 금강쇄 파생본도 갱신해야 연타 변화가 해당 스킬에 반영된다.
- 머리 카메라 기준은 `Rogue10mAppearanceCameraComponent`: 초기 pitch -20도, 머리 회전 반영 비율 0.15, 최대 회전 8도다. 이전 FirstPersonPresentation 설정과 혼동하지 않는다.

## 궤적 설계

전방 목표를 무작정 멀리 놓으면 현재 `Reach * 0.94` 제한에 걸려 차이가 작아진다. 최종 접촉 위치를 중심으로 준비와 회수의 이동 폭을 늘린다. 다음 값은 구현 후 실제 포즈 측정으로 확정할 후보이며 검증 한도를 넓히는 근거로 쓰지 않는다.

| 공격 | 강화 방향 | 시작 후보 |
|---|---|---|
| 왼·오른 잽 / 연환권 | 주먹을 어깨 쪽으로 짧게 당겼다가 기존 정면 목표로 내지르기 | 후방 준비 3~5cm, 측면 1~3cm |
| 스트레이트 / 붕권 | 팔꿈치를 과하게 펴지 않는 후방 로딩과 빠른 뻗기 | 후방 준비 5~7cm, 기존 접촉 목표 유지 |
| 오른 훅 | 바깥에서 안쪽으로 들어오는 원호 확대 | 기존 phase 원호 24cm에서 약 31cm로 |
| 백열난무 / 금강쇄 | 좌우 주먹이 교대하는 폭 확대, 접합 가드 유지 | 기존 phase 원호 13cm에서 약 20cm로 |
| 승룡권 | 아래로 모았다가 위로 치솟는 폭 확대 | 기존 하방 준비 36cm에서 약 44cm로 |
| 축경권 | 작은 후방 로딩을 추가해 짧은 강타의 방향 변화 강조 | 후방 준비 약 5cm |

준비와 follow-through 가중치는 Activity=0 및 접촉 phase=0에서 0이 되는 매끄러운 함수로 작성한다. 접촉 목표, 판정 순간과 지속 시간은 유지한다. 흉추 회전은 기존 authored 설정을 유지하고 팔 이동만으로 차이를 확인한다. 반대 손은 가드에 남으며 연타 겹침 중 팔꿈치가 반전되지 않아야 한다.

## 안정성 계약

- render bone local translation과 scale, 원본 사지 길이를 보존한다. 공격 뼈 회전만 기록한다.
- 닫힌 주먹의 wrist local rotation과 finger pose를 보존한다. 손 방향 확대를 손목 비틀림으로 대체하지 않는다.
- TwoBoneIK의 stretch를 끄고 reach 0.94 제한을 유지한다. pole은 현재 가드 팔꿈치에서 Activity에 따라 연속 이동하며 Activity→0에서 원본 가드로 수렴한다.
- 손·발 IK는 해당 render bone 포즈에 맞춘다. 골반, 다리, 발의 authored 가드 포즈는 보존한다.
- 머리의 원본 component orientation 보정을 유지한다. authored head angle 0.01도 이하, head travel 5cm 이하와 실제 머리 카메라 8도 한도를 유지한다.
- 기존 120Hz 연속성 검사: 최대 팔꿈치 한 프레임 이동 8cm, 최대 팔 회전 한 프레임 차이 30도, 활성화 경계 팔꿈치 0.6cm 및 회전 2.5도 한도를 유지한다.
- 새 궤적은 동일 접촉 시간에서 기존 전방 도달 검사와 훅·어퍼컷 구분 검사를 통과해야 한다. 증가량은 동일 시간 샘플의 궤적 길이 및 준비 구간 측면/수직 이동 폭으로 전후 비교한다. 판정점/기존 카메라와의 단순 거리만으로 공격 강화 완료를 주장하지 않는다.

## Ultrawork Packets

### P1 — authored 궤적 강화

- 목표: 맨손/너클 9개 공격의 준비와 휘두르기 폭을 키운다.
- 입력: 변경 전 C++ 및 9개 시퀀스의 120Hz 포즈, 현재 DA timing.
- 수정 위치: `Source/Rogue10m/Editor/Rogue10mFistCombatAuthoring.cpp`의 opt-in 강화 모드.
- 완료 조건: 포즈 변화가 측정되며 가드 수렴, IK 제한, 뼈 길이, 기존 접촉 순간을 보존한다.
- 검증 명령: `./Scripts/BuildEditor.ps1`, 좁은 Editor authoring/pose audit.
- 되돌림 경계: 이번 C++ diff 및 허용된 authored 시퀀스/몽타주의 사전 백업. 이전 사용자의 dirty 파일을 Git 기준으로 되돌리지 않는다.

### P2 — 실제 적용과 금강쇄 파생 유지

- 목표: 실제 재생하는 클립을 갱신하고 금강쇄에도 같은 넓어진 연타를 반영한다.
- 입력: 현재 `FistAuthored` 9개, 금강쇄 source-time mapping, 기존 DA/profiles snapshot.
- 수정 위치: `Scripts/Editor/EnhanceFighterArmSwing.py`, 허용된 authored 에셋 및 금강쇄 파생 시퀀스. 바이너리는 Editor API로만 저장한다.
- 완료 조건: 재로드 후 실제 DA는 같은 montage/timing을 가리키며 금강쇄 접합의 기존 local position/rotation 기준을 통과한다.
- 검증 명령: `Scripts/Editor/EnhanceFighterArmSwing.py`의 baseline/apply/reload 감사, `Scripts/Editor/BindFighterSkillAnimations.py`의 기본 read-only 검사.
- 되돌림 경계: 이번 허용 에셋만 백업 복원. 유운수/무극패왕권/태산압정 원본과 미관련 DA는 이번 대상에서 제외한다.
- 실행 주의: 기존 `PrepareFistCombatAnimations.py`는 9월 29일 증거를 덮어쓰고 DA 설명도 저장하며 종료 시 Editor를 닫는다. 그대로 재실행하지 말고 이번 evidence 경로와 허용 저장 목록을 둔 적용 경로를 사용한다. 기존 binding 생성 모드는 이미 있는 결과를 거부하므로 전체 재생성 플래그를 사용하지 않는다.

### P3 — 기능 검증과 육안 확인

- 목표: 실제 입력/피해와 팔·카메라 안정성을 확인한다.
- 입력: P2 저장 결과와 변경 전 metrics, 격리 QA save slot.
- 수정 위치: `Feature/doc/evidence/fighter-arm-swing-20261005`의 새 증거.
- 완료 조건: 9개 공격 회귀 및 금강쇄 실제 판정 PASS, 머리 카메라 cap/뼈 길이 PASS. 가능한 렌더에서 1인칭과 V 3인칭으로 팔꿈치/손목/발 왜곡과 주먹 화면 가림을 확인한다.
- 검증 명령: `./Scripts/VerifyFighterSkillAnimations.ps1 -Phase combat -NullRHI -Label fighter-arm-swing-20261005-combat -TestSlot Rogue10m_Fighter_QA_20261005_ArmSwing`; `-Phase stone`은 별도 label과 격리 slot으로 실행한다. 그래픽 경로는 새 label을 사용한다.
- 되돌림 경계: 검사 전 기존 증거를 백업한다. 기존 QA 실행 도구가 고정 evidence 폴더를 쓰는 경우 이번 실행 결과를 새 폴더에 수집하고 과거 증거 바이트를 보존한다.
- 결과 해석: NullRHI PASS는 실제 포즈/판정/카메라 수치 검증이며 시각적 자연스러움이나 픽셀 접촉 확인을 뜻하지 않는다. 렌더가 없으면 그 한계를 문서와 최종 답변에 명시한다.

### P4 — 검토와 기록

- 목표: 이번 차이가 좁고 재현 가능하며 원래 작업이 보존됨을 확인한다.
- 수정 위치: 결과 문서, `DevLog/20261005.txt` append, `Docs/SprintChangeLog.md`의 `Sprint#5-8` 작업 항목.
- 완료 조건: 독립 diff review, 실제 결과/제한 기록, 변경 목록과 관련 증거 연결.
- 검증 명령: `git diff --check`, `./Scripts/CheckGeneratedChanges.ps1` 및 기준 파일 hash 비교.
- 되돌림 경계: 이번 문서 추가분만 제거 가능한 단위로 기록한다. 사용자의 별도 확인 없이 commit/push하지 않는다.

## 역할과 종료 조건

기획 1 / 개발 2 / 검증 2 역할을 사용한다. 동시 실행 슬롯 한도 내에서 검증 역할은 구현이 끝난 뒤 순차 투입할 수 있다. Editor 에셋 변경과 live MCP는 한 담당자가 직렬 수행한다. 같은 실패가 두 번 반복되면 확대량/검사 접합을 다시 설계한다.

종료 시 실제 재생 클립에 팔 확대가 반영되어야 하며 빌드·pose·runtime 결과를 기록한다. 머리 시야 안정성과 손·발 형태가 보존된다는 수치 근거를 함께 남긴다. 적용하지 못한 경로나 육안 미확인 구간은 완료로 계산하지 않는다.
