# 대검 분쇄참 동작 기록

- 격리 작업 사본 `GreatSwordTechniqueWorktree20261121`, 브랜치 `Sprint#26-3-crushing-slash-motion`. 첫 기준은 `af9ffb60`이다. StartHub에서 실제 검을 장착하고 배운 분쇄참을 Q에 연결한 뒤 60Hz로 프로그램 입력을 보냈다.
- 기존 프로젝트의 Mixamo 파생 `A_GreatSword_Primary02`와 `A_GreatSword_Idle`을 Unreal에서 Manny FBX로 내보내 Blender 4.5.14에서 편집했다. 초기에는 양손 준비·내리기·가드 자세를 만들고, 폭이 부족했던 205cm 표적을 재측정해 앞발 디딤을 추가했다. 재반입한 89개 뼈의 111개 키 시점 최대 위치 오차는 0.00020cm였고 루트 위치·방향은 고정이었다.
- 적용한 데이터는 `DA_Hybrid_GreatSword_CrushingSlash`, `AM_GreatSword_CrushingSlash`, `A_GreatSword_CrushingSlash`이다. 몽타주 1.833333초, 수락 틱의 단발 피해와 범위 밖 미피해가 일치했다. 두 손바닥 간격 최대 94.88→28.47cm, 오른손목 한 프레임 이동 22.72→10.39cm였다. [동작·판정 기록](../../Feature/doc/2026-11-21_greatsword-crushing-slash-motion.md)
- 실제 1280×720 프로그램 입력에서 머리 위 준비, 내려베기와 회복을 확인했다. 대표 화면은 정면 205cm 검증의 수락 59프레임 뒤이며, 몽타주 0.763200초다. [대표 화면](downstroke-205cm.png)
