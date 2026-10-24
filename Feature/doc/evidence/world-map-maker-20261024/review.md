# World Map Maker — Reviewer 결과

검토일: 2026-10-24, 최종 갱신 2026-10-25 (Asia/Seoul). 검토 범위는 새 Editor 모듈의 모델, Unreal 변경·저장·WP 수명 계약, 수정된 UI 이벤트 및 최종 빌드/자동화 보고서이다. UI를 구현한 작업자가 모델을 별도 관점으로 검토했으며, UI 전체의 독립 시각 검토로 주장하지 않는다.

## 판정

아래 검증 범위에서 미해결 기능 차단 결함은 확인하지 못했다. 최초 지적한 이동 잠금 우회는 수정·회귀 검증되었다. 시험용 패키지 잔여물 정리도 완료했고 최종 Content 변경 목록은 비어 있다. 실제 생산맵을 화면에 렌더링한 사용성 검증과 패키징 검증은 포함하지 않는다.

## 수정 확인

- **Actor Lock Movement 우회:** `IsEditableEnvironmentActor()`가 `Actor->IsLockLocation()`을 거절한다. 선택 영역 자동선택 및 전체 그룹 이동 사전검사에 함께 적용된다. 최종 통합 시험은 잠긴 액터 이동 거절과 위치 불변, 보호 액터가 섞인 선택의 전체 이동 거절을 포함한다.
- **PIE 종료 후 상태:** UI 등록/해제 모두 `ShutdownPIE`를 사용한다. 로컬 UE5.8 `Editor.h`의 같은 bool 델리게이트 및 `PlayLevel.cpp`의 정리 이후 broadcast를 확인했다. 이 수정은 정적 API/순서 검토이며 실제 PIE 시작·종료를 반복 조작한 결과로 보고하지 않는다.
- **지형 배치 trace:** 모델은 로컬 `FEdModeLandscape::LandscapeTrace`와 같은 `LineTraceMultiByObjectType` + `ECC_Visibility` + complex query를 사용하고, 결과를 실제 `ULandscapeHeightfieldCollisionComponent`로 다시 제한한다. 이전 generic component body trace 경로를 제거했다. 최종 통합 시험의 실제 Landscape 위 배치와 높이·yaw·scale 검사가 통과했다.
- **UE5.8 Slate API:** 지원되는 `SWrapBox::UseAllottedSize(true)` 사용과 최종 빌드 성공을 확인했다.
- **시험 외부 패키지 정리:** native map 삭제 후 남는 시험용 OFPA 액터·폴더 패키지를 `ObjectTools::CleanupAfterSuccessfulDelete`로 정리한다. 시험 전용 함수는 정확한 prefix+32자리 GUID, fixture 맵 부재, 계산한 외부 액터·객체 경로와 개별 파일의 동일 subtree를 모두 검사한다. 다른 맵이나 공유 폴더를 지우는 경로가 없다. 최종 시험에서 해당 fixture 아래 잔여 파일 0개를 확인한다.

## 모델 검토

- 현재 Editor world의 정확한 생산맵 경로·World Partition 초기화와 PIE/SIE 거절을 확인한다. 시험맵 예외는 개발 자동화 빌드에서 자동화 실행 중 고유 prefix로 제한한다.
- 격자 좌표는 음수 floor·유한값·외곽 포함·가장자리 절단을 처리하며, 런타임 셀 ID와 편집 영역을 구별한다.
- 네이티브 WP가 로더를 보유하고 모델은 약한 핸들만 기록한다. 해제 시 해당 WP의 등록 집합에 남은 자기 핸들만 해제해 다른 사용자 로더와 맵 종료 이후 수명을 침범하지 않는다.
- 환경 편집은 소유 태그·정확한 native StaticMeshActor 클래스·양의 폴더 목록과 보호 라벨/부모·자식 부착/이동 잠금 규칙을 통과해야 한다. 그룹 이동은 월드·레벨·레벨 잠금·전체 bounds를 변경 전에 전량 검사한다.
- 배치와 이동은 `FScopedTransaction`, 액터/루트 컴포넌트 `Modify()`, dirty 처리 및 editor post-edit 경로를 사용한다. 생성 액터는 OFPA 패키지를 갖는다.
- 저장은 현재 맵 패키지와 `ULevel::GetExternalObjectsPaths` 아래의 dirty 패키지만 수집한다. 다른 맵·일반 콘텐츠에 대한 Save All 경로가 없다.

