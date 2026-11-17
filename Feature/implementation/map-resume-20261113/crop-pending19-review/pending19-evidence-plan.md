# 실제 pending 19개에 대한 최소 비GUI 증거 계획

2026-11-13. 이 작업은 문서 검토만 수행했다. 기존 inventory, Source/Config/Content/Build/Scripts/Plugins, 동결 helper를 수정하지 않았고 UE/UBT/UAT/MCP/Cook/Game/GUI를 실행하지 않았다. Computer Use 및 다른 API로 같은 GUI 입력을 보내는 우회도 사용하지 않았다. **실제 crop 실행 권한·전체 editable 보존 인증은 계속 false다.** 아래 API는 후속 설계이며 이 문서가 구현·실행을 승인하지 않는다.

## 판단과 최소 차단 조건

실제 독립 QA는 수치·바이너리 정적 검사를 통과했고 explicit failure는 없다. 남은 19개는 **source inventory coverage 10개, 복원계획·실측 8개, destination 비교 1개**다. 8채널을 다시 추출하는 일이 다음 우선순위는 아니다. 이미 확보한 증거를 범위별로 연결하고, 빠진 authoring metadata·참조·경계·충돌만 읽는 작은 수집기를 설계하는 것이 가장 좁다.

현재 재작성 경로의 핵심 차단 조건은 네 가지다.

1. **literal EditLayer GUID 정책과 Import 동작이 충돌한다.** 현재 정책은 `literal_identity_required_until_explicit_mapping_is_reviewed`다. `Import(InImportLayers)`는 새 EditLayer GUID를 만든다. 현재 정책 아래에서 이를 literal 보존으로 표시할 수 없다. `SetGuid(old)`만 다시 호출하는 방법도 component LayersData key·외부 참조 전체를 재매핑하지 않는다. GUID 정책 또는 실제 보존 경로가 별도 검토되기 전 Import 실행 단계로 넘어가지 않는다.
2. **raw B/A는 보존해야 할 실제 데이터이며 수송 검증이 아직 없다.** 실제 179,488개에서 packed min 22,139/max 45,181/distinct 2,065다. 0xFFFF와 다른 값은 전부이고, 0x7F7F와 다른 값은 172,026개다. 현재 단일 additive layer라는 이유로 이 바이트를 버리거나 기본값으로 대체하지 않는다. 공개 setter가 비트를 쓸 수 있다는 코드 근거와, 실제 setter→합성→저장/재로드 이후 동일하다는 증거는 별개다.
3. **authoring 설정과 참조가 미완이다.** per-component overrides, layer별 paint blend 관계, material/grass/RVT/physics 설정을 아직 전부 목록화하지 않았다. v3는 PCGSubsystem 1개와 미지원 property 14종을 관측했다. 이를 의존성 0으로 바꾸지 않는다.
4. **새 경계의 composite normals/halo·렌더 합성·실제 collision·cold reload가 미검증이다.** RG/paint/BA 8채널 동일성만으로 전체 texture·표면·향후 편집 동등성을 인증하지 않는다. NullRHI `IsUpToDate()`의 true도 새로운 합성 완료 증거가 아니다.

## 직접 읽어 확인한 현재 증거

기준 경로는 `Feature/doc/evidence/map-resume-20261113/`다.

| 입력 | 확인 결과 | 이 검토에서 확인한 SHA256 |
| --- | --- | --- |
| `RawInventoryD/inventory.json` | schema 2, source A, ROI [315,630,882,945], source layer 1개, paint 2개, presence 45개, channels 8개, fallback false | `1B610FDF079D0ED98477FB2BCD7D19F33D59B4335B9F95CC43E593CB84D48E4C` |
| `RawInventoryD/result.json` | 360 tile/1,843,200B, canonical 1,794,880B, candidate package/semantic 불변, save/crop false, GUI 0 | `C2C733E46A0F16720CA09BE551EFBE1F53E88368077CEDA20264387B6567B9C2` |
| `RawInventoryD/independent-qa-host-shortpath/result.json` | numericStaticPass true, explicitFailures [], pending 19, safeNonGUIRewritePathProven/fullFidelity/actualCrop false | `603D4E96E472AE802C8DD1E072330103650512F181369F3CC7AF10BFF66136FA` |

