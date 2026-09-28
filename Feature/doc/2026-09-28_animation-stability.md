# 캐릭터 애니메이션 왜곡 진단 및 수정

- 날짜: 2026-09-28
- 브랜치: `Sprint#4-42-animation-stability`
- 상태: 확인된 리타깃·IK·잽 스탠스 오류 수정 및 지정 범위 실행 검증 완료. 원본 훅의 빠른 전신 회전은 유지.
- 기획: `Feature/architect/2026-09-28_animation-stability.md`

## 결과

공격 중 몸통과 허벅지 연결부가 늘어나고 발이 뒤집히던 원인을 분리해 수정했다. 카메라는 기존 Appearance Mesh 머리 본을 그대로 따라가며, 카메라 감쇠로 몸의 오류를 숨기는 변경은 하지 않았다. V키 전신 확인과 별도 First Person Mesh 비활성 상태도 유지한다.

| 측정 항목 | 수정 전 | 수정 후 |
| --- | ---: | ---: |
| Appearance 왼쪽 허벅지 연결 길이비 | 0.644~3.546 | 1.000 |
| 왼잽 오른발 최대 회전/프레임 | 150.812도 | 17.183도 |
| 왼잽 왼발 최대 회전/프레임 | 123.061도 | 23.875도 |
| 오른잽·복귀 오른발 최대 회전/프레임 | 110.279도 | 16.049도 |
| 훅·복귀 머리 최대 회전/프레임 | 83.123도 | 57.771도 |
| 훅 full-weight 원본→Source 왼발 회전 오차 | 최대179.214도 | 0.0001도 미만 |

회전/프레임은 30fps 고정 테스트에서 진입과 복귀를 포함한 동일 구간의 최댓값이다. 프레임 회전과 동작 전체 회전 범위는 서로 다른 지표다. 실제 캡처 PASS는 데이터 기록 성공을 의미하며, 별도 독립 분석과 화면 확인으로 수정 효과를 판단했다.

## 원인과 변경

1. **골반을 루트로 취급한 리타깃 설정.** 마지막 Root Motion 연산이 골반을 덮으면서 이미 계산된 척추·허벅지와의 연결 길이를 바꿨다. 6종 외형 리타깃의 SourceRoot/TargetRoot를 실제 root 본으로 분리했다.
2. **Appearance의 추가 FBIK.** 동일 프레임 Source 발은 약9도 움직이는데 Appearance 발이 약151도 회전했다. 추가 Run IK Rig 연산을 끄고 FK로 원래 관절 자세와 체형 비율을 보존한다. 확인된 Tail/Cape/Tabard의 무관한 Spine/HandRootIK 매핑도 해제했다. 원본 IK Rig와 솔버 설정은 보존한다.
3. **공격 포즈 뒤에 적용된 Source FootIK.** `ABP_Common_Unarmed`의 순서를 `이동 포즈 → FootIK → DefaultSlot → 출력`으로 바꿨다. 이동의 지면 보정은 유지하고, 공격 full-weight에서는 원본 발 포즈를 덮지 않는다. 세 연결만 재배치하고 노드 설정은 유지했으며 Blueprint 컴파일 오류 없이 저장했다.
4. **오른잽의 전신 미러.** 원본 하체 스탠스를 유지하고 상체를 미러한다. 가슴의 컴포넌트 회전을 기준으로 골반 회전 중복을 제거하고, 손 IK를 최종 손 포즈에 맞췄다. 오른손 전진 피크는 몽타주0.333초/52.46cm, 타격 기준0.342초에는52.37cm다. 초기 수정은 방향 검사에 실패해 저장하지 않았으며, 기준을 낮추지 않고 재설계했다.
5. **훅 진입 블렌드와 원본 급회전의 겹침.** 전용 `AM_BrawlerRightHook_Stable`의 진입 블렌드만0.25→0.10초로 변경했다. 공유 몽타주·원본 시퀀스 해시는 일치하며, 입력·피해·차징·재생 속도·타격 시간 등 DA 속성은 유지했다.

외형 리타깃 생성기와 공통 AnimBP 생성기도 수정된 설정을 재사용한다. Boxing 원본 FBX·Preview Manny·원본 gameplay 시퀀스는 수정하지 않았다. 좌잽의 가시 포즈도 보존한다.

