# 마을 NPC 서비스와 캐릭터 진행 영구 저장 개발 결과

작성일: 2026-10-11

작업 브랜치: `Sprint#5-24-npc-services-persistence`

동시 개발 반영: 본 기능은 별도 `Sprint#5-24-npc-services-persistence` 작업 폴더에서 구현했고, 원본 반영 전 파일별 기준 사본 대조와 백업을 수행했다. 원본에서는 다른 작업이 계속되므로 완료 기록 시점의 `Sprint#5-26-monster-test-ui` 브랜치를 유지했다. 해당 브랜치를 본 기능 작업 브랜치로 임의 전환하지 않았다.

상태: 완료. 분리 작업 폴더와 원본의 빌드·서비스/저장·포탈/런 검사가 모두 통과했고, 두 해상도의 화면·입력 및 원본 생성 출력 검사도 통과했다. 최신 원본 F8 입력과 NPC의 Enter/Down/Esc 입력을 함께 사용하는 화면 검사도 통과했다.

## 문제와 변경 결과

기존 안내 NPC는 대화와 최초 출발 보급을 제공했지만 아이템 강화, 특성 강화, 난이도 선택을 실행할 수 없었다. 성장과 인벤토리는 Pawn/PlayerState에만 남아 하드 트래블과 재시작을 넘기지 못했고, 보급품만 세션 Subsystem이 별도로 운반했다.

기존 아린 대화에 물약 구매, 장착 주무기/몸통 방어구 강화, 활력/힘 강화와 쉬움/보통/어려움 선택을 연결했다. 프로필 GUID별 SaveGame이 성장, 수련/토벌 횟수, 해금과 공격 슬롯 배치, 장비/아이템/가방/재화/소모품 슬롯, 개별 강화, 특성, 선택 난이도와 보급 수령 기록을 저장한다.

기본 수치와 조작은 [사용 가이드](../../Docs/TownServicesPersistenceGuide.md)에 정리했다. F로 대화를 열고 ↑↓로 선택한 뒤 Enter로 서비스를 실행한다. 마을에서 살아 있으며 출발 전인 캐릭터만 서비스를 이용할 수 있고, 실패 사유와 저장 오류는 대화창에 표시한다.

기본 물약은 10골드다. 강화는 다음 단계 × 25골드, 최대 +5이며 주무기는 단계마다 공격 +2, 몸통 방어구는 방어 +2를 얻는다. 레벨 상승으로 얻은 포인트를 활력(최대 HP +10) 또는 힘(공격 +2)에 사용하며 각 특성 상한은 5단계다. 어려움의 몬스터 HP/피해/XP 배율은 1.5/1.25/1.5, 쉬움은 모두 0.75, 보통은 모두 1이다.

## 데이터 소유권과 저장 경계

| 소유자 | 책임 |
| --- | --- |
| PlayerState | GAS 성장 수치, 기술 사용/몬스터 처치 횟수, TownProgressionComponent 소유 |
| TownProgressionComponent | 특성 단계/실제 누적 보너스, 다음 탐험 난이도/실제 배율 |
| InventoryComponent | 장비·그리드·기존 슬롯·가방·재화·퀵슬롯, 개별 아이템 강화 |
| CombatComponent | 해금 스킬과 실제 공격 슬롯 배치 |
| ProgressionSubsystem | 프로필별 로드/복원/자동 저장, 보급 수령 기록, 저장 실패 상태 |
| TutorialNPC | 범위/생존/StartHub/런 상태 검사, 실제 거래, 대화·서비스 입력 |
| Character/GameMode/RunPortal | 준비 완료 후 복원, 프로필 전환 격리, 종료/포탈 전 저장 |
| BasicMonster | 생성 당시 저장 난이도 적용, 경험치와 골드 보상 |

에셋은 소프트 참조를 저장하고, 아이템은 GUID와 강화 단계/누적 보너스를 이동 경로 전체에서 보존한다. 물약 구매와 장비 강화는 골드와 결과 상태를 함께 변경해 중간 상태를 저장하지 않는다. 인벤토리는 모든 에셋, 수량, 슬롯, 중복 GUID와 그리드 충돌 검사가 통과한 뒤 한 번에 교체한다.

변경 후 1초 자동 저장과 포탈/UnPossessed/EndPlay 즉시 저장을 사용한다. 쓰기 실패 시 포탈 이동을 막고 대화에서 오류를 표시한다. 복원 실패·버전 불일치·누락 에셋은 기존 파일을 보존하며 자동 저장을 차단한다. 실패 단계와 롤백 결과를 프로젝트 로그에 남긴다.

