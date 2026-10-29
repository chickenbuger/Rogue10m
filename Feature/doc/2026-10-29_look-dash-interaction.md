# 아래보기·회피 및 NPC 입력 체크포인트

이번 완료 기능은 **NPC에서 소비한 F 입력이 사용자 지정 공격으로 중복 실행되는 문제 수정**이다. 아래보기·달리기·회피는 새 실제 실행으로 검사했고 몸통 가림 결함을 확인했으나, 카메라 후보가 전체 시나리오를 통과하지 못해 제품에 채택하지 않았다. 시각 문제는 미완료다.

## 구현과 파일

- Components/Rogue10mSkillBindingComponent.cpp: 높은 우선순위 legacy 입력이 소비한 키는 SkillPressed에서 반환. F 저장/예약 정책, release/cancel, 자원·공격 수락 로직은 유지.
- Character/Rogue10mCharacter.h/.cpp: 실제 IsDodging 및 남은 쿨다운 읽기 API만 추가. 회피 규칙/시간/자산 변경 없음.
- Tests/Rogue10mNpcInteractionRuntimeTest.cpp와 Scripts/VerifyNpcInteraction.ps1: 실제 F/기존 UMG/비용형 Special 양성 대조·메뉴/대화 음성 대조·프로필 격리·캡처.
- Tests/Rogue10mLookDashRuntimeTest.cpp와 Scripts/VerifyLookDash.ps1: 실제 pitch 제한, W/Shift, E 회피, 아래보기+E, 벽 접촉·반복·쿨다운·공중/UI 거부. 선택적 EyeForwardCm은 detached QA 런타임 캘리브레이션이며 영구 설정이 아니다.
- 카메라 컴포넌트와 기존 MouseHeadStableRuntimeTest는 4676fdad 내용으로 복원됐다. git reset/다른 작업 덮어쓰기를 쓰지 않았고, 검증한 실험 변경 두 파일만 원복했다.

## 빌드와 실행 결과

UE5.8.3 CL58210709, MSVC14.44/SDK10.0.22621. 최종 Editor Win64 Development 빌드 PASS(33.01초). XGE 비활성 라이선스 경고는 standalone 빌드로 완료됐으며 설치/라이선스 변경을 하지 않았다.

NPC 최종 D: **39항목/0실패**, D3D11 Offscreen 실제 viewport PNG3장. F를 기존 DA_Attack_Knuckle_Special에 지정한 양성 대조에서 사용횟수0→1, 스태미나100→93, 쿨다운관측0.75초. 메뉴/대화에서는 사용횟수1 유지, 자원불변, 쿨다운0, 차지없음. 기존 “F 대화하기” 안내가 있어 중복 UI를 추가하지 않았다. C도39/0이나 최종 판정은 D를 사용한다.

LookDash 수치 실행(프레임별 검사 수이며 독립 테스트 개수와 다름):

| 실행 | 범위 | 검사/실패 | 시각 판정 |
|---|---|---|---|
| LDWeightA1029 | Weighted | 483/0 | 몸통 가림 미해결 |
| LDOriginalA1029 | Original | 476/0 | 몸통 가림 미해결 |
| LDWeightB1029 | Weighted | 537/0 | 몸통 가림 미해결 |
| LDEye40A1029 | Weighted | 529/0 | 몸통 가림 미해결 |
| LDWeightFinal1029 | Weighted | 528/0 | 몸통 가림 미해결 |

기존 회귀는 AttackResources4 + WeaponCooldown5 + WeaponSwitch8 = **17개 PASS**. 기존 runner의 assertions/expected counts는 유지하고 task 복사본에서 프로젝트 경로와 새 LookDashReg1029 QA namespace만 지정했다.

E는 별도 Dash 시스템이 아니라 현재 DoDodge다. 실제 DA_Dodge_StoneFist 값은 Distance250cm/Duration0.16초/Cooldown0.55초/스태미나6. 아래보기에서도 수평 이동·타이머·반복 거부·공중/UI 거부·벽 정지가 동작했다. PNG IO를 측정 뒤로 옮긴 실행에서 자유 이동 약47~51cm, 벽 앞 약7.6cm를 관측했다. Distance는 초기 속도 계산값이며 실제250cm 이동을 검증했다고 표현하지 않는다. 스태미나는 재생을 유지한 관측 비용 상한 검사이고 정확한 장부 청구 검증은 별도 자원 회귀다. 무적은 QA fixture에서 피해 간섭을 막기 위해서만 사용했으며 제품 회피 규칙에 추가하지 않았다.

