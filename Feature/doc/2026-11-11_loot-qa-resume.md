# 전리품 획득 재개 결과와 실행기 수정

## 현재 기능

전날의 자동 획득 재시도 구현은 이미 공유 프로젝트에 반영돼 있다. DroppedItem 두 파일은 이전 최종 격리본과 내용이 같다. 통합 과정에서 테스트용 빈 프로필 저장 초기화와 BeginPlay된 월드의 EndPlay 순서가 보완됐다.

보존된 `LootAllA/report/index.json`은 전리품 테스트 8개 성공·경고/오류 0을 기록한다. 별도 통합 기록은 처치·충돌·저장 Write 26개와 새 프로세스 Read 9개 성공, 기존 저장 변경 0을 보고한다. 이는 이전 실행 결과다. 이번 세션은 공유 UE·에디터·빌드를 사용하지 않았다.

자동화 전체 로그에는 테스트 시작 전 엔진 자체 검사 오류 문구 15개가 있다. 실제 전리품 테스트 시작 이후와 종료 단계에서는 오류 문구가 없다. 전체 로그 오류 0과 전리품 테스트 보고서 오류 0을 구분한다.

## 이번 구현

게임플레이나 카메라를 다시 바꾸지 않고 검증 실행기를 수정했다. 3인칭 유지·1인칭 보류 지시를 유지했다.

| 파일 | 수정 |
|---|---|
| Scripts/VerifyLootPickupRetry.ps1 | 명령줄 미확인 UE·경로 대소문자/슬래시/상대 파일명 검사, 실행 직전 재검사, 숨김 파일을 포함한 전체 저장 해시, 저장 루트·정확한 슬롯·확장자에 한정한 새 QA 저장 허용, 테스트 시작 이후 오류와 정확한 큐 완료 표식 검사, 실제 종료 확인 |
| Scripts/Tests/TestLootPickupRetryLauncher.ps1 | 실제 실행기를 가짜 프로세스와 임시 파일로 호출하는 29개 동작 검사. 실제 UE 실행 경로는 테스트 함수가 가로챈다. 선택적으로 보존된 실제 로그·보고서를 재판정한다. |

이 패치는 C++·에셋·세이브 형식·빌드 설정을 변경하지 않으므로 새 UE 컴파일 대상은 없다. 기존 기능의 추가 실게임 검증은 공유 담당자가 별도로 조율해야 한다.

## 자동 검증

- 수정 전 기존 실행기: 공통 26개 검사 중 14개 통과, 12개 미충족. 최초 테스트 작성 중 발견한 모의 함수 상태 범위 문제는 수정했으며 그 초기 결과는 별도 `Evidence/Before`에 남겼다. 비교 기준은 `Evidence/BeforeValidated/results.json`이다.
- 수정 후: 숨김 파일 2개와 보존된 실제 전체 로그 재판정을 추가한 29개 모두 통과했다. 근거는 `Evidence/ReviewedFinal/results.json`이다.
- 실제 UE 프로세스 실행 0, 실제 프로세스 목록 조회 0, 사용자 세이브 접근 0이다. 모의 프로젝트는 격리 작업 공간의 테스트 하위 디렉터리만 사용했다.
- 전체 8개 보존 로그 재판정은 시작 전 오류 문구 15개를 별도 기록하고 테스트 단계 오류 0 및 정확한 8개 완료 표식을 확인했다. 새 엔진 테스트 8개를 실행했다는 뜻이 아니다.
- 별도 Sol 6.1 Ultra 검토에서 숨김 파일 열거 누락을 찾아 `-Force`와 회귀 사례로 보완했다. 동적 테스트 프레임·약한 참조·월드 종료 순서에 새 구체적 게임플레이 결함은 찾지 못했다.

핵심 검사 범위는 계획 출력, 정상 3개/8개 보고서, 에디터 명령줄 변형·미확인, 기존 QA 슬롯 대소문자, 증거 덮어쓰기 방지, 새 저장 소유 범위, 숨김/일반 기존 파일 변경·삭제, 누락/중복/경고/오류 보고서, 테스트 중 엔진 오류·ensure·Python 오류, 테스트 시작·완료 표식이다.

## 통합

작업 공간: `C:/Users/PC/AppData/Local/Temp/Rogue10m-LootPickup-20261111`
브랜치: `Sprint#9-29-loot-qa-resume`

부모 담당자는 `loot-qa-resume-scripts.patch`의 두 스크립트를 검토 후 적용한다. 이전 전리품 runtime 패치를 다시 적용하지 않는다. 새 계획서·결과서는 `loot-qa-resume-feature-docs.patch`로 적용할 수 있다. 공동 일지는 `DevLog-addition-20261111.txt`, Sprint 문서는 `SprintChangeLog-addition.md`의 추가문을 기존 내용 뒤에 병합한다. 공유 파일 상태가 바뀌면 apply check를 다시 수행한다.

```powershell
git apply --check 'C:/Users/PC/AppData/Local/Temp/Rogue10m-LootPickup-20261111/loot-qa-resume-scripts.patch'
git apply 'C:/Users/PC/AppData/Local/Temp/Rogue10m-LootPickup-20261111/loot-qa-resume-scripts.patch'
.\Scripts\Tests\TestLootPickupRetryLauncher.ps1 -EvidenceRoot 'C:/Users/PC/AppData/Local/Temp/RogueLootQaIntegratedA' -RetainedSuiteRoot '.\Feature\doc\evidence\monster-loot-pickup-retry-20261110\LootAllA'
# The following actual UE run needs the shared editor/process owner to coordinate.
.\Scripts\VerifyLootPickupRetry.ps1 -Label ResumeQA1111 -FullSuite -Execute
```

공유 소스·에셋·에디터·빌드·Git 상태를 수정하지 않았고 커밋·푸시하지 않았다. 최종 패치 적용 검사·해시·기존 파일 보존 결과는 작업 공간의 `validation-summary.json`에 기록한다. 실제 입력·전체 런, 사망·인벤토리 교체 전용 회귀는 기존 범위의 남은 검증이다.

## 전리품 QA 실행기 보호 통합·현재 DLL 전수 재검증 (UTC 2026-11-11)
- 기존 드롭 재획득 gameplay는 이미 반영되어 있어 다시 덮어쓰지 않았다. 신규 QA 실행기/격리 테스트와 기능 문서만 순차 통합했다.
- 전체·숨김 저장 파일 보존, namespace/중첩경로 검사, 점유 에디터 재확인, 실제 suite 시작/정확8개 종료 marker, 테스트 단계 Error/Fatal/ensure/Python 오류 검사를 보강했다.
- 공유본 fake-process 29/29, 현재 DLL ResumeQA1111 전체8/8 성공·경고0·실패0. 원본 저장 변경0·예상 밖 신규0, suite/종료 red0이다. 엔진 시작 전 의도된 self-test red15는 분리 보존하여 전체 로그 오류0으로 주장하지 않는다.
- 물리입력·전체런·새 Write/Read 재실행과 구분한다. 이전35검사 증거는 보존했다.
- Notion 요약 후보: 전리품 구현을 중복 적용하지 않고 실제8개 검사를 새 실행기로 재확인했다. 숨김 파일과 세이브 충돌 보호를 강화했다.
- 근거: Feature/doc/evidence/loot-qa-resume-20261111/IntegratedLauncherA, Feature/doc/evidence/monster-loot-pickup-retry-20261110/ResumeQA1111.