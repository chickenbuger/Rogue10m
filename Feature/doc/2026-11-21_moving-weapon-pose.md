# 걷기 시작하며 대검을 해제할 때의 자세

인간 남성이 걷기 시작하며 인벤토리에서 대검을 해제하면, 새 리타깃 그래프가 첫 평가부터 직전의 표시 자세를 받는다. 이후 움직이고 있는 다리의 가중치는 새 걸음으로 점차 넘어간다. 기존 0.18초 장비 전환과 기존 보행 시퀀스를 사용했다. [계획](../architect/2026-11-21_moving-weapon-pose.md)

StartHub 평지에서 실제 W 입력을 재현했다. 60Hz에서 수평 속도 136.53cm/s일 때 대검을 해제했다. 인벤토리 호출 자체에서 발생하던 손 또는 발의 최대 29.25cm 이동과 36.04° 회전이 모두 0으로 줄었다. 새 그래프의 다음 프레임에서 손은 1.28cm, 발은 3.31cm 움직였다. 정지 상태에서의 장착·해제 검사도 별도로 통과했다.

이후 보폭에서 왼발의 최대 프레임 이동은 13.65cm였다. 같은 입력으로 대검을 계속 들고 있었을 때는 12.74cm였다. 두 경우 모두 74번째 프레임에서 최대값을 기록했다. 보행 그래프의 정규화된 시계는 전환 전 0.04444에서 0.05556, 0.06667 순서로 이어졌다. [프레임 대조](evidence/moving-weapon-pose-20261121/stride-comparison.csv) · [검증 출력](evidence/moving-weapon-pose-20261121/results.txt)

움직이며 해제하는 재현은 기본 3인칭 인간 남성의 0.18초 설정으로 진행했다. 물리 입력, 전체 필드 플레이, 다른 속도와 카메라 거리에서의 렌더 검증은 미실시다. `./Scripts/VerifyMeleeAnimationVfx.ps1 -Label FreshMovingEquip -SwordEquipAppearance HumanMale -SwordEquipWalking -LogicOnly`로 재현한다.
