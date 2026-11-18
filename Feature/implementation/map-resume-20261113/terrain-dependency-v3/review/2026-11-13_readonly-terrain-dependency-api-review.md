# Terrain dependency v3 읽기 전용 설계·API 검토

2026-11-13. 검토자는 파일 읽기와 이 신규 문서 작성만 수행했다. Source/Config/Content, staged/helper 파일, 기존 문서와 외부 Codex 자료를 수정하지 않았다. UE/UBT/MCP/Game/GUI, 프로세스 조회·종료를 실행하지 않았다. 실제 UObject/Asset Registry inventory를 얻은 결과가 아니라 구현 전 API·범위 검토다.

기준은 현재 공유 `Source/Rogue10m/Editor/Rogue10mCompactWorldAuthoring.cpp`, SHA256 `3A730A23AF3A92BBD569034702D6394F576E929D9C933505333D51D1A40FAC19`다. v2는 exact candidate/Editor/no-PIE, 256 또는 retained45 격자·collision·원점 검사를 제공한다. 이 검사는 참조 완전성이나 crop 안전 판정이 아니다. 부모가 전달한 full-region actor1360은 조사 예상 범위이며 이 문서에서는 live 재측정하지 않았다.

## 실행·판정 차단 조건

1. 무로드 조사에서 `FObjectPropertyBase::GetObjectPropertyValue`, `TObjectPtr::Get`, `LoadObjectPropertyValue`, `TryLoad`, `LoadSynchronous`, CDO 생성, Blueprint compile/construction script, PCG generate/cleanup, spline rebuild, foliage fixup을 호출하면 안 된다. 특히 TObjectPtr의 Get은 늦은 object handle을 resolve한다. 이미 resolved인 복사본만 Get하고 unresolved는 경로·상태로 남긴다.
2. Asset Registry의 0건을 actor/component 무참조나 삭제 안전으로 바꾸면 안 된다. 이 API는 on-disk package 관계이며 dirty live state·property/subobject 관계와 다르다.
3. UObject reflection 또는 FReferenceFinder 하나로 soft/native/serialized/Blueprint/foliage 전체를 조사했다고 표시하면 안 된다. 아래 plane별 `complete`, `unsupported`, `unresolved`, `truncated`를 독립 기록한다.
4. Foliage instance/base 연결은 native 비반영 필드가 있으므로 reflection-only 결과에서는 미검증이다. PCG 동적 선택·실행 시 생성 관계, 동적 문자열 경로·이름/GUID 조회도 정적 UObject 참조와 동등하지 않다. 실제 대상이 있고 필요한 reader가 없으면 교체 안전 판정을 차단한다.
5. 미로드 actor/Level Instance container, async load/GC 중 객체, 읽는 동안 descriptor/object 집합·dirty/selection/편집 레이어가 달라짐, budget 초과·잘린 결과, registry gathering/scan 범위 미확정이면 완전성 주장을 차단한다. 관측 실패를 대상0개로 바꾸지 않는다.
6. 외부 소비자의 outside component/collision/texture 참조, candidate→source terrain namespace 참조, retained/outside가 공유하는 proxy/atlas texture는 별도 해결 전 변경 차단 사유다. 일반 공유 material/mesh/LayerInfo asset 참조는 terrain actor/subobject 소유권과 구분한다.

## 가장 좁은 다중 plane 조사

하나의 전체 완전성 플래그 대신 다음을 함께 출력한다. 참조를 조사하는 함수는 삭제 가능 목록을 반환하지 않는다.

| Plane | 최소 seed와 산출물 | 확인할 수 없는 것 |
|---|---|---|
| Saved metadata / package graph | candidate map, 실제 actor→external package 매핑, external objects, terrain 관련 texture/LayerInfo/brush/BP package의 양방향 direct edges | 특정 actor/component/property, dirty state, 미등록·미스캔 패키지 내부 |
| World Partition descriptors | 모든 관련 container의 GUID/package/full actor path/native class, GetReferences/GetEditorOnlyReferences, IsLoaded와 실제 actor 매핑 | property path, 임의 soft path와 동적 runtime 조회 |
| Loaded reflected values | actor1360의 실제 범위, 그 소유 subobjects/components, terrain/layers/brushes/textures, 이미 loaded BP classes/CDO/templates; nested property path별 hard/soft/weak/interface/delegate | native 비반영 저장소, custom serialization, 미로드 BP 내부, 실행으로 생성할 관계 |
| Optional native GC references | FReferenceFinder의 property+AddReferencedObjects 결과 또는 FReferencerFinder loaded referencers. reflection 결과와 별도 provenance | 모든 soft path와 on-disk/custom-Serialize 전체, project-wide incoming refs |
| Optional current-object serialization | 명시적으로 검토된 custom archive가 현재 UObject의 Serialize 경로에서 관측한 hard/soft paths | 원래 디스크 bytes와의 동일성, 실행되지 않는 코드 분기·동적 관계. Serialize가 순수 관측이라는 일반 보장도 없음 |

