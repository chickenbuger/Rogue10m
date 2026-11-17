# 게임 종료 UI 구현·검증 기록

메인 메뉴의 즉시 종료를 확인창으로 바꾸고, 캐릭터 선택·설정·탐험 결과 화면에도 종료 버튼을 추가했다. 플레이 중에는 기존 설정 키인 `Esc / O / F10`으로 설정을 열어 종료할 수 있다. 종료 확인 전 저장에 실패하면 앱을 유지하고 재시도 또는 취소를 제공한다.

이 문서는 격리 사본에서 확인한 결과와 원본 반영·배포 상태를 구분한다. 자동 QA 254개와 독립 게임 창의 네이티브 마우스·Esc 검증을 통과했고 원본 Source에도 반영했다. 새 Windows 패키지 검증·배포는 아직 남아 있다.

## 구현 범위

- 기존 메인 메뉴 `UI_QuitGameButton`을 공통 종료 확인 흐름에 연결했다. 캐릭터 선택·설정·결과 화면은 오른쪽 위 공통 종료 버튼을 사용한다.
- C++ UMG 위젯을 사용하며 `.uasset`이나 맵은 변경하지 않았다. 종료 오버레이 ZOrder는 `2000`으로 메인 메뉴(`1100`)와 캐릭터 선택 화면(`1000`)보다 높다.
- 기본 초점은 **취소**다. `Esc`, `Backspace`, 게임패드 B는 확인창만 취소한다. 확인창 아래 버튼이나 게임 입력이 실행되지 않도록 UI 전용 입력 모드를 적용한다.
- 확인창을 열 때 월드·pawn·선택 프로필·부착 프로필·런 ID를 저장하고 종료 직전에 다시 검사한다. 런 단계 전환, 패널 전체 닫기, `EndPlay`에서는 확인 상태를 해제한다.
- 정상 부착 세션은 종료 직전 `SaveNow()` 성공이 필요하다. 저장 실패 시 게임을 종료하지 않고 같은 확인창에서 재시도·취소할 수 있다. 선택·부착 프로필이 모두 없는 기본 pawn은 불필요한 진행 저장 없이 종료할 수 있다.
- 탐험 결과 화면은 정산 및 정산 저장 완료를 확인한다. 정산 저장이 끝나지 않았다면 결과 화면의 기존 마을 복귀·저장 재시도 경로를 안내한다. 종료 UI가 보상 정산을 새로 수행하지 않는다.

주요 파일은 `Core/Rogue10mPlayerControllerQuit.cpp`, `UI/Widgets/Rogue10mQuitMenuWidget.{h,cpp}`, 기존 Controller 연결부와 메인 메뉴 종료 handler다. 검증 fixture는 `Tests/Rogue10mQuitMenuRuntimeTest.cpp`, 실행기는 `Scripts/VerifyQuitMenu.ps1`이다.

## 기존 런·저장 정책

이번 변경은 런 중단·재화·보상 정책을 바꾸지 않는다. 정상 v5 입장 마커와 유효한 입장 기준잔액이 있는 미완료 런을 다음 `StartHub / WaitingToStart`에서 열면 기존 복구 코드가 중단 런을 처리한다.

- 런에서 얻은 추가 아이템, 성장, 습득 전투·스킬 트리 상태와 런 단위 NPC 영수증은 초기화한다. 새 pawn에 설정된 기본 인벤토리를 복구하므로 기본 지급 무기가 인벤토리에 남을 수 있다. 기본 장착을 Knuckle로 고정하지 않는다.
- 골드·크리스털·로비 적립액은 입장 비용을 지불한 뒤의 기준으로 복원한다. 입장 비용은 환불하지 않으며 중단에 따른 새 로비 보상을 지급하지 않는다.
- 발견 기록과 영구 마을 강화는 보존한다. 구버전·수동 시작·잘못된 기준 기록에는 동일한 복구 결과를 보장하는 문구를 쓰지 않는다.
- 필드에서는 설정·종료 확인창을 열어도 시간과 전투가 진행된다. 안전한 Hub 대기 상태의 기존 메뉴 일시정지만 유지한다. 필드 확인 중 사망·시간초과로 런 단계가 바뀌면 종료 확인을 닫고 기존 결과 흐름으로 넘긴다.

## 격리 및 동시 작업 보호

기존 `D:/R10/Parallel1111A/Rogue10m`의 ArchiveE 대응 체크포인트를 `D:/R10/QuitUI1111A/Rogue10m`으로 물리 복사했다. 원본 Content 연결이나 공유 저장 폴더는 사용하지 않았고, 기존 `ArchiveE/Windows` 배포본은 보존했다. 새 종료 UI 작업은 로컬 작업 브랜치 `Sprint#9-32-game-quit-ui`에서 진행했다.

검증은 전용 `Rogue10m_*_QA_Quit_<Label>` 저장 이름을 사용했다. launcher는 격리 저장, `D:/Project/Rogue10m/Saved/SaveGames`, Windows 기본 저장 위치의 기존 `.sav` 해시를 전후 비교한다. Restore 회차에서 발생하는 해당 QA 프로필·진행 저장의 정상 변경만 별도로 허용하고 기록한다. 다른 프로세스는 중단하지 않는다.

