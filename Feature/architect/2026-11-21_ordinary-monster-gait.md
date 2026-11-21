# 필드 일반 몬스터의 대기와 보행 전환

OpenRunMap에 연결된 초록 점액체, 붉은 점액체, 철갑 등껍질의 실제 데이터로 한 묶음의 A/B 검수를 진행한다. 월드 파티션 외부 액터와 `DA_OW_*`의 기본 공격 프로필을 기준으로 삼는다. Dungeon Mason Monster For Survival Game의 Polyart Slime, PBR Slime, PBR TurtleShell 원본 클립 및 같은 스켈레톤을 사용한다. 보유 기록은 Fab 재사용 가이드와 기존 로컬 패키지 확인에 근거한다.

격리된 StartHub에서 각 필드 프로필을 사용하는 개체의 대기, 보행 시작, 한 루프 이상 지속 보행, 정지 자세를 60Hz로 비교한다. 같은 입력과 이동 속도를 사용해 몸통·척추·머리와 등껍질의 위치·크기 변형을 기록한다. 전환의 큰 한 프레임 변화가 줄고, 원래 보행 루프의 몸 높이와 스쿼시 리듬이 이어질 때 적용한다. 같은 기본 공격 클립과 피해 접촉 시점, 피격 클립 및 회복을 확인한다.

검증 뒤 실제 `DA_OW_BasicAttack_*`의 `blend_legacy_locomotion` 한 값만 Unreal Editor에서 저장한다. 새 실행에서 저장본을 읽고 같은 비교를 반복한다. 되돌리기 범위는 이 세 프로필과 전용 테스트다. 검증 커맨드는 `VerifyHumanoidMonsterAI20261110.ps1 -OrdinaryWalk <종 ID> -Label <새 라벨>`이다.
