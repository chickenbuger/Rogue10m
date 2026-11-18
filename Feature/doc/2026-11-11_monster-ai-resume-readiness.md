# 보스 AI 재개 확인 및 검증 도구 구현

## 현재 결과

타깃 소멸 수정은 이미 공유 컨트롤러에 통합됐다. 현재 테스트는 최초 후보의 추상 네이티브 캐릭터 생성을 concrete HumanMale BP와 PlayerState/ASC 초기화로 교정한 버전이다. 이 보완을 유지한다. 이번 작업은 격리 사본에서 실제 PowerShell 검사 도구·자동 회귀와 문서 갱신을 구현했으며 공유 런타임 파일은 바꾸지 않았다.

## 과거 실측과 이번 실행의 구분

- 과거 Engine_BossTargetA: 추상 클래스 생성으로4개 실패, 무타깃 이동만 성공. 이전 정적 리뷰에서 놓쳤던 문제이며 실패 기록을 보존한다.
- 과거 Engine_BossTargetB: 교정 후 실제 UE 자동화5개 성공, 표준 report warnings0/failed0. 로그의5개 Success와 queue5 기록을 이번 도구로 다시 검사했다.
- 과거 BossNaturalB: 자연 시야·추적·공격·정리112검사 실패0. 장판 첫 Niagara 컴파일 전 캡처·cooked 가시성·물리 입력·전체런은 이 합격에 포함하지 않는다.
- 현재 DLL은 과거 B의 DLL과 다르다. 이번에는 Editor·UBT·엔진 테스트를 실행하지 않았다. 현재 소스/DLL 조합을 새로 검증했다고 표현하지 않는다.

## 구현 파일

- `Scripts/QA/MonsterAITargetReadiness.psm1`: 파일별 기존 패치 적용 상태 분류, 정확한 테스트 목록과 완결 로그 검사. old source 패치가 현재 교정된 테스트와 다르면 `diverged`로 표시한다.
- `Scripts/CheckMonsterAITargetReadiness.ps1`: 현재 소스/abstract 지정·concrete BP/ASC 초기화·파일 존재·검사 전후 SHA 확인. 과거 로그를 읽기만 하고 source binding 미확인과 새 엔진 실행 없음도 기록한다.
- `Scripts/Tests/MonsterAITargetReadinessTests.ps1`: 엔진/Pester 의존성 없는 양성·음성 회귀. 소스 적용/부분/드리프트·빈 로그·누락/중복/추가/실패/Skipped·오류·잘린 큐·종료코드 부재·LF/CRLF를 검증한다.

엔진 없는 최종 자동 회귀49개가 모두 통과했다. 이 중2개는 실제 checker의 미통합/누락 파일 처리를 확인한다. 결과는 전달 폴더의 `test-runs/Final02/verification.json`이다. 실제 프로젝트 읽기 전용 검사는 `verification/current-readiness-final2.json`에 있다. 첫 current-readiness.json은 CRLF 정규식 고정 전의 실패를 보존한 파일이다. 도구를 수정한 뒤 같은 과거 로그를 새 파일로 검사했다.

## 통합 순서와 한계

1. 현재 런타임 C++와 교정된 테스트 CPP를 보존한다. 2026-11-10 옛 source 패치를 다시 적용하거나 테스트 파일을 되돌리지 않는다.
2. 이번 Scripts3개와 문서 패치를 검토한다. DevLog/SprintChangeLog 동시 추가분은 보존해 병합한다. 도구 테스트는 에디터·UBT 사용권 없이 격리 폴더에서 실행할 수 있다.
3. 읽기 전용 checker의 `ProjectRoot`, `PriorPacketRoot`, 새 `OutputPath`를 지정한다. 과거 자동화 로그를 주면 실제 실행 없이 완결성을 재검사한다.
4. 새 런타임 검증이나 승격이 필요할 때만 담당자와 단독 사용권을 조율하고 선택한 소스의 Editor 빌드 → lifecycle5개 → 관련 애니메이션/기하/자연AI 회귀를 실행한다. 성공 빌드와 테스트의 소스/DLL SHA를 함께 기록한다.

이번 도구는 소스·로그 검사다. C++ 컴파일, 자산 로드, 시간 경과 BT/perception, 실제3인칭 렌더·새 입력 검증을 대신하지 않는다. 공유 브랜치·소스·에셋·저장·빌드 프로세스·1인칭은 변경하지 않았고 커밋/푸시도 하지 않았다.

## 보스 AI·카탈로그 QA 통합과 현재 DLL 재검증 (UTC 2026-11-11)
- 이전 gameplay 패치가 이미 반영된 것을 확인해 중복 적용하지 않았다. 보스 readiness 3스크립트와 기능 문서, 카탈로그 실행기/격리 검사·문서만 공유 프로젝트에 통합했다.
- 보스 엔진 없는49/49 통과. BossResume1111 실제 타깃수명5/5, strict parser·실제 종료코드0·정확 queue1회 통과, 자연 AI112 assertions 실패0·기존445저장 보존이다. 자연AI 실행의 새 QA 저장2개는 격리 증거로 남았다.
- 자연AI 파동/장판 캡처2장을 직접 확인했다. 파동은 보이나 장판 효과는 사진에서 뚜렷하지 않으므로 시각 최종 완료로 승인하지 않는다. 기술 회귀와 구분한다.
- 카탈로그 엔진 없는8/8, CatalogQa11B의 실제4단계·6/6검사 성공. 원본447개 저장 변경0·예상 밖 신규0·DLL해시 동일·실제 테스트/종료 오류0. 카탈로그는 사전/테스트·종료 red가4단계 모두0이었다. 전리품의 사전 self-test15와 구분했다.
- 카탈로그20기술 콘텐츠/선택UI/production opt-in·저장이관 완료를 뜻하지 않는다. 이 작업은 실제 구현 소비경로와 정책/성장의 현재 회귀다. 1인칭 보류·커밋/푸시 없음.
- Notion 요약 후보: 이미 반영된 보스/카탈로그를 덮어쓰지 않고 QA 도구를 통합했다. 타깃5·자연AI112·카탈로그6의 현재DLL검사를 통과했으며 콘텐츠 제작과 최종 시각 검수는 별도 관리한다.
- 근거: tmp/work-boss-ai-target-loss-20261110/Engine_BossResume1111, Feature/doc/evidence/humanoid-boss-npc-20261110/BossResume1111, Feature/doc/evidence/catalog-consumption-20261110/CatalogQa11B, tmp/catalog-current-dll-20261111/verification.json.