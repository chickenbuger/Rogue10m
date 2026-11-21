# 기본 회피와 어깨 구르기

Alt에서 재현한 기본 3인칭 회피는 전방 조깅에서 만든 스텝을 전방에서만 재생한다. 측면과 후방에서는 캐릭터가 바라보는 방향을 기준으로 이미 연결된 방향별 이동 자세를 사용한다. 최신 통합본의 왼쪽 Ctrl 바인딩으로 같은 경로가 실행된다.

전수한 구르기는 Manny의 전용 제자리 클립을 실제 회피 진행률로 재생한다. 초반과 후반은 일반 이동 자세로 블렌딩한다. 몸과 IK 체인은 구르기 시작과 복귀에 걸쳐 이동 방향에 맞춰 서서히 회전한다. 대검은 몸이 접히는 동안 오른손에 붙어 바깥쪽으로 기울어지며, 회복할 때 두 손 자세로 돌아온다. 이동은 캐릭터 이동 컴포넌트가 캡슐과 바닥, 벽을 처리하는 경로에서 결정한다.

로컬 VaultCache에 있는 Flexible Combat System 구르기를 참고해 손, 어깨, 발이 순서대로 땅에 닿는 타이밍을 확인했다. 프로젝트 클립의 키는 Blender에서 Manny 리그에 맞춰 새로 만들었다. Blender 비교의 이전 포즈에서는 머리 프록시가 바닥을 약 24cm 통과했다. 보정한 클립에서는 접힌 구간의 머리 프록시가 바닥과 22cm 이상 떨어져 있었고, 전방 구르기의 짚는 손이 지나치게 뒤로 미끄러지지 않도록 손 위치를 조정했다. [Fab 자산 정보](https://www.fab.com/listings/40d402ad-0b0d-42c5-ab14-fffe458f6308)

## StartHub 검증

- UE 5.8 Editor 빌드 성공. 기본 3인칭 검사는 1,604개와 마우스 입력 13건을 통과했다. 실제 회피 키와 방향 키를 프로그램으로 입력했을 때 앞쪽 259.18cm, 뒤쪽 259.21cm, 오른쪽 258.40cm를 이동했다. 벽에 부딪히면 39.97cm에서 멈췄다. 후방과 오른쪽에서는 전방 몽타주가 0프레임이었다. 같은 통합본의 맨손 대기·가드·대검 준비에서 카메라를 ±65° 움직였을 때 팔과 머리 본의 위치 차이는 0cm, 회전 차이는 최대 0.000003°였다. [검사 결과](evidence/alt-dodge-motion-20261121/third-person-results.txt)
- 전수한 구르기는 기존 아이템 인벤토리에서 대검을 장착하고 실제 스킬트리에 네 노드를 배운 뒤 실행했다. 스킬 강화 상태에서 304.56cm 이동하며 22개 진행 프레임을 기록했다. 머리 본의 최저 바닥 여유는 32.28cm였다. 대검 메시의 보수적인 바운딩 박스는 얇은 축을 세우도록 보정한 뒤 모든 측정 프레임에서 바닥 위에 있었고, 최소 여유는 1.05cm였다. 회피 중 입력 수용 제한과 회복 자세도 확인했다. 두 수치 실행의 Unreal 출력 오류는 0건이다. [구르기 결과](evidence/alt-dodge-motion-20261121/roll-numeric.txt) · [프레임별 측정](evidence/alt-dodge-motion-20261121/roll-motion.csv)
- 1280×720 소프트웨어 렌더에서 준비, 몸이 접히는 구간, 회복을 확인했다. 접힌 자세에서 대검 손잡이는 오른손에 있고 칼날과 그 그림자가 분리됐다. 회복 때 양손 그립이 돌아왔다. 준비 캡처에서는 아린이 플레이어 몸통 일부를 가린다. 렌더 실행에는 NVIDIA 어댑터 초기화 오류 1건이 남았고 Microsoft Basic Render Driver로 이어서 모든 게임 검사를 통과했다. [렌더 결과](evidence/alt-dodge-motion-20261121/roll-rendered.txt) · [준비](evidence/alt-dodge-motion-20261121/roll-ready.png) · [구르기](../../DevLog/20261121_AltDodgeMotion/roll-active.png) · [회복](evidence/alt-dodge-motion-20261121/roll-recovered.png)

검증은 격리된 StartHub에서 컨트롤러 입력을 재현했다. 대검 구르기의 고정 카메라는 전방 회피를 촬영했다. 측면·후방에서 배운 구르기와 전체 플레이의 검증은 별도로 남아 있다. [이전 후보 기록](evidence/alt-dodge-motion-20261121/roll-verification-attempts.txt)

[설계와 점검 범위](../architect/2026-11-21_animation-motion-audit.md) · [Blender 비교](evidence/alt-dodge-motion-20261121/blender/README.md) · [동작 기록](../../DevLog/20261121_AltDodgeMotion/AnimationRecord.md)

### 카메라 모드별 회피 재검사

현재 입력의 회피 키는 왼쪽 Ctrl이다. StartHub에 정상 진입한 뒤 실제 시점 입력으로 3인칭 하한 −70°를 확인했다. 같은 검사에서 분리한 테스트 캐릭터를 기존 전신 1인칭으로 전환해 하한 −85°와 아래를 보면서 달리기, 전방 회피 및 벽 충돌을 확인했다. Original 936개와 Weighted 940개 수치 검사가 통과했다. [카메라 모드별 수치](evidence/look-dash-20261029/camera-mode-regression-20261121.txt)

### 왼쪽 대검 구르기

대검을 장착하고 공용 구르기를 전수한 여섯 외형이 왼쪽 A와 Ctrl 입력으로 각각 304.56cm 이동했다. 캐릭터마다 22개 자세를 60Hz에서 기록했다. 대검 경계 상자의 최저 바닥 여유는 드워프 남성 +0.511cm, 드워프 여성 +0.556cm였다. 인간과 오크 외형도 바닥 위에서 가드로 돌아왔다. [외형별 검증 수치](evidence/alt-dodge-motion-20261121/left-sword-roll-20261121.txt)
