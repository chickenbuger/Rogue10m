# 보스 처치·런 정산·마을 복귀 개발 결과

작성일: 2026-10-20. 작업 브랜치: `Sprint#5-39-boss-run-completion`.

상태: 기능 구현·Editor 빌드·렌더된 핵심 런타임 QA·기존 특성 효과 회귀·독립 정적 검토와 원본의 병행 작업을 보존하는 통합 완료. 필수 생성 경로 검사·정확한 67개 스테이징 대조·공백·PowerShell 구문 검사 후 기능 커밋 `c680f1ba`를 만들고 로컬 develop→test→main에 fast-forward 승격했다. 원격 push는 실행하지 않았다. 이 브랜치는 Sprint5의 main 병합 전에 만들어졌으므로 번호를 유지한다.

## 해결한 문제와 실제 동작

기존 아자토르는 죽을 때 경험치와 골드를 지급했지만 런 승리로 이어지지 않았다. 제한 시간 종료는 캐릭터를 죽였으나 보상을 저장하고 마을로 돌아가는 결과창이 없었고, 일반 캐릭터 사망도 런 종료와 연결되지 않았다.

이제 활성 런에서 실제 플레이어가 최종 보스를 처치하면 기존 처치 보상을 한 번 지급한 뒤 승리로 종료한다. GameMode 소유 `RunCompletionComponent`가 진행 저장과 결과 상태를 관리하며 네이티브 결과창에 성공/실패, 기존 보스 보상, 저장 상태와 마을 귀환 동작을 표시한다. 검증한 아자토르의 기존 보상은 경험치 3,000과 골드 300이다. 별도 클리어 보너스나 아이템을 추가하지 않았다.

결과창에서 Enter/Space 또는 귀환 버튼을 누르면 진행을 다시 저장하고 StartHub를 연다. 저장에 실패하면 현재 결과와 보상을 유지한 채 오류를 표시하고 이동을 막는다. 저장 경로가 복구된 뒤 같은 동작으로 저장을 재시도할 수 있으며, 재시도는 몬스터의 보상 지급을 다시 실행하지 않는다. StartHub에는 `StartRun` 옵션을 전달하지 않아 런은 대기 상태로 시작한다.

## 종료·저장·입력 계약

- `GameState`는 Running에서 최초 승리/패배만 확정한다. 이미 끝난 런의 결과 변경과 같은 월드에서의 재시작을 거절한다. 시간 초과·플레이어 사망·보스 사망이 이어져도 먼저 확정한 결과를 유지한다.
- 최종 보스 통지는 단일 플레이 권한, 활성 런과 양수 잔여 시간, 실제 사망, `FinalBoss` 등급, 유효한 로컬 플레이어와 살아 있는 Pawn을 확인한다. 일반 몬스터와 `CombatTestInstance`는 승리를 만들지 않는다.
- 보스의 기존 XP·골드 지급 이후 정산을 저장하고, 그 뒤 사망 델리게이트와 소멸을 이어간다. 기존 죽음 가드와 정산 가드가 중복 피해·통지의 보상 재지급을 막는다.
- 저장과 귀환은 기존 `ProgressionSubsystem`의 연결 Pawn·선택 프로필·저장 프로필 일치와 복원 유효성을 확인한다. 이번 기능은 SaveGame 버전과 스킬트리 경제를 변경하지 않았다.
- 결과창은 Controller의 입력 차단 창이며 UIOnly 포커스를 우선한다. 키 반복과 Escape/F6로 완료된 전투에 복귀하지 않도록 처리한다. 종료 후 플레이어·몬스터 피해도 거절한다.
- 새 맵의 Pawn에서 전투 중 LastStand 사용 상태가 초기화되는 것을 확인했다. 같은 맵의 완료 상태를 억지로 재시작하는 방식은 사용하지 않는다.

## 하네스 패킷과 변경 범위

[설계](../architect/2026-10-20_boss-run-completion.md)의 R1~R4 패킷으로 작업을 분리했다.

| 패킷 | 결과와 범위 | 현재 상태 |
| --- | --- | --- |
| R1 Architect / Implementer | GameState 종료 가드, GameMode 소유 RunCompletionComponent, 몬스터·캐릭터 사망 연결과 종료 후 피해 차단 | 구현·빌드·런타임 검사 완료 |
| R2 Implementer | 네이티브 RunResultWidget, Controller 입력·포커스, 저장 실패 표시·재시도·StartHub 귀환 | 실제 화면·Enter 귀환 검사 완료 |
| R3 Builder / Reviewer | 격리 QA 코드와 실행 스크립트, 고정 콘텐츠 빌드, 여섯 프로세스 검사와 독립 코드 검토, 기존 특성 fixture의 종료 계약 보완 | 핵심 QA 200개·특성 회귀 101개·정적 검토 완료 |
| R4 Doc Writer / Exit Gate | 결과 문서·한국어 DevLog·Sprint 기록, 범위 검사와 병행 작업 보존 통합 | 기록·원본 통합·최종 검사·기능 커밋·로컬 승격 완료 |