## 확인된 시각 결함과 채택하지 않은 후보

기본 Original과 Weighted 양쪽에서 -80/-85° 하향, 하향 달리기에 몸통이 화면 대부분을 가린다. 단순 유한 transform/POV=trace 수치 PASS는 시각 PASS가 아니다.

안정화 경로가 head-relative EyeOffset을 full control pitch로 다시 회전하는 overwrite를 제거한 후보도 재현 문제가 남았다. actor 기준 계산에서 기본 눈 위치는 약14.5cm 뒤, -85° 기존 경로는 약24.8cm 뒤였다. 기준 머리 앵커가 캡슐 뒤쪽에 있어 오프셋 한 줄만으로 해결되지 않는다.

QA X40 후보는 실제 카메라를 캡슐 전방 약17.5cm로 옮겨 정지 하향/달리기 몸통 가림을 줄였다. 그러나 기본 손 프레이밍이 멀어지고 하향 대시에서는 토르소가 크게 남았다. 대시 중 raw head는 actor 전방 약25~85cm로 이동하지만 안정화 카메라는 약17.5cm에 남는 측정이 있다. 후보를 채택하지 않았고 기본 EyeOffset8을 유지한다. 단순히 near clip을 늘리거나 몸통 bone을 숨기거나 원본 포즈/상태를 임의 변경하지 않았다.

재현/후속 시작점: 최종 기본 설정의 LookDash 사진·frames.csv와 미채택 patch를 함께 보고, 소유자 1인칭 팔/몸 표현과 대시 포즈-카메라 결합을 별도 묶음으로 설계한다. 가능한 material section 분리와 기존 팔 모델을 먼저 읽기 전용 감사해야 한다. Source 마스킹이 shadow proxy에 복제되는 경로도 확인해야 한다. 새 에셋/대형 콘텐츠 작업은 이번 범위가 아니다. 원작 대시를 복제한다는 의미도 아니다.

## 실패와 보존

NpcBaseA는 F 대화+공격 동시 실행을 실제 재현해 FAIL했다. NpcFinalB는 비용형 Primary가 없는데 그 조건만 요구한 fixture 오류로 F 지정 전 중단됐다. 실제 게임이 허용하는 지상 noncharged Special로 대상을 바로잡아 자원·쿨다운·공격/차지 검사는 유지했다. 실패 로그/사진은 그대로 남겼다.

세이브/기존 증거747개(238세이브+509증거)를 독립 복사·SHA256 검증했다. 원본/보존본 최종 비교는 task/ look-dash-preservation-result.json 및 체크포인트 참조. 새 QA 세이브는 label별 전용 namespace로 남기며 실제 사용자 슬롯을 삭제/초기화하지 않는다. Content/맵·저장 형식·유튜브/다른 프로젝트·원격 Git 변경 없음.

DevLog/20261029_LookDash_Review에는 사진 디코딩/해시 manifest, 실제 geometry 요약과 미채택 패치를 보존했다. 각 기능 폴더 README에 성공/실패/실험 상태를 표시했다. 수동 물리 입력·일반 필드 완주·성능 개선률·전체 프레임 clipping-free를 검증했다고 주장하지 않는다. 현재 미해결 승인/사용자 입력 요청은 없다.

## 사용법·참고 자료

### NPC 대화 키와 기존 화면 안내

NPC 가까이에서 **F**를 누르면 대화한다. 대화 중에는 ↑/↓로 서비스를 고르고 Enter로 실행하며 Esc로 닫는다. F를 다시 누를 때는 미수령 시작 물품이 있으면 물품을 받고, 이미 받았거나 지급 물품이 없는 NPC이면 대화를 닫는다.

현재 대화 키는 Enhanced Input 에셋의 추정값이 아니라 `ARogue10mTutorialNPC::ActivateNearbyPlayer`의 `BindKey(EKeys::F, IE_Pressed, ...)`로 확인했다. 이 바인딩은 입력 소비를 요청하며 NPC 입력 우선순위는 2다. 화면 우측 상단의 기존 대화 카드에 **“F  대화하기”**가 이미 표시되므로 새 안내를 중복 추가할 필요는 없다. 대화 서비스의 ↑/↓/Enter/Esc도 `SetServiceInputEnabled`의 실제 바인딩에서 확인했다.

