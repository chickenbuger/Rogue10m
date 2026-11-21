# 철산고의 어깨 주도 타격

철산고는 기존 `A_Knuckle_ShortBlow`에서 오른손을 앞세웠다. Blender에서 같은 격투 가드를 시작점으로 삼아 팔을 접고, 앞발로 짧게 디디며 오른어깨와 몸통을 넣은 뒤 가드로 돌아오는 동작을 만들었다. 루트가 제자리에 있는 동안 왼발은 지면을 지지한다. [설계](../architect/2026-11-21_knuckle-shoulder-motion.md)

`DA_Attack_Knuckle_Charged`에 전용 `A_Fighter_ShoulderBash`와 `AM_Fighter_ShoulderBash`를 연결했다. 60Hz 110개 구간, 89개 Manny 본, 1.833333초와 기존 `DefaultSlot`의 몽타주 구간을 유지했다. 184.8cm 전방 상자, 0.72초 충전, 0.22초 피해 시작 파라미터와 기존 스킬 재생률 0.9를 사용한다. [Unreal 반입 기록](evidence/knuckle-shoulder-motion-20261121/unreal-import.json)

원본의 13번째 소스 프레임에서 오른손이 어깨보다 41.00cm 앞섰다. 첫 편집본에서는 반대 손이 어깨보다 3.49cm 앞에 남았다. 두 번째 편집에서 Blender 기준 어깨가 5.51cm 앞섰으나 게임에서 왼손이 3.71cm 앞에 남았다. 마지막 편집에서 두 팔을 더 접어 같은 Blender 시점에서 오른어깨가 양손보다 12.00cm 앞선다. 독립 FBX 재반입에서 뼈 길이 오차 0.000043cm, 지지발 끝 최대 이동 0.000054cm, 루트 이동 0cm였다. [FBX 재반입](evidence/knuckle-shoulder-motion-20261121/fresh-reimport.json)

StartHub에서 60Hz 일반 전투 14개 사례를 기존본과 수정본 각각 실행했다. 두 실행 모두 철산고의 첫 피해는 테스트 프레임 1533, 수락 후 0.233333초, 실제 몽타주 위치 0.222600초였다. 대상의 체력 감소는 한 번이었고 두 실행의 Unreal 출력 오류는 0건이었다.

실제로 전수한 철산고를 격투가의 차징 슬롯에 넣고 오른쪽 마우스 버튼을 눌렀다 놓는 별도 비교도 진행했다. 첫 피해는 기존·수정 동작에서 각각 테스트 프레임 124, 수락 후 0.233333초, 몽타주 0.238500초였다. 캐릭터 정면을 기준으로 양손보다 앞선 오른어깨의 위치는 기존 −48.69cm에서 최종 +6.14cm가 됐다. 표시 자세와 Manny, 배우 방향과 양발 좌표를 같은 프레임에 저장했다. [첫 피해 포즈 비교](evidence/knuckle-shoulder-motion-20261121/first-damage-comparison.csv)

전방 상자 판정은 어깨의 외형과 별도로 작동한다. 이 비교의 110cm 표적은 기존 상자 범위 안에 있다. 검사 스크립트 `VerifyMeleeAnimationVfx.ps1 -Label FreshShoulder -ShoulderBash -ExpectAuthoredShoulder -LogicOnly`로 실제 기술 연결을 확인한다. 기존 숏블로 비교에서는 `-ExpectAuthoredShoulder`를 빼면 격리 프로세스에서 원본 몽타주를 사용한다.

1280×720 소프트웨어 렌더에서 준비 61, 첫 피해 124, 회복 226 프레임을 기록했다. 오른쪽 측면에서 앞발을 디디고 어깨를 내미는 자세, 접힌 팔과 가드 복귀를 확인했다. 110cm에서 캐릭터의 표면과 표적 사이에는 간격이 남는다. 세 사진의 요청과 저장은 같은 테스트 프레임이며 Unreal 출력 오류는 0건이다. [준비](evidence/knuckle-shoulder-motion-20261121/ready.png) · [첫 피해](../../DevLog/20261121_ShoulderBash/first-damage-720p.png) · [회복](evidence/knuckle-shoulder-motion-20261121/recovery.png)

작업 사본의 `tmp/ShoulderD`에 Blender 편집본과 FBX가 있다. 기존 ShortBlow를 시작점으로 내보낸 뒤 저작, 독립 반입 확인, Unreal 반입 순으로 실행했다. [저작 스크립트](evidence/knuckle-shoulder-motion-20261121/AuthorShoulderMotion.py) · [Blender 반입 검사](evidence/knuckle-shoulder-motion-20261121/VerifyShoulderFbx.py)
