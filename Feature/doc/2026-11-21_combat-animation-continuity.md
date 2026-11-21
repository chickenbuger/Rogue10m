# 대검 가드에서 지상 이동으로 전환

3인칭에서 대검을 들고 W를 처음 누르면 서 있는 가드가 한 프레임에 사라졌다. 기존 가드와 보행의 비율을 최대 0.18초 동안 바꿔 흉부와 손이 이어지도록 했다. 원래 대기 `A_GreatSword_Idle`, 이동 `BS_GreatSword_Ground`와 Manny 소스 및 외형 리타깃을 사용한다. 점프·회피·차지·승인된 공격과 그 회복은 기존 가드 해제 조건을 사용한다. 정지 가드의 기존 상승·하강 시간과 이동 공격의 하체 마스크에는 별도 조정을 적용하지 않았다. [작업 계획](../architect/2026-11-21_combat-animation-continuity.md)

## 같은 입력으로 비교

`StartHub`의 실제 프로필을 새로 선택하고 프로그램이 60Hz 고정 스텝에서 같은 EKeys::W 입력을 보냈다. 표의 흉부·손 변화는 보이는 리타깃 메시에서 이동 입력 첫 일곱 표본의 최대치다. 위치는 골반 상대 cm, 각도는 흉부의 인접 컴포넌트 포즈 사이 각도다. 하체 입력은 실제 캐릭터 이동을 사용했다.

| 실행 | 최대 흉부 회전 | 최대 손 이동 | 결과 |
| --- | ---: | ---: | --- |
| A · 즉시 해제 | 27.4547° | 19.4340cm | 첫 이동 표본에서 가드 1→0 |
| B · 기존 0.45초 사용 | 0.9629° | 0.8215cm | 307.2cm/s에서도 가드 74.1% |
| C · 이동 전용 최대 0.18초 | 3.4389° | 2.7181cm | 보행 중 가드 해제와 정지 가드 복귀 통과 |

별도 300cm/s 제한에서는 0.45초 대조군이 속도 도달 후 가드 5% 이상을 15프레임(250ms) 유지했다. 최종 후보는 동일 조건에서 1프레임(16.7ms)이다. 처음 5% 미만은 W 적용 후 열 번째 표본이며 대조군은 스물네 번째였다. 300cm/s 후보의 첫 일곱 표본 흉부는 최대 3.3063°, 손은 2.6657cm였다. 대조군의 길이와 속도는 검증 사본의 상태에서만 바꿨다. [초기 경계 원시 요약](evidence/combat-animation-continuity-20261121/baseline-summary.json)

정지·이동 맨손 잽은 같은 승인 몽타주가 재생됐다. 이동을 시작하면 몽타주 0.078초에서 이동 공격 비율이 0→1이 됐지만, 동일 시각의 오른손 골반 상대 이동은 2.88cm로 정지 잽의 11.53cm보다 작았다. 현 수치에서는 별도 수정 대상으로 확정하지 않았다. 대검 RMB는 실제 장착 기술로 눌림 48프레임과 해제 후 승인 몽타주 72프레임을 관찰했다. 공격 표적과 HP 감소 시점은 이번 검사에 포함하지 않았다.

## 실행과 기록

UE 5.8 Editor 게임플레이 후보 빌드 87.96초. `MotionContinuityC`는 6,497개 검사, 실패 0, Unreal 오류 0을 기록했고 기존 QA 저장 4개를 해시로 확인했다. 일반 3인칭 회귀는 1,568개 검사와 마우스 13건을 통과했고 카메라·회피·벽 충돌과 기존 시선 분리를 재검증했다. 최신 통합 기준 `a24f522e`에서 130.31초 빌드 후 `MotionContinuityLatest`의 같은 6,497개 검사와 QA 저장 10개 해시 확인을 통과했다. 대검 가드와 이동, 비교한 맨손·대검 공격 자산, 대검 스킬 프로필의 Git 블롭이 이전과 동일했다. 처음 빌드는 다른 에디터의 Live Coding 가드에서 멈췄다. 별도 물리 체크아웃의 DLL을 빌드하는 경로를 확인한 뒤 `-NoHotReload -NoHotReloadFromIDE`를 사용했다. 작업 중 사용자 에디터 로그 3,874행 이후 새 오류는 0개다. [검증 수치](evidence/combat-animation-continuity-20261121/verification.json) · [애니메이션 기록](../../DevLog/20261121_CombatAnimationContinuity/AnimationRecord.md)

같은 통합 기준의 `MotionLatestWarp`는 Microsoft Basic Render Driver에서 일반 이동 80프레임을 재생하고 검사 994개를 통과했다. 받침발과 칼끝을 볼 수 있도록 외형을 옆뒤에서 촬영했다. 프로그램이 W를 누르기 전과 가드가 보행에 섞이는 도중에 캡처를 요청했다. 기본 하드웨어 탐색 오류 `887A0004` 이후 Unreal 오류는 0개였다. [이동 전 자세](evidence/combat-animation-continuity-20261121/01_sword_guard_before_W.png) · [가드 전환 중](evidence/combat-animation-continuity-20261121/02_sword_guard_fading.png) · [렌더 결과](evidence/combat-animation-continuity-20261121/results-warp.txt)

수치 재현 명령은 `./Scripts/VerifyThirdPersonGameplay.ps1 -Label FreshSwordContinuity -NullRHI -CombatMotion`이고, 소프트웨어 렌더는 `./Scripts/VerifyThirdPersonGameplay.ps1 -Label FreshSwordRender -CombatMotionRender`이다. 보존된 CSV에는 기준·최종·속도 대조군의 입력 직후 표본과 같은 몽타주 시각의 맨손 공격 표본이 있다. 실제 키 입력 경로를 프로그램으로 구동한 결과이며 물리 키 직접 입력과 일반 필드 완주는 미실시다. [전후 프레임 비교](evidence/combat-animation-continuity-20261121/guard-transition-frames.csv) · [최종 수치 결과](evidence/combat-animation-continuity-20261121/results-latest-nullrhi.txt)
