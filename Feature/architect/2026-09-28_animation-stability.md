# 캐릭터 흔들림과 손발 변형 원인 분리 및 안정화

작성: 2026-09-28 / 브랜치: Sprint#4-42-animation-stability / 역할: 기획1

## 요청과 성공 기준

사용자가 지적한 실제 몸 흔들림과 손발의 일그러짐을 확인하고 원인을 수정한다. 화면의 흔들림만 줄이는 카메라 감쇠를 해결책으로 삼지 않는다. 같은 애니메이션을 거치는 각 단계의 포즈를 비교해 변형이 처음 생기는 지점을 입증하고, 그 단계의 변환·리타깃·IK·본 설정을 최소 범위로 고친다.

Appearance Mesh의 머리에 연결된 카메라, 원본 몸 동작을 보는 1인칭, V키 전신 확인, 좌클릭 두 잽과 우클릭 짧게/길게 입력은 유지한다. 화면을 예쁘게 만들기 위한 별도 FP Mesh 재활성화, 손만 재배치하는 시야용 IK, 판정 범위 확대, 카메라 추종 중지는 기본 해결 범위에 포함하지 않는다.

## 기존 결론의 재검증

이전 Appearance 실행의 최대 head 회전 약 152.90도와 이동 약 82.46cm는 **최종 Appearance에서 관측한 값**이다. 그것이 FBX 원본의 의도된 동작이라는 인과관계는 입증되지 않았다. 이번에는 해당 해석을 전제로 사용하지 않는다.

기존 `ValidateBoxingRetarget.py`는 FBX→Manny의 손·발·골반 위치 궤적 상관, 피크 시점, 좌우 우세를 주로 검사했다. 손발 관절의 회전·twist·scale, 전체 피부 변형, Manny→Appearance의 두 번째 리타깃까지 검증한 자료가 아니다. 이전 카메라 cache 오차 0은 카메라가 계산식과 일치한다는 뜻이며, 입력 pose 자체가 정상이라는 뜻은 아니다.

## 변경 전 보존

- Source, Config, 관련 제작 스크립트와 리타깃·Skeleton·AnimBP·원본/파생 애니메이션·스킬 애셋의 기준 목록과 SHA-256을 기록한다.
- 수정 대상 `.uasset`은 먼저 별도 작업 백업에 복사하고, 에디터 API를 통해서만 수정·저장한다. 직접 바이너리 편집은 금지한다.
- 기존 dirty 변경은 그대로 유지한다. 사용자 원본 FBX/영상 및 재생성 입력도 유지한다.
- 기존 tmp labeled 프레임 일부는 직전 정리로 제거되었다. 보존 영상·원본 프레임·복원 매니페스트를 활용하되 과거 프레임을 이번 코드의 새 실행 결과로 표시하지 않는다. 새 증거는 별도 디렉터리에 생성한다.

## 원인 분리 순서

### 1. 증상 재현과 입력 고정

실제 사용 Pawn의 asset/AnimClass/retarget 설정/본 계층/component transform을 기록한다. Idle, 걷기·정지, 점프, 왼잽, 오른잽, 스트레이트, 훅을 구분해 전신 고정 관찰 시점과 1인칭을 촬영한다. V 뒤쪽 관찰로 식별하기 어려운 손목·발목은 검증 전용 측면/정면 관찰을 사용한다.

초기 시선, capsule 위치, 시간 간격, 재생 속도, montage position, slot/weight, 공격 serial을 기록한다. 카메라가 크게 움직였다는 관찰과 메시 자체가 변형됐다는 관찰을 분리한다. Temporal AA/모션 블러 잔상으로 뼈 변형을 오인하지 않도록 한정된 진단 캡처에서 비활성 비교하고 최종 사용자 설정은 보존한다.

### 2. 단계별 동일 시각 비교

