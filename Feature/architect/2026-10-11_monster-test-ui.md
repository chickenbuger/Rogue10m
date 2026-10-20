# 현재 위치 몬스터 테스트 UI

2026-10-11 · Sprint#5-26-monster-test-ui · Architect

## 목표와 범위

넓은 OpenRunMap에서 몬스터를 찾아 이동해야 하는 불편을 해결한다. 개발 빌드의 F6 테스트 창에서 실제 Encounter Blueprint 6종을 가까운 안전한 지면에 한 마리씩 소환한다. 아자토르 기본/정면 파동/심연 영역을 선택 실행하고 HP40% 강화 상태를 만들 수 있다. 테스트 대상만 정리하고 플레이어 회복·자동 AI 전투를 제공한다. 창을 열면 단독 플레이를 일시정지하고 실행 버튼은 창을 닫아 정상 시간으로 공격을 관찰하게 한다.

Shipping과 원격 클라이언트에서는 기능을 생성하거나 실행하지 않는다. 테스트 몬스터의 토벌·EXP·골드 보상은 차단한다. 기존 맵 몬스터·원본 Data Asset·스튜디오를 변경하지 않는다. 일반 스킬 사용 수련은 기존 규칙을 따른다. 배치 장소가 막혀 있으면 실패를 표시하고 벽을 통과하거나 공중에 강제 소환하지 않는다.

## ULW 패킷

| 역할/패킷 | 수정 위치 | 완료 조건 | 검증 | 되돌리기 경계 |
| --- | --- | --- | --- | --- |
| U1 Architect | 이 문서 | 입력/소유권/중단 조건 확정 | 실제 Controller/AI/패턴 코드 조사 | 새 문서 |
| U2 Implementer | 새 CombatTestComponent, 새 CombatTestWidget, Controller 최소 연결 | F6·버튼·일시정지·안전 소환·회복·정리 작동 | Editor 빌드, 실제 게임 UI | 새 클래스와 Controller 연결부 |
| U3 Implementer | BasicMonster 보상 플래그, Pattern 테스트 진입점 | 테스트 대상만 지정 패턴/보상 차단, 일반 공격 보존 | 실제 대상과 일반 대상 비교 | 플래그/테스트 진입점 |
| U4 Builder/Reviewer | 전용 런타임 QA | 6종 소환·간격·패턴·강화·보상·입력·정리·화면 통과 | BuildEditor.ps1, 전용 QA 실행 | 전용 검증 파일 |
| U5 Doc Writer | Feature/doc, DevLog, SprintChangeLog | 사용법/실제 검증/한계 기록 | diff --check, CheckGeneratedChanges | 추가 문서와 로그 |

## 소유권과 충돌 방지

마을 서비스 채팅은 tmp의 별도 작업 폴더에서 검증 중이며 원본에는 영구 저장 코드가 반영되어 있다. 기존 Controller/UI 전체 재작성 대신 새 컴포넌트와 새 C++ Widget에 기능을 분리한다. 본 작업은 테스트 대상 생성/보상 차단/패턴 진입점과 Controller의 F6·focus·blocking 상태 연결만 소유한다. 빌드/Editor 호출은 순차 실행한다. 기존 미커밋 변경과 콘텐츠를 보존하며 커밋·푸시는 하지 않는다.

## 설계

- 컴포넌트는 Weak 참조로 자신이 생성한 대상만 추적하고 최대 6개로 제한한다. 기존 대상이 죽으면 새 대상 소환 가능. 정리는 Destroy로 공격 타이머/데칼/AI Controller를 함께 제거한다.
- 클래스와 소환 거리는 EditDefaultsOnly 데이터로 노출한다. 가로 화면 우측 작은 힌트 버튼과 F6을 함께 제공하며 창은 화면에 맞춰 축소 가능하게 한다.
- 소환은 현재 view 방향의 여러 후보를 ground trace와 capsule overlap으로 검사한다. 자동 AI는 기본 정지. 선택 패턴은 기존 공격 파이프라인을 사용하며 테스트 대상에게만 쿨타임을 초기화한다.
- 창을 열 때 기존 UI 입력 정리, 이동 중단, 마우스 표시, pause 소유권 기억. 닫기/ESC/F6/다른 메뉴/EndPlay 때 자신이 건 pause만 해제한다. 패턴과 자동 전투는 먼저 창을 닫고 실행한다.
- 회복은 살아 있는 현재 Pawn만 대상으로 하며 사망 후 부활을 위장하지 않는다.

## Exit Gate

실제 로드된 게임 DLL에서 버튼 경로와 F6 입력, 소환된 종의 실제 DA/메시/패턴, 전투/경고/보상 차단과 정리, 화면 크기를 확인한다. 실패 검증은 그대로 남기고 수정 뒤 재검증한다. 최종 에디터는 플레이 종료·미저장 없음으로 돌려놓는다.

## 설계 보완과 결과

플레이어 무적 토글을 추가하고 캐릭터 교체·EndPlay에서 원래 CanBeDamaged를 복원한다. F8은 에디터 PossessEjectPlayer와 충돌하므로 F6을 사용한다. 초기 QA의 저장 접두사와 추가 Slate 마우스 경로 오류는 엔진 소스/프로젝트 규칙 검토 후 수정했다. 최종 Editor 빌드 성공, F6 실제 게임80확인 실패0, 1920×1080 추가 검증80확인 실패0, 실제 PIE 로드·공식 MCP 화면 확인, OpenRunMap·PIE없음·dirty0로 Exit Gate 통과. 전체 결과와 사용법은 Feature/doc/2026-10-11_monster-test-ui.md에 기록했다.
