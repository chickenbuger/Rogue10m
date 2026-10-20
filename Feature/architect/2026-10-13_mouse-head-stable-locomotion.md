# 마우스 시선을 기준으로 머리를 안정화하고 상체·하체 동작을 분리한다

작성일: 2026-10-13
작업: `Sprint#5-30-mouse-head-stable-locomotion`
역할: 기획 1. 이 문서는 읽기 전용 소스 분석 결과와 구현 계약이다. 기획 담당은 코드, 엔진, 자산, Git 상태를 변경하지 않는다.

## 목표와 적용 범위

이전의 애니메이션 머리 회전을 그대로 따라 뒤를 보는 카메라 요구를 이번 요청으로 대체한다. 마우스가 시선의 기준이고, 애니메이션은 작은 잔여 움직임만 더한다. 걷기 2도, 달리기 4도, 공격 3도를 넘는 머리/시점 흔들림은 허용하지 않는다. 머리 스핀과 뒤돌아차기를 유발하는 클립은 활성 캐릭터 재생 경로에서 사용하지 않는다.

상체는 마우스 조준 방향을 따르고, 하체는 실제 이동 상태에 맞는 보행을 유지한다. 이동 재생 속도는 실제 수평 이동 속도에, 공격 재생 속도는 승인 당시 공격속도에 맞춘다. 카메라만 고정하고 외부에서 보이는 머리가 회전하는 상태도 완료로 보지 않는다. 1인칭에서 숨긴 머리의 평가 자세, 3인칭 점검 화면, 전신 그림자가 같은 머리 방향 계약을 따라야 한다.

이번 변경은 C++의 활성 appearance 카메라, Common AnimBP의 native proxy/이동·공격 데이터, 금지 동작 정책, 제한된 Editor 자산 연결과 검증에 한정한다. 기존 미커밋 작업, 원본 애니메이션, 엔진, 관련 없는 외형/HUD/맵은 보존한다. Preview 폴더의 원본 보관과 실제 활성 재생 허용은 분리한다. 에셋 편집이 필요한 구현 단계에서는 Unreal Editor API만 사용하며 이 기획 단계에서는 실행/저장하지 않는다.

## 현재 경로와 수정 지점

1. 실제 기본 경로는 `ARogue10mCharacter::CalcCamera` → `URogue10mAppearanceCameraComponent::UpdateCameraFromAppearance`이다. 외형 메시의 평가된 local pose를 합성하여 숨김 이전 머리를 읽으며, 별도 FirstPersonMesh는 숨기고 일시 정지한다. 이전 `URogue10mFirstPersonPresentationComponent`의 큰 머리 추종은 appearance 활성 시 CameraManager에서 우회한다. 따라서 오래된 FistAnimInstance만 수정해서는 이번 요청이 적용되지 않는다.
2. `ABP_Common_Unarmed`의 부모 `URogue10mFighterCombatStanceAnimInstance`는 `FRogue10mViewAlignedUpperBodyProxy`를 사용한다. proxy가 최종 graph를 한 번 평가하고 spine_01~03/upperarm에 조준 회전을 준 뒤, neck/head는 원래 authored component-space 회전으로 복원한다. 현재 머리가 마우스를 따르지 않는 지점이다.
3. AppearanceCamera는 ref head 대비 authored 회전 `WorldHeadMotion`을 ControlRotation에 곱한다. 무기 Unarmed/Knuckle에만 weight 0.15, 최대 8도를 적용하고 다른 무기는 최대 180도를 허용한다. 위치는 animated head의 전역 위치를 그대로 사용한다. 새 proxy에서 머리에 mouse aim을 넣고 기존 합성을 유지하면 mouse aim이 두 번 들어간다.
4. Common AnimBP는 FootIK→guard→공유 locomotion cache와 DefaultSlot→공유 montage cache를 가진다. `MovingAttackUpperBodyAlpha`가 지상 이동 공격에 spine_01/ik_hand_root 마스크를 선택한다. 현재 공중 공격/회피는 authored full-body 예외이며, 정지하면 0.12초에 원래 공격으로 복귀한다.
5. Combat는 `AttackSpeedMultiplier * SkillData.AnimationPlayRate`로 montage를 재생하고 첫 타격 지연도 같은 배율로 나눈다. 다중 타격 간격/콤보/재사용 시간도 기존 공속 계약이 있다. 재생 전에 자원과 쿨다운을 소비하고, 재생 실패해도 타격 시퀀스가 시작되므로 금지 클립 검사 위치가 중요하다.
6. 활성 montage가 안전 대체로 바뀌면 `AcceptedSkill->AttackMontage`와 실제 active montage가 다르다. stance의 하체 마스크, 공격 흔들림 상태, 타격 시점은 실제 승인된 presentation montage와 속도를 함께 참조해야 한다.