source map은 `/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A`, Landscape GUID는 `147B73D247543E06FDA1A7AF9D467458`다. 현재 raw layer는 `/Script/Landscape.LandscapeEditLayer`, 이름 `Layer`, GUID `9C4488BD4196BCC81051B5A001047577`, visible true/locked false, height·weight alpha 1, persistent textures/editing tools true다. paint는 visibility와 Auto Layer이며 blendMethod는 각각 0,1이고 blendGroup은 둘 다 None이다.

capture 시각은 `2026-11-13T13:37:24.015Z`다. manifest/source/DLL은 실제 연결 파일을 가진다.

- source package manifest: `79f2357a02883956b77a9354522bb8dcbd58f45aa154ec09e53bbad34b1f3255`
- exporter CPP: `2de929a98000f4efa94d74e31f0ba573c2e3cd7d199fad9a3fc734e3c432144b`
- exporter DLL: `92cb712bbc20f8299b9474fe704306089005c9760d4a1ec4e0e99bddeeb49490`

이 pin은 해당 capture epoch의 신원이다. 이후 부모의 재빌드 DLL에 예전 pin을 붙이지 않는다. 후속 수집은 새 source/DLL·manifest epoch로 기록하고, 기존 증거는 당시 바이트 그대로 유지한다.

독립 QA 결과는 각 채널의 canonical 179,488개와 seam 추가 occurrence 4,832개에 대한 실제 파일 대조 통과를 기록한다. 이 문서 작업에서 검사기를 다시 실행하지 않았다. raw B와 A의 canonical SHA256은 각각 `dd66472cc7210eacfb7d291d38686ac24a0ea163afffaf3ac9da47bc701607e2`, `9a0e1ae12d6055f130b34a0b51e0ffc174f9707097a0341f90d278590787d26a`다. 이것은 bit payload 확보 증거이며 destination 적용 증거는 아니다.

`TerrainDependencyD/dependencies.json`과 reviewed summary에서 확인한 범위:

- 전체 WP actor descriptor 1,360/1,360을 이미 로드된 actor GUID+전체 경로로 대조했다. loaded candidate actor는 1,366개로 기록돼 있다. descriptor 수와 전체 loaded actor 수를 동일한 개념으로 취급하지 않는다.
- PCG 계층 객체는 1개인 `World:PCGSubsystem_0`다. Actor가 아니며 PCG component/graph/cached execution 상태가 완전 조사됐다는 의미가 아니다. foliage/spline/brush 계층 owner 0, layer brush 배열 0을 관측했다.
- 1,045,085 reference value를 읽었고 truncation은 false다. 미지원 property 14종 때문에 collector success/loaded-reflection-complete는 false다. occurrence는 lazy GUID 528, opaque native storage 471, unknown reference-bearing type 39다.
- 세 Landscape lazy 필드는 `LandscapeComponent:CollisionComponent`, `LandscapeHeightfieldCollisionComponent:RenderComponent`, `LandscapeStreamingProxy:LandscapeActor`다. 다른 11종은 ControlRig/PropertyBag/DataHierarchy/PropertyAccess/Niagara/Python/RigVM 선언이며, occurrence owner 경로를 아직 내보내지 않아 현재 후보와 무관하다고 단정할 수 없다.
- collector에서 `source_is_terrain_internal=false`로 표시된 edge 54개는 읽은 JSON상 LandscapeInfo 36개, LandscapeSubsystem 18개다. 관측된 bookkeeping edge이며 0이 아니다. 이것을 gameplay 외부 참조 54개 또는 미지의 참조 전부로 확대하지도 않는다. 교체 후 Info/subsystem 재등록 관계는 다시 검증해야 한다.
- texture/layer/selection/editor pointer/loaded UObject/transaction/dirty 불변은 모두 true다. 이는 누락 plane의 완전성을 보완하지 않는다.

