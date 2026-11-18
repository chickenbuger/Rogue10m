# 공개 Landscape 데이터 API로 축소 후보 재작성 가능성 검토

2026-11-13. **기술검토만 완료 / 구현·UE·UBT·MCP·Game 실행 없음.** Computer Use/CUA/화면·마우스·키보드·브라우저·데스크톱 GUI 자동화와 다른 API를 이용한 GUI 입력 우회는 모두 금지한다. 이 문서는 네이티브 지형 데이터 API의 실제 동작을 설치된 UE5.8 소스에서 읽은 결과다. 기존 Source, 동결 helper, 맵, 자산을 수정하지 않았다.

## 판단

**공개 ALandscapeProxy::Import로9×5 또는8×5의 새 Landscape 데이터를 구성하는 것은 가능하다. 그러나 기존 Landscape 액터 전체를 제거하고 Import하는 것만으로 raw EditLayer·브러시·페인트·재질·PCG·참조가 모두 보존되지는 않는다. 일반적인 자동 교체는 현재 안전하다고 판단할 수 없다.**

현재 허용할 수 있는 다음 단계는 원본과 현재 독립 후보를 유지한 채, 별도 신규 후보에서 단순 수동 편집층의 데이터 재구성 가능성을 검증하는 실험의 설계다. 실제 실행에 앞서 현재 레이어/브러시/스플라인/foliage/PCG/직접 참조 인벤토리를 확보해야 한다. 과거 첫 저작에는 수동 장식과1개 EditLayer 기록이 있지만, 현재 사용자 편집 상태가 그 기록과 같다는 근거로 사용할 수 없다.

- 조건부 검토 가능: 지원되는 수동 persistent layer, 모든 raw 입력 확보, 브러시·스플라인·foliage·PCG 의존과 Landscape/component 직접 참조가 없거나 완전히 식별·매핑됨, 재질/물리/컴포넌트 설정 이전이 명시됨.
- 현재 보존 인증 불가: 미확인 procedural/custom layer, 원본을 가리키는 brush, 누락 raw layer, 의미 있는 raw B/A 미복원, 미확인 PCG/foliage/spline/외부 참조, 레이어 GUID의 문자 그대로 동일성 요구.
- GUI 금지로 보류한 Manage/Delete 도구를 private EdMode 호출이나 입력 에뮬레이션으로 대신하는 안은 검토·구현 대상이 아니다.

## 실제 공개 API와 한계

| API | 설치 소스 근거 | 가능한 일 | 자동으로 하지 않는 일 |
| --- | --- | --- | --- |
| ALandscapeProxy::Import | LandscapeProxy.h:1421 / LandscapeEdit.cpp:3156 | 별도 X/Y extent, per-GUID height/paint 배열로 새 지형 생성 | 기존 지형 잘라내기, 임의 참조 이행, 전체 UObject identity 보존 |
| ALandscape::GetLayersConst | Landscape.h:451 | 레거시 FLandscapeLayer/현재 EditLayer·brush 연결 읽기 | 독립적인 깊은 백업 |
| DuplicateLayerAndMoveBrushes | Landscape.h:422 / LandscapeEditLayers.cpp:8834 | EditLayer 객체 복제와 brush 소유 이전 | 기존 GUID 유지, 원본 brush 독립 복제 |
| CopySharedProperties | LandscapeProxy.h:1277 / Landscape.cpp:5241 부근 | LandscapeInherited/Overridable로 분류한 공유 설정 복사 | 모든 actor/component/custom property와 참조 복사 |
| ULandscapeSubsystem::ChangeGridSize | LandscapeSubsystem.cpp의 동명 함수 | 새 LandscapeInfo의 지형을 WP proxy grid로 분할 | 임의 ROI crop |
| FLandscapeConfigHelper::ChangeConfiguration | 공개 LandscapeConfigHelper.h / cpp:461 | subsection/section/grid 크기 변경 | 임의9×5/8×5 extent 선택 |
| FLandscapeEditDataInterface::SetHeightData/SetAlphaData | LandscapeEdit.h / LandscapeEditInterface.cpp:264,2106 | 후보의 지정 층 데이터 쓰기 | 원본 읽기 전용 보존, 모든 사용자 저작 관계 이행 |
| UWorld의 actor spawn/destroy lifecycle | 공개 Engine actor/world API / Landscape.cpp:3309,5104 | 별도 후보에서 전체 terrain actor 수명 관리 | 기존 actor/component를 참조하던 객체의 의미상 재연결 |

