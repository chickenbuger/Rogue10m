# Compact 초급 의뢰 실제 제작과 게임 검증

WorldLayoutCompact20261113A에 초급 NPC 서린1명·회수목표2개·골드40을 실제 연결했다. 편집 가능한 신규 DA/Director를 저장하고 새 Editor 프로세스로 재로드했다. 실제 게임에서도 수락→회수→재방문→한 번 보상→중복 거부→새 RunId 초기화를 관찰했다. **지형은 아직1800×1800m이며 축소 완료가 아니다.**

## 실제 제작·저장·재로드

신규 DA는 /Game/Rogue10m/World/Production/WorldLayoutCompact20261113A/Quests/DA_IntroSupply_Compact20261113B, Director label은 CompactIntro_Serin_Director다. 이 Actor만 always-loaded이며 editor-only=false, DataLayer 없음이다. Director가 런타임 NPC1·목표2를 생성하므로 NPC/목표를 Editor에 중복 배치하지 않았다.

고정5행 중 첫 의뢰 Q_Intro_supply만 활성이다. 전투·책·비급 보상은 없다. 위치·외형·반경·대사·보상을 DA에서 편집한다. 기존 사용자 아린 RefinedV3/Standing Idle의 skeleton 일치를 확인해 임시 외형으로 재사용했다. vendor·기존 TutorialData 수정, Fab 가져오기·구매는 없다.

공식 MCP 지면 trace3곳은 바닥 Z6389.9912109375cm로 일치했다. anchor=(1000,42550,6389.9912109375), NPC=(1000,42550,6391.9912109375), 목표=(500,42950,6414.4912109375)와(1500,42950,6414.4912109375)cm다. Engine marker 바닥은 지면보다2cm 높다.

후보1419개/18,234,465B를 물리 백업하고 신규 DA1·Director external actor1·folder external object3, 총5파일만 저장했다. 기존 umap/Actor 패키지는 저장하지 않았다. Editor PID50968 정상 종료 후 PID51996에서 동일 DA5행·좌표/회전·presentation·Director 참조/속성의 직렬화 값이 모두 일치했다. native validation도 통과했다. 잘못된 Python 읽기 별칭 in_progress_text를 reflection의 progress_text로 고친 첫 실패는 보존했다. 이 실패 때 asset 수정/저장/종료는 없었다.

## 실제 게임 결과

Native51B DLL 그대로 UnrealEditor.exe -game PID51220을 실행했다. **패키지 EXE 플레이는 아니다.** 격리 Temp UserDir와 유효 QA 슬롯3개를 사용해 정상 메뉴에서 QAIntroB01 프로필을 만들고 Hub로 진입했다. 실제 프로필 저장이 QA SaveGames에 생성됨을 확인했다.

| 항목 | 관찰 결과 |
| --- | --- |
| 수락·F | F 열기/닫기/재열기와 실제 Enter 수락, 0/2 |
| 회수 | 첫 목표 overlap 후1/2; 같은 목표 재접촉 뒤에도1/2; 두 번째 후2/2 |
| 한 번 보상 | 완료 보고 Enter 전 Gold0(19:16:26UTC), native 보상 로그1건(19:19:21), 실제 Gold40(19:19:50) |
| 중복 보고 | 다시 F·Enter에 ‘이번 탐험에서 이미 보상을 받았습니다.’, 실제 Gold40 유지(19:23:49) |
| 포기·귀환 | 정상 탐험 포기 확인→보상+0/재화 복원 저장 완료 UI→마을 돌아가기→StartHub |
| 새 런 | 같은 프로필에서 다른 활성 RunId, 후보 의뢰는 수락 전0/2·Gold0 |
| 종료 | 정상 게임 종료 확인 UI→progression_saved→RequestExit→19:25:41UTC Exiting; 강제 종료0 |

프로필 DF1401B94FFEEBF376824BA86AC0E2C8, 첫 RunId A3C6878A4FDD458EEC2E899E1136627E, 새 RunId FF9142314C7E305992A965929D7D97DF다. 조회마다 활성 SaveData 참조를 먼저 확인했으며 마지막 정확한 객체 /Engine/Transient.Rogue10mProgressionSaveGame_4에서 같은 프로필/새 GUID를 대조했다. PNG만으로 잔액·RunId를 증명했다고 하지 않는다.

실제 F9 캡처6장: [수락0/2](evidence/npc-intro-compact20261113B/Visual/01-accepted-0of2.png), [재접촉1/2](evidence/npc-intro-compact20261113B/Visual/02-revisit-duplicate-first-1of2.png), [완료2/2](evidence/npc-intro-compact20261113B/Visual/03-ready-2of2.png), [보상+40](evidence/npc-intro-compact20261113B/Visual/04-reward-40.png), [중복 거부](evidence/npc-intro-compact20261113B/Visual/05-duplicate-reward-denied.png), [새 런0/2](evidence/npc-intro-compact20261113B/Visual/07-new-run-available-0of2.png). 별도 Work가6장의 직접 보이는 상태를 읽기 검토했다. 정산 UI는 CUA로 관찰했으나 F9 PNG는 생성되지 않아06번 파일은 없다.