`AssetDependencyD/reviewed-summary.json`은 candidate 1,419 package 모두 등록, candidate→original world/private package 참조 0, candidate 밖 incoming package referencer 0, shared dependency 39개와 package bytes 불변을 기록한다. on-disk package plane만의 결과다. shared 목록에 실제 terrain material, LayerInfo, `LGT_SimpleGrass`, HLODLayer 등이 있으므로 공용 의존을 빈 목록으로 바꾸지 않는다. live soft subobject/native/dynamic query 관계와 분리한다.

## 가장 작은 진행 순서

**E0 — 기존 증거 연결(새 C++·UE 실행 불필요).** 향후 별도 패킷에서 현재 세 JSON, v3 dependency JSON, AssetDependencyD 결과를 manifest SHA와 source/DLL epoch로 연결한다. 기존 inventory를 이 문서 작업에서 수정하지 않는다. source layer 1개·brush/spline/foliage owner 관측·on-disk graph의 범위를 그대로 기록한다. 모르는 상태를 0 또는 `complete:[]`로 채워 검사기를 통과시키지 않는다.

**E1 — 작은 read-only authoring/relationship 수집기 설계.** 아래 A/B 설계로 property 설정과 미지원 occurrence owner를 수집한다. 기존 v2/v3/v4 결과는 유지하고 새 API로 분리한다. source map·프로젝트 경로, full 256 또는 retained 45 lattice, object/selection/editing layer/transaction/dirty/Source ID 불변 가드를 그대로 사용한다. 현재 freeze 동안 구현·실행하지 않는다.

**E2 — border/collision source baseline 설계.** 아래 C/D는 현재 데이터가 없다고 실패하도록 설계한다. 데이터를 얻기 위해 force update·physics 재생성·PCG actor 생성·렌더 합성·저장을 하지 않는다. baseline이 생겨도 destination 검증 gate는 남긴다.

**E3 — 이후 별도 destination 보존 실험.** literal GUID 정책/객체 이행표/현재 미지 참조를 먼저 검토한 뒤에만 별도 허용된 신규 사본에서 논의한다. source A와 원본을 in-place 교체하는 실행안은 아니다. 새 destination의 데이터·메타데이터·경계·충돌·cold reload 결과가 있어야 해당 plan gate를 평가할 수 있다. 이 단계의 구현·실행은 현재 작업에 포함되지 않는다.

## 실제 19개 항목별 다음 증거