첫 v3의 두 파일·Build.cs 무변경 범위에는 loaded reflection과 기존 Landscape 공개 getter를 우선 사용한다. typed PCG/Foliage/AssetRegistry plane을 구현할 권한·모듈이 없으면 이를 실행했다고 채우지 않고 `not_queried`/`unsupported`로 남긴다. 이 한계를 숨겨 inventory complete를 만들지 않는다.

## Loaded UObject 조사 세부

- exact candidate world와 기존 no-PIE 검사를 재사용하고 game thread에서 수행한다. 처음·끝의 actor GUID/package/object path 목록을 정렬해 비교한다. 1360개의 파일·actor·descriptor는 서로 다른 count이므로 별도 기록한다. `GetActor(false, false)`/`IsLoaded(false)`의 loaded 대상만 사용하며 descriptor를 로드하거나 pin하지 않는다.
- candidate Landscape/Main/StreamingProxy, render component256, collision256, edit-layer objects, 실제 brush actors, LandscapeInfo, spline components/controlpoints/segments, raw/composite textures를 roles로 나눈다. `outside211`, `retained45`, `collision_of_outside`, `mixed_proxy`, `shared_texture`, `nonterrain_actor`, `source_namespace`, `common_asset`를 구분한다.
- direct references와 transitive closure를 섞지 않는다. 소유 객체 closure를 만들 때 Outer 관계와 UObject reference edge를 별도 기록한다. actor의 외부 package와 UObject Outer/Owner는 같은 개념이 아니다.
- property walker는 inherited fields와 `ArrayDim`을 포함한다. Array는 element index, Map은 key/value 양쪽, Set은 sparse slot을 조사한다. Map/Set은 `GetMaxIndex()`와 `IsValidIndex()`를 사용하고 `Num()`을 연속 slot 범위로 쓰지 않는다. Struct는 실제 struct type을 기록하고 재귀한다. 순환/깊이/행수 예산은 누락 카운터와 truncated 플래그를 남긴다.
- 검사 순서는 SoftClass/SoftObject, Weak/Lazy, Interface/Delegate, Object/Class, Struct/containers처럼 좁은 종류를 먼저 구분한다. 모두 FObjectPropertyBase로 처리하면 soft의 타입·주소 계약을 잃는다.
- SoftObject/SoftClass는 `ToSoftObjectPath()`의 전체 package.object:subobject를 보존한다. `GetLongPackageName()`은 검색 보조 key일 뿐 원본 edge를 대체하지 않는다. FSoftObjectPath/FSoftClassPath 구조체도 별도 처리한다. FName/FString은 임의 참조로 단정하지 말고 이름 기반 사용 가능성을 unknown으로 남긴다.
- `TObjectPtr<UObject> P = ObjectProperty->GetObjectPtrPropertyValue(Address)`는 복사본이다. `P.IsResolved()`를 확인한 뒤에만 `P.Get()`을 사용한다. 미해결은 `P.GetPathName()`와 unresolved 상태만 기록한다. interface/delegate에서 내부 TObjectPtr를 우회 resolve하지 않는지도 확인해야 한다.
- Weak는 강한 수명 소유권과 구분하고 stale/null 상태를 보존한다. Lazy는 GUID를 기록할 수 있으나 getter/resolve를 실행하지 않는 범위라면 unresolved GUID edge로 남긴다. interface, delegate receiver, FieldPath, InstancedStruct/PropertyBag native storage, bytecode, native raw pointer 처리가 없으면 종류별 unsupported를 명시한다.
- 전체 process incoming 관측이 필요하면 candidate actor만 순회해서는 안 된다. 이미 loaded된 외부 세계·BP·CDO·templates를 포함하는 bounded process-object scan이 추가로 필요하다. 전체 UObject 수/포함·제외 플래그/처리 예산을 기록한다. 전체 프로젝트 asset을 로드해서 이 빈틈을 채우는 작업은 별도 범위다.

