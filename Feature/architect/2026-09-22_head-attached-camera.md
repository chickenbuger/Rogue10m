# 머리 움직임을 따르는 전신 1인칭 카메라

- 작업: Sprint#4-33-head-attached-camera
- 요청: 카메라가 머리에 달린 듯 전신 동작을 따라가며, 달리기에서 함께 흔들리고 실제 뒤돌아차기에서는 후방을 본다.
- 변경 범위: C++ 카메라 표현, native 전신 애니메이션 머리 신호/이동 흔들림, 독립 런타임 검증과 실제 게임 영상. 에셋 바이너리 변경과 공격 입력 재배정은 범위 밖이다.

## 확인한 현재 상태

기본 전신은 Manny Simple, capsule 기준 mesh (-5,0,-94), yaw -90도, camera (20,0,76)이다. 머리는 neck_01 숨김으로 제거했고 ordinary world rendering을 사용한다. 기존 first-person 렌더 옵션을 전신에 다시 적용하면 재질의 CameraDepthFade/Head Cutout 때문에 몸에 구멍이 보일 수 있으므로 현 방식을 유지한다.

기존 카메라는 native Fist의 몸 신호를 최대 2cm/2도 정도만 최종 POV에 추가한다. SingleNode Martelo 시퀀스는 Fist 인스턴스가 아니므로 신호가 전혀 적용되지 않는다. native Basic은 aim pitch 보정 이전의 실제 head delta를 이미 내보내지만 Knuckle은 raw head 신호가 없다. 이동 중 기본 애니메이션은 idle이므로 기존 머리 추적만 켜도 달리기 흔들림은 발생하지 않는다.

## 설계

1. 최종 PlayerCameraManager POV에 전신용 head-follow를 분리한다. ControlRotation, CameraComponent의 고정 기준점, 공격 trace 입력은 바꾸지 않는다. 기존 arms-only의 작은 흔들림은 유지한다.
2. native Fist는 최종 몸 자세에서 숨김/마우스 aim 이전 head 위치와 회전 차이를 내보낸다. Basic과 Knuckle 모두 같은 계약을 사용한다. 원본 local pose를 직접 다루므로 숨긴 neck 아래의 renderer transform은 추적하지 않는다.
3. SingleNode는 현재 UAnimSequence의 현재 playback time에서 pose를 평가하여 head CS를 얻고 해당 클립 0초 pose를 기준으로 차이를 구한다. baked Manny non-additive 시퀀스가 이번 확인 대상이다. additive 또는 skeleton 불일치/불량 pose는 무효 처리하고 중립으로 복귀한다. 매 프레임 asset load, raw FBX 재추출, socket 부착은 하지 않는다.
4. head delta는 mesh yaw를 변환한 capsule-local 기준이다. 위치는 actor 회전으로 world에 합성하며 마우스 pitch로 돌리지 않는다. 회전은 actor 공간의 quaternion delta를 입력 view에 합성해 yaw 180도 경계를 연속적으로 통과한다. source 변경/무기 전환/사망/시점 전환 때 이전 기준과 필터 상태를 해제한다.
5. 달리기/걷기는 실제 수평 속도, grounded 상태, 이동 거리에서 phase를 구해 같은 신호를 spine_01 및 자식(가슴·어깨·머리) pose에 반영한다. 골반과 발의 기존 자세는 유지한다. 정지 시 진폭을 감쇠한다. 공중에서 보행 phase를 진행하거나 캡슐의 점프 world Z를 다시 더하지 않는다. 기존 점프 압축/착지 pose의 head delta는 그대로 카메라에 전달한다.
6. 현재 눈높이/앞쪽 간격은 유지하고 기준 head 위치 대비 delta만 반영한다. 회전 시 눈과 기준 머리 사이의 간격을 함께 회전하는 eye lever 보정을 포함해 뒤를 볼 때 눈이 가슴 앞에 남지 않도록 한다. eye lever는 최대 50cm, 전체 위치 변화는 기본 최대 65cm로 제한하고 실제 영상으로 몸 관통 여부를 확인한다. 새 offset/rotation gain과 응답 속도는 UPROPERTY로 조절한다.