## 확인한 실행 근거

- [정리 수정 후 최종 빌드](build-cleanup-final.log): `Result: Succeeded`, 18.62초.
- [최종 실행기 결과](tests-final/result.json): exit 0, timeout false, 42.39초, 필수 3개 테스트 모두 `Success`.
- [최종 Unreal 원본 보고서](tests-final/report/index.json): `failed=0`, `notRun=0`, `inProcess=0`; 정상 성공 2개와 경고를 동반한 성공 1개이다. `EditorIntegration`에 경고 4개·오류 0개가 있다.
- `GridBounds`: 유한값·음수/내부/외곽 경계·64개 영역·전체 1.8km 사각형 면적.
- `EnvironmentPolicy`: 비소유·보스 바닥·보호 폴더·게임플레이 클래스 제외.
- `EditorIntegration`: 고유한 새 WP/Landscape fixture에서 다른 맵·PIE 가드, Slate 생성/prepass, 로더 중복 방지와 사용자 로더 보존, 지형 배치·설정, 배치/이동 Undo/Redo, 잠금/혼합 선택 거절, 필터 선택, 외부 액터 저장 및 맵을 닫고 다시 로드한 위치·크기·메시 참조·이름 보존.
- 실행기의 생산 `OpenRunMap.umap`·외부 액터·외부 객체 1,260파일 전후 SHA-256 비교에서 변경 0개이다. 전체 프로젝트 콘텐츠의 무변경을 검사했다는 뜻은 아니다.
- [생산 월드 읽기 결과](production-world-readback.json) 및 [실행 로그](production-world-readback.log): 실제 OpenRunMap을 에디터 명령줄에서 로드하고 기존 bounded editor-region 로더로 프록시를 읽었다. `world_partition=true`, `streaming_enabled=true`, RuntimeHashSet, Landscape/충돌 컴포넌트 각 256개, 공간 프록시 16개, XY 크기 각 180,000cm, `success=true`, `requires_save=false`를 확인했다. 로그의 명시 성공 marker와 정상 종료를 함께 확인했으며 저장을 호출하지 않았다.

## 경고·한계와 인계

- 검증은 `UnrealEditor-Cmd -NullRHI`이다. Slate 생성과 desired-size 계산을 확인했지만 실제 렌더된 패널, 해상도별 레이아웃, 생산맵 위 마우스 조작·지형 sculpt/paint·native WP 탭 전환은 시각 검증하지 않았다.
- 통합 fixture는 새로 만든 작은 WP/Landscape 맵이다. 생산 OpenRunMap을 복제해 전체 PCG·보스·조우·포탈 흐름을 실행한 시험은 아니다. 20분 런, 실제 스트리밍 이동 성능, cook/Shipping은 미실행이다.
- 원본 보고서의 경고는 네이티브 참조 수집 알고리즘 fallback 1개와 fixture/동반 HLOD 에셋 삭제 후 AssetRegistry 재인식 3개다. 테스트 성공과 별개로 이 경고를 숨기지 않는다.
- 앞선 리뷰에서 발견한 이전 attempt1 HLOD 2개와 attempt2 외부 액터 1개·외부 객체 2개는 정확한 fixture 범위에서 Unreal 경로로 정리되었다. 최종 fixture도 정리 검사를 통과했다. 리뷰 시 `git status --short -- Content`는 출력 0개였다. 이 시험 패키지는 기능 산출물이나 게시 파일에 포함되지 않는다.
- 앞선 [읽기 시도](cleanup-readback.log)는 프록시가 언로드된 상태에서 기존 authoring 검사의 엄격한 컴포넌트 수 조건을 만족하지 못했다. WP/streaming 플래그는 정상이며, bounded editor-region 로드 후 위 생산 월드 읽기 결과가 통과했다. 생산 WP 결함으로 해석하지 않는다.
- 초기 빌드 실패와 첫 통합 시험 실패는 이전 시도 로그로 보존되며 최종 성공 수치에 합산하지 않는다. 원본 보고서의 경고 4개는 최종 시험에도 남아 있다. 이 문서 시점의 commit/push/main 승격 결과는 검토 범위 밖이다.