## FReferenceFinder / FReferencerFinder의 실제 의미

`CoreUObject/Public/UObject/UObjectGlobals.h:3197`:

```cpp
FReferenceFinder(TArray<UObject*>& InObjectArray, UObject* InOuter = nullptr,
    bool bInRequireDirectOuter = true, bool bInShouldIgnoreArchetype = false,
    bool bInSerializeRecursively = false, bool bInShouldIgnoreTransient = false);
void FindReferences(UObject* Object, UObject* ReferencingObject = nullptr,
    FProperty* ReferencingProperty = nullptr);
```

UE5.8의 실제 `GarbageCollection.cpp:4004`는 UClass가 아니면 `AddPropertyReferences`, 이어 `CallAddReferencedObjects`를 호출한다. 일반 `UObject::Serialize` 전체를 실행하는 패스가 아니다. direct edge를 얻을 때는 `InOuter=nullptr`, recursion=false로 하고 transient/archetype 포함 여부를 결과에 명시한다. candidate Outer로 필터하면 중요한 외부 세계/자산 참조를 버릴 수 있다. 이 기본 클래스는 unique object 목록으로 중복을 줄이므로 모든 property occurrence/path의 증거로 쓰지 않는다. native AddReferencedObjects callback의 무부작용·무로드를 모든 플러그인에 대해 보장하지 않으므로 strict no-load v3에 무조건 추가하지 않는다.

`CoreUObject/Public/UObject/ReferencerFinder.h:29`:

```cpp
static TArray<UObject*> GetAllReferencers(const TArray<UObject*>& Referencees,
    const TSet<UObject*>* ObjectsToIgnore,
    EReferencerFinderFlags Flags = EReferencerFinderFlags::None);
// TSet<UObject*> overload도 있음
```

이것은 loaded process의 referencer 객체 목록이다. `SkipInnerReferences`, `SkipWeakReferences`는 결과를 바꾸므로 flag를 기록한다. 구현의 weak 포함 경로는 SerializeBin을 이용하며 엔진 주석도 custom Serialize의 일부 누락 가능성을 적고 있다(`ReferencerFinder.cpp:123`). group별 batch 조회는 가능하지만 수백 target 각각 전체 process를 재검색하는 방식은 피한다. 결과가 특정 target/property를 증명하는지 별도 edge 조회로 보완한다. 0은 project-wide 무참조가 아니다.

`ArchiveUObject.cpp:96–124`에서 기본 reference-collector가 modifying=false이면 soft pointer를 건너뛴다. custom serialization 패스가 필요하면 `FSoftObjectPtr&`와 `FSoftObjectPath&` operator를 직접 override해 경로만 기록해야 한다. loading/saving/modifying flag, pointer 대입, asset load를 켜지 않는다. reflection·native ARO·Serialize의 세 가지 coverage를 서로 대신했다고 표시하지 않는다.

## Asset Registry / Blueprint

공개 `IAssetRegistry`의 `GetDependencies/GetReferencers(FAssetIdentifier, TArray<FAssetDependency>&, EDependencyCategory, FDependencyQuery)`를 사용하면 `AssetId/Category/Properties`를 보존할 수 있다. FName 배열 overload는 edge 속성을 잃는다. 기본 감사는 `Package + NoRequirements`다. Hard-only/Game-only는 editor brush·PCG·soft 관계를 누락한다. Manage와 SearchableName은 별도 관계이며 actor hard reference가 아니다. Hard 부재는 Soft, Game 부재는 EditorOnly이고 Build는 cook 변환 관련 속성이다. direct edge와 별도 visited-set 전이 closure를 구분한다.

`IsGathering()`은 초기·후속 수집 상태를 보여 주며 `IsLoadingAssets()`만으로 모든 gather가 끝났다고 판단하지 않는다. mount/scan 범위와 dirty live 상태 차이를 함께 기록한다. `GetAssetsByPackageName(..., bIncludeOnlyOnDiskAssets=true)`는 metadata이며 external actor package마다 top-level asset이 있다고 가정하면 안 된다.

