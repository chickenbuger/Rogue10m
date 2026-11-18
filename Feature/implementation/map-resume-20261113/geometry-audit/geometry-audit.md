# 월드맵 축소 기하·보존 API 격리 감사

2026-11-13. 원본/UE/UBT/MCP/게임 사용권은 부모 작업자만 보유한다. 이 검토는 문서·기존 증거·설치된 UE5.8 소스의 읽기와 순수 산술만 수행했다. Source/Config/Content/기존 Docs/Scripts를 수정하지 않았고, 원본 생성 스크립트도 실행하지 않았다. 이 폴더의 새 감사 문서와 JSON만 작성한다. 모델 또는 fast 설정을 검증했다는 주장은 하지 않는다.

## 결론과 정확한 입력

**현재 사용자 편집 보존을 우선하면 정식 WP 독립 복제 후 9×5 원본 컴포넌트만 남기는 안이 우선이다.** 900×562.5m는 비교안이며 반드시 맞출 크기로 취급하지 않는다. 8×5 원본 경계 crop은 서쪽 BM 평탄대 끝에 7m만 남고 채석 사냥터 장식의 보수적 범위도 자를 수 있다.

| 안 | 원본 샘플 inclusive X / Y | 크기·표본 | 실제 지형 XY 경계(cm) | 판정 |
| --- | --- | --- | --- | --- |
| 8×5 원본 경계 | 378..882 / 630..945 | 900×562.5m / 505×316 / 319,160B R16 | X -22500..67500, Y 22500..78750 | 서쪽 여유 부족 |
| 8×5 새 컴포넌트 재포장 | 351..855 / 638..953 | 900×562.5m / 505×316 / 319,160B R16 | X -27321.428571..62678.571429, Y 23928.571429..80178.571429 | 높이 샘플 비교 가능, 레이어·페인트 동등 보존은 별도 |
| **9×5 원본 보존** | **315..882 / 630..945** | **1012.5×562.5m / 568×316 / 358,976B R16** | **X -33750..67500, Y 22500..78750** | **권장 검토안** |

9×5에서 유지할 원본 컴포넌트 좌표는 X=5..13, Y=10..14다. 원본 section base는 X=315,378,...,819와 Y=630,693,...,882이며, 마지막 공유 표본은 각각 882와 945다. 유지 45개/삭제 211개는 원본이 여전히 256개일 때만 성립한다. section-base 판정은 X>=315 && X<882 && Y>=630 && Y<945, 각 축 간격63이다.

**복제본의 컴포넌트를 삭제하는 방식에서는 Landscape 액터 원점 (-90000,-90000,0), 스케일 (1250/7,1250/7,256)을 그대로 둔다.** (-33750,22500)은 남은 지형 경계의 최솟값이다. 액터를 이 지점으로 이동하면 기존 월드 앵커와 도로가 어긋난다. 새 Landscape를 0..567 / 0..315로 재Import할 때만 새 액터 원점을 (-33750,22500,0)으로 사용한다.

9×5 면적은 0.56953125㎢, 원본 대비 82.421875% 감소다. 8×5는 0.50625㎢/84.375% 감소다. 원본 샘플 간격은 정확히1250/7cm다. sample i의 좌표는 -90000+i*1250/7cm다. [crop-proposals.json](crop-proposals.json)에 원본 인덱스, 경계, 21노드, 29간선과 산술 결과를 함께 보존한다.

## 포함하는 공간과 계산의 한계

확보된 원본 변환은 worldXY=(15000+100*localX,32550+100*localY)cm다. S=(15000,32550), J100=(15000,42550), J180=(15000,50550), J240=(15000,56550), B=(15000,62550)로 중앙300m를 보존한다. 최종 보스 입구 B와 전투장/보스 위치는 다르다. 기존 전투장 중심은 (15000,68000), 반경5500cm, 평탄고도9600cm이며 북쪽 끝은73500cm다. 9×5 북쪽 경계78750cm까지52.5m 여유가 남는다. 기존 원형 지지 바닥은110m 지름/상면9602cm 기록이다.