Import는 InGuid 유효성, height/paint map의 key 개수 일치, 비-template 객체와 **LandscapeComponents.Num()==0**을 check한다. 반환형도 void다. 기존256컴포넌트에 재호출하는 crop 함수가 아니며, 실패 전 부분 생성/수정이 있었는지를 별도로 확인해야 한다. 새 actor만 대상으로 하고, 기존 생성물을 자동 삭제·재Import하는 가드는 풀지 않는다.

FLandscapeConfigChange에는 임의 extent/9×5 수를 지정하는 인자가 없다. 기존과 같은1section·63quads·grid4면 ChangeConfiguration은 바로 no-op으로 반환한다. Clip 열거형만 보고 원하는 ROI를 자를 수 있다고 해석하면 안 된다. XYtoComponentMap이나 bounds를 임시로 속여 해당 함수를 crop 도구처럼 쓰는 우회도 제외한다.

## 정확한 좌표와 입력 데이터

원본 간격은1250/7cm, Z scale256, actor origin=(-90000,-90000,0), field anchor=(15000,32550)cm다. 세계 앵커와 중앙300m, 건물·보스 바닥·길·표식 액터의 월드 변환을 축소 수단으로 바꾸지 않는다.

| 후보 | 원본에서 읽을 inclusive samples | Import sample coordinates | 새 Landscape actor origin(cm) | 표본·높이 바이트 |
| --- | --- | --- | --- | --- |
| 권장9×5·원본 section 유지 | X315..882 / Y630..945 | **Min(315,630), Max(882,945)** | **(-90000,-90000,0)** |568×316 /358,976B |
| 9×5·0기준 재구성 | 동일 | Min(0,0), Max(567,315) |(-33750,22500,0) |568×316 /358,976B |
| 8×5·재포장 비교 | X351..855 / Y638..953 | **Min(0,0), Max(504,315)** |(-27321.428571,23928.571429,0) |505×316 /319,160B |
| 8×5·원본 component 경계 | X378..882 / Y630..945 | Min(378,630), Max(882,945) |(-90000,-90000,0) |505×316 /319,160B |

9×5 첫 안이 actor origin·기존 section-base와 전역 XY를 가장 직접적으로 유지한다. 남는 실제 지형 bounds는 X[-33750,67500],Y[22500,78750]cm다. 최종장 중심(15000,68000), 반경5500cm의 북쪽 끝73500cm까지 포함하며 경계 여유52.5m가 남는다.

8×5 재포장 입력의351/638을 그대로63quads section base에 쓰면 안 된다. GetHeight/SetHeight와 PCG의 좌표 계산은 component index×63을 사용한다. 해당 표본은 source window의 시작 인덱스이며, 새 component lattice는0기준으로 두고 actor origin에 source window 위치를 반영한다. 이 안은 원본 actor origin과 component 좌표까지 같게 보존하는 안이 아니다. 원본 경계8×5는 BM18m pad 서쪽 여유7m 등의 문제로 권장하지 않는다.

height 배열은 X가 빠른 행 순서, uint16의 실제 height 값이다. Z=0+((raw-32768)/128)×256cm를 유지한다. 각 paint target은 같은 직사각형의 uint8 배열이며,9×5 기준179,488B다. 전체 텍스처 파일/atlas hash가 아닌 world sample 값의 canonical hash로 비교한다.