현재 Runtime Rogue10m Build.cs에는 AssetRegistry의 명시적 의존이 없고 Rogue10mEditor에는 있다. 두 Source 파일만 바꾸는 범위에서 새 AssetRegistry 직접 호출/링크를 추가하지 말고, owner가 모듈 의존과 실행 범위를 먼저 정한다.

actor instance만으로 Blueprint 참조를 다 읽은 것은 아니다. 이미 loaded된 Blueprint의 GeneratedClass, `GetDefaultObject(false)`로 이미 존재하는 CDO, ComponentTemplates, SimpleConstructionScript, InheritableComponentHandler, DynamicBindingObjects, Timelines를 역할별로 포함한다. SkeletonGeneratedClass는 transient·불완전하므로 완성 class 증거가 아니다. 미로드 BP는 FAssetData의 GeneratedClassPath/ParentClassPath/NativeParentClassPath tags까지만 관측하고 내부 graph/property는 not_inspected로 남긴다. compile·construction script·CDO 생성으로 inventory를 보완하지 않는다.

## Landscape / brush / PCG / spline / foliage별 보완

| 대상 | 현재 공개 읽기 지점 | 교체 위험과 누락 |
|---|---|---|
| Landscape/Component | 기존 v2 exact transform/lattice/collision, GetEditLayersConst, GetLayerData, GetHeightmap/GetWeightmapTextures의 const getter | 같은 sample·transform이어도 UObject path/GUID/identity가 바뀌면 다른 actor의 hard/soft/subobject 참조가 끊길 수 있음 |
| Edit layer / brush | Landscape.h의 GetBrushesForLayer/GetBrushForLayer, FLandscapeLayerBrush::GetBrush, loaded layer/brush reflection | brush의 BP class/CDO/material/RT/외부 actor refs와 순서·enabled state 별도. SupportsBlueprintBrushes=false만 보고 actor 부재로 추정 금지 |
| Landscape spline | LandscapeInfo::GetSplineActors/ForAllSplineActors, LandscapeSplinesComponent의 const GetControlPoints/GetSegments와 reflected ForeignWorldSplineDataMap | 다른 level/world의 soft world key와 generated mesh refs, spline→component deformation 관계. 재생성·Rebuild 호출 금지 |
| PCG | 이미 loaded component/graph/settings/managed-resource의 reflected fields와 soft generated actor/component paths | graph의 actor selector/tag/name, landscape spatial query, 동적 tracking과 미래 generation은 직접 UObject edge 없이도 의존. GetLandscapePCGData 같은 const getter도 캐시 생성 가능성이 있어 호출 금지. Generate/Cleanup/Refresh 금지 |
| Foliage | typed API가 허용되면 const GetFoliageInfos, GetBaseComponentFromBaseId, GetInstancesForComponent | InstancedFoliageActor의 FoliageInfos/InstanceBaseCache, FFoliageInfo::Instances, BaseId/BaseComponent는 비반영 native 필드. reflection-only로 base binding 완전성 불가. 삭제/수정 API 사용 금지 |
| Scene attachment | reflected AttachParent/AttachChildren, owner/level/package/GUID, component identity | retained component를 재생성하면 붙은 child/base/weak/soft 참조가 새 객체로 자동 이관된다고 보장할 수 없음 |

PCG plugin은 현재 엔진 `Engine/Plugins/PCG/`에 있다. plugin class를 문자열 접두사만으로 확정하지 말고 class path와 superclass chain으로 범주를 표시한다. typed module 없이 reflection만 사용한다면 native PCG/foliage 자료구조는 unsupported다. 미검출을 미존재로 바꾸지 않는다.

## future 9×5 crop의 판정 자료

