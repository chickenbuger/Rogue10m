# 전역 로드 참조 검사기 UE 5.8 호환성과 개발 중 새 오류 수정

## Scope Gate

사용자는 남은 전역 참조 검사기의 미지원 오류 해결과 개발 중 새 Error의 지속 분석·수정을 요청했다. 기존 Sprint#9-32 work branch에서 같은 checkout의 지형 작업과 범위를 조율한다. Source 검사기와 회귀검증 도구를 고치며 에셋·엔진 원본·저장 파일은 직접 수정하지 않는다. 진행 중인 공유 에디터를 임의 종료하지 않는다.

기존 실패의 원문은 보존한다. 전역이라는 표현은 현재 process에 로드된 UObject의 반사 속성 순회를 뜻한다. custom Serialize/AddReferencedObjects·미로드 package·문자열 경로·전이 참조 전체의 안전 인증으로 확대하지 않는다. 미지원 오류를 무시 목록이나 성공 플래그 변경으로 없애지 않는다.

## Ultrawork Packets

| Packet | 목표 | 수정 영역 | 완료 조건 / 검증 | 되돌림 경계 |
| --- | --- | --- | --- | --- |
| R1 Architect | 실패 타입과 비로딩 API 조사 | 기존 로그·설치 UE 소스 읽기 | lazy/FieldPath/Optional/dynamic structs API와 경계 확정 | 읽기 전용 |
| R2 Implementer | 실제 값 순회 지원 | CropWorldAuthoring의 FReferenceScan 및 전용 fixture | loaded/unresolved/null·Optional set/unset·동적 payload·일반 이름유사 struct를 검출; limit와 실패 닫힘 유지 | before source복사+정확한 diff |
| R3 Implementer | 새 런타임 profile 이름 오류 | 원인이 입증된 최소 C++ 초기화 경로·전용 테스트 | 선택 프로필 이름/GUID가 정상 spawn flow에서 일치, 저장/기존 외형 계약 유지 | 해당 파일 before사본 |
| R4 Builder | 정규 빌드·의미 있는 C++ 테스트 | build/test runner·증거 | Editor/UHT·새 fixture 실행 결과 확인; compiler 실패만 국소수정 | 소유 프로세스/별도 로그 |
| R5 Reviewer | 실제 후보 스캔과 read-only 보존 | 후보 읽기 전용 inspection | unsupported0·nontruncated·보존 체크 실제 확인, unknown coverage한계 유지 | 기존 실패 증거 불변 |
| R6 Doc Writer | 로그 관찰·결과·통합 상태 | 결과서·Sprint·한국어 DevLog | 새 오류와 과거 오류 구분, 검증/미해결/통합상태 명시 | append/새 문서 |

## 구현 원칙

- lazy GUID는 기존 loaded UObject의 이미 존재하는 GUID와 비교한다. GetOrCreateIDForObject/Resolve/Get 등 원본 캐시·dirty·로드를 바꾸는 API를 사용하지 않는다.
- FieldPath는 원본을 복사해 저장된 weak owner/path만 읽는다. ToString/GetTyped의 resolve 부작용을 피한다.
- Optional은 unset이면 payload를 읽지 않고 set 상태만 해당 FProperty로 재귀한다.
- InstancedStruct/SharedStruct/ConstSharedStruct/InstancedPropertyBag는 정확 타입으로 동적 type+memory의 실제 reflected fields를 읽는다. 타입 이름 contains 분류를 제거한다. serializer/ARO는 호출하지 않는다.
- 원래 방문/깊이/행 제한과 알 수 없는 reference-bearing property의 실패를 유지한다. 실제 참조 경로 검출을 fixture로 검증한다.
- 실행 중인 에디터가 있어 빌드/새 DLL 검증을 못 한다면 독립 물리 staging 또는 소유 작업과 clean restart를 조율한다. 사용자의 미저장 작업 폐기는 허용하지 않는다.

## 현재 새 오류의 초기 관찰

정확한 live 로그는 cold-editor.log, PID25892 실행이다. 기존 Error62줄/30초 신규0을 기록했다. profile QA API세 종류는 현재 스크립트에서 수정돼 있으나 runtime display_name 기본값 `로그`와 선택 UI 이름 CropWalkQA의 불일치는 별도 C++ 원인 후보다. Humanoid의 vendor skeleton 의존 누락도 이 세션에서 실제 재발했다. 첫 로그 검사에서 stale MCP session1회가 발견됐으며 fresh 초기화 후 읽기 호출은 복구됐고 일시 연결 실패의 현재 상태를 재확인한다.
