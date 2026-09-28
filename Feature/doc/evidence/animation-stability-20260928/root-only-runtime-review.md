# 애니메이션 안정성 검증 2 — root-only 비교 및 미러 코드 리뷰

- 검사일: 2026-09-28
- 입력: `tmp/animation-stability/before/{poses,frames,skeletons}.csv`, `tmp/animation-stability/root-only/{poses,frames,skeletons}.csv`
- 동일 fixture: `Rogue10m.TestAnimationStability`, 331개 pose 샘플 / 330개 영상 프레임 / 30 fps.
- 이 단계에서는 제품 코드·에셋을 변경하지 않았다.

## 판정

**root-only는 골반 자식 본의 비정상적인 늘어남을 수정했지만, 전체 애니메이션 안정성 검증은 통과하지 못했다.** 발목 회전 급변과 우클릭 원본 포즈 문제는 남아 있다. 런타임 `CAPTURE_PASSED`는 기록 완결성과 유한 변환값을 확인한 결과이며 모션 품질 승인과 다르다.

## 실측 비교

| Appearance 본 | 수정 전 local 길이 / reference 길이 | root-only | 판단 |
|---|---:|---:|---|
| spine_01 | 0.824–3.131 | 1.000 고정 | 골반-척추 연결 길이 정상화 |
| thigh_l | 0.644–3.546 | 1.000 고정 | 왼쪽 골반-허벅지 연결 길이 정상화 |
| thigh_r | 0.779–3.388 | 1.000 고정 | 오른쪽 골반-허벅지 연결 길이 정상화 |
| pelvis | 0.743–1.071 | 0.089–1.163 | root→pelvis 이동 길이는 일반 사지 길이와 다르므로 이 수치만으로 오류 판정하지 않음. Source pelvis도 0.020–1.147로 크게 이동하며 우클릭 원본을 별도 조사해야 함 |

Source 단계의 요약값은 before/root-only에서 동일하며, 모든 기록 raw/local 변환 성분을 직접 비교했을 때 최대 절대 차이는 0.015212였다. 서로 다른 실행의 미세한 초기 idle 시점 차이를 허용한 비교이며, Source를 변경해 target 오류를 숨긴 결과가 아니다.

발목 문제는 left jab 시작 구간에서 이미 나타나므로 아직 실행하지 않은 오른손 미러 수정과 독립적이다.

| 시점 | 본 | Source 회전 변화 / 프레임 | Appearance 회전 변화 / 프레임 |
|---|---|---:|---:|
| frame 50 | foot_r | 9.443° | 150.813° |
| frame 52 | foot_l | 5.693° | 123.068° |

전체 캡처 최대 발 회전은 Source가 약 26.5°, Appearance가 왼발 약 123.1° / 오른발 약 150.8°다. root-only 전후에도 그대로다. 회전의 원인이 live retarget/IK 단계에서 추가되는 것은 확인되며, 정확한 operation은 후속 A/B 실험으로 분리해야 한다.

## C++ 미러 수정 정적 리뷰

검사 대상: `Source/Rogue10m/Editor/Rogue10mAppearanceBoxingAuthoring.cpp`의 PreserveSourceBone / AlignCenterBone 로직.

- 배열은 CompactPose 본 수에 맞춰 초기화되고 RefRotation은 부모 우선으로 계산한다. 루트/골반/하지 보존과 중앙 본 시작 회전 보정에서 직접적인 인덱스·수명 문제는 발견하지 않았다.
- 중앙 본의 초기 회전 오프셋은 `source_start * mirrored_start^-1`이며 매 프레임 정규화한다. 시작 프레임의 local 중앙 회전은 원본과 일치한다.
- 왼잽은 분기 변경 없이 원본 전체 변환을 유지한다.
- **조건부 우려:** `!bUpperBody`는 root-parented `ik_hand_*`도 원본 왼잽에 고정한다. 팔은 미러지만 손 IK goal은 원본이므로 후속 단계가 이 goal을 사용하면 불일치할 수 있다. 실제 소비 그래프 확인 또는 mirrored hand와 일치하는 목표 갱신이 필요하다.
- 부분 authoring 실패 시 Target의 in-memory 변경이 남을 수 있는 기존 구조이므로, 호출 스크립트는 실패 결과에서 에셋 저장을 중단해야 한다.
- 이 리뷰는 bake 성공/실제 시각적 품질을 대체하지 않는다. 양손 잽 전환 시 허리 방향, 어깨/손목, 좌우 손 IK 목표를 실제 재생에서 다시 검사해야 한다.

## 다음 검증 조건

1. 발 IK op/goal A/B 후 같은 frame 50/52 및 전체 attack 구간의 발목 회전 차이를 다시 비교한다.
2. 우클릭 source pose 문제를 해결한 뒤 source head/hand/thigh의 급격한 변화가 줄었는지 분리해서 확인한다.
3. 수정된 오른잽 bake 뒤 왼잽→오른잽 전환과 idle 복귀를 3인칭에서 확인한다.
4. 최종 1인칭에서 카메라가 여전히 Appearance head를 따라가면서, 몸 원인 수정으로 흔들림이 감소했는지 확인한다.

원시 비교 요약: `root-only-independent-metrics.json`.
