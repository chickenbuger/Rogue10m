# 전신 그림자 소스 리뷰 — 2026-09-28

## 판정

최신 저장본에서 현재 Appearance 머리 카메라와 V 3인칭 경로를 막는 추가 코드 결함은 발견하지 못했다. 원래 그림자 비활성 설정 무시, 보이지 않는 파츠의 그림자 생성, 오래된 proxy 누적 문제는 수정되어 재검토했다. 이 판정은 소스와 UE5.8 엔진 구현을 읽은 결과이며 빌드·실제 그림자 렌더링 통과를 대신하지 않는다.

검토 파일: `Rogue10mAppearanceShadowComponent.h/.cpp`, `Rogue10mAppearanceCameraComponent.h/.cpp`. 관련 캐릭터 파츠 생성과 커스터마이징 코드, runtime fixture의 검증 범위도 확인했다. 상품 코드는 리뷰 과정에서 수정하지 않았다.

## 확인한 계약

- **GC/소유권**: camera의 UPROPERTY/TObjectPtr ShadowComponent, shadow component의 UPROPERTY Entries, 각 entry의 UPROPERTY/TObjectPtr Proxy를 통해 새 UObject가 추적된다. 원본 component와 asset 캐시는 TWeakObjectPtr이며 소유 Actor의 instance component 목록에도 등록된다.
- **원본 포즈 보존**: Body GetBoneSpaceTransformsView의 raw local 배열을 proxy BoneSpaceTransforms에 복사한다. 숨긴 rendered component-space head scale을 복사하지 않는다. proxy에는 AnimBP·montage 재생이 없고 독립 tick도 없다.
- **갱신·bounds**: 엔진 `PoseableMeshComponent.cpp`의 RefreshBoneTransforms가 FillComponentSpaceTransforms, FinalizeBoneTransform, UpdateChildTransforms, UpdateBounds, MarkRenderTransformDirty, MarkRenderDynamicDataDirty를 수행함을 확인했다. 명시적인 Refresh 호출이므로 tick 비활성이 포즈 갱신을 막지 않는다.
- **표시와 충돌**: proxy는 MainPass/DepthPass/CustomDepth를 끄고 hidden-shadow를 사용한다. collision/overlap/navigation은 꺼져 있어 공격·이동 판정에 참여하지 않는다. FirstPersonMesh를 활성화하지 않는다.
- **그림자 중복·복원**: 활성화 최초 원본 CastShadow를 저장하고 원본을 끈다. proxy CastShadow는 저장한 값에 따르며 원래 false였던 파츠에 새 그림자를 강제하지 않는다. V 전환/기능 비활성/camera Restore에서 원본 플래그를 돌려주고 proxy 그림자를 끈다. 반복 호출은 적용 중 false 값을 원본 상태로 덮어쓰지 않는다.
- **파츠 처리**: 현재 body의 직접 자식이면서 LeaderPose가 body이고 실제 asset이 있는 visible/non-hidden skeletal 파츠만 선택한다. 현재 카탈로그의 얼굴·머리카락 경로와 맞는다. body CS를 파츠 본 이름으로 매핑하고 파츠 계층을 따라 local을 재구성한다. 미매핑 accessory 본에는 파츠 reference local을 유지한다.
- **외형 교체**: proxy asset이 달라지면 SetSkinnedAssetAndUpdate, bone map의 body/part asset cache 검증, material 슬롯 동기화를 수행한다. 이전 외형의 불필요한 material override를 정리한다. source component에 부착된 proxy의 상대 변환은 identity라 source component 변환을 따른다.
- **오래된 파츠**: 현재 탐색에서 사라진 entry는 원본 상태 복원 후 instance 목록에서 제거하고 proxy를 DestroyComponent한 뒤 entry를 제거한다. child 목록은 proxy 생성 전 snapshot으로 확보하여 순회 중 attach-child 증가 문제를 피한다.
- **종료**: EndPlay에서 원본 shadow 복원 후 모든 proxy를 unregister/destroy하는 ActorComponent 경로로 정리한다. camera와 shadow component의 EndPlay 순서가 바뀌어도 빈 entry 또는 반복 Restore 호출은 안전하다.
- **입력·카메라**: 새 연결은 기존 RefreshAppearanceCamera에서 head-hidden owner 상태에만 그림자 활성화를 요청한다. head 계산·ControlRotation 결합·V OutPOV·공격 trace용 카메라 경로는 바꾸지 않는다.

## 수정되어 해소된 항목

| 항목 | 초기 구현 | 최신 구현 |
|---|---|---|
| 원본 CastShadow=false | proxy에 true 강제 | 저장값에 따라 proxy 설정 |
| 숨긴 cosmetic | asset과 LeaderPose만 확인 | IsVisible 및 !bHiddenInGame 추가 |
| 삭제·교체된 cosmetic | 그림자만 비활성화하고 entry 유지 | restore + instance 제거 + destroy + entry 제거 |

## 검증해야 할 범위와 한계

- 실제 runtime에서 머리 silhouette 복원을 확인해야 한다. raw pose 일치만으로 최종 VSM에서 머리 그림자가 보인다는 사실까지 보장할 수 없다.
- 현재 render 설정은 VSM 활성이고 proxy는 `VisibleInRayTracing=false`다. 프로젝트의 r.RayTracing=True 설정과 별개로, 이 구현은 ray-traced 광원 그림자·반사에서 full-body proxy를 제공하지 않는다. 해당 경로까지 복원했다고 보고하면 안 된다.
- 원본 CastShadow만 변경·복원한다. lighting channels, dynamic/static/contact shadow 등의 비기본 세부 설정과 morph/cloth 변형 전체를 복제하는 일반적인 메시 복제기는 아니다. 현재 캐릭터 코드/카탈로그에는 해당 세부 설정을 조절하는 경로가 확인되지 않았다.
- 현재 파츠 계약은 direct-child LeaderPose skeletal cosmetics이다. head socket static mesh 장식, groom, 독립 애니메이션 파츠까지 지원했다고 확대 해석하지 않는다.
- 같은 frame에 camera/공격 trace가 여러 번 refresh를 요청하면 poseable CS 계산과 render dirty 요청도 반복될 수 있다. 애니메이션 중복 평가를 추가하지는 않지만 CPU pose 복사·추가 shadow skinning 비용 자체가 없다고 표현하면 안 된다. 이번 코드 리뷰에는 GPU 프로파일링이 포함되지 않았다.
- 외형 교체, 종료, 비활성 복원은 정적 검토와 runtime fixture에서 실제 검사한 항목을 결과 문서에서 구분해야 한다.

## 검토 시점 SHA256

| 파일 | SHA256 |
|---|---|
| AppearanceShadowComponent.h | D12BFC3DAC769AFE37374E82615414A3FCF8B1431FF5A6EE03EBED258A748973 |
| AppearanceShadowComponent.cpp | 65A673332864C16E65EFC9A8734E55FC33CF076E4A6FD89427751286A28A26C0 |
| AppearanceCameraComponent.h | B4B3E481297F3BFF2F8288FE66266648243DBBEA381156264ADB44A3211EA725 |
| AppearanceCameraComponent.cpp | 95332620871DFE47205ABBB008AECE978B259D320C55EE3EC74BD9A5A5D4F1F3 |
