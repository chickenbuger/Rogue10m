# 스포어링 보행과 기본 공격 애니메이션 관찰

2026-11-21 KST, 독립 작업 사본 `SporelingWalkWorktree20261121`, 브랜치 `Sprint#20-1-sporeling-motion-audit`, 검증 기준 `cd0b0c86`. StartHub의 전투 테스트 도구로 OpenRunMap의 원래 데이터와 메시를 쓰는 개체를 소환했다. [검증 결과와 재현](../../Feature/doc/2026-11-21_sporeling-walk-transition.md)

- 사용 데이터와 프로필은 `/Game/Rogue10m/World/OpenWorld/Monsters/`의 `DA_OW_Sporeling`과 `DA_OW_BasicAttack_Sporeling`이다. 프로젝트 파생 프로필에서 `bBlendLegacyLocomotion`을 켰다.
- 메시와 스켈레톤은 보유 Monster For Survival Game의 `/Game/MonsterForSurvivalGame/Mesh/Polyart/Mushroom_SK`와 `Mushroom_Skeleton`이다. 다섯 클립 허용 경로는 `/Game/MonsterForSurvivalGame/Animation/Polyart/Mushroom/SmileVersion/`이다. 대기 `Mushroom_IdleNormalSmile_ANIM` 1.333초, 걷기 `Mushroom_walkFWDSmile_ANIM` 1.000초, 기본 공격 `Mushroom_Attack01Smile_ANIM` 0.833초, 피격 `Mushroom_GetHitSmile_ANIM` 0.733초, 사망 `Mushroom_DieSmile_ANIM` 0.833초가 같은 스켈레톤을 사용하며 재생 배율은 모두 1.0이다. 피격·사망은 자산 연결을 확인했다.
- 보행은 기존 C++ 가속·이동을 프로그램 입력으로 실행했다. 각 설정은 90프레임 중 연속 메시 포즈 89쌍을 남겼다. 치마와 머리 본의 위치는 메시 컴포넌트 좌표 cm다. 뼈 위치 차이, 월드 게임 시간, 갱신 리비전, 이동 속도와 거리를 CSV에 남겼다. [저장본 60Hz 표본](../../Feature/doc/evidence/sporeling-walk-transition-20261121/saved-pose-frames.csv)
- 기본 공격 준비 시간 0.350초, 회복 구간 0.483초는 실제 프로필 값이다. 기준과 적용본 모두 21번째 60Hz 표본에서 첫 피해 1 HP, 50번째에서 회복 완료와 대기 자세 복귀를 확인했다. 무기 소켓 접촉 지점·피격 연출·사망 연출의 렌더 화면은 이번 검사 범위 밖이다.