Import 입력 map에는 **FGuid() 최종 합성 데이터 key**가 반드시 있어야 한다. 코드가 FindChecked(FGuid())로 초기 height/paint를 읽는다. 각 old EditLayer GUID에 해당하는 raw height map과 FLandscapeImportLayerInfo 배열도 추가하고, height/paint 두 map의 key 집합과 각 배열 길이를 사전에 검증한다. paint의 LayerInfo 객체·이름·blend method와 visibility target을 유지해야 한다. 과거 terrain-height.r16 또는 height_at 재생성 함수는 최신 사용자 편집 데이터의 대체 입력이 아니다.

## EditLayer와 brush의 보존 위험

Import의 InImportLayers 경로는 다음 순서다(LandscapeEdit.cpp:3764~3830).

1. 각 FLandscapeLayer를 DuplicateLayerAndMoveBrushes로 복제한다.
2. OldLayer.EditLayer GUID를 source key로, 새 EditLayer GUID를 destination key로 매핑한다.
3. 새 destination layer에서 SetHeightData/SetAlphaData를 호출한다.
4. resource/update를 요청하고 최종 합성을 다시 계산한다.

DuplicateLayerAndMoveBrushes는 EditLayer 객체를 DuplicateObject하고 **SetGuid(FGuid::NewGuid())**를 호출한다. 이름·class·alpha·visible·locked 같은 UObject 설정의 복제 경로는 있지만 GUID identity는 달라진다. 레이어 상한 등으로 일부 복제가 실패할 수 있으므로 layer count/order/class/매핑을 결과에서 다시 검증해야 한다.

FLandscapeLayer의 Brushes 배열은 기존 brush actor 포인터를 복사한다. 이후 FLandscapeLayerBrush::SetOwner는 다른 Level이면 brush를 Rename하고 SetOwningLandscape를 호출한다(LandscapeEditLayers.cpp:9174). SetOwningLandscape는 이전/새 Landscape에 변경 통지를 한다(LandscapeBlueprintBrushBase.cpp:668). **원본 source의 GetLayersConst()를 새 월드 Import에 그대로 넘기면 원본 brush가 이동/재소유될 수 있다.**

따라서 가능한 실험에서도 입력 layer/brush는 신규 후보에 이미 독립 복제된 소유 객체여야 한다. 데이터 배열을 만들기 전에도 brush world/level/package/owner/연결이 후보에 속하는지 검사한다. layer 구조의 얕은 복사만 보관하고 그 outer인 기존 Landscape를 제거한 뒤 재사용하는 수명 관리는 안전하다고 전제할 수 없다. 새로운 owner 아래의 독립 staging 객체와 raw 배열을 확보하고, GC/참조/델리게이트를 검증하는 별도 설계가 필요하다.

brush는 Initialize에서 Landscape transform/size/render-target size를 받는다. 축소 뒤 이 값들이 달라지면 같은 brush 설정이라도 출력이 바뀔 수 있다. 특히 월드가 아닌 normalized UV/전체 bounds 기반 brush와 procedural/custom layer는 잘린 사각형 안에서도 예전 결과를 재현한다고 보장할 수 없다. AActor::GetOwner와 GetOwningLandscape도 별개로 대조해야 한다.

### RG 높이만으로 raw 층 전체를 증명할 수 없음

최종 합성 height texture의 B/A는 법선이다. 그러나 raw height 편집층을 읽는 GetHeightMapAlphaBlendData/FlagsData는 B/A를 alpha-blend·flags로 해석한다(LandscapeEditInterface.cpp:846,852).

현재 Import의 raw-layer SetHeightData 호출은 RG 높이만 제공하고 InHeightAlphaBlendData/InHeightFlagsData를 전달하지 않는다. public SetHeightData에는 이 추가 입력을 받는 경로가 있지만, Import만 호출하면 의미 있는 raw B/A가 자동 복원됐다고 판단할 수 없다. 수동 층이 기본 alpha/flags만 쓰는지 인벤토리로 확인하거나, 추가 raw 채널 복원과 canonical 검증을 별도 설계해야 한다.

