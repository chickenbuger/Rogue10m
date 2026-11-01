# 기존 공격·대시 애니메이션 관찰

2026-11-01 UTC, D:\Project\Rogue10m. L 실행 당시 branch Sprint#9-23-normal-growth-boss-qa, HEAD c9cca558, QA 소스 A23D08BD의 미커밋 상태였고 이후37aa0056으로 보존했다. 새 애니메이션 제작·원본 수정·재인코딩은 없다. 프로그램 입력으로 얻은 실제 사진이다. 사람의 직접 플레이 기록은 아니다.

| 관찰 | world 초: 요청 → 처리 | 사진과 한계 |
|---|---|---|
| 공격 전 idle |108.603995 →108.612329|[준비 사진](evidence/Captures/21_animation_attack_ready.png). 실제 windup 자세가 아니며 강한 블러가 있다.|
| 최초 HP 감소 후 |111.824387 →111.935217|[첫 피해 사진](evidence/Captures/23_animation_attack_first_damage.png). 관측보다 약0.111초 뒤 처리되어 정확한 contact 프레임이 아니다.|
| 대시 전 idle |108.735464 →108.743797|[준비 사진](evidence/Captures/22_animation_dodge_ready.png). 준비 상태 관측이며 선행 동작의 품질 증거가 아니다.|
| IsDodging 활성 |112.092733 →112.101066|[대시 사진](evidence/Captures/24_animation_dodge_active.png). 전경 메시가 크게 가려 시야·자세·궤적을 정상으로 인정하지 않았다.|

공격 회복 PNG는 처리 때 상태가 달라 제외했고, 대시 회복은 촬영 큐 때문에 누락됐다. 다른 프레임으로 채우지 않았다. 원본 실패 자료는 그대로 남겼다. 사진4장 모두1280x720 열기/전체 디코딩/원본SHA 일치와 직접 시각 검토를 확인했다. 상태·파일 일치는 시각 품질 PASS가 아니다.

대검 Primary01의 실제 Skill은 /Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Primary01, Montage는 /Game/Rogue10m/Animation/Combat/GreatSword/AM_GreatSword_Primary01, 재사용 segment는 /Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack이다. L의 실제 입력은 짧은 기본 공격이며 clip 이름만으로 차징 입력이라고 해석하지 않는다. 기록된 hit delay0, play rate0.78, cooldown0.32초와 Montage길이1.8333초를 정확한 접촉/회복 시점으로 혼동하지 않는다. 대검 전용 스윙·무기 외형 품질은 미검증이다.

대시 자산은 /Game/DataAsset/DodgeSkill/DA_Dodge_Unarmed와 /Game/Rogue10m/Animation/Combat/HeadSafe/AM_Safe_Dodge_Step, segment A_Safe_Dodge_Step이다. 명목1000cm/0.16초, cooldown0.45초, 비용10이다. 이 사진은 forest의 world112 실행이고,305.96cm 거리 관측은 boss의 world459 실행이므로 같은 동작의 전후 증거로 합치지 않는다.

source animation mesh의 SKM_Manny_Simple, retarget body의 SK_Hu_M_FullBody와 owner view 표현의 역할을 구분했다. 손 world 좌표에는 캐릭터 이동·회전이 포함되므로 손 local/weapon-tip 궤적이 아니다. 실제 표시 mesh를 사진만으로 단정하지 않는다. 정확한 contact/GPU 시간, 회복 자세, 시각 클리핑 해결은 남은 검증이다.

evidence에 원본 raw3개, 상세 자산/타이밍/손 좌표/사진 대조와 해시를 함께 보존했다. 실행 중인 M의 기록을 이 결과에 섞지 않았다.
