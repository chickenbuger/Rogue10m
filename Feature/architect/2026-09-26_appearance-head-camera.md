# Appearance Mesh 머리 카메라와 실제 Boxing 기본 공격

작성: 2026-09-26 / 역할: Architect / 구현 전 기획

## 요구와 범위

사용자는 별도 First Person Mesh로 가공한 팔이 아니라, 캐릭터 외형인 Appearance Mesh의 실제 동작을 머리에 설치한 카메라로 보기를 원한다. 기존 요청인 Boxing.fbx 기본 공격 연결도 함께 완료한다. 기본 시작 시선 아래 30도, 이후 자유 마우스 시선, 전신과 머리 움직임은 유지한다.

이번 변경은 C++ 표현·카메라·공격 재생 경로와 실제 몽타주/스킬 연결을 포함한다. 생성 코드나 바이너리를 직접 편집하지 않는다. 기존 작업 브랜치 및 변경 사항은 root가 관리한다. 커밋·푸시는 하지 않는다.

## 확인한 현 구조

- `ARogue10mStylizedCharacter::GetMesh()`가 실제 Appearance body이다. 숨긴 Manny `AnimationSourceMesh`를 부모로 두고, 별도 Skeleton의 Retarget AnimBP로 재생한다. 외형의 tick은 SourceMesh 뒤에 실행된다.
- `GetAnimationPlaybackMesh()`는 Stylized에서 AnimationSourceMesh, 일반 Character에서는 기존 FirstPersonMesh를 반환한다. 일반 Character의 새 모드 fallback도 명시적으로 바꿔야 한다.
- 기존 FirstPersonPresentation은 Appearance를 OwnerNoSee로 숨기고 FirstPersonMesh를 Manny 전신으로 교체해 보였다. 기존 머리 follow도 이 메시의 FistAnimInstance 또는 SingleNode를 읽는다.
- 실제 카메라는 생성자에서 FirstPersonMesh의 head에 붙어 있다. 새로운 경로는 Appearance head를 기준으로 한다.
- Hair/Facial은 Appearance에 LeaderPose로 붙으며 OwnerNoSee=true가 기본이다. 본체 외형 갱신이 mesh/AnimClass를 바꾸므로 카메라 기준과 가림 정책을 다시 확인해야 한다.
- CombatComponent::GetEffectAttachMesh는 로컬에서 FirstPersonMesh를 우선하므로 새 모드에서는 Appearance로 바꿔야 한다.

## 선택한 구조

### 화면과 동작의 단일 기준

화면에 보이는 몸은 Appearance body 한 개다. 별도 First Person Mesh는 새 모드에서 렌더·애니메이션·이펙트 재생에 참여하지 않는다. Blueprint 직렬화된 inherited component와 기존 API 참조가 있으므로 컴포넌트 정의는 비활성 호환용으로 남길 수 있다. 이는 이번 카메라에 필요한 메시가 아니라 기존 Blueprint를 한 번에 깨뜨리지 않기 위한 이전 단계다. 사용자에게 잔존 이유를 명확히 알린다.

숨긴 Animation Source Mesh는 First Person Mesh와 다른 역할이다. 현재 Appearance의 서로 다른 Skeleton이 기존 Manny 애니메이션을 재생하기 위한 리타깃 소스이므로 유지한다. 실제 화면에는 Appearance만 나오며 카메라도 Appearance에서 읽는다.

### 기본 공격

기존 Source locomotion/idle ABP 및 Appearance Retarget ABP를 유지한다. FistAnimInstance를 SourceMesh에 덮어 씌우지 않는다. 원본 Boxing에서 실제 왼잽 및 오른잽 시퀀스/몽타주를 제작하고, 기존 입력·스킬 재생 경로에 연결한다. 오른잽이 원본 미러 파생이면 그 사실을 결과 문서에 적는다. 공격 타격 시각은 동작 최대 신장 시각과 연결하고, 이동·Idle 복귀·콤보 중단에서 원상 복귀를 확인한다. 기존 우클릭 스트레이트·홀드 훅 입력 의미는 유지하며 이번 원본 좌클릭 연결과 구분한다.

### 머리 카메라

새 Appearance camera 경로는 최종 Appearance head의 위치·회전을 읽는다. 카메라 위치는 head에 대한 눈 위치 오프셋으로 노출하고, 본 좌표축 차이는 기준 head 회전으로 보정한다. 마우스 시선은 별도로 합성하되 actor yaw를 두 번 더하지 않는다. 공격이 머리를 뒤로 돌리면 실제 시야도 따라가야 한다. 기존 FirstPersonPresentation의 head/additive 동작을 동시에 적용하면 두 배로 흔들리므로 새 모드에서 그 경로를 배제한다.