| 단계 | 비교 대상 | 분리할 원인 |
| --- | --- | --- |
| S0 | FBX의 실제 활성 take, 평가된 local/global transform | 단위·축계·pre/post rotation·부모 scale·원본 동작 |
| S1 | 변환된 Manny 원본 sequence를 단독 재생 | import/bake 좌표계, Skeleton 기준 pose, translation retarget, 회전 순서 |
| S2 | 실제 좌우 파생 sequence와 montage 재생 | 시간 재매핑, 미러 좌우 대응, additive 설정, root/hip 이동, blend 구간 |
| S3 | AnimationSourceMesh의 기존 ABP 최종 pose | locomotion/slot 합성, IK/ControlRig, additive 중복, 원본과 실제 montage weight 차이 |
| S4 | Appearance Retarget ABP 최종 pose | IK Rig/Retargeter chain, retarget pose, IK goal, root scale, post-process/본 계층 차이 |
| S5 | 렌더링된 Appearance와 head camera | hide bone의 영향, skinning/weight, tick 지연, camera reference 변환·좌표계 |

모든 단계가 같은 Skeleton이 아니므로 절대 좌표나 Euler 각을 그대로 빼지 않는다. 단위를 cm로 정규화하고 축계 변환을 명시한다. 리그별 중립/retarget pose와 부모 좌표를 기준으로 quaternion 상대 회전, 관절 체인 길이 대비 끝점 위치를 비교한다. 실제 play rate와 원본 시간 재매핑 함수를 사용해 같은 동작 위상의 샘플을 대조한다.

### 3. 진단 항목

- root/pelvis/spine/head, clavicle/upperarm/lowerarm/hand 및 twist, thigh/calf/foot/ball, 필요한 손가락의 local translation·rotation·scale과 component/world pose.
- scale 0·음수·비균일 scale, NaN, quaternion 비정규화 및 같은 rotation의 부호 반전에 따른 가짜 큰 회전. frame delta는 quaternion 최단 회전각으로 계산한다.
- upperarm–elbow–wrist, thigh–knee–ankle 체인 길이 및 기준 길이 대비 변화, 손목/발목의 부모 상대 twist와 끝점 속도·가속도의 불연속.
- 발이 지면에 고정되는 구간과 의도된 스텝 구간을 구분한다. 발 이동을 무조건 오류로 취급하거나 원본 스텝을 고정시키지 않는다.
- root motion 비활성 상태의 실제 pose root/hip translation과 capsule 이동을 분리한다. 동일 이동·회전을 두 번 적용했는지 확인한다.
- additive sequence 여부, additive base pose/space, montage slot의 적용 순서, post-process AnimBP/ControlRig/IK 중복 적용을 확인한다.
- Source→Appearance tick prerequisite, 병렬 평가 완료 시점, pose frame 번호와 CameraManager cache의 관계. 숨긴 head를 다시 읽을 때 visibility scale이 섞이는지 확인한다.
- 좌우 미러의 본 이름 대응과 반사축은 체형 기준으로 확인한다. 한 손만 틀어졌다면 오른잽 파생만 별도 검사한다.

특정 가설은 한 번에 하나의 진단 우회/설정 변경으로 A/B 비교한다. 실패한 접근이 두 번 반복되면 같은 변경을 되풀이하지 않고 원인 분리 단계로 돌아간다.

## 수정 원칙

증상이 최초로 생긴 단계에 맞춰 최소 변경한다. 예를 들어 리타깃 전에는 정상이고 Appearance에서만 twist/scale이 깨지면 원본 클립을 가공하지 않고 해당 retarget pose/chain/IK 설정을 고친다. 원본 bake의 축계·pre/post 회전이 틀렸다면 import/제작 경로를 고치고 의존하는 파생 시퀀스를 에디터에서 다시 만든다.

카메라만 잘못됐다는 독립 증거가 있을 때에만 카메라 transform 계산을 수정한다. 몸 변형을 해결하지 않은 채 rotation/translation gain을 낮추거나 clamp를 걸어 검증 통과로 만들지 않는다. 의도된 몸 움직임의 크기와 기술적 변형을 구분하고, 시각적 선호에 따른 완화는 별도 선택으로 설명한다.

공격 타격 시각·입력·피해·범위는 유지한다. 원인 수정으로 머리 방향 자체가 정상화되면 실제 타격 방향은 자연스럽게 달라질 수 있으므로 독립 충돌 질의와 피해 결과를 다시 비교한다. 모든 공격이 무조건 정면 표적에 맞는다는 가정으로 시험을 작성하지 않는다.