## 실제 Martelo 회전 측정

2026-09-22, UE AnimPoseExtensions로 /Game/Rogue10m/Animation/Preview/Martelo/A_Martelo2_Manny의 40프레임을 읽기 전용 평가했다. 첫 프레임 대비 head quaternion 차이를 mesh yaw -90도 기준으로 변환했다.

- head yaw: -1.27 ~ 39.24도
- head pitch: -7.51 ~ 26.67도
- head 위치 최대 변화: 29.02cm
- head 위치 축별 span: 25.81 / 23.70 / 35.42cm
- root yaw 최대: 97.08도
- head forward dot 최소: 0.763 (후방을 향하지 않음)

따라서 Martelo 원본에 임의 180도 회전을 넣고 원본 움직임이라고 소개하지 않는다. 실제 영상은 원본의 약 39도 머리 회전을 따른다. 후방 회전 지원은 별도 임시 회전 시퀀스(0→180→200→복귀)를 메모리에 만들어 검증하며 원본 에셋을 수정하지 않는다.

측정 근거: tmp/head-camera/retarget-head-poses.json, tmp/head-camera/head-excursion-summary.json. 기능 완료 시 Feature/doc/evidence에 보존한다.

## Ultrawork Packets

| Packet | 목표/수정 영역 | 완료 조건 | 검증 명령 | 되돌림 경계 |
| --- | --- | --- | --- | --- |
| H1 | PresentationComponent의 native/SingleNode head-follow | 실제 pose delta, 입력 불변, 180도 연속 회전, 전환 초기화 | BuildEditor.ps1; Rogue10m.TestHeadCamera | 해당 컴포넌트 변경 |
| H2 | FistAnimInstance 이동 자세와 raw head snapshot | 속도/접지에 따른 동기 흔들림, 정지 감쇠, 점프 중복 없음 | Rogue10m.TestBasicBrawler smooth; 이동 실제 영상 | native AnimInstance 변경 |
| H3 | 실제 Martelo camera 지표와 별도 후방 회전 fixture | 실제 clip 움직임 추종, 후방 dot<0, yaw wrap 한프레임 튐 없음 | Rogue10m.PreviewMartelo; Rogue10m.TestHeadCamera | editor-only tests |
| H4 | 전신/legacy 회귀, 시각 검증, 기록 | 손/몸/다리 framing, 무기복원, 소스리뷰, 문서/로그 | TestFistPresentation views; TestArmsOnly; git diff --check | 문서/증거만 |

## 검증 주의점

기존 3도 이하 카메라 envelope는 legacy에 유지하고 전신은 새 계약에 맞춰 검사한다. 입력/카메라 컴포넌트 불변 검사는 유지한다. 실제 이동 속도가 0인데 테스트에서 임의 gait 강도를 주어 통과시키지 않는다. 원본 머리 회전이 없는 클립에 큰 camera yaw를 연출로 덧붙여 추적 기능이 완성된 것으로 간주하지 않는다. 동일 실패를 두 번 반복하면 설계 또는 review 단계에서 원인을 다시 확인한다.

역할은 기획 1, 개발 2, 검증 2로 구분하되 동시 실행 수 제한에 맞춰 기획 담당은 이후 독립 검증 역할로 전환한다. 최종 완료 문서, 한국어 DevLog/Notion 후보, SprintChangeLog를 갱신한다. 커밋/푸시는 사용자 지시 없이 수행하지 않는다.

## 완료 상태

H1~H4 완료. Editor 빌드, 후방/각도경계 1,446샘플, 기본480·너클120(시야3)·팔390·최종프리뷰419프레임 모두 통과. 독립 소스/시각 검토 및 한국어 기록 완료. 최종 결과는 짝문서를 참조한다.