## 머리, 몸, 카메라의 합성 계약

기준 머리 자세는 현재 애니메이션의 첫 프레임이 아니라 skeleton reference pose의 component-space 방향이다. 메시의 앞축 차이는 mesh/actor quaternion으로 변환한다. 매 프레임 authored head의 변화는 reference 대비 residual로 분리한다.

- **하체:** locomotion/FootIK가 pelvis, thigh, calf, foot을 소유한다. 이동 지상 공격은 upper-body montage만 덮는다. 조준 처리로 root/pelvis/다리의 local T/R/S를 변경하지 않는다.
- **상체:** 기존 완만한 상체 조준 보간을 유지한다. 어깨와 손은 aim과 공격에 반응한다. IK helper는 최종 FK 손과 일치시키며 render bone의 길이/scale을 바꾸지 않는다.
- **목/머리:** 상체 조준과 공격이 적용된 최종 부모 회전에서 역산한 local rotation을 기록하여, 원하는 head CS가 `reference + exact mouse aim + bounded authored residual`이 되도록 한다. 머리에는 상체 조준 지연을 그대로 적용하지 않는다. neck은 ref 기준으로 안정화하고 해부학적 목 연결을 유지한다.
- **카메라 회전:** ControlRotation을 정확히 한 번 사용한다. native proxy에서 공유한 residual만 합성하거나, 최종 head에서 이미 포함된 mouse basis를 제거한 residual을 사용한다. 단순 `raw head delta * ControlRotation`은 금지한다. Character ControlRotation 자체를 애니메이션으로 수정하지 않는다.
- **위치:** reference head rest anchor + 실제 actor/capsule 이동을 기준으로 한다. authored head의 body-local 위치 변화만 작은 범위로 제한/감쇠한다. capsule의 점프, 경사 이동, 순간이동을 bob과 섞어 필터링하지 않는다. eye offset은 기존 외형에 맞춘 값으로 유지하되 불필요한 큰 eye lever 회전은 제거한다.
- **동일 프레임:** camera, combat trace, inspection의 반복 read가 필터를 여러 번 진행시키지 않아야 한다. 상태 보간은 프레임당 한 번만 수행하고 결과를 공유한다. DeltaTime=0, 반복 read, 병렬 애니메이션 평가, mesh/LOD 변경을 검증한다.
- **검사 뷰:** 1인칭에서는 머리를 계속 숨기되 그 평가 자세는 위 계약을 유지한다. 3인칭 inspection에서는 머리를 드러내 마우스를 따르는지 확인한다. shadow proxy/leader pose도 동일한 최종 머리 자세를 사용한다.

## 흔들림 제한과 기본값 가정

사용자가 지정한 각도는 각 Euler 축별 상한이 아니라 **마우스 기준 residual quaternion의 총 각도 상한**으로 해석한다. Pitch/Yaw/Roll이 동시에 들어와도 전체 흔들림은 상한을 넘지 않는다. 이 해석은 대각선 방향에서 2도가 3.46도로 늘어나는 문제를 막는다. 값은 상한이며, 매 동작마다 그 크기까지 억지로 흔들지는 않는다.

| 상태 | 최종 회전 상한 | body-local 위치 잔여 기본 상한 | 상태 판정 |
| --- | --- | --- | --- |
| 정지·걷기 | 2도 | 1cm | 지상, sprint 아님 또는 속도 거의 0 |
| 달리기 | 4도 | 2cm | 실제 지상 이동 + sprint 상태 |
| 공격·차징·공격 blend-out | 3도 | 1.5cm | 승인된 안전 공격 montage가 실제로 기여함 |
| 점프·착지·회피 | 3도 | 1.5cm | 공중/착지 반응/안전한 회피 표현 |

