# Boxing 원본 모션을 권사 기본 공격에 연결

- 날짜: 2026-09-23
- 단계: Architect / 구현 전 설계
- 사용자 요청: 비교 영상에서 본 `Boxing.fbx` 모션을 실제 기본 공격에 연결한다.
- 역할: 기획 1, 개발 2, 검증 2. 동시 실행 제한에서는 역할별 작업을 순차 재사용한다.

## 범위와 입력 계약

기본 무기 `Unarmed`의 좌클릭 1타는 기존 리타깃 `A_Boxing_Manny`의 왼쪽 잽을 평가하고, 좌클릭 2타는 같은 모션의 좌우 미러를 사용한다. 원본은 1.73333초, 53샘플이며 최대 전진 프레임은 13이다. 원본에 없는 오른쪽 스트레이트와 훅은 기존 우클릭 짧게/길게 눌렀다 떼기 동작을 유지한다. 원본 전체를 고정 속도로 재생하는 미리보기와 달리 게임에서는 현재 공격 속도와 타격 시점에 맞춰 구간 속도를 조정한다.

평상시 손을 내린 Idle, 게임 시작 시 아래 30도 시선, 자유 마우스 조작, 전신 표시와 머리 추종, HUD를 유지한다. 캡슐 이동, 공격 사거리, 자원 비용, 피해량, 입력 버퍼, 콤보 창은 변경하지 않는다. 원본 FBX 재수입은 하지 않는다. 검증된 Preview 리타깃을 Unreal Editor API로 실제 전투용 경로에 복제하고 해당 폴더를 쿠킹 대상으로 명시한다.

## 확인한 현재 구조

- `URogue10mBasicBrawlerComponent::NotifySkillAccepted`가 실제 승인 이후 `AcceptedSerial`, 공격 종류, 경과 시간, 공격 길이, `HitFraction`을 제공한다.
- 공격 길이는 몽타주 길이 / (공격 속도 × AnimationPlayRate), 타격 비율은 실제 `HitStartDelaySeconds / Rate / AttackDuration`이다.
- `FRogue10mFistAnimInstanceProxy::Evaluate`의 기본 권사 경로는 Idle 샘플 → `EvaluateBasicPose` → 이동 포즈 → `CaptureHeadDelta` → 마우스 조준이다.
- 기존 기본 공격은 손 IK, 몸/머리 보조 회전, 발 지지 이동을 직접 생성한다. 원본 모션을 샘플하더라도 이 포즈로 다시 덮으면 요청을 충족하지 못한다.
- 검증된 원본 소스는 `/Game/Rogue10m/Animation/Preview/Boxing/A_Boxing_Manny.A_Boxing_Manny`이다. 실제 런타임은 Editor API로 복제한 `/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingJab_Manny.A_BoxingJab_Manny`를 참조한다. `Scripts/Editor/PrepareBoxingBasicAttack.py`에서 53프레임 × 7본(371샘플)의 위치·회전 일치를 검증하고 `Config/DefaultGame.ini`에 전투용 폴더의 AlwaysCook을 명시한다.

## 구현 설계

1. FistAnimInstance에 원본 소프트 참조, 활성화 및 블렌딩 튜닝 값을 노출한다. 로드된 UObject는 UPROPERTY/TObjectPtr로 유지하고, 애니메이션 작업 스레드에서는 게임 스레드에서 준비된 읽기 전용 포인터·스냅샷만 사용한다. 로드 실패나 호환 불가 시 기존 기본 공격으로 안전하게 복귀한다.
2. 기본 권사와 전신 표현이 활성화되고 승인된 좌/우 잽인 경우에만 원본을 평가한다. 입력 이벤트 자체가 아닌 `AcceptedSerial`을 시작 기준으로 삼아 거절된 공격은 재생하지 않는다.
3. `AttackElapsed / AttackDuration`을 소스 시간에 매핑한다. 게임의 HitFraction에서 원본의 13/30초가 일치하도록 전반/후반 구간을 각각 매핑한다. 공격 속도가 바뀌어도 피해 타이머를 새로 만들지 않는다.
4. Idle·기존 전투 자세와 원본 사이의 시작/종료 블렌드를 적용한다. 이전 공격이 끝나기 전에 다음 공격이 승인되는 경우에도 손과 몸이 프레임 사이에 갑자기 바뀌지 않도록 경계를 확인한다. 원본 타격 정점에서는 충분한 원본 가중치를 확보한다.
5. 오른잽은 본 이름만 교환하지 않는다. 로컬 UE 5.8 `AnimationRuntime.h`의 `FAnimationRuntime::MirrorPose` 오버로드와 본 매핑, component-space reference 회전을 이용한다. 현재 Manny 포즈의 좌우 축은 X이다. 본 목록과 LOD가 바뀌면 캐시를 무효화한다.
6. 원본 전신 움직임을 포즈로 유지하되 캡슐 root-motion 추출은 수행하지 않는다. 이동 시 보행 보조 포즈와 발 미끄러짐, 중간 공중 전환을 확인한다. 애니메이션을 다시 procedural 팔 IK로 덮지 않는다.
7. 원본을 포함한 최종 포즈에서 `CaptureHeadDelta`를 실행한 뒤 기존 마우스 조준 보정을 적용한다. 이미 숨긴 head/neck 렌더 포즈가 아닌 내부 평가 포즈를 사용한다.

