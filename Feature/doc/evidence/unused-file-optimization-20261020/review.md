# 독립 검토와 보존 결정

## Architect / 원본 조사

- Source/Scripts/Config 조사에서 안전한 삭제 항목 없음. FirstPersonPresentation, FistAnimInstance, ViewAlignedUpperBodyProxy는 현재 코드 연결이 있고 이전 HUD/Arin 제작기는 현재 적용·복구·검증 도구의 입력이다.
- Art/Feature 추적 크기 조사: Art687.49MiB, Feature2,495.19MiB. 초기 전체 추적 파일11,638개. 현재 Content6,423개 약4,273.14MiB는 수정하지 않는다.
- `.fbm` 텍스처20개는 Textures와 동일하지만 FBX RelativeFilename이 실제 `.fbm`을 가리킨다. 재수입 계약을 보존한다.
- `ReferenceRefined/Arin.blend1`은 승인 기준56cc68... 파일이며 현재 `.blend`는bd96f4...로 다르다. `Arin_Motions.blend1`도 현행 `.blend`와 해시가 다르다. 확장자만으로 삭제하지 않는다.

## Reviewer / 적용 전 독립 검토

독립 Reviewer가 selection의 원본7개와 별도 보존 사본4개의 크기·SHA-256을 재계산해 모두 일치함을 확인했다. 삭제 집합과 보존 집합은 겹치지 않고 선택 파일은 regular/non-reparse다. 경로 이탈·reparse 거부, 전체 사전 검증, 압축본 해제 해시 확인 후 unlink, 복구 시 exclusive create가 적절하며 적용 차단 사항이 없다는 결론이다.

검증기 변경은 일반 JSON 우선, gzip fallback, 잘못된 파일 오류 전파, 압축 baseline 존재 시 재캡처 거절을 보존한다. 게임 코드·바이너리 변경이 없으므로 오래된 Unreal 제작기를 다시 실행할 필요가 없다. 과거 검증기 실제 실행은 현재 변경된 DataAsset 계약으로 실패하거나 증거를 덮어쓸 수 있으므로 실제 reader와 main의 제어 흐름을 AST로 추출해 오프라인 검증한다.

## 제한된 조사 범위

기존 AuditUnusedWorkFiles.py의 전체 tmp 읽기 감사는 다수의 보존 worktree·도구·출력까지 탐색하며 이번 범위에 비해 비용이 커져 종료했다(감사 프로세스만 Ctrl+C, exit1). 이 감사에서 완성된 후보 목록은 없으며 tmp 파일은 삭제하지 않았다. 기존2026-09-28 복구 계약과 진행 중인 무기 작업을 보존한다.

## 잔여 운영 사항

압축/복구 도중 프로세스가 중단되면 부분 출력이 남을 수 있으며 도구는 해시 불일치를 검출해 진행을 거절한다. 원본 또는 검증된 압축본을 보존하고, 해당 항목의 검증에 실패한 부분 출력만 확인 후 제거하고 재시도한다. 자동으로 부분 출력을 덮어쓰거나 복구 원본을 삭제하지 않는다. 자세한 복구 방법은 RESTORE.md를 따른다.

## 적용 후 최종 독립 검토

Reviewer가 보존 파일7개를 독립적으로 다시 읽어 원시 보존 파일의 SHA/크기와 gzip 해제 bytes를 selection/verify와 대조했다. 원본7개 부재, restore에서7개 재생성과 해시 확인, 뒤이은 apply/verify 재정리, reader의 실제3개 각10클립 파싱·압축 baseline 보호 결과가 일치했다. 순감소308,406,800바이트와 복구 문서도 일치하며 최종 차단 사항이 없다는 결론을 받았다. 선행 main10개 blob 동일 대조와 파일 덮어쓰기 없는 baseline 반영도 검토했다.
