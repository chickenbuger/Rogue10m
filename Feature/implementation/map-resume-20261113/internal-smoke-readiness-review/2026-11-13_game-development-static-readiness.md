# Game Win64 Development 정적 사전검토

검토 시각: 2026-11-13 UTC. 대상은 현재 통합 Source의 helper v2, G08/VFX, VillageQuest/NPC/Tuning 및 관련 tests/Build.cs이다. **확인된 무가드 UnrealEd/Landscape 링크 누수는 없다. 실제 Game 컴파일·링크·실행 통과 판정은 아니다.**

Source/Config/Content/Git와 빌드 설정은 수정하지 않았다. UE/UBT/UAT/Cook/Game/MCP/GUI 실행은 모두 0이다. 외부 Windows Work의 내부 Development smoke runner를 구현하거나 중복하지 않았다. 작성 중인 staged v3 helper는 읽거나 수정하지 않았다. 새 파일은 이 검토 폴더의 문서와 `static-evidence.json`뿐이다.

## 타깃·모듈 경계

- `Source/Rogue10m.Target.cs:11–14`는 Game 타깃이며 `ExtraModuleNames`에 Rogue10m만 추가한다. `Source/Rogue10mEditor.Target.cs:11–15`의 Rogue10mEditor는 별도 Editor 타깃이다. `Rogue10m.uproject`도 두 모듈을 각각 Runtime/Editor로 선언한다.
- `Source/Rogue10m/Rogue10m.Build.cs:31`의 PhysicsCore/Chaos는 runtime 의존성이다. `:32–35`의 MeshDescription/SkeletalMeshDescription/UnrealEd/Landscape/Json/AnimGraph/BlueprintGraph/KismetCompiler는 `Target.bBuildEditor` 안에 있다. 현재 신규 runtime 코드 때문에 이 의존성을 Game에 무조건 추가할 근거는 발견하지 못했다.
- 설치 UE5.8 `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:6551–6556`는 Editor/Program 조건 밖에서 `WITH_EDITOR=0`을 정의한다. `:6316–6332`는 Development에서 기본적으로 `WITH_DEV_AUTOMATION_TESTS=1`을 정의한다. 따라서 **WITH_DEV_AUTOMATION_TESTS는 Editor 전용 guard가 아니다.**

## helper v2

`Editor/Rogue10mCompactWorldAuthoring.cpp`의 UnrealEd/Json/Landscape/WorldPartition editor 접근은 `:5–633`, `:746–1199`의 WITH_EDITOR 블록 및 각 공개 함수 내부의 WITH_EDITOR 분기에 있다. 공개 UCLASS는 Game에도 생성되지만 헤더가 사용하는 형식은 Core/Engine의 FString/bool/UBlueprintFunctionLibrary뿐이다.

공개 함수 5개의 Game 정의가 모두 남아 있다.

| 함수 | WITH_EDITOR 분기와 Game stub |
| --- | --- |
| InspectSourceTerrain | cpp:635–641 |
| InspectCandidateTerrain | cpp:644–650 |
| LoadCandidateRegion | cpp:653–695 |
| SelectOutsideComponents | cpp:698–743 |
| SnapshotRetainedTerrainData | cpp:1201–1207 |

Game 분기는 실패 JSON 문자열만 반환한다. Landscape 타입·GEditor·FJsonObject를 요구하지 않는다. 헤더의 `DevelopmentOnly` metadata를 compile guard로 착각하면 안 되지만, 현재는 실제 함수 정의의 stub이 있으므로 이 점 때문에 unresolved 함수가 생기는 구조는 아니다. Game에서 이 함수로 terrain editor 검증을 수행할 수는 없다.

기존 `Rogue10mWorldLayoutAuthoring.cpp`도 editor include/namespace를 `:5–268`로 감싸고 공개 3함수에 Game stub을 둔다(`:270–484`). 향후 v3를 설치하면 이 검토의 v2 판정을 그대로 재사용하지 말고 새 include/공개 선언/구현/의존성을 재대조해야 한다.

## G08/VFX