## Ultrawork Packets

| 패킷 | 목표 / 수정 영역 | 완료 조건 | 검증 명령 및 관찰 | 되돌리기 경계 |
| --- | --- | --- | --- | --- |
| P1 원본 기본 공격 | `Rogue10mFistAnimInstance.h/.cpp`의 자산 참조·샘플·미러·블렌딩 | 실제 승인된 왼잽/오른잽에만 원본이 연결되고 기존 우클릭 유지 | `Scripts/BuildEditor.ps1`, 원본 연결 전용 런타임 검사 및 좌우 프레임 관찰 | 해당 소스 변경 또는 노출 활성화 옵션 |
| P2 실게임 증거 | `Source/Rogue10m/Tests`와 필요 촬영 도구 | 실제 입력 경로에서 잽 2회·짧은 우클릭·차징 훅·Idle 복귀를 촬영 | 전용 검사, 기존 `Rogue10m.TestBasicBrawler` 등 관련 회귀 검사, 타격 시각/시간과 소스 활성 상태 로그 | 추가 검사/촬영 파일만 제거 |
| P3 통합 검증과 기록 | Feature/doc, evidence, DevLog, SprintChangeLog | 빌드·독립 코드/영상 검증·원본 자산 해시 기록 완료 | `Scripts/CheckGeneratedChanges.ps1`, scoped `git diff --check` | 문서·영상 산출물만 제거 |

## 검증 우선순위

- 승인된 왼잽은 원본, 오른잽은 미러이며 원본 데이터가 실제 최종 포즈에 기여하는지 확인한다. 단순 플래그만으로 성공 판정하지 않는다.
- 타격 시점의 소스 시간 오차는 측정 프레임 범위 이내여야 한다. 실제 피해 횟수·자원·콤보 순서·쿨다운이 중복되거나 지연되지 않아야 한다.
- 양팔 뼈 길이와 quaternion이 유효하고, 미러 시 손목·팔꿈치가 뒤집히지 않아야 한다.
- Idle→왼잽, 왼잽→오른잽, 잽→우클릭, 취소/무기 전환→Idle 경계의 손 위치와 머리 카메라 연속성을 확인한다.
- 원본의 큰 전신 이동이 기존 머리 추종 최대치에 잘려 발생하는 어긋남, 팔이 화면 밖으로 사라지는 구간, 발 미끄러짐을 실제 기본 30도 시점에서 점검한다.
- 우클릭 스트레이트/훅, 장착 너클과 다른 무기, 사망/입력 제한, 재입장에 원본이 남지 않아야 한다.
- 원본 FBX와 Preview 리타깃 uasset은 변경하지 않는다. 전투용 복제 에셋은 Editor API로만 생성하며 실제 런타임 로드와 AlwaysCook 포함 조건을 확인한다.

## Exit Gate

빌드와 관련 실게임 검증, 독립 소스 검토 및 실제 영상 확인 후 결과 문서에서 각 항목을 통과/제한으로 구분한다. 기본 공격에 연결한 소스 모션과 기존 우클릭 모션을 사용자에게 분명하게 설명한다. DevLog는 한국어로 추가하고 Notion 요약 후보를 남긴다. 커밋과 푸시는 사용자 확인 없이는 수행하지 않는다.