범위: 이 설명은 현재 `Rogue10mTutorialNPC` 계열의 안내·마을 서비스 NPC 기준이다. 다른 종류의 NPC가 모두 같은 상호작용을 구현했다고 확대하지 않는다. 대화 가능 여부는 거리, 로컬 플레이어, 생존 상태와 차단 UI 등의 실제 조건을 따른다.

근거:

- `Source/Rogue10m/World/Rogue10mTutorialNPC.cpp`: 생성자의 `InputPriority = 2`, `ActivateNearbyPlayer`, `HandleInteract`, `RefreshDialogueView`, `SetServiceInputEnabled`.
- `Source/Rogue10m/UI/Widgets/Rogue10mTutorialWidget.cpp`: 우상단 배치, 폭 420, `TutorialAction` 16pt 금색 안내.
- F와 사용자 지정 스킬 키의 중복 실행 수정·재검증 상태는 이 메모에 포함하지 않는다. 본 결과 문서의 실제 실행 결과를 사용한다.

### Map Maker를 여는 순서

1. `D:\Project\Rogue10m\Rogue10m.uproject`를 Unreal Editor로 연다. C++ 모듈을 새로 빌드했다면 해당 모듈을 읽는 에디터를 사용한다.
2. Content Browser에서 **`/Game/Rogue10m/Maps/OpenRunMap`**을 직접 연다. 현재 기본 시작 맵은 `L_Menu`이므로 패널을 여는 것만으로 플레이맵이 자동 로드되지는 않는다.
3. Play/Simulate를 종료하고 **Window > World Map Maker**를 연다. 같은 패널을 콘솔 명령 **`Rogue10m.MapMaker.Open`**으로도 열 수 있다.
4. 8×8 격자에서 작업 영역을 고른 뒤 **Load + focus**를 누른다. 영역당 기본 크기는 256m이며 전체 지원 경계 1.8km에서 가장자리 영역이 잘린다. 이것은 편집 작업 영역 좌표이고 런타임 셀 ID가 아니다.

### 현재 가능한 작업

- **Select environment in area**로 로드된 허용 환경물을 고른 뒤 일반 Details/기즈모로 위치·회전·크기·메시를 편집한다. 표시 숫자는 로드된 편집 가능 환경물 수이며 전체 월드 개체 수가 아니다.
- 여러 환경물을 선택하고 목적 영역의 **Move selection here**를 누르면 그룹의 상대 위치와 Z를 유지하며 XY 중심을 옮긴다. 이동 후 지면에 자동 밀착하지 않으므로 높이는 확인해야 한다.
- Content Browser에서 **Static Mesh 하나**를 선택하고 **Use selected mesh**를 누른다. 높이·회전·크기를 지정한 뒤 **Place mesh at area center**로 배치한다. 로드된 Landscape 충돌을 찾아 메시 바닥을 맞추며, 교량·건물 지붕 등 임의 표면에 붙이는 도구는 아니다.
- 배치와 그룹 이동은 **Ctrl+Z / Ctrl+Y**가 가능하다. 새 환경물은 `OpenWorld/MapMaker` 폴더와 프로젝트 태그를 사용한다.
- **Landscape tools**, **World Partition**은 기존 UE 편집 도구를 연다. 기존 PCG 식생은 해당 PCG 컴포넌트/그래프의 Details에서 편집한다.
- **Release tool areas**는 이 도구가 로드한 영역만 해제하고 사용자가 다른 곳에서 로드한 영역을 유지한다.

### 저장 산출물과 실제 게임 반영

**Save map + external actors**는 현재 `OpenRunMap`과 그 맵에 속한 dirty 외부 액터·객체 패키지를 저장한다. 즉 같은 맵에서 패널 밖으로 편집한 미저장 변경도 함께 저장될 수 있다. 다른 맵과 원본 메시·머티리얼 패키지를 일괄 저장하는 기능은 아니다. 도구 자체의 Export/Import 또는 별도 Apply 과정은 없으며, 저장된 같은 맵을 게임에서 다시 로드하면 해당 월드 변경을 읽는다. 메시나 머티리얼 원본을 별도로 편집했다면 그 에셋의 저장도 별도로 필요하다.

