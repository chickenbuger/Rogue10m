# 초록·붉은 점액체와 철갑 등껍질 보행 전환

OpenRunMap의 초록 점액체, 붉은 점액체, 철갑 등껍질이 멈춰 있다가 걸을 때 대기 클립을 교체하며 자세가 튀었다. 세 종의 기본 공격 프로필에서 검수된 지상 보행 블렌드를 켰다. Dungeon Mason의 Monster For Survival Game에 있는 동일한 Polyart Slime, PBR Slime, PBR TurtleShell의 Idle·Walk 시퀀스를 사용한다. 기본 공격과 피격은 각 종의 기존 시퀀스를 재생한다.

StartHub에 각 OpenRunMap 프로필과 실제 메시를 사용하는 개체를 소환했다. 같은 60Hz 이동 입력을 주고, 몸의 이동을 뺀 메시 좌표에서 인접 프레임의 자세를 비교했다. 보행 시작 후 28개, 정지 전후 32개 프레임의 최대 변화를 기록했다.

| 몬스터 | 측정 본 | 보행 시작 최대 | 정지 구간 최대 | 실측 속도 |
| --- | --- | ---: | ---: | ---: |
| 초록 점액체 | 정수리 `Crown` | 10.637 → 1.289cm | 8.398 → 1.785cm | 220cm/s |
| 붉은 점액체 | 정수리 `Crown` | 10.637 → 1.289cm | 8.398 → 1.706cm | 235cm/s |
| 철갑 등껍질 | 등껍질 뿔 `HornMidTip03` | 3.481 → 0.975cm | 6.977 → 2.248cm | 170cm/s |

같은 속도에서 한 보행 루프를 비교했다. 점액체 정수리의 몸통 대비 높이 변화와 철갑 등껍질의 몸통 대비 높이·회전 변화는 원래 범위의 99.972~100.001%였다. 같은 입력에서 이동 거리는 각각 367.05cm, 391.62cm, 285.40cm로 일치했다. 기본 공격은 모두 21프레임에 한 번 적중했고 50프레임에 회복했다. 비치명 피해 뒤 각 피격 클립의 복귀도 50프레임이었다.

최신 `3607e9e2`를 합친 격리 사본에서 UE 5.8 Editor 빌드 후 저장된 세 프로필을 각각 새 프로세스로 읽었다. 검사 77개가 모두 통과했고 새 Unreal 오류는 0건이었다. 기존 격리 세이브도 각 실행에서 보존됐다. [실행 요약](evidence/ordinary-monster-gait-20261121/saved-run-summary.json)

재현은 `VerifyHumanoidMonsterAI20261110.ps1 -OrdinaryWalk <VerdantSlime|CrimsonSlime|IronShell> -OrdinarySavedProfile -Label <새 라벨>`로 한다. 같은 방법으로 켜기 전 임시 프로필을 비교할 때 `-OrdinarySavedProfile`을 뺀다. 이는 실제 게임 메시의 본과 피해 이벤트를 읽는 프로그램 입력 검사다. 개별 동작의 측면 렌더와 일반 필드 완주는 검수 범위 밖이다.

[설계](../architect/2026-11-21_ordinary-monster-gait.md) · [실제 본과 보행 진폭 비교](evidence/ordinary-monster-gait-20261121/saved-pose-comparison.json) · [Unreal 저장 속성](evidence/ordinary-monster-gait-20261121/asset-profiles.json)