## 개발 설정과 남은 검증

실제 StartHub 포털은 OpenRunMap·기존 FiveVillageGraybox·WorldLayout20261112의3개이며 Compact 연결0이다. 접근 위치를 개발 명령으로 준비한 뒤 기존 포털 실제 overlap을 통과하는 baseline을 확인하고, 후보는 추가 open ...Compact...?StartRun=1로 진입했다. 추가 open은 GameState1200초를 다시 시작했다. 일반 Hub→후보 경로 완성이나 시간 예산 검증으로 계산하지 않는다.

NPC/목표 접근에 BugItGo 위치 설정을 사용했다. native Ghost를 동반하므로 Walk로 충돌을 복원하고 실제 collision=true/MOVE_Walking을 확인했다. 수락/보고는 실제 F/Enter, 목표는 실제 overlap이며 Accept/Observe/Claim/Gold setter 호출은 없다. 대화 종료 뒤 일반 이동 탭에 따른 위치 변화는 확인했으나 자연 보행 경로 인증은 아니다.

빠른 연속 위치 이동 후 NPC F 안내가 사라진 사례가 있었다. 감지 범위를 완전히 벗어나 별도로 재진입하면 복구됐다. 이 설정 의존 실패를 보존하며 자연 보행 재방문까지 모두 원활하다고 표시하지 않는다. 무장 없는 새 프로필의 Q/E/R은 잠겨 있어 활성 대검·지팡이 스킬과 F 충돌 회귀 전체는 미검증이다.

월드 한글 TextRender가 네모로 표시되고 주요 버튼은 여러 줄로 줄바꿈된다. 기존 플레이어 회색 재질도 남아 있다. 게임 로그에는 vendor skeleton의 /Engine/EngineMeshes/Humanoid 누락과 material usage 경고가 있다. 이 게임 세션 전체 RED0/시각 통과가 아니다. Native51B RED0은 별도 C++ 회귀 세션 결과다.

이번 회수 의뢰는 처치 귀속 수정의 실제 combat Director·loot·production GUID/Blueprint listener 통합 검증을 대체하지 않는다. 의뢰 진행/보상 상태의 프로세스 재시작 후 영구 보존도 구현·검증되지 않았다. 자산 cold reload와 런타임 진행 복원은 다르다. 물리 직행3회·15분 성장·완주·실제 packaged EXE/UI7·저장 실패/재시작은 남는다. 기존 E5는 NPC 수정 전 상태로 고정하고 새 Windows 빌드는 별도 패킷으로 준비한다.

## 증거·보존

[계획](../architect/2026-11-13_compact-intro-quest-vertical-slice.md), [물리 백업](evidence/npc-intro-compact20261113B/physical-checkpoint.json), [live inventory](evidence/npc-intro-compact20261113B/live-inventory-A.json), [authoring](evidence/npc-intro-compact20261113B/authoring-result.json), [저장 목록](evidence/npc-intro-compact20261113B/authoring-save-intent.json), [cold reload](evidence/npc-intro-compact20261113B/cold-reload-result.json), [실제 Hub 포털](evidence/npc-intro-compact20261113B/hub-portals-live.json), [게임 로그](evidence/npc-intro-compact20261113B/game-D1.log), [QA 저장 routing](evidence/npc-intro-compact20261113B/game-D1-profile-routing.json), [정상 종료 후 프로세스0](evidence/npc-intro-compact20261113B/processes-after-game.json).

[최종 보존 검사](evidence/npc-intro-compact20261113B/post-game-preservation.json)가 통과했다. 기존 입력10460개·보호 저장2루트·이전 E5 payload50파일의 SHA가 모두 일치했다. Content는 신규5개로9526개, 전체 입력은 생성 Scripts 캐시1개를 포함해10466개다. 캐시는 authoring import와 source timestamp/size/code object가 일치해 삭제·수정 없이 별도 명시했다. 보존 검사기의 첫 bytes/size 비교 실패와 예상 밖 캐시 발견 기록도 유지했다. 이번 content 패킷에서는 Source/Config 수정·재빌드가 없었다. 최신 Computer Use 허용 후 UI를 조작했으며 과거 금지 기간 입력0 기록은 유지한다. 거절된 Root 승인 파일 생성과 의존 metadata-only/Temp clone QA는 중단·추가 실행0·우회0이다. 일반 NPC 제작/게임 검증이 그 승인을 대신하지 않는다. 사용자 WIP를 섞는 commit/push는 하지 않았다.
