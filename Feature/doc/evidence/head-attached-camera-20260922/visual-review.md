# 독립 검증 2 — 애니메이션 신호 및 시각 검증

검증 시점: 2026-09-22. 검토 범위는 `tmp/head-camera/before/Rogue10mFistAnimInstance.*` 대비 현재 native Fist 변경이다. 카메라 구현은 다른 독립 검증 역할에서 검토한다.

## 초기 소스 검토

현재 검토한 애니메이션 변경에서 수정을 막을 결함은 발견하지 못했다. 아래 실제 캡처와 런타임 결과를 추가 확인했으며, 검토한 범위는 통과다.

- `PreUpdate`가 캐릭터 이동 상태와 조절값을 game thread에서 복사한다. `Update`와 `Evaluate`의 신규 gait 코드는 이 복사값과 local pose만 사용한다. worker 평가에 새 owner/UObject 접근을 추가하지 않았다.
- grounded 수평 속도가 보행 위상을 진행시키며, 정지·공중에서는 위상을 멈추고 진폭을 감쇠한다. dt와 신규 조절값은 유한값/범위를 방어한다. capsule의 이동·점프 높이를 pose에 중복 더하지 않는다.
- 실제 spine 변형을 head/shoulder/guard 자식이 공유한다. 평가한 최종 head delta를 마우스 aim 보정과 renderer neck 숨김 전에 수집하므로 마우스 pitch가 camera signal에 다시 들어가는 경로가 없다.
- Basic 및 Knuckle 양쪽 평가 결과에서 head 신호를 수집한다. 기존 작은 legacy camera 신호는 그대로 유지하고, 전신 비활성화 시 gait를 0으로 초기화한다.
- 새 보행은 spine 및 그 자식의 작은 움직임이다. pelvis/다리 보행이나 발 IK가 새로 구현된 것은 아니다. 설계·완료 문서는 이 범위를 정확히 기술해야 한다.

## 시각 검증 항목

실제 게임 캡처와 해당 런타임 로그로 다음을 검증했다.

1. 이동 중 환경과 시선의 주기적 변화, 손/머리와 몸통의 동기 움직임.
2. 점프·착지의 시점 동작과 급정지 후 안정화.
3. Martelo 재생 시 실제 head 방향 추종, 과도한 몸통 관통·머리 내부 노출 여부.
4. 원본에 없는 후방 회전을 원본 Martelo로 오인하지 않도록 별도 fixture 구분.
5. 회복 및 아래보기에서 팔·몸·다리 프레이밍 유지.


## 실제 캡처 결과 — PASS (아래 한계 포함)

확인 자료: `Saved/Screenshots/WindowsEditor/HeadCameraPreview/frame_####.png`에서 생성한 `Feature/doc/images/head-attached-camera-20260922/review-sheet.jpg`, 추가 이동/점프 비교 이미지 `gait-jump-review.jpg`, `tmp/head-camera/preview.log`, `tmp/head-camera/head.log`. 직접 확인한 대표 프레임은 12, 55, 98, 105, 112, 120, 275, 285, 295, 304, 305, 315, 325, 350, 359, 366, 373, 390, 409, 418이다. 정지 이미지의 시간 순서와 수치 로그를 함께 검토했으며 실시간 연속 재생을 직접 관찰했다는 의미는 아니다.

- 프레임 55에서 아래보기 팔·몸·다리가 유지된다. 검토한 프레임에 머리 내부, 몸통 관통, 기존 재질 구멍은 보이지 않는다.
- 이동 구간에서 환경 위치가 실제로 바뀌면서 양손 높이/기울기가 함께 변한다. 점프 구간 359→366→373에는 지평선과 벽 높이가 달라지고 손이 내려갔다 복귀하는 모습이 있다.
- 정지 후 390/409/418은 가드로 복귀한다. 로그도 390부터 camera offset/rotation 0을 보여 잔류 흔들림이 남지 않는다.
- Martelo 원본 속도 및 느린 속도 구간은 머리 회전과 함께 시야가 기울어진다. 실제 rendered yaw 최대 39.097도, 최대 offset 35.973cm, 실제 gait 87프레임, gait Z 범위 2.679cm를 기록했다. `RESULT=MARTELO_PREVIEW_PASSED frames=419 failures=0`이다.
- 후방 회전 검증은 `synthetic_only=1`인 별도 수학/pose fixture이다. 723개 sample, rear dot -1, 최대 frame angular step 3도, pitch 0/-70/+35에서 360도 연속 yaw를 통과했다. 이 검증을 원본 Martelo의 실제 후방 킥 영상으로 해석하지 않는다.

## 표현상 한계

원본 Martelo의 head pitch/roll을 충실하게 따라가기 때문에 98~112 부근에서 시야가 하늘 쪽으로 올라가며 발과 표적이 화면 밖으로 나간다. 머리 추적 동작 자체는 의도와 일치하지만 이전 고정 시점에 비해 킥의 가독성은 낮다. 현재 원본을 180도 뒤돌아차기라고 소개하면 안 된다. 이후 전투용 킥을 만들 때는 원본의 머리 방향 또는 회전별 camera gain을 별도로 연출할 수 있으나 이번 검증에서는 원본을 수정하지 않았다.

달리기 구현 범위는 spine와 자식의 보행 동기 움직임 및 카메라 추종이다. 새로운 다리 보행 animation/foot IK의 품질을 검증한 것은 아니다. 빌드 및 전체 회귀 결과는 root의 최종 로그와 소스 검증 문서에서 통합한다.

## 최종 빌드 재확인

루트가 최종 빌드 preview-final.log의419프레임실패0와 동일yaw39.097/offset35.973/gait87프레임지표를 확인했다. 새 촬영105/304프레임도 다시 직접 확인했다. 확장후방검증은head-final.log에서1,446샘플PASS이며최대각도변화3도,반강도1.5도다. 최종MP4는419프레임/30fps/13.966667초다.