머리 안쪽 표면과 머리카락이 시야를 막지 않아야 한다. head/neck HideBone은 변환 스케일을 0으로 만들 수 있어 카메라를 숨긴 소켓의 최종 변환만으로 계산하면 안 된다. 개발자는 실제 Appearance pose의 숨김 전 head 변환을 읽거나, 카메라 위치를 보존하는 검증 가능한 가림 방법을 적용한다. Pelvis/상체/팔다리는 숨기지 않는다. 원본 애니메이션의 팔 위치에 별도 시야용 IK 보정을 추가하지 않는다.

CameraManager의 최종 POV와 공격 aim 방향을 같은 기준으로 맞춘다. 공격 trace가 카메라 컴포넌트를 쓰는 기존 위치는 별도 확인한다. 카메라 전환·사망·unpossess·외형 갱신에서는 잘못된 캐시나 타 Pawn pose를 재사용하지 않는다. 무기 변경 후 FirstPersonMesh 경로로 자동 복귀하지 않는다.

## Ultrawork Packets

| Packet | 목표 / 위치 | 완료 조건 | 검증 | 되돌림 경계 |
| --- | --- | --- | --- | --- |
| P1 개발1 | Appearance camera component, 기존 Presentation/CameraManager 경로 분기 | 최종 Appearance head를 따라가며 마우스 조작·전신 표시 유지, 중복 흔들림 없음 | Editor 빌드 + 실제 Pawn 런타임 카메라/시야 샘플 | 새 카메라 모드와 component 변경만 |
| P2 개발2 | Boxing 시퀀스·몽타주와 스킬 연결, 기존 Source ABP 유지 | 좌클릭 실제 원본 Boxing 재생, 연속 입력/타격/Idle 복귀 확인 | 에디터 생성 로그 + 런타임 입력/타격 샘플 | 생성 애니메이션 및 변경 스킬 경로 |
| P3 통합 | Character/Stylized lifecycle, playback/effect mesh 라우팅 | FP 메시가 비활성이어도 공격·이펙트·외형 교체가 유지 | 활성 메시·socket·부모·모드 로그 및 시각 캡처 | Character/effect 라우팅 |
| P4 검증1·2 | 독립 소스 검토, 실제 화면/동작 검토 | 빌드 통과 + 요구별 재현 증거 + 제한 명시 | 관련 런타임 테스트, 영상/정지 프레임 | 검증·문서 산출물 |

요청된 역할은 기획1 / 개발2 / 검증2로 운영한다. 동시 실행 슬롯에 맞춰 기획 담당은 구현 후 검증1로 전환한다. root는 통합·빌드·기록을 담당한다.

## 검증 기준

1. 실제 사용 Pawn의 Appearance asset/retarget ABP가 변경 전후 동일하다. 기존 FirstPersonMesh는 화면에 안 나오고 해당 컴포넌트 애니메이션·이펙트에 의존하지 않는다.
2. 카메라가 Appearance head 위치의 허용 오차 내에 있으며, Idle/Boxing/걷기/점프의 pose 변화가 시야에 반영된다. 마우스 pitch/yaw와 머리 회전이 중복 합성되지 않는다.
3. 기본 시작 시선 -30도는 한 번 적용되고 자유 입력 및 무기 전환 뒤 유지된다.
4. LMB 왼잽/오른잽 동작·판정·콤보 복귀가 새 Source montage에서 실행된다. 원본 기반 동작이라는 사실을 재생 애셋 로그로 증명한다.
5. Appearance/head/손발·몸통이 원하는 방향으로 보이고 머리 안쪽/머리카락이 화면을 덮지 않는다. 실제 영상으로 판단한다.
6. 공격 조준 및 이펙트 부착은 실제 표시 몸과 시야를 기준으로 하고 숨어 있는 FP 메시를 쓰지 않는다.
7. 외형 변경·무기 전환·생명주기 종료 후 안정적이다. 일반 Character fallback 및 Stylized 경로를 소스/실행 범위에 맞게 구분해 기록한다.
8. `Scripts/BuildEditor.ps1`, 관련 runtime test, `Scripts/CheckGeneratedChanges.ps1` 및 diff 검사를 완료한다. 미실행 항목은 통과로 기록하지 않는다.

## 문서 종료 조건

최종 구조와 실제 실행 증거를 `Feature/doc/2026-09-26_appearance-head-camera.md`에 남긴다. `DevLog/20260926.txt`에 한국어 개발 결과와 Notion 요약 후보를 append하고, `Docs/SprintChangeLog.md`에 변경/검증/상태를 반영한다.

## 종료 기록

2026-09-26: P1~P4 완료. 실제 원본 head 추종과 별도 FP 비활성화, 좌우 잽 몽타주 연결을 구현했다. 최종570프레임 검증 PASS. 모든 공격이 정면 표적에 명중한다는 가정 대신 실제 시야의 독립 충돌 질의와 피해 결과를 대조했다. 스트레이트 원본의 큰 머리 회전에 따른 정상 빗나감·시야 가림은 유지한다. 실행 범위와 남은 생명주기 확인은 결과 문서 참조.
