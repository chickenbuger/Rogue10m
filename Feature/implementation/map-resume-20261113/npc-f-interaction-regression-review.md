# NPC F 상호작용·스킬 입력 충돌 회귀 메모

현재 Source를 읽고 기존 결과 파일을 확인한 사전 검토다. 새 Windows seal로 동결된 Source/Config/Content/Scripts와 기존 증거를 수정하지 않았다. UE/UBT/UAT/Cook/Game/MCP/GUI/키보드·마우스 및 입력 API 우회 실행은 모두 0이다. 새 파일은 이 메모 하나다. 1인칭은 보류하고 현재 3인칭 계약을 유지한다.

**과거 F 충돌이 현재 게임에서도 해결됐다고 단정하지 않는다. NativeTestsD 43개 통과는 F 입력·NPC 패널 회귀 증거가 아니다.** 해당 43개 선택 목록에는 NpcInteraction/NpcDialogue/SkillBinding 입력 fixture가 없으며, VillageQuest 수치 모델·보상 검사와 실제 입력 라우팅은 서로 다른 범위다.

## 현재 코드의 경로

| 경계 | 확인한 코드 |
| --- | --- |
| NPC의 F | `World/Rogue10mTutorialNPC.cpp:169–179`에서 근접 플레이어 입력을 활성화하고 F Pressed를 `bConsumeInput=true`로 바인딩한다. `:134–152`가 사망·거리·local controller·다른 blocking UI를 검사하고 `:298` 이후 HandleInteract가 대화/수령/닫기를 결정한다. |
| VillageQuest NPC | `World/Rogue10mVillageQuestNPC.cpp:30–36`이 같은 availability 확인 후 대화 열기/닫기를 처리한다. 수치 quest 검증만으로 이 입력 연결까지 확인되지는 않는다. |
| 대화 widget의 F | `UI/Widgets/Rogue10mTutorialWidget.cpp:110–118` PreviewKeyDown이 열린 대화의 F를 non-repeat일 때 Interact로 보내고 Handled를 반환한다. 실제 focus/Slate 전달이 이 경로를 정확히 한 번 타는지는 정적 검토로 증명되지 않는다. |
| F의 스킬 할당 거부 | `Components/Rogue10mSkillBindingComponent.cpp:121–129`는 Q/E/R만 usable이며 나머지는 reserved다. `:164–174`의 short-circuit로 F는 AssignSkillToInputSlot 전에 거부된다. `:67–78`은 저장된 arbitrary/비허용 키를 정리한다. |
| 키 선택 UI | `Rogue10mHybridSkillTreeWidget.cpp:984–998`, `Rogue10mMenuWindowWidgets.cpp:2409–2435`는 modifier chord를 거부하고 공통 SetSkillKey로 보낸다. F가 선택돼도 위 예약키 검사를 우회하는 별도 성공 경로는 발견하지 못했다. 실제 key-selector focus/capture는 미검증이다. |
| 일반 스킬/소비 키 | `SkillBindingComponent.cpp:218–245`는 consumed key와 8개 modifier를 검사하고 release/cancel 전달은 유지한다. `Character/Rogue10mCharacter.cpp:340–366`은 고정 Q/E/R 및 마우스 전투 입력을 바인딩하며 `:629–650`은 modifier 시작 차단과 release 전달을 분리한다. |
| 대화 중 입력 차단 | `Core/Rogue10mPlayerController.cpp:515–544`가 대화 controller·focus·move/look ignore 소유권을 관리한다. `:737–742`는 NPC 대화를 blocking UI에 포함한다. `:1330–1349`는 blocking 전환 시 skill held/pending attack 입력을 취소하고 `:1415–1446`은 대화 GameAndUI와 닫힌 뒤 GameOnly를 전환한다. |
| 실제 전투·마우스 chord | `CombatComponent.cpp:1250–1256`의 CanUseCombatInput, `MouseCombatInputComponent.cpp:12–15`의 CanUseInput은 blocking UI를 거부한다. 마우스 component는 `:24–56`에서 LMB/RMB 동시 창과 accepted skill snapshot을 처리하고 `:97–104`에서 UI/상태 불가 시 timer·held·snapshot을 정리한다. |

위는 방어 경로가 존재한다는 관찰이다. legacy NPC input stack, Enhanced Input, widget preview와 GameAndUI 사이의 실제 순서·포커스 복구·중복 전달까지 통과했다는 판정은 아니다.

## 실제로 남아 있는 과거 증거

