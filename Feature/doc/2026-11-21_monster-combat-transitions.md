# 필드 몬스터 공격·피격 자세 연결

초록·붉은 점액체, 스포어링, 철갑 등껍질과 가시수호자가 공격·피격 때 보행 그래프에서 개별 시퀀스로 바뀌며 첫 자세가 크게 튀었다. 다섯 종에서 기존 대기·보행 그래프의 슬롯에 현재 공격과 피격 시퀀스를 연결했다. 진입과 복귀는 각각 0.10초 동안 섞는다. 공격 시작부터 기존 전체 구간의 비율로 원본 동작을 진행하고, 피격 동작도 원래 길이의 타이머를 사용한다.

## 게임 안 60Hz 비교

현재 배치된 여섯 종의 실제 메시와 프로필을 StartHub의 격리 전투 대상에 적용했다. 대기와 보행 각각에서 실제 기본 공격을 실행해 한 번의 피해와 회복을 관찰한 다음, 별도 비치명 피해로 기존 피격 클립과 복귀를 확인했다. 표는 보행 뒤 첫 공격 프레임에서 표시 메시의 본별 위치와 회전 변화 중 가장 큰 값이다.

| 몬스터 | 첫 기본 공격 본 이동 | 첫 기본 공격 본 회전 | 기본 피해 / 복귀 프레임 |
| --- | ---: | ---: | ---: |
| 초록 점액체 | 25.39 → 4.04cm | 35.18 → 1.16° | 21 / 50 |
| 스포어링 | 16.56 → 1.37cm | 20.31 → 2.24° | 21 / 50 |
| 붉은 점액체 | 25.39 → 4.04cm | 35.18 → 1.16° | 21 / 50 |
| 철갑 등껍질 | 28.24 → 4.42cm | 35.58 → 4.19° | 21 / 50 |
| 가시수호자 | 60.37 → 8.85cm | 79.24 → 9.26° | 17 / 41 |
| 아자토르 | 24.90cm | 38.39° | 60 / 143 |

가시수호자의 첫 피격 프레임 최대 본 이동은 44.46→3.32cm였다. 피격 종료부터 여섯 프레임의 최대 이동은 32.85→8.06cm였다. 철갑 등껍질의 같은 피격 복귀 구간은 회전 23.23→5.28°, 위치 0.29→1.44cm였다. 아자토르의 네이티브 몽타주를 대조군으로 포함했다.

열두 사례 모두 기존 공격 클립과 첫 피해 및 후딜 프레임, 피격 클립 이탈 프레임을 대조했다. 짝지은 공격·피격 클립 재생 시각의 최대 차이는 0.000050초였다. 위치는 표시 메시 컴포넌트 공간의 cm, 회전은 같은 본 전후 quaternion의 각도 차이다. 격리 시험은 프로그램 입력 및 실제 피해 호출로 진행했고 필드 순회와 물리 입력은 별도 범위다.

기존 동작의 결과와 수정 후 결과는 같은 측정 루틴으로 수집했다. `VerifyHumanoidMonsterAI20261110.ps1 -MonsterLivingMotion -Label <새 라벨>`에서 12개 사례를 다시 실행할 수 있다. [설계](../architect/2026-11-21_monster-combat-transitions.md) · [전후 수치](evidence/monster-combat-transitions-20261121/pose-comparison.json)

## 가시수호자 720p 확인

시점은 같은 위치를 따라가는 측면 카메라다. 준비 자세를 읽은 뒤 검수용 표적을 공격 범위 안에 두었다. 다음 프레임에서 공격 명령을 적용하고 첫 자세, 실제 피해, 공격 복귀, 별도 피격을 촬영했다. 보행의 몸통과 팔 방향에서 첫 공격 자세가 이어지는 것을 확인했다. 현재 화면은 적용본이며 이전 프레임 변화는 같은 60Hz 수치 자료에 있다. [준비](evidence/monster-combat-transitions-20261121/attack-ready.png) · [첫 공격](../../DevLog/20261121_MonsterLivingMotion/attack-first-720p.png) · [첫 피해](evidence/monster-combat-transitions-20261121/attack-damage.png) · [공격 복귀](evidence/monster-combat-transitions-20261121/attack-recovered.png) · [피격 첫 프레임](evidence/monster-combat-transitions-20261121/nonlethal-first-frame.png)

첫 WARP 실행은 매 프레임을 그려 다섯 화면을 캡처한 뒤 피격 복귀 전에 100초 감시 제한에 걸렸다. 두 번째 `-MonsterLivingRender` 실행은 다섯 캡처 시점만 화면으로 그리면서 모든 움직임과 게임 판정을 매 프레임 계산해 복귀까지 완료했다. 하드웨어 장치 탐색 오류 한 건을 기록했으며 Microsoft Basic Render Driver 선택 이후 새 오류는 없었다. [최종 렌더 결과](evidence/monster-combat-transitions-20261121/warp-results.txt)

동일 빌드의 기본 실행 파일에서는 게임 검사 99개가 통과한 뒤 로그 검사에서 Concert 시작 오류 두 건으로 보류했다. 엔진 `FConcertSessionChallengeData`의 생성 시각 초기화 검사가 `StartHub`와 검수 캐릭터를 로드하기 전에 발생했다. 별도 저장 디렉터리의 `UnrealEditor-Cmd -game -NullRHI` 재실행에선 99개와 신규 로그 오류 0건을 확인했다. 기존 가시수호자 회귀에서 싱글 노드 형식만 확인하던 조건은 현재 재생 중인 기존 공격 클립과 기존 준비 시간을 확인하도록 조정했다. [NullRHI 기록](evidence/monster-combat-transitions-20261121/nullrhi-results.txt) · [Concert 시작 오류 기록](evidence/monster-combat-transitions-20261121/rejected-concert-startup.txt)
