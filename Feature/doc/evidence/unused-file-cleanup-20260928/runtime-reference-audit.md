# 런타임·에디터 도구 참조 감사

- 일자: 2026-09-28
- 범위: `Source/`, `Scripts/`, `Config/`의 파일 목록 249개와 카메라/복싱 관련 호출 경로. 코드·설정·애셋 삭제 및 수정 없음.
- 결론: 이번 정적 감사에서 **완전히 미사용임을 입증해 즉시 삭제할 수 있는 소스·스크립트·설정 파일은 발견하지 못했다.** 현재 기본 경로에서 실행되지 않는 코드와 참조가 없는 파일은 구분해야 한다.

## 보존할 파일과 근거

| 대상 | 실제 근거 | 판단 |
|---|---|---|
| `Source/Rogue10m/Components/Rogue10mFirstPersonPresentationComponent.h/.cpp` | Character.cpp:43에서 default subobject 생성, PlayerController.cpp:92에서 초기 30도 시선 함수 호출. AppearanceCameraComponent.cpp:24와 Character.cpp:91/115/623 및 CombatComponent.cpp:504에서 Refresh 호출. | Appearance가 기본이어도 현재 실행 연결이 남아 있어 삭제 불가. |
| `Source/Rogue10m/Character/Rogue10mFistAnimInstance.h/.cpp` | Presentation.cpp:27에서 StaticClass 지정, :453/460에서 레거시 설정에 따라 동기 로드 및 메시 적용. Presentation.cpp:433/435의 Appearance enabled 분기가 빠지면 레거시 모드를 사용할 수 있음. 여러 테스트에서도 직접 Cast와 메서드 사용. | 현재 기본 화면의 비활성 경로일 뿐, 참조가 존재하는 대체 모드. 별도 기능 폐기와 호출 제거 작업 없이는 삭제 불가. |
| Character의 First Person Mesh 관련 선언/생성 | Character.h:86 접근자 및 Character.cpp 생성, AppearanceCameraComponent의 숨김·정지·복구, CombatComponent.cpp:1310의 대체 VFX 메시 선택, BasicBrawlerComponent.cpp:277 사용. | Blueprint CDO의 상속 컴포넌트 및 직렬화 참조까지 있으므로 단순 제거 대상 아님. |
| `Source/Rogue10m/Editor/Rogue10mFirstPersonArmsAuthoring.cpp` | :161의 정적 FAutoConsoleCommand가 `Rogue10m.CreateFirstPersonArms` 등록. `Scripts/Editor/CreateFirstPersonArms.py:39`에서 실제 명령 호출. | 에디터 수동 저작 진입점이 있으므로 다른 cpp의 include가 없다는 이유로 삭제 불가. |
| `Source/Rogue10m/Editor/Rogue10mAppearanceBoxingAuthoring.cpp` | :134에서 `Rogue10m.AuthorAppearanceBoxing` 등록, `Scripts/Editor/PrepareAppearanceBoxing.py:55`에서 호출. | 현재 Appearance 복싱 애셋 재생성 경로. 보존. |
| `Source/Rogue10m/Tests/`의 cpp 15개 | 전부 `#if WITH_EDITOR`와 정적 `FAutoConsoleCommand` 등록을 갖춤. TestAppearanceCamera, TestThirdPersonInspection, TestBoxingBasicAttack 및 레거시 회귀/촬영 명령 등. | 에디터 전용 실행 진입점. Shipping에서 사용하지 않는다는 사실은 불필요 파일임을 뜻하지 않음. 오래된 fixture의 기본 설정 호환성은 별도 확인 대상이며 미사용 증거는 아님. |
| `Scripts/`의 준비·촬영·검증 파일 | 독립 실행형 에디터 Python/PowerShell 도구가 많고, 일부는 `RunEditorRemote.py` 또는 콘솔에서 경로를 직접 전달하여 실행. | 코드 참조 검색 0건만으로 삭제 판정 불가. |

## 애셋 참조 주의점

- Presentation.cpp:28/30의 `GuardFullBodyMesh`, `GuardArmsMesh`에는 소프트 경로가 남아 있고 :460에서 실제 로드한다. `SK_FirstPersonArms`가 현재 기본 모드에서 숨겨진다는 이유만으로 삭제하면 레거시·도구 참조가 깨질 수 있다.
- `Scripts/Editor/PrepareBoxingBasicAttack.py:6-7`은 Preview의 `A_Boxing_Manny`를 Combat의 `A_BoxingJab_Manny`로 승격한다. `PrepareAppearanceBoxing.py`와 에디터 C++ 도구는 Combat 원본에서 좌·우 Appearance 시퀀스/몽타주를 만든다. Preview 경로 또한 재생성 입력이므로 자동 삭제 대상이 아니다.
- Blueprint/DataAsset의 하드 참조, 소프트 오브젝트 경로, CDO의 클래스/컴포넌트 참조는 일반 텍스트 검색만으로 완전 열거할 수 없다. Content 삭제는 Unreal Editor의 Asset Registry/Referencer 확인과 로드·재생성 영향 확인이 필요하다. 이번 감사에서는 Content를 삭제하지 않았다.

## 추가 확인 결과

- Source/Scripts/Config 249개 파일에서 0바이트 파일 없음.
- 같은 범위 전체 파일 SHA-256 비교에서 바이트가 완전히 동일한 중복 파일 없음.
- Config/DefaultEngine.ini:23-24에 동일 Boolean 설정의 대소문자만 다른 중복, :45-46에 동일 `DefaultGraphicsRHI` 줄 중복을 발견했다. 이는 정리 가능한 **중복 줄 후보**이지 파일 삭제 근거가 아니다. 이 감사에서는 수정하지 않았다.
- 모듈 Build.cs는 에디터 빌드에만 UnrealEd/MeshDescription 의존성을 추가하며, cpp 내 WITH_EDITOR 가드가 테스트·저작 도구의 실행 영역을 제한한다.

## 수행한 검사

`rg --files Source Scripts Config`, 대상 클래스/메서드/애셋 경로 텍스트 검색, Tests/Editor의 콘솔 등록 및 WITH_EDITOR 확인, 파일 길이 0 검사, SHA-256 동일 파일 그룹 검사. 빌드·Unreal Asset Registry 전체 참조 검사는 수행하지 않았다. 권장 삭제 목록은 비어 있다.