기존 v2 Snapshot은 의도적으로 RG/weight만 해시한다. **v2 통과는 raw B/A·전체 brush 저작 상태까지 보존됐다는 인증이 아니다.** 특히 새 Import 검증에는 이 범위 차이를 기록해야 한다. 동결 v2를 이 검토에서 수정하지 않았다.

### GUID 비교 정책

Import 후에는 정상적으로 old→new GUID 매핑이 생긴다. 기존 v2 metadata_digest/semantic_snapshot_digest에는 GUID가 포함돼 있어 새 Import 이후 그 값은 달라질 수 있다. 이를 오류 없이 통과시키려고 GUID 비교를 몰래 제거하거나 재Import 후 SetGuid(old)만 호출하면 안 된다.

SetGuid는 값과 변경 이벤트를 갱신할 뿐 이미 생성한 component LayersData key와 임의 외부 GUID 참조를 전부 재매핑하는 함수가 아니다(LandscapeEditLayer.cpp:106). 바뀐 identity를 허용하는 실험에서는 명시적인1:1 매핑 기록과 class/name/order/alpha/visibility/locked/brush 연결 동등성 검증, 매핑 정규화된 raw 데이터 비교가 별도로 필요하다. 문자 그대로의 GUID 유지가 필수라면 이 Import 경로는 부적합하다.

## Landscape lifecycle·재질·PCG·참조

전체 actor 교체는 native 데이터 수명 관리이며 GUI 입력 에뮬레이션과 구분할 수 있다. 다만 현재 작업은 구현 허가가 아닌 기술검토다. 실제 수명 관리 코드를 여기서 작성하거나 실행하지 않는다.

LandscapeProxy의 UnregisterAllComponents는 LandscapeInfo와 LandscapeSubsystem에서 actor를 해제한다(Landscape.cpp:3309). Destroyed는 LandscapeInfo 재구성과 Nanite 무효화 등을 수행한다(:5104). 이는 전체 actor의 정식 lifecycle이며, 일부 component를 단순 DestroyComponent하여 native crop 정리를 흉내 내는 것과 다르다. 그러나 새 actor/component의 UObject identity와 외부 package 경로는 기존 객체와 같지 않다.

새 actor에는 유효한 새 Landscape GUID를 사용해 살아 있는 old actor/proxy 및 WP actor-partition grid와 충돌시키지 않는 편이 타당하다. 기존 GUID 재사용은 old descriptor/cache/Info가 완전히 해제됐다는 증거 없이 시도하지 않는다. label·world XY·Landscape GUID만 맞춘다고 hard/weak/soft/component 참조가 모두 다시 연결되지 않는다.

CopySharedProperties는 LandscapeInherited/Overridable로 분류한 속성을 복사한다. 재질·hole material·물리 재질·RVT/grass·collision/LOD/Nanite 등 실제 사용 설정을 비교하고, per-component material/hole override나 custom property는 별도 world-coordinate 매핑으로 이전해야 한다. vendor나 공용 material/LayerInfo 자산 자체를 수정하지 않는다.

foliage의 base collision component, Landscape spline ownership/연결, HLOD·Nanite·NavMesh·water/brush 및 임의 gameplay actor의 terrain/component 참조는 Import가 자동 이전하는 계약이 아니다. ChangeConfiguration은 별도 MoveFoliageToLandscape를 마지막에 호출하지만 Import에는 그 수송 경로가 없다. 알려지지 않은 참조가 남아 있으면 재작성 성공을 보존 완료로 보고하지 않는다.

PCG 소스에서도 독립적인 참조와 캐시가 확인된다.

