# 전역 로드 참조 검사기 UE 5.8 호환성 수정 결과

## 결과

남아 있던 lazy GUID·FieldPath·Optional·동적 Struct/PropertyBag 미지원 오류를 실제 값 순회로 해결했다. 저장된 Landscape 후보를 새 Editor-Cmd 프로세스에서 읽어 **객체 90,810개·속성 값 1,093,395개, 미지원 0, 잘림 없음, loaded_reflection_walk_complete=true, success=true**를 확인했다. 검사 중 자산을 저장하지 않았다.

텍스처·레이어·지형 선택·editor 포인터·로드된 객체 identity/flags·일반 선택·트랜잭션·dirty package 등 9개 보존 조건이 모두 통과했다. 이전 cold-reload와 metadata/semantic digest, composite 높이·페인트, raw edit layers, layer metadata, Landscape GUID와 material도 동일했다.

개발 로그에서 확인했던 프로필 이름 오류도 수정했다. 기본 스폰은 선택한 class만 정하고 프로필을 적용하지 않아 PlayerState 이름이 기본값 `로그`로 남았다. `PossessedBy`에서 PlayerState 연결 후 선택 프로필을 적용하도록 고쳤다. 실제 native virtual Possess/BeginPlay 경로의 초기 이름·프로필 GUID·수동 교체·잘못된 프로필 거부 회귀가 통과했다. 저장 serialization은 테스트 전용 메모리 overlay에서만 수행했다.

## 구현

- **Lazy:** 로드된 객체의 기존 GUID annotation을 읽어 index로 대조한다. null/loaded/unresolved를 구분하고, unloaded GUID는 원문과 상태를 남긴다. GUID 생성·Resolve/Get을 호출하지 않는다. 중복 loaded identity는 실패로 처리한다.
- **FieldPath:** 원본의 복사본에서 저장된 weak owner와 경로만 읽는다. UE5.8에서 cache를 갱신할 수 있는 ToString/GetTyped/ResolveField는 검사 경로에서 호출하지 않는다.
- **Optional:** 설정된 값만 inner FProperty로 순회한다. unset payload를 읽거나 설정하지 않는다.
- **동적 저장소:** 이미 로드된 정확한 기반 타입/super chain으로 InstancedStruct/SharedStruct/ConstSharedStruct/InstancedPropertyBag를 식별해 동적 schema·memory의 실제 속성을 순회한다. 타입 이름에 PropertyBag가 들어가는 일반 struct는 정상 반사 순회를 한다.
- **상속·순환:** wrapper 파생 타입의 inherited payload와 추가 UPROPERTY를 모두 처리한다. 현재 재귀 stack의 type/memory 쌍으로 공유 순환을 backedge로 기록하며 서로 다른 alias 경로를 유지한다.
- **실패 조건:** 알 수 없는 reference-bearing property, 불완전 dynamic storage, 깊이·값·행 제한은 계속 불완전/실패로 남긴다. serializer/ARO를 호출하거나 오류를 무시 목록으로 덮지 않는다.

## 변경 파일과 검증

| 파일 | 변경 |
| --- | --- |
| `Source/Rogue10m/Editor/Rogue10mCrop20261114WorldAuthoring.cpp` | FReferenceScan 호환성·지원 상태 계수·진단 경계 |
| `Source/Rogue10m/Tests/Rogue10mReferenceScanTestTypes.h` | transient 반사 fixture·wrapper 파생 타입 |
| `Source/Rogue10m/Tests/Rogue10mReferenceScanTests.inl` | 실제 production scanner를 사용하는 회귀5개 |
| `Source/Rogue10m/Character/Rogue10mCharacter.cpp` | 선택 프로필의 possession 후 초기화 |
| `Source/Rogue10m/Tests/Rogue10mProfileInitializationTestCharacter.h` | 실제 production PossessedBy를 유지하는 native fixture |
| `Source/Rogue10m/Tests/Rogue10mProfileInitializationTests.cpp` | 메모리 저장 격리·프로필 회귀1개 |
| `Scripts/Editor/ValidateReferenceScanner20261116.py` | 실제 후보의 유한 읽기 전용 검증 |
| `Feature/implementation/reference-scanner-20261116/RunValidation.ps1` | 소유 숨김 PID·별도 로그·timeout·test/candidate 실행 |
| 계획서·본 결과서·워크플로·Sprint·DevLog | 변경/실패/한계/통합 상태 |