## 검증

- 최종 UE5.8 Editor 빌드 성공: 8.40초.
- 통합 게임 실행: 450프레임/15초 촬영, 실패0. 좌·우 잽, 짧은 우클릭 스트레이트, 홀드 후 훅, 대기 복귀, 걷기187.74cm, 점프89.99cm, 착지와1인칭 복귀 확인.
- V키 회귀: 280프레임/실패0. 실제V입력 왕복, UI 차단, 벽 충돌, 양쪽 잽 재생, 별도FP 비활성 확인. 머리 카메라 위치 오차0cm, 회전 오차0.00000382도.
- 파생 잽121개 샘플: 양쪽 하체 동일, 손 IK 위치 오차0.000098cm 미만, 회전0.000122도 미만, 팔 길이 오차0.000445cm 미만.
- 독립 검증2개: 같은 클립 시각의 원본→Source 발 포즈 복원 및 Source→Appearance 길이·회전 검사, 진입/복귀를 포함한 공격별 전후 분석.
- Python AST·git diff 공백·Harness 생성 경로 검사 통과. 바이너리 변경은 Unreal Editor API에서만 수행했다. 공유 훅 원본 해시 보존, 관련31파일 최종 해시 기록.
- 이번 통합 기록은 공격 입력 및 재생을 확인하며, 모든 공격의 실제 표적 명중을 보증하는 테스트는 아니다. 기존 피해·범위·입력 수치는 변경하지 않았다.

## 남은 연출 특성과 범위

훅의 원본 `MM_Attack_03`에는 매우 빠른 큰 회전이 있다. 오른손은 한 프레임130.522도 회전하는 구간이 남으며, 원래 발 움직임을 복원하면서 일부 훅 발 회전은 이전보다 커졌다. 이는 추가 리타깃 왜곡이 사라졌다는 결과와 구분해야 한다. 훅 자체를 더 차분하게 보이게 하려면 원본의 타이밍·포즈를 별도로 다듬는 작업이 필요하다.

공격 중에는 지형에 맞춘 추가 발 접지 IK 대신 원본 공격 자세를 우선한다. 경사면 접지와 체형별 손 도달 보정은 별도 검증 대상이다. 6종 외형 설정을 수정했지만 이번 실제 영상과 수치 QA는 Human Male 기준이다. 다른5종의 개별 전투 영상, 네트워크, 무기 전체·회피·재빙의 검증은 수행하지 않았다.

이전에 전체152도 머리 회전을 단순히 'Boxing 원본 동작'으로 설명한 것은 정확하지 않았다. 해당 수치는 우클릭 훅까지 합친 값이었으며, 이번 검사에서 FBX/Manny 잽 내부 변화 약14.83도와 분리했다. Manny→Appearance가 머리 회전을152도로 증폭시킨다는 가설도 기각했다. Source/Target 발 목표 본 불일치와 Manny Spine 범위 문제도 검사 결과 기각했다.

## 증거와 복구

- `evidence/animation-stability-20260928/after-gameplay.mp4`: 최종15초 실행 영상.
- `before-gameplay.mp4`, `before-contact.jpg`, `after-contact.jpg`: 동일 입력 전후 비교.
- `independent-after-review.md`, `final-source-review.md`, `final-runtime-review.md`, `independent-before-vs-after.json`, `secondary-after-original-comparison.json`: 독립 검증.
- `runtime-pose-captures.zip`: before/root-only/root-no-ik/after의 CSV·분석 기록.
- `boxing-assets.json`, `source-ik-order.json`, `hook-blend.json`, `retarget-policy.json`: Editor 검증 결과.
- `build-final.log`, `after-runtime.log`, `v-regression.log`, `media-validation.json`, `final-files.json`: 최종 빌드·실행·영상·파일 해시.
- 수정 전 원본 백업: `tmp/animation-stability/before-jab-assets`, `source-abp-before`, `hook-blend-before`, `tmp/appearance-retarget/live-root-only`, `live-no-ik`, `final-policy`의 manifest와 읽기 복사본. 중간 실험 복구 스크립트는 최종 정책 적용 후 해시가 달라지므로 그대로 재실행하지 않는다. 최종 복구는 해당 기록을 근거로 Unreal Editor에서 수행한다.

커밋·푸시는 하지 않았다.