성장/특성을 새 프로필의 기본값으로 되돌려 같은 Controller의 PlayerState를 재사용해도 이전 캐릭터 진행이 섞이지 않게 했다. 부착된 프로필 GUID와 선택 프로필 GUID의 일치를 검사하며, 이전 캐릭터는 새 Pawn 생성 전에 해당 GUID로 저장한다. 보급의 영구 수령 기록을 기준으로 판정하고 프로필 저장 모드에서는 기존 보급 전용 운반을 중단한다.

지원 환경은 로컬 단일 플레이다. 선택 프로필이 없으면 기존 세션 보급 동작을 유지한다. 물리 스킬 키 설정은 기존 로컬 플레이어 공통 저장이며, 실제 공격 슬롯 배치만 프로필 진행에 포함된다. 현재 런, 현재 HP/스태미나/마나, 전투 쿨다운과 월드의 드롭 아이템 액터는 저장하지 않는다.

## 범위별 변경 파일

아래 목록은 작업 시작 시 보존한 `tmp/baseline/Source`와 작업 폴더의 Source를 파일 내용으로 대조한 범위다. 이전 미커밋 작업 전체를 이 기능의 변경으로 집계하지 않았다. 바이너리 에셋과 모듈/Build.cs는 수정하지 않았다.

| 범위 | 파일 | 목적 |
| --- | --- | --- |
| NPC/화면 | `World/Rogue10mTutorialNPC.h/.cpp`, `UI/Widgets/Rogue10mTutorialWidget.h/.cpp` | 8개 서비스, 키보드 선택/실행, 상태·오류·난이도 표시 |
| 서비스 데이터/특성 | `Data/Rogue10mTownServiceDataAsset.h/.cpp`, `Components/Rogue10mTownProgressionComponent.h/.cpp` | 조정 가능한 서비스 수치와 PlayerState 특성 소유권 |
| 저장 코어 | `Core/Rogue10mProgressionSaveGame.h`, `Core/Rogue10mProgressionSubsystem.h/.cpp` | 프로필 GUID 저장, 소프트 스냅샷, 변경 저장, 복원/오류 검사 |
| 성장/전투 | `Core/Rogue10mPlayerState.h/.cpp`, `Components/Rogue10mCombatComponent.h/.cpp` | 성장/수련 저장 API, 특성 컴포넌트, 해금/공격 배치 저장, 초기 무기 일관성 |
| 아이템 | `Components/Rogue10mInventoryComponent.h/.cpp`, `Components/Rogue10mInventoryPersistence.cpp`, `World/Rogue10mDroppedItem.h/.cpp` | 거래/재화/강화, 검증 후 복원, 아이템 상태 이동/재획득 |
| 생명주기/프로필 | `Character/Rogue10mCharacter.h/.cpp`, `Core/Rogue10mGameMode.cpp`, `Core/Rogue10mCharacterProfileSubsystem.h/.cpp` | 준비 후 복원/파생 수치, 프로필 전환, 분리 QA 프로필 슬롯 |
| 포탈/보상 | `World/Rogue10mRunPortal.cpp`, `World/Rogue10mTutorialSupplySubsystem.h/.cpp`, `Enemy/Rogue10mBasicMonster.h/.cpp` | 이동 전 저장, 보급 중복 방지, 난이도/골드 보상 |
| 실제 게임 검증 | `Tests/Rogue10mTownServicesRuntimeTest.cpp`, `Tests/Rogue10mTownRunRuntimeTest.cpp`, `Tests/Rogue10mProgressionFailureRuntimeTest.cpp`, `Tests/Rogue10mTownVisualRuntimeTest.cpp` | 재시작/서비스/런/오류/화면 검증 명령 |

파일 표의 Source 경로 기준은 `Source/Rogue10m/`다. 설계 문서는 [Architecture](../architect/2026-10-11_town-services-persistence.md), 운영 설명은 [사용 가이드](../../Docs/TownServicesPersistenceGuide.md)다.

검증 실행 스크립트는 `Scripts/ValidateTownServices.ps1`, `Scripts/ValidateTownRun.ps1`, `Scripts/ValidateProgressionFailures.ps1`, `Scripts/ValidateTownVisual.ps1`을 추가했다.

## 빌드와 검증 기록

