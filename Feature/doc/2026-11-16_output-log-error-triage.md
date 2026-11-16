# 개발 중 출력 오류 분석과 재발 검사 결과

## 실제 결과

실제 정상 에디터의 `-AbsLog`를 확인해 과거 `Saved/Logs/Rogue10m.log`와 분리했다. 시작 시 이름 없이 출력되던 `Condition failed` 15줄은 설치 UE 5.8의 한국어 startup smoke 테스트 3개로 확정했다. 별도 프로세스에서 같은 429개를 영어로 실행하면 전부 성공한다.

다음 실행을 위한 프로젝트 한정 우회책을 추가했다. `OpenCheckedEditor.ps1`는 시작 테스트를 영어로 정상 수행하고 지연 Python 명령으로 세션 언어 `ko`·locale `ko-KR`를 복원한다. 최종 동일 인자 진단에서 **429/429 성공·Error/Fatal 0·설정 저장 false·ko/ko-KR 복원·정상 종료 0**을 확인했다. 현재 편집 에디터를 재시작하거나 이 실행기로 교체하지 않았다. 엔진 테스트의 한국어 호환성 결함 자체와 전체 UI 시각 복귀는 별도 범위다.

개발 중 정확한 로그를 검사하는 CLI와 상시 하네스 규칙도 추가했다. 실제 live 로그 30초 감시에서는 **기존 Error 52줄을 그대로 보존하고 신규 0줄**이었다. 이 결과를 전체 이력 오류 0으로 해석하면 안 된다.

## 원인과 해결 수준

| 묶음 | 원인/관찰 | 상태 |
| --- | --- | --- |
| startup `Condition failed` 15줄 | `FUnifiedErrorTest_CreateErrorMessage` 7, `CreateErrorMessageWithContext` 4, `FStructuredLogFormatTest` 4. 한국어 결과와 영어 고정 기대값 불일치 | 엔진 원본 보존. 다음 세션용 영어 smoke 후 한국어 복원 우회책 실제 검증 |
| `retained-dependencies` traceback 1건 | process-wide reflection walker가 미지원 속성을 만나 fail-closed | 실패 이력 유지. 별도 후보 한정 검사와 전역 안전성의 범위 구분; 전역 성공 미인증 |
| `outside-preview` traceback 1건 | 의도적인 read-only preview의 `success=false`를 일반 실패로 취급 | 기존 Landscape 작업에서 `valid_preview` 경로로 수정·선택 성공; 본 작업의 수정으로 중복 주장하지 않음 |
| Undo/Redo `Retained snapshot incomplete` 각 1건 | composite Auto Layer의 공유 경계 `(378,693)` 값 차이. component165 local(63,63)와166 local(0,63), 같은 proxy. 원본 edit-layer 높이·페인트/metadata는 동일 | 별도 Landscape 개발 작업의 native refresh 후 실제 `after-native-refresh/comparison.json` 보존검사 통과를 관찰. 본 작업에서 지형 수정하지 않음 |
| 과거 `-game` Python 7 traceback/누락 Humanoid | 종료 세션에서 editor-only Python 타입 부재 및 vendor skeleton 의존 참조 | 현재 정상 editor/진단 로그에서 재발0. 과거 assets 문제까지 모두 수정했다고 주장하지 않음 |

`LogTemp: Error test...`는 엔진 smoke의 의도적인 일반 Log이며 실제 Error severity가 아니다. 검사기는 문장 내 단어 검색으로 실패를 만들지 않는다.

## 변경 파일

| 파일 | 목적 |
| --- | --- |
| `Scripts/CheckUnrealOutputLog.py` | severity·UTC/줄 경계·기존/신규 구분·유한 watch·rotation 감지·JSON·exit code |
| `Scripts/Tests/TestUnrealOutputLog.py` | 오탐·읽기 전용·경계·watch·파일 교체·출력 보호 검증 |
| `Scripts/OpenCheckedEditor.ps1` | 중복 editor/build 차단과 영어 startup/한국어 복원 실행 |
| `Scripts/Editor/RestoreEditorCultureAfterSmoke.py` | editor-only setter/getter 검증, 세션 언어/locale 복원, 설정 저장 없음 |
| `Feature/implementation/output-log-errors-20261116/ProbeStartup.ps1` | 소유 숨김 진단 PID·timeout·언어 대조·정상 종료 증거 |
| `AGENTS.md`, `Docs/OutputLogErrorWorkflow.md` | 실제 세션 로그를 개발 전후 검사하는 상시 규칙·사용법·한계 |
| 계획서·본 결과서·`DevLog/20261116.txt`·`Docs/SprintChangeLog.md` | 역할/검증/진행 상태 기록 |
| `Feature/doc/evidence/output-log-errors-20261116/` | 실행 원문·요약·실패 후보·로그 보고서·독립 리뷰 |

