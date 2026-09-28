# 기본 직업 좌우 연타·차징 어퍼컷·점프 개발 기록

- 작업 브랜치: Sprint#4-25-basic-brawler-inputs
- 날짜: 2026-09-10
- 상태: 구현·빌드·Editor 적용·실게임·회귀·독립 시각 검증 완료. 원본 레퍼런스 영상 재생 및 직접 비교는 미완료.

## 합의한 기본 동작

| 입력 | 동작 |
| --- | --- |
| LMB press | 왼손 잽을 즉시 실행 |
| 유효 연계 안의 두 번째 LMB | 오른손 스트레이트, 빠른 입력은 하나만 예약 |
| 연계 시간 경과 후 LMB | 왼손부터 다시 시작 |
| RMB 누름·유지 | 오른손을 낮추고 몸을 오른쪽으로 돌리며 차징 |
| RMB 짧게 놓기 | 기본 오른손 어퍼컷 1회 |
| 0.85초 완충 뒤 RMB 놓기 | 강화 오른손 어퍼컷 1회, 완충 대기 중 자동 공격 없음 |
| Space | 일반 점프, 지상 공격/차징 취소 후 점프. 공중 LMB는 수락 전 거절 |

몸통·목·머리의 실제 포즈 변화가 최종 표시용 카메라에 전달된다. 타격 기준 CameraComponent와 ControlRotation은 유지한다. 점프는 기존 CharacterMovement 물리를 유지하고 이륙·공중 관성·착지 표현을 추가한다. 기본 Unarmed만 대상으로 하며 Knuckle/StoneFist의 기존 표현과 데이터는 보존한다.

## 구현 구조

- BasicBrawlerComponent가 입력 순서·버퍼·차징 상태를 관리한다.
- 실제 Combat/GAS 경로에서 공격이 승인된 뒤에만 손 종류와 시리얼을 갱신한다.
- FistAnimInstance는 게임 스레드에서 받은 상태를 포즈에 적용하고, 평가된 머리 변화량을 카메라에 전달한다.
- Editor 스크립트는 전용 기본 공격 4개를 만들고 기존 Unarmed 프로필만 연결한다.
- 점프·UI·무기·사망 취소에서 지연된 release가 공격을 실행하지 않도록 정리한다.

## 현재 검증 및 수정

독립 검토에서 취소 후 Combat 연계 상태와 전용 입력 상태가 어긋나던 문제를 발견했다. 연계 표시를 초기화하고 예약된 쿨다운의 종료시각을 보존해, 무기를 왕복해도 쿨다운을 우회하지 못하게 수정했다.

Unreal Header Tool의 기본 IsActive 함수 이름 충돌은 IsBasicBrawlerActive로 이름을 명확하게 바꿔 수정했다. 최종 C++ 빌드는 8.57초에 성공했다. Editor에서 기본 공격 4개와 프로필 1개를 저장하고 바인딩·해금·몽타주·기존 피해/비용/쿨다운 보존을 검증했다.

## 레퍼런스 상태

사용자가 전달한 정확한 링크:
- https://www.youtube.com/shorts/cci0hNBFiZI
- https://www.youtube.com/shorts/M_4S6WFM-wM

두 링크 모두 웹 조회에서 Cache miss가 발생했다. 업데이트된 computer-use를 통한 창 조회도 Windows sandbox ACL 오류로 실행되지 않았다. 영상의 프레임을 시청하거나 동작을 계측했다는 주장은 하지 않는다. 현재 구현은 사용자가 명시한 동작 요구를 기준으로 진행 중이며 영상 검토 항목은 완료하지 않았다.

## 자료

- 기획: Feature/architect/2026-09-10_basic-brawler-inputs.md
- 검증 준비: Source/Rogue10m/Tests/Rogue10mBasicBrawlerRuntimeTest.cpp
- 자산 적용: Scripts/Editor/ConfigureBasicBrawler.py
- 접근 근거: tmp/basic-brawler/reference-access.md

기획 1·개발 2·검증 2 구조를 단계별로 운용한다. 선행 미커밋 작업을 보존하며 커밋·푸시는 하지 않는다.

## 실게임 검증으로 보완한 동작