큰 거점은 H/M/T, 초소는 P/Q/G/O/F다. H 수렵·회복/재료, M 상점·기초 수련/운송, Q 너클 수련/채석, T 기록·학습조건/유적, F 장비·제련 단서의 다섯 기능을 유지한다. G 대검과 O 마법 실습, P 후속 활 자리를 지우거나 큰 마을5곳으로 되돌리지 않는다. W/A/Q외곽/R/Z의 일반15종과 BM/BW/BE/BG 중간4곳, B 북쪽 최종1곳을 분리한다. 기존53개 표식과5개 보스 예약은 실제 NPC·획득·AI·전투 완성을 뜻하지 않는다.

보존된 평탄대15개, scene sites20개, 표식53개, 도로29개의19m shoulder+feather 상한, 장식 생성 코드의 최대 범위를 비교했다. 장식 nature 중심은 노드에서 최대7600cm, 큰 나무 XY 치수1020×1020cm의 임의 yaw에 따른 AABB 반폭은721.2489168cm다. 따라서 nature는 노드별8321.2489168cm 범위를 보수적으로 포함한다. 이 코드 상한과 보존된 site/marker/도로/pad를 합친 XY는 다음과 같다.

- X: -24821.2489168..60321.2489168cm. 극값은 W/Z의 nature 최대 범위다.
- Y: 30650..73500cm. 남쪽은 S의19m 도로 여유, 북쪽은 최종장55m 반경이다.
- 9×5 경계 여유: 서89.288m/동71.788m/남81.5m/북52.5m.
- 8×5 재포장 경계 여유: 서25.002m/동23.573m/남67.214m/북66.786m.
- 8×5 원본 경계는 위 보수적 nature:W/ring:Q 범위가 넘어간다. BM18m 평탄대 서쪽 여유도7m다.

이는 현재 로드된 액터 실측 AABB가 아니며, 미래 추적·공격·카메라·PCG 범위를 보증하지 않는다. 8×5에서는 이 코드 상한 전체에 양쪽30m 여유를 동시에 붙이면911.425m가 필요해900m에 들어가지 않는다. 부모가 현재 실제 액터/추적 범위를 다시 측정하기 전 8×5를 최종 크기로 승인할 근거는 부족하다. 기존 generation helper의 active box는 필수 공간 경계와 별개다.

모든29간선이 양방향이고 중앙/선택4개 보스 곁가지가 유지된다. 볼록한 직사각형 안에 중심선 끝점과19m 확장 경계가 들어가므로 기존 직선 도로의 평면 전체가 들어가는 것은 산술로 확인할 수 있다. 실제 충돌·전 길 보행·AI/안전 경계·3인칭 시야는 새 후보에서 별도 확인해야 한다.

## UE5.8의 독립 복제·삭제 API

설치된 엔진 소스를 직접 확인했다.