## 확인된 검증 결과

증거 기준 폴더: `D:/R10/QuitUI1111A/Rogue10m/Feature/doc/evidence/quit-menu-20261111`.

| 회차 | 경로 | 네이티브 검사 | 프로세스·저장 결과 |
| --- | --- | --- | --- |
| MenuB | `QuitMenuB1111` | 36개, 실패 0 | 실제 메인 메뉴 종료 handler·확인/취소, `QuitGame` 종료 코드 0, `OnPreExit`, 기존 저장 보존 확인 |
| HubB | `QuitHubB1111` | 27개, 실패 0 | Hub 종료·저장, 실제 종료 코드 0, `OnPreExit`, 기존 저장 보존 확인 |
| FailureB | `QuitFailB1111` | 54개, 실패 0 | QA 저장 파일 읽기 전용 실패 주입, 종료 차단·오류 표시·취소·재시도 성공 후 실제 종료 코드 0, 기존 저장 보존 확인 |
| RunC | `QuitRunC1111` | 54개, 실패 0 | 필드 시간 진행·취소·명시 저장·중단 마커·실제 기본 인벤토리 기준 보존, 실제 종료 코드 0, `OnPreExit`, 기존 저장 보존 확인 |
| RestoreC | `QuitRunC1111_restore` | 25개, 실패 0 | 별도 프로세스의 실제 Hub 재접속·중단 복구·재저장·종료, 실제 종료 코드 0, `OnPreExit`, 허용된 해당 QA 변경 외 기존 저장 보존 확인 |
| TerminalB | `QuitTermB1111` | 58개, 실패 0 | 확인 중 런 종료에 따른 모달 해제·정산 보호·결과 화면 종료, 실제 종료 코드 0, `OnPreExit`, 기존 저장 보존 확인 |

선정한 여섯 모드의 합계는 **254개 검사, 실패 0**이다. 최신 Editor 빌드는 `D:/R10/QuitUI1111A/build-editor-policy.log`에서 `Succeeded`, 17.38초를 기록했다.

MenuB·FailureB·RunC·TerminalB는 오프스크린 렌더링 PNG를 남겼다. HubB와 RestoreC는 NullRHI 검증이다. 위 검증은 실제 UMG delegate와 일부 프로그램 방식 Slate 키 입력을 사용했으며, 사람이 마우스·키보드·게임패드를 조작한 검증이나 전체 런 플레이를 뜻하지 않는다. 결과 파일·launcher receipt·`exit_callback.txt`를 함께 확인해 단순 종료 요청과 실제 프로세스 종료를 구분했다.

RestoreC는 RunC가 저장한 동일 프로필·런의 실제 디스크 스냅샷으로 별도 프로세스를 시작했다. 종료 전 런 골드 `20`이 저장된 뒤, 재접속 시 입장 기준인 골드 `13`, 크리스털 `0`, 로비 적립액 `0`으로 복원됐다. 성장 레벨 `1`, 경험치 `0`, Brawler 숙련 `0`을 확인했고, 새 GUID를 제외한 기본 인벤토리의 자산·수량·배치·장착 슬롯이 실제 시작 기준과 같았다. 기본 장착은 시작·복구 모두 `Unarmed`였으며 런에서 얻은 GreatSword는 사라졌다(`restored_has_greatsword=0`). 새 런 ID, 이전 런의 단일 정산 표식, 발견 기록 보존도 확인했다. RestoreC 마지막 종료에서는 테스트가 추가한 골드 `7`까지 반영해 다시 `20`이 저장된 뒤 실제 종료했다.

## 네이티브 창 입력 검증

별도로 CUA에서 이 Work가 시작한 `UnrealEditor.exe -game`의 독립 게임 창 `2297634`, PID `56936`만 선택해 확인했다. 메인 메뉴의 종료 버튼을 네이티브 마우스로 누르면 확인창이 열렸고, 마우스 취소 후에는 메인 메뉴로 돌아가며 아래 버튼이 눌리지 않았다. 다시 열어 네이티브 `Esc`로 취소한 뒤, 재차 열어 마우스로 종료 확인을 누르자 해당 게임 창과 프로세스가 닫혔다. 공유 Editor 창 `7474982`는 그대로 유지됐다. 이 검증에서는 프로필을 만들지 않았다.

`D:/R10/QuitUI1111A/NativeQuitB.log`의 1598행은 `no_attached_session`, 1599행은 종료 확인, 1602행은 `RequestExitWithStatus(0, 0)`, 1644행은 `LogExit: Exiting`을 기록했다. launcher 식별 정보는 `NativeQuitB-launcher.json`에 남겼다. 이는 **Editor 바이너리의 standalone `-game` 실행 검증이며 Windows 패키지 실행 검증은 아니다.** 실제 하드웨어 게임패드 조작과 사람의 전체 런 플레이는 수행하지 않았다.