- 충전 오른손이 화면 아래로 사라지던 문제: 준비 손 offset을 (2, 2, -7)cm로 조정했다.
- 어퍼컷 정점 팔의 삼각형 변형: 정점 (14, 57, 164)cm, 상승 팔꿈치 목표, 측정 팔 길이 94% 도달 제한과 기존 목 숨김을 적용했다. 독립 시각 검토에서 정상 실루엣과 준비 손 노출을 확인했다.
- 모션 전에 피해가 표시되던 문제: HitStartDelaySeconds 기본값 0을 추가하고 새 4개 공격만 주먹 0.34초/어퍼컷 0.633초로 설정했다. 실제 수락 시 공격 속도×AnimationPlayRate로 나눠 재생 시간과 맞춘다. 연계창은 0.40~0.55초이며 디버그 타격 도형을 끈다.
- 지연 피해 취소: 점프·UI·무기 변경·입력 취소에서 아직 첫 타격이 나오지 않은 예약을 제거한다. 실행 직전 입력 상태도 재검사한다. 기존 무기의 즉시/다단 타격은 지연 기본값 0 경로를 유지한다.
- 공중 LMB: 지상 기본 공격 규칙에 맞춰 자원·쿨다운·공격 시리얼을 소비하기 전에 거절한다.

## 검증 fixture와 결과 해석

실제 스폰 Unarmed에서 Combat/GAS 입력 경로로 실행한다. 테스트 대상만 AI를 정지하고 체력 2000, 무작위 피해 계수 1, 재생 0으로 통제한다. 1280×720, 30Hz 시뮬레이션의 480프레임을 검증하고 앞 10초를 15fps/150장으로 촬영한다.

실제 기본 공격 비용은 0이다. 자원 부족 검사는 WITH_EDITOR 테스트에서 Special 비용을 일시적으로 10으로 바꾸고 정상 완료/조기 종료에서 원복하며 저장 API는 호출하지 않는다. 첫 시도에 비용 0을 유료로 오해한 테스트 오류를 바로잡았다. 두 번째 시도에서는 오른손 검사에 이전 왼손 회수의 최댓값을 포함하던 측정 오류를 찾았다. 오른손 정점의 동일 프레임에서 양손을 비교하도록 수정했다. 이 수정으로 생산용 피해·모션 조건을 느슨하게 바꾸지 않았다.

확인된 측정: 일반 어퍼컷 피해 31.8, 강화 36.2, 차징 몸통 오른쪽 yaw 7.54도, 점프 최고 높이 약 90cm, 평가된 머리 변화 최대 5.84도와 표시 카메라 회전 최대 1.27도. 값은 테스트 캐릭터/몬스터의 실제 능력치가 포함된 결과이며 스킬의 원시 피해 설정과 동일한 의미가 아니다. 실제 CameraComponent와 ControlRotation 불변도 별도 검사한다.

독립 코드 검토에서 지연 타이머·취소·기본값 0 회귀와 공중 입력 보정을 확인했다. 최종 검토 범위의 중대/중간 회귀는 발견하지 못했다. 기존 자산 SHA256 비교에서 이번 작업으로 바뀐 기존 파일은 DA_SkillProfile_Unarmed 하나이며 원본 Unarmed 공격/다른 프로필/기본 캐릭터 파일은 보존됐다.

## 최종 검증

- UE 5.8.2 Editor 빌드 성공: 최종 증분 8.57초.
- RESULT=BASIC_BRAWLER_ASSETS_PASSED skills=4
- RESULT=BASIC_BRAWLER_PASSED frames=480 failures=0
- RESULT=FIST_PRESENTATION_PASSED frames=120 failures=0, VIEW_AUDIT completed=3
- 기존 너클의 시선 ±25도와 2560×1080, 표시 카메라 강도 0·무기 전환·사망 복원 확인.
- 독립 소스 검증 1과 시각 검증 2 완료. 생성/캐시 파일 경로 검사 통과.
- GIF 검증: 150프레임, 10,000ms, 960×540. `Feature/doc/images/basic-brawler-20260910/basic-brawler.gif`
- 증거: `Feature/doc/evidence/basic-brawler-20260910/`의 build/assets/runtime/regression 로그와 review.md, existing-asset-hashes.json.

현재 공개 미리보기는 테스트 맵과 기존 Manny 메시에서 촬영했다. 외부 영상에 사용된 리그·애니메이션을 가져오거나 동일하게 재현했다고 주장하지 않는다. 원본 영상 재생 접근이 해결되거나 사용자가 영상 파일을 제공하면 페이크·체중 이동·시선 선행의 구체적인 리듬 비교를 진행할 수 있다. 해당 비교가 남아 있어 전체 목표를 완료 처리하지 않았다.

## 원본 영상 비교 대기
구현 이후 연속 재확인에서도 YouTube 두 URL은 Cache miss이고 브라우저 도구는 windows sandbox failed: apply deny-read ACLs로 시작되지 않았다. 이전 재시도는 추가 진전이 없는 턴으로 분류한다. 원본 영상 파일 제공이나 도구 복구 없이는 장면을 직접 검토할 수 없어 전체 목표를 blocked로 기록한다. 이는 완료 판정이 아니다. 현재 기획·C++·Editor 자산·실게임/회귀/시각 검증 결과는 그대로 유지하며 추가 코드 변경은 하지 않는다.
