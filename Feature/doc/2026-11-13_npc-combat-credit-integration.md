# NPC 처치 귀속 통합 결과

격리 commit `7c31084`의 combined 12개 파일을 한 번 적용했다. 몬스터가 죽었다는 이력과 올바른 플레이어의 퀘스트 처치 인정을 분리하고, 공격자·RunId·세대·소유자가 일치할 때만 진행을 확정한다. 무귀속 사망의 복구, 중복 보상 억제, 재진입과 시체 파괴 후 지연 전달을 함께 처리한다.

정규 Editor 빌드와 새 DLL의 C++ 회귀 51개가 성공했다. 이는 소스 통합과 격리 C++ 검증의 완료이며, 실제 생산 맵의 Director·충돌·loot·Run 전환 및 플레이 검증은 남아 있다.

## 적용과 보존

기존 8개 파일을 물리 백업한 뒤 신규 4개를 포함한 작은 delta만 적용했다. full 30개 패킷은 덮어쓰지 않았다. combined SHA256은 `237c14c86d6a1bdfaddd8c6af8b4b0395c3b3cf7a024b64902c75d934c0fb1e8`이다. 이후 테스트 fixture 1개만 보강했으며 runtime 9개의 정규화 postimage는 전달본과 일치한다.

최종 전체 입력은 10,460개다. 이전 E5 입력 10,456개와 비교해 지정한 기존 8개 변경과 신규 4개 추가만 확인됐다. Content 9,521개, 다른 Source·Config·Scripts·Build·uproject 입력, 보호 저장 2개 루트와 기존 Windows E5 payload는 모두 보존됐다. Units derivative와 G08/VFX 및 사용자 WIP도 이 비교 범위에 포함된다. Git 작업 브랜치는 `Sprint#9-32-landscape-world-blockout`이며 commit·push·브랜치 전환은 하지 않았다.

## 실제 빌드와 검증

| 검증 | 실제 결과 |
| --- | --- |
| 최초 정규 UHT/Editor 빌드 | exit 0, 102.097초 |
| 설치된 소스의 MSVC standalone 재실행 | 202,871개 검사, 실패 0; UE 검사와 별도 |
| Native51A | 기존 43 성공, 신규 8 실패, 전체 RED 57; 실패 증거 보존 |
| fixture 수정 후 정규 Editor 빌드 | exit 0, 41.881초 |
| 새 DLL의 Native51B | 정확히 51/51 성공, 120.187초, 전체 RED 0 |
| Native51B 구성 | NPC 26개(신규 8 + 기존 18), G08/VFX 25개 |
| Native51B 경고 | 11개 사례에 19건: WorldContext 없는 Actor 파괴 9건, Unarmed profile 미설정 10건 |
| 전후 무결성 | Source/Config 368개 및 사용 DLL 불변 |

두 Editor 빌드의 기본 XGE 실행에서 license 미활성 경고가 각 1건 있었으며 컴파일 오류는 없었다. Native51B는 경고가 없는 결과로 표현하지 않는다. 정확한 테스트 이름·완료 로그·Queue 51 및 오류 로그 전체를 대조했고 독립 읽기 검토도 같은 결과를 확인했다.

현재 Editor runtime DLL SHA256: `CA647B1E2264CB5DBC5025F81D2C28E10E12CDE5BF889F552033F8B1D2CB9175`.

## 첫 실패의 원인과 fixture 수정

Native51A의 UE 프로세스 종료 코드는 0이었지만 보고서의 신규 8개는 실패했다. runner는 이를 exit 1로 판정했다. 공통 원인은 `APawn::ShouldTakeDamage`가 AuthGameMode 없는 월드의 피해를 거절해 실제 `TakeDamage`가 0이 된 것이었다. 처치 귀속 로직에 도달하기 전의 fixture 초기화 문제다.

테스트 CPP 하나에서 native `AGameModeBase`의 deferred spawn, 실제 `Rogue10mGameState` 지정과 public `UWorld::CopyGameState`로 framework 관계를 구성하고 authority·world 관계를 확인하는 assertion을 추가했다. 실제 피해 API와 기존 credit assertion, NoGI·NoWorldBegin 조건은 유지했다. 피해 guard override, Health 직접 변경, private field reflection은 사용하지 않았다. 실패 A의 소스와 DLL도 별도로 보관했다.

## 산출물과 재현

