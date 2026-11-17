# 개발과 동시에 Unreal 출력 오류 확인

의미 있는 편집·빌드·테스트의 전후에 실제 세션 로그를 확인한다. `-AbsLog`를 지정한 실행은 `Saved/Logs/Rogue10m.log`에 기록되지 않을 수 있다. 로그의 문장에 `Error`가 들어 있다는 이유만으로 오류로 세지 않는다.

## 현재 세션

2026-11-16 정상 에디터의 정확한 로그:

```powershell
$TaskLog = 'D:\Project\Rogue10m\Feature\doc\evidence\landscape-production-20261116\editor-recovery\normal-editor.log'
& .\tmp\blender-mcp-venv\Scripts\python.exe -B .\Scripts\CheckUnrealOutputLog.py --log $TaskLog
```

전체 검사에서 남아 있는 과거 오류도 실패로 반환한다. 개발 작업을 시작할 때 로그의 줄 수 또는 UTC 경계를 기록하고, 해당 구간의 새 오류를 확인한다. 줄 경계는 배타적이며 해당 줄 다음부터 검사한다. UTC 시간 경계는 포함한다.

```powershell
& .\tmp\blender-mcp-venv\Scripts\python.exe -B .\Scripts\CheckUnrealOutputLog.py --log $TaskLog --watch --duration-seconds 60 --json-output .\Feature\doc\evidence\my-feature\output-watch.json
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

엔진 설치 파일을 바꾸지 않는 다음 실행용 우회책:

```powershell
.\Scripts\OpenCheckedEditor.ps1 -Map /Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A
```

이 실행기는 기존 프로젝트 에디터나 빌드가 있으면 중단한다. 모든 시작 smoke 테스트를 `en/en`으로 실행하고, 지연 Python 명령으로 세션 언어 `ko`와 locale `ko-KR`를 복원한다. 설정 저장은 하지 않는다. 실행 로그에 테스트별 결과와 `ROGUE_EDITOR_CULTURE` 복원 기록이 남는다. 시작 성공을 PID만으로 선언하지 않으며 해당 기록과 로그를 검사해야 한다.

영어 smoke → 한국어 API 복원은 숨김 진단 프로세스에서 실제 확인했다. 전체 UI의 시각적인 한국어 복귀와 시작 전에 이미 로드된 현지화 에셋까지 인증한 것은 아니다. 엔진 테스트의 한국어 호환성 결함 자체는 남으며, 나중에 한국어 상태에서 해당 엔진 테스트를 직접 실행하면 다시 실패할 수 있다. 현재 편집 중인 에디터는 이 실행기로 교체하지 않는다.

상세 증거와 현재 지형 오류의 경계는 `Feature/doc/2026-11-16_output-log-error-triage.md`를 따른다.

## 2026-11-16 후속: 전역 반사 검사 호환성 해결

`Feature/doc/2026-11-16_reference-scanner-compatibility.md`에서 lazy/FieldPath/Optional/동적 Struct·PropertyBag와 상속 저장소를 보강했다. 새 DLL의 실제 후보 검사에서 로드 객체90,810개·값1,093,395개·미지원0·잘림없음·검사완료 및 보존 조건이 통과했다. 이전 cold 세션의 미지원 실패를 삭제하거나 성공으로 바꾼 것이 아니라, 수정 후의 새 실행 증거다.

프로필 초기 스폰의 이름 불일치는 PlayerState 연결 뒤 selected profile을 적용하도록 수정했고 실제 native Possess/BeginPlay 회귀가 통과했다. 앞으로 개발할 때도 정확한 새 세션 로그를 계속 지정한다. 이전 cold-editor.log는22:29:21에 닫혔으므로 이를 현재 live 로그로 감시하지 않는다.

vendor legacy Humanoid/RigConfig 문제와 native/custom/unloaded 참조의 안전 인증은 위 검사 완료와 별개다. 문서에 남은 기존 오류와 새 실행의 신규0을 혼동하지 않는다.
