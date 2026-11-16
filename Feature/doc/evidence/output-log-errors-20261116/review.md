# 독립 검토 기록

## 검사기

root 외 별도 Implementer/Reviewer 경로에서 로그 심각도·UTF-8/명시 UTF-16·시간/줄 경계·기존/신규 분리·한정 watch·파일 교체·hardlink 원본 보호를 검토했다. EOF partial행/유한 시간 인자/출력 alias/LoadErrors 명시 Warning·Display의 오탐을 보정했다. 최종 15 tests 중14통과·OS hardlink 생성1권한skip이며 samefile분기 mock는 통과했다.

## 실행기와 문화권 복원

별도 읽기 전용 Reviewer가 설치 엔진의 `KismetInternationalizationLibrary`, ExecCmd 지연 실행과 culture resource refresh 경로를 확인했다. 영어 고정 smoke 기대값을 영어로 정상 검사한 뒤 두 API로 세션 언어/locale를 복원하는 우회책은 타당하다고 판단했다. SetCurrentCulture는 asset-group override를 지우므로 사용하지 않는다.

첫 코드 검토의 두 발견:

- 중첩 큰따옴표가 UE 명령 파서를 끊을 수 있음: 내부 작은따옴표로 수정하고 동일 인자의 실제 probe 재실행.
- `/` 형식 경로와 null CommandLine이 duplicate guard를 빠져나갈 수 있음: separator 정규화·OrdinalIgnoreCase 비교·관련 process의 알 수 없는 명령줄은 차단. 순수 fixture5개 통과.

최종 독립 재검토는 `startup-en-restore-ko-final/startup.log`를 직접 읽고 smoke429성공·실패0·Error/Fatal0·en/en→ko/ko-KR·SaveToConfig=false·exit0/로그 정상 닫힘을 확인했다. 잔여 actionable finding은 없었다. 전체 UI 시각 복귀·현재 live 에디터 교체는 검증/수행하지 않았다.

## 지형 오류 조사

별도 읽기 전용 조사자가 Undo/Redo 후 composite Auto Layer 경계 `(378,693)` 불일치와 source byte/export 진단 경로를 확인했다. root는 공유 지형 파일을 변경하지 않았다. 별도 소유 개발에서 수행한 native refresh의 보존 PASS를 증거 파일로 재확인했다. 전체 reflection walker 실패를 성공으로 바꾸지 않았다.
