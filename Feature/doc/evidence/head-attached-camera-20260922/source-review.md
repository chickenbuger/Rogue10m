# 독립 소스 검토: 머리 추적 전신 카메라

검토 역할: 기획 담당에서 검증 1 역할로 전환. production 파일은 수정하지 않고 별도 synthetic runtime test만 추가했다.

## 확인한 내용

- native Fist는 최종 spine/어깨/머리 pose에서 마우스 aim 보정 전에 head delta를 계산한다. Basic과 Knuckle 모두 같은 getter 계약이다.
- 이동 신호는 CharacterMovement의 실제 수평 속도 및 지상 상태로 계산하며, spine_01과 자식만 보정한다. 이동하지 않을 때 위상은 정지하고 진폭은 감쇠한다. capsule의 점프 world 이동량을 다시 더하지 않는다.
- SingleNode는 GetAnimationPose와 현재 required bones로 숨김 이전 head CS를 평가하므로 숨긴 neck의 socket transform에 의존하지 않는다. non-additive, 동일 skeleton만 허용하며 평가 실패 시 중립 방향으로 감쇠한다.
- 위치는 actor 기준으로 합성해 아래보기 입력이 vertical bob을 전진 움직임으로 바꾸지 않는다. 최종 POV만 바꾸고 ControlRotation 및 CameraComponent 기준 위치는 유지한다.
- 눈-머리 간격을 회전하는 eye lever 보정은 회전 시 카메라가 가슴 앞에 남는 문제를 줄인다. MaxHeadCameraOffset 기본 65cm와 eye lever 50cm 제한은 실제 시각 검증을 병행해야 한다.
- legacy arms-only 카메라 경로와 full-body 경로는 분리되어 있다. 시점/무기/사망/강도 0 경로에서 reset하며 UObject 비소유 sequence/mesh cache는 TWeakObjectPtr를 사용한다.

## 발견 및 전달한 문제

1. 초기 구현은 normalized yaw에 gain을 곱한 뒤 quaternion smoothing을 적용했다. gain 1에서는 +180/-180 경계가 연결되지만 gain 0.5에서는 +89.5에서 -89.5로 target이 건너뛴다. CameraMotionScale, HeadCameraRotationScale.Yaw 및 MaxHeadCameraRotation.Yaw의 비기본 설정에서도 회전 연속성을 지키도록 runtime 개발 담당에게 전달했다. 수정은 source별 yaw 누적 unwrap을 gain보다 먼저 적용하고, 180도 미만 제한은 누적값을 같은 쪽에서 유지하는 방식이다. 기본 180도 설정은 전체 회전을 허용한다. sequence/native source 변경 또는 playback 되감김에 누적 기준만 초기화하고 filter 회전은 부드럽게 회복한다. 코드 수정은 확인했으며 확장 런타임 검증 결과를 아래에 기록한다.

## 검증 산출물

- Rogue10m.TestHeadCamera: transient reference-pose sequence에 0→180 유지→200→360 회전만 적용. 원본 Martelo와 분리한 synthetic fixture임을 로그에 표시한다.
- 0 / -70 / +35도 입력 pitch에서 실제 SingleNode 평가 및 PlayerCameraManager.UpdateCamera 경로를 검사한다.
- 후방 평면 forward dot, yaw 누적 360도, 한 프레임 quaternion 변화량, 원본 입력 불변, 카메라 컴포넌트 위치 불변, neck 숨김 유지, 강도 0 동작 및 AnimBP 복원을 검증한다.
- 실제 Martelo 원본 head yaw 범위는 약 -1.27~39.24도다. 180도 뒤보기는 이 원본 클립의 실제 동작이라고 주장하지 않는다.

최종 독립 소스 검토: PASS. 발견한 reduced-yaw 경계 문제는 수정 후 실제 런타임 검증을 통과했다.

## 초기 실행 결과 및 추가 검증

기본값 후방 회전 723 samples 통과: pitch 0/-70/+35 각각 rear dot=-1, continuous yaw=360도, 최대 한 프레임 변화 3도. 반강도, yaw gain 0.5, yaw limit 90도 조건과 source 변경 후 가드 복귀 검증을 추가했다. 확장 검증과 최종 회귀 결과는 통합 담당 실행 후 기록한다.

## 최종 독립 검증

최신 빌드 성공(27.01초) 뒤 실행한 tmp/head-camera/head-final.log에서 RESULT=HEAD_CAMERA_PASSED, failures=0 확인.

- 실제 SingleNode/PlayerCameraManager 경로 총 1,446 samples 통과.
- 기본 설정 3개 pitch: 후방 dot=-1.000000, 누적 yaw=360.000도, 최대 한 프레임 회전 3.000도.
- CameraMotionScale 0.5: 최대 step 1.500도, 음수 방향 반전 없음.
- HeadCameraRotationScale.Yaw 0.5: 최대 step 1.500도, 음수 방향 반전 없음.
- MaxHeadCameraRotation.Yaw 90: 최대 step 3.000도, 음수 방향 반전 없음.
- 각 제한 설정 후 native guard로 변경하고 명시 Reset 없이 회복하는 검사 통과.
- 강도 0 비활성화, 입력 불변, 카메라 컴포넌트 기준 위치 불변, 숨김 머리 유지 및 animation mode 복원 통과.

후방 회전 시험은 synthetic fixture이며 원본 Martelo yaw 39.24도의 실제 영상과 구분된다. 보행/전투/legacy 통합 회귀 및 최종 실제 영상 검토는 다른 검증 역할에서 별도로 수행한다.