1. UnrealEd/Public/WorldPartition/WorldPartitionRenameDuplicateBuilder.h가 정식 builder를 제공한다. cpp:139~140의 입력은 NewPackage= 및 선택 Rename이다. 기본은 duplicate이며 Rename을 넣을 때만 원본 패키지를 삭제한다(cpp:447).
2. cpp:205는 **대상이 존재하면 먼저 DeleteExistingMapPackages를 호출한다.** 목표 패키지/umap/외부 액터·오브젝트 경로가 모두 신규인지 부모가 확인하고, source와destination이 다름을 하드가드해야 한다. 재실행으로 기존 후보를 덮어쓸 수 있다고 가정하면 안 된다.
3. cpp:230은 EDuplicateMode::World/StaticDuplicateObjectEx, cpp:296은 외부 액터 패키지의 의존 UObject 이동, 이후 soft path/object ref 보정과 패키지 저장을 수행한다. 원본의 디스크 저장본에서 복제하므로 현재 live unsaved 편집을 자동으로 포함하지 않는다. 원본 저장·복구 범위는 부모가 결정하고 기록한다.
4. cpp:152에서 actor-desc container가 여러 개면 source 외 container 액터를 복제하지 않는다는 경고가 있다. LevelInstance/추가 container/PCG/외부 공유 콘텐츠를 조사해야 한다. 일반 게임/재질/메시 등 별도 콘텐츠 자산은 참조로 남을 수 있어, 후보의 재질을 바꿀 때 원본 공용 자산을 수정하면 안 된다.
5. **LandscapeEditorUtils::DeleteComponents 공개 함수는 이 UE5.8에 없다.** LandscapeEditor/Public/LandscapeEditorUtils.h에는 SetHeightmapData/SetWeightmapData 등이 있다. 실제 삭제는 LandscapeEditor/Private/LandscapeEdMode.cpp:3505의 FEdModeLandscape::DeleteLandscapeComponents이며 선언도 Private/LandscapeEdMode.h:612다. 클래스에 LANDSCAPEEDITOR_API export가 없어 설치형 엔진의 게임모듈에서 안정적으로 직접 링크할 공개 API로 취급하지 않는다.
6. 실제 삭제 구현은 공유 heightmap을 SplitHeightmap으로 분리하고 붙은 foliage를 제거하며, 모든 EditLayer heightmap/weightmap/weightmap usage와 collision, neighbor render state를 정리한다. 단순 DestroyComponent/DestroyActor 반복으로 대체하지 않는다. 공개 RemoveOverlappingComponent는 MapCheck의 겹친 컴포넌트 제거용이며 이 전체 정리와 동등하지 않다.
7. 공개 연결점은 LandscapeInfo.h:277~280의 GetSelectedComponents/UpdateSelectedComponents/ClearSelectedRegion이다. UpdateSelectedComponents(...,true)는 기존 선택집합을 정확히 새 집합으로 대체한다(LandscapeEdit.cpp:6990,7020). target-only 신규 helper가 삭제211개를 선택하고 부모가 Landscape Manage/Delete 도구를 적용하는 경로를 검토할 수 있다.
8. LandscapeEdModeComponentTools.cpp:920~940은 선택집합이 있으면 브러시 범위를 쓰지 않고 선택 컴포넌트를 실제 삭제 함수에 전달한다. 도구/모드 전환 후 선택집합이 그대로인지, stroke Undo/Redo와 저장·재로드를 유지하는지는 부모의 후보 검증 항목이다. 이 감사에서 실행하지 않았다.
9. 원본4×4 proxy grid에서9×5 남은 컴포넌트는 proxy cell X1/2/3 × Y2/3의6칸에 걸친다. 칸별 보존 컴포넌트 수는 행별6/8/4 및9/12/6이다. 삭제 구현은 빈 proxy actor를 자동 정리한다고 보증하지 않는다. nonempty proxy6개와 빈 proxy 수를 구분하고, 빈 proxy를 지울 때도 target/소유/GUID/0컴포넌트 조건을 확인해야 한다.

신규 target 전용 helper를 만들 때도 기존 WorldLayout20261112/OpenRunMap 하드가드를 풀지 않는다. 공개 selection/read/export 보조 API만 추가하는 편이 private 엔진 코드 복사나 링크보다 범위를 제한하기 쉽다. Build.cs에는 현재 Landscape/UnrealEd/Json 의존성이 있으나 LandscapeEditor는 없다. 새 UCLASS/UFUNCTION을 추가하면 UHT/Editor 정규 빌드는 부모가 수행해야 하며 이 감사는 빌드나 live reflection을 인증하지 않는다.

## 높이 Import·레이어 보존 비교

ALandscapeProxy::Import는 Runtime/Landscape/Private/LandscapeEdit.cpp:3156에서 VertsX/Y, NumComponentsX/Y를 독립 계산한다. 63quads×1section에서 505×316과568×316은 각각40/45컴포넌트를 구성할 수 있다. API의 직사각형 지원과 현 프로젝트 wrapper의 지원은 다르다.

