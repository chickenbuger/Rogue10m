# 맨손·너클 카메라 독립 소스 리뷰

작성: 2026-09-29 / 기준: Sprint#5-1-unarmed-knuckle-animation 작업 변경

## 판정

현재 카메라 구현에서 머리 위치, 마우스 입력, 공격 trace, raw 전신/그림자 포즈를 훼손하는 코드 결함은 발견하지 못했다. 새 초기 시선·V fixture의 기대식은 변경된 명세와 맞는다. 기존 AppearanceCameraRuntimeTest의 raw 회전1:1 기대식 불일치는 담당자가 수정했고 재검토에서 해소됨을 확인했다. 이 리뷰는 코드 검토이며 빌드·runtime·실제 시야 편안함 통과를 뜻하지 않는다.

## 확인한 구현

- 안정화 적용 조건은 Unarmed 또는 Knuckle이며 비활성 설정에서는 기존1:1 경로를 사용한다. ReadCameraTransform은 Appearance camera가 실제 적용된 경우에만 동작한다.
- 머리 회전은 정규화한 quaternion의 identity 대비 최단각을 사용한다. weight는 `[0,1]`, 각도 제한은 `[0,180]`로 제한하고 실제 보간 비율을 `min(weight, limit/angle)`로 계산한다. 기본값은0.15·8°이다.
- UE5.8 `TQuat::AngularDistance`는 quaternion 부호에 무관한 최단 회전각을 계산하며 `FQuat::Slerp`는 짧은 경로를 사용한다. `q`와`-q`의 표현 차이를 회전량 차이로 취급하지 않는다.
- 상태를 누적하지 않는 순수 합성이므로 같은 입력으로 camera/render/attack trace가 여러 번 호출되어도 감쇠가 중복 적분되지 않는다. 시간 기반 smoothing이나 mouse filtering을 추가하지 않았다.
- 최종 회전은 안정화된 머리 motion × 즉시 ControlRotation이다. ActorRotation은 눈 offset 방향을 정할 때만 사용하므로 actor yaw를 카메라 회전에 중복 적용하지 않는다.
- 실제 AnimatedHead 위치는 유지한다. eye offset의 방향만 안정화된 head motion으로 계산하므로 캡슐 고정 카메라가 아니다. 기존 전신 이동·점프에 따른 머리 위치 움직임까지 제거하는 기능은 아니다.
- 최종 계산값을 실제 CameraComponent에 기록한다. CombatComponent의 hit pulse는 UpdateCameraFromAppearance 후 같은 camera 위치·forward를 읽으므로 렌더용 감쇠와 조준용 카메라를 분리하지 않는다. V는 기존 최종 OutPOV 관찰 경로를 유지한다.
- ReadAnimatedHead와 shadow proxy가 읽는 raw local pose는 수정하지 않는다. 머리 숨김, 전신 애니메이션, 그림자 포즈는 카메라 합성에 의해 변하지 않는다. 별도 FirstPersonMesh 재활성화도 없다.
- Appearance 초기 시선은 별도 InitialViewPitch=-20°이며 legacy FP의 -30°는 보존한다. 한 번만 적용하는 기존 guard와 유효한 local pawn/0° 시작 pitch 검사를 유지한다. 무기교체나 반복 refresh가 이미 입력한 pitch를 덮어쓰지 않는다.
- 신규 설정은 UPROPERTY tuning 값이며 새 UObject 참조·상시 Tick·constructor world 접근은 추가되지 않았다.

## 테스트 검토

수정된 ThirdPersonInspection 및 InitialHeadFraming fixture는 production ComposeCameraTransform을 호출해 자기검증하지 않는다. raw head quaternion을 W>=0으로 canonicalize한 뒤 axis-angle에서 독립적으로 `min(angle×weight, limit)`를 계산한다. location과 rotation 모두 같은 새 명세로 비교한다.

초기 시선 테스트는 fresh spawn의 ControlRotation을 먼저 검사하고, 별도 명시 시선·실제 UpdateRotation 입력·무기 교체 후 유지 여부를 검사한다. 카메라 pitch가 control pitch와 완전히 같아야 한다는 옛 가정을 제거한 것은 명세 변경에 맞다. 그래도 실제 runtime에서는 engine cache와 현재 포즈의 프레임 순서를 함께 확인해야 한다.

### 기존 Appearance fixture 수정 재검토 — 지적 해소

최초 검토에서 `Rogue10mAppearanceCameraRuntimeTest.cpp`의 ExpectedCamera가 raw 회전1:1을 가정해 새 카메라를 거짓 실패시키는 문제를 통지했다. 담당자의 다음 수정까지 읽고 검토하여 이 항목을 해소했다.

