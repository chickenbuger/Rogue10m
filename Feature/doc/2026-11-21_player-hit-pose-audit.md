# 플레이어의 작은 피격 자세

3인칭 캐릭터가 땅에 서 있거나 걸을 때 체력이 실제로 감소하는 비치명적 피격에 0.25초의 상체 반응을 더했다. 전방과 후방에서 맞을 때 척추가 다른 방향으로 움직인다. 짧은 시간에 연달아 맞으면 피해와 공격 중단은 매번 처리하고, 이미 시작한 자세는 첫 반응을 끝까지 재생한다.

## 원본과 적용 경로

- 프로젝트의 Manny 원본 `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01`과 `MM_HitReact_Back_Med_01`을 사용했다. 원본의 뒷걸음·총 자세·머리 움직임을 제외하고 `spine_01`부터 `spine_04`까지 회전 변화만 Blender에서 줄이고 다시 타이밍을 맞췄다.
- Blender의 `MannyPlayerHitReaction.blend`에 전방·후방 액션을 담았다. 양쪽 클립은 같은 Manny 리그에 맞으며 120Hz 애니메이션 FBX를 새 Blender 장면에서 다시 불러와 검증했다. 루트·골반·양발과 끝 자세가 첫 프레임과 일치한다.
- 활성 파생본은 `/Game/Rogue10m/Animation/Combat/FighterStance/A_PlayerHitRecoil_Front`와 `A_PlayerHitRecoil_Rear`다. 둘 다 Manny 스켈레톤의 첫 프레임을 기준으로 가산 재생한다. C++ 프록시는 지정된 척추 회전만 강도 0.78로 섞는다. 5프레임(0.083초)에 최대가 되고 0.25초에 중립으로 돌아간다.
- 실제 `TakeDamage`의 기존 공격 중단 뒤 체력 감소를 확인해 자세를 시작한다. 점 피해 방향, 범위 피해 중심, 피해를 준 액터 위치 순으로 방향을 읽는다. 막힌 피해와 치명상 방지 특성은 새 자세를 시작하지 않는다. 기존 공격 몽타주 중단 시간 0.08초를 사용하며 이동·피해 판정·사망 흐름은 기존 경로를 따른다.

## 첫 여섯 프레임

StartHub Human Male 프로필에서 점 피해 입력 전 1개와 직후 6개를 60Hz로 기록하고, 무피격 대기의 같은 프레임 수를 대조했다. 수치는 실제 표시 메시의 컴포넌트 공간이며 몬스터 AI 연출과 물리 마우스 입력은 별도 범위다. 표는 최초 적용본 비교 D의 수치다.

| 항목 | 기존 | Blender 적용 후 |
| --- | ---: | ---: |
| 전방 피격의 최대 추가 위치 이동 | 오른손 0.54cm | 왼손 3.57cm |
| 후방 피격의 최대 추가 위치 이동 | 왼손 0.73cm | 머리 3.28cm |
| 5프레임 전방·후방 흉부 회전 | 대기 변화 내 | 5.69° / 5.54° |
| 5프레임 전방·후방 머리 회전 | 대기 변화 내 | 1.38° / 0.30° |
| 전방·후방 최대 추가 발 이동 | 대기 변화 내 | 0.053 / 0.043cm 이내 |

기존 피격 대기에는 새 몽타주가 없었고 두 방향에서 실제 체력이 각각 1 감소했다. 대기와의 차이는 최대 0.73cm 및 국소 회전 0.61°였다. 적용본도 두 방향에서 실제 체력이 감소했다. 거부한 점 피해는 체력 및 반응을 유지했고, 0.033초에 반대 방향에서 또 맞은 경우 체력만 추가로 감소하면서 첫 반응의 방향과 시각을 유지했다.

실제 왼쪽 마우스의 `/Game/Rogue10m/Animation/Combat/FistAuthored/AM_Unarmed_LeftJab`을 같은 몽타주 시각에서 중단했다. 처음 표시 중량 0.376이 다섯 표본에 걸쳐 0으로 떨어졌다. 최종 비교에서 피격 직전의 골반 차이는 0.192cm였다. 원래 공격을 끝까지 놓아둔 경로와 중단된 경로의 루트 좌표 차이는 몽타주와 대기의 기준 차이를 포함하므로 접지 변위 판정에 사용하지 않았다.

두 번째 점프를 통합한 기준 `48de957d`의 E에서 같은 잽 중단을 가산 강도 0과 기본 강도로 대조했다. 두 경로의 몽타주 중량은 정확히 일치했다. 가산 적용으로 다섯 번째 프레임에 표시 흉부 5.80°, 머리 0.24°, 오른손 3.78cm가 더 움직였고 양발의 추가 이동은 최댓값 0.059cm였다. 대기·거부·단일 및 연속 피격에서 카메라의 위치 변화는 0cm, 회전 변화는 최대 0.000003°였다.

## 검증

- UE 5.8 통합 빌드와 `PlayerHitPoseD`의 프로그램 입력 수치 검사 1,785개를 오류 없이 통과했다. `48de957d` 기준의 `PlayerHitPoseE`는 8개 상황과 수치 검사 2,048개를 통과했다. 같은 통합 바이너리에서 이단 점프 2,976개와 패왕참 준비·발동 네 상황을 다시 확인했다. 첫 시도 A는 Human Male 리그에 없는 `spine_05`를 요구해 멈췄다. 공유 본 16개로 맞춰 B에서 기존 상태를 확인했다. [초기 검증 기록](evidence/player-hit-pose-20261121/baseline-setup-notes.txt) · [통합 결과](evidence/player-hit-pose-20261121/runtime-results.txt) · [실제 포즈 자료](evidence/player-hit-pose-20261121/runtime-poses.csv)
- 재현 명령은 `./Scripts/VerifyThirdPersonGameplay.ps1 -Label FreshPlayerHit -PlayerHitMotion -NullRHI`다. 별도의 렌더 모드 `-PlayerHitMotionRender`는 같은 카메라에서 대기와 전방 피격 5프레임의 PNG를 저장한다.
- `PlayerHitPoseWarpA`가 1280×720 Microsoft Basic Render Driver에서 544개 검사를 통과했다. 같은 각도에서 앞쪽 어깨와 흉부가 움찔하고 머리와 팔 자세가 이어지는 것을 확인했다. 발은 화면에 잘려 수치로 판정했다. 시작 중 하드웨어 탐색 오류 `887A0004` 1건을 원본 924행에 보존했고 소프트웨어 어댑터 선택 이후 새 오류는 없었다. [대기](evidence/player-hit-pose-20261121/idle-control.png) · [전방 피격](../../DevLog/20261121_PlayerHitPose/front-recoil.png) · [렌더 결과](evidence/player-hit-pose-20261121/render-results.txt)
- 결과를 저장한 기준 작업 사본은 `D:/Project/Rogue10m/tmp/PlayerHitMotionAudit20261121`이며, Blender 프로젝트와 원본 읽기·가져오기 보고서는 `D:/Project/Rogue10m/tmp/PlayerHitBlender20261121`에 있다. 현 수치는 Human Male 지상 자세에 해당한다.
