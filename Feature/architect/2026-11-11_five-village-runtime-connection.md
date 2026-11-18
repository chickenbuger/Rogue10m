# 다섯 마을 런 연결 우선 구현

상태: **신규 소스 구현·독립 C++52검사 통과, UE 통합 컴파일/활성화 대기**. 공유 UE/빌드 미사용.2026-11-11.

## Scope Gate

확인된 후보는92액터 그레이박스이며 EngineGameModeBase를 사용한다. 기존 StartHub 포털은 OpenRunMap으로 간다. 새 전용 포털·GameMode와 PlayerState 소유 방문 기록을 추가해 생산 캐릭터·3인칭·1200초 런으로 연결할 준비를 한다. 기존 포털/맵/게임모드/플레이어컨트롤러/세이브/무기 소스를 수정하지 않는다. 공유 에디터와 UBT는 다른 담당자가 소유하므로 실행하지 않는다.

현재 work 브랜치는 Sprint#9-29-humanoid-boss-npc다. 신규 고유 파일만 작성하며 기존 사용자 WIP를 보존한다. C++의 UE 통합 컴파일·반영과 에디터 배치는 담당자의 통합 단계로 명시한다.독립 상태 모델 테스트와 Python AST/정적 검사는 실제 UE 플레이로 표현하지 않는다.

## 구현 계약

1. 새 RunPortal subclass는 기존 입장 기준 저장·보급 캡처·StartRun=1 이동을 상속한다. 대상은 후보 전체 패키지이며 URL game 옵션으로 새 GameMode를 선택한다. 기존 포털은 유지한다.
2. 새 GameMode는 Rogue10mGameMode를 상속한다. 기존 생산 Pawn/컨트롤러/프로필/정산 흐름을 유지하고 카메라를 바꾸지 않는다. 직접 PIE 자동 런 시작과30초 테스트 override를 켜지 않는다.
3. PostLogin에서 PlayerState에 전용 ProgressComponent를 한 번만 추가한다. 캐릭터가 바뀌어도 같은 PlayerState의 방문 기록을 유지하며, 런 Running 시작 때 새 기록으로 초기화한다. 방문 수는 통로·보스 입장 조건으로 쓰지 않는다.
4. Data Asset 정의에 마을 ID/위치/반경과 보스 접근 지점을 둔다. 이번5마을 기본값은 기존 로컬 배치와 일치한다. 보스 접근 반경 관측은 실제 입구선 통과·60초 보행 실측과 구분한다. 의뢰 수락/보상/상점 기능을 방문 기록으로 대체하지 않는다.
5. 에디터 적용 스크립트는 새 포털만 StartHub에 추가할 수 있도록 준비한다. 원본 포털·위치·대상은 변경하지 않는다. 공유 에디터 사용권과 새 클래스 빌드 확인 후에만 실행한다. 같은 태그 포털이 이미 있거나 미저장 패키지가 있으면 중단한다.

## Ultrawork Packets

| Packet | 목표·변경 위치 | 완료·검증 | 롤백 경계 |
| --- | --- | --- | --- |
| V0 조사 | 기존 GameMode/포털/정산/내비게이션 읽기 | 현재 구현과 공용 수정 필요점 기록 | 변경 없음 |
| V1 상태 구현 | 신규 RouteDataAsset/ProgressModel/ProgressComponent | 독립 상태 모델 테스트, 소스 검토. UE 빌드는 담당자에게 인계 | 신규 파일만 |
| V2 진입 구현 | 신규 FiveVillageGameMode/RunPortal | 상속·중복 부착·여행옵션 정적 검토와 후속 UE Automation | 신규 파일만 |
| V3 에디터 활성화 준비 | 신규 전용 Python 적용 코드 | AST·경로/dirty/기존 포털 보존 guard 검토. 실제 실행 금지 | 신규 코드/후속 태그 포털만 |
| V4 통합·결과 | 담당자 빌드/Editor, feature doc·DevLog·SprintChangeLog | 정상 Hub 진입→1200초→생산3인칭·방문/재방문→종료/귀환 | 새 포털·새 클래스·후보 설정만 |

## 확인된 통합 제약

- 기존 OpenWorldBootstrap의 Navigation 설정은 OpenRunMap+WorldPartition 전용이다. 후보의 NavMeshBounds/Recast와 AI 추적을 별도로 준비해야 한다. 검증되지 않은 보스를 자동 스폰하지 않는다.
- 최종 보스 후보는 기존 BP_OW_FinalBoss다. 실제 FinalBoss data/profile과 인간형 연결, 정상 사망→정산을 에디터 담당자가 확인해야 한다. 파일 존재를 현재 소유·라이브 연결로 세지 않는다.
- IsSafeMenuContext는 nonvirtual이며 StartHub 대기만 허용한다. subclass로 이름만 가려서 필드 마을 pause를 구현했다고 주장하지 않는다. 후속 공용 확장 지점 조율이 필요하다.
- 후보 cook 포함은 미검증이다. 패키징 시 명시적 cook/참조와 실제 여행 검증이 필요하다.
- 정상 Hub 프로필/입장 기준 저장 없이 직접 PIE로 들어간 실행은 정산 통과 증거가 아니다. 기존 실제 저장은 삭제·초기화하지 않는다.

공유 빌드·에디터 사용권이 없는 동안 source/격리 테스트까지만 수행한다. 기존 월드 연결 완료·60초/15분·서비스 구현·보스전 완료는 별도 실제 근거가 있어야 한다.