- ExpectedCamera는 raw local bone chain과 reference chain을 직접 누적한 뒤 독립 axis-angle 방식으로0.15·8° 정책을 계산한다. production ComposeCameraTransform을 호출하지 않으며, 같은 독립 결과를 automatic cache와 명시 CalcCamera 양쪽에 사용한다.
- fresh spawn pitch는 이전 고정-30° 대신 Appearance InitialViewPitch와 비교한다. 기본-20° 자체의 검사는 InitialHeadFraming fixture가 별도로 유지한다.
- 좌우 잽의 과거 montage 이름 비교를 현재 Primary DA 및 NextComboSkill.AttackMontage의 실제 객체와 비교하도록 바꿨다. montage 실제 재생 표본3개 이상·몸 손 이동5cm 이상 검사는 유지한다. 이 비교는 현재 DA대로 재생하는지 확인하는 것이며, DA가 의도한 신규 전용 자산을 참조하는지의 검증은 별도 authoring 감사가 맡아야 한다.
- 스트레이트가 반드시 빗나가야 한다는 과거 원본 연출 가정만 제거했다. 독립 충돌 geometry 측정이 존재하고, 그 결과에 따라 실제 DamageSamples가 정확히0 또는1인지 검사는 유지했다. 다른3공격의 피해1회, 피해 시점 오차 한계, 공격 밖 피해 금지 검사도 유지한다.
- automatic camera cache570샘플, 실제 camera와 기대식 오차 한계, input control 불변, raw head/이동/점프 존재 검사는 그대로다. 새 애니메이션 때문에 성공 기준을 무조건 완화한 변경은 아니다.

이번 수정은 코드상 적절하다. 실제 fixture 실행 결과는 아직 대기이며, 이를 runtime PASS로 표시하지 않는다. 리뷰 담당자는 상품/테스트 코드를 수정하지 않았다.

## 남은 실행 검증

- 새로운9클립의 전후 head 유래 회전범위·연속 step, 최대 각도8° 준수, 실제 camera cache/trace 일치.
- Unarmed/Knuckle 양쪽과 Staff 등 비대상 무기의 기존1:1 경로, 사용자 설정 비활성, 반복 동일 입력 합성.
- V 왕복·초기-20°·마우스입력·무기전환·머리 내부 가림·전신 그림자 유지.
- quaternion 최단축은180° 경계에서 방향 선택이 바뀔 수 있다. stateless8° 제한만으로 임의의 원본 전회전을 매끄럽게 만든다고 보장하지 않는다. 이번 전용 모션의 실제 head 범위와 전체 진입/회수 step을 보고 판정한다.
- 위치는 raw head 그대로이므로 큰 head translation은 모션 제작 단계에서 검토해야 한다. 카메라 각도 감소만으로 손발 본 정상 또는 전신 애니메이션 자연스러움을 판정하지 않는다.

## 검토 시점 SHA256

|파일|SHA256|
|---|---|
|AppearanceCameraComponent.h|264D4850A03EF0B74F0E0FEFAD53A0457CD5AFE325D10F2BCC2E8F334E3C79D8|
|AppearanceCameraComponent.cpp|FC1C40908C2AFFEBE6C3540AA938ABEBC9836C11C1CAB987820B4F33E322C8AE|
|FirstPersonPresentationComponent.cpp|E99354E138D62829606019DA93B7A11228D8FC1CA1190B773CDE323DF5509052|
|ThirdPersonInspectionRuntimeTest.cpp|9C283F88366644002A146E77099B93FCE7B06B0965534D362956834496338718|
|InitialHeadFramingRuntimeTest.cpp|1E885F79F476C22590B914CF379AD53D34D2E6A76414CE901B757434F512B48E|

|AppearanceCameraRuntimeTest.cpp (지적 수정 재검토)|5DF6589597B7CF0A8831CA66ED4FE779EA0AA401808B30093E03932C6A1555C8|

## 통합 담당자의 최종 실행 확인

2026-09-29 v2 자산 적용 후 InitialHeadFraming 90프레임, ThirdPersonInspection 280프레임, AppearanceCamera 570프레임을 모두 실패 0건으로 완료했다. 6체형 FistCombat도 각각 840프레임을 통과했다. 상세 결과는 같은 폴더의 initial-framing-final.log, inspection-v2.log, appearance-v2.log, *-v2.log 및 runtime-final-review.md에 보존했다. ThirdPersonInspection의 과거 몽타주 이름 고정 비교는 실제 Primary DA/NextComboSkill 참조 비교로 교체했고 실제 몽타주 샘플 수 검사는 유지했다. 이는 위 초기 코드 리뷰 이후의 통합 실행 결과이며, 미실행으로 남겨 둔 설정 극값·모든 다른 무기 원본까지 검증했다고 확장하지 않는다.
