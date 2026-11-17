# 보스 AI 재개·검증 준비 도구 계획

## 범위

현재 작업 트리와 2026-11-10 독립 패치를 먼저 비교한다. 공유 소스·에셋·에디터·UBT·빌드 프로세스는 수정하거나 사용하지 않는다. 1인칭은 보류하고 기존 3인칭을 유지한다. 소스 스냅샷을 가진 별도 물리 작업 공간의 `Sprint#9-29-ai-target-resume-readiness`에서 도구·문서 패치를 만든다. Content 링크는 만들지 않는다.

## 확인된 기준

1. 컨트롤러의 HasTarget 정리는 이미 공유 작업 트리에 들어 있다. 옛 source 패치를 재적용하지 않는다.
2. 현재 테스트는 실제 HumanMale BP와 PlayerState/ASC 초기화를 사용한다. 예전의 추상 네이티브 캐릭터 생성은 실패한 후보이므로 복원하지 않는다.
3. Engine_BossTargetA는 실패 후보, Engine_BossTargetB는 5개 성공이다. 자연 AI BossNaturalB는 112검사 실패0이다. 현재 DLL과 과거 B DLL은 달라 새 실행 합격으로 취급하지 않는다.
4. 최초 인계 문서의 미적용·미실행 상태는 현재 상태와 다르다. 과거 기록은 보존하며 현재 결과를 따로 표시한다.

## Ultrawork Packets

| Packet | 목표 | 수정 위치 | 완료 조건 | 검증 | 롤백 경계 |
| --- | --- | --- | --- | --- | --- |
| 감사 | 현재 소스·실패A·합격B 구분 | 격리 감사 문서/JSON | 실제 로그·소스 SHA 확인 | 읽기 전용 비교 | 격리 보고서만 제거 |
| 구현 | 이미 적용/분기된 파일과 로그 판정 | Scripts/QA/MonsterAITargetReadiness.psm1, Scripts/CheckMonsterAITargetReadiness.ps1 | 원본 수정 없이 상태와 한계를 JSON으로 출력 | 실제 현재 소스 및 과거 B 로그 | 신규 도구 2개 제거 |
| 자동 검사 | 잘못된 상태·로그를 거부 | Scripts/Tests/MonsterAITargetReadinessTests.ps1 | 양성/음성 회귀와 실제 A/B 로그 통과 | 엔진 없는 PowerShell 테스트 | 신규 테스트 제거 |
| 문서 | 현재 결과·적용 범위 명시 | 결과·이전 결과 상단·DevLog·SprintChangeLog | 구현/검증/과거 기록 구분 | patch check·경로검사 | 문서 항목만 원복 |

## 도구 계약

- 파일별로 컨트롤러 baseline/prepared/drift, 테스트 missing/prepared/evolved/잘못된5개 목록을 구분한다. 파일이 달라졌다는 이유로 덮어쓰지 않는다.
- 로그는 정확한5개 성공, 중복·추가 케이스 없음, 엔진 오류 없음, 단일 `Automation Test Queue Empty 5 tests performed` 기록을 요구한다.
- OS 종료 코드가 제공되지 않으면 로그 완결성과 프로세스 합격을 구분한다. 과거 B 기록에 없는 종료 코드를 만들어 넣지 않는다.
- checker는 native 클래스가 abstract임을 기록하고 현재 concrete BP/ASC 소스 계약을 확인한다. 이 정적 검사는 UObject나 자산 로드의 대체가 아니다.
- 입력 파일 SHA를 검사 전후 대조한다. 새 엔진 실행·빌드·저장 읽기·공유 파일 변경은 없다.

## 통합과 남은 검증

새 Scripts 도구와 문서만 선택적으로 통합한다. 런타임 C++·테스트 CPP·에셋은 새 패치에 포함하지 않는다. 실제 새 빌드·UE 회귀가 필요하면 담당자의 단독 사용권과 소스/DLL 바인딩 증거를 먼저 확보한다. Boss 장판 최초 사용·cooked 가시성, 물리 입력·전체런은 계속 별도 검수다.