- `Editor/Rogue10mG08SequenceAuthoring.cpp:2–626` 전체가 WITH_EDITOR다. AssetRegistry, Editor.h, Json, montage 제작·SavePackage 호출과 `:581`의 `Rogue10m.AuthorG08Sequence`는 Game에 들어가지 않는다. parser automation도 WITH_EDITOR 안에 중첩된 WITH_DEV guard다.
- runtime의 AuthoredContactTimeline은 표준 C++ array/cstdint/cmath/limits만 사용한다. CombatVfxPlacement는 Core math만 사용한다. AttackSkillData의 contact/recovery 및 VFX 4필드는 runtime DataAsset 형식이며 Editor 모듈 타입을 노출하지 않는다.
- `Components/Rogue10mCombatComponent.cpp`의 새 코드가 요구하는 Engine/GameplayAbilities/Niagara/Core 형식은 기존 runtime 의존성 범위다. 3인칭 검사 `:430`, `:1373`, VFX 분기 `:2073`, `:2144`가 유지된다. 피격 취소는 `Character/Rogue10mCharacter.cpp:288–311`의 실제 유한 양수 피해 이후 InterruptAttacksOnHit 경로를 유지한다. 면역 기능은 추가하거나 활성화하지 않았다.
- `Tests/Rogue10mAuthoredContactSequenceTests.cpp:1–827`과 CombatVfxPlacementTests.cpp:1–39는 WITH_DEV_AUTOMATION_TESTS라 Game Development에서도 컴파일 대상이 될 수 있다. 확인한 include/API는 runtime Engine/Core/GAS/Physics 계열이다. G08 자동화와 VFX placement 자동화의 EditorContext는 **등록·실행 컨텍스트 필터이며 compile guard가 아니다.**
- `Tests/Rogue10mStaffVfxRuntimeTest.cpp:1–377`은 전체 WITH_EDITOR다. `:375`의 `Rogue10m.TestStaffVfx`는 정상 Game 타깃에서 존재하지 않는다. 이를 Game ExecCmds로 호출하면 runtime feature 실패가 아니라 사용할 수 없는 QA 명령 문제가 된다.

Editor 전용 IWYU 후속 확인점: G08 authoring `:9`, `:556`은 AssetRegistry를 직접 사용하지만 Rogue10m.Build.cs의 editor 조건부 목록에는 AssetRegistry가 명시되어 있지 않다. Rogue10mEditor 모듈의 의존성 선언은 runtime 모듈의 직접 의존성 선언을 대신하지 않는다. 현재 Editor 빌드의 전이 include/link 환경과 별개로 owner는 향후 editor 의존성 정리 때 editor 조건부 AssetRegistry 선언을 확인할 수 있다. **WITH_EDITOR=0에서 이 cpp가 비워지므로 이것을 Game 빌드 차단으로 판정하지 않는다.** 이번 검토에서는 수정하지 않았다.

## VillageQuest/NPC/Tuning

이 범위는 하위 읽기 전용 reviewer가 독립 검토했다. runtime Component/DataAsset/Director/NPC/Objective/Model/Presentation의 include는 Core/Engine/UMG/GAS/프로젝트 runtime 범위이며 UnrealEd/Landscape 무가드 경로를 발견하지 못했다.