## 검증과 독립 리뷰

- 검사기 테스트 15개 중 14통과, 실제 OS hardlink 생성1은 Windows 권한으로 skip. `samefile` 보호 분기는 별도 mocking 테스트 통과.
- Python AST3개, PowerShell parser2개, 기존 세션 탐지 fixture5개 통과.
- 최종 startup 진단: `startup-en-restore-ko-final/startup.log`, 429성공·실패0·Error/Fatal0·en/en→ko/ko-KR·setter true·설정 저장 false·정상 종료0.
- ko/en 대조는 각각 `startup-ko-host-v2`와 `startup-en-host-v2`에 보존했다. 프로세스 종료코드0만으로 검증 성공이라 하지 않고 실제 Error 수와 테스트 결과를 검사했다.
- 독립 리뷰에서 중첩 따옴표로 복원 명령이 잘리는 위험과 slash/null command line의 중복 세션 탐지 누락을 찾아 수정했다. 같은 최종 인자의 실제 probe를 다시 실행했고 재검토에서 잔여 수정 사항은 없었다.
- 첫 sandbox 진단은 exit0이지만 로그 없음으로 시작을 증명하지 못했다. 첫 host 진단의 잘못된 ini override가 MCP8001 포트 충돌을 만들었고 Quit가 editor 종료 명령이 아니어서 timeout이 났다. 해당 실패를 보존하고 **소유 진단 PID50640만** 종료했다. override의 section bracket과 `QUIT_EDITOR`로 보정한 후 정상 종료를 확인했다.
- generated/cache 직접 편집 없음. 진단 엔진이 만든 User 캐시는 삭제하지 않고 본 증거 디렉터리의 제한된 ignore로 버전 관리에서 제외한다.

## 남은 경계와 통합 상태

- 현재 한국어 에디터에서 직접 해당 엔진 smoke를 다시 실행하면 결함이 재현될 수 있다. 우회책은 다음 정상 실행에 적용한다.
- reflection walker의 미지원 실패는 남아 있다. native refresh 보존 통과와 전역 참조 안전성·전체 지형/게임플레이 완료를 혼동하지 않는다.
- C++·Build.cs·기존 Config/Content를 이 작업에서 편집하지 않아 C++ 재빌드는 추가로 실행하지 않았다. 별도 Landscape 작업의 에디터 조작·컴파일/저장은 소유 작업이 담당한다.
- 같은 work branch와 checkout에서 지형 기능이 계속 검증 중이며 누적 WIP가 많다. 이 결과만으로 공유 브랜치 전체를 커밋·통합·승격하지 않는다. 현재 기능의 통합 완료와 main 승격을 주장하지 않으며 별도 개발의 최종 검증과 범위 분리가 필요하다.

관련: `Feature/architect/2026-11-16_output-log-error-triage.md`, `Docs/OutputLogErrorWorkflow.md`, `Feature/doc/evidence/output-log-errors-20261116/verification-summary.json`.

## 2026-11-17 독립 게시 검증

사용자의 기능별 즉시 commit·push 지시를 적용해, 원격 main의 e32e5abe에서 별도 물리 sparse checkout과 Sprint#9-33-output-log-workflow 브랜치를 준비했다. 본 도구·문서·증거만 옮기며 C++·Config·Content와 지형/전투 누적 변경은 포함하지 않는다. 최초 계획과 위 통합 대기 설명은 당시 공유 checkout의 이력이다.

검증 패킷은 P1 범위·의존성 확인, P2 독립 checkout의 Python 테스트·AST·PowerShell parser·generated 및 diff 검사, P3 독립 리뷰 후 작업 브랜치 push 및 develop→test→main 순서의 fast-forward 반영이다. 각 패킷의 복구 경계는 본 신규 도구·문서만이며 기존 작업 폴더와 Unreal 세션은 조작하지 않는다. C++ 변경이 없으므로 Editor 재빌드는 필요하지 않다. 참조 scanner와 초기 프로필 수정은 별도 미게시 작업이며 이 커밋의 구현 범위에 포함하지 않는다.

이번 공개 문서는 미게시 scanner 결과 링크와 미게시 Landscape 맵 의존 예시를 제외한다. 원문 후속 기록은 원래 작업 폴더에 유지한다. 11/16의 정상/실패 진단 원문은 보존하며 11/17에는 엔진 진단을 다시 실행하지 않고 독립 도구 검증만 재실행한다.

11/17 독립 checkout 재검증: 검사기15개 중14통과·hardlink권한1skip, Python AST3·PowerShell parser2통과, 보관된 최종 startup 로그 검사 Error0. 신규 Unreal 실행·기존 에디터 변경0. 게시 검증 기록은 Feature/doc/evidence/output-log-errors-20261116/publication-20261117.json에 보관한다.