걷기/달리기와 공격이 겹치면 최종 3도 상한이 우선한다. 별도 hit feedback이나 recoil을 추가한 뒤에도 최종 상한을 다시 검사한다. 마우스의 큰 움직임 자체는 제한하지 않는다. 달리기에서 공격으로 전환할 때 smoothing의 이전 상태가 4도 남아 3도를 넘지 않도록 최종 clamp가 필요하다.

기본 residual 반영 비율은 기존 안정화의 0.15를 출발점으로 유지하고, 위치/잔여 회전의 안정화 응답은 초당 18로 가정한다. 상체 조준 응답도 기존 18을 유지한다. 상태 변경에는 약 0.1초의 부드러운 감쇠를 사용하되 상한은 즉시 지킨다. 이동 분류는 단순 입력 키가 아니라 현재 실제 속도와 `IsSprinting()`을 사용한다. 이동/공격 속도의 상한을 바꿀 목적은 없으며 기존 gameplay clamp를 유지한다. 실제 화면에서 튜닝을 바꾸면 결과 문서에 이유와 측정값을 기록한다.

## 위험 동작 정책과 안전 대체

런타임은 문자열에 `Kick`이 있는지만 검사하지 않는다. 검토된 명시적 분류를 Data Asset의 설정 또는 중앙 allow/deny policy에 둔다. 예시 분류는 `Unreviewed`, `StableViewAligned`, `SpinningOrBackKick`이며 정책 명칭은 구현 담당이 기존 타입 구조에 맞춘다. 움직임 종류와 안정성 평가를 구분하여, 이름이 펀치인 큰 회전 클립도 위험으로 표시한다.

기존 근거를 재사용하되 현재 바인딩을 다시 확인한다. `2026-10-02_close-combat-animation-preparation.md`에서 StraightKick은 골반 약 177도, LowRightThaiKick은 약 90도, Martelo는 약 120.88도 변화로 보류했다. 이전 Martelo head yaw 약 39도와 pelvis 변화는 다른 측정이다. `2026-10-02_fighter-skill-animation-binding.md`에는 태산압정의 `AM_Knuckle_Special/MM_Attack_03`이 아직 남아 있다고 기록되어 있어 우선 감사 대상이다. 회피의 MM_Dash와 신규/중첩 montage segment도 확인한다.

정책 흐름:

1. 입력으로 SkillData를 결정한 뒤 **자원·쿨다운·타격 예약 전에** presentation을 resolve한다.
2. 명시적으로 안전한 원본은 유지한다. 금지 클립이면 같은 공격 목적의 검증된 정면 잽/스트레이트/훅/비회전 내려찍기 등 안전 대체를 선택한다. 뒤돌아차기를 카메라 clamp만 씌워 계속 사용하지 않는다.
3. 대체 clip의 skeleton, additive/root motion, 구간 방향, 길이, 접촉 시점과 피해 pulse 수를 검증한다. 다른 길이의 클립에 공속만 곱한다고 원래 타격 시점이 맞지는 않는다. 필요하면 Editor에서 대체 파생 클립의 길이/접촉 지점을 기존 스킬 계약에 맞춘다.
4. 검증된 대체가 없으면 원본 금지 동작으로 fallback하지 않는다. 해당 승인 요청을 부작용 없이 거절하고 구체적 로그를 남긴다. 주요 활성 스킬을 단순히 모두 끄고 완료로 간주하지 않고, 이번 사용 범위에 안전 대체를 준비한다.
5. 실제 resolved montage/rate/duration을 승인 시 snapshot으로 남기고, stance/상하체 마스크와 공격 상태 판단이 그 값을 사용하게 한다. 데미지·소모·입력 슬롯·타격 횟수·콤보 창의 의미는 유지한다.
6. `PlayCommonMontage`와 회피 호출에도 우회 방지를 적용한다. 테스트/Preview 전용 SingleNode 경로는 활성 게임 동작과 분리하고, 과거의 회전 데모를 완료 검증으로 재사용하지 않는다.

## 속도와 하체 재생

