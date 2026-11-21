# 대검 가드 전환과 공격 입력 관찰

2026-11-21 KST, 독립 작업 사본 `CombatMotionWorktree20261121`, `Sprint#18-2-combat-animation-continuity`. 최초 비교 기준 `2666013d`, 최종 통합 기준 `a24f522e`. 실행은 `StartHub`의 정상 프로필 진입 뒤 QA 저장 슬롯에서 했다. [결과와 재현](../../Feature/doc/2026-11-21_combat-animation-continuity.md)

## 애니메이션과 출처

- 기존 대검 가드 `/Game/Rogue10m/Animation/Combat/GreatSword20261110/A_GreatSword_Idle`, 지상 `BS_GreatSword_Ground`와 `A_Armed_MM_Idle` 및 방향 이동 표본을 사용했다. 원본 Manny 호환 클립과 대검 리타깃의 출처는 이전 검수에 있다. [원본 및 파생 에셋](../../Feature/doc/2026-11-10_sword-knuckle-visuals.md)
- 맨손 `DA_BasicBrawler_LeftJab`은 `/Game/Rogue10m/Animation/Combat/FistAuthored/AM_Unarmed_LeftJab`과 `A_Unarmed_LeftJab`을 참조한다. 대검 `DA_Attack_GreatSword_Charged`는 `/Game/Rogue10m/Animation/Combat/GreatSword20261110/AM_GreatSword_Charged`와 `A_GreatSword_Charged`를 참조한다. 두 몽타주는 `/Game/Characters/Mannequins/Meshes/SK_Mannequin`을 사용한다.
- 후보의 변경은 일반 3인칭 대검 이동 진입 때 가드의 하강 시간이 최대 0.18초가 되는 것이다. 에셋·Montage·Sequence는 기존 그대로 재생한다. RMB 해제와 LMB 수락은 실제 게임 컨트롤러 입력에서 관찰했다.

## 관찰 지점

- 정지 대검 가드에서 W 입력 후 첫 표본은 속도 34.13cm/s였다. 기준 대비 보이는 흉부는 27.455°에서 0.714°, 골반 상대 오른손은 19.434cm에서 0.460cm, 골반 상대 왼발은 12.008cm에서 0.298cm로 줄었다. 이후 일곱 표본의 최대값과 300cm/s 대조군은 검수 결과에 있다.
- 맨손 공격 시작·이동 진입에는 동일한 몽타주 시간이 사용됐다. 0.078초의 골반 상대 오른손 이동은 제자리 11.53cm, 이동 2.88cm였다. 대검 차지는 눌림 48프레임과 해제 후 실제 공격 몽타주 관측 72프레임을 기록했다. 최초 피해와 손·무기의 물리 접촉은 수집 범위 밖이다.
- 기준과 후보의 위치는 소스와 외형의 컴포넌트 좌표 및 골반 상대 cm이고, 회전은 컴포넌트 quaternion 차이다. 각 프레임의 리비전과 월드 시간을 CSV에 기록했다. 60Hz NullRHI의 수치와 별도 프로그램 입력 소프트웨어 렌더를 구분한다. 사람의 물리 키와 일반 필드 완주는 미실시다.
- 소프트웨어 렌더의 일반 이동에서 고정 측후면 카메라로 시험 5번째 프레임(월드 0.400초)에 이동 전 자세, 12번째 프레임(월드 0.516667초)에 가드 전환 중 자세의 캡처를 요청했다. 1280×720 PNG 두 장의 생성 완료를 확인했다. Microsoft Basic Render Driver를 사용했고, 하드웨어 탐색 오류 `887A0004` 이후 Unreal 오류는 0개였다. [이동 전](../../Feature/doc/evidence/combat-animation-continuity-20261121/01_sword_guard_before_W.png) · [전환 중](../../Feature/doc/evidence/combat-animation-continuity-20261121/02_sword_guard_fading.png)