- PCGLandscapeData.h는 Landscapes를 TSoftObjectPtr<ALandscapeProxy> 배열로 보유한다. Initialize는 첫 Landscape transform과 LandscapeInfo/cache를 저장한다.
- PCGLandscapeCache.h:191의 cache key는 LandscapeGuid+component coordinate+WorldKey다.
- PCG는 actor reference/component coordinate/paint/physical material도 결과 metadata로 만들 수 있다.
- 따라서 새 GUID,0기준 재포장, proxy/object 변경과 축소 bounds는 PCG 선택·캐시·seed·결과에 영향을 줄 수 있다. Import가 PCG graph/property selector/캐시/생성 자원을 자동 보존한다고 볼 수 없다.
- PCGLandscapeData는 겹치는 여러 Landscape를 지원한다고 가정하지 않는다. old/new terrain이 잠시 겹친 단계에서는 PCG 실행/자동 재생성을 허용하지 않고, 후보에서만 명시적으로 이행·검증해야 한다. 전역 PCG 캐시 삭제로 원본까지 바꾸는 안은 제외한다.

## 조건부 실험의 API 순서

아래는 후속 구현의 검토 순서이며 실행 스크립트가 아니다. 원본 또는 기존 검증 후보를 in-place로 교체하지 않는다.

1. 부모가 공식 MCP/API로 현재 저장/dirty·full load·레이어/brush/PCG/spline/foliage/참조 인벤토리를 읽는다. 원본 최신 데이터와 package manifest를 보존한다. source export가 없거나 현재 소유/저장 상태가 불명확하면 진행하지 않는다.
2. WorldPartitionRenameDuplicateBuilder의 기본 duplicate로 **또 다른 신규 후보**를 만든다. 대상 map·external actors/objects 경로가 모두 새 것이어야 하고 Rename 옵션은 사용하지 않는다. 원본 파일 SHA256 불변을 확인한다.
3. 신규 후보의 old terrain에서 fresh composite/raw height·paint·필요 raw alpha/flags와 layer metadata를 메모리에 확보한다. 공용 LayerInfo/material은 읽기 참조만 유지한다. 현재 Snapshot은 digest만 반환하므로 Import용 raw 배열 전달 기능은 별도 구현이 필요하다.
4. old/new GUID·actor/component·brush·spline·foliage·PCG 참조의 이행표와 사본 전용 lifecycle/실패 복구 경계를 먼저 검토한다. 원본 객체를 입력에 넘기는 경우, 미확인 참조나 지원하지 않는 layer가 있는 경우 중단한다.
5. 신규 후보에만 빈 ALandscape를 만들고 원점/scale/공유 설정을 준비한다. old terrain과 GUID가 충돌하지 않게 한다. old layer/brush의 outer 수명을 보존하는 staging을 마련한 뒤에만 old terrain lifecycle 변경을 고려한다. 자동 source 저장이나 범용 cleanup은 하지 않는다.
6. 검증한 배열과 복제본 소유 layer input으로 public Import를 호출한다.9×5는가능하면 nonzero original section coordinates를 유지한다. old→new layer GUID 매핑·부분 복제 실패·추가 기본 layer 여부를 즉시 확인한다.
7. 추가 raw alpha/flags와 필요한 paint/override 설정을 공개 데이터 setter로 후보에만 복원한다. 새 LandscapeInfo를 대상으로 공개 ChangeGridSize(4) 경로를 사용해 WP proxy를 만든다. 기존 WP terrain에 이 함수를 crop처럼 재호출하지 않는다.
8. old candidate terrain의 모든 actor/proxy/descriptor와 참조를 정식 lifecycle·명시 매핑으로 정리한다. 새 terrain이 완성되기 전 old를 무조건 삭제하는 단일 파괴 패스는 피한다. partial failure이면 소유한 신규 후보만 미완으로 남기고 보존 checkpoint에서 다시 검토한다.
9. 실제 합성과 collision/paint readback이 안정된 뒤 canonical ROI를 비교한다. raw data, composite height/paint, layer/brush metadata와 참조 매핑을 모두 검증한다. expected45/40 render+collision count, proxy 소유/streaming, 원점·경계, 기존 actor world transforms, 보스 바닥/중앙300m/29간선도 확인한다.
10. 후보 package allowlist만 저장하고 다른 맵을 거친 재로드·구독/PCG/의존 복구를 검증한다. source SHA 불변과 direct/reference graph 검증, 향후 편집이 실제로 동작하는 데이터 테스트를 통과하기 전 완료로 보고하지 않는다.