공격 속도는 입력 승인 시 고정한 `AttackSpeedMultiplier * AnimationPlayRate`를 montage와 첫 타격 예약에 일관되게 사용한다. 공격 도중 능력치가 변해도 이미 승인된 타격 시점을 임의로 바꾸지 않는다. 안전 대체가 별도 길이를 가지면 그 시간 변환을 승인 snapshot에 명시한다.

Common BP의 BS_Idle_Walk_Run 입력은 실제 수평 속도이고, BlendSpace sample/reference speed에 맞는 재생 비율을 전달한다. 속도가 배가 될 때 보행 phase도 실제로 빨라져야 하며 단지 walk→run 포즈만 바뀌는 것으로 검증을 끝내지 않는다. stop은 idle로 복귀하고 공중에서는 ground gait 진행을 멈춘다. FootIK와 공유 cached pose를 보존하고 slot을 중복 평가하지 않는다. 새 구현에서 하체를 idle로 덮거나 고정해서 흔들림을 제거하지 않는다.

## Ultrawork Packets

| Packet | 목표·수정 영역 | 완료 조건 | 검증 | 되돌림 경계 |
| --- | --- | --- | --- | --- |
| P1 | Native proxy/AnimInstance: mouse head와 torso aim 분리 | visible head와 source head가 Control 기준 2/4/3도 안, 하체 bone 보존 | synthetic pose + 실제 inspection, LOD/정지/걷기/달리기/공격 전환 | proxy/AnimInstance 변경만 |
| P2 | AppearanceCamera: residual 1회 합성·position 안정화 | double aim 없음, 반복 trace/camera read 동일, 큰 위치 튐 없음 | 반복 read/Delta0/±yaw wrap/mesh 교체/실제 이동·점프 검사 | camera 컴포넌트 변경만 |
| P3 | Gameplay presentation 정책·대체·속도 | 금지 동작 활성 재생 0, 주요 스킬 안전 대체, hit timing 보존 | 현재 binding audit, deny/unknown/replacement 경로, 공속0.5/1/2배 | policy/Combat snapshot 및 지정한 파생 assets |
| P4 | Common BP locomotion 재생 속도 | 실제 이동속도에 따른 발/phase 변화, 이동 공격 때 하체 보행 | graph read-only audit, walk/run속도비교, FootIK/stance 회귀 | 지정 BP graph 변경/참조만 |
| P5 | 독립 소스·실제 화면 검증과 기록 | UE build, 회귀 통과, 카메라·머리 상한 지표와 영상, DevLog/Notion 후보 | BuildEditor.ps1, runtime commands, git diff --check | 기능 문서·검증 코드·증거 |

역할은 기획 1, 개발 2, 검증 2로 분리하고 동시 실행 제한 때문에 검증 역할을 순차 전환할 수 있다. 같은 실패가 두 번 반복되면 같은 수정을 재시도하지 않고 기획/리뷰 단계로 돌아간다. 소스 빌드는 관련 변경이 모인 후 시행하며 기존 검증을 불필요하게 반복하지 않는다.

## 검증과 종료 조건

- 새로운 bounded-head runtime 검증 명령을 추가하거나 기존 AppearanceCamera/AnimationStability 검증을 확장한다. 실제 사용할 명령 이름은 구현 후 결과 문서에 확정한다.
- 기준 Control quat와 rendered camera quat의 차이, visible head의 reference-forward 기준 차이를 각각 측정한다. 두 검사 모두 필요하다.
- 걷기/달리기/정지공격/이동공격/차징/공격 blend-out/점프·착지/회피/inspection/무기 전환/죽음/창 UI 입력차단을 포함한다.
- 시선 pitch 0/-70/+70, yaw 179→-179 입력에도 마우스 추종은 한 번이며 animation residual은 설정 상한 이하다.
- 위치 잔여량은 body-local로 측정한다. actor 이동/점프까지 빼먹고 카메라가 가만히 있는 구현을 통과시키지 않는다. 정지 후 residual 감쇠, capsule world travel 즉시 반영, 같은 프레임 재호출 동일 결과를 검사한다.
- 금지 분류 source/segment가 실제로 재생되지 않는지 확인한다. 안전 대체 실패 시 비용/쿨다운/타격 예약이 없는지 검사한다. 실제 resolved montage가 moving upper-body 판정에 쓰이는지 확인한다.
- 공속0.5/1/2배의 montage time과 contact 시간, 실제 수평 이동속도 변화에 대한 gait phase/하체 foot travel을 비교한다. 검사 편의를 위해 키 입력과 실제 속도를 혼동하지 않는다.
- 기존 moving upper-body/stance/appearance camera/shadow/animation stability/combat의 관련 회귀만 실행한다. 원본/무관 assets hash와 Git 상태를 전후 비교한다.
- 최종 문서에 이번 기능 파일 변경, 실측 상한, 실제 게임 영상, 알려진 한계를 기록한다. DevLog/20261013.txt 한국어 append, Notion 후보, Docs/SprintChangeLog.md를 통합 담당이 갱신한다. 커밋/푸시는 하지 않는다.