| # | 실제 pending key | 현재 사실과 가장 좁은 다음 증거 / 공개 API | 남겨야 할 판정 |
| --- | --- | --- | --- |
| 1 | `destination.candidate_inventory_and_per_channel_comparison` | destination=null. 현 단계는 source A 증거만 연결한다. 이후 별도 후보에서 같은 v4 8채널·45 tile와 A/B/C/D 메타데이터를 수집해 GUID/전체 경로·section base·world XY 매핑을 함께 비교한다. | source 추출을 destination roundtrip으로 표시하지 않음. |
| 2 | `inventory.brushes` | v3 layer brush 배열 0, owner 0. 해당 두 경로와 all-desc epoch를 연결한다. 재조회 시 `GetLayersConst`/`GetLayerConst`, 해석되지 않은 brush handle 사전 검사 후 `GetBrushesForLayer`; owner는 copied object handle path로 읽는다. | 현재 candidate source 범위의 관측된 빈 목록. 원본/다른 맵의 brush 부재나 향후 Import 무이동 보증 아님. |
| 3 | `inventory.componentOverrides` | 전부 미수집. A에서 256개 component의 source section base·owner 및 retained/outside 역할, OverrideMaterial/OverrideHoleMaterial/PerLOD, LOD/collision/RVT/physics·bounds 설정을 기록한다. stored UPROPERTY와 `GetPerLODOverrideMaterials() const`를 사용한다. | default와 explicit override를 구분. mixed proxy/atlas 전체를 삭제 가능으로 표시하지 않음. |
| 4 | `inventory.compositeNormalsAndBorderHalo` | 8채널에 composite B/A/외부 halo 없음. C에서 composite `FGuid()` RG+B+A와 경계 바깥의 실제 source sample을 읽는다. mip0의 1-ring [314,629,883,946]은 최소 조사 시작 범위이며 모든 mip/atlas/padding 보증이 아니다. | 새 지형 경계 정상 계산은 기존 외부 삼각형과 달라질 수 있음. 허용 변경 기준 미정이면 동일성 미증명 유지. |
| 5 | `inventory.editLayers` | source metadata 1개와 raw 45 presence·360 tile 이미 있음. A에서 legacy/edit-layer order 대응, class별 persistent/editable/brush 지원, layer별 target blend map을 추가한다. `GetEditLayersConst`/`GetLayersConst`와 reflected const fields. | nonempty layer 이행 검토가 필요. 단일 raw layer와 mixed composite를 별도 데이터로 유지. |
| 6 | `inventory.externalPackages` | on-disk 1,419+shared 39 증거가 있음. 현재 capture manifest와 package SHA를 연결하고 private/shared·incoming/outgoing plane을 표시한다. 새 조회가 필요하면 공식 AssetRegistry dependency/referencer API의 on-disk scope를 유지한다. | package graph 0을 live object/soft subobject/native 참조 0으로 바꾸지 않음. |
| 7 | `inventory.externalReferences` | v3 14종 unsupported와 bookkeeping edge 54개가 있음. B에서 각 occurrence의 owner/path/world/role와 lazy GUID→이미 로드된 target을 수집한다. | candidate 관련 미해결 occurrence를 모두 매핑하기 전 unknown=false 금지. |
| 8 | `inventory.foliage` | current foliage owner 0 관측. native InstanceBaseCache/FoliageInfos/Instances/BaseId/BaseComponent는 reflection 불완전. 현재 `LGT_SimpleGrass` dependency와 Landscape grass 설정은 별도 확인한다. | `no_loaded_owner_objects_observed`는 일반 foliage/grass 무의존 보증 아님. owner가 나타나면 typed Foliage 공개 API 범위의 별도 검토 필요. |
| 9 | `inventory.landscapeProperties` | origin/scale/63quads/1subsection·256 components는 기존 inspector 증거. A에서 proxy/main material/hole material/LayerInfo blend·grass/RVT/physical/LOD/Nanite/streaming 및 dimension 설정을 보완한다. | `CopySharedProperties`가 모든 actor/component/custom property를 옮긴다는 전제 금지. |
| 10 | `inventory.pcg` | PCGSubsystem 1개 실제 존재. B에서 subsystem/component/execution-source/graph/resource 역할을 나눠 수집한다. typed PCG module 범위가 별도 허용될 때 `GetAllRegisteredExecutionSources() const`의 이미 존재하는 집합을 읽는 설계를 검토한다. | subsystem 존재를 no-PCG로 바꾸지 않음. native tracking/cache/dynamic selection 한계 유지. |
| 11 | `inventory.splines` | current spline owner 0. all-desc/owner scan epoch 연결 후 public const `GetControlPoints`/`GetSegments` 및 spline layer class/actor GUID 관계를 기록한다. | owner 0만으로 다른 맵/자산의 spline 경로나 future migration 완료로 확대하지 않음. |
| 12 | `plan.candidateColdReloadValidated` | 새 destination save/reload 없음. 이후 허용된 별도 사본에서 저장 allowlist·fresh process/다른 맵 경유 재로드 후 v4+A/B/C/D를 다시 대조한다. | 현재 source A 재독/semantic 불변을 destination cold reload true로 쓰지 않음. |
| 13 | `plan.collisionReadbackValidated` | component/collision 256 등록·짝 검증은 있음. D의 existing collision height/material-index tile과 필요한 world trace baseline을 source/destination에서 대조한다. | count·등록 여부와 actual heightfield/query 검증을 분리. |
| 14 | `plan.layerGuidsRemainLiteral` | 현재 정책은 literal, source GUID 고정. Import의 DuplicateLayerAndMoveBrushes는 새 GUID. | 정책 충돌을 현재 hard blocker로 유지. 명시 검토 없이 mapping 허용으로 변경하거나 SetGuid(old)만 사용하지 않음. |
| 15 | `plan.no_unknown_external_references` | unsupported/native/dynamic plane 미완. B 결과를 candidate 관계 범위에 한정해 확인하고 noncandidate 제외 사유를 남긴다. | `unknownExternalReferences=false`는 빈 관측 수치가 아니라 남은 plane의 검토 결과가 있어야 함. |
| 16 | `plan.publicApiOnly` | 기존 collector는 public read 경로. 실제 rewrite 경로는 미확정. public Import/setter/actor lifecycle조차 identity·brush 이행을 자동 보증하지 않는다. | private EdMode/DeleteComponents·GUI 입력 우회·Collapse 경로 제외. collector public 사용과 rewrite 경로 승인 분리. |
| 17 | `plan.rawBASetterPass` | raw B/A 실제 바이트와 codec 근거는 있음. 후속 별도 사본에서 public SetHeightData의 alphaMask+flags 함께 쓰는 경로 또는 packed input 경로를 검토하고 mip/update/재합성/재로드 뒤 모든 B/A를 대조한다. | Import만으로는 이 gate를 충족하지 못함. setter 코드만 보고 실측 true 금지. |
| 18 | `plan.renderCompositeReadbackValidated` | 현재 CPU Source 동일성만 있음. 새 destination에서 실제 렌더 가능 환경의 합성 완료·readback과 material/normal/paint 대조가 필요하다. | NullRHI IsUpToDate true, native 영어 테스트 pass 또는 RG 동일성으로 대체하지 않음. |
| 19 | `plan.sourceBrushPointersPassedToImport_must_be_false` | source brush 연결 0 관측이 있어도 향후 Import input ownership 계약은 별도다. 입력 FLandscapeLayer/brush가 신규 사본 소유인지 전체 경로와 GUID·world·owner를 검사하는 설계를 명시한다. | 원본의 layer view/brush 포인터를 그대로 넘기는 경로 금지. 아직 Import하지 않았다는 사실만으로 future contract 검증 완료라 쓰지 않음. |

