# RMB 원본·SourceABP 독립 검증 — 2026-09-28

## 결론

우클릭 훅에서 나타난 머리 83°/프레임과 오른손 125°/프레임을 Appearance 리타깃 증폭으로 설명할 수 없다. MM_Attack_03 원본의 큰 회전이 1.56배로 재생되며, 공격 시작 0.25초의 몽타주 진입 블렌드가 머리의 회전 전환에 추가된다. 완전 가중 구간의 머리·양손·골반 회전은 원본과 SourceABP가 사실상 일치한다.

발은 별개다. 완전 가중 구간에도 원본과 SourceABP 발 자세가 크게 다르므로 SourceABP FootIK를 분리한 실행 비교가 필요하다. Source→Appearance에서 확인된 추가 발 급회전과 구분해야 한다.

## 방법과 증거

- 읽기 전용 Editor 스크립트: `Scripts/Editor/AuditBrawlerSecondaryStability.py`.
- 원본 `MM_Attack_02`, `MM_Attack_03`를 Manny에서 30fps로 직접 평가했다.
- `before/frames.csv`에 기록된 실제 `clip_s`로 다시 평가하여 같은 프레임의 `poses.csv` Source raw component-space quaternion과 비교했다.
- quaternion 절댓값 내적의 최단 회전각을 사용하므로 Euler ±180° 래핑이 아니다.
- `secondary-stability.json`: 전체 포즈와 프레임 비교.
- `secondary-stability-summary.json`: 클립과 몽타주 메타데이터, 극값.
- `secondary-independent-conclusions.json`: 0.25초 이후 완전 가중 구간 검산.

## 원본 및 재생 설정

| 항목 | 스트레이트 | 훅 |
|---|---:|---:|
| 원본 클립 길이 | 1.0초 | 1.666667초 |
| 런타임 clip time / elapsed time | 1.04배 | 1.56배 |
| additive | NONE | NONE |
| root motion | 활성 | 활성 |
| 몽타주 blend in/out | 0.25초 / 0.25초 | 0.25초 / 0.25초 |
| 원본 30fps 머리 최대 step | 24.624° | 38.782° |
| 원본 30fps 오른손 최대 step | 50.932° | 98.064° |

두 몽타주는 단일 DefaultSlot에서 해당 원본 전체 구간을 재생한다. 클립·몽타주의 asset rate_scale은 1.0이고 런타임 호출 속도가 더해진다. `Rogue10mCombatComponent.cpp`는 attack speed multiplier와 SkillData.AnimationPlayRate를 곱해 몽타주에 전달한다.

## 훅 급회전 프레임

프레임 251→252는 실제 공격 시간 0.166667→0.2초이며 원본 시간은 0.26→0.312초이다. 아직 0.25초 진입 블렌드 구간이다.

| 본 | 원본 동일 시간 step | SourceABP step |
|---|---:|---:|
| 머리 | 56.394° | 82.841° |
| 왼손 | 65.510° | 89.355° |
| 오른손 | 130.522° | 125.228° |
| 골반 | 40.396° | 58.871° |

원본 큰 회전과 진입 블렌드가 겹친다는 증거다. 블렌드 포즈와 가중치를 별도 기록하지 않았으므로 정확한 26.447° 차이의 수학적 분해까지 확정한 것은 아니다.

0.266667초 이후의 원본→Source 최대 회전 오차는 다음과 같다. 스트레이트 14표본, 훅 17표본이다.

| 본 | 스트레이트 최대 오차 | 훅 최대 오차 |
|---|---:|---:|
| 머리 | 0.0000145° | 0.0000184° |
| 왼손 | 0.0000223° | 0.0000485° |
| 오른손 | 0.0000796° | 0.0001109° |
| 골반 | 0.0000187° | 0.0000250° |
| 왼발 | 70.136° | 179.214° |
| 오른발 | 81.719° | 133.577° |

상체 회전 오류를 SourceABP IK가 만든다는 주장은 이 결과와 맞지 않는다. 발은 SourceABP `CR_Mannequin_FootIK` 노드가 Alpha=1로 설정된 사실이 확인되었지만, 별도 A/B 없이 모든 발 차이를 그 노드의 결함으로 확정하지 않는다.

## 위치 해석 주의

원본은 root motion 활성 클립이고 runtime은 root transform을 추출한다. 따라서 훅 말미 원본→Source 약 210cm 위치 차이를 본 늘어남으로 간주하면 안 된다. root의 위치 차이를 제거하면 상체에는 공통 최대 약 2.275cm가 남는다. 이 값도 FootIK 골반 이동 또는 base offset과 구분해야 한다. 발에는 root 보정 후에도 큰 위치 차이가 남는다.

## 한계 및 다음 검증

- 원본 MM_Attack_03의 모션이 사용자가 원하는 짧은 오른쪽 훅에 적합한지는 별도 시각 검토 및 클립 선택 문제다. 기존 자산이 원본이라는 이유로 1인칭 적합성을 통과시킬 수 없다.
- animation data model/curve 목록은 현재 Python API에서 노출되지 않아 `unavailable`로 남겼다. additive NONE 및 몽타주 blend·slot·원본 경로는 실제 API에서 읽었다.
- 원본을 재저장하거나 리타깃하지 않았다. 평가만 수행했다.
- SourceABP FootIK 비활성 A/B 및 Appearance retarget IK 비활성 A/B를 나누고, 최종 실제 영상과 유효한 무릎·팔꿈치 각도로 확인해야 한다.
- 검증 완료 여부: 원인 분리 증거 확보. 수정 결과 전체 PASS를 뜻하지 않는다.

## 적용 결과 확인 — 최종 런타임 전

다음은 Editor 산출물에서 확인했으며 실제 플레이 품질 통과와 구분한다.

- `source-ik-order.json`: 실제 기존 링크는 StateMachine_1→Slot→ControlRig→Root였고, StateMachine_1→ControlRig→Slot→Root로 3개만 변경되었다. 기존 노드와 다른 링크 유지, Blueprint compile 성공, 저장 성공을 확인했다.
- `hook-blend.json`: 전용 `AM_BrawlerRightHook_Stable`의 HermiteCubic BlendIn 시간만 0.25→0.1초이다. 길이1.666667초와 blend-out 등 검사 필드를 보존했다. Hook Data Asset은 montage 참조만 전환했으며 gameplay_unchanged=true이다.
- 공용 `AM_Punch_03`와 원본 `MM_Attack_03`의 적용 전후 SHA256가 일치했다.
- `ConfigureCommonCharacterAnimation.configure_common_anim_blueprint()`가 FootIK 순서 helper를 재사용하도록 수정하여 해당 생성기 재실행에서 기존 순서가 복구되지 않도록 했다. 생성기를 전체 재실행하지는 않았다.

기각한 설명: 전체 머리152°를 Boxing 원본15°와 직접 비교해 Appearance 리타깃 증폭으로 보는 설명, root-motion 추출 위치 차이를 본 길이 늘어남으로 보는 설명, Source FootIK가 상체125° 회전을 생성한다는 설명은 위 측정과 맞지 않는다.

남은 판정: 새로운 최종 fixture에서 4공격의 시작·타격·복귀 전체 step과 Source→Appearance 오차를 비교해야 한다. BlendIn 단축이 공격0.2초의 추가 회전을 줄일 가능성은 있으나 전체 프레임 최대값 감소는 아직 확정하지 않는다. 원본 훅의 큰 전신 회전과1.56배 재생 자체는 이번 전용 몽타주 수정에 남아 있다.
