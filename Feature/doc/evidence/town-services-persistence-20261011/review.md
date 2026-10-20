# 마을 서비스·영구 진행 저장 독립 리뷰

2026-10-11 · Reviewer: `/root/qa` · 검토 위치: `tmp/npc-services-persistence-worktree`

현재 검토한 구현에서 조치가 필요한 차단 결함은 남아 있지 않다. 격리 빌드와 원본 프로젝트 통합 빌드 모두 서비스·저장 및 실제 포털·몬스터·드롭/줍기 검증을 통과했다. 아래 실행 수와 failures=0은 실제 증거 파일에서 확인했다. 1280×720 및 1920×1080 시각 검증과 원본 프로젝트의 추가 입력 공존 검증도 완료했고 원본 PNG를 직접 확인했다. 이 기능의 최종 검증에 남은 대기 항목은 없다.

## 구현 검토

- **상태 소유권:** 성장·카운터·특성은 PlayerState와 전용 컴포넌트, 장비·인벤토리는 Character의 InventoryComponent, 해금·바인딩은 CombatComponent가 관리한다. ProgressionSubsystem은 선택한 프로필 GUID별 스냅샷과 저장 수명주기를 관리한다. NPC와 위젯은 공개 서비스 API를 호출하며 비용·상한·배율은 Data Asset에 둔다. 저장 연결은 로컬 권한을 가진 Standalone 캐릭터에 한정한다 (`Core/Rogue10mProgressionSubsystem.cpp:128`).
- **난이도 적용:** BasicMonster는 BeginPlay에서 HP·기본 피해·경험치를 한 번 조정한다. AttackSkill 피해는 저장된 난이도 배율을 한 번 곱하고, 패턴·기본 공격은 이미 조정한 기본 피해를 사용한다. 캐릭터 공격력이나 복원된 능력치에 난이도를 다시 누적하는 경로는 발견하지 못했다 (`Enemy/Rogue10mBasicMonster.cpp:103`, `:273`).
- **실패한 저장·복원:** 버전·프로필·에셋·배치 검증 실패는 save-ready를 열지 않는다. Inventory는 임시 배열에서 전체 에셋·GUID·점유 셀·퀵슬롯 검증을 마친 뒤 커밋한다. 전체 복원은 이전 성장·인벤토리·특성·전투 스냅샷으로 되돌리며, 실패 후 SaveNow와 Detach가 손상 파일을 덮어쓰지 않는다 (`Components/Rogue10mInventoryPersistence.cpp:85`, `Core/Rogue10mProgressionSubsystem.cpp:151`, `:265`, `:284`).
- **프로필 분리와 보급 영수증:** 선택한 프로필이 있으면 영구 ClaimedRewards가 수령 여부의 기준이다. 같은 클래스의 프로필을 바꿔 재사용한 PlayerState의 임시 수령 기록은 다른 프로필에 적용하지 않는다. 임시 예약 키는 RewardId에 ProfileId GUID를 붙이고, 영구 영수증 ID는 원래 RewardId를 유지한다 (`World/Rogue10mTutorialNPC.cpp:276`, `:320`).
- **아이템 정체성과 서비스 제한:** 장착↔그리드↔드롭↔줍기 경로는 인스턴스 GUID와 강화 수치 전달을 확인했다. 서비스 진입은 열린 대화, 권한, 생존·거리·UI 상태, StartHub, 출발 전 run phase, 선택 프로필의 저장 준비 상태를 검사한다 (`World/Rogue10mTutorialNPC.cpp:384`, `World/Rogue10mDroppedItem.cpp:77`).

## 발견했던 P1과 적용된 수정

| 최초 문제 | 적용된 수정과 회귀 검증 |
| --- | --- |
| 같은 클래스 A→B 전환 시 A의 임시 보급 수령 기록 때문에 B의 첫 수령이 막힘 | 프로필의 영구 영수증만 조회하고 예약 키를 GUID별로 분리. B의 독립 수령 후 A의 전체 스냅샷과 영수증 복원을 native02 Read에서 확인. |
| 새 캐릭터의 기본 무기 타입과 실제 장착된 시작 무기가 달라 첫 저장부터 전투 바인딩 복원이 실패할 수 있음 | Combat.ApplyCharacterData가 실제 장착 MainWeapon 타입을 우선 사용. 조작 전 SaveNow→Detach→Attach와 네 스냅샷 동일성을 native02 Write에서 확인. |
| Combat.RestoreCombat가 여러 입력 슬롯에 같은 스킬을 둔 정상 기본 설정을 중복으로 거부 | 작성된 ChargedPrimary/ChargedSpecial 별칭을 허용. 입력 슬롯·에셋·해금·활성 트리 검증은 유지. native01에서 재현한 첫 복원 실패가 native02에서 해소됨. |

## 확인된 실제 실행