- [적용 결과](evidence/npc-combat-credit-20261113A/integration-result.json)
- [fixture 변경과 전후 해시](evidence/npc-combat-credit-20261113A/FixtureDerivativeB/derivative.json)
- [Editor 빌드 결과](evidence/npc-combat-credit-20261113A/editor-build-fixture-result.json)
- [Native51B 판정](evidence/npc-combat-credit-20261113A/Native51B/reviewed-result.json), [UE 원본 보고서](evidence/npc-combat-credit-20261113A/Native51B/report/index.json)
- [전체 입력·저장·기존 Windows 보존 검사](evidence/npc-combat-credit-20261113A/preservation-result.json)
- [최종 경로·생성 파일·소스 해시 검사](evidence/npc-combat-credit-20261113A/final-checkpoint.json): `CheckGeneratedChanges.ps1`와 범위 한정 `git diff --check`가 exit 0이며, 18:08:08 UTC의 엔진·빌드·게임 프로세스는 0개다.
- [Native51B 실행기](../implementation/npc-combat-credit-20261113A/run_native51_b.py): 기본은 plan 출력이며 이미 사용한 결과 폴더에 덮어쓰지 않는다. 재실행 시 새 버전·프로세스/해시 확인이 필요하다.

## 남은 검증과 별도 차단

실제 Director 생성·복구 재시도·충돌, loot와 외부 Blueprint listener 보상 순서, production GUID/소유자 전환, GI를 통한 새 런·저장·재시작과 F 상호작용은 이번 C++ 검증에 포함하지 않았다. 기존 잘못된 live receipt의 hot-reload 이관도 보장하지 않으므로 실제 검증은 새 런 기준이다. 3인칭·피격 시 스킬 취소·면역 보류 결정은 유지한다.

기존 `InternalWindows20261113E5_UNVERIFIED`는 이 NPC 수정 전 빌드다. payload와 봉인을 그대로 보존했으며 이번 수정 포함 또는 새 회귀 통과 패키지로 표시하지 않았다. 이번 패킷에서는 Game/Cook/패키지 재생성과 게임 실행을 하지 않았다.

최신 사용자 지시로 이번 작업의 Computer Use 금지는 해제됐고 활성 하위 Work에도 전달했다. 해제 전 기간과 이번 NPC 패킷의 GUI 입력은 모두 0회다. 일반 에디터/게임 검증은 현재 상태를 확인한 별도 작은 작업으로 재개할 수 있지만, Landscape 전체 보존과 crop은 아직 입증되지 않았다.

읽기 전용 후속 검토의 대상은 `/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A`이며, Temp clone의 QA 맵과 구분한다. 다음 최소 단계는 정상 공유 Editor에서 맵 identity·dirty 상태, Director 인스턴스·연결 DA·SupportedWorld, GameMode/PlayerStart와 combat MonsterData/LootTable의 read-only live inventory다. 실제 배치 유무를 먼저 확인한 뒤 3인칭 진입·지면 충돌·짧은 이동의 일반 플레이 baseline을 별도로 검증한다. asset 저장·기본맵 변경·자동 배치·MapMaker 이동/저장/cold 검증은 이 범위에 포함하지 않는다. 이 제안은 아직 실행하지 않았다.

NPC 생산 검증에는 정확한 후보 맵을 지원하는 명시적 QuestDefinition Data Asset과 Director, 활성 전투 목표의 MonsterData/LootTable, 정상 프로필·유효 Run GUID 연결 확인이 먼저 필요하다. 소스의 기본 퀘스트 정의는 과거 FiveVillageGraybox만 지원하고 GameMode에서 Director를 자동 연결하지 않는다. Intro 예제는 NPC 1명·회수 표식 2개·전투 0개이므로 처치/loot 검증을 대신하지 못한다. Director의 Standalone 조건도 일반 PIE baseline과 구분한다. 현재 후보의 실제 배치 연결은 추가 live 확인 대상이며, 과거 전용 맵/인공 드롭 테이블의 F·loot harness를 그대로 새 후보 검증으로 재사용하지 않는다.

자동 승인 심사가 거절한 `C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/root-startup-approval-A.json` 생성과 그 승인에 의존하는 metadata-only commandlet/clone QA는 계속 중단했다. 사유는 임의의 `approved=true` 영수증이 실제 승인 전제를 우회할 위험이다. 해당 파일은 없고, 이 단계의 재시도·이름/경로/런처/GUI/다른 에이전트 우회 및 QA 실행은 0회다. Computer Use 허용과 NPC 소스 검증 허가는 이 별도 차단의 승인으로 해석하지 않는다.