| 검증 | 현재 결과 | 최종 기록 |
| --- | --- | --- |
| 작업 폴더 최초/후속 Editor 빌드 | 성공: Builder 실행 기록 기준 | `Scripts/BuildEditor.ps1` |
| 원본 최종 통합 Editor 빌드 | 성공: 29.64초, 초기 링크 잠금 해소 | [원본 최종 빌드 로그](evidence/town-services-persistence-20261011/final-integrated-build.log) |
| 최신 검증 코드 포함 작업 폴더 증분 빌드 | 성공: 5개 action, 80.86초 | [최종 빌드 로그](evidence/town-services-persistence-20261011/final-isolated-build.log) |
| 신규 캐릭터 초기 저장/즉시 복원 | `native01` 원인 수정 후 `final01` Write 성공 | [Write 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_final01/write-results.txt) |
| 실제 서비스/강화/특성/인벤토리 재시작 | `final01` Write 60개/Read 25개 통과, failures=0 | [Write 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_final01/write-results.txt), [Read 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_final01/read-results.txt) |
| 프로필 전환 격리/보급 재수령 방지 | `final01` Read 성공 | 동일 클래스의 두 프로필 전환과 각자의 보급 수령 확인 |
| 포탈 이동/난이도/몬스터 XP·골드 | `finalrun01` Write 42개/Read 15개 통과, failures=0 | [런 Write 결과](evidence/town-run-persistence-20261011/Rogue10m_Town_QA_Run_finalrun01/write-results.txt), [런 Read 결과](evidence/town-run-persistence-20261011/Rogue10m_Town_QA_Run_finalrun01/read-results.txt) |
| 저장 버전/누락 에셋 복원 거부와 원본 보존 | `failure01` 27개 통과, failures=0 | [오류 복원 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_failure01/failures/results.txt) |
| 대화 조작/화면 레이아웃 | 1280×720 / 1920×1080 각각 9개 통과 | [720p 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_finalvisual720warm/visual-results.txt), [1080p 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_finalvisual1080/visual-results.txt) |
| 생성 출력 검사 | 원본 `CheckGeneratedChanges.ps1` 통과 | [검사 로그](evidence/town-services-persistence-20261011/generated-check.log), 기존 다른 작업의 바이너리 변경 경고는 구분 |
| 원본 최종 빌드 통합 서비스 검사 | `integrated01` Write 60개/Read 25개 통과, failures=0 | [원본 Write 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_integrated01/write-results.txt), [원본 Read 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_integrated01/read-results.txt) |
| 원본 최종 빌드 통합 포탈/런 검사 | `integratedrun01` Write 42개/Read 15개 통과, failures=0 | [원본 런 Write 결과](evidence/town-run-persistence-20261011/Rogue10m_Town_QA_Run_integratedrun01/write-results.txt), [원본 런 Read 결과](evidence/town-run-persistence-20261011/Rogue10m_Town_QA_Run_integratedrun01/read-results.txt) |
| 원본 최신 F8 입력과의 추가 화면 검사 | `integratedvisual720` 9개 통과, failures=0 | [원본 화면 결과](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_integratedvisual720/visual-results.txt), [원본 화면](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_integratedvisual720/town-services.png) |

실제 게임 검증은 사용자 저장과 분리된 `RogueProfileSaveSlot`, `RogueProgressionSavePrefix`, `RogueSkillKeySaveSlot` 옵션을 사용한다. 최신 `final01`에는 몸통 방어구의 실제 GAS Defense +2와 25골드 차감, 비어 있지 않은 회복 물약 퀵슬롯의 0→2 이동·재접속 및 기존 0번 칸 비움 검사를 포함했다. 이벤트 자동 저장과 프로필 격리도 동일 코드로 다시 통과했다.

초기 실행 증거는 `evidence/town-services-persistence-20261011/Rogue10m_Town_QA_native01/write.log`와 `write-results.txt`에 있다. 처음 생성한 장비와 활성 무기의 불일치는 실제 장착 아이템을 우선하도록 수정했다. 이후 초기 저장/복원 실패는 기본 프로필이 ChargedPrimary/ChargedSpecial에 같은 스킬을 연결하는 정상 배치를 복원 코드가 중복으로 거부한 것이 원인이었다. 정상 기본 배치를 허용하도록 수정했다.

수정 후 `evidence/town-services-persistence-20261011/Rogue10m_Town_QA_native02/`의 `write-results.txt`와 `read-results.txt`는 각각 failures=0을 기록했다. 이벤트 자동 저장이 전체 진행 상태를 남기는 것과, 동일 캐릭터 클래스의 다른 프로필에서 진행과 보급 수령 기록이 격리되는 것을 확인했다.

