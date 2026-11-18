# 개발과 동시에 Unreal 출력 오류 확인

의미 있는 편집·빌드·테스트의 전후에 실제 세션 로그를 확인한다. `-AbsLog`를 지정한 실행은 `Saved/Logs/Rogue10m.log`에 기록되지 않을 수 있다. 로그의 문장에 `Error`가 들어 있다는 이유만으로 오류로 세지 않는다.

## 검사 예시

아래 명령은 프로젝트 루트에서 함께 보관한 진단 로그를 검사한다. 실제 개발에서는 활성 에디터 명령행의 `-AbsLog` 경로를 지정한다. 검사기는 Python 3 표준 라이브러리를 사용한다.

```powershell
$TaskLog = (Resolve-Path -LiteralPath '.\Feature\doc\evidence\output-log-errors-20261116\startup-en-restore-ko-final\startup.log').Path
python -B .\Scripts\CheckUnrealOutputLog.py --log $TaskLog
```

전체 검사에서 남아 있는 과거 오류도 실패로 반환한다. 개발 작업을 시작할 때 로그의 줄 수 또는 UTC 경계를 기록하고, 해당 구간의 새 오류를 확인한다. 줄 경계는 배타적이며 해당 줄 다음부터 검사한다. UTC 시간 경계는 포함한다.

```powershell
python -B .\Scripts\CheckUnrealOutputLog.py --log $TaskLog --watch --duration-seconds 60 --json-output .\Feature\doc\evidence\my-feature\output-watch.json
```

경계 없는 watch는 시작 시 EOF를 기존 이력으로 분리하고 이후 오류로 종료코드를 결정한다. JSON의 `existing_error_count`와 `new_error_count`를 함께 보고하며, 신규 0을 전체 오류 0으로 바꿔 표현하지 않는다. 기본 단발 검사는 전체 로그를 검사한다. 종료코드는 0=선택 구간 오류 없음, 1=오류 있음, 2=입력/읽기 실패다. 무시 목록이나 자동 복구·로그 삭제 기능은 없다.

로그 경로를 생략하면 최신 Unreal 형식 로그를 찾지만 별도 진단 프로세스가 더 최근일 수 있으므로 실제 개발에는 `--log`를 지정한다. UTF-16 로그는 명시 경로로 읽을 수 있고 자동 탐색은 UTF-8 형식만 지원한다. 감시 중 교체·절단은 기존 오류를 보존하고 새 파일을 처음부터 검사한다.

## 원인별 처리

1. 새 Error/Fatal/ensure/Blueprint Runtime Error와 LoadErrors 오류를 기록한다. 원문 경로·줄·시각을 유지한다.
2. 실제 컴파일 실패, 런타임 문제, 에셋 참조, 자동화 fixture, 검사 도구의 호환성 문제를 근거로 구분한다.
3. 소유 코드/도구에서 원인을 고치고 같은 작업을 다시 검증한다. 같은 실패가 두 번 반복되면 Architect/Reviewer로 돌아간다.
4. 검사 실패를 성공으로 바꾸거나 verbosity를 낮추지 않는다. 현재 편집 중인 다른 세션·맵·저장을 재시작하거나 덮어쓰지 않는다.
5. 결과서와 한국어 DevLog에 수정, 재검증, 남은 문제를 적는다.

## UE 5.8 한국어 startup smoke 문제

현재 설치 엔진의 `UnifiedError.CreateErrorMessage`(7), `CreateErrorMessageWithContext`(4), `StructuredLogFormat`(4)가 한국어 메시지와 영어 고정 기대값을 비교해 시작 시 `Condition failed` 15줄을 출력한다. 별도 정상 종료 진단에서 한국어는 429개 중 3개 실패/15줄, 영어는 같은 429개 모두 성공/오류0을 확인했다. 이는 게임플레이 코드의 오류와 구분한다.

아래 프로젝트 실행기는 엔진 설치 파일을 유지하면서 시작 시 언어를 조정한다.

```powershell
.\Scripts\OpenCheckedEditor.ps1
```

이 실행기는 기존 프로젝트 에디터나 빌드가 있으면 중단한다. 모든 시작 smoke 테스트를 `en/en`으로 실행하고, 지연 Python 명령으로 세션 언어 `ko`와 locale `ko-KR`를 복원한다. 설정 저장은 하지 않는다. 실행 로그에 테스트별 결과와 `ROGUE_EDITOR_CULTURE` 복원 기록이 남는다. 시작 성공을 PID만으로 선언하지 않으며 해당 기록과 로그를 검사해야 한다.

영어 smoke → 한국어 API 복원은 숨김 진단 프로세스에서 실제 확인했다. 전체 UI의 시각적인 한국어 복귀와 시작 전에 이미 로드된 현지화 에셋까지 인증한 것은 아니다. 엔진 테스트의 한국어 호환성 결함 자체는 남으며, 나중에 한국어 상태에서 해당 엔진 테스트를 직접 실행하면 다시 실패할 수 있다. 현재 편집 중인 에디터는 이 실행기로 교체하지 않는다.

상세 증거와 현재 지형 오류의 경계는 `Feature/doc/2026-11-16_output-log-error-triage.md`를 따른다.

## 2026-11-16 후속 검증

참조 스캐너의 lazy/FieldPath/Optional/동적 Struct·PropertyBag와 상속 저장소 처리를 보강했다. 새 DLL로 당시 후보 맵의 로드 객체 90,810개와 값 1,093,395개를 검사했다. 미지원과 잘림은 0건이었으며 데이터 보존 조건을 통과했다. 이전 cold 세션의 미지원 실패는 이력으로 보존한다. [참조 검사 결과](../Feature/doc/2026-11-16_reference-scanner-compatibility.md)

프로필 초기 스폰의 이름 불일치는 PlayerState 연결 뒤 선택한 프로필을 적용하도록 수정했다. 실제 native Possess/BeginPlay 회귀 검사를 통과했다. 당시 `cold-editor.log`는 22:29:21에 닫혔다. 개발 중에는 새 세션의 실제 로그 경로를 사용한다.

위 검사의 범위는 당시 로드된 반사 데이터다. 이전 로그의 vendor Humanoid/RigConfig 기록과 native/custom/unloaded 참조의 상태는 별도로 확인한다.