## 실패·수정 기록

- 첫 C++ 컴파일의 C4458은 로컬 변수 `Character`가 기반 클래스 이름을 가린 문제였다. `PawnCharacter`로 수정했고 최신 `build-editor-policy.log`의 Editor 빌드도 통과했다.
- 오래된 test object가 재사용된 것을 확인해 fixture를 강제 재빌드했다. 초기 FailureA 실행은 이 진단 과정에서 직접 시작한 정확한 PID `32436`만 종료했다. 해당 실패 증거는 보존했다.
- RestoreB는 17개 검사 중 1개가 실패했다. fixture가 중단 런의 무기를 제거한 뒤 Knuckle을 기본 장착해야 한다고 잘못 가정했다. 실제 현재 기본 장착은 Unarmed이고 기본 무기는 기본 인벤토리에 들어 있다. 게임 정책을 바꾸지 않고, 시작 시 실제 기본 상태를 기록해 GUID를 제외한 인벤토리 의미값과 비교하도록 fixture를 수정했다.
- RestoreB launcher의 `preserved=false`는 해당 회차의 격리 QA 프로필 저장 파일이 정상 변경된 것을 허용하지 않아 발생했다. receipt의 변경 대상은 `Rogue10m_Town_QA_Quit_QuitRunB1111.sav` 한 개이며, 이를 원본 사용자 저장 손상으로 해석하지 않는다. launcher는 해당 Restore QA 이름만 허용하는 `IsExpectedQA`와 `expectedQAChanges` 기록으로 보완했다.
- 첫 네이티브 검증 시도는 숨겨진 `UnrealEditor-Cmd` 창이라 입력 대상을 선택할 수 없었다. 해당 시도는 UI 검증으로 계산하지 않았다. 시작 시각까지 대조한 이 Work 소유 PID `57264`만 정리한 뒤 위의 보이는 standalone 게임 창에서 다시 검증했다.

## 남은 검증·반영 상태

- **완료 — 원본 Source 통합:** 2026-11-11 17:40 UTC에 `D:/Project/Rogue10m`에 scoped source patch를 적용하고 reverse `git apply --check`까지 통과했다. 기존 세 파일은 `C:/Users/PC/Documents/Codex/2026-11-12/task/QuitUI_IntegrationBefore`에 백업했다. receipt 상태는 `integrated_without_shared_build_or_git_branch_mutation`이다. 사용자 브랜치 `Sprint#9-32-landscape-world-blockout`, 다른 변경과 에디터를 유지했다. 원본 Editor/Game 바이너리는 재빌드하거나 재시작하지 않았다. 다른 Work의 B 사본은 source patch 읽기 전용 preflight만 통과했으므로 그 바이너리에는 UI가 포함되지 않은 상태로 취급한다.
- **PENDING — Windows 패키지·배포 검증:** 이번 Work의 `D:/R10/QuitUI1111A/Package/package-result.json`은 다른 빌드가 진행 중이어서 preflight에서 차단됐다. `sdkExit`와 `buildCookRunExit`는 `null`이고, 여기서는 새 Windows archive를 만들지 않았다. 기존 ArchiveE는 종료 UI가 없는 이전 배포본이며, 다른 Work의 B 스냅샷도 종료 UI 반영 전에 동결됐다. 패키징 Work에서 종료 UI Source 패치를 포함한 뒤 Editor/Game 재빌드·cook·package를 수행하고, 그 새 패키지에서 마우스·키보드 종료/취소를 확인해야 한다. 위 standalone 네이티브 입력 통과를 이 단계의 완료로 대체하지 않는다.
- **PENDING — 최종 Git 상태:** 공유 원본의 브랜치 전환·commit·push·승격은 아직 수행하지 않았다. 미완료 검증을 완료로 표시하지 않는다.

## 재현

격리 프로젝트를 해당 Source로 Editor 빌드한 뒤 새 label을 사용한다.

```powershell
./Scripts/VerifyQuitMenu.ps1 -ProjectRoot 'D:\R10\QuitUI1111A\Rogue10m' -Label '<fresh-label>' -Mode Menu -Rendered
./Scripts/VerifyQuitMenu.ps1 -ProjectRoot 'D:\R10\QuitUI1111A\Rogue10m' -Label '<another-fresh-label>' -Mode Failure -Rendered
./Scripts/VerifyQuitMenu.ps1 -ProjectRoot 'D:\R10\QuitUI1111A\Rogue10m' -Label '<fresh-run-label>' -Mode Run -Rendered
./Scripts/VerifyQuitMenu.ps1 -ProjectRoot 'D:\R10\QuitUI1111A\Rogue10m' -Label '<same-run-label>' -Mode Restore
```

Restore는 바로 앞 Run과 같은 QA label을 사용한다. 기존 label의 증거·저장은 덮어쓰지 않는다. 최종 통합은 `Scripts/IntegrateQuitUI.ps1`의 입력·대상 충돌 및 patch preflight를 먼저 통과해야 한다.