## 후속 read-only API 설계 후보

### A. `InspectCandidateAuthoringMetadata()`

기존 helper 의미를 바꾸지 않는 새 함수 후보다. layer 원본값과 composite 값을 합치지 않고, 아래 네 집합을 별도 반환한다.

- main/proxy 17개 관계 및 actual layout/extent/actor transform, LayerInfo/material/grass/RVT/physics/LOD/Nanite·streaming 설정.
- 256 component별 section base→proxy와 `retained45/outside211` 역할, component material/hole/PerLOD override, collision mip/scale/size, 기타 실제 사용 설정. 45 retained의 전체 속성 대조에 필요한 키를 고정한다.
- layer GUID/name/class/order/alpha/visibility/locked·raw presence 및 target-layer blend 관계. `GetWeightmapLayerAllocationBlend`는 설치 헤더에 UE_INTERNAL 표시가 있어 새로운 collector에서 일반 공개 계약처럼 직접 호출하지 않고 reflected stored map을 읽는 설계를 우선한다.
- texture 실제 사용자 집합과 retained/outside 공유 여부. vendor/shared material이나 LayerInfo 자체는 변경하지 않는다.

`FProperty` public const read와 copied `TObjectPtr.GetPathName()`를 사용한다. resolved default material을 만들거나 asset을 로드하는 fallback getter 대신 stored value와 effective value의 차이를 명시한다. dynamic parameter·native/custom state를 다 읽지 못했으면 이름·scope를 unsupported로 남긴다. 결과는 migration-ready가 아니라 source configuration inventory다.

### B. `InspectCandidateReferenceOccurrences()`

전체 property 전수를 더 크게 반복하는 대신 v3의 14종 미지원 occurrence와 현재 candidate의 직접 관계를 먼저 좁혀 읽는다. `owner object path/class/package/world`, field/container path, transient/CDO/asset/subsystem/component 역할, stored identity, loaded target, unresolved 사유를 반환한다. v3의 14개 선언명을 단순히 whitelist하여 지우지 않는다.

