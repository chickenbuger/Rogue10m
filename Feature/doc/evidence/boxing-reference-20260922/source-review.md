# Boxing reference stances — 독립 소스 검토

- 날짜: 2026-09-22
- 역할: 검증 1 (런타임 소스를 직접 수정하지 않은 별도 에이전트)
- 판정: 정적 소스 검토 PASS. 아래 범위에서 해결되지 않은 차단 결함을 찾지 못했다. 빌드·실행·화면 검증은 별도 증거로 판정한다.
- 비교 기준: `tmp/boxing-reference/before/Rogue10mFistAnimInstance.h`, `.cpp`, `Rogue10mBasicBrawlerRuntimeTest.cpp`와 현재 작업 파일. 저장소 전체 미커밋 변경을 이번 기능으로 간주하지 않았다.
- 검토 소스 SHA256: CPP `9DD96D8B6B327149554557BFFEE39D0775C06A00C5CF85CA24667DB8F5DD15B7`, header `75F4D0D0DF3438CE2C7BA5E4FBB9FA188794263E2D6B75E7005393B0708E7CC2`.

## 확인 결과

1. **UObject 수명 및 스레드 경계** — 자세 enum과 튜닝값은 게임 스레드 `PreUpdate`에서 POD 스냅샷으로 복사된다. 평가 경로는 기존 UPROPERTY/TObjectPtr 소유의 애니메이션 데이터를 빌려 읽고, 새 UObject 비소유 포인터나 월드 조회를 추가하지 않았다. 새 프로필·발 IK 평가는 Actor/Component 상태를 직접 읽지 않는다. generated include는 마지막이며 새 enum은 UENUM으로 노출됐다.
2. **범위와 비활성 경로** — 새 자세 가중치는 전신 표시 중인 자신의 1인칭 메쉬이며 Unarmed·생존·BodyMotionStrength 양수일 때만 활성화된다. Knuckle과 팔 전용 경로에는 새 발 자세/가드 보정을 적용하지 않는다. BasicStanceStrength=0이면 기존 경로이며 새 수치는 finite 검사와 clamp를 거친다. 입력·피해·사거리·캡슐 이동을 바꾸지 않았다.
3. **뼈 변환** — 골반/척추는 부모부터 component transform을 재구성한다. 발은 thigh/calf/foot 존재를 검사하고 현재 자세에서 TwoBoneIK를 계산한다. stretch=false, 부모 인덱스순 정렬, LocalBlendCSBoneTransforms 및 안전한 CS→local 변환, normalize 순서를 유지한다. 기존 팔 길이 94% 제한과 손목 해부학 축 계산을 보존했다.
4. **이동·공중 전환** — 초기 버전의 GroundSpeed>30 또는 falling 즉시 IK 해제는 발 위치가 갑자기 바뀔 수 있어 피드백했다. 최종 버전은 속도 5~65 구간의 연속 목표와 18Hz SupportingFeetWeight 보간을 사용하고, IK 결과를 가중 혼합해 FK로 돌아간다. 비활성 클래스/강도0에서는 잔여 자세가 남지 않는다.
5. **머리 추종** — EvaluateBasicPose→ApplyLocomotionPose→CaptureHeadDelta→ApplyFullBodyAim 순서가 그대로다. 발 IK는 하체만 조정하고, 머리 신호는 보행까지 합성한 뒤 마우스 조준 보정 전에 측정한다. 따라서 기존 머리 카메라에 같은 움직임을 중복 합산하거나 조준 회전을 다시 넣지 않는다.
6. **스타일·전투 계약** — CompactBoxing/LongGuard/RootedMartial은 명시 선택값이며 무작위 선택이 없다. 비공격 손 가드·팔꿈치·골반 회전비·발 지지의 제한된 차이를 적용한다. 기존 HitFraction, 공격 순번, 차징·취소 및 손 회수 곡선을 유지한다. 데미지 프레임에 최대 전진이 도달하는 기존 계약은 테스트에서 계속 검증한다.
7. **테스트 변경** — 세 스타일별 별도 캡처 폴더와 enum 선택을 추가했다. 가드 이후 공격·이륙 전후의 대퇴/종아리 길이가 0.1cm 이상 달라지면 실패하는 검사를 추가했다. 기존 직선 전진 8cm 초과, 훅 수평 15cm 초과 및 straight 대비 5cm 차이, 판정 오차 0.1초 이하, 공격당 1회 피해, 손 프레임 이동 20cm 미만, 카메라 컴포넌트/ControlRotation 불변 검사를 완화하지 않았다.

## 검증 범위와 표현상 한계

- 이 검토는 소스 리뷰다. 3개 스타일 런타임, 팔 전용/Knuckle 전환, 머리 카메라 회귀와 실제 영상은 통합 검증 담당의 결과가 필요하다. 30fps 테스트만으로 60fps 실행을 완료했다고 표시하면 안 된다.
- FBX 전체 리타깃 재생이 아니라 원본의 왼발 전진·왼손 직선과 별도 자세 자료를 참고한 절차적 포즈다. 원본에 여러 격투기 스타일이 들어 있었다고 설명하지 않는다.
- 발 보정은 포즈 공간의 작은 전진·뒷발 들림·회전이다. 지면 추적, 발볼 고정 피벗, 실제 루트 이동을 구현하지 않았다. 바닥 고정 정확성과 다리 실루엣은 독립 화면 검증에서 판단한다.
- 현재 스타일은 튜닝용 프로퍼티/검증 콘솔로 선택한다. 플레이 도중 스타일을 교체하는 UI나 연속 전환 애니메이션을 제공한다고 주장하지 않는다.
