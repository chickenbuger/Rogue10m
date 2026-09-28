# 머리를 포함한 전신 그림자 복원

작성: 2026-09-28 / 작업: Sprint#4-43-full-body-shadow / 역할: 기획1

## 목표와 원인

1인칭 Appearance 머리 카메라에서는 내부가 보이지 않도록 head 본을 숨긴다. 현재 HideBoneByName은 렌더 포즈의 head와 하위 본 scale을0으로 만들기 때문에, 몸 그림자에도 머리가 사라진다. 카메라 위치·추종·공격 애니메이션을 변경하지 않고 머리가 포함된 전신 그림자를 복원한다. 별도 FirstPersonMesh를 다시 사용하지 않는다.

## ULW 범위

| 패킷 | 목표·수정 영역 | 완료 조건 | 검증 | 롤백 경계 |
|---|---|---|---|---|
| S1 그림자 표현 | 전용 ActorComponent와 UPoseableMesh 그림자 복제, Appearance raw pose 연동 | 실제 몸과 동일한 숨김 이전 포즈로 머리까지 그림자 표시, 화면에는 복제 메시 미노출 | 빌드와 본 변환 수치, 1인칭 그림자 캡처 | 새 컴포넌트 및 캐릭터 연결 |
| S2 전환·복원 | Appearance camera 활성/비활성, V 전환, 외형 교체, EndPlay | 3인칭에는 원래 메시 그림자만 사용, 캐시 설정 복원, 오래된 proxy 제거 | 전환 전후 cast/visible 플래그 및 pose, 교체·종료 검증 | camera/lifecycle 연결 |
| S3 통합 확인 | 테스트·결과·일지 | 머리 그림자 복원과 카메라·공격 경로 보존 증거 | 관련 Editor 빌드, runtime fixture, CheckGeneratedChanges | 신규 검증 자료 |

## 구현 계약

- 그림자 전용 UPoseableMeshComponent는 현재 Appearance skeletal asset과 material 슬롯을 사용한다. AnimBP·montage·물리·충돌·게임플레이 판정은 갖지 않는다. 실제 애니메이션은 기존 Source→Appearance 경로에서 한 번 평가한다.
- Appearance의 GetBoneSpaceTransformsView로 얻은 raw local pose를 그대로 복사한다. hidden-head 처리된 rendered component-space pose나 LeaderPose의 숨긴 본 결과를 복사하면 같은 결함이 반복된다.
- 같은 skeleton 순서와 개수가 확인된 경우 local 배열 전체를 복사하고 RefreshBoneTransforms로 poseable component-space/render 상태를 갱신한다. 원본 hidden bone 상태는 proxy에 적용하지 않는다. body head는 계속 숨겨서 내부 얼굴 노출을 막는다.
- shadow proxy는 MainPass와 DepthPass를 끄고 그림자만 표시한다. 프로젝트 shadow 경로에 필요한 cast 플래그를 설정하되 기존 원본의 그림자 허용 상태를 존중한다. 숨김 상태를 사용할 경우 CastHiddenShadow를 함께 고려한다.
- 1인칭 활성 중 원래 body의 그림자는 임시로 끄고 proxy를 켜서 겹치는 두 그림자를 만들지 않는다. V 3인칭에서는 proxy 그림자를 끄고 원래 body 설정을 되돌린다. 이때 캐시 원본 값을 재캐시해 덮어쓰지 않는다.
- 기존 camera/pose 갱신 지점과 결합해 같은 프레임의 최신 포즈를 사용한다. 매번 메시 생성·본 이름 검색·배열 재할당을 하지 않으며, 독립 Tick은 기본 비활성으로 둔다. 병렬 애니메이션 평가 완료 뒤 복사해야 한다.
- 생성한 UObject component 참조는 UPROPERTY/TObjectPtr로 추적하고, 원본 component 캐시는 TWeakObjectPtr로 둔다. 소유 Pawn 외 다른 캐릭터의 그림자를 변경하지 않는다.

## 얼굴·머리카락 파츠

얼굴·머리카락이 body와 별도 skeletal component인 외형도 점검한다. 실제로 그림자를 드리우는 파츠가 head 숨김의 영향을 받으면 해당 파츠에도 그림자 proxy가 필요하다. material의 masked/투명 silhouette와 component transform을 보존한다. LeaderPose 파츠는 자체 raw pose 배열이 비어 있을 수 있으므로 body raw pose를 본 이름·reference skeleton으로 매핑하거나, 원본 파츠의 유효한 숨김 이전 포즈를 사용해야 한다. 렌더 후 숨긴 포즈를 단순 복제하지 않는다. 지원하지 않은 파츠를 검증 없이 복원 완료로 표시하지 않는다.

## 생명주기와 복원

각 원본 component의 최초 shadow 설정을 저장하고 비활성·3인칭·외형 교체·EndPlay에서 복원한다. 외형 교체 시 기존 proxy와 원본 캐시를 정리한 뒤 새 asset/material/bone map을 준비한다. 실패 또는 유효하지 않은 pose에서는 원래 그림자로 복구하며, 검은 화면이나 화면용 proxy 노출을 만들지 않는다. camera 컴포넌트 비활성이나 로컬 소유권 변경 시에도 같은 정리가 필요하다.

## 검증 및 제외 범위

- head가 숨겨진 body 렌더 포즈와 달리 proxy head scale은 정상이며, 원본 raw local pose와 proxy pose가 일치하는지 수치 확인한다.
- idle·잽·훅·걷기·점프에서 그림자가 동작을 따라가고 머리 실루엣이 보이는지 새 runtime 캡처로 확인한다.
- 1인칭↔V 3인칭 반복, 외형 교체, 비활성/종료에서 이중 그림자·설정 누수·머리 내부 노출이 없는지 확인한다.
- camera cache·공격 조준·montage/입력 연결·FirstPersonMesh 비활성 상태를 유지한다. 이번에는 애니메이션 안정화나 HUD 변경을 추가하지 않는다.
- 현 조명/VSM 경로에서 본 결과와 다른 렌더 경로(ray tracing 등)까지 검증한 결과를 구분한다. 추가 poseable skinning·shadow draw 비용은 구현 검토와 실행 확인 대상으로 남긴다.

결과 문서와 DevLog/Sprint 변경 기록은 통합 담당자가 작성하며, 독립 검증1은 구현 후 코드·상태 복원·pose 일치 증거를 검토한다.