특히 lazy GUID는 다음 공개 read-only 경로를 쓸 수 있다. `FLazyObjectProperty::GetPropertyValue`로 얻은 local copy에서 const `GetUniqueID()`를 읽고, 그 `FUniqueObjectGuid::ResolveObject()` 결과를 현재 loaded object와 대조한다. 설치 `LazyObjectPtr.cpp:62–65` 구현은 `GuidAnnotation.Find(*this)`뿐이며 package load를 하지 않는다. 원래 lazy pointer의 `Get()`/weak cache를 갱신할 필요가 없다. **GetOrCreateIDForObject는 GUID를 만들고 MarkPackageDirty할 수 있어 금지한다.** invalid stored GUID는 `observed_empty_guid`, valid하지만 대상이 없으면 `unresolved_not_loaded`로 구분한다. 빈 legacy field만으로 현재 actor/component 관계 전체가 없다고 결론내리지 않는다.

PCG의 경우 subsystem 1개를 별도 role로 유지한다. `GetLandscapeCache()`는 `GetPCGWorldActor()`를 거쳐 editor actor 생성이 가능한 경로이므로 read-only collector에 쓰지 않는다. 별도 module 범위가 허용되면 public `GetAllRegisteredExecutionSources() const`/tracking 집합의 const 구현을 추가 감사한 뒤 읽는다. 모듈 의존을 현재 동결 Build.cs에 추가하지 않는다. dynamic lookup·native ARO/custom Serialize/bytecode·미로드 package plane은 별도이며 이 작은 API로 전체 무의존을 선언하지 않는다.

### C. `ExportCandidateCompositeBorderData()`

v4의 Texture.Source read-only copy/cache와 strict raw-presence 정책을 재사용하는 설계다. composite `FGuid()`의 RG와 **composite B/A normal bytes**를 raw layer B/A와 다른 채널로 반환한다. 현재 ROI [315,630,882,945]와 바깥 halo, 실제 mip/atlas 크기·bias·padding·source ID·source bytes digest를 연결한다. 원본 lattice를 world 좌표로 읽고, 1section/63quads 조건을 유지한다.

1-ring 시작 조사 범위 [314,629,883,946]는 570×318=181,260 sample이다. native normal 계산은 인접 quad의 삼각형 법선을 사용하고 영역 가장자리에는 외부 삼각형 기여가 빠질 수 있음을 코드가 명시한다. 따라서 1-ring baseline이 있다고 모든 mip/LOD/새 crop edge의 렌더 동등성이 증명되는 것은 아니다. 필요한 mip별 footprint와 border padding은 별도 수집한다. 새 경계 정상 재계산으로 변하는 항목을 보존 허용 편차로 삼을지조차 아직 결정되지 않았다.

### D. `InspectCandidateCollisionReadback()`

설치 공개 `ULandscapeHeightfieldCollisionComponent::FillHeightTile(TArrayView<float>,Offset,Stride) const`와 `FillMaterialIndexTile(TArrayView<uint8>,Offset,Stride) const`는 기존 HeightfieldRef가 없으면 false를 반환하고 caller 배열에만 값을 복사한다. 전자는 component transform/Z scale을 반영한 world height를 만든다(`LandscapeCollision.cpp:2633–2694`). collision size/scale/mip를 실제 값으로 기록하고 buffer 크기·stride를 검증한다. 64×64라고 고정 추정하지 않는다.

실제 HeightfieldRef가 없으면 unavailable로 반환하며 collision을 생성·재등록·cook·rebuild하지 않는다. 필요하면 이미 존재하는 physics scene에 대한 public const `UWorld::LineTraceSingleByChannel`을 후속 설계로 사용해 보스 바닥/경로·seam·holes 및 physical material/normal을 확인한다. scene 또는 geometry를 얻기 위해 상태를 바꾸지 않는다. material-index 배열과 실제 physical-material 의미의 대응도 별도 기록한다. 이 source baseline만으로 future destination collision gate를 true로 바꾸지 않는다.

## 보존 정책과 검증기의 한계

단일 EditLayer export와 mixed composite export는 지금처럼 분리한다. `CollapseAllLayers`에 해당하는 공개 `CollapseAllEditLayers()`는 층을 base로 합치고 brush 연결을 제거하며 나머지 layer를 삭제하는 경로다. 설치 `LandscapeEditLayers.cpp:8222–8233`과 editor UI 설명에도 brush·hidden layer data 제거가 드러난다. **원본·source A·사용자 보존층에서 Collapse/flatten을 사용하지 않는다.** GUI 전용 도구를 private 함수 호출로 우회하지도 않는다.

