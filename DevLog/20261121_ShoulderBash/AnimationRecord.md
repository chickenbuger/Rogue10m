# 철산고 애니메이션 기록

`DA_Attack_Knuckle_Charged`의 실제 기존 `AM_Knuckle_ShortBlow`와 `A_Knuckle_ShortBlow`를 비교했다. 프로젝트 Manny 가드에서 Blender 4.5.14로 전용 어깨 동작을 만들고 UE 5.8로 반입했다. 60Hz 110개 구간, 1.833333초 몽타주, 고정 루트, 0.9 재생률과 전방 상자 피해 계약을 유지했다.

StartHub 실제 차징 슬롯, 프로그램 오른쪽 마우스 입력. 기존과 최종 첫 피해는 프레임 124, 수락 후 0.233333초, 몽타주 0.238500초다. 표시 어깨가 가장 앞선 손보다 −48.69cm에서 +6.14cm가 됐다. 중간 실패에서는 −3.71cm였다. 비교는 실제 배우 정면으로 투영한 같은 피해 프레임 뼈 위치다. [정량 결과](../../Feature/doc/evidence/knuckle-shoulder-motion-20261121/first-damage-comparison.csv)

실제 720p 측면에서 준비 61, 첫 피해 124, 회복 226의 캡처를 확인했다. 110cm 표적과 표시 어깨 사이의 거리는 기존 상자 판정과 다르다. 프로그램 입력과 렌더를 검증했다. [상세 결과](../../Feature/doc/2026-11-21_knuckle-shoulder-motion.md)
