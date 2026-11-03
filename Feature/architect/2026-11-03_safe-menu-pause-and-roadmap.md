# 남은 개발 계획과 안전 문맥 메뉴 정지

기준: UE 5.8, e5ec1703, 기존 Sprint#9-24 작업은 보존하고 Sprint#9-25-safe-menu-pause에서 진행한다. 기존 미커밋 next-run QA 두 파일과 11월1일 기록, 미추적 증거를 지우거나 이번 기능으로 포함하지 않는다. 저장파일 원본 해시는 task/baseline에 별도 보존한다.

## 확인된 상태와 우선순위

1. P0: 동일 프로필의 다음 판 연결 QA. 기존 #9-24 드라이버를 재사용해 첫 승리 → 저장 → 마을 → 새 RunId/너클/1200초/성장·기술 초기화/Discovery 유지 → 실제 포기 UI/귀환을 검증한다. 현재 실제 연속 실행은 미완료다. 녹화는 취소됐으므로 재시도하지 않는다. 공유 PC 입력 소유권을 먼저 확인해야 하는 별도 실행 packet이다.
2. P1a(이번 구현): 출발 전 StartHub에서 메뉴를 열 때만 월드 정지. 필드 메뉴는 월드·AI·20분 타이머가 계속된다. 외부 pause 소유권과 중첩 창, pause 상태의 닫기 입력, 종료를 검증한다.
3. P1b: Ctrl/Q/E/R 4액티브와 무기별 런 프리셋. 기존 좌/우 기본공격·차징 enum을 재사용하지 않는다. Learned만 장착하고 Discovery를 사용권으로 보지 않는다. Ctrl 좌우 키 정책과 비전투 편집 provider가 미정이므로 이를 확정한 뒤 입력/HUD/장착/교체를 함께 검증한다. Alt 회피와 무기 교체 gate/0.5초 floor는 구현됐으므로 재구현하지 않는다.
4. P1c: 상위 기술 Discovery 이전 조건 ??? UI. 공개 대상 기술 분류를 명시하고 기존 영구 정보 기록을 재사용한다. 모든 기존 노드를 임의로 숨기지 않는다.
5. P2: 보스 직행 기본 이동 약60초 실측, 특정 기술이 없어도 가능한 보스 대응, 1인칭 손·몸통 가림/블러/접촉·회복 시점 기록. 실제 입력과 자동 전략, 사람 플레이를 구분한다.
6. P2: 필드 상점·의뢰·비급·선택형 보스·장비 획득 확장. 가격/개수/희귀도 표·정확한 위치는 미정이며 임의 확정하지 않는다.
7. P3: 마나/내공/투지 호환성과 돌파·융합. MageRank 획득·미니게임·손실 수치·최초 보상 금액은 미정이다. 이후 cook/package, 콜드 실행, 해상도, 저장 실패, 전체20분·사람 직접 플레이 QA를 수행한다.

## ULW packet A — 제품 구현

- 목표: 현재 명시적으로 안전한 출발 전 마을 메뉴의 시간 정지 계약을 연결한다.
- 입력: RapidFieldRules.md의 안전 문맥+메뉴 조건, 기존 Controller 창 상태, GameState phase.
- 수정 위치: GameMode 및 PlayerController의 h/cpp 네 파일. 에셋·맵·저장 스키마·밸런스는 변경하지 않는다.
- provider: PIE prefix를 제거한 정확한 /Game/Rogue10m/Maps/StartHub 패키지 + Standalone + WaitingToStart. 이름만 같은 다른 맵과 필드 대기 상태는 안전으로 가정하지 않는다. 새 필드 안전 volume은 후속 범위다.
- 대상: 인벤토리/장비/스킬/설정/영구 특성 창. 현재 NPC 대화 자체·미래 별도 상점은 후속 연결 대상이다.
- 소유권: GameMode가 컨트롤러별 엔진 pause delegate handle을 저장한다. 마지막 창 닫기/phase 전환/EndPlay는 정확한 handle만 해제하고 전역 ClearPause를 호출하지 않는다. 메뉴 도중 추가된 외부 Controller pause도 별도 등록한다.
- 입력: I/P/K/O/Esc/F10을 pause 중에도 실행한다. 기존 held 기술 취소를 유지한다.
- 완료 조건: 마을 idle은 흐르고 메뉴 중 월드 시간이 정지하며, 중첩 창 마지막 닫기에서만 재개한다. 외부 pause는 살아 있고 필드 메뉴 중 타이머는 흐른다.
- 검증: Scripts/BuildEditor.ps1, 새 격리 runtime QA, 기존 메뉴/재화/무기 회귀, Scripts/CheckGeneratedChanges.ps1.
- 되돌림 경계: 이번 네 제품 파일의 기능 diff와 신규 QA/문서만. 기존 WIP/원본 에셋/세이브는 보존한다.

## ULW packet B — 증거와 통합

- 신규 QA namespace와 bounded 프로세스를 사용한다. 기존 세이브 전후 SHA256, 실행 로그, 결과 수, 가능한 실제 스크린샷을 보존한다.
- nullrhi fixture와 렌더링, 정상 입력 완주, 사람 물리 입력을 구분한다. 빌드 성공을 플레이 성공으로 기록하지 않는다.
- Feature/doc, SprintChangeLog, 20261103 DevLog를 한국어로 추가한다. 원격과 기존 통합 상태를 확인하고 이번 기능만 커밋한다. 미검증 #9-24 WIP를 자동 승격하지 않는다.

리뷰 보완: 메뉴가 pause되기 전에 마우스 좌/우 차징과 brawler 입력 버퍼도 즉시 취소한다. 이미 수락된 hit timer/몽타주/공격 행동 시간/쿨타임은 유지한다. 관련 Combat/BasicBrawler h/cpp 네 파일이 추가되어 제품 소스 범위는 총8개다. 첫 제품 빌드는 성공했고 후속 지역변수명 충돌은 최소 수정 후 재검증한다.
