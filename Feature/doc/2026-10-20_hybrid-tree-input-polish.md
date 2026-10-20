# 혼합 스킬트리 도형·실입력 보완 결과

2026-10-20. 작업 `Sprint#7-2-hybrid-tree-input-polish`, 최종 검증 기준 main `e12df414`. 이전 검토에 남겨 둔 노드 표식과 실제 입력 검증을 완료했다.

## 달라진 동작

아이덴티티 입문·심화·성질은 마름모, 공명은 이중 원으로 구별하며 보이는 외곽까지 클릭된다. 원형 몸체와 선택·hover를 유지하고 라벨은 외곽에서9px 이상 띄웠다.

기존 K 창이 실제 HUD 공격 슬롯을 덮어 드래그할 수 없던 부분을 고쳤다. 전수 공격을 끌면 창을 흐리게 하고 하단 HUD를 실제 드롭 대상으로 노출한다. 놓기/거절/취소/창 닫힘/컨텍스트 해제 때 표시를 복원하며 이 패널이 소유한 Slate operation만 종료한다. 드래그 중 메뉴의 일시정지·전투 입력 차단은 그대로다. 기술명과 기존 아이콘을 드래그 표시로 사용한다.

초기 외부 격투가 개념도에 남아 있던 마나3노드와 간선·설명을 제거했다. 내공·투지2계열 대표 경로이며 실제53노드 게임 화면과 구분한다. 원본 백업과16노드·17간선·JS/선택 상태 검사를 보관했다. 성장 수치·전수 원장·전투 실행 API·SaveGame 버전·콘텐츠 저작은 바꾸지 않았다.

## 검증

| 검증 | 결과·증거 |
| --- | --- |
| 최신 통합 Editor 빌드 |103.04초·11 actions 성공, [build-02.log](evidence/hybrid-tree-input-polish-20261020/build-02.log). 기존 Incredibuild license 경고1건은 standalone 실행 안내이며 compile 실패는0이다. |
| 기존 자동화 |3 Success, 오류/경고 결과0. [Automation index](evidence/hybrid-tree-input-polish-20261020/automation-report/index.json). |
| 실제 게임 Write |176 PASS·0 FAIL·exit0, baseline/input 두 성공 marker. [결과](evidence/hybrid-tree-input-polish-20261020/runtime-results.txt). |
| 새 프로세스 Read |77 PASS·0 FAIL·exit0. 기존5영역·슬롯/키 저장 복원 뒤 대검 UI/입력 재실행. [결과](evidence/hybrid-tree-input-polish-20261020/runtime-read-results.txt). |
| 실제 화면 |격투가1080p·대검720p, drag 중 HUD 노출·드롭 후 원상 복원 PNG와 [화면 검토](evidence/hybrid-tree-input-polish-20261020/visual-review.md). |
| 독립 검토 |[코드·실행 검토](evidence/hybrid-tree-input-polish-20261020/review.md), [콘텐츠 복원 검토](evidence/hybrid-tree-input-polish-20261020/incident-review.md). |

새 검사는 실제 Slate pointer down/move/up과 키 선택기 key event, 플레이어 입력 press/release를 통과한다. 두 직업의 실제 HUD Primary 드롭, E 회피 슬롯 거절, 잠김 drag 불가, 외곽 클릭, drag 도중 창 비활성화에 따른 release 이전 취소, 표시·선택 복원, Esc/예약키/해제/재지정, 정확한 기술 사용 횟수1회·쿨다운을 확인했다. 델리게이트/OnDrop 직접 호출로 성공을 대체하지 않았다. QA는 세 전용 저장 접두사를 검증한 독립 프로필만 사용한다.

OS 입력 장치 자체나 모든 충전 기술·화면 배율·20분 전체 플레이·Shipping·멀티 검증으로 확대하지 않는다. 초기 PolishRun01의 숨은 장비 미리보기 선택에 따른5실패는 [원문](evidence/hybrid-tree-input-polish-20261020/runtime-results-attempt-01.txt)에 남겼다. 실제 MainHUD 소유/Slate ancestry와 프레임 간 입력으로 고친 뒤 최종 검증을 실행했다.

## 작업 중 콘텐츠 연결 오류와 복원

작업 폴더의 Content junction 구성을 제가 잘못해 sparse checkout 정리가 공유 원본의 추적 에셋6,423개를 삭제했다. 즉시 사용자에게 알리고 사과했다. 전체 복원은 미커밋 에셋을 덮을 위험으로 자동 승인 검토가 거절하여, 읽기 감사로 실제 부재 경로만 확정한 뒤 존재 여부를 다시 확인하고 그 파일만 복원했다. 기존 존재 파일 덮기0, 원본 Content diff0을 확인했다.

독립 검토에서6,423개·4,480,714,917B를 전량 검사했다. raw blob6,412개가 일치하고 텍스트11개도 CRLF→LF 이외 차이 없이 clean-filter blob이 일치해 불일치0이다. Root의 알려진 보스3파일과 기존 스킬트리4파일도 예상 코드와 대조했다. 사고 전 Root Source/미추적 파일 전체 해시가 없으므로 작업 내내 원본 전체가 불변이었다는 주장은 하지 않는다.

이후 콘텐츠까지 실제 독립 checkout으로 전환해 위 최종 빌드·Write/Read를 다시 실행했다. AGENTS와 Lazy Harness에 추적 Content를 다른 checkout으로 junction/symlink 연결하지 않는 규칙을 추가했다. [감사](evidence/hybrid-tree-input-polish-20261020/content-recovery-audit.json), [복원](evidence/hybrid-tree-input-polish-20261020/content-recovery-result.json), [독립 검토](evidence/hybrid-tree-input-polish-20261020/incident-review.md)를 보존한다.

## 통합과 게시

기능 커밋 `e6269dfab273b7eb36feaae64804434050cc1a0d`의48개 명시 파일을 스테이징 대조하고 생성 경로·공백·JSON·25개 문서 링크·append-only 로그·고신뢰 비밀 패턴 검사를 통과했다. 최신 원격과 조상/worktree 점유를 확인한 뒤 develop→test→main CAS fast-forward와 일반 atomic push를 완료했다. 실제 원격 작업/develop/test/main4개 SHA가 모두 기능 커밋과 같음을 직접 확인했다.

[명시 변경 목록](evidence/hybrid-tree-input-polish-20261020/commit-files.txt), [최종 검사](evidence/hybrid-tree-input-polish-20261020/final-checks.json), [통합](evidence/hybrid-tree-input-polish-20261020/local-integration.json), [푸시 원문](evidence/hybrid-tree-input-polish-20261020/public-push.log), [원격 검증](evidence/hybrid-tree-input-polish-20261020/public-push-result.json)을 참조한다. 최종 Source tree는 `fc0d959160125e9aea1fad27e40e23a39f2d6397`, Content tree는 복원 검증한 `561dc863470c130a9b5fe511c6f9c68dd8f74b41`이다. 공유 root의 병행3개 Source 변경을 건드리지 않고 별도 worktree의 기능을 게시했다. 이 게시 결과와 증거만 문서 커밋으로 기록한다.