- retained45/outside211을 actual component→proxy owner별로 집계한다. proxy가 섞인 경우 outside가 있다고 proxy actor 전체를 삭제 대상으로 분류하지 않는다. proxy 배치가 일정할 것이라는 추정 대신 실제 소유 관계를 쓴다. main Landscape는 components가 없어도 info/layers/brush/spline을 소유·조정할 수 있다.
- composite/raw height/weight texture마다 retained/outside의 실제 사용자 set을 만든다. atlas를 공유하면 outside component 삭제를 texture 전체 삭제로 바꿀 수 없다. material/LayerInfo 공용 자산과 candidate-owned terrain data를 구분한다.
- 외부 actor의 attachment, foliage base, PCG graph query/tracking, spline, delegate/soft component path, Blueprint template가 outside/retained 객체를 참조하는지를 각각 보고한다. 숫자·샘플 해시만 같은 새 Landscape로 교체하면 이 관계를 잃을 수 있다.
- 현재 snapshots는 sample 보존 증거다. referencer zero, 샘플 동일, package graph zero 중 어느 것도 native Manage/Delete 또는 재import/재생성을 승인하는 근거가 아니다. native editor crop가 참조 정리·foliage·collision·nav·PCG를 어떻게 다루는지는 별도 구현·검증 packet이다. GUI 금지인 현재 native GUI Manage/Delete는 계속 보류한다.

## 권장 report 필드와 Exit Gate

`operation`, `helper/build/source identity`, `exact_world`, `world_type`, `PIE`, `game_thread`, `actor/descriptor/package/object counts_before/after`, `target_roles`, `edges[{from,from_class,from_package,property_path,kind,to_full_path,to_package,to_guid,role,provenance,transient/editor_only}]`, `unresolved_handle_paths`, `unsupported_by_kind`, `skipped_objects`, `truncation`, `registry_state/query/scan_scope`, `dirty_before/after`, `selection_before/after`, `editing_layer_before/after`를 남긴다.

`inventory_complete_for_declared_reflected_loaded_scope`와 `crop_reference_safety_verified`를 분리한다. 후자는 이 inventory 함수에서 항상 false다. native/serialized/unloaded scope를 확인하지 못했다면 이름과 이유를 표시하고 전체 완전성은 false로 남긴다. UObject loaded count/selection/dirty package 이름이 같다는 사실은 package bytes의 비변경 증명과 다르다. 기존 v2의 source hashes·dirty·sample snapshot을 보존하고 서로 다른 증거 층을 합쳐 과장하지 않는다.

## 대조한 UE5.8 1차 근거

모든 경로의 앞부분은 `D:/Program Files/UE_5.8/Engine/Source/`다.

- `Runtime/CoreUObject/Public/UObject/UnrealType.h:2894–2897` object/ptr getters; `Private/UObject/PropertyObject.cpp:448–455` Get은 resolve, Ptr getter는 복사.
- `Runtime/CoreUObject/Public/UObject/ObjectPtr.h:116–118,198,215,759–794` Get/IsResolved/무해결 path 접근.
- `Runtime/CoreUObject/Public/UObject/UObjectGlobals.h:3181–3238`; `Private/UObject/GarbageCollection.cpp:4004–4039` FReferenceFinder.
- `Runtime/CoreUObject/Public/UObject/ReferencerFinder.h:14–31`; `Private/UObject/ReferencerFinder.cpp:123–126,323–355` scope·weak 경로.
- `Runtime/CoreUObject/Public/Serialization/ArchiveUObject.h:29–33`; `Private/Serialization/ArchiveUObject.cpp:96–129` soft 처리.
- `Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h:184,517,520,580,583,1025`; `Runtime/CoreUObject/Public/Misc/AssetRegistryInterface.h:36–156,220` package graph·category/query.
- `Runtime/Engine/Public/WorldPartition/WorldPartitionActorDesc.h:392–404`; `WorldPartitionActorDescInstance.h:81–83,106–107` GUID graph·loaded getter.
- `Runtime/Engine/Classes/Engine/BlueprintCore.h:21–25`; `BlueprintGeneratedClass.h:470–496`; `Runtime/CoreUObject/Public/Blueprint/BlueprintSupport.h:27–35` BP roles/tags.
- `Runtime/Landscape/Classes/Landscape.h:118–151,205–211,521–529`; `LandscapeInfo.h:207,498–499`; `LandscapeSplinesComponent.h:134–152,166–170` brush/spline 참조.
- `Runtime/Foliage/Public/InstancedFoliageActor.h:35–47,214`; `InstancedFoliage.h:84–88,283,307` native foliage base 자료구조.
- PCG public headers는 `Engine/Plugins/PCG/Source/PCG/Public/PCGComponent.h`, `PCGSubsystem.h`, `PCGManagedResource.h`. callback/generate/get-cache 메서드 존재를 무부작용 getter 허가로 해석하지 않았다.
