# 스포어링 대기·보행 전환

어린 포자 버섯(스포어링)이 움직이기 시작하거나 멈출 때, 현재 속도에 맞춰 기존 대기와 걷기 자세가 이어지도록 조정했다. Unreal Editor에서 프로젝트의 `DA_OW_BasicAttack_Sporeling`에 보행 블렌딩 옵션을 켰다. Monster For Survival Game의 기존 버섯 메시와 애니메이션을 사용했다. [작업 계획](../architect/2026-11-21_sporeling-walk-transition.md)

StartHub에서 OpenRunMap과 동일한 스포어링 데이터·메시·프로필을 사용하는 개체를 소환했다. 60Hz에서 같은 이동 입력을 넣고 메시 컴포넌트 좌표의 인접 프레임을 비교했다.

| 측정 | 기존 전환 | 기존 블렌딩 적용 |
| --- | ---: | ---: |
| 이동 시작 구간 치마 본 최대 변화 | 27.9136cm | 3.9633cm |
| 정지 구간 치마 본 최대 변화 | 29.0540cm | 4.0187cm |
| 이동 시작 이후 머리 본 최대 변화 | 22.3336cm | 4.3463cm |
| 최대 보행 속도 | 230.0000cm/s | 230.0000cm/s |
| 같은 입력에서 이동한 거리 | 138.1507cm | 138.1507cm |

두 구성은 기존 기본 공격 클립을 재생했다. 첫 피해는 시작 이후 21번째 게임 프레임(0.350초), 회복 완료는 50번째(0.833초)였고, 각각 한 번 명중해 1 HP가 줄었다. 회복 후에는 해당 설정의 대기 자세로 돌아왔다. 최초 저장본 실행에서 한 검사가 준비 구간의 체력 재생까지 합해 피해를 0.8984로 기록했다. 캐릭터 로그의 실제 피해는 양쪽 1.0이었다. 검사를 체력이 감소한 이벤트에서 실제 감소량을 합산하도록 바꿨다. [최초 재검증 기록과 원인](evidence/sporeling-walk-transition-20261121/verification.json)

UE 5.8 Editor 빌드와 임시 설정 및 저장본의 60Hz 검사 각 17개를 통과했다. 저장본은 새 프로세스에서 읽었으며 같은 자세와 피해를 확인했다. 기존 가시수호자 검사 10개의 결과는 이전 기록과 바이트 단위로 일치했다. 최종 수치 실행들의 Unreal 오류는 0개였다. 렌더 확인, 사람의 직접 조작, 일반 필드 완주는 미실시다. [임시 비교](evidence/sporeling-walk-transition-20261121/transient-results.txt) · [저장본 검증](evidence/sporeling-walk-transition-20261121/saved-results.txt) · [본 표본](evidence/sporeling-walk-transition-20261121/saved-pose-frames.csv)

저장본 재현은 `./Scripts/VerifyHumanoidMonsterAI20261110.ps1 -Label FreshSporeling -SporelingWalk -SporelingSavedProfile`로 실행한다.
