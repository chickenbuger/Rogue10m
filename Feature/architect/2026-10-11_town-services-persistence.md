# 마을 서비스와 캐릭터 진행 영구 저장

## 범위와 기본 설계

사용자 요청은 스튜디오 제거 후 게임 기능을 직접 개발하는 것으로 반영한다. 기존 아린 대화·보급·포탈과 플레이어 전투를 유지하면서 아이템 강화, 특성 강화, 다음 탐험 난이도 선택을 제공한다. 성장·수련/토벌·해금/스킬 배치·인벤토리·강화·난이도·보급 수령 기록은 선택 캐릭터 프로필 GUID마다 저장한다. 실행 중인 런 시간/몬스터 위치/체력은 저장 대상이 아니다.

아이템 강화는 실제 장착 아이템의 개별 상태다. 장착 해제·그리드 이동·드롭·재획득 때 강화 수치를 보존한다. 강화 비용과 상한, 특성별 증가량, 난이도 배율은 Data Asset에서 조정한다. 특성은 PlayerState의 전용 ActorComponent가 소유하고 레벨 상승으로 얻은 포인트를 사용한다. 몬스터 처치 보상이 서비스 재화로 연결된다. 서비스는 마을의 대화 범위·생존·런 시작 전 조건을 검사한다.

영구 저장은 전용 SaveGame과 GameInstanceSubsystem을 사용한다. Pawn 준비 후 PlayerState→인벤토리→스킬→파생 스탯 순으로 한 번 복원한다. 변경 이벤트 자동 저장과 이동/종료 전 저장을 함께 사용한다. 저장 실패·버전 불일치·누락 에셋·잘못된 배치는 기존 저장을 덮어쓰지 않는다. 테스트는 사용자 저장 슬롯과 분리한다. 프로필 없는 에디터 플레이는 기존 세션 보급 정책을 유지한다.

## Ultrawork Packets

| 패킷 | 담당/수정 위치 | 완료 조건 | 검증 | 되돌림 경계 |
| --- | --- | --- | --- | --- |
| 1 | Architect / 이 문서 | 데이터 소유권·저장 복원 순서·작업 경계 확정 | 현재 Source/기존 Feature 결과 대조 | 이 문서 |
| 2 | Implementer / 새 TownProgression Component·서비스 Data Asset·TutorialNPC/UI | 안내 NPC에서 실제 서비스 실행·실패 사유·선택 수치 표시 | Editor 빌드·서비스 런타임 검증 | 해당 파일의 작업 전 사본 |
| 3 | Implementer / Progression SaveGame/Subsystem·PlayerState·Combat·Inventory·DroppedItem | 선택 프로필 격리·스냅샷 복원·강화 이동 보존 | Editor 빌드·분리 슬롯 저장/재시작 검증 | 해당 파일의 작업 전 사본 |
| 4 | Implementer / Character·RunPortal·BasicMonster·TutorialSupply | 생명주기 연결·난이도 적용·재화 보상·보급 중복 방지 | 실제 게임/포탈·회귀 검증 | 해당 파일의 작업 전 사본 |
| 5 | Builder→Reviewer→Doc Writer / 검증 도구·Feature/doc·Docs·DevLog | 실제 검증 결과·독립 검토·한계 기록 | BuildEditor.ps1·CheckGeneratedChanges.ps1 | 검증/문서 추가분 |

## 동시 작업과 검증

아자토르 개발은 원본 작업 폴더에서 진행 중이다. `Sprint#5-24-npc-services-persistence`의 별도 작업 폴더에서 현재 미커밋 Source/Config를 보존한 사본을 기준으로 개발한다. Content는 원본을 읽는 연결만 사용하며 바이너리를 파일 도구로 수정하지 않는다. 기본 소스 사본은 `tmp/baseline/Source`에 보존한다. 원본 반영 때 파일별 변경을 대조해 다른 작업의 변경을 덮어쓰지 않는다. 브랜치 번호는 Sprint5 main 병합 전 work24 기준이다.

컴파일/자동 검증은 UE5.8 명령줄 경로를 우선한다. 게임 코드 완료는 빌드와 실제 기능 검사로 판정하며 문서나 종료 코드만으로 완료하지 않는다. 에디터/빌드 동시 실행은 피한다. 커밋·푸시는 사용자 확인 없이 수행하지 않는다.
