# v3 동결 소스 독립 읽기 검토

2026-11-13. 검토 대상:

- `terrain-dependency-v3/Rogue10mCompactWorldAuthoring.h`: SHA256 `960588DDC503C73393475AEAFC46757E9AADAD4DA76B7049FAE6B72F7F75EF73`.
- `terrain-dependency-v3/Rogue10mCompactWorldAuthoring.cpp`: SHA256 `1ECC82DFD010D8D6A01276DDAEC30CCCC409B18691DBC94F879477CABF0F3185`, 94,260 bytes.
- 현재 공유 v2 CPP: `3A730A23AF3A92BBD569034702D6394F576E929D9C933505333D51D1A40FAC19`를 기준으로 새 namespace와 export를 읽었다. 검토 도중의 이전 `9D071...` 버전이 아니라 최종 동결 해시에 대한 결과다.

## 정정: descriptor getter는 strict passive 관측에 쓰지 않는다

앞선 `2026-11-13_readonly-terrain-dependency-api-review.md`의 `GetActor(false,false)/IsLoaded(false)` 사용 제안은 strict passive 관측에 부적합하여 철회한다. 로컬 UE5.8 `Runtime/Engine/Private/WorldPartition/WorldPartitionActorDescInstance.cpp:61–70`에서 GetActor는 `FlushAsyncLoad()`를 호출한다. IsLoaded는 같은 파일 46–58행에서 FindObject로 cached ActorPtr를 갱신할 수 있다. IsLoaded 자체가 FlushAsyncLoad를 호출한다는 뜻은 아니다.

최종 v3는 이 getter들을 호출하지 않고 `TActorIterator<AActor>`로 이미 열거한 candidate actor의 GUID 및 정확한 full object path를 descriptor의 GUID/soft path와 함께 비교한다. 누락 path/count를 따로 기록하므로 이 방향이 적절하다. nested/unregistered container 또는 미로드 package 전체의 완전성 증거로 확대하지 않는다.

## 결과

선언된 읽기 전용 범위에서 Critical/Medium 문제를 발견하지 못했다. UHT/컴파일·실행은 수행하지 않았으므로 빌드 또는 실제 무변경 통과로 표현하지 않는다.

| 검토 지점 | 확인 내용 |
|---|---|
| API/모듈 | UTransactor, FScriptInterface, object/soft/weak/delegate/container property, FProperty::ContainsObjectReference, Landscape legacy layer/brush API를 설치된 UE5.8 선언과 대조했다. 새 PCG/Foliage/AssetRegistry typed 링크를 추가하지 않는다. |
| Interface no-resolve | FScriptInterface를 복사하고 복사본 GetObjectRef의 TObjectPtr에서 GetPathName/IsResolved만 사용한다. GetObject의 implicit handle resolve를 피한다. |
| Brush no-resolve | legacy layer와 edit-layer path가 같은지 확인하고, BlueprintBrush의 copied handle을 먼저 검사한 뒤 이미 resolved인 경우에만 기존 brush getter를 사용한다. unresolved는 기록하고 오류로 남긴다. |
| Raw BA | raw GetLayerData 존재와 persistent layer 여부를 먼저 확인한다. v2의 validated mip copy·격자를 재사용하고 64×64 block/retained index를 검사한다. B high/A low, alpha mask0xFFFC, flags A&3, 179488 unique/4832 seam duplicates가 일치한다. native EHeightmapAlphaFlags의0/1/2/3 해석과 대조했다. 임의 기본값으로 데이터를 보충하지 않는다. |
| Reflection walk | Soft를 hard보다 먼저 처리하고 전체 path를 보존한다. Weak/delegate는 loaded receiver 관측, Lazy는 unsupported다. ArrayDim과 nested array/map/set/struct를 처리하고 sparse container validity를 검사한다. opaque storage와 budget/depth/row truncation은 완료로 바꾸지 않는다. |
| 상태 보존 | dirty, UObject identity/flags, editor selections, transaction queue/active 상태, texture source ID/dirty/transactional, layer metadata 및 Landscape 선택/편집 레이어를 전후 비교한다. 이 항목들은 package byte 동일성이나 transaction buffer 전체 byte 동일성 증거가 아니다. |
| Game build | 새 InspectCandidateDependencies export는 WITH_EDITOR로 구현을 제한하고 비에디터 FString 실패 stub을 제공한다. 기존 UCLASS/generated-header 계약을 유지한다. |
| 안전 판정 | preservation_ready, project_wide_dependency_complete, crop_reference_safety_verified는 false로 고정된다. loaded owner scan 또는 raw BA complete를 crop 승인으로 사용하지 않는다. |

## 반드시 유지할 결과 해석

- `success`는 선언한 inspection 수행 결과다. 모든 세계/asset/native 참조 또는 crop 안전의 통과가 아니다.
- `loaded_reflection_walk_complete`도 선언된 loaded UPROPERTY walk 범위다. 미해결 object handle은 path와 별도 count를 제공하며, native ARO/custom Serialize/bytecode/CDO/archetype/on-disk graph는 제외되어 있다.
- `loaded_candidate_dependency_owner_scan_complete`는 현재 process-loaded candidate 소유 객체 범주 열거다. full actor descriptor가 미로드이면 전체 세계의 소유 객체 없음으로 바꿀 수 없다.
- foliage native base/PCG dynamic tracking/spline native 내부는 구현에서 제한을 명시한다. owner0은 loaded 후보의 관측0이며 자산·다른 맵·미로드 객체·native registry의 의존성 없음이 아니다.
- raw BA 또는 참조 행의 해시는 관측 payload의 digest다. 디스크 저장, crop, 복구, import/export 또는 runtime 품질을 증명하지 않는다.
- 기존 helper의 selection export가 파일에 남아 있는 사실은 GUI 금지 상태에서 실행할 권한이 아니다. 이번 신규 inspection의 상태 조회는 선택 변경/GUI 입력을 수행하지 않는다.

## 검토 경계

Source/helper/staged/Config/Content 변경0, UE/UBT/MCP/Game/GUI 입력·우회0, 프로세스 조회·종료0. 이 신규 리뷰 문서만 작성했다. 설계/API 정정과 최종 동결본 결과를 root 및 geometry 담당에게 전달했다. 실제 build/read-only 실행의 소유권과 검증은 root에 있다.