| 실행 | 결과 | 증거 |
| --- | --- | --- |
| native02 Write | 57 PASS, failures=0 | [write-results.txt](Rogue10m_Town_QA_native02/write-results.txt), [write.log](Rogue10m_Town_QA_native02/write.log) |
| native02 Read | 24 PASS, failures=0 | [read-results.txt](Rogue10m_Town_QA_native02/read-results.txt), [read.log](Rogue10m_Town_QA_native02/read.log) |
| failure01 | 27 PASS, failures=0 | [results.txt](Rogue10m_Town_QA_failure01/failures/results.txt) |
| final01 Write | 60 PASS, failures=0 | [write-results.txt](Rogue10m_Town_QA_final01/write-results.txt), [write.log](Rogue10m_Town_QA_final01/write.log) |
| final01 Read | 25 PASS, failures=0 | [read-results.txt](Rogue10m_Town_QA_final01/read-results.txt), [read.log](Rogue10m_Town_QA_final01/read.log) |
| finalrun01 Write | 42 PASS, failures=0 | [write-results.txt](../town-run-persistence-20261011/Rogue10m_Town_QA_Run_finalrun01/write-results.txt), [write.log](../town-run-persistence-20261011/Rogue10m_Town_QA_Run_finalrun01/write.log) |
| finalrun01 Read | 15 PASS, failures=0 | [read-results.txt](../town-run-persistence-20261011/Rogue10m_Town_QA_Run_finalrun01/read-results.txt), [read.log](../town-run-persistence-20261011/Rogue10m_Town_QA_Run_finalrun01/read.log) |
| finalvisual720warm | 9 PASS, failures=0 · 원본 PNG 1280×720 | [visual-results.txt](Rogue10m_Town_QA_finalvisual720warm/visual-results.txt), [town-services.png](Rogue10m_Town_QA_finalvisual720warm/town-services.png) |
| finalvisual1080 | 9 PASS, failures=0 · 원본 PNG 1920×1080 | [visual-results.txt](Rogue10m_Town_QA_finalvisual1080/visual-results.txt), [town-services.png](Rogue10m_Town_QA_finalvisual1080/town-services.png) |
| 원본 integrated01 Write | 60 PASS, failures=0 | [write-results.txt](Rogue10m_Town_QA_integrated01/write-results.txt), [write.log](Rogue10m_Town_QA_integrated01/write.log) |
| 원본 integrated01 Read | 25 PASS, failures=0 | [read-results.txt](Rogue10m_Town_QA_integrated01/read-results.txt), [read.log](Rogue10m_Town_QA_integrated01/read.log) |
| 원본 integratedrun01 Write | 42 PASS, failures=0 | [write-results.txt](../town-run-persistence-20261011/Rogue10m_Town_QA_Run_integratedrun01/write-results.txt), [write.log](../town-run-persistence-20261011/Rogue10m_Town_QA_Run_integratedrun01/write.log) |
| 원본 integratedrun01 Read | 15 PASS, failures=0 | [read-results.txt](../town-run-persistence-20261011/Rogue10m_Town_QA_Run_integratedrun01/read-results.txt), [read.log](../town-run-persistence-20261011/Rogue10m_Town_QA_Run_integratedrun01/read.log) |
| 원본 integratedvisual720 | 9 PASS, failures=0 · F8 UI 추가 후 입력 공존 | [visual-results.txt](Rogue10m_Town_QA_integratedvisual720/visual-results.txt), [town-services.png](Rogue10m_Town_QA_integratedvisual720/town-services.png) |

native02는 실제 NPC 구매·강화·특성·난이도, 조건 기반 스킬 해금과 바인딩, 이벤트 자동 저장, 독립 프로세스 재시작, 동일 클래스 프로필 분리와 보급 중복 방지를 확인했다. failure01은 실제 QA 저장 파일의 잘못된 버전과 성장 복원 후 누락 에셋 실패를 주입하여 네 상태의 롤백, NPC 서비스 차단, SaveNow/Detach 이후 파일 바이트 보존을 확인했다.

## 최종 통합 검증

