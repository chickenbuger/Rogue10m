# Boxing 기본 공격 독립 소스 검토

- 검토일: 2026-09-23
- 검토 역할: 검증 1 (기획 담당과 동일 세션, 개발 코드 미수정)
- 기준: `tmp/boxing-basic/before/Rogue10mFistAnimInstance.h/.cpp`와 현재 소스 차이
- 상태: **연속 공격/취소 전환 보완 후 재검토 필요**. 런타임·영상 검증은 별도 담당.

## 확인한 사항

- `ApplySourceBoxingPose`가 기본 procedural 포즈 다음, 이동·머리 델타 캡처·마우스 조준 이전에 적용된다. 원본이 실제 최종 local bone transform에 반영되며 기존 IK가 다시 원본 팔을 덮지 않는다.
- 원본 기여는 Unarmed + full-body + 유효 소스 스켈레톤 + 승인된 좌/우 잽에 한정된다. 우클릭 직선/훅은 이 경로에 들어오지 않는다.
- `AttackElapsed / AttackDuration`과 `HitFraction`의 전·후 구간 매핑으로 타격 시점에 원본 13/30초를 평가한다. 기존 피해 타이머, 공격 승인, 자원 비용, 공격 속도 계산을 변경하지 않았다.
- 오른잽은 X축으로 `FAnimationRuntime::MirrorPose`를 사용한다. `_l/_r` 본 대응과 component-space reference quaternion 보정을 포함한다. 로컬 UE 5.8 `AnimationRuntime.cpp`를 대조했고, 매칭이 없는 compact bone의 INDEX_NONE은 엔진에서 건너뛴다.
- 원본 포즈·본 serial 변경 시 미러 캐시를 다시 만들며, `FBoneContainer::GetSerialNumber`는 본 컨테이너 재생성 시 캐시를 갱신하기 위한 엔진 계약이다.
- loaded sequence는 `UPROPERTY(Transient) TObjectPtr`로 보관한다. NativeInitialize에서 동기로 로드하고 PreUpdate에서 안전한 포인터/적격 상태를 전달한다. 평가 스레드에서 액터·월드·비용·입력 상태를 직접 조회하지 않는다.
- 루트 초기 위치를 빼서 고정 시작 오프셋을 제거하고 이동은 mesh pose에만 남긴다. 캡슐 root motion 또는 공격 사거리 변경은 없다.
- 신규 `/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingJab_Manny`는 Editor API로 복제되고 원본은 보존된다. `asset-validation.json`에서 53프레임 × 7본 = 371개 샘플, 위치 및 quaternion 성분 오차 0을 확인했다. Config의 `DirectoriesToAlwaysCook`에 전투용 폴더를 명시했다. 패키지 전체 빌드는 이 검토에서 수행하지 않았다.

## 발견한 전환 문제 — 수정 대기

소스 공격의 시작·끝에서 0이 되는 envelope만으로는 실제 콤보 진입 연속성이 보장되지 않는다. 실제 왼잽 콤보 창은 `ConfigureBasicBrawler.py`에서 0.40~0.55초이며, 다음 serial이 승인되면 현재 원본 기여가 남아 있어도 새 Progress=0으로 소스 가중치가 곧바로 0이 된다. 기존 procedural 손 캐시는 원본으로 블렌딩된 최종 포즈를 저장하지 않으므로 몸과 손이 이전 procedural 포즈로 튈 수 있다. 공격 도중 취소도 같은 위험이 있다.

권고: source 단계의 이전 최종 local pose를 저장하고 serial 변경/원본 종료 시 그 포즈에서 새 목적 포즈로 짧게 크로스페이드한다. 이후에 적용되는 locomotion/aim까지 저장하면 효과가 중첩되므로 캐시는 source 단계에서 저장한다. LOD/mesh/weapon 비활성화 시 캐시를 무효화하고 실제 타격 전에 진입 블렌드를 끝낸다. Root가 같은 문제를 발견했고 개발 담당에게 보완을 요청한 상태이다.

## 런타임·영상 확인 필요

- 최초 잽, 버퍼 오른잽, 취소 및 우클릭 전환의 손·몸·카메라 연속성
- 타격 프레임 원본 기여와 실제 피해 횟수/시간
- 원본/미러의 뼈 길이 및 손목 방향, 이동 중 발 미끄러짐
- 원본 큰 전신 움직임과 기존 카메라 추종 제한의 상호 작용
- 기본 30도 시점에서 공격 손과 적이 보이는지