- `BuildEditor.ps1 -NoHotReload`: UHT/Editor 성공0, 최초123.06초·fixture교정 후30.59초. Build.cs 및 helper public header는 before 사본과 동일했다.
- `Rogue10m.Editor.ReferenceScan.*`: **5/5 성공, Error0·Warning0**. pointer/Optional, dynamic payload/lookalike/상속, empty/unresolved, 공유 cycle/alias, quota fail-closed를 검사한다. 전후 객체/internal flags·GUID annotation·package dirty와 payload pointer도 확인했다.
- `Rogue10m.CharacterProfile.PossessionInitialization`: **1/1 성공, Error0·Warning13**. 경고는 빈 QA 저장 첫 읽기와 fixture의 기본 무기/공격 슬롯 미설정이다. 숨기거나 낮추지 않았다.
- 실제 후보 검사 **success=true**, 미지원0, 보존9개 및 이전 cold snapshot 비교7개 통과.
- 최종 scanner/profile/candidate 세 로그 모두 검사기로 **Error/Fatal/ensure0** 확인. 후보 감시30초도 신규0이다.
- `CheckGeneratedChanges.ps1` 통과. 기존 다른 작업의 바이너리 변경 및 줄바꿈 경고는 유지했다.

## 첫 실패와 독립 리뷰

첫 scanner 실행은4성공·1실패였다. 새 fixture의 `NewObject<UObject>`가 UE5.8의 추상 class 제한에 걸려 ensure를 발생시켰다. 동일 assertion을 유지하고 구체 transient fixture class로만 교정했다. 첫 로그 Error55줄·자동화 report27 errors는 삭제하지 않고 `scanner-tests-first`에 보존한다. 재실행은5개 모두 error/warning0이다.

독립 리뷰의 P2: engine의 FInstancedStruct 파생 타입을 exact 기반 이름만으로 분류하면 payload를 놓칠 수 있었다. loaded super chain 탐색·파생 own fields 순회와 실제 파생 fixture를 추가했다. 최종 재검토에서 잔여 actionable은 없었다. 프로필 fixture의 SaveGames 경로 격리와 teardown 검증도 리뷰 후 보강됐다.

## 남은 경계

- 이번 전역 검사는 **현재 프로세스에 로드된 반사 속성 범위**다. native AddReferencedObjects/custom Serialize, 미로드 package, 문자열 경로, 전체 전이 closure 또는 삭제 안전성까지 인증하지 않는다. `project_wide_dependency_complete=false`, `crop_reference_safety_verified=false`는 그대로다.
- 기존 cold 세션의 vendor `/Engine/EngineMeshes/Humanoid` 누락은 UE4.18 skeleton의 제거된 RigConfig.Rig import로 별도 추적했다. 원본6개를 임의 resave하거나 known-missing 설정으로 숨기지 않았다. 원본보존을 전제로 한 프로젝트 파생 skeleton/메시/animation 참조 전환이 필요한 별도 자산 호환성 문제로 남는다. 이번 최종 세 검증 로그에서는 발생하지 않았다.
- 프로필의 물리 입력/UI 전체·기존 지형의 도로/레이 통행·전체 게임플레이 인수는 이번 C++ 회귀와 별도다.
- 작업 시작 전부터 work branch에 다수의 변경·untracked source와 미완료 지형 QA가 있었다. 두 기존 cpp의 작업 전 사본과 이번 정확한 diff를 따로 보존했다. 그 전체를 섞어 commit/push/main 승격하지 않았으며 통합 완료를 주장하지 않는다. 코드와 증거는 현재 `Sprint#9-32-landscape-world-blockout`에 있다.

증거: `Feature/doc/evidence/reference-scanner-20261116/verification-summary.json`, `inspection-first/dependencies.json`, `scanner-tests-concrete/automation-report/index.json`, `profile-tests-first/automation-report/index.json`, `review.md`.