- final01은 추가한 갑옷 강화의 골드 25 차감과 실제 GAS Defense +2를 통과했다. 실제 체력 물약을 퀵슬롯 0에 지정해 2로 이동한 뒤, 독립 프로세스 재시작에서도 슬롯 2의 물약·수량·인벤토리 참조와 슬롯 0의 해제를 확인했다. 기존 서비스·스킬·자동 저장·프로필 분리 검증도 다시 통과했다.
- finalrun01은 실제 캐릭터 캡슐과 포털 충돌로 StartHub→OpenRunMap을 여행했다. 새 World의 Pawn·PlayerState에서 성장·인벤토리·전투·특성 스냅샷이 복원되고 RunPhase가 Running이며 보급품·영수증이 중복되지 않음을 확인했다. 예상 스냅샷·보급 에셋은 StrongObjectPtr로 GC를 견디고, 여행 대기는 CoreTicker로 120초 제한한다.
- 같은 실행에서 실제 강화 무기를 DropGridItem→DroppedItem.TryPickup→재장착 경로로 이동하여 GUID·수량·강화 공격/방어 보너스를 보존했다. Hard 몬스터의 실제 GAS HP 120·패턴 기본 피해 15, ApplyDamage 사망의 경험치 30·골드 3·처치 기록 1회와 사망 후 중복 보상 거부를 확인했다. Read 프로세스에서 처치 후 전체 진행과 영구 보급 영수증이 복원됐으며, 이전 run timer는 재개되지 않았다.
- 최종 테스트를 포함한 격리 Editor 빌드는 80.86초에 성공했다 ([final-isolated-build.log](final-isolated-build.log)). 이후 열린 Editor의 DLL 잠금과 동시 UI 컴파일 문제를 해소하여 **원본 프로젝트 Editor 통합 빌드도 29.64초에 성공**했다 ([final-integrated-build.log](final-integrated-build.log)). 원본 DLL로 실행한 integrated01·integratedrun01 결과도 각각 60/25 및 42/15 PASS로 확인했다.
- QA 저장 접두사 길이 제한은 ProgressionSubsystem과 동일한 80자로 맞췄다. 수정한 테스트 파일의 `git diff --check`와 TownRun PowerShell 구문 검사는 통과했다. Reviewer는 결과 파일과 빌드 로그를 독립 확인했으며 빌드나 런타임 실행을 직접 수행하지 않았다.

## 원본 화면 독립 확인

두 PNG의 IHDR에서 각각 1280×720, 1920×1080 픽셀을 확인하고 원본 크기로 열어 검토했다. 구매·무기 강화·방어구 강화·활력·힘·쉬움·보통·어려움의 **8개 서비스**, 무기 강화 행의 선택 표시, 현재 골드 490·특성 0포인트·다음 탐험 보통 요약, ↑↓/Enter/Esc 조작 안내, 구매 완료 메시지와 마지막 F 보급품 행동이 모두 화면 안에 보인다. 쉬움·어려움의 XP 배율은 줄바꿈되어 있으나 문자가 잘리거나 다른 행과 겹치지 않는다. 한글 자형이 깨지지 않고 어두운 패널 위의 밝은 글자를 읽을 수 있다.

대화 패널은 오른쪽 약 22% 폭을 사용하며 중앙·왼쪽 플레이필드를 유지한다. 마지막 행동까지 패널 안에 들어가고 아래 체력·스태미나·스킬 HUD와 겹치지 않는다. 실제 입력 스택의 Enter 서비스 실행, Down 선택 이동, Esc 닫기도 두 해상도에서 통과했다.

첫 `finalvisual720` 실행은 차가운 SM5 셰이더 준비 중 240초 제한시간을 넘겼으며 [원래 로그](Rogue10m_Town_QA_finalvisual720/visual.log)를 보존했다. 제한을 600초로 둔 warm 재시도에서 통과했다. 720p PNG 좌상단에는 임시 `Preparing Shaders (1)` 표시가 남아 있지만 서비스나 HUD를 가리지 않는다. 1080p PNG에는 해당 준비 표시가 없다.

원본 프로젝트의 integratedvisual720 PNG도 직접 열어 확인했다. 화면 우상단에 동시 작업으로 추가된 F8 몬스터 테스트 버튼이 보이는 상태에서 NPC의 Enter 구매, Down 선택, Esc 닫기가 모두 통과했다. 8개 서비스·요약·마지막 행동은 기존 배치를 유지하고 잘림이나 HUD 겹침이 없으며 셰이더 준비 표시도 없다.

## 원본 소스 통합 보존 확인

[source-integration.json](source-integration.json)의 34개 소스 항목을 확인했다. 32개는 통합 기능 SHA-256과 원본 폴더의 최종 SHA-256이 일치하고, BasicMonster.h/.cpp 2개만 `PreservedConcurrentAddition=true`로 기록된 동시 추가 차이가 있다. 실제 원본 파일을 다시 해시해 기록의 FinalRootSha256과 비교한 결과 34개 모두 일치했다.

BasicMonster의 `bCombatTestInstance`는 기본 false이며 명시적으로 표시한 테스트 배우에만 보상 억제를 적용한다. 일반 배우의 난이도·보상 경로가 유지됨을 소스와 원본 integratedrun01의 실제 처치 보상으로 확인했다. 원본 브랜치는 다른 작업이 소유한 `Sprint#5-26-monster-test-ui`를 유지하며 Reviewer는 브랜치나 소스를 변경하지 않았다. 원본 작업 폴더의 [generated-check.log](generated-check.log)는 `Harness path check passed.`로 끝난다.