## Ultrawork Packets와 역할

| Packet / 역할 | 목표·범위 | 완료 조건 | 검증 명령·자료 | 되돌림 경계 |
| --- | --- | --- | --- | --- |
| P1 기획1 | 위 가설과 단계 분리, 기준 보존 계획 | 이 문서 및 최소 재현 범위 확정 | 기존 소스/증거 읽기 | 계획 문서 |
| P2 개발1 | FBX→Manny/파생 montage와 source ABP 진단 | 원본 대비 이상 최초 발생 여부와 본·시각 수치 | FBX SDK/Editor pose extraction, 원본 분석 스크립트 | 제작 코드·원본 파생 애셋 |
| P3 개발2 | Manny→Appearance retarget/IK/scale/tick 진단 및 원인 수정 | 실제 변형 원인에 대응한 A/B 증거와 최소 수정 | Editor API 검토·pose 비교·실행 캡처 | Retarget/AnimBP/component 설정 |
| P4 검증1 | 독립 소스·메트릭 검토 | 검사가 원인 가정을 반복하지 않으며 기술적 오류 개선 입증 | 동일 시각 단계별 보고서, scoped diff | 검토 증거 |
| P5 검증2·root | 전신/1인칭 실제 QA, 빌드·회귀·문서 | 손발/몸 움직임 정상화와 기존 입력·V·카메라 보존 확인 | BuildEditor, 관련 runtime/asset 검사, 영상 | 검증 산출물 및 결과 문서 |

기획1 / 개발2 / 검증2 역할을 유지한다. 역할별 파일 소유권은 root가 정해 충돌을 막는다. 기획 담당은 이후 검증1로 전환한다. 원인 확정 후 root 승인으로 Source FootIK·훅 Editor 수정 도구도 분담했으며, 해당 변경의 최종 검토는 별도 검증2가 수행했다. 애셋 실행·저장은 root가 순차 수행했다.

## 완료 기준과 검증 한계

1. 수정 전후 같은 동작·동일 시점 자료로 어디서 이상이 생겼고 어떤 변경이 해결했는지 제시한다.
2. 새로 발생한 손목/발목 twist·본 길이 왜곡·비정상 scale·프레임 점프가 사라지거나 감소했음을 원본/기준 pose와 비교한다. 근거 없는 공통 임계값으로 정상 동작을 억제하지 않는다.
3. 기존 Appearance head camera와 V키 1↔3인칭, FP 비활성, Source/Retarget ABP 역할을 유지한다.
4. LMB 2타, RMB 짧게/홀드 release, Idle 복귀, 이동·점프·무기 전환을 관련 범위에서 재확인한다. 실제 일반 게임 경로와 pose 단독 재생을 구분한다.
5. 코드 변경 시 Editor 빌드, 애셋 변경 시 저장·새 프로세스 재로드·실행을 확인한다. 생성 경로/diff 검사, 비변경 공유 애셋 hash 보존을 확인한다.
6. 수정한 rig/캐릭터 종류와 아직 실행 검증하지 않은 종족·성별/네트워크/생명주기 범위를 명시한다. 이번 진단으로 근거가 바뀐 이전 설명은 결과 문서에서 정정한다.

결과는 `Feature/doc/2026-09-28_animation-stability.md`, 증거는 `Feature/doc/evidence/animation-stability-20260928/`에 둔다. root가 `DevLog/20260928.txt`에 한국어와 Notion 요약 후보를 append하고 `Docs/SprintChangeLog.md`를 갱신한다. 커밋·푸시는 하지 않는다.
## 독립 검증 이후 추가 원인 분리와 수정 후보

2026-09-28 baseline과 root-only 실행 비교로 Appearance pelvis 직계 자식의 번역 길이 늘어남은 retarget root 설정과 관련됨을 확인했다. root-only에서 thigh 로컬 길이비는 전 공격 1.0으로 복원되지만 발 급회전과 원본 상체 급회전은 남았다.