`failure01`은 SaveVersion=999와 성장 복원 이후 발견되는 누락 아이템 에셋을 각각 실제 저장 파일로 작성했다. Attach 실패 시 성장/인벤토리/스킬/특성을 되돌리고 NPC 서비스·보급 수령·후속 저장을 거절하며, 거절과 Detach 후에도 기존 디스크 바이트가 정확히 동일한 것을 확인했다. `version.sav`와 `missing_asset_after_growth.sav`를 증거로 보존했다.

최종 `finalrun01`은 StartHub의 실제 캐릭터 캡슐이 포탈에 진입해 OpenRunMap으로 이동하고 런이 Running이 되는 것을 확인했다. 포탈 전후의 모든 진행 스냅샷이 일치하며 최초 보급이 추가 복제되지 않았다. 강화 주무기의 실제 드롭/재획득에서도 GUID, 수량과 누적 보너스가 유지됐다. 어려움에서 생성한 실제 네이티브 몬스터는 HP 80→120, 피해 12→15, XP 보상 20→30과 골드 3을 적용했고, 이미 죽은 몬스터에 다시 피해를 줘도 보상이 늘지 않았다. 두 번째 프로세스에서도 처치 보상과 모든 진행 스냅샷이 일치했다.

원본 최초 반영 시 기능 범위의 Source 34개 파일은 작업 폴더와 내용 해시가 일치하는 것을 확인했다. 이후 동시 작업에서 `BasicMonster.h/.cpp`에 `bCombatTestInstance`와 `MarkCombatTestInstance`를 추가해 테스트 인스턴스의 보상만 막는 변경을 보존했다. 최종 대조는 32개 파일의 내용 해시 일치와 해당 2개 파일의 수동 diff 확인으로 수행했고, 본 기능의 난이도/보상 코드는 유지됐다. 동시 개발 파일 전체를 덮어쓰지 않고 해당 파일들의 기준 사본 대조와 백업을 거쳐 반영했다.

[원본 반영 기록](evidence/town-services-persistence-20261011/source-integration.json)과 [독립 리뷰](evidence/town-services-persistence-20261011/review.md)에 대조·초기 문제 수정·최종 검증 판단을 정리했다. 최종 원본 `integrated01`과 `integratedrun01`은 동시 테스트 인스턴스 플래그 추가를 포함하는 현재 모듈에서 실행됐고, 실제 보상·프로필 격리·저장 복원까지 모두 통과했다.

화면 검증은 D3D11의 1280×720과 1920×1080에서 실제 대화를 렌더링했다. 두 해상도 모두 8개 서비스와 요약/최종 조작 안내가 화면 안에 들어오며, 실제 입력 스택의 Enter 구매, Down 선택 이동과 Esc 닫기가 통과했다. PNG 해상도를 확인했고 셰이더 준비 오버레이가 없는 화면을 증거로 남겼다. 첫 `finalvisual720`은 차가운 SM5 셰이더 준비로 240초 제한에 도달했으며 로그를 보존했다. `finalvisual720warm`은 600초 제한으로 다시 실행해 통과했다.

[720p 화면](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_finalvisual720warm/town-services.png)과 [1080p 화면](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_finalvisual1080/town-services.png)에 최종 레이아웃을 보존했다.

최종 원본 `integratedvisual720`도 9개 검사와 렌더링을 통과해 현재 PlayerController의 F8 입력 변경과 NPC 서비스 입력이 함께 동작하는 것을 확인했다. 확정 검사 수는 원본 서비스/런 142개, 오류 복원 27개, 두 해상도 화면 18개로 합계 187개이며, 원본 720p의 9개 검사를 추가로 반복 확인했다.

초기 원본 Editor 링크는 동시 진행 중인 개발 Editor의 DLL 잠금으로 LNK1104가 발생했다. 사용 중인 Editor를 강제 종료하지 않았으며, 이후 원본 최종 통합 빌드가 29.64초에 성공해 링크 제한을 해소했다. 원본 프로젝트 경로와 생성 DLL 갱신을 확인했고, 해당 빌드를 사용하는 원본 서비스/포탈 Write/Read 검사도 통과했다.

## 후속 범위

향후에는 저장 버전 마이그레이션, 백업/복구와 수동 저장 UI, 멀티플레이어 서버 저장 정책을 설계한다. 실제 플레이 결과를 기준으로 골드 수급과 강화/특성 비용을 조정하고, 추가 NPC·아이템 카탈로그와 디자이너 Data Asset을 확장한다. 이번 기능은 기존 ServiceData가 비어 있으면 C++ 기본값으로 동작하므로 새 에셋 생성이나 기존 .uasset/.umap 수정은 필요하지 않았다.
