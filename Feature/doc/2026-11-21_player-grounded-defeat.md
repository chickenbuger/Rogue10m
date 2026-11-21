# 플레이어 지상 패배 동작

실제 탐험에서 지상 치명 피해를 받으면 마지막 자세에서 제자리 쓰러짐으로 이어진다. 기존 결과 카드 아래 보이는 팔과 다리가 달리기 뒤 다시 일어서던 문제가 줄었다. 패배 판정과 이동 정지는 피해 프레임, 결과 표시와 정산은 다음 월드 틱이다.

## 적용 범위와 원본

3인칭 활성 탐험에서 지상 치명 피해를 받은 플레이어의 숨은 Manny에 `A_CommonGroundedDefeat`를 적용한다. 기존 `MM_Death_Front_01`의 상체 동작을 활용하고 Blender에서 수평 루트 이동을 없애며 골반과 발 지지를 편집했다. 60Hz 0.80초 시퀀스와 0.22초 마지막 자세 보간을 쓴다. 몽타주는 마지막 자세를 유지한다. 공중, 시간 만료, 1인칭은 기존 경로를 사용한다.

- 원본 `/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01`, 프로젝트 초기 커밋부터 포함된 Manny 시퀀스.
- 파생 `/Game/Rogue10m/Animation/Common/Defeat/A_CommonGroundedDefeat`, 같은 `SK_Mannequin`, 루트 모션 없음. Blender 원본은 로컬 `D:/Project/Rogue10m/tmp/PlayerDeathAudit20261121/grounded-v2/MannyGroundedDefeat.blend`에 보관했다.

## 확인한 결과

실제 포탈로 `OpenRunMap`에 들어간 기본 HumanMale의 너클 장착 경로를 사용했다. W·Space는 엔진 컨트롤러에서 프로그램 입력했다. 실제 네이티브 피해로 체력 0과 패배를 일으켰다. 발과 손 이동은 표시 메시의 컴포넌트 좌표에서 프레임 간 거리를 측정했다.

| 조건 | 60Hz 확인 | 결과창 | 표시 머리 첫 회전 | 표시 왼발 첫 6틱 최대 이동 |
| --- | ---: | ---: | ---: | ---: |
| 지상 대기 | 46/46 | 1틱 | 7.40→0.33° | 0.00→0.58 cm |
| 지상 달리기 620 cm/s | 46/46 | 1틱 | 16.39→0.28° | 19.34→10.42 cm |
| Space 상승 중 대조 | 45/45 | 1틱 | 9.32→9.32° | 14.63→14.63 cm |

달리기의 표시 오른손은 첫 프레임에 6.26→1.12 cm, 첫 여섯 프레임 중 최대 9.39→13.56 cm였다. 후자는 쓰러지는 손의 이동이다. 지상 달리기에서 표시 골반은 92.03→55.97 cm로 내려갔고 마지막 여섯 틱의 주요 본 좌표는 같았다. 세 조건 모두 캡슐 위치·충돌, 이동 정지, 패배 저장과 다음 틱 결과를 확인했다. 1280×720 소프트웨어 렌더에서는 기존 61/61, 후보 66/66을 통과했고 실제 결과 화면의 Enter를 통해 마을 복귀까지 확인했다. GPU 초기화는 해당 세션에서 887A0004로 실패한 뒤 Microsoft Basic Render Driver로 진행됐다.

Blender를 새로 열어 FBX를 재가져온 결과 89본·49샘플·정지 루트가 일치했다. 위치 오차는 0.00019 cm, 발볼 최저 높이는 0.75 cm, 한 프레임 최대 무릎 변화는 좌 6.01°·우 4.48°였다. 초기 Blender 후보와 기본 슬롯 보간 후보의 탈락 수치는 검수 기록에 남겼다. 검사 기준 커밋은 `ad94906d`, 원래 런 재생 비교 기준은 `41d5a224`이다.

[수치 비교](evidence/player-grounded-defeat-20261121/comparison.json) · [변경 전 결과 화면](evidence/player-grounded-defeat-20261121/baseline-standing-result.png) · [변경 후 결과 화면](../../DevLog/20261121_PlayerGroundedDefeat/grounded-result.png) · [검수 과정](evidence/player-grounded-defeat-20261121/validation.json)

재현은 기존 `Rogue10m.TestRunCompletionDeath` 명령과 새 프로필·진행·키 저장 접두사를 사용한다. `-UseFixedTimeStep -FPS=60 -RoguePlayerDeathMotion=Idle`, `=Jog`, `=Air`로 측정한다. `-NullRHI`는 수치와 저장 검사까지 실행한다. `-d3d11 -RenderOffScreen -AllowSoftwareRendering -RogueDeathMotionCaptureFramesOnly`는 지정한 프레임과 실제 결과 입력을 확인한다.
