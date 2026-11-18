# GitHub 게시 전 레거시 정리 결과

## 코드

- `Rogue10mMapDataLibrary.h/.cpp`에 있던 초기 맵·NPC 샘플과 `Rogue10mRunHUD`의 고정 미니맵 샘플을 제거했다. 실제 맵과 포탈, 미니맵 위젯은 계속 기존 경로를 사용한다.
- 호출이 없던 전투 슬롯 조회 두 개, 빈 공격 디버그 훅, 데이터 에셋 도입 전 샘플 무기 생성 함수를 제거했다.
- 폐기된 FiveVillage 맵·전용 런타임에 의존하던 제작·검증 코드 7개를 제거했다. 공용 의뢰, RouteBudget 입력, 재실행 가능한 과거 회귀와 맵 복구 자료는 보관했다.
- 현재 분할 구조와 맞지 않는 구형 HUD 적용·검증 코드 3개, 초기 단축키 제거 스크립트와 추적된 Python 바이트코드를 제거했다.
- 현재 맵 제작 데이터 위치를 [월드 구조 문서](../../Docs/WorldStructure.md)에 갱신했다. 기존 저장 이행, 에셋 기본값과 애니메이션 호환 코드는 현재 사용처가 확인되어 보관했다.

## JSON 정리 목록

삭제한 Git 추적 JSON 5개의 로컬 파일 크기는 총 4,204,023바이트다. 경로는 `Feature/doc/evidence/` 기준이다. 아래 네 중복 사본은 삭제 전에 대응 원본과 SHA-256이 같은지 확인했다.

| 정리 경로 | 바이트 | 현재 보관본과 확인 근거 |
| --- | ---: | --- |
| `arin-apollo-20261005/promote-draft.json` | 109 | [컴파일 후 상태](evidence/arin-apollo-20261005/state-after-compile.json)에 동일한 에셋·노드 ID가 있고 [중간 호출 로그](evidence/arin-apollo-20261005/promote-draft-mcp-stderr.txt)에 성공 상태가 있다. |
| `arin-quality-20261007/export-candidate-1/final-visual-review.json` | 21,547 | 상위 폴더의 [최종 시각 검토](evidence/arin-quality-20261007/final-visual-review.json)와 같다. |
| `camera-forward-attacks-20261009/before-audit/inventory.json` | 419,617 | 상위 폴더의 [인벤토리](evidence/camera-forward-attacks-20261009/inventory.json)와 같다. |
| `camera-forward-attacks-20261009/before-audit/protected-packages.json` | 32,067 | 상위 폴더의 [보호 패키지 목록](evidence/camera-forward-attacks-20261009/protected-packages.json)과 같다. |
| `camera-forward-attacks-20261009/before-audit/source-poses.json` | 3,730,683 | 상위 폴더의 [원본 포즈](evidence/camera-forward-attacks-20261009/source-poses.json)와 같다. |

과거 검토의 파일명·해시는 실행 당시 기록 그대로다. 삭제한 추적 파일의 원본은 작업 기준 커밋 `084865cd`에서 복구할 수 있다.

## 검증

- C++·설정·스크립트의 삭제 심볼 및 실행 경로를 점검했다. Unreal 패키지 6,386개에서 삭제한 반사 이름을 ASCII와 UTF-16LE로 검색했으며 참조가 없었다.
- 남은 Python 446개, PowerShell 100개의 구문 검사와 오프라인 테스트 85개를 통과했다. 오프라인 테스트는 RouteBudget 47개, 입문 의뢰 제작 30개, 카탈로그 사전 점검 8개다.
- 삭제 경로 19개와 보존 JSON 5개의 파싱을 확인했다. 경로 검사와 변경 파일 공백 검사도 통과했다.
- 캐릭터 선택 UI의 동시 작업과 격리한 소스 사본에서 UE 5.8 `Rogue10mEditor Win64 Development` 빌드의 컴파일·링크·메타데이터 작업 228개가 완료됐다. UBT 종료 코드는 0이고 소요 시간은 456.79초다. 공용 가속 캐시 경로의 접근 오류는 빌드 전용 폴더로 경로를 지정해 해결했다. 최종 빌드의 캐시 저장 메시지와 전체 출력은 로컬 `tmp/LegacyCleanupBuild20261119/ubt-isolated.log`에 남아 있다.
- 검증 범위는 정적 참조, 오프라인 테스트, Editor 빌드까지다. 후속 요청에 따라 레거시 정리와 관련 기록을 별도 커밋으로 묶어 현재 작업 브랜치에 게시한다.

[작업 계획](../architect/2026-11-19_legacy-cleanup.md)
