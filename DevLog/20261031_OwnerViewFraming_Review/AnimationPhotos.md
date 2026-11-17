# 실제 애니메이션 단계별 사진 인덱스

기존 Jab 몽타주/시퀀스와 게임플레이 시계는 유지하고 소유자 표시 포즈의 팔 회전4개/IK 및 component framing만 변경한 후보이다. 사진은 D3D11 Offscreen 실제 캡처이며 보정·연출 이미지가 아니다. 단계 구분은 실제 입력 수락·피해·회복 메타데이터를 따른다.

실제 스킬/몽타주/시퀀스 경로와 모든 시각·궤적은 owner-framing-bounded-final1031.json에 있다. world/body와 actual proxy/cached camera의 손 궤적은 서로 다른 좌표로 집계한다. 사진은 요청과 같은 렌더 프레임 또는 이후 프레임에 촬영됐으며 지연을 함께 표시한다.

물리 입력/일반 필드 완주/패키징은 미실행이다. 제품 기본은 OFF다. 이 인덱스는 사진 링크와 측정값이며 모든 모션의 clipping-free 동작을 인증하지 않는다.

## Weighted — OFWeightB1031

### 공격 각도 0°

실제 재생률 1.393600; 수락→첫 피해 0.122327s, 기대 지연 0.107635s, 오차 0.014692s. 허용 구간 [-0.019114, 0.123327]s는 실제 관측 프레임 간격을 따른다. 고정16.7ms 이내라고 해석하지 않는다.

입력 전 준비: world 17.452303s / montage 0.000000s / 요청→촬영 1프레임, 0.075427s.

![Weighted 입력 전 준비 0도](Weighted/15_level_ready_before_LMB.png)

실제 수락 직후: world 17.934285s / montage 0.025244s / 요청→촬영 0프레임, 0.000000s.

![Weighted 실제 수락 직후 0도](Weighted/09_level_actual_LMB.png)

최초 피해 관측: world 18.056612s / montage 0.195720s / 요청→촬영 0프레임, 0.000000s.

![Weighted 최초 피해 관측 0도](Weighted/16_level_first_damage_observed.png)

action·쿨타임·combo 회복: world 18.802346s / montage 0.000000s / 요청→촬영 1프레임, 0.016667s.

![Weighted action·쿨타임·combo 회복 0도](Weighted/17_level_action_recovered.png)

### 공격 각도 -30°

실제 재생률 1.393600; 수락→첫 피해 0.110662s, 기대 지연 0.107635s, 오차 0.003027s. 허용 구간 [-0.025802, 0.017667]s는 실제 관측 프레임 간격을 따른다. 고정16.7ms 이내라고 해석하지 않는다.

입력 전 준비: world 19.935275s / montage 0.000000s / 요청→촬영 1프레임, 0.032339s.

![Weighted 입력 전 준비 -30도](Weighted/18_downlook_ready_before_LMB.png)

실제 수락 직후: world 20.443471s / montage 0.034564s / 요청→촬영 0프레임, 0.000000s.

![Weighted 실제 수락 직후 -30도](Weighted/10_downlook_actual_LMB.png)

최초 피해 관측: world 20.554133s / montage 0.188782s / 요청→촬영 0프레임, 0.000000s.

![Weighted 최초 피해 관측 -30도](Weighted/19_downlook_first_damage_observed.png)

action·쿨타임·combo 회복: world 21.336484s / montage 0.000000s / 요청→촬영 1프레임, 0.016667s.

![Weighted action·쿨타임·combo 회복 -30도](Weighted/20_downlook_action_recovered.png)

## Original — OFOrigB1031

### 공격 각도 0°

실제 재생률 1.393600; 수락→첫 피해 0.115349s, 기대 지연 0.107635s, 오차 0.007714s. 허용 구간 [-0.017667, 0.116349]s는 실제 관측 프레임 간격을 따른다. 고정16.7ms 이내라고 해석하지 않는다.

입력 전 준비: world 17.408935s / montage 0.000000s / 요청→촬영 1프레임, 0.084217s.

![Original 입력 전 준비 0도](Original/15_level_ready_before_LMB.png)

실제 수락 직후: world 17.907296s / montage 0.023227s / 요청→촬영 0프레임, 0.000000s.

![Original 실제 수락 직후 0도](Original/09_level_actual_LMB.png)

최초 피해 관측: world 18.022645s / montage 0.183977s / 요청→촬영 0프레임, 0.000000s.

![Original 최초 피해 관측 0도](Original/16_level_first_damage_observed.png)

action·쿨타임·combo 회복: world 18.796706s / montage 0.000000s / 요청→촬영 1프레임, 0.016667s.

![Original action·쿨타임·combo 회복 0도](Original/17_level_action_recovered.png)

### 공격 각도 -30°

실제 재생률 1.393600; 수락→첫 피해 0.112220s, 기대 지연 0.107635s, 오차 0.004585s. 허용 구간 [-0.017667, 0.017667]s는 실제 관측 프레임 간격을 따른다. 고정16.7ms 이내라고 해석하지 않는다.

입력 전 준비: world 19.920783s / montage 0.000000s / 요청→촬영 1프레임, 0.016667s.

![Original 입력 전 준비 -30도](Original/18_downlook_ready_before_LMB.png)

실제 수락 직후: world 20.419188s / montage 0.023227s / 요청→촬영 0프레임, 0.000000s.

![Original 실제 수락 직후 -30도](Original/10_downlook_actual_LMB.png)

최초 피해 관측: world 20.531408s / montage 0.179616s / 요청→촬영 0프레임, 0.000000s.

![Original 최초 피해 관측 -30도](Original/19_downlook_first_damage_observed.png)

action·쿨타임·combo 회복: world 21.316865s / montage 0.000000s / 요청→촬영 1프레임, 0.016667s.

![Original action·쿨타임·combo 회복 -30도](Original/20_downlook_action_recovered.png)
