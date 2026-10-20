# 현재 상태 main 통합

- 요청일: 2026-10-20.
- 작업 브랜치: `Sprint#5-37-main-checkpoint`.
- 대상: 누적 전투/애니메이션·카메라·HUD·특성·NPC/저장·월드/몬스터 작업, 원본과 검증 자료, Studio 제거, 설계/개발 문서, Fab/Unreal 설정 가이드.
- 신규 변경: AGENTS와 Git/Lazy 지침에 기능 완료 후 커밋·develop→test→main 통합·푸시의 상시 승인을 반영했다.
- 계획: [통합 계획](../architect/2026-10-20_main-checkpoint.md).

## 검증과 통합 결과

- 프로젝트 파일 생성 성공: 16.47초. Android SDK 및 VS2022의 .NET 10 C# 프로젝트 지원 안내가 있었으나 생성은 성공했다.
- `Scripts/BuildEditor.ps1 -NoHotReload` 성공: 77 actions, 144.03초. XGE 라이선스 안내 후 standalone 모드로 컴파일·링크를 완료했다.
- `Scripts/ValidateTownRun.ps1 -Label checkpoint20261020 -TimeoutSeconds 180` 성공: 격리 QA 저장 슬롯으로 별도 Write/Read 두 프로세스가 모두 exit 0, failures 0. 실제 마을/NPC·포탈·Hard 몬스터 처치/보상·아이템 드롭/회수·재시작 저장 복원을 검증했다.
- 스테이징 `CheckGeneratedChanges.ps1` 통과. 활성 소스·설정·스크립트·문서의 `git diff --cached --check` 통과. 전체 공백 검사에서 기존 증거 3파일의 diff 문맥·Markdown 줄바꿈 239건이 보고되어 원문을 보존했다.
- 파일/원격/소스/지침 독립 검토 완료. 초기 대상에서 100MiB 초과 파일이나 고신뢰 자격 증명 후보·중첩 저장소·생성 캐시 게시 대상을 발견하지 못했다.
- 커밋·로컬 승격·원격 게시 결과는 실제 수행 뒤 아래에 추가한다.

## 변경 파일

- [전체 파일 목록과 설명](evidence/main-checkpoint-20261020/commit-files.txt).
- `Source/`, `Content/`, `Art/`, `SourceArt/`, `Scripts/`: 기존 누적 구현·에셋·원본·도구와 검증 코드를 보존한다.
- `Config/`, `Rogue10m.uproject`: 기존 에디터/플러그인 설정을 보존한다.
- `AGENTS.md`, `Docs/`, `Feature/`, `DevLog/`: 기존 기록과 이번 상시 승인·통합/검증 기록을 포함한다.
- `Studio/`, `OpenPersonalStudio.bat`, `Scripts/OpenPersonalStudio.ps1`: 기존 사용자 요청에 따른 제거를 포함한다.

## 근거와 한계

- [독립 검토](evidence/main-checkpoint-20261020/review.md), [Editor 빌드](evidence/main-checkpoint-20261020/editor-build.log), [마을/저장 QA 실행](evidence/main-checkpoint-20261020/town-run-launcher.log).
- [Write 결과](evidence/town-run-persistence-20261011/Rogue10m_Town_QA_Run_checkpoint20261020/write-results.txt), [Read 결과](evidence/town-run-persistence-20261011/Rogue10m_Town_QA_Run_checkpoint20261020/read-results.txt).
- 이번 QA는 headless 통합 검사다. 이번에 렌더/시각·모든 기존 기능 회귀·Shipping/패키징·네트워크를 새로 인증하지 않았다. 기존 기능별 증거는 각 문서에 보존했다.
- 캐릭터 교체 실패 시 기존 Pawn의 자동 저장 연결 복구는 독립 검토에서 후속 점검으로 남겼다. 현재 상태 보존 범위를 넘는 게임 코드 수정은 추가하지 않았다.
- 검증 완료 뒤 다른 채팅이 새 스킬트리 구현을 시작했다. 사용자의 추가 선택에 따라 최초 스테이징 blob으로 검증 당시 코드를 고정하고 이후 구현은 이번 게시에서 제외한다. 별도 인덱스와 fast-forward 참조 승격으로 공유 작업 파일을 보존하며, 현재 작업 브랜치의 기준만 게시 체크포인트에 맞춘다.
