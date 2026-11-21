# 두 번째 점프의 다리 동작

두 번째 Space로 공중에서 다시 상승할 때 왼쪽 다리가 짧게 아래로 뻗었다가 낙하 자세로 돌아오도록 했다. 프로젝트의 Manny 첫 점프와 낙하 클립을 Blender 4.5.14에서 비교했다. 첫 점프의 밀어내는 자세를 약하게 적용해 다리 관절 회전만 담은 0.25초 시퀀스로 만들었다. [작업 계획](../architect/2026-11-21_double-jump-motion-audit.md)

실제 두 번째 점프 이벤트가 시퀀스를 시작한다. 기존 낙하 클립은 같은 속도와 위상으로 계속 재생되며 새 시퀀스의 하체 관절 회전을 그 위에 합친다. 공격이 시작되면 이미 적용 중이던 보정을 약 0.05초 동안 줄인다. 접지, 회피, 다른 카메라 모드에서는 적용 조건이 해제된다. [Blender 측면 대조](evidence/double-jump-motion-audit-20261121/fall-profile.png) · [Blender 두 번째 도약](evidence/double-jump-motion-audit-20261121/second-jump-profile.png) · [Unreal 반입값](evidence/double-jump-motion-audit-20261121/unreal-import.json)

## 실제 Space 입력 비교

StartHub에서 기본 맨손 캐릭터로 안내원 근처에 가서 공중 제어와 이단 점프를 순서대로 전수했다. 같은 해금 상태에서 Space를 한 번 누르는 대조군과 하강 중 두 번째로 누르는 실험군을 60Hz로 비교했다. 프로그램이 실제 게임 컨트롤러로 입력을 보냈다.

두 실행 모두 9프레임의 +403.67cm/s에서 첫 점프가 시작됐다. 이단 점프의 39번째 프레임 수직 속도는 −86.33cm/s였고 40번째 프레임에 점프 횟수가 2, 수직 속도가 +403.67cm/s로 바뀌었다. 착지는 단일 점프 60프레임, 이단 점프 101프레임이었다. 후보와 기준의 수직 속도, 위치 높이, 접지 모드 및 점프 횟수가 209개 게임 프레임에서 일치했다.

기준에서는 두 번째 점프를 지나도 `ABP_Common_Unarmed`의 `Fall Loop`와 `MM_Fall_Loop`의 재생 시간이 이어졌다. 40~52프레임에서 Manny와 외형 메시의 골반·다리 위치와 회전은 단일 점프 대조와 같았다. 후보는 같은 낙하 클립 위에 41~54프레임 동안 다리 보정을 적용했다. 47번째 프레임의 왼발 위치는 기준 대비 Manny에서 19.58cm, 표시 메시에서 19.54cm 달라졌다. 보정 중 골반 위치는 같았다. 두 번째 도약 보정 종료 이후와 두 착지 구간의 골반과 다리 자세는 원래 자세와 일치했다. 첫 대조 실행의 시작 대기 자세는 반복 실행 간 위상이 달라 첫 도약 초기의 표시 메시에서 최대 0.35cm 차이가 났다. 안정된 첫 점프 구간에서는 0.000001cm 이내였다. [기준 비교](evidence/double-jump-motion-audit-20261121/baseline-comparison.json) · [후보 비교](evidence/double-jump-motion-audit-20261121/final-versus-baseline.json) · [도약 전후 본 표본](evidence/double-jump-motion-audit-20261121/boost-pose-window.csv)

UE 5.8 Editor 빌드, 기준 검사 2,959개와 대검 차지 개선을 포함한 최신 기준 `3b1e8047`에서 후보 검사 2,976개를 통과했다. 후보의 신규 Unreal 오류는 0건이고 기존 격리 저장 8개를 보존했다. 같은 빌드에서 카메라·회피·충돌·마우스 13건을 포함한 3인칭 회귀 1,594개, 패왕참 유지·발동 회귀 4건도 통과했다. 두 번째 도약 직후 공중 공격을 연이어 사용하는 경우는 검사 범위 밖이다. [최종 수치](evidence/double-jump-motion-audit-20261121/final-results.txt) · [검증 요약](evidence/double-jump-motion-audit-20261121/verification.json)

## 게임 화면

Microsoft Basic Render Driver의 1280×720 소프트웨어 렌더에서 같은 측후방 카메라로 47번째 프레임의 단일 점프와 두 번째 도약을 촬영했다. 두 장에서 캐릭터 전신과 발을 확인했다. 첫 렌더에서는 초기 바닥 접지를 기다리지 못해 촬영 전에 검사가 멈췄다. 접지 상태를 기다린 뒤 재실행한 결과, 1,748개 검사와 캡처 두 장을 완료했다. 두 실행의 하드웨어 탐색 오류 `887A0004`를 보존했고, 최종 실행에서 소프트웨어 어댑터를 선택한 뒤 Unreal 오류는 0개였다. 사람의 직접 키 입력과 일반 필드 완주는 미실시다. [단일 점프 화면](evidence/double-jump-motion-audit-20261121/01_single_space_at_matched_frame.png) · [두 번째 도약 화면](../../DevLog/20261121_SecondJumpMotion/second-jump-720p.png) · [촬영 결과](evidence/double-jump-motion-audit-20261121/render-results.txt) · [최초 렌더 진단](evidence/double-jump-motion-audit-20261121/first-warp-attempt.txt)

수치 재현은 `./Scripts/VerifyThirdPersonGameplay.ps1 -Label FreshJumpAudit -NullRHI -DoubleJumpMotion -ExpectSecondJumpPose`로 실행한다. 화면은 별도 레이블에 `-DoubleJumpMotionRender -ExpectSecondJumpPose`를 지정한다.
