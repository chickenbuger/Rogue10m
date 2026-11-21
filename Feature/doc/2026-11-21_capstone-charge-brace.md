# 패왕참 차징 준비 자세

대검을 들고 지상에서 정지한 상태로 패왕참 입력을 유지하면 어깨와 손목이 살짝 조여진다. 기존 발동 몽타주로 넘어가면서 준비 동작을 걷어 낸다. 기존 Manny 대검 가드와 공격을 기준으로 Blender에서 키를 편집했다. Unreal Editor에서 첫 프레임을 기준으로 가산하는 120Hz 클립으로 가져왔다. 최대 가중치 0.40은 실제 리타깃된 몸과 검의 움직임으로 조정했다. [설계](../architect/2026-11-21_capstone-charge-brace.md) · [Blender 재검증](evidence/capstone-charge-brace-20261121/blender-validation.json) · [Unreal 가져오기](evidence/capstone-charge-brace-20261121/unreal-import.json)

| StartHub 60Hz 측정 | 기존 | 준비 자세 적용 |
| --- | ---: | ---: |
| 우클릭 유지 중 손목 최대 이동 | 1.15cm | 3.74cm |
| 같은 유지 시간에서 검 축 방향 왼손목 최대 추가 이동 | 기준 | 3.06cm |
| 같은 유지 시간에서 검날 최대 추가 회전 | 기준 | 6.47° |
| 발동 직후 첫 표시의 왼손목 이동 | 0.84cm | 0.86cm |
| 발동 직후 첫 표시의 오른손목 이동 | 0.62cm | 0.51cm |
| 발동 직후 첫 표시의 검날 회전 | 2.32° | 2.42° |
| 최초 피해의 몽타주 시계 | 0.000초 | 0.000초 |

프로그램으로 실제 우클릭을 재현했다. 짧게 떼면 기존 일반 특수기, 충분히 유지하고 떼면 패왕참, 유지 중 좌클릭을 더하면 취소가 실행됐다. 시점 ±65°에서도 같은 차징 단계의 팔과 검 자세를 유지했다. 기존 자세에 더해진 발 위치 변화는 양쪽 최대 0.30cm였다. 기존 몽타주의 시작 이후 19개 표시 표본을 각각 수집했다. [표본](evidence/capstone-charge-brace-20261121/runtime-poses.csv) · [수치 실행](evidence/capstone-charge-brace-20261121/runtime-results.txt) · [이전 후보](evidence/capstone-charge-brace-20261121/calibration-attempts.md)

UE 5.8 Editor 빌드, 패왕참 4건, 기존 대검·너클 근접 공격 14건을 통과했다. 두 수치 실행에서 Unreal 출력 오류는 0건이었다. [근접 회귀](evidence/capstone-charge-brace-20261121/regression-results.txt)

같은 0.333333초의 기존과 보정 자세를 1280×720 WARP 실행에서 확인했다. 고정 카메라에서 보정된 오른팔과 세운 검이 보이며, 양손이 겹치는 부분은 장착한 차징 효과가 일부 가린다. GPU 프로브 오류 1건을 보존했고 소프트웨어 어댑터를 선택한 이후 오류는 0건이다. [기존](evidence/capstone-charge-brace-20261121/hold-stock.png) · [보정](evidence/capstone-charge-brace-20261121/hold-brace.png) · [캡처 시간](evidence/capstone-charge-brace-20261121/render-results.txt)

StartHub의 인간 남성 캐릭터로 확인했다. 전체 필드 플레이와 물리 마우스 입력은 미실시다. Blender 원본 `MannyGreatSwordCapstoneHold.blend`은 로컬 `tmp/GreatSwordCapstoneAnticipation20261121/ReleaseContinuity`에 보존했다. [동작 기록](../../DevLog/20261121_CapstoneChargeBrace/AnimationRecord.md)