현 Rogue10mWorldLayoutAuthoring.cpp:32 이후는1009²·16×16·180000cm정사각형·16proxies·256components가 고정돼 있다. cpp:77의 정확한 project-root/map/Editor/noPIE/WP 가드, :110의 무관 dirty map 거절, :292의 유일한 canonical R16 경로, 기존 loaded landscape 및 unloaded descriptor 거절을 유지해야 한다. Material은 UMaterialInterface 파라미터와 IsValid를 검사하지만 특정 재질 경로까지 강제하지 않는다. AutoLayerInfo는 UObject 입력을 ULandscapeLayerInfoObject로 cast하고 정확한 패키지와 Auto Layer 이름을 검사한다.

SHA1은 cpp:327에서 계산해 결과에 기록하지만 승인된 digest와 대조해 거절하는 코드는 없다. 따라서 정확한 파일명/바이트 수가 승인 내용의 불변성을 증명하지 않는다. 향후 source export와 candidate input에 manifest SHA를 붙여 재읽기 시 검사해야 한다. TerrainFacts의 samples_per_axis 같은 일부 출력도 관측값이 아닌 컴파일 상수다.

현 Import는 새 기본 height map과 Auto Layer=255 paint를 만들고 빈 InImportLayers를 전달한다(:367~373). **새 R16만 Import하면 기존 paint, 여러 EditLayer의 순서·가중치·숨김·락·class/GUID, brush/spline/PCG의 저작 관계가 자동 보존되지 않는다.** 기존 write_heightmap도 고정 original 파일만 쓸 수 있고 source height_at은 과거 함수값을 다시 생성할 뿐 최신 사용자 지형을 읽지 않는다.

공개 FLandscapeEditDataInterface는 특정 edit-layer GUID constructor/SetEditLayer와 GetHeightDataFast/GetWeightDataFast를 제공한다(LandscapeEdit.h:128~194). ULandscapeInfo::ExportHeightmap/ExportLayer는 지정 region export가 가능하지만 기본 FLandscapeEditDataInterface(this)를 사용하므로 현재 선택 edit layer 문맥을 명확히 해야 한다. 이 구현의 ExportRegion.Max는 inclusive이고 Width()+1/Height()+1을 출력한다.

LandscapeEditorObject.h의 기본 옵션은 bExportEditLayer=true(선택 레이어), bExportSingleFile=false, ImportExportMode=LoadedOnly다. source export 계획에 selected/blended, full-world/loaded-only, per-layer/per-target-layer, visibility, Y flip과 단일/타일 옵션을 명시해야 한다. blended R16은 최종 높이의 비교에는 도움이 되지만 편집 레이어 구조 자체의 백업은 아니다. 완전 복제는 최신 저장된 UObject 저작 상태를 보존하는 기반에 더 적합하고, export는 원본/후보 ROI의 byte equality를 검증하는 보조 증거로 사용한다.

## 기존 저장본과 증거의 한계

- retained terrain-inspection은1 Landscape/16proxy/256render+collision/1EditLayer, origin/scale/bounds를 확인한 과거 저장·재로드 기록이다. 새로 live 확인한 결과가 아니다.
- scene-author는1336 spawn/1335 placements,53편집 표식, 기능 미완성을 명시한다. reserved-boss-sites는4중간+1최종 모두 편집용 예약이고 보스 AI 배치가 아니다.
- PhysicsA는3431검사 중20실패/29간선 검사 완료다. 기존 실패를 새로운 후보 통과로 재사용하지 않는다. 원본 exact-cell audit도 횡경사13.664°/합성15.095°가 남는다.
- terrain-height.r16은2,036,162B, 보관 SHA256 033a24440e126a2a989057933c3599e8e961a0b67b0535f37730a5a4dface300이다. 과거 보관 값이며 이번 live 사용자 편집과의 동등성 검증이 아니다.
- 이 감사의 디스크 열람 시 umap은14,204B/UTC 2026-11-11 17:43:29, external actors는1360파일/18,124,382B, external objects는58파일/118,250B다. 원본 manifest 전체 해시를 새로 작성하거나 바이너리를 파싱하지 않았다.
- Saved/Autosaves에는 RoadRepair 재질2개와 해당 candidate 외부 액터5개/외부 객체2개의 더 늦은18:07:41 UTC Auto2가 있다. 이들은 저장 원본과 별도의 복구 후보이며 자동 적용하거나 현재 편집 완료로 단정하지 않는다. live dirty/복구 상태의 책임자는 부모다.

