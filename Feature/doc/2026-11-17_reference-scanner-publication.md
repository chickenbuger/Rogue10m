# Landscape 관측·로드 반사 참조 검사 도구 독립 게시 결과

2026-11-17. 현재 상태는 **독립 네 소스 파일 선별 완료·정규 빌드와 회귀 재검증 대기**다. 게시 완료나 지형 맵 전체 인수 완료를 아직 주장하지 않는다. [계획](../architect/2026-11-17_reference-scanner-publication.md)을 따른다.

## 선별한 변경

| 파일 | 역할 |
| --- | --- |
| `Source/Rogue10m/Editor/Rogue10mCrop20261114WorldAuthoring.h` | 정확한 맵을 대상으로 한 관측·영역 로드·후보 선택·raw tile 반환 API |
| `Source/Rogue10m/Editor/Rogue10mCrop20261114WorldAuthoring.cpp` | Landscape 데이터와 editor 상태 관측, UE5.8 로드 반사 참조 검사 |
| `Source/Rogue10m/Tests/Rogue10mReferenceScanTestTypes.h` | transient pointer/Optional·동적 payload·상속 fixture |
| `Source/Rogue10m/Tests/Rogue10mReferenceScanTests.inl` | 실제 production scanner를 호출하는 기존 다섯 회귀 |

원래 누적 작업 트리의 파일을 바이트 그대로 물리 복사하고 SHA256 일치를 확인했다. Build.cs·런타임 Character·전투·프로필·NPC·Config·Content는 선별하지 않았다. 기타 프로젝트 include가 없고 main의 기존 엔진 모듈 의존성으로 닫히는 것을 읽기 확인했다.

## 동작과 검증 대상

검사기는 기존 loaded GUID annotation을 index로 대조하고 FieldPath의 복사본·Optional set 상태·실제 동적 struct/property bag 저장소를 순회한다. 상속된 native payload와 추가 반사 필드를 모두 처리하며 공유 cycle을 backedge로 기록한다. 미지원 reference-bearing property나 quota 초과는 계속 실패로 남긴다. resolve·GUID 생성·serializer 호출·불완전 결과의 성공 치환을 하지 않는다.

최종 판정에는 main 기반 정규 UHT/Editor 빌드, `Rogue10m.Editor.ReferenceScan` 다섯 회귀, 실제 지정 로그의 Error/Fatal/ensure 검사, 생성 파일·diff 검사와 독립 리뷰가 필요하다. 이전 11/16 전체 로컬 작업 트리의 scanner5/5·후보 loaded walk 결과는 원문 이력이며 이번 독립 게시 패킷의 재검증을 대신하지 않는다.

## 경계

이 패킷은 미게시 Crop 맵과 NPC 의존성을 제공하지 않는다. 맵 한정 API의 실제 사용은 정확한 콘텐츠·물리 경로·editor 세션이 있을 때만 가능하다. 단독 scanner 회귀는 transient 객체만 사용한다. 영역 로드·명시적 선택 함수는 읽기 전용 속성 순회와 구분한다.

현재 로드된 반사 속성의 완전한 순회가 native custom serialization·미로드 package·문자열 참조·전역 transitive closure나 삭제 안전성 인증을 뜻하지 않는다. 과거 Undo/Redo 직후 AutoLayer composite 실패, E19/E27 strict point-ray 실패와 전체 지형/성장/전투 인수는 이 도구 패킷의 완료 조건이 아니며 해결했다고 표시하지 않는다.

## 새 검증·게시 기록

빌드·실제 회귀·로그·리뷰·원격 확인 결과를 소유 실행자가 아래에 추가한다. 현재 이 문서에는 준비 단계 결과만 기록했다.

## 최종 독립 검증

main 기반 UHT/Editor 113.28초·exit0, transient5/5·failed0/notRun0, 실제 AbsLog Error/Fatal/ensure0, 독립 리뷰 잔여 문제0. Content/Config/Build.cs 변경0.

미게시 지형 맵과 NPC는 포함하지 않으며, 기존 지형 Undo/Redo·point-ray 실패를 해결한 것으로 확대하지 않는다.