월드 저장 산출물은 `Content/Rogue10m/Maps/OpenRunMap.umap`과 해당 맵의 `Content/__ExternalActors__/Rogue10m/Maps/OpenRunMap`, `Content/__ExternalObjects__/Rogue10m/Maps/OpenRunMap` 계열 패키지다. 이번 점검에서는 이 패키지를 편집하거나 저장하지 않았다.

### 지원 범위의 한계

자동 선택·그룹 이동 대상은 정확한 네이티브 `AStaticMeshActor`, 프로젝트 소유 태그, `OpenWorld/Scenery`, `Camp`, `Roads`, `River`, `Bridges`, `Wayfinding`, `MapMaker` 폴더 등의 허용 조건을 만족하는 환경물이다. 잠긴 액터, 붙어 있는 계층, Boss/Boundary/PlayerStart/Bootstrap/StreamProbe/Checkpoint 표식 등은 보호한다. 보호 액터가 섞인 그룹은 일부만 옮기지 않고 전체 이동을 거절한다.

몬스터·NPC·퀘스트·보스 패턴을 자동 생성하거나, Blueprint 액터를 일괄 이동하거나, PCG 결과를 수동 소품으로 변환하거나, 런타임 스트리밍 규칙을 바꾸는 도구는 아니다. 이런 게임플레이 액터는 실제 유형과 의도를 확인하고 기존 World Outliner/Details 등 적절한 편집 경로로 별도 작업해야 한다.

근거:

- `Docs/WorldMapMakerGuide.md`.
- `Source/Rogue10mEditor/Private/Rogue10mEditorModule.cpp`: Window 메뉴, 콘솔 명령과 패널 등록.
- `Source/Rogue10mEditor/Private/MapMaker/Rogue10mMapMakerModel.h`: 25600cm, ±90000cm, -4..3 영역 계약.
- 같은 디렉터리의 `Rogue10mMapMakerModel.cpp`: `GetWorld`, `IsEditableEnvironmentActor`, `LoadCell`, `FocusCell`, `PlaceMesh`, `MoveSelectionToCell`, `SaveMap`.
- `Config/DefaultEngine.ini`: EditorStartupMap/GameDefaultMap `L_Menu`.

### 데바데 참고: 관찰과 적용 해석을 분리

아래 관찰은 참고 담당 에이전트가 실제 공개 플레이 영상을 재생해 전달한 내용이다. 이 메모 작성자는 영상을 새로 재생하지 않았다. 코드·에셋을 추출하거나 복제한 것은 없다.

관찰 근거는 [NoCommentaryHorror의 공개 트래퍼 플레이](https://www.youtube.com/watch?v=lOniHzDTUTc)다. 공식 개발 자료가 아닌 사용자 플레이 영상이다.

- 0:06.9, 0:23.8, 0:33.9의 이동 화면에서 큰 무기가 우하단에 대각선으로 놓이고 중앙 전방 시야가 비교적 열린다.
- 0:12.0~0:12.7의 아래보기에서 무기는 오른쪽에 남고 몸통 내부가 화면을 가득 채우지는 않는다.
- 0:14.5의 깊은 아래보기에는 덫 설치 동작 중 두 손·팔뚝과 일부 다리가 보인다. 고유 상호작용 장면이므로 일반 이동의 손·몸 노출 정책으로 그대로 복사하지 않는다.
- 별도 달리기 입력, 최대 아래보기 각도, 카메라 흔들림 수치, 대시 규칙은 이 관찰로 확인하지 못했다. 원작의 수치나 조작 계약을 확인했다고 쓰지 않는다.

Rogue10m 적용 해석은 **중앙의 적·공격 판정을 읽을 수 있도록 시야를 열고, 맨손을 낮은 좌우에 두며, 목·가슴 내부가 과도하게 들어오지 않게 하고, 손의 준비·회수와 작은 조절 가능한 카메라 움직임으로 무게감을 주는 것**이다. 이는 관찰을 참고한 설계 판단이며, 원작의 달리기·대시 기능을 재현했다는 뜻이 아니다. 현재 게임의 이동·충돌·자원·쿨다운은 이번 실제 QA 결과에 따라 별도로 평가한다.

이 초안에는 새 LookDash 수치·성공 개수·클리핑 수정 완료·수동 플레이 완료를 적지 않았다. 최종 결과에는 실제 실행 로그·캡처와 해당 시나리오 범위를 사용해야 한다.