주요 변경은 `Core/Rogue10mGameState`, `Core/Rogue10mGameMode`, `Character/Rogue10mCharacter`, `Enemy/Rogue10mBasicMonster`, 새 `Components/Rogue10mRunCompletionComponent`, 새 `UI/Widgets/Rogue10mRunResultWidget`와 `Core/Rogue10mPlayerController`다. 검증은 새 `Tests/Rogue10mRunCompletionRuntimeTest.cpp`, `Scripts/ValidateRunCompletion.ps1`와 기존 특성 효과 fixture를 사용한다. 위 C++ 경로는 `Source/Rogue10m/` 기준이다. 새 애니메이션·VFX·Fab 반입이나 원본 `.uasset`/`.umap` 편집은 없다.

## 빌드와 실제 게임 검증

체크포인트 `207c595`와 같은 게임 소스·콘텐츠에 기능 변경을 적용하고, 문서 종료 기록만 추가된 `05f74f1`을 현재 기준으로 사용했다. Editor 최종 빌드는 성공했으며 90.27초가 걸렸다. [빌드 원문](evidence/boss-run-completion-20261020/editor-build-frozen.log)과 [고정 기준 대조](evidence/boss-run-completion-20261020/content-freeze.json)를 보존했다.

최종 검증은 D3D11, 1280×720, ScreenPercentage 75의 실제 렌더링 게임 프로세스에서 실행했다. `RenderOffscreen`과 숨김 실행으로 사용자 화면을 방해하지 않았으며 NullRHI는 사용하지 않았다. StartHub에서 별도 QA 프로필을 만들고 실제 포탈 캡슐 중첩으로 OpenRunMap에 진입했다. 북부 지역의 저장된 `BP_OW_FinalBoss` 아자토르에 플레이어 좌클릭 공격으로 마지막 피해를 주어 승리를 만들었다. 정상 경로는 `FinishRun(true)`를 검사 도구에서 직접 호출하지 않았다.

| 최종 사례 | 검사 수 | 결과 | 증거 |
| --- | ---: | --- | --- |
| 승리: 포탈·보스 처치·기존 보상·저장·Enter·마을 복귀 | 53 | 실패 0, exit 0 | [Write](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_victory/write-results.txt) |
| 승리 저장의 별도 프로세스 복원 | 12 | 실패 0, exit 0 | [Read](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_victory/read-results.txt) |
| 실제 저장 실패·귀환 차단·복구 후 Enter 재시도 | 58 | 실패 0, exit 0 | [Failure](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_failure/failure-results.txt) |
| 저장 재시도 결과의 별도 프로세스 복원 | 12 | 실패 0, exit 0 | [FailureRead](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_failure/failureread-results.txt) |
| 실제 타이머 만료·패배·입력·마을 복귀 | 33 | 실패 0, exit 0 | [Timeout](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_timeout/timeout-results.txt) |
| 실제 치명적 피해·플레이어 사망·패배·마을 복귀 | 32 | 실패 0, exit 0 | [Death](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_death/death-results.txt) |
| 합계 | 200 | 여섯 프로세스 모두 통과 | [실행기 원문](evidence/boss-run-completion-20261020/frozen-runtime-launcher.log) |

검사는 일반/테스트 몬스터의 승리 제외, 중복 피해와 사망 통지의 보상 불변, 종료 결과 변경·재시작 거절, 승리 후 치명적 피해 차단을 포함한다. Growth의 XP·레벨·처치 기록, 아이템 GUID와 골드를 포함한 Inventory, Combat 해금·바인딩, Town 특성·난이도의 전체 스냅샷을 하드 트래블과 별도 프로세스 재시작 후 정확히 비교했다. 저장 실패 사례는 기존 QA `.sav`에 Windows 읽기 공유 핸들을 열어 실제 쓰기를 막았으며 Saved 파일 바이트를 직접 편집하거나 파일/디렉터리를 교체하지 않았다. 핸들을 닫은 뒤 같은 보상 상태를 저장해 귀환했다.

[승리 화면](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_victory/run-result.png), [저장 실패 화면](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_failure/run-result.png), [시간 초과 화면](evidence/boss-run-completion-20261020/Rogue10m_Town_QA_RunCompletion_frozen1_timeout/run-result.png)을 직접 열어 결과·보상·저장 상태·귀환 안내의 가독성을 확인했다. [독립 코드 검토](evidence/boss-run-completion-20261020/review.md)는 종료 순서, 저장 실패, 입력, 피해 경계를 확인했으며 발견한 배경 정렬과 기존 fixture의 종료 계약 문제를 수정했다.