- [NpcRegF9/results.json](../../doc/evidence/npc-interaction-20261029/NpcRegF9/results.json): 92 checks, failures0, F binding rejected, 기존 Q loadout 보존, collision_detected=false. Q 유료 비차징 기술 positive control, settings/dialogue 중 Q 차단, F 대화·중복 공격 차단, Q/E/R bare·LeftShift·menu-close release-only·overlap 경계가 기록되어 있다.
- 같은 폴더의 run-envelope.json〔중간·보조 자료 정리로 삭제됨; [보존 목록](../../doc/evidence/README.md)〕과 `events.csv`: D3D11 rendered/offscreen, Controller InputKey 프로그램 입력, `PhysicalInput=false`, `ManualFieldRun=false`. 당시 화면은 `DevLog/20261029_NpcInteraction_NpcRegF9/01_f_prompt.png`, `02_f_dialogue.png`, `03_f_collision_probe.png`에 남아 있다.
- [ResumeF3/final-summary.json](../../doc/evidence/npc-dialogue-20261110/ResumeF3/final-summary.json): 당시 F9 DLL `DF2F590B…CBE597`에서 NpcInput92/NpcDialogue58 등의 단계 성공을 기록한다. 현재 seal/DLL의 재검증으로 사용할 수 없다. 같은 폴더의 `npc-input-F9-launcher.txt`, `npc-dialogue-F9-launcher.txt`와 `DevLog/20261110_NpcDialogue/NpcResumeF9/results.txt`, `01_proximity_prompt.png`~`06_death_boundary.png`가 실제 보존 위치다. PhysicalInput/FullFieldRun은 false이고 FirstPersonDevelopment는 paused다.
- `Source/Rogue10m/Tests/Rogue10mNpcInteractionRuntimeTest.cpp:2–581`에 당시 fixture가 남아 있다. `:323–329`는 F 할당 거부/전체 Q loadout 보존, `:396–516`은 메뉴·대화·F collision probes다. `Scripts/VerifyNpcInteraction.ps1`이 이를 검사한다.
- `Rogue10mNpcDialogueRuntimeTest.cpp:2–222`와 `Scripts/VerifyNpcDialogue.ps1`도 존재한다. 이 fixture의 `:74–89`는 Slate key/pointer routing과 Controller InputKey를 사용한다. **둘 다 현재 GUI/입력 우회 금지에서 실행하면 안 된다.** NullRHI나 프로그램 API라는 이유로 예외가 되지 않는다.

기존 92검사의 자체 한계도 유지한다. 다른 7개 modifier 조합, charged-skill modifier-after-press 복구, 실제 물리 입력·풀 필드 플레이는 당시에도 미검증이었다. 별도 mouse cases 성공을 NPC 대화와 LMB/RMB chord·RMB charge의 교차 검증으로 합쳐 주장하지 않는다.

## 현재 허용되는 검토와 향후 미검증 범위

현재 작업에서는 Source 정책·키 enum·short-circuit·blocking 상태·취소 경로 및 과거 JSON/로그/해시를 읽는 검토만 수행했다. 입력 전달을 호출하지 않는 분리된 데이터/상태 모델로 F 거부 시 무변경, Q/E/R 허용, modifier 차단, UI 진입의 pending/charge/chord 취소, 닫힌 뒤 release-only 상태를 검증하는 것은 별도 비GUI 모델 검증으로 구분할 수 있다. 그런 모델 통과도 실제 widget focus·Enhanced/legacy stack·물리 F의 성공을 증명하지 않는다. 이번에 모델 코드를 만들거나 실행하지는 않았다.

현재 seal의 실제 3인칭 NPC 플레이에서 다음은 여전히 검증 대상이다.

1. 근접/범위 밖/재진입/여러 NPC/다른 패널에서 F가 정확히 한 번 올바른 대화만 열고 닫는가. key-selector가 F를 거부하면서 NPC나 전투로 누출하지 않는가.
2. 대화 전후 Q/E/R의 정상 positive control과 대화 중 비용·use·cooldown·추가 타격 차단, held→대화 열기→닫기→release에서 지연 실행이 없는가.
3. 무기별 LMB+RMB 동시 기술 및 RMB 차징을 준비한 상태에서 F 대화/패널 진입이 pending timer·charge를 취소하고, 닫힌 뒤 원치 않은 기본/동시/차징 공격을 만들지 않는가.
4. 대화·서비스·사망·거리 이탈·맵 전환 뒤 포커스와 이동/시점 소유권이 복구되는가. 실제 물리 패널 조작·입력은 현 금지 상태에서 미검증으로 남긴다.

이 항목은 필요한 회귀 범위를 보존한 목록이며 새 실행이나 승인 기록이 아니다. Source 변경, 기존 캡처 재라벨, 이전 결과의 현재 DLL 증거 전환, 1인칭 재개는 하지 않는다.


2026-11-18 자료 정리: 위 실행 결과·수치·실패 설명은 당시 기록이다. 중간·보조 사진/JSON의 현재 보관 여부는 [검증 자료 보존 목록](../../doc/evidence/README.md)을 따른다. 삭제 자료를 다른 성공 증거로 대체하지 않았다.