old/new를 한 월드에서 동시 보유하는 중간 단계, brush owner 이동, actor 제거와 GC, object/package remap은 아직 구현·검증하지 않은 부분이다. 위 순서만으로 안전성이 증명된 것은 아니다. 현재 단계에서 범용 재작성 도구를 바로 실행 가능한 것으로 승인하지 않는다.

## GUI 금지와 commandlet 경계

Import 내부에는 FScopedSlowTask::MakeDialog 호출이 있다. FSlowTask::MakeDialog는 **IsRunningCommandlet이면 dialog를 만들지 않는다**(Core/Private/Misc/SlowTask.cpp:231). 따라서 진짜 commandlet 데이터 처리 경로는 GUI 입력 우회 없이 검토할 수 있다. 창을 찾거나 버튼을 누르거나 GUI 전용 tool/mode를 코드로 조작하는 경로는 사용하지 않는다.

반대로 rendering 없는 commandlet에서 ALandscape::IsUpToDate()는 바로true를 반환한다(LandscapeEditLayers.cpp:7966). 이 값만으로 GPU 합성/브러시/CPU readback·collision이 끝났다고 인증하면 안 된다. procedural 합성에 렌더링이 필요하면 명시적인 commandlet rendering 또는 공식 Editor MCP의 실제 데이터 API 환경을 별도 검증해야 한다. GUI 입력이 필요한 단계는 보류하며 다른 입력 API로 우회하지 않는다.

## 현재 권고와 남은 판단 자료

현 시점의 권고는 **기존 독립 복제본과 데이터 스냅샷을 유지하고, 현재 실제 인벤토리가 단순 수동층 범위인지 먼저 확인하는 것**이다. 단순 case가 입증되면9×5 original section/global XY를 유지하는 새 후보의 data reconstruction 실험을 설계할 수 있다.8×5 재포장은 원점/component identity 변화와 좁은 여유 때문에 뒤로 둔다.

부모가 확보한 현재 기준 증거는 아래와 같다. 이 검토자는 JSON을 읽어 확인했으며 해당 빌드/commandlet을 실행하지 않았다.

- 경로: `Feature/doc/evidence/map-resume-20261113/TerrainSnapshotA/{terrain.json,retained-data.json,result.json}`. 부모 보고상 v2 정규 빌드 exit 0, 30.55초와 NullRHI commandlet exit 0 이후의 결과다.
- `terrain.json`: 대상 `/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A`, main Landscape 1, proxy 16, render component 256, collision component 256. 원본 lattice 계약 통과, retained 9×5 lattice는 아직 아님.
- 현재 EditLayer는 `/Script/Landscape.LandscapeEditLayer`의 `Layer` 하나이며 GUID는 `9C4488BD4196BCC81051B5A001047577`이다. persistent textures/editing tools 지원, visible true, locked false, height/weight alpha 각각 1이다. 이것만으로 brush·PCG·참조 부재까지 입증하지는 않는다.
- `retained-data.json`: BLAKE3 기준 요청 데이터 완전, sample 179,488, height 358,976B, paint target마다 179,488B. composite와 해당 raw layer의 RG 높이 및 두 paint target 값이 각각 일치한다. 두 target은 `__LANDSCAPE_VISIBILITY__`와 `Auto Layer`다.
- semantic digest: `623961bac1faadd4138952f15694b5c54ae2226f51afba9acd5b7fd5857a9e1c`. metadata digest: `6c10d91dca2d7995e6aae578e04e79ddc494c7a4ec632f1395fc9ea142f57017`. 이는 현재 대상의 저장된 CPU Source RG/paint 기준값이며 Import 후 GUID 매핑을 처리하지 않은 semantic digest의 단순 일치 요구에는 사용할 수 없다.
- 읽기 전후 texture source·metadata·selection·editing layer·dirty package 목록 불변이 기록돼 있다. `result.json`도 초기/최종 dirty map/content가 비어 있고, save/crop/selection 실행 false, GUI 입력 0을 기록한다. 부모의 별도 파일 manifest 검사에서는 원본 2,478개·후보 1,419개 SHA가 유지됐다고 보고됐다. 이 문서 검토자가 그 전체 manifest를 재해시한 것은 아니다.