기존 Trait Effects 회귀도 101개 검사, 실패 0, exit 0으로 통과했다. [회귀 결과](evidence/trait16-runtime-20261011/Rogue10m_Town_QA_boss_completion_frozen1/effects-results.txt)에 최종 기록이 있다. 따라서 이번 검증은 렌더된 핵심 검사 200개와 기존 특성 회귀 101개, 총 301개다.

## 격리와 초기 실패에서 확인한 내용

원본에서 스킬트리와 체크포인트 작업이 진행 중이므로 별도 `tmp/boss-run-completion-worktree`에서 구현·빌드·게임 검사를 수행했다. 최초 Content 연결 방식에서는 Write의 예상 스냅샷이 20:40:43에 생성된 뒤 공유 대검 프로필과 Hybrid 기술 5개가 20:41:00에 바뀌었고, 20:41:22의 Read에서 기술 5개가 추가된 차이를 검출했다. 순서 차이로 처리하거나 스냅샷 비교를 완화하지 않았다.

이후 Content 연결을 제거하고 체크포인트의 콘텐츠 파일 6,417개를 별도로 고정했다. 콘텐츠 트리는 `890c83ca26eb5ac410bbd9b0d34e406c9a3c44e3`이며 복원 후 콘텐츠 diff는 0이다. 기준 소스 205개도 의도한 기능 변경을 제외한 차이가 없는지 대조했다. 초기 QA 프로필 이름이 기존 12자 제한을 넘은 문제와 보호된 입력 메서드를 직접 호출한 컴파일 오류도 수정했으며, 현재 QA는 실제 Controller 좌클릭과 Slate Enter 입력을 사용한다. 초기 실패 로그는 최종 고정 검증과 구분해 보존했다.

실행 스크립트는 각 자식 게임 프로세스에만 `UE_SKIP_UBT_SDK_SETUP=1`을 전달한 뒤 부모 환경을 복원한다. 이는 이미 Editor 빌드에서 확인한 Windows 플랫폼 SDK의 재탐색을 생략하여 다른 작업의 컴파일 대기를 피하는 설정이다. C++ 컴파일이나 Windows 빌드 검증을 생략한 결과로 해석하지 않는다.

## 검증의 한계와 통합 상태

이번 검사는 북부 지역으로 순간이동하고 기존 몬스터 AI를 정지한 통제된 게임 사례다. 이동 준비 중에는 임시 무적을 사용하고 종료 후 피해 차단 검사 전에 해제했다. 실제 보스 HP를 비치명적 피해로 1까지 낮춘 뒤 플레이어 공격이 마지막 피해를 주게 했으므로, 전체 보스전 난이도나 20분 필드 횡단을 인증하지 않는다. 타임아웃은 실제 타이머를 사용하되 남은 시간을 1초로 줄인 fixture다.

로컬 단일 플레이와 Editor Development 게임 실행을 검증했다. 전체 20분 밸런스, Shipping cook/패키징, D3D12와 장시간 GPU 안정성, 네트워크, 모든 무기·외형의 회귀는 별도다. 결과창 스크린샷 확인도 모든 해상도에 대한 UI 인증은 아니다.

이 기능의 순수 변경을 원본 작업 폴더에 3방향으로 반영했다. 기존 파일은 원본 바이트를 별도 백업한 뒤 병합했고 DevLog와 Sprint 기록은 다른 작업 뒤에 추가했다. 원본의 `Sprint#5-38-hybrid-skill-tree-runtime` 브랜치와 HEAD `05f74f1`은 바꾸지 않았으며 스킬트리 작업을 보존했다. [통합 증거](evidence/boss-run-completion-20261020/primary-integration.json)에 파일별 이전/결과 해시를 남겼다. 검증은 고정 기준에서 수행한 결과이며 원본의 미완성 스킬트리까지 통합 검증한 결과는 아니다. 필수 검사 후 기능 커밋 `c680f1ba689d643956c954023d306c45404c8a54`를 만들었다. 원격 fetch 결과 선행 로컬 main은 origin/main보다 3개 앞서고 0개 뒤처져 있었으며, 이후 develop/test/main의 조상 관계와 체크아웃 부재를 확인한 원자적 compare-and-set으로 세 참조를 기능 커밋까지 fast-forward했다. [로컬 통합 결과](evidence/boss-run-completion-20261020/local-integration.json)에 이전/이후 참조와 검사 결과를 남겼다. 이 최종 기록은 별도 문서 커밋으로 같은 순서로 승격한다.

공개 원격 게시에는 기존 체크포인트 채팅의 승인 대기 상태가 이어진다. 체크포인트 `207c595`와 문서 종료 `05f74f1`은 로컬에 있고 확인된 `origin/main`은 `b7ab6b6`이다. 그 채팅에서 자동 승인 검토가 저장소 소유 확인과 별개로 공개 코드·에셋·자료 전송의 명시 승인을 요구했다. 이번 기능에서는 push를 시도하지 않았으며, 그 선행 승인과 실제 전송 결과가 확인되기 전에는 원격 게시 완료를 주장하지 않는다.
