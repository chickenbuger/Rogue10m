# Landscape 관측·로드 반사 참조 검사 도구 독립 게시 계획

2026-11-17. 남은 지형·전투 변경을 검증하고 완료 기능부터 각각 게시하라는 사용자 지시를 따른다. 미게시 Crop 맵과 NPC 시스템을 가져오지 않고, 독립적으로 컴파일·회귀검증할 수 있는 관측 도구를 먼저 분리한다.

## Scope Gate

대상은 `Rogue10mCrop20261114WorldAuthoring.h/.cpp`, `Rogue10mReferenceScanTestTypes.h`, `Rogue10mReferenceScanTests.inl` 네 파일과 본 계획·결과 문서다. 프로필 초기화·전투·NPC·Build.cs·Config·Content는 이 패킷에서 변경하지 않는다. 기존 지형 기능의 strict point-ray·Undo/Redo 실패를 해결한 것으로 표시하지 않는다.

도구는 지정된 원본/후보의 Landscape·retained raw/composite/paint/metadata 관측, 한정된 영역 로드, 엄격한 후보 확인 뒤 외곽 컴포넌트 선택, raw tile 반환, 로드된 반사 속성 참조 검사를 제공한다. 저장·지형 삭제·raw import를 수행하지 않는다. 영역 로드와 명시적 선택은 에디터 상태에 영향을 주므로 해당 함수의 계약과 읽기 전용 검사를 구분한다.

## Ultrawork Packets

| 패킷 | 목표·수정 영역 | 완료 조건·검증 | 되돌림 경계 |
| --- | --- | --- | --- |
| S1 Architect | main 기반 독립 checkout에 네 소스 파일만 선별 | 프로젝트 include closure가 네 파일로 닫히고 미완료 맵/런타임 기능이 없어도 컴파일 가능 | 신규 네 파일 |
| S2 Builder | 정규 UHT/Editor 빌드와 실제 scanner 회귀 실행 | Editor 빌드 성공, `Rogue10m.Editor.ReferenceScan` 다섯 테스트 전부 성공, 실제 `-AbsLog`에 신규 Error/Fatal/ensure 없음 | 소유 빌드·headless PID와 새 증거 경로 |
| S3 Reviewer | 지원 타입·읽기 전용·실패 닫힘·범위 검토 | 오해를 부르는 전역 안전성 주장과 WIP 의존성 없음, generated/path·diff 검사 통과 | 이 패킷 소스·문서만 보정 |
| S4 Doc Writer | 결과·DevLog·Sprint 기록과 기능별 commit/push | 새 빌드/회귀 증거를 기록하고 단독 기능 커밋 후 작업·통합 브랜치 게시 확인 | 본 기능과 문서 체크포인트 |

## 의존성 확인

네 파일 밖 프로젝트 include는 없다. 기타 include는 Core/CoreUObject/Engine/UnrealEd/Landscape/Json의 설치 엔진 API다. 기존 main의 Build.cs가 이미 해당 모듈을 선언한다. UE5.8 `StructUtils/InstancedStruct`, `SharedStruct`, `PropertyBag`는 CoreUObject export이므로 다른 작업의 Build.cs 변경을 가져오지 않는다. editor 구현은 `WITH_EDITOR`, 실제 회귀 include는 `WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`로 제한된다.

## 검증·한계

다섯 transient 회귀는 실제 production scanner를 같은 translation unit에서 호출한다. pointer/Optional, 동적 저장소와 일반 이름유사 struct·상속, 빈 저장소/미해결 GUID, 공유 cycle/alias, quota fail-closed를 검사하며 객체 flag·GUID annotation·dirty 보존도 확인한다. 신규 테스트와 별도 대체 scanner를 만들지 않는다.

이 독립 패킷에 맵은 포함되지 않는다. helper는 원래 프로젝트 물리 경로 `D:/Project/Rogue10m`와 정확한 원본·후보맵을 요구하며 임의 맵의 범용 검사 API가 아니다. 맵 한정 함수는 해당 콘텐츠와 세션이 존재할 때만 사용할 수 있다. 독립 회귀는 맵 없이 transient 객체만 사용한다. native AddReferencedObjects/custom Serialize, 미로드 package, 문자열 경로, 전체 전이 closure 또는 삭제 안전성을 인증하지 않는다. 과거 로컬 전체 작업 트리의 성공 결과는 이번 main 기반 빌드·회귀 결과로 재사용하지 않는다.
