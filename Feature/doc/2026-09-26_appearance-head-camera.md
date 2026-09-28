# Appearance Mesh 머리 카메라와 Boxing 기본 공격 연결

작성: 2026-09-26
브랜치: `Sprint#4-39-appearance-head-camera`
상태: 구현·애셋 저장·Editor 빌드·소스/시각 검토 완료. 최종 독립 충돌 검증 포함 570프레임 실행 PASS. 원본 스트레이트의 큰 머리 회전 때문에 정면 고정 표적에 빗나가는 동작은 그대로 유지한다.

## 구현 결과

화면에 보이는 몸과 카메라의 기준을 실제 캐릭터 외형인 Appearance Mesh로 통일했다. 이제 별도 First Person Mesh를 Manny 전신으로 바꿔 보여 주는 경로를 사용하지 않는다. Appearance에 적용된 애니메이션의 머리 위치·회전을 실제 카메라 컴포넌트에 반영하고, 이 카메라의 위치·방향으로 공격을 판정한다.

기본 좌클릭은 미리보기 전용이었던 Boxing 원본 기반 실제 몽타주에 연결했다. 왼잽은 원본 동작, 오른잽은 좌우 미러 파생이다. 원본 전체를 원래 속도 그대로 재생하는 것은 아니며, 기존 공격 타이밍에 맞춰 시간만 재매핑했다. 기존 우클릭 짧게 스트레이트와 길게 눌렀다 떼기 훅의 입력·몽타주 연결은 유지했다.

## 메시와 애니메이션 경로

| 요소 | 역할과 변경 결과 |
| --- | --- |
| Appearance Mesh (`Character.GetMesh()`) | 실제 보이는 몸. 기존 외형 asset 및 Retarget AnimBP 유지. 팔·다리·몸통을 표시한다. |
| Animation Source Mesh | 현재 외형 Skeleton이 Manny 애니메이션을 쓰기 위한 숨긴 리타깃 입력. 기존 `ABP_Common_Unarmed`를 유지하고 실제 공격 몽타주를 재생한다. |
| First Person Mesh | 새 경로에서 숨김·애니메이션 일시 정지·컴포넌트 tick 중지. 공격 몽타주 복제 재생과 이펙트 부착에서 제외한다. |
| Appearance Camera Component | 실제 Appearance head를 읽어 눈 위치와 회전을 계산한다. 기존 FistAnimInstance의 시야용 팔 보정이나 별도 head additive를 사용하지 않는다. |

First Person Mesh는 이번 방식의 화면 구현에 필요하지 않다. 기존 Blueprint에 직렬화된 상속 컴포넌트와 API 참조를 한 번에 깨뜨리지 않도록 정의만 호환용으로 남겼다. 목록에 보인다는 이유로 현재 화면에 사용되는 것은 아니다. 반면 Animation Source Mesh는 현재 리타깃 구조의 애니메이션 입력이므로 유지했다.

실행 중 확인 대상으로 사용한 외형은 Human Male의 `SK_Hu_M_FullBody`, Retarget AnimBP는 `ABP_Retarget_Hu_M`, 소스 메시는 `SKM_Manny_Simple`이다. 다른 종족·성별의 개별 화면까지 검증했다고 주장하지 않는다.

## 머리 카메라

- 실제 카메라의 부모를 Appearance Mesh의 `head`로 변경했다. 기본 눈 오프셋은 중립 캐릭터 축 기준 `(8, 0, 4)`cm이며 편집 가능한 값이다.
- 머리 안쪽이 보이지 않도록 owner의 head 본을 숨긴다. 목 아래 전신은 유지한다. 머리카락·얼굴 별도 파츠는 기존 OwnerNoSee 설정을 따른다.
- 숨긴 소켓의 scale 0으로 카메라가 무너지지 않도록, Appearance의 숨김 이전 local bone pose를 부모 체인으로 합성해 머리 변환을 구한다. 카메라 attachment는 head에 두되 absolute transform을 사용한다.
- 머리의 기준 회전 대비 애니메이션 회전과 마우스 ControlRotation을 합성한다. actor yaw를 중복해서 더하지 않는다. 시야용 가짜 팔 자세를 추가하지 않는다.
- 캐릭터 `CalcCamera`와 실제 공격 hit pulse 직전에 동일 카메라를 갱신한다. CameraManager의 이전 FP head additive는 새 모드에서 건너뛰어 중복 흔들림을 막는다.
- 기본 시작 시선 아래 30도는 기존 한 번 적용 정책을 유지한다. 그 후 자유 마우스 시선과 무기 전환 뒤 시선을 보존한다.
- 새 컴포넌트는 별도 Tick을 켜지 않는다. 외형 갱신·무기 전환·카메라 계산 시 갱신하며, 비활성화와 unpossess에는 저장한 상태를 복원한다.