RMB 원본 비교에서 MM_Attack_03 훅은 1.56배로 재생되며 head/hand 큰 회전이 원본에 존재한다. 0.25초 진입 blend가 겹친 공격 0.2초 구간의 head step은 원본56.394° → Source82.841°였다. full blend에서는 상체 회전이 원본과 0.000111° 이내로 일치했다. 발은 Source 단계에서도 원본 대비 최대179.214° 차이가 남는다. 상세 증거는 `Feature/doc/evidence/animation-stability-20260928/independent-rmb-review.md`에 기록한다.

### ULW 추가 패킷: SourceABP FootIK 순서

- 목표: locomotion 지면 FootIK를 유지하면서 공격 원본을 FootIK 후처리로부터 보호한다.
- 수정 영역: `ABP_Common_Unarmed`의 기존 pose chain 3개 링크만 재연결. `upstream → Slot → FootIK → Root`에서 `upstream → FootIK → Slot → Root`로 변경한다.
- 사전 조건: 실제 링크를 Editor API로 조회해 정확한 원래 순서 확인, unique slot/rig/root 및 DefaultSlot·FootIK 자산 확인. 구조가 예상과 다르면 중단한다.
- 실행 도구: `Scripts/Editor/ConfigureBrawlerSourceFootIKOrder.py`. 기본은 read-only preview. 실제 적용 시 별도 명령행 `-Rogue10mApplySourceIKOrder`가 필요하다.
- 백업: 원본 uasset를 Content 밖 tmp에 읽기 복사하고 SHA256를 기록한다. 모든 변경은 Editor graph API로만 수행한다.
- 완료 조건: 다른 링크·노드·node tuning 보존, compile 성공, error node 없음, 새 topology 정확히 일치, 저장 성공. 이어 실제 런타임에서 공격 full-weight foot 원본→Source 오차 감소와 idle/이동 FootIK 유지 확인.
- 롤백: 저장 전 실패는 기존 링크 복구, 재컴파일, 미저장 종료. 저장 후 불만족은 보존 원본을 근거로 Editor에서 원래 3개 링크를 복구한다.

### 몽타주 blend 후보의 검증 경계

훅 전용 montage에서 blend-in을0.1초로 단축하면 원본의0.2초 급회전과 blend 종료가 분리된다. 다만 idle→공격 포즈 전환을 더 빨리 하므로 전체 head max가 원본56° 이하라고 사전 보장할 수 없다. 진입·타격·복귀 모든 프레임 A/B와 실제 영상으로 판단한다. 원본 공용 클립과 공용 montage를 무조건 덮어쓰지 않는다. 카메라 감쇠로 이 문제를 숨기지 않는다.

### Editor 적용 확인

`source-ik-order.json`에서 3개 링크 순서 변경·compile·저장이 성공했다. `hook-blend.json`에서 전용 훅 몽타주 blend-in만0.1초로 바뀌었고 DA gameplay 필드 및 공용 원본 clip/montage 해시는 보존되었다. 이 값은 적용 검증이며 플레이 품질 완료 조건을 대신하지 않는다. 최종 fixture는 기존4공격 전후 비교에 보행·점프 구간을 더하며, head camera와 V 전신 시점 요구를 계속 유지한다. 기존 원본의 과도한 상체 회전은 카메라 보정으로 숨기지 않는다.

## 최종 공격 수치 검증

after 공통0~329프레임에서 진입·복귀를 포함해4공격을 비교했다. 왼잽 Appearance 발 최대 회전은123.061/150.812°→23.875/17.183°, 오른잽·복귀는110.497/110.279°→20.619/16.049°로 감소했다. thigh_l 로컬 길이비는 모든 구간1.0이다. 훅 머리 최대는83.123°→57.771°로 줄었으나, 원본 오른손130.522° 회전 등 원본의 큰 동작은 유지되었다. 모든 회전 감소를 목표로 삼지 않는다.

Source FootIK 순서 변경 후 원본→Source full-weight 발 오차는 기존 최대179.214°에서0.000053° 이하로 해소되었다. 확정 결과·범위·남은 한계는 `Feature/doc/evidence/animation-stability-20260928/independent-after-review.md`에 기록했다. 추가 root/pelvis 재부모화는 이번에 적용하지 않았다.
