# 맨손·너클 기존 구현 독립 소스 재검토

검토일: 2026-09-29. 대상은 현재 미커밋 맨손·너클 애니메이션 구현이다. 새 구현을 만들지 않고 기존 소스와 보존된 검증 자료를 읽어, 머리 흔들림 감소와 팔다리 본 정상성 요구에 대한 재사용 적합성을 검토했다.

## 판정

검토 범위에서 **중대한 신규 결함은 발견하지 못했다. 기존 구현을 재사용해 완료 검증을 진행하는 데 적합하다.** 이 판정은 아래의 소스 재검토와 기존 증거 확인에 근거한다. 이번 검토자가 새 Unreal 실행이나 Editor 빌드를 수행했다는 뜻은 아니다.

## 이번에 직접 수행한 검사

- `git -c safe.directory=D:/Project/Rogue10m status --short`와 해당 카메라 컴포넌트 diff를 읽어 미커밋 변경 범위를 확인했다.
- `Rogue10mAppearanceCameraComponent.h/.cpp`, `Rogue10mFirstPersonPresentationComponent.cpp`, `Rogue10mFistCombatAuthoring.cpp`, `Rogue10mFistCombatRuntimeTest.cpp`, `PrepareFistCombatAnimations.py` 및 관련 분석·독립 리뷰 문서를 읽었다.
- 공격 판정의 `Rogue10mCombatComponent::ExecuteAttackHitPulse`가 `UpdateCameraFromAppearance()` 호출 후 같은 카메라의 위치와 방향을 읽는 것을 확인했다.
- 아래 다섯 파일의 현재 SHA-256을 직접 계산해 기존 최종 독립 리뷰에 기록된 값과 일치함을 확인했다.
- `authored-animation-validation.json`의 원본 패키지 해시 48개를 실제 디스크 파일로 다시 계산했다. 불일치는 0개였다.
- 생성 검증 JSON의 PASS, 9클립·14접촉, 오른잽/스트레이트 궤적 구분 결과와 전용 자산 18개의 실제 존재를 확인했다.
- 저장된 최종 빌드·체형별 런타임·카메라 회귀 로그의 결과 행을 읽고, 1인칭 및 체형별 대표 이미지 두 장을 직접 확인했다.

|현재 파일|직접 계산한 SHA-256|
|---|---|
|Source/Rogue10m/Components/Rogue10mAppearanceCameraComponent.h|264d4850a03ef0b74f0e0fefad53a0457cd5afe325d10f2bcc2e8f334e3c79d8|
|Source/Rogue10m/Components/Rogue10mAppearanceCameraComponent.cpp|fc1c40908c2affebe6c3540aa938abebc9836c11c1cab987820b4f33e322c8ae|
|Source/Rogue10m/Components/Rogue10mFirstPersonPresentationComponent.cpp|e99354e138d62829606019da93b7a11228d8fc1ca1190b773cde323df5509052|
|Source/Rogue10m/Editor/Rogue10mFistCombatAuthoring.cpp|6430f50df3a922a13a7be44567c3fa31d9d7ab9e721d95360b3af839e2b7c383|
|Scripts/Editor/PrepareFistCombatAnimations.py|d16fe331dfafbc306064bd6f89e6f9729f4381d682bb36238e9751b5a2929e7c|

## 소스상 요구 충족 근거

카메라 안정화는 맨손·너클에만 적용한다. 애니메이션 머리 회전 기여를 15%, 최대 8도로 제한하며, 실제 머리 위치와 사용자의 ControlRotation은 유지한다. 초기 시선 -20도는 기존 한 번만 적용하는 조건 안에서 처리하므로 무기 교체나 반복 갱신이 이미 입력한 시선을 초기화하지 않는다. 카메라 합성은 상태를 누적하지 않으며 화면과 공격 판정이 같은 갱신 경로를 사용한다.