## 부모가 실행하기 전/후 확인할 범위

1. 현재 선택된 포털·열린맵·source 저장/unsaved/복구 상태와 원본 사용권을 확인한다. 원본·외부 패키지·후보 전용 콘텐츠·레이어/brush/PCG 상태를 독립 물리 snapshot/manifest로 보존한다.
2. target package와 외부 경로가 모두 신규임을 확인한 다음 정식 WP duplicate를 수행한다. Rename은 사용하지 않는다. 원본 manifest 전후 동일성과 target 참조 소유를 확인한다.
3. source/target의 모든 지형을 실제 로드해 component section-base, transform, 256 count, 레이어/페인트/visibility를 확인한다. 계약이 달라졌으면 위 인덱스를 적용하지 말고 다시 계산한다.
4. target9×5 keep45/delete211 선택 목록을 먼저 출력한다. 부모가 Editor Manage/Delete 경로로 적용한 뒤 retained samples와 모든 layer/weight를 원본 ROI와 비교한다. 빈 proxy와 외부 패키지까지 저장·재로드한다.
5. 지형 sculpt/paint/EditLayer와 건물/길/표식의 Details·기즈모 수정을 Undo/Redo, 저장·재로드로 확인한다. 런 진입/맵 재열기에 자동 재생성으로 사용자 편집이 덮어써지지 않아야 한다.
6. 중앙300m/29간선 양방향·실제 지형충돌·안전/추적·최종장 남쪽 접근·3인칭 카메라를 검증한다. 기존48.537초나 산술50초를 새 후보 실측으로 바꾸어 쓰지 않는다.
7. 15일반/4중간/1최종 분리와 H/M/Q/T/F 기능,3큰거점+5초소, 책 획득/학습/장착 분리, 중상급 탐험 발견을 유지한다. Ctrl→첫Q 고정기한→QQQ 자동발사, 무기별 지원 입력,3인칭·피격 취소를 보존하고 면역은 결정 전 보류한다.
8. 이 감사 자체를 게임플레이/패키지/완성 기능 통과로 기록하지 않는다. 부모가 새 파일2개를 DevLog/Sprint 기록에 반영하고 해당 검증 결과/잔여 blocker를 구분한다.

## 읽은 핵심 근거

프로젝트: Docs/WorldMapProductionGuide.md, Docs/References/WorldMap/README.md, world-layout-20261112.json, acquisition-catalog-20261112.json, retained rogue10m-world-map-concept-20261112.png, Feature/architect/2026-11-12_mabinogi-map-density-plan.md, Feature/doc/2026-11-12_landscape-world-blockout.md, Scripts/WorldLayoutTerrain20261112.py, Scripts/Editor/AuthorWorldLayoutScene20261112.py, ManageWorldLayout20261112.py, Source/Rogue10m/Editor/Rogue10mWorldLayoutAuthoring.h/.cpp 및 위에 명시한 기존 evidence JSON.

엔진: D:/Program Files/UE_5.8/Engine/Source 아래 WorldPartitionRenameDuplicateBuilder.h/.cpp, LandscapeEditorUtils.h, LandscapeEdMode.h/.cpp, LandscapeEdModeComponentTools.cpp, LandscapeEditorObject.h, LandscapeInfo.h, LandscapeProxy.h, LandscapeEdit.h/.cpp, LandscapeConfigHelper.h/.cpp, LandscapeSubsystem.cpp, LandscapeToolInterface.h.

