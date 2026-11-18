# Native startup 오류와 영어 대조의 한계

읽기 전용 독립 재검토. 아래 수치는 기존 raw log/process/result/report를 읽어 재계산한 값이다. 새 엔진 실행·테스트·빌드는 하지 않았다. 동결 중인 내부 Windows run02의 Source/Config/Content/Build/Scripts/Plugins, 기존 A/B/C 증거와 NOT_APPROVED, Culture-D validator 및 UTC guard/runtime 수정은 변경하지 않았다. 새 파일은 이 문서 하나다.

## 실제 관찰

| 기존 실행 | DLL SHA 접두부 | 선택 43개 | startup / 이후 RED | global RED |
| --- | --- | --- | --- | --- |
| NativeTestsA | 180DA46E668B | 43 Success | 15 / 0 | 15 |
| StartupCultureProbeD | 180DA46E668B | 실행하지 않음 | 0 / 해당 없음 | 0 |
| NativeTestsB | 34264A823A98 | 43 Success | 0 / 0 | 0 |
| NativeTestsC | 92CB712BBC20 | 43 Success | 0 / 0 | 0 |

RED는 Culture-D validator의 실제 정규식을 AST에서 읽어 적용했다. 단순히 `Error`라는 단어를 찾은 수치가 아니다. A/B/C 모두 exit 0, 선택 43개 정확 일치, 6개 성공 사례에서 12 warning event가 보고된다. RED 0은 warning 0이나 게임·화면·입력 검증을 뜻하지 않는다.

A의 15행은 `editor.log:1758–1772`의 실제 `LogAutomationTest: Error: Condition failed`이며 첫 선택 테스트 시작 `:2394`보다 앞선다. **선택 43개 성공과 프로세스 전체 startup 오류 15건은 함께 성립한다. A를 clean 실행으로 승인할 수 없다.**

근거: [Root 영어 probe review](../../../doc/evidence/map-resume-20261113/StartupCultureProbeD/review.json), [A](../../../doc/evidence/map-resume-20261113/NativeTestsA/editor.log), [B](../../../doc/evidence/map-resume-20261113/NativeTestsB/editor.log), [C](../../../doc/evidence/map-resume-20261113/NativeTestsC/editor.log). A log SHA는 `6D1FE242A6EA21F995538026DBA625AACDEFB92CC2B6052A15316ACA7DE901C7`, probe log SHA는 `EB5093A5AE8D0E268264066D401FC1622B8A7E063EF6E290B3564D8FAAFDC968`이다.

## 코드로 확인한 설명과 아직 남은 귀속

설치 엔진 `Engine/Source/` 기준:

- `Runtime/Core/Public/Misc/LowLevelTestAdapter.h:130`: 실패한 CHECK는 공통 문구 `Condition failed`만 AddError한다. 해당 문구에 CHECK 표현식·파일·라인이 없다.
- `Runtime/Launch/Private/LaunchEngineLoop.cpp:4374–4376`: startup에서 RunSmokeTests를 호출하며 반환 bool을 검사하지 않는다. 선택 43개 명령의 실행보다 이른 engine smoke 실패가 별도 오류로 남을 수 있다.
- `Runtime/Core/Private/Misc/AutomationTest.cpp:531–592`: SmokeFilter를 선택해 startup 테스트를 실행하고 결과를 dump한다. `:1298–1306`은 SmokeFilter의 일반 test-start 로그를 생략한다. `:1243–1269`의 dump는 TMap을 순회하며 오류를 출력하므로 로그 인접성만으로 실패 CHECK 순서를 복원하면 안 된다.
- `Runtime/Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp:479–535`: localized FText의 ToString 결과를 영어 literal과 비교하는 SmokeFilter CHECK가 있다. `:973–1000`의 StructuredLogSmoke는 A와 probe에서 관찰된 LogTemp 예제 5종을 출력한다. 이 예제는 `Log` severity이며 문자열 속 Error 단어 자체는 RED 오류가 아니다.

이 네 엔진 파일의 현재 SHA는 원래 NOT_APPROVED에 보존된 source pin과 모두 일치했다.

**관찰:** 같은 원래 DLL에서 A에는 한글로 표시된 UnifiedError 예제와 startup 15건이 있고, explicit `-culture=en -language=en -locale=en-US` probe에는 영어 예제와 RED 0이 있다. 원래 DLL 전체 SHA는 `180DA46E668B12C82DCF3A85B4EB51DB27DB14DBF3E7131FADE312CBB56CB76D`다.

**추정:** localized FText와 영어 literal의 비교 불일치라는 설명은 코드와 이 대조에 부합한다. 다만 probe는 ExecCmds=Quit인 startup-only 실행이며 A와 모든 argv·프로세스 조건이 같은 단일변수 실험은 아니다. B/C도 서로 다른 새 DLL이다. 이 결과로 원래 각 15행을 특정 CHECK 사이트에 일대일 귀속하거나 모든 환경의 근본 원인이 증명됐다고 할 수 없다.

**미귀속:** 원래 15건 각각의 실제 CHECK 표현식·호출 위치·기대값/관측값은 독립적으로 확인되지 않았다. 의도한 negative test였다는 승인도 없다. 따라서 [원래 NOT_APPROVED](../internal-smoke-culture-d/startup-red-review.NOT_APPROVED.json)는 그대로 유지하며 영어 대조를 소급 면제 근거로 쓰지 않는다.

## 현재 수용 계약

Culture-D `native43_validation.py` SHA는 `985500591447274B367B97A89257D1CA992BA7891890D220608D7B13B383CF98`이다. `:116–127`은 executable/project를 포함한 정확 18개 command 항목과 끝의 en/en/en-US 3개, 실제 LogInit command 일치를 강제한다. 마지막 startup 검사에서는 RED 0을 요구하며 startup review 유무·approval로 오류를 허용하지 않는다. 선택 테스트 중·종료 중 RED도 계속 거부한다.

영어 startup probe는 이 18-argument Native43 전체 실행을 대신하지 않는다. B/C의 clean 43개는 각 실행의 DLL/Source/Config/원본 로그에만 귀속된다. 기존 오류 15개를 지우거나 필터·Smoke 비활성화로 pass를 만들지 않는다. 최신 runtime의 UTC guard 단일 수정은 별도 Work의 범위이며 이 문서에서 중복 수정하거나 승인하지 않았다.
