# 스포어링 대기·보행 전환 점검

기준 `cd0b0c863421cca5f6429dab88488620b76673cb`, 브랜치 `Sprint#20-1-sporeling-motion-audit`, 독립 물리 작업 사본 `SporelingWalkWorktree20261121`.

OpenRunMap의 `DA_OW_Sporeling`이 참조하는 `DA_OW_BasicAttack_Sporeling`은 Monster For Survival Game의 `Mushroom_SK`, `Mushroom_IdleNormalSmile_ANIM`, `Mushroom_walkFWDSmile_ANIM`, `Mushroom_Attack01Smile_ANIM`을 사용한다. 프로젝트의 에셋 표본에서는 대기의 여덟 자세와 보행 첫 자세 사이 `mushroom_skirt08`이 27.66~28.65cm 차이를 보였고, 보행 루프의 처음과 마지막은 일치했다. 이 값은 클립 비교이며 게임 실행의 프레임 측정과 구분한다. 검증 대상의 기존 클립에 공용 보행 블렌딩을 적용할 수 있어 외부 클립과 벤더 원본 편집이 필요하지 않다.

| 작업 | 범위 | 완료 조건과 검증 | 되돌림 |
| --- | --- | --- | --- |
| 실제 전환 진단 | 기존 몬스터 QA에 스포어링 모드를 선택 옵션으로 추가 | StartHub에서 실제 데이터와 메시를 사용하는 복제 개체를 같은 60Hz 이동 경로로 구동한다. 임시 프로필 두 개로 기존 전환과 기존 블렌딩을 대조하며 대기→보행·보행→대기 뼈 프레임 이동, 속도, 이동 거리 및 실제 공격 타이밍을 기록한다. 가시수호자 모드도 다시 실행한다. | 스포어링 QA 옵션만 제거 |
| 조건부 프로필 적용 | 실제 `DA_OW_BasicAttack_Sporeling`의 `bBlendLegacyLocomotion` | 외형 메시의 치마 본에서 재현되는 큰 시작 프레임 변위가 후보에서 줄고, 동일한 230cm/s 보행과 기본 공격 클립, 첫 피해 및 회복이 확인되는 경우 Unreal Editor에서 해당 옵션만 저장한다. 재시작 후 원본 프로필로 같은 QA를 다시 실행한다. | 프로젝트 파생 프로필의 옵션 한 개를 복구 |

정규 필드 이동, 물리 입력 또는 렌더 관찰은 이 수치 점검의 범위 밖이다.
