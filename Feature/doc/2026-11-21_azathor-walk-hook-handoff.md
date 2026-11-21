# 아자토르 보행에서 기본 훅으로 전환

아자토르가 걸으면서 기본 훅을 시작할 때 뒤에 있던 발이 먼저 크게 튀었다. 스킬이 수락되면 기존처럼 그 자리에서 이동을 멈춘다. 표시 자세는 마지막 보행의 발 위치에서 0.22초 동안 부드럽게 풀리고, 같은 시간 동안 기존 훅 몽타주가 들어온다. 기본 훅의 몽타주와 원본 클립은 제자리에서 처음부터 재생된다. 기존 보스 프로필·클립·맵과 타격·후딜·재사용·경고 로직을 사용했다. [작업 계획](../architect/2026-11-21_azathor-walk-hook-handoff.md)

## 실제 60Hz 비교

StartHub 검사에서 현재 필드와 동일한 아자토르 DA, 메시, 공격 프로필을 불러왔다. 고정된 평지에서 실제 이동을 240cm/s까지 올린 뒤 동일한 기본 공격 수락 경로를 실행했다. 원본 `AM_HumanBoss_Attack`의 `A_HumanBoss_Hook_Raw`를 수락 이후 0.008333초부터 관측했다.

| 보행 이후 표시 자세 | 기준 `9cd829f3` | 몽타주만 0.22초 | 보행과 몽타주를 함께 보간 |
| --- | ---: | ---: | ---: |
| 첫 표시 본 이동 최대 | 24.90cm | 16.05cm | 3.45cm |
| 첫 표시 본 회전 최대 | 38.39° | 24.57° | 11.53° |
| 처음 여섯 프레임의 최대 이동 | 33.02cm | 16.36cm | 16.19cm |
| 처음 여섯 프레임의 최대 회전 | 49.65° | 24.65° | 23.95° |

수정안의 처음 여섯 프레임에서 위치 변화가 가장 큰 본은 왼쪽 발볼이었다. 왼쪽 발목 본의 월드 높이는 33.61, 29.75, 24.94, 20.62, 17.75, 16.60cm로 차례로 낮아졌다. 공격 동안 캐릭터 기준점은 같은 위치였다. [표시 본 수치](evidence/azathor-walk-hook-handoff-20261121/pose-comparison.json) · [몽타주 가중치와 발목 좌표](evidence/azathor-walk-hook-handoff-20261121/final-detail.csv)

기존·수정·취소 뒤 재실행의 첫 피해는 모두 60프레임, 몽타주 0.500000초, 회복은 143프레임이었다. 처음 3프레임에서 공격을 취소한 사례는 피해 0회였다. 정지 시작의 몽타주 시간 차이는 0초였고, 표본의 최대 차이는 0.00011°였다. 보행 후 20프레임을 넘긴 뒤에는 후드와 옷 본을 포함한 이웃 프레임 최대값이 0.3661cm와 1.7113° 범위에서 달랐다. 같은 훅의 첫 피해·회복과 전체 몽타주 샘플 시각은 일치했다.

## 720p 화면

`AzathorFinalWarp3b`는 소프트웨어 렌더러에서 여섯 장을 촬영했다. 첫 프레임에 걷던 다리 모양을 유지하고, 8프레임까지 발을 내리며 상체를 훅 준비 자세로 돌린다. 8프레임과 이후 화면에서 앞 부츠 끝은 아래쪽에서 잘리므로 발 위치는 본 좌표로 확인했다. [보행 준비](evidence/azathor-walk-hook-handoff-20261121/attack-ready.png) · [공격 첫 프레임](../../DevLog/20261121_AzathorHook/first-step-720p.png) · [공격 8프레임](evidence/azathor-walk-hook-handoff-20261121/attack-early-motion.png) · [첫 피해](evidence/azathor-walk-hook-handoff-20261121/attack-damage.png) · [회복](evidence/azathor-walk-hook-handoff-20261121/attack-recovered.png)

## 재현

`./Scripts/VerifyHumanoidMonsterAI20261110.ps1 -Label FreshAzathorHook -AzathorHook`는 새 저장 접두사로 정지, 보행, 취소 뒤 보행 재실행을 확인한다. 별도 검사 이름에 `-MonsterLivingRender`를 추가하면 720p에서 지정된 자세를 촬영한다. 기본 공격, 정면 파동, 심연 영역 등 기존 보스 검사 18건과 배치 몬스터 여섯 종의 공격·피격 12건도 통과했다. 보스 검사의 첫 실행에서는 고정 프레임 옵션 때문에 기존 사망 확인 콜백에 오류가 났다. 같은 빌드를 기존 실시간 조건으로 실행해 검증을 마쳤다. 전체 필드 플레이와 물리 입력, 경사면은 이 검사의 범위 밖이다. [검증 기록](evidence/azathor-walk-hook-handoff-20261121/validation.txt) · [애니메이션 기록](../../DevLog/20261121_AzathorHook/AnimationRecord.md)
