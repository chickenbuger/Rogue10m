# 전방 달리기 착지의 발 접촉과 보폭

StartHub의 기본 3인칭 HumanMale이 맨손으로 전진하며 첫 Space 점프에서 내려올 때, 왼발로 받아 낸 후 다음 달리기 보폭으로 넘어가게 했다. 기존 Manny `MM_Fall_Loop`, `MM_Land`, `MF_Unarmed_Jog_Fwd_PalmReference`를 UE에서 내보내 Blender에서 같은 골격으로 편집했다. 새 60Hz 인플레이스 클립 `A_CommonForwardRunLanding_L`은 0.45초다. `ABP_Common_Unarmed`가 전방용 클립을 선택하고 0.10초 동안 다음 Jog를 섞는다. 접지 +0.400초에 실제 두 번째 Jog 오른발 마커로 인계했다. 정지와 옆 방향의 착지는 기존 `MM_Land`로 재생됐다.

## 60Hz 프로그램 입력 추적

두 발볼이 각각 바닥에서 8cm 안쪽에 있던 연속 표본의 수평 경로를 접지 +67~300ms에서 합산했다. 위치는 실제 게임의 접지 평면과 비교했다. 무릎 각은 허벅지·종아리·발 본으로 계산했다. 발볼 본과 메시 발바닥은 측정 위치가 다르다.

| 실측 접지 | 숨김 Manny 발볼 이동 | 표시 HumanMale 발볼 이동 | 두 번째 Jog R 실측 |
| --- | ---: | ---: | ---: |
| 전방 620cm/s | 187.23 → 82.31cm (−56.0%) | 174.93 → 40.08cm (−77.1%) | 0.852108초 |
| 전방 610cm/s | 185.38 → 80.63cm (−56.5%) | 173.64 → 39.65cm (−77.2%) | 0.851830초 |
| 정지, 우측 620cm/s | 기존 `MM_Land` | 원본과 일치 | 기존 보폭 |

원본 전방 Jog의 오른발 목표는 0.851702초다. 완전히 인계된 뒤 같은 Jog 시각의 원본과 비교한 양쪽 무릎 각도 오차는 1.08° 이내였다. 접지 +67~300ms의 표시 외형 무릎 최대 프레임 차이는 왼쪽 8.88°, 오른쪽 9.33°다. 이 구간의 Manny 발볼 최저 높이는 왼쪽 +0.39cm, 오른쪽 +0.98cm다. 오른발이 다음 보폭에 들어오는 +0.40초에는 표시 외형 오른 무릎이 13.97° 움직였다. 같은 Jog 위상에서 원본 대비 증가량은 약 3.8°이며 이후의 지속된 오차는 없었다.

이동과 정지, 이륙과 접지, 숨김 소스와 표시 외형을 실제 게임에서 수치 검증했다. 임시 평지에서 W/D/Space를 `PlayerController::InputKey`로 주입했다. 물리 입력, 전체 플레이, 경사와 다른 외형의 시각 검수는 별도 범위다. 검사에 쓰인 610cm/s는 게임 세팅을 변경하지 않고 해당 실행의 이동 컴포넌트에만 적용했다.

장비와 피격을 포함한 `e990e106` 위에서 UE 5.8 Editor를 다시 빌드했다. 통합 수치 검수 9,640개가 오류 없이 끝났으며, 변경 전 검수 9,636개와 같은 발볼·무릎 측정 방법을 사용했다. WARP 720p에서는 2,396개를 통과했다. +0.133초 왼발 앞쪽 접촉과 +0.450초 오른발 보폭의 화면에서 몸과 양발을 살펴봤다. 하드웨어 장치 탐색이 한 번 실패했고 Microsoft Basic Render Driver 선택 이후 새 Unreal 오류는 0건이었다. 당시 기존 세이브 40개가 보존됐다. [왼발 접촉](../../DevLog/20261121_ForwardRunLanding/left-support-720p.png) · [달리기 보폭 인계](evidence/player-locomotion-audit-20261121/jog-handoff-720p.png)

기본 카메라·머리·팔·이동·회피 및 마우스 경로의 회귀 검사 1,629개와 마우스 사례 13개를 통과했다. 해당 실행에서 신규 Unreal 오류는 0건이며 기존 세이브 42개가 보존됐다. [3인칭 회귀](evidence/player-locomotion-audit-20261121/third-person-smoke.txt)

`Scripts/VerifyThirdPersonGameplay.ps1 -Label <새 라벨> -ForwardLanding -NullRHI`에서 원본 비교는 `-ExpectStockLanding`을 추가한다. 렌더 실행은 `-NullRHI`를 빼고 `-TimeoutSeconds 260`을 지정한다. 실제 출력과 `forward-landing-frames.csv`는 지정한 라벨에 남는다.

## 참고와 제외한 시험

전환 시간만 줄인 0.20/0.34초 시험은 지면 가까운 발볼 이동을 줄이는 동안 회복 무릎의 최대 프레임 각도 차이를 원본 10.46°에서 각각 31.05°/17.49°로 높였다. 두 시험은 제외했다. 앞서 전방 Jog 위상을 맞춘 v10도 오른 무릎 프레임 차이 14.92°와 왼발 볼 −0.95cm로 제외했다. 편집본 v13은 Blender 재가져오기에서 89개 본 및 28키를 보존했고 재가져오기 전후 최대 위치 차이가 0.00016cm 이하였다. 편집 가능한 `MannyMovingLandLeft.blend`와 FBX 원본은 로컬 Blender 작업 폴더에 보관했다. Unreal의 프로젝트 파생 시퀀스는 기존 Manny Skeleton을 사용한다.

[설계](../architect/2026-11-21_player-locomotion-audit.md) · [60Hz 위상·본 비교](evidence/player-locomotion-audit-20261121/forward-landing-phase-comparison.json) · [제외한 전환 비교](evidence/player-locomotion-audit-20261121/landing-phase-metrics.json) · [Blender 재가져오기](evidence/player-locomotion-audit-20261121/blender-reimport.json) · [Unreal 가져오기](evidence/player-locomotion-audit-20261121/unreal-import.json)