## 기본 공격 애셋

생성 명령은 `Rogue10m.AuthorAppearanceBoxing`, 에디터 실행 스크립트는 `Scripts/Editor/PrepareAppearanceBoxing.py`다. `.uasset` 파일은 Unreal Editor API로 제작·저장했다.

| 입력 | 실제 몽타주 | 변환 |
| --- | --- | --- |
| 좌클릭 1타 | `AM_BoxingLeftJab_Appearance` | 기존 `A_BoxingJab_Manny` 원본 pose를 시간 재매핑 |
| 좌클릭 2타 | `AM_BoxingRightJab_Appearance` | 같은 원본의 좌우 미러 + 시간 재매핑 |
| 우클릭 짧게 | 기존 `AM_Punch_02` | 기존 스트레이트 유지 |
| 우클릭 길게 눌렀다 떼기 | 기존 `AM_Punch_03` | 기존 훅 유지 |

좌우 새 시퀀스는 120fps, 121개 키, 89개 본, 길이 1초이며 DefaultSlot을 사용한다. 기존 스킬 재생 배율 1.5와 타격 지연 0.34초를 유지한다. 타격 지연과 몽타주 속도 모두 공격 속도 배율을 같은 기준으로 적용한다. 별도 뿌리 위치 제거·손 위치 변경·팔 IK 보정은 추가하지 않았다. Root Motion 추출은 꺼져 있어 원본 pose 이동이 곧 충돌 캡슐 이동이 되는 것은 아니다.

에디터 제작 검증에서 왼잽의 root/pelvis/head/양손/양발 위치를 시간 재매핑한 원본과 121개 키에서 비교했고, 최대 위치 오차는 약 **0.0000971cm**였다. 오른잽은 미러 생성 검사를 통과했으며 같은 의미의 원본 위치 오차 측정은 적용하지 않았다. 스킬의 피해·범위·쿨다운·콤보·재생 속도·타격 지연 등 기존 값이 바뀌지 않았는지 검사하고 몽타주 참조만 연결했다.

## 검증 상태

| 검증 | 상태 / 증거 |
| --- | --- |
| Editor 빌드 | PASS, 최종 9.40초. `evidence/appearance-head-20260926/build-geometry.log` |
| 원본 기반 애셋 제작 | PASS, `RESULT=APPEARANCE_BOXING_AUTHORING_PASSED` |
| 애셋 저장 및 스킬 참조 연결 | PASS, `RESULT=APPEARANCE_BOXING_ASSETS_PASSED`, `boxing-assets.json` |
| 독립 소스 리뷰 | 로컬 단일 플레이어 범위 차단 결함 없음. `evidence/appearance-head-20260926/source-review.md` |
| 초기 실행 | 실제 Human Male 외형·Retarget/Source ABP 유지, fresh pitch -30도와 새 좌우 몽타주 경로가 로그에 확인됨. 전체 판정은 스트레이트 미적중으로 FAIL. |
| 촬영 실행 | 570프레임. 모든 공격 명중을 가정한 테스트에서 스트레이트 관련 assertion 2개 실패. `final.log` 보존. 카메라 cache 오차 0cm / 0.00000342도. |
| 최종 독립 충돌 검증 | 570프레임, `RESULT=APPEARANCE_CAMERA_PASSED`, 실패 0. `geometry-verified.log`. 카메라 cache 오차 0cm / 0.00000382도. |
| 영상·프레임 시각 검증 | 1280×720, 30fps, 570프레임, 19초 H.264. 실제 외형 팔·몸통 및 점프 시점 상승 확인. 하체를 숨기지는 않지만 몸통/HUD에 가려 발 전체 가독성은 확인하지 못함. |
| 생성 파일·diff·에셋 hash 검사 | root 확인: CheckGeneratedChanges 및 diff 검사 PASS, 원본 공유 애셋 7개 hash 동일. before/after-hashes.json 참고. |

저장 성공과 생성 pose 오차 검사는 같은 Editor 프로세스에서 실행했다. 별도 게임 프로세스의 몽타주 재생 로그가 디스크에 저장한 연결을 실제로 쓰는지 확인하는 증거다.

### 스트레이트 미적중 원인과 검증 수정

