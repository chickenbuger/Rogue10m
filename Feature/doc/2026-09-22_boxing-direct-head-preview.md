# Boxing.fbx 직접 리타깃과 머리 카메라 미리보기

사용자 원본의 실제 복싱 클립을 Manny 체형으로 리타깃해 현재 플레이어 전신 메시에서 직접 재생했다. 머리 움직임을 위치·회전 모두 1배로 따라가는 실제 게임 화면을 원본 속도와 0.5배속으로 촬영했다. 기존 절차형 권사 포즈를 겹치지 않았다.

- 브랜치: `Sprint#4-35-boxing-direct-head-preview`
- 엔진: UE 5.8.2
- 상태: 구현·빌드·실게임 촬영·독립 소스/시각 검토 완료. 커밋·푸시 없음.
- 계획: `Feature/architect/2026-09-22_boxing-direct-head-preview.md`

## 실제로 보이는 느낌

원본은 약 1.73초 동안 왼발을 내딛고 왼손을 뻗은 뒤 회수하는 동작이다. 몸을 앞으로 실을 때 시야가 전진하고 기울었다가 돌아온다. 머리 추종 이동량 최대 67.494cm, yaw 13.954도, pitch 6.890도, roll 4.539도가 측정됐다. 카메라 제한에 닿은 프레임은 없다.

원본의 가드와 타격 높이가 낮아 정면 머리 시점에서는 손이 화면 아래로 빠지고 하단 HUD와 겹친다. 원본 손 위치를 바꾸지 않고 별도 보조 시점을 제공했다. 최종 비교 영상 왼쪽은 정면·기존 HUD, 오른쪽은 아래로 40도 바라본 시점·UI 제외다. 오른쪽에서 왼팔 전진·펴짐·회수가 보인다. 25도 아래보기 중간 검토에서는 HUD 가림이 남아 최종 보조 시점을 조정했다.

영상은 준비 자세 → 원본 속도 → 준비 자세 → 0.5배속 → 원본 끝 자세 → 기존 권사 가드 복귀 순서다. 각 영상 278프레임, 30fps, 9.266667초이며 전환은 블렌딩 없이 구분했다.

- 비교 영상: `images/boxing-direct-head-20260922/boxing-direct-comparison.mp4` (1920×540)
- 정면 원본: `images/boxing-direct-head-20260922/boxing-direct-head.mp4` (1280×720)
- 동작 확인 보조: `images/boxing-direct-head-20260922/boxing-direct-lookdown.mp4` (1280×720)

## 구현과 보존 범위

`Scripts/Editor/PreviewBoxingRetarget.py`가 `/Game/Rogue10m/Animation/Preview/Boxing`에만 소스 메시·시퀀스·IK 리그·리타깃 결과를 저장한다. 정적 `Take 001` 대신 실제 `mixamo.com` 스택을 사용했다. 결과는 `/Game/Rogue10m/Animation/Preview/Boxing/A_Boxing_Manny.A_Boxing_Manny`다.

`Source/Rogue10m/Tests/Rogue10mBoxingDirectPreviewRuntimeTest.cpp`는 Editor 빌드의 standalone 촬영 명령이다. 실제 FirstPersonMesh에 SingleNode 시퀀스를 재생하고 같은 샘플에서 최종 카메라를 갱신한다. 머리가 숨겨진 렌더용 포즈가 아니라 시퀀스 원본 포즈를 평가한다. 카메라 기준점은 원본 첫 프레임 머리의 캡슐 좌표에 전방 12cm·상방 8cm 눈 오프셋을 더한 해부학적 근사다. 촬영 중 위치/회전 추종을 1로, 평활화를 0으로 두고 종료 때 원래 카메라 위치와 설정을 복구한다.

골반·머리의 전진을 지우지 않았지만 root motion을 캡슐 이동으로 추출하지는 않는다. 원본 FBX와 공용 메시·스켈레톤·스킬 프로필 4파일은 SHA256으로 보존을 확인했다. 일반 게임의 공격 입력·피해·카메라 기본값은 변경하지 않았다.

## 검증

- 최종 Editor 빌드 성공: 7.86초. 정면 278프레임과 최종 보조 278프레임 모두 실패 0. 중간 25도 촬영도 278프레임 수치 검사 통과했다.
- `Scripts/ValidateBoxingRetarget.py`: 실제 53포즈 비교. 왼손 전방 궤적 상관 0.999781, 최대 타격 프레임 13 일치. 골반 전방/수직 상관 0.999996/0.999991. 양발 전진 방향·앞발 우세 보존.
- 프레임별 원본 머리 delta와 적용 카메라 비교: 위치 오차 0cm, 회전 오차 최대 0.000002도, 제한 도달 0프레임.
- 캡슐·메시·입력 시선 유지, 공격 발생 없음, 기존 애니메이션과 카메라 복구 검사 통과.
- 기획 1 / 개발 2 / 검증 2 역할로 진행. 기획 담당이 독립 소스 검토, 자산 담당이 자신이 작성하지 않은 런타임 결과의 화면 검토를 맡았다. 루트가 빌드·영상·통합 기록을 수행했다.
- 실제 대표 프레임 검토에서 원본 타격·회수와 native 가드 복귀 확인. 발 전체의 접지 품질은 화면에 충분히 보이지 않아 시각 판정에서 제외했다. 패키징과 공격 입력 연결은 이번 범위에 포함하지 않았다.

검증 로그·원본 대비 수치·소스/시각 리뷰·해시·재현 촬영 스크립트는 `evidence/boxing-direct-head-20260922/`에 보존했다.

## 다시 보기

새 standalone 게임을 `-game -windowed -ResX=1280 -ResY=720 -ExecCmds="Rogue10m.PreviewBoxingDirect"`로 실행한다. 아래보기 보조 촬영에는 `-RogueBoxingLookDown`을 추가한다. 명령은 촬영 후 프로세스를 종료하므로 작업 중인 Editor/PIE에서는 사용할 수 없다. 실제 캡처는 Saved/Screenshots/WindowsEditor의 BoxingDirectPreview 또는 BoxingDirectPreviewLookDown40 폴더에 생성된다.
