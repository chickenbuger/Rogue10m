# 3인칭 상하 시선과 팔 자세

2026-11-21 `Sprint#16-2-arm-look-pitch`의 UE 5.8 StartHub에서 맨손 대기, 맨손 가드, 대검 준비를 검증했다. Manny 원본과 인간 남성 리타깃 본의 위치와 회전을 같은 애니메이션 시점과 컴포넌트 좌표에서 기록했다. 대검은 `/Game/Weapons/Starter20261110/SM_StarterGreatSword`가 `hand_r`에 붙어 있는 상태였다.

기존 애니메이션 클립과 몽타주에서 시선이 만드는 추가적인 상하 회전을 끊었다. 마우스 전투 검사는 `/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab`을 포함한 입력 수락을 확인했다. 실행은 프로그램 입력과 NullRHI 수치 검사다. 준비 자세에서 완료한 본 비교와 달리 타격 순간의 궤적이나 물리 입력은 이번 검사 범위 밖이다. [결과](../../Feature/doc/2026-11-21_arm-look-pitch.md)
