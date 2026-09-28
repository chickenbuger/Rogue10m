# 머리 움직임을 따르는 전신 1인칭 카메라

작업 브랜치: `Sprint#4-33-head-attached-camera`

## 반영 내용

전신 카메라는 애니메이션에서 평가한 머리의 위치·회전을 따른다. native 격투는 머리를 가리거나 마우스 조준을 적용하기 전의 포즈를 사용하고, SingleNode 시퀀스는 재생 시각의 원본 평가 포즈를 사용한다. 숨긴 목 아래의 렌더러 뼈 좌표를 읽어서 생기는 오류를 피했다.

실제 지상 이동 속도와 거리에 따라 상체·머리·팔에 같은 보행 흔들림을 넣었다. 정지 시 진폭을 감쇠하며 공중에서 보행 위상을 진행하지 않는다. 기존 점프·착지 머리 움직임은 반영하되 캐릭터의 실제 점프 높이를 중복해서 더하지 않는다. 발의 보행 애니메이션이나 접지 IK를 새로 추가한 작업은 아니다.

머리 기준 눈의 간격도 회전시켜 뒤를 볼 때 시점이 가슴 안에 남는 것을 방지한다. 위치 변화는 몸 기준으로 월드에 더하고, 회전은 쿼터니언으로 보간한다. 입력 ControlRotation과 카메라 컴포넌트 기준점은 유지하며 최종 렌더링 시점만 변경한다. 팔 전용 모드의 기존 작은 흔들림은 유지한다.

## 조절값

PresentationComponent의 Head Camera 설정: 활성화, 위치 강도 1, 회전 강도 pitch 0.75/yaw 1/roll 0.5, 최대 이동 65cm, pitch 55/yaw 180/roll 25도, 반응 속도 24. yaw 180은 회전 자체를 제한하지 않아 한 바퀴도 같은 방향으로 이어진다. 낮은 yaw 감도는 누적 회전량에 적용하므로 중간 자세가 달라질 수 있고 native 가드 복귀 시 중립으로 돌아온다. FistAnimInstance 보행 설정: 기준 속도 600cm/s, 한 주기 거리 340cm, 기본 수직 진폭 1.8cm.

## 원본 동작과 후방 보기

실제 Martelo의 head yaw는 최대 약 39.24도다. root는 약 97.08도 회전하지만 머리는 상대를 보는 자세를 유지하므로, 이 파일을 임의 180도 뒤돌아차기로 바꾸지 않았다. 머리가 후방을 향하는 모션에서는 시점도 뒤를 보도록 구현했으며, 원본과 별개인 메모리상의 360도 회전 시퀀스로 후방 시야와 각도 경계를 검사한다. 이 검증 시퀀스는 에셋으로 저장하지 않는다.

## 검증

- UE 5.8.2 Editor 최종 빌드 성공(27.01초).
- 후방 회전 1,446샘플: pitch 0/-70/+35도에서 후방 dot -1, 연속 yaw 360도, 최대 각도 변화 3도. 반강도·90도 제한에서도 반전 없음, native 가드 복귀와 강도 0 검증.
- 기본 격투 480프레임 실패 0. 좌우 잽/스트레이트/훅·차징·점프·입력 보호 회귀 통과. 점프 높이 89.99cm, 착지 이벤트 1회.
- 너클 120프레임 및 상하/울트라와이드 시야 3조건 실패 0.
- 팔 전용 모드 390프레임 실패 0.
- 최종 빌드 실제 프리뷰 419프레임 실패 0: rendered yaw 최대 39.097도, offset 최대 35.973cm, 실제 지상 이동 gait 87프레임, 상하 범위 2.679cm. 최종 빌드로 재실행해 동일 지표를 확인했다.
- 독립 소스/시각 검토 통과. 대표 화면에서 머리 내부 노출·몸 관통·재질 구멍 없음. Martelo 원본의 pitch/roll을 따라가므로 킥 도중 하늘이 보이고 발·표적이 화면 밖으로 나가는 구간은 있다.
- 공용 자산 네 파일 해시 보존, 변경 경로 및 diff 검사 통과.

실행 증거: `Feature/doc/evidence/head-attached-camera-20260922/`. 초기 컴파일에서 프리뷰 검증 코드의 TObjectPtr 타입 추론 문제 1건을 수정했다. 엔진 실험 플러그인의 기존 시작 로그 오류와 이번 기능의 성공/실패 마커를 구분했다. 패키징 빌드는 수행하지 않았다.

## 변경 파일

- `Source/Rogue10m/Components/Rogue10mFirstPersonPresentationComponent.h/.cpp`: 전신 머리 추종, SingleNode 포즈 평가, 눈 회전 기준, 전환 초기화.
- `Source/Rogue10m/Character/Rogue10mFistAnimInstance.h/.cpp`: 동기화된 보행 자세 및 머리 신호.
- `Source/Rogue10m/Tests/Rogue10mHeadCameraRuntimeTest.cpp`: 일시적인 실제 회전 포즈를 통한 독립 후방 시점 검사.
- `Source/Rogue10m/Tests/Rogue10mMarteloPreviewRuntimeTest.cpp`: 실제 시점 지표·이동·점프 촬영.
- `Source/Rogue10m/Tests/Rogue10mBasicBrawlerRuntimeTest.cpp`, `Rogue10mFirstPersonFistRuntimeTest.cpp`: 전신용 제한값 검증. 기존 입력·카메라 컴포넌트 불변 조건 유지.

공용 에셋과 공격 입력은 변경하지 않았다. 커밋·푸시 없음.

## 실제 게임 영상

`Feature/doc/images/head-attached-camera-20260922/head-attached-camera.mp4`: 1280×720, 30fps. 아래보기→Martelo 원래 속도/0.5배속→이동·점프·착지→정지 복귀. 원본 Martelo 후방 180도 영상이 아니다.