이 기준값은 **현재 RG/paint를 바꾸지 않고 읽었다는 증거**다. B/A·GPU export·새 합성·brush 동작·gameplay 인증은 명시적으로 제외돼 있다. NullRHI에서 `composite_settled_after_read`가 true인 것 역시 새 지형을 Import한 뒤 합성을 검증한 결과로 확대 해석하지 않는다.

아직 필요한 준비는 현재 raw B/A의 의미·기본값 여부, brush/spline/foliage/PCG 및 직접 참조 인벤토리, old→new GUID와 객체 참조 이행표, fresh Import용 raw 배열 전달 설계다. 이 자료와 실제 합성·collision·재로드 검증이 없는 상태에서는 crop/재작성은 보류한다. 전체 layer/brush/PCG/actor reference 보존은 미검증이며, 이 검토에서는 source/helper 빌드와 새 지형 생성을 하지 않았다. 3인칭·피격 취소·면역 보류, 무기별 입력 지원과 Ctrl→첫Q 고정기한→QQQ 자동발사는 이 데이터 재작성 검토의 변경 범위가 아니다.

## 소스 근거

엔진 루트: D:/Program Files/UE_5.8/Engine

- Source/Runtime/Landscape/Classes/LandscapeProxy.h:1421 — 공개 Import; :1277 CopySharedProperties.
- Source/Runtime/Landscape/Private/LandscapeEdit.cpp:3156 — Import 본체; :3748 default layer; :3764 layer duplicate/GUID mapping; :3810 raw setters; :140 component init.
- Source/Runtime/Landscape/Classes/Landscape.h:422,451,529 — layer duplicate/read/brush read.
- Source/Runtime/Landscape/Private/LandscapeEditLayers.cpp:8834 — 새 GUID와 brush 재소유; :9174 brush owner; :7966 IsUpToDate.
- Source/Runtime/Landscape/Private/LandscapeEditLayer.cpp:106 — SetGuid 동작.
- Source/Runtime/Landscape/Private/LandscapeEditInterface.cpp:264,846,852,2106 — height/alpha/flags/paint setter·reader.
- Source/Runtime/Landscape/Public/LandscapeBlueprintBrushBase.h 및 Private/LandscapeBlueprintBrushBase.cpp:668 — brush initialization/owner.
- Source/Runtime/Landscape/Public/LandscapeConfigHelper.h 및 Private/LandscapeConfigHelper.cpp:461 — arbitrary ROI가 없는 설정 변경.
- Source/Runtime/Landscape/Private/Landscape.cpp:3309,5104,5241 — unregister/destroy/shared property.
- Plugins/PCG/Source/PCG/Public/Data/PCGLandscapeData.h, Private/Data/PCGLandscapeData.cpp, Public/Grid/PCGLandscapeCache.h:191 — landscape soft refs/transform/cache key.
- Source/Runtime/Core/Private/Misc/SlowTask.cpp:231 — commandlet dialog guard.

프로젝트 참조: Docs/WorldMapProductionGuide.md, Docs/References/WorldMap/README.md 및 두 JSON/PNG, Feature/architect/2026-11-12_mabinogi-map-density-plan.md, 이 작업의 geometry-audit와 동결 retained-snapshot v2. 기존 증거와 현재 실측을 구분한다.