예비 `runtime.log`와 촬영 `final.log`는 스트레이트 미적중으로 두 assertion이 실패했다. 정면 조준(ControlPitch 0도)에서도 실제 타격 시점의 head camera는 Pitch -41.27도, Yaw -65.98도다. 기존 AM_Punch_02가 머리를 크게 돌리므로 시선 방향으로 판정하는 현재 규칙에서 정면 표적에 빗나간다. 카메라/판정 동기화 결함으로 확인된 것은 아니다.

최종 검증은 모든 공격이 무조건 명중한다는 가정을 제거하고 실제 스킬 데이터와 Appearance raw pose로 독립 충돌 질의를 수행했다. 기존 GatherAttackTargets를 호출하지 않았다. frame 193에서 스트레이트 LinearBox 길이180cm, 반폭40cm, 반높이60cm이며 표적은 카메라 좌표 `(28.24,126.58,12.39)`cm다. 실제 표적 collision과 overlap이 없다는 결과와 피해 0을 함께 확인했다. 좌우 잽·훅의 각 1회 명중과 타이밍 검사는 유지했다. 테스트 통과를 위해 판정 범위·표적 위치·애니메이션·게임플레이 코드를 변경하지 않았다.

`geometry-verified.log`의 별도 게임 프로세스에서 570프레임, 실패 0으로 종료했다. 촬영 이후 변경은 이 검증 코드뿐이므로 영상의 런타임 구현은 동일하다. 영상은 원본 움직임의 강한 흔들림과 스트레이트 빗나감을 포함한다. 고개 회전을 완화하거나 조준을 별도로 유지하는 개선은 이번 요청의 원본 모션 보존과 다른 선택이므로 적용하지 않았다.

### 검증 범위와 후속 확인

최종 fixture는 좌우 잽, 우클릭 짧게/차징 후 release, Idle 복귀, 무기 전환, 마우스 시선, 아래보기, 걷기·점프를 대상으로 실행했다. 직접 CalcCamera 호출 전 실제 CameraManager cache와 Appearance 기반 예상 카메라를 570샘플 비교하여 실제 화면 동기화를 확인했다. 위 실행에서 스트레이트 피해 외 검사는 실패를 보고하지 않았다.

사망, 다른 viewtarget으로 전환, 플레이 중 종족/성별 외형 교체, 재빙의, 다른 캐릭터 Skeleton은 소스상 복원·재설정 경로를 검토했으나 개별 실행 검증을 완료하지 않았다. 멀티플레이와 split-screen의 owner별 head hide/조준 동기화도 이번 검증 범위에 포함하지 않는다.

## 최종 실행 수치와 영상

최종 로그 `evidence/appearance-head-20260926/geometry-verified.log`:

| 항목 | 결과 |
| --- | --- |
| 프레임 / cache 샘플 | 570 / 570 |
| 전체 결과 | PASS, 실패 0. 명중 3회와 기하학적으로 확인한 미적중 1회 |
| CameraManager cache 최대 위치 / 회전 오차 | 0cm / 0.00000382도 |
| 왼잽 / 오른잽 / 스트레이트 / 훅 피해 횟수 | 1 / 1 / 0 / 1 |
| 왼잽 / 오른잽 / 훅 최대 hit 시간 오차 | 0.015385 / 0.015384 / 0.027350초 |
| 점프 높이 측정 | 89.98890cm |
| 총 피해 | 97.5 |

[실제 게임 영상](evidence/appearance-head-20260926/appearance-head-gameplay.mp4)

원본 애니메이션을 따르는 과정에서 공격 중 팔이 화면을 크게 가리고 모션 블러가 강하다. 본 테스트 최대 head 회전 변화는 약152.90도, 이동은82.46cm다. 실제 몸통과 팔이 보이며 별도 FP 손이 겹치지 않는다. 아래보기에서는 몸통과 HUD 때문에 발 전체가 명확히 보이지 않는다. 카메라 위치·손 자세를 미관용으로 추가 보정하지 않은 결과다.

## 관련 문서

- 계획: `Feature/architect/2026-09-26_appearance-head-camera.md`
- 소스 검토: `Feature/doc/evidence/appearance-head-20260926/source-review.md`
- 제작 데이터: `Feature/doc/evidence/appearance-head-20260926/boxing-assets.json`
- 일지 및 Sprint 기록: `DevLog/20260926.txt`, `Docs/SprintChangeLog.md`에 최종 결과 기록.

커밋·푸시는 수행하지 않았다.