# 개발 중 Unreal 출력 오류 분석과 재발 검사

## 범위와 보존

- 사용자 요청: 개발과 동시에 빨간 Error를 분석하고 해결한다.
- 기존 `Sprint#9-32-landscape-world-blockout` 작업 브랜치를 유지한다. 같은 checkout의 별도 Landscape 작업이 진행 중이므로 브랜치 전환·통합·전체 WIP 커밋·에디터 재시작·지형/선택 변경은 하지 않는다.
- 당시 live 로그는 `Feature/doc/evidence/landscape-production-20261116/editor-recovery/normal-editor.log`에 기록됐다. 당시 `Saved/Logs/Rogue10m.log`는 앞선 `-game` 세션 로그였다.
- Source/Config/Content 및 기존 지형 검사기는 이번 작업의 수정 대상에서 제외한다. 엔진 설치 원본과 실패 로그는 보존한다.

## Ultrawork Packets

| Packet | 목표 / 수정 범위 | 완료 조건 | 검증 | 복구 경계 |
| --- | --- | --- | --- | --- |
| E1 Architect / 조사 | live/과거 로그와 설치 엔진 소스의 오류 경로 구분 | 발생 시각·호출 경로·확정/가설 구분 | 공식 MCP GetLogEntries + 좁은 소스 읽기 | 읽기 전용 |
| E2 Builder / 진단 | 별도 숨김 `UnrealEditor-Cmd`에서 startup smoke를 영어/한국어 비교 | 테스트 이름과 실패 수를 실제 로그로 확인 | `-nullrhi -unattended -LogCmds="LogAutomationTest Log" -ExecCmds=Quit` | 소유 PID·별도 로그 경로만 |
| E3 Implementer | 표준 라이브러리 기반 로그 검사기 및 검증 | severity 오탐 방지·기존/신규 분리·유한 watch·실패 exit code | `Scripts/Tests/TestUnrealOutputLog.py` + 실제 live 로그 | 신규 스크립트 2개 |
| E4 Reviewer / Doc Writer | 독립 리뷰·상시 개발 규칙·분석 결과 기록 | 원본 오류 보존, 해결/남음/미검증 명시 | generated 체크·diff·live 구간 재확인 | 본 작업의 신규 문서 및 공용 문서 append만 |

## 초기 발견

- 21:19:48 KST: 시작 smoke 결과 `Condition failed` 15줄. 엔진의 CHECK adapter가 식을 버리고 같은 문구를 출력하며 기본 Warning verbosity가 테스트 이름을 가린다. UnifiedError의 영어 고정 기대값과 한국어 메시지 불일치가 후보지만 15개 전부로 단정하지 않는다.
- 21:24:33: 지형 전역 reflection 검사의 미지원 속성 때문에 fail-closed. 기존 실패를 보존하고 inspector 성공으로 바꾸지 않는다.
- 21:29:37: 의도적 선택 preview `success=false`를 실패로 취급했던 wrapper 오류. 기존 Landscape 작업에서 별도 판별 분기로 수정되어 재선택 성공 이력이 있다.
- 과거 `-game` 종료의 Python editor-only 타입 부재와 Humanoid 의존 누락은 현재 live 로그 재발과 구분한다.

## Exit Gate

원인별 해결 수준과 실제 확인 범위를 결과서에 남긴다. 엔진 설치 바이너리 결함이나 진행 중인 별도 개발 때문에 검증/통합이 불가능하면 이유를 명시하며 전체 오류 0 또는 전체 기능 완료로 보고하지 않는다.

## 확정 진단 뒤 추가한 최소 우회책

영어/한국어 대조로 startup 15줄이 엔진 테스트 3개의 번역 기대값 불일치임을 확정했다. 동일 테스트를 끄지 않고 영어로 정상 수행한 후 세션 언어/locale를 ko/ko-KR로 복원하는 신규 실행기와 작은 editor Python 스크립트를 추가한다. 기존 Landscape 실행기·현재 에디터·엔진 원본은 수정하지 않는다. 별도 숨김 진단으로 실제 테스트 통과·setter/getter 복원·정상 종료를 검증한다. 전체 UI 시각 복원은 별도 검증 범위로 남긴다.

## 2026-11-17 게시 범위 분리

- P1: origin/main 기준 독립 물리 sparse checkout에서 로그 도구·문서·증거만 선별한다. 완료 조건은 C++/Config/Content 변경0 및 미완료 기능 의존0이며 파일 목록과 diff로 확인한다.
- P2: Python 회귀·AST, PowerShell parser, generated/path와 diff 검사를 실행하고 별도 Reviewer가 범위를 검토한다. 실패하면 이 도구 범위로 돌아가며 Unreal 세션·바이너리는 변경하지 않는다.
- P3: scoped commit을 즉시 work branch에 push하고 develop→test→main에 fast-forward 반영한다. fetch 후 원격 분기나 검증 실패가 발견되면 다른 변경을 덮어쓰지 않고 이유를 기록한다. 원래 Sprint#9-32 작업과 누적 WIP는 그대로 유지한다.