## 제작 중 기획 수정 — 파생 클립의 루트 기준 높이 보존

첫 적용 시도는 저장 전에 접촉 위치 검사에서 중단됐다. 읽기 전용 재검사에서 현재 `A_Knuckle_Uppercut`과 `A_Knuckle_ShortBlow`의 root 위치가 시작·접촉·종료 모두 `[-1.51228, 1.66641, 90.76727]cm`로 같음을 확인했다. 길이 90.79516cm인 이 값은 루트 이동량이 아니라 원본 스탠스의 기준 위치이며, 최초 접촉 검사 오차 90.79517cm와 일치한다. 이를 0으로 만드는 초안은 몸과 발의 높이를 바꾸므로 철회한다.

새 파생본은 첫 평가 포즈의 **root local translation을 그대로 유지하고 이후 travel만 고정**한다. root 회전·scale와 모든 다른 본의 local T/R/S는 원본의 명시적 시간 매핑을 따른다. 현재 두 주먹 소스처럼 root가 상수이면 원본 hand·head·pelvis·양발의 component 위치와 높이도 그대로 유지돼야 한다.

접촉 검사는 손의 root 상대 전체 T/R/S를 비교하며 위치 0.20cm, 회전 0.05도, scale 0.0001 기준을 유지한다. 매 프레임 구조 본의 component 위치도 원본 대비 0.01cm 안인지 검사하고, 제거한 동적 root travel과 시작·접촉·종료 높이를 별도 기록한다. 이 검사가 모두 통과하기 전에는 새 파생본이나 DA를 저장하지 않는다. 원본 시퀀스와 기존 DA 수치·공속·타격 시각은 보존한다.

## 실행 중 기획 수정 — 실제 평가 시점과 이동 순서

공격 blend-out 마지막 프레임은 PreUpdate 이후 Montage update에서 상태가 종료된다. 머리의 회전 제한은 게임 스레드의 PreEvaluateAnimation에서 다시 읽어 실제 평가 포즈와 같은 상태를 사용한다. 조준 방향과 소스 컴포넌트 회전의 평가 스냅샷은 유지하고, 워커에서 UObject를 읽지 않는다. 검사 허용 상한을 완화해 종료 프레임을 넘기는 방식은 사용하지 않는다.

실제 기존 Fist 회귀에서 착지 프레임의 숨겨진 Animation Source Mesh가 이동 컴포넌트보다 먼저 평가되는 문제가 확인됐다. StylizedCharacter의 PostInitializeComponents에서 Movement→Source→Appearance 의존 순서를 연결한다. 집중 QA는 이 production 의존성이 이미 존재하는지 요구하며, 실제 Source에 테스트 전용 의존성을 추가하지 않는다. 대조 메시의 자체 시간·이동 의존성만 fixture가 구성한다.

대체 montage는 DA의 정적 참조로 연결하므로 AcceptedSkill의 montage와 실제 재생 montage가 일치한다. 별도의 runtime 대체 스냅샷은 불필요하며 기존 승인 당시 공속·타격 시각 계약을 유지한다. 600cm/s 이하의 보행은 현재 BlendSpace의 Idle/Walk/Jog 샘플 혼합을 유지하고 그 이상은 실제 속도/600으로 cadence를 증가시킨다.