제작기는 두 본 IK의 길이 확장을 끄고 상완·전완 회전만 기록한다. 도달 거리를 팔 길이의 94%로 제한하고 손목은 검증된 가드의 local 회전을 유지한다. root·골반·다리는 가드 스탠스를 유지하며 변형 본의 local translation/scale 보존 검사도 남아 있다. 현재 손의 component-space 위치에서 목표로 보간하고 팔꿈치 Pole도 Activity에 따라 보간하므로 기존 고정 가드 좌표에 의한 연타 경계 불연속 원인이 제거되어 있다.

네이티브 epsilon 검사와 Python의 실제 생성 클립 경계·전체 step 검사를 함께 유지한다. 원본 보존, IK 정합성, 타격점과 첫 접촉 이전 블렌드 종료를 검사한 후 자산을 저장하고 Data Asset을 연결한다. 보호 필드 비교는 피해·콤보·해금·입력 등 기존 계약을 유지하며, 너클 첫 타격 지연 변경은 결과 문서에 명시되어 있다.

런타임 테스트는 production 카메라 합성 함수를 그대로 호출하는 자기검증 대신 raw 본과 reference pose에서 독립 axis-angle 기대값을 구성한다. 본 길이·scale·유한값, 실제 몽타주 관측, 입력 승인, 무기 전환 중 차지 취소, 이동·점프·착지, 카메라 캐시와 그림자 proxy를 확인한다. 실제 타깃 피해 옵션은 기대 히트 일정과 별도로 체력 감소를 기록한다.

## 기존 실행 자료에서 확인한 결과

아래는 `Feature/doc/evidence/fist-combat-20260929/`에 이미 보존된 결과를 이번에 읽어 확인한 것이다. 이번 검토의 신규 실행 결과로 계산하지 않는다.

- `build-final.log`: UE Editor 증분 빌드 `Result: Succeeded`, 7.09초.
- Human/Dwarf/Orc × 남/여 `*-v2.log`: 각 `RESULT=FIST_COMBAT_PASSED frames=840 failures=0`.
- `HumanMale-v2.log`: 9종 실제 피해 횟수 1/1/1/1/1/1/6/1/1, 합계 14회. 첫 피해 시각 오차 최대 절댓값 약 0.026923초.
- 6체형 최종 카메라 로그: 머리 회전 기여 최대 약 7.002087도, 설정 상한 8도 이내.
- `inspection-v2.log`, `appearance-v2.log`: 각각 280·570프레임 실패 0건.
- `before-after-camera.json`: 같은 Human Male 맨손 구간에서 머리 카메라 최대 프레임 회전 57.770749→1.877301도, 위치 step 23.855292→4.977427cm. 이는 보존된 비교 수치이며 이번 검토에서 원시 전체 CSV를 다시 집계하지 않았다.
- `first-person-contact-sheet.jpg`, `body-variants-contact-sheet.jpg`: 대표 자세와 시점이 실제 파일로 존재하고 정상 표시됨을 직접 확인했다. 정지 이미지 확인을 동영상 전체의 시간적 자연스러움 검사로 간주하지 않는다.

## 완료 범위와 한계

완료 주장은 머리 흔들림 감소, 검사한 6체형의 구조 정상성, 기록된 공격·입력·시점 회귀 범위로 제한하는 것이 적절하다. 손·손가락의 빠른 회전이 남는다는 기존 문서의 설명도 유지해야 한다. 정상 길이·scale만으로 모든 자세의 자연스러움이나 장비 관통 부재를 보장할 수 없다.

모든 장비·방어구 조합, 다른 체형의 실제 피해 범위, 경사·계단, 극단적 공격속도, 네트워크·사망·재빙의, RT 그림자는 이 재검토의 검증 범위가 아니다. 생성 자산의 내용을 새 Unreal 프로세스로 다시 평가하지 않았고, 새 런타임 실행이나 빌드도 수행하지 않았다. 통합 담당자의 새 빌드·실행 결과는 별도 증거로 확인해야 한다.

이번 검토 중 제품 소스·자산·기존 증거를 변경하지 않았다. 이 보고서만 추가했다.
