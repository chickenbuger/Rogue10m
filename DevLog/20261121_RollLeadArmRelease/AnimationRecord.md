# 대검 구르기 오른팔 지지 해제 기록

- 프로젝트 애니메이션 /Game/Rogue10m/Animation/Common/Evade/A_CommonShoulderRoll.A_CommonShoulderRoll, 원본 Manny /Game/Characters/Mannequins/Meshes/SK_Mannequin. 기존 프로젝트 소유 Blender v4 편집본을 파생했다. 제작 파일은 tmp/AltMotionLeadArmBlender20261121/MannyShoulderRoll.blend와 같은 이름의 FBX다.
- 키는 제자리 60Hz 25구간, 애니메이션은 0.416667초다. 실제 StartHub에서 네 공용 스킬을 적용하면 구르기 정규화 시계의 0→98.04%를 0.35초간 기록하며, 세 방향에서 캡슐 304.56cm를 이동했다.
- 51.4%는 오른쪽 어깨 쪽으로 지지가 넘어가는 구간이며, 60.7%는 오른팔이 몸 쪽으로 돌아오는 구간이다. 같은 후방 측면 카메라에서 프로그램 키 입력으로 확인했다. 오른팔뚝의 부모 회전과 골반 기준 아래팔 방향, 월드 수평면의 오른손목 움직임을 따로 기록했다.
- 표시 본과 Manny 소스 비교, 공격 수용 제한 및 일어서기, 보수적인 대검 경계의 지면 여유, 720p 비교 화면은 [Feature 결과](../../Feature/doc/2026-11-21_roll-lead-arm-release.md)에 있다.