- `Data/Rogue10mVillageQuestDataAsset.cpp:112–137`의 Modify/MarkPackageDirty와 템플릿 초기화는 WITH_EDITOR 내부다. 헤더 `:80–84`의 CallInEditor metadata 자체는 Editor 링크 의존성이 아니다.
- `ResetToIntroSupplyExample()`는 Game에서 템플릿을 초기화하지 않는다. SelectionTests.cpp:50처럼 이 함수를 이용해 만든 Editor fixture를 Game smoke에 그대로 재사용하면 같은 결과를 기대할 수 없다. Game smoke는 이미 구성된 runtime 정의 또는 별도 runtime fixture를 사용해야 한다.
- `DataAsset.cpp:82`의 UWorld::RemovePIEPrefix는 설치 UE5.8 `Engine/Classes/Engine/World.h:4456`, `Engine/Private/World.cpp:9338`의 guard 밖 runtime API다. 별도 UnrealEd 의존성이 아니다.
- `Tests/Rogue10mVillageQuestRuntimeTest.cpp:2–691` 전체가 WITH_EDITOR다. `:687`의 `Rogue10m.TestVillageQuests`와 Json/Slate GUI fixture는 Game에서 제외된다.
- Lifecycle4/Reward5/Selection3/Definitions2/Tuning4, 총 18개 자동화는 WITH_DEV_AUTOMATION_TESTS이며 runtime API로 작성되어 있다. EditorContext만 바꾸어 실제 Game fixture의 의미가 동일하다고 주장해서는 안 된다.
- `Tests/Rogue10mVillageQuestRewardTestProxy.h:8–23`은 UCLASS와 inline UFUNCTION을 모든 target에 유지하고 callback 호출만 WITH_DEV guard 안에 둔다. 생성된 UHT 함수의 Game 구현이 사라지는 비대칭 guard는 없다. CatalogTestCharacter/HitInterruptionTestTarget도 runtime 형식의 fixture다.
- runtime Director/QuestComponent는 authority, IsGameWorld, NM_Standalone 및 정상 GameState/PlayerState/선택 DataAsset 조건을 요구한다. 이것은 모듈 링크 문제가 아니라 smoke fixture의 실행 전제다(`VillageQuestDirector.cpp:19`, `VillageQuestComponent.cpp:24`).

## smoke 인계 시 주의할 구체 계약

1. 정상 Rogue10m Game/Win64/Development를 검증한다. 성공을 만들기 위해 WITH_EDITOR를 강제로 켜거나 Editor 모듈을 Game에 무조건 링크하는 우회는 적절하지 않다.
2. Editor 전용 AuthorG08Sequence/TestStaffVfx/TestVillageQuests 명령과 helper stub을 Game 기능 검증 수단으로 삼지 않는다. Development 자동화의 컴파일 가능 여부와 EditorContext 테스트의 Game 발견·실행 가능 여부를 별도 기록한다.
3. 기존 GUI 자동화 fixture·InputKey/Slate/OnClicked 경로를 내부 API라는 이유로 GUI 금지 예외로 사용하지 않는다. 외부 Work의 비GUI runner는 그 Work의 별도 구현·검증 대상이며 이 문서는 이를 생성하거나 실행하지 않았다.
4. 실제 authored montage/DA 접촉·회복, actual AppliedDamage/피격취소, VFX 표시, NPC 보상·맵 연결은 컴파일 guard 검토만으로 통과했다고 표시하지 않는다. 3인칭과 면역 보류 정책을 바꾸는 compile workaround도 만들지 않는다.
5. staged v3 설치, Build.cs 변경, 새로운 internal-smoke 소스 추가가 끝나면 frozen 입력을 기준으로 재검토하고 실제 Game build/실행 증거를 새로 얻어야 한다. 기존 HelperBuildA Editor DLL은 그 증거가 아니다.

## 근거와 제한

`static-evidence.json`은 핵심 파일 39개의 raw SHA/크기와 runtime 모듈의 cpp/h 347개에 대한 보수적인 lexical guard marker scan을 기록한다. WITH_EDITOR=0, WITH_DEV_AUTOMATION_TESTS=1에서 WITH_EDITORONLY_DATA를 0/1 양쪽으로 조사했으며 활성 가능 영역에서 선정한 Editor/Json/Landscape include·GEditor marker는 0건이었다. 미해석 조건 9개는 보수적으로 양 분기를 열어 두었다.

이 검사는 실제 C++ preprocessor, UHT, UBT dependency graph, unity/non-unity 컴파일, 링크 또는 plugin 전체 호환성 검사가 아니다. 특히 Game Development에 남는 automation translation unit의 모든 C++ API 오버로드가 컴파일된다는 보증은 하지 않는다. 이번 결과는 **구체적인 Editor guard 누수 미발견 + smoke 실행 계약 위험 인계**이며, 실제 Game 빌드와 런타임 QA는 미실행이다.