QA checker는 coverage를 `complete + items:list + evidence` 형태로 받고 nonempty 항목에는 새 `review_nonempty_*_migration` pending을 만든다. destination이 채워져도 explicit identity mapping/cold readback의 독립 검토를 pending으로 남긴다. fullFidelity/execution authorization도 false로 고정한다. 따라서 19를 단순히 0으로 낮추는 것이 목표가 아니며, items를 빈 배열로 위조하거나 검사기 조건을 완화하지 않는다. source metadata를 더 수집하면 pending 이름이 migration review로 바뀔 수 있다.

현재 가장 작은 다음 산출물은 **E0의 증거 연결표와 A/B의 source-only 수집 설계**다. C/D는 필요한 빠진 source baseline이다. literal GUID·실제 복원·destination 렌더/충돌/저장 재로드는 그 이후의 별도 gate다. height-only roundtrip 또는 현재 source CPU bytes 동일성을 full editable 보존으로 표현하지 않는다. gameplay의 3인칭·피격 취소·면역 보류와 사용자 직접 편집 가능한 Landscape/Actors 보존 범위는 바꾸지 않는다.

## 로컬 UE5.8 API 근거

모두 설치 엔진 `D:/Program Files/UE_5.8/Engine/`를 읽기만 했다. private cpp는 동작을 감사하기 위한 근거이며 private header/API 호출 제안이 아니다.

- `Source/Runtime/Landscape/Classes/LandscapeProxy.h:1421`, `Private/LandscapeEdit.cpp:3156,3764,3810` — blank Import·layer duplicate/GUID mapping·raw setter 입력 누락.
- `Source/Runtime/Landscape/Private/LandscapeEditLayers.cpp:8834,9174` — 새 GUID·brush owner/level 이동. `Classes/Landscape.h:451,506,529` — const layers·CollapseAllEditLayers·brush getters.
- `Source/Runtime/Landscape/Private/LandscapeEditInterface.cpp:264,283–314,419–424,846–855` — normal 계산의 영역 가장자리 한계, BA write/read packing. `LandscapeEdit.cpp:3265` 부근 — Import normal 생성.
- `Source/Runtime/Landscape/Classes/LandscapeComponent.h:456,459,474,583,621,625,828`; `LandscapeProxy.h:847,851,1135`; `LandscapeLayerInfoObject.h:116,126`; `LandscapeEditLayer.h:249` — 저장된 override/설정·public getters·UE_INTERNAL 구분.
- `Source/Runtime/CoreUObject/Public/UObject/UnrealType.h:2895,3319`; `Public/UObject/PersistentObjectPtr.h:104`; `Public/UObject/LazyObjectPtr.h:70`; `Private/UObject/LazyObjectPtr.cpp:62–65,90–107` — copied pointer/GUID read와 mutating ID 생성 구분.
- `Source/Runtime/Landscape/Classes/LandscapeHeightfieldCollisionComponent.h:356–365`; `Private/LandscapeCollision.cpp:2633–2694`; `Source/Runtime/Engine/Classes/Engine/World.h:2161` — existing heightfield readback·physics query 후보.
- `Plugins/PCG/Source/PCG/Public/Subsystems/PCGSubsystem.h:132,157,250`; `Private/Subsystems/PCGSubsystem.cpp:431–445,715–718` — query와 actor 생성 경로 구분. 기존 public `PCGSubsystem.h`는 새 Subsystems 헤더를 include하는 wrapper다.
- `Source/Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_EditLayers.cpp:225,457` 및 Runtime `LandscapeEditLayers.cpp:8222–8233` — Collapse의 파괴적 저작 상태 변경 근거. 해당 UI/실행 경로는 사용 금지.

새 파일은 이 문서 하나뿐이다. 실제 수집기·복원기 구현, source/candidate 자산 변경, 실행 권한 승격은 하지 않았다.
