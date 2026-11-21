# 회피 동작 기록

2026-11-21 `Sprint#16-3-alt-dodge-motion`. 기본 회피의 전방 스텝은 `/Game/Rogue10m/Animation/Combat/HeadSafe/AM_Safe_Dodge_Step`으로 연결된다. 기존 Manny 방향별 이동의 `BS_Unarmed_PalmReference`가 측면과 후방 회피에도 해당 방향의 발동작을 제공한다.

문제는 Alt 입력에서 재현했다. 작업 중 통합된 조작 변경으로 회피 입력은 왼쪽 Ctrl이 됐다. 검증기에서는 프로젝트의 현재 회피 키 상수를 통해 입력한다.

전수한 구르기의 제작본은 Blender 4.5.14에서 편집한 Manny 액션 `Manny Shoulder Roll`이다. Unreal 시퀀스는 `/Game/Rogue10m/Animation/Common/Evade/A_CommonShoulderRoll`, 스켈레톤은 `/Game/Characters/Mannequins/Meshes/SK_Mannequin`이다. Unreal 저장본은 0.4167초, 60Hz, 25프레임 간격이고 루트 이동·회전이 0이다. 게임에서는 캡슐 회피 시간에 맞춰 정규화해 재생한다. 기본 구르기 속도에서 저자 프레임 36–48%는 오른손 지지, 52–64%는 오른쪽 어깨 지지, 76% 이후는 발 복귀다. 이 구간은 Blender 표본이며 실제 게임 바닥과 본의 높이는 별도로 기록한다. 회피에는 공격 접촉이나 피해 프레임이 없다.

로컬 VaultCache의 Flexible Combat System - Basic `Roll`에서 지지 순서를 확인했다. 실제 프로젝트 키는 기존 Manny `MM_Idle`의 골격을 사용해 Blender에서 작성했다. [자산과 파일 기록](../../Feature/doc/evidence/alt-dodge-motion-20261121/blender/README.md)

StartHub에서 대검을 실제 장착하고 `회피 후 회복 단축`, `구르기`, `회피 거리 증가`, `구르기 재사용 단축`을 전수했다. 60Hz 실행에서 실제 회피 시간은 약 0.357초였다. 진행률 56%의 포즈에서는 골반이 준비 자세보다 51.86cm 내려갔다. 22개 진행 표본에서 머리 본의 바닥 여유는 최소 32.28cm, 대검 메시 경계의 바닥 여유는 최소 1.05cm였다. 이 측정에는 대검 손잡이와 가드가 포함된다. 회복 시 골반은 준비 높이에서 0.23cm 이내로 돌아왔다. [실행 수치](../../Feature/doc/evidence/alt-dodge-motion-20261121/roll-numeric.txt) · [양손·관절과 칼날 높이](../../Feature/doc/evidence/alt-dodge-motion-20261121/roll-motion.csv)

최종 720p 화면을 준비, 56% 구르기, 회복에서 촬영했다. 준비 화면은 아린이 겹쳐 플레이어 몸통이 일부 가려진다. [준비](../../Feature/doc/evidence/alt-dodge-motion-20261121/roll-ready.png) · [접힘과 검 위치](roll-active.png) · [회복](../../Feature/doc/evidence/alt-dodge-motion-20261121/roll-recovered.png)
