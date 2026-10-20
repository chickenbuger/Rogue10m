# 격투가 팔 휘두르기 강화 결과

브랜치: `Sprint#5-8-fighter-arm-swing`. 기본 맨손 4종, 너클 5종, 금강쇄의 기존 파생 시퀀스 10개를 Unreal Editor API로 저장했다.

## 적용

- 잽·스트레이트·붕권·축경권의 후방 준비와 회수 폭을 늘렸다. 훅 추가 원호는 최대 7cm, 연타는 최대 4cm, 어퍼컷 준비 낙폭은 최대 8cm다. 실제 추가량은 타격 보호 구간에서 부드럽게 0이 된다.
- 기존 타격 순간을 둘러싼 플랫폼 샘플 키와 여유 구간을 보호해 준비·회수만 강화했다. 연타는 모든 타격의 보호 구간을 양팔에 적용한다.
- 금강쇄는 같은 두 권 시간 매핑으로 다시 굽고 실제 FK 손·발에서 IK 5개를 재계산했다. 기존 파생본의 최대 IK 오차 약 0.097cm도 함께 개선했다.
- 머리 카메라, 몸통 회전, 입력, 몽타주, 재생 배율, 피해 설정, 콤보·타수·쿨다운과 14노드 프로필은 보존했다.

## 측정과 검증

다음 수치는 120Hz **Raw 클립**의 활성 손 전체 이동거리/최고 속도다. 실제 게임의 속도 증가율로 바꿔 말하지 않는다.

| 동작 | 원시 손 경로 증가 | 원시 최고 속도 증가 |
|---|---:|---:|
| Unarmed_LeftJab | 15.04% | 35.53% |
| Unarmed_RightJab | 5.51% | 22.02% |
| Unarmed_Straight | 6.55% | 10.15% |
| Unarmed_Hook | 16.45% | 29.61% |
| Knuckle_Jab | 5.41% | 22.18% |
| Knuckle_BodyStraight | 5.53% | 26.60% |
| Knuckle_HookFlurry | 9.77% / 4.96% | 22.22% / 21.98% |
| Knuckle_Uppercut | 24.30% | 56.05% |
| Knuckle_ShortBlow | 21.45% | 46.08% |
| IronChain_TwoPunch | 10.31% / 4.76% | 18.39% / 29.02% |

- Editor 빌드 성공. 적용 전 10개 기준선, 저장 전 검사, 새 프로세스 재로드 및 기존 contact/activation-boundary 검사 PASS.
- 실제 1인칭 게임 전후 동일한 841프레임·118,581포즈를 비교했다. Appearance 공격 손 경로는 1.03~9.62% 늘었고 같은 시각의 손 위치 차이는 3.68~9.53cm다. 전체 몽타주 최대 속도에는 Idle→가드 전환이 포함되어 원시 클립 최고 속도와 구분한다.
- 실제 9개 공격·14회 피해·첫 판정 오차 및 타수 검사 PASS. V 전환 검사 1인칭 580/3인칭 260샘플 PASS. 금강쇄 포함 StoneFist 3개 스킬의 실제 피해·재생 검사 1,234항목 PASS.
- 카메라 최대 회전은 기본 회귀에서 전후 모두 7.002087도, StoneFist 7.002835도로 기존 8도 이내다. 새 손발 길이·scale 비정상 수치가 없었다. 금강쇄 Raw IK 최대 오차는 약 0.000030cm다.
- 전체 Content 4,215패키지 해시 비교에서 허용된 10개 sequence만 변경, 나머지 4,205패키지는 같은 바이트다. 몽타주와 Data Asset도 보존됐다.

원본 120Hz 키 사이의 보간과 플랫폼 샘플 키를 구분해서 평가했다. 시퀀스 검사 기본은 Raw이며 실제 게임 Montage→Appearance 포즈·피해·카메라는 별도 실행으로 확인했다. 피해 총합의 전후 차이는 동일 피해 범위/치명 확률 안에서의 난수 롤이므로 같은 총합이라고 주장하지 않는다.

첫 강화안은 금강쇄 접합 회전 기준을 넘어서 저장 전 중단했고, 다음 안은 실제 평가한 어퍼컷 접촉점 차이로 중단했다. 기존 기준을 넓히지 않고 연타 확대량과 접촉 구간을 재설계했다. 마지막 남은 금강쇄 IK 오차는 실제 FK에서 재동기화했다. 실패 로그를 보존했다.

태산압정·유운수·무극패왕권과 기본 Idle은 이번 팔 확대 대상에서 제외했다. 커밋/푸시는 하지 않았다.

## 재현

`Scripts/VerifyFighterArmSwing.ps1`의 baseline/apply/reload/combat/stone/render 단계를 사용한다. 이미 적용한 상태에서는 기본 reload 또는 새 label의 runtime 검사를 사용한다. 기존 evidence를 덮어쓰지 않는다. 실행 영상의 모션 블러와 HUD는 세밀한 손가락/발의 육안 확인을 제한한다.

## 관련 파일

- `Source/Rogue10m/Editor/Rogue10mFistCombatAuthoring.cpp`: 명시적으로 실행하는 AuthorFistSwing 제작기
- `Scripts/Editor/EnhanceFighterArmSwing.py`: 기준선·안전 저장·재로드 검사
- `Scripts/VerifyFighterArmSwing.ps1`: 제한 시간/격리 저장 슬롯/단계별 실행
- `Feature/architect/2026-10-05_fighter-arm-swing.md`: 계획과 작업 패킷
- `Feature/doc/evidence/fighter-arm-swing-20261005/`: 원시/실제 게임 검증, checkpoint, SHA 비교, 비교 영상

## 실제 화면과 영상

최종 1280×720 렌더 840프레임과 실제 게임 검사 PASS. 30Hz 고정 시뮬레이션의 PNG 프레임을 H.264로 묶은 무음 28초 영상이며 캡처에만 Ray Tracing/Path Tracing을 비활성화했다. 프로젝트 렌더 설정 파일은 변경하지 않았다.

- [전체 1인칭 실행 영상](evidence/fighter-arm-swing-20261005/fighter-arm-swing-first-person.mp4)
- [변경 전후 비교: 왼쪽 BEFORE, 오른쪽 AFTER](evidence/fighter-arm-swing-20261005/before-after-first-person.mp4)

비교 영상은 최초 렌더 제한 전에 확보한 프레임0~217의7.27초 구간이다. 최초 렌더 전체 회귀가 완료됐다는 뜻은 아니며, 변경 전 전체 회귀는 별도 NullRHI 실제 게임 검사에서 완료했다. 최종 변경 후 렌더는 전체 완료했다. 독립 검토는 선택 프레임의 정면 시야/손목 반전·팔 늘어남 부재와 120Hz 원시·실제 게임의 수치 검증을 결합한다. 자연스러움 전체를 수학적으로 보장하는 점수로 해석하지 않는다.
