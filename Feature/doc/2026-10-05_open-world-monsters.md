# 오픈월드 기본 몬스터와 시작마을 안내인

최종 사용자 요청에 맞춰 기존 오픈월드 6종에 보유 모델과 Idle·이동·기본 근접 공격을 연결했다. 시작마을에는 F로 대화하고 체력 물약 2개를 받을 수 있는 길잡이 아린을 배치했다. 작업 브랜치는 `Sprint#5-14-open-world-monsters`이며 커밋·푸시는 하지 않는다.

## 몬스터 구현과 데이터

`URogue10mMonsterAttackPatternComponent`가 타이머로 Idle·이동 동작을 선택하고 기본 공격을 준비→타격→회복 순서로 처리한다. 타격 순간 선택한 대상의 실제 캡슐·거리·방향·높이·벽 차단을 확인한다. 사망 시 진행 중인 공격과 AI를 종료한다. 피격·사망 동작도 보유 클립을 이용한다. Actor Tick은 추가하지 않았다.

전용 프로필을 지정하지 않은 기존 몬스터는 기존 전투 경로를 유지한다. 오픈월드 6종에만 새 `/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_*`와 `DA_OW_BasicAttack_*`를 연결했다. 원본 25종 로스터·구매 메시·시퀀스·재질은 수정하지 않았다. MonsterId와 원본 체력·피해·경험치·이동 속도·공격 사거리를 유지했다. 별도의 전투 밸런스 완성을 주장하지 않는다.

| 종 | 보유 모델 | Idle / 이동 / 기본 공격 |
| --- | --- | --- |
| 초록 점액체 | Polyart Slime | IdleNormal / Walk / Attack01 |
| 어린 포자 버섯 | Polyart Mushroom | IdleNormalSmile / walkFWDSmile / Attack01Smile |
| 붉은 점액체 | PBR Slime | IdleNormal / Walk / Attack01 |
| 철갑 등껍질 | PBR TurtleShell | IdleNormal / Walk / Attack01 |
| 가시 수문장 | PBR Cactus | IdleNormal / WalkFWD / Attack01 |
| 아자토르 | PBR Beholder | IdleNormal / WalkFWD / Attack01 |

전용 기본 공격 프로필과 시퀀스 길이를 맞췄다. 보스 외형도 보유 모델이며 이번 범위에는 특수 패턴·돌진·장판·체력 단계·승리 목표 변경을 포함하지 않는다. 기존 지역 스포너의 개체 수와 보충 정책을 유지한다.

## NPC와 물약 이동

안내인은 남쪽 시작 지점 근처 `(440, -1610)`cm에 배치했다. 보유 Quinn 모델과 호환되는 MM_Idle을 사용한다. F는 대화 열기→물약 받기→닫기로 동작하며 대화는 이동·시점 입력이나 마을 대기 타이머를 막지 않는다. 가방이 꽉 차면 수령 기록을 남기지 않고 재시도할 수 있다.

PlayerState와 GameInstance의 세션 기록으로 반복 수령을 막는다. 포탈이 OpenLevel로 Pawn을 다시 만드는 기존 구조에 맞춰, 지급한 스택의 실제 GUID와 남은 수량만 이동 전에 확인하고 다음 맵에서 한 번 복원한다. 사용·삭제된 스택을 새로 지급하지 않는다. 일반 인벤토리나 영구 프로필 저장을 확장하지 않았다.

실제 F 입력으로 대화 열기·2개 지급·닫기를 확인했다. 화면 검사에서 NativeConstruct 이후 만들어진 WidgetTree가 표시되지 않는 문제를 발견하여 RebuildWidget 이전에 트리를 만들도록 수정했다. NPC 상세 계약은 `Feature/doc/2026-10-05_start-hub-tutorial-npc.md`에 기록했다.

## 검증 및 근거

최종 실행 결과는 `Feature/doc/evidence/open-world-monsters-20261005/`에 저장한다.

- Editor NoHotReload 첫 빌드 성공(111.69초), 보급품 이동 클래스 재로드 확인, 대화 표시 보완 빌드 성공(70.11초).
- 에디터 재시작 후 저장된 에셋·6종 호환 클립·기존 마을 배치 검사 100/100 통과.
- 최종 표시 보완 빌드에서 NPC 실제 지급·중복 방지·만석 재시도·물약 사용·기존 포탈 overlap·잔여 1개 전달·1200초 런 시작 검사 57/57 통과. 검사 종료 후 PIE=false, StartHub의 미저장 맵·이번 소유 콘텐츠가 없음을 확인했다.
- 최종 빌드의 몬스터 실제 PIE 검사 219/219 통과. 6종 모두 Idle·이동 클립의 시간 진행, native AI 추적과 실제 플레이어 피해, 공격 1회 타격·범위 이탈 회피·사망 시 취소를 확인했다. 검사 소유 PIE 종료도 확인했다.
- 실제 F 입력으로 대화 열기·회복 포션 2개 수령·닫기를 다시 확인했다(`guide-key-final.json`). `guide-dialogue-play.png`, `guide-reward-play.png`에서 한글 대화 카드와 수령 문구가 보이는 것을 확인했다. NPC 외형은 보유 Quinn 기본 모델이다.
- 전체 Content 6,292개 SHA256 기준선에서 6,282개 동일, 허용된 기존 10개 변경·신규 15개만 추가. 원본 에셋과 기존 마을 액터를 보존했다.

초기 검증용 Python에서 읽기 전용 인벤토리 속성과 비노출 생성 API를 사용한 문제는 실제 인벤토리·스포너 API를 이용하는 검증으로 수정했다. 실패 근거도 별도 JSON으로 보존한다. 플레이 검사는 단독 PIE의 집중 검사이며 전체 20분 플레이·온라인·패키징·출시 성능을 보장하지 않는다.

## 변경 파일과 검토

몬스터는 `Enemy/Rogue10mMonsterAttackPattern*` 3개 신규 파일과 BasicMonster·MonsterDataAsset·MonsterAIController의 opt-in 연결부를 변경했다. NPC는 World의 Tutorial 데이터·Actor·보상 컴포넌트·보급 Subsystem 및 UI의 TutorialWidget을 추가했다. Inventory와 RunPortal에는 보급 스택 운반을 위한 짧은 연결부만 추가했다. 기존 지면 스폰 수정과 다른 전투·HUD 작업은 보존했다.

Blueprint와 DataAsset은 공식 Unreal MCP 및 Epic Python으로 설정하고 에디터에서 저장했다. 원본 모델·동작·아이템을 참조하며 신규 바이너리를 파일 도구로 작성하지 않았다. 조사·적용·재로드·실행·해시 검사 도구와 설계/결과 문서를 함께 남겼다.

독립 검토에서 공격 중 AI Focus가 회전 방향을 바꾸는 위험을 확인하고 공격 중에는 새 Focus를 설정하지 않도록 반영했다. 사망/EndPlay의 타이머 해제, 약한 플레이어 참조, GC 추적된 데이터, 인벤토리 델리게이트 해제와 복원 중 재진입 방지를 검토했다. NPC 카드 표시 문제는 실제 화면으로 발견하고 최종 빌드와 화면으로 수정 결과를 확인했다.
