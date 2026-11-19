# Blender 머리·목 동작 비교

2026-11-19에 UE 5.8의 `BS_Unarmed_PalmReference` 표본에서 `MM_Idle`, `MF_Unarmed_Walk_Fwd_PalmReference`, `MF_Unarmed_Jog_Fwd_PalmReference`를 내보냈다. 세 클립의 스켈레톤은 `/Game/Characters/Mannequins/Meshes/SK_Mannequin`이며 원본 길이는 각각 7.567초, 1.500초, 1.767초다. 걷기와 달리기는 원본의 루트 모션 설정을 사용했다. 10월 9일 손바닥 보정 작업에서 만든 파생본의 실제 소스 경로와 프레임 수를 수치 자료에 기록했다. 원본 작업 사본과 이번 기능 작업 사본의 세 클립 및 BlendSpace SHA-256도 일치했다. [수치와 원본 경로](blender-metrics.json)

Blender 4.5.14에서 `neck_01`과 `head` 회전만 별도 액션으로 저장했다. 시선의 상하 회전을 제외한 상태에서 기존 감쇠 속도(18/s)를 사용해, 원본 회전의 15%를 반영하는 기존 설정과 35%, 50% 후보를 비교했다. 실루엣은 동일한 본에 붙인 머리·목·어깨 표시이며 피부 메시, 카메라 조작, 플레이 영상은 이 비교의 범위 밖이다. [옆모습 비교](profile-comparison.png)

| 동작 | 기존 머리 회전 폭 | 35% 보정안 머리 회전 폭 | 35% 최대 기준 자세 편차 | 35% 반복 시작·끝 차이 |
| --- | ---: | ---: | ---: | ---: |
| 정지 | 0.89° | 2.17° | 5.00° | 0.01° |
| 전방 걷기 | 0.69° | 1.85° | 4.99° | 0.14° |
| 전방 달리기 | 2.23° | 4.82° | 6.97° | 0.27° |

35%에서는 고개와 목이 보행 리듬을 더 드러내고 원본 달리기의 깊은 고개 숙임을 줄였다. 50% 걷기의 회전 폭은 1.89°로, 35%에 비해 움직임이 0.04° 늘고 아래로 숙이는 각도는 더 커졌다. Unreal에는 35% 후보를 적용했다. 활성 몽타주가 없고 지상에서 3인칭으로 움직일 때, 300~600cm/s 사이에서 회전 상한을 5°에서 7°로 연속 보간한다. 정지에도 5° 상한을 사용한다. 1인칭과 전투·회피·공중 동작은 기존 제한을 사용한다.

재개방한 Blender 파일 3개에서 후보별로 881개 커브의 키, 핸들, 보간이 원래 액션과 일치했다. 편집한 8개 커브는 위 두 본의 쿼터니언이다. 표본에서 비교한 루트·골반·손·발의 최대 위치 차이는 0cm였으며 회전 차이는 0.000007° 미만이었다. [Blender 재개방 결과](fresh-reopen.json)

## 재현

1. 이 폴더의 Python 및 PowerShell 스크립트를 로컬 작업 폴더로 복사한다. `ROGUE_HEAD_EXPORT_SKELETON_ONLY=1`을 설정한 뒤 UE 5.8 `UnrealEditor-Cmd.exe <project> -run=PythonScript -Script=<export_locomotion.py> -nullrhi -unattended -language=en -locale=en -AbsLog=<log>`를 실행한다.
2. 같은 환경 변수에서 `blender.exe --background --factory-startup --python-exit-code 1 --python <compare_locomotion.py>`를 실행한다. `skeleton-only/`에 새 Blender 파일과 프레임별 평가 요약, 비교 렌더를 기록한다.
3. 별도 Blender 프로세스에서 `--python <verify_blender_actions.py>`로 저장 파일을 다시 읽고, `compose_profiles.ps1`로 비교 이미지를 만든다.

원본 FBX와 편집 가능한 Blender 사본은 로컬 `tmp/HeadPitchBlender20261119/skeleton-only/`에 있다. UE 패키지는 읽기 전용으로 확인했고, 새 클립의 필요가 관찰되지 않아 기존 자산을 사용했다.

## 내보내기 실패 기록

- 첫 시도에서 프리뷰 메시를 포함해 NullRHI로 내보내면 `USkinnedMeshComponent::GetCPUSkinnedVertices`에서 `Assertion failed: MeshObject`가 발생했다. 원문은 `export-unreal-native.log` 1861~1903행이며 검사 도구는 오류 40줄을 집계했다.
- D3D11 프리뷰 메시 내보내기는 150초 안에 진행되지 않아 해당 프로세스만 종료했다. 원문은 `export-unreal-rhi.log`다. 별도 PID 43356이 종료됐음을 확인했다.
- 골격 전용 내보내기 `export-unreal-skeleton.log`는 종료 코드 0, UE 오류 0, UE 경고 3으로 끝났다. 사용자의 열린 에디터는 같은 실제 로그의 시작 경계 3435행 이후 신규 오류 0이었다.
