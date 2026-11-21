# 필드 몬스터 사망 자세 전환

초록·붉은 점액체, 스포어링, 철갑 등껍질과 가시수호자가 치명 피해를 받으면 마지막 표시 자세에서 기존 사망 클립으로 0.15초 동안 이어진다. 몬스터가 죽기 전 표시하던 자세를 공격 취소 전에 읽고, Unreal이 새 그래프를 처음 평가하기 전에 전달한다. 기존 클립은 치명 피해 프레임부터 원래 속도로 진행한다.

## 60Hz 치명 피해 검증

OpenRunMap에 배치된 여섯 클래스와 실제 데이터·메시를 사용하는 격리 개체에 대기, 보행, 기본 공격 준비 중 치명 피해를 적용했다. StartHub의 타깃 검수 개체로 전체 18개 사례를 비교했다. 표는 각 몬스터 세 상태에서 가장 큰 첫 평가의 본 이동과 회전이다. 위치는 표시 메시의 컴포넌트 공간으로 측정했다.

| 몬스터 | 첫 프레임 최대 이동 | 첫 프레임 최대 회전 | 사망 후 제거 |
| --- | ---: | ---: | ---: |
| 초록 점액체 | 62.51 → 2.12cm | 27.1 → 0.9° | 83프레임 |
| 스포어링 | 37.35 → 1.27cm | 35.6 → 1.2° | 53프레임 |
| 붉은 점액체 | 62.51 → 2.12cm | 27.1 → 0.9° | 83프레임 |
| 철갑 등껍질 | 47.85 → 1.64cm | 20.0 → 0.7° | 123프레임 |
| 가시수호자 | 77.77 → 2.44cm | 78.9 → 2.6° | 47프레임 |
| 아자토르 | 12.73cm | 27.3° | 69프레임 |

다섯 몬스터 모두 첫 일곱 프레임의 최고 본 이동·회전이 줄었다. 가시수호자의 보행에서 이어진 사망은 이 구간의 최대 본 이동이 77.77→16.55cm였다. 대기·보행·공격 준비 사례 모두 피해를 받는 호출 안에서 표시 자세가 이어졌고, 블렌드가 끝난 뒤 원래 사망 클립의 같은 시각에서 관측한 본 위치 차이는 0cm였다. 아자토르의 기존 사망 몽타주는 세 상태 모두 같은 자세와 재생 시각이 관측됐다.

치명 피해는 공격을 취소하고 이동과 충돌을 끈다. 공격 준비 중 사망한 개체가 뒤늦게 피해를 주지 않는지, 기존 사망 시퀀스 시간이 정상 진행하는지, 제거 프레임이 같은지 확인했다. 격리 대상은 처치 경험치와 골드를 지급하지 않는 기존 검수 개체다. 실제 필드 정산, 물리 입력과 전체 런은 이 검수 범위 밖이다.

## 실제 게임 화면

가시수호자가 걷는 동일한 위치와 측면 카메라에서 치명 피해 전, 첫 사망 프레임, 그리고 블렌드가 끝난 뒤를 720p로 촬영했다. 기존 첫 프레임에는 팔과 몸통이 갑자기 바뀌며 모션 블러가 길게 생겼다. 적용본에서는 걷던 윤곽에서 무너지기 시작했고, +0.167초에는 두 실행에서 같은 원래 사망 동작을 볼 수 있었다. WARP의 장치 탐색에서 기존 하드웨어 오류가 한 번 있었고 Microsoft Basic Render Driver 선택 이후 새로운 오류는 없었다. [기존 첫 프레임](evidence/monster-death-motion-20261121/thorn-stock-first-720p.png) · [적용 첫 프레임](../../DevLog/20261121_MonsterDeathMotion/thorn-first-720p.png) · [블렌드 종료 뒤](evidence/monster-death-motion-20261121/thorn-follow-through-720p.png)

`VerifyHumanoidMonsterAI20261110.ps1 -MonsterDeathMotion -Label <새 라벨>`로 수치 검사를, `-MonsterDeathRender`로 가시수호자 측면 비교를 실행한다. 렌더 검수는 같은 한 프로세스에서 기존 사망 경로 대조와 적용 경로를 순서대로 사용한다. [수치 비교](evidence/monster-death-motion-20261121/pose-comparison.json) · [렌더 결과](evidence/monster-death-motion-20261121/render-results.txt)

격리된 `678e22c5` 기준 사본에서 UE 5.8 Editor 빌드 후 수치 검사 189개와 렌더 검사 32개를 통과했다. 최종 수치 실행의 새 Unreal 오류는 0건이었다. [수치 실행 요약](evidence/monster-death-motion-20261121/final-run-summary.json)

## 사용 에셋

모델과 사망 원본은 Dungeon Mason의 Monster For Survival Game에 있는 Polyart/PBR `Slime_Die_ANIM`, `Mushroom_DieSmile_ANIM`, `TurtleShell_Die_ANIM`, `Cactus_Die_ANIM`이다. 아자토르는 인간형 Garret의 프로젝트 `A_HumanBoss_Death_Right`와 `AM_HumanBoss_Death_Right`를 쓴다. 현재 적용은 런타임 포즈 연결이다.

첫 후보는 새 애님 인스턴스를 만든 뒤에 이전 자세를 전달해, 클래스 초기화 중 대기 포즈가 잠깐 나왔다. 현재는 초기화 전에 포즈를 준비하며 치명 피해 호출 전후의 본 차이를 검사한다. [설계](../architect/2026-11-21_monster-death-motion.md) · [초기 전환 실패 수치](evidence/monster-death-motion-20261121/rejected-synchronous-initialization.json)
