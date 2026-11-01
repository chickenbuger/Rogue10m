# 정상 입력 경로의 자동 한 판 시도

상태: task 전용 미적용 초안. D 프로젝트 수정·빌드·UE 실행을 수행하지 않았다. 부모가 Alt 묶음을 완료한 뒤 별도 `Sprint#9-18` 작업 브랜치에서 검토·적용한다. 사용자 최신 지시는 자동 검증 계속, 직접 검증은 나중이다.

부모가 확보한 적용 전 기준은 `Sprint#9-18-basic-run-playtest` / `3fd15894`, primary 세이브321개와 원본31패키지 보존이다. 근거는 task의 `BASIC_RUN_BASELINE.json`이다. 이 자식이 해당 보존 검사를 실행한 결과라는 뜻은 아니다.

목표는 기존 정상 메뉴부터 실제 성장·전투·자연 종료·결과 UI 귀환까지 연결을 관찰하는 것이다. 기존 API fixture를 정상 플레이 완료로 대체하지 않는다. 승리 보장용 HP/XP/아이템/위치/무적/타이머 변경이나 임시 적·바닥 생성은 금지한다.

## 입력과 경로

- 메인 메뉴/생성/입장/학습/결과는 현재 보이는 실제 UMG 버튼 delegate를 자동 호출한다. 이름은 실제 입력 창에 Slate 문자 이벤트로 전달한다. OS 입력이나 Slate hit-test 성공으로 표현하지 않는다.
- 이동·달리기·공격·회피·점프는 Controller.InputKey pressed/released를 프레임 사이에 유지한다. 시야는 DoAim에 유한한 입력을 주며 실제 control rotation 반응으로 부호를 보정한다. 위치·회전 setter를 사용하지 않는다.
- NPC는 실제 접근 후 F로 열고 F로 정상 보급을 받은 다음 Esc로 닫는다. 보급은 기존 게임 동작이며 직접 인벤토리 지급하지 않는다.
- Hub 중앙 우물을 x650 경로로 우회해 포털에 실제 overlap한다. 필드 고정 대검은 실제 actor XY에 20cm까지 접근하여 overlap으로 획득하고 기존 GUID를 관찰한다. 장착 변경은 수행하지 않는다.
- 서쪽 도로로 실제 VerdantSlime에 접근하고 실제 공격/회피 입력으로 XP·처치 기록을 얻는다. 포인트가 생기면 실제 K 노드와 학습 버튼을 사용한다.
- 서쪽 도로/다리를 거쳐 보스 남문과 기둥을 우회한다. 지형·Z는 실제 충돌에 맡긴다. 보스 위치/HP/공격 상태는 읽기만 한다. 좁은 다리에서는 횡방향 stuck 복구를 하지 않는다.

## 실패와 범위

자연 사망·시간초과도 결과 UI 저장 gate와 귀환을 관찰하되 보스 승리로 승격하지 않는다. 보스 도달/첫 처치/대검/학습/귀환은 각각 별도 flag다. `full_boss_completion=1`은 모든 선행 milestone과 보스 접근·승리·귀환을 관찰했을 때만 기록한다. 자동 조종의 판단·조작성 부족과 게임 버그는 실행 CSV/실패 사진으로 분리한다.

Streaming 대기, 단계 timeout, 최대 3회 일반 점프 복구, 1,300~1,800초 wall watchdog을 둔다. 게임의 기존 1,200초 제한은 변경하지 않는다. 결과 버튼 저장 재시도는 최대 5회이며 직접 SaveNow/ReturnToTown을 호출하지 않는다. 포기 terminal은 예상하지 않은 경로로 차단한다.

## 보존과 증거

실행기는 정확한 fresh QA 슬롯3개, 새 출력 디렉터리, D 물리 경로 및 기존 UE 프로세스 부재를 확인한다. 모든 기존 `.sav`의 사전/사후 SHA256과 새 파일 namespace를 비교하며 변경이나 미확인 파일은 삭제하지 않고 보고한다. cleanup은 본인이 시작한 ProcessObject/PID/시작시각/경로가 일치할 때만 수행한다.

Driver는 단계 이벤트, 0.25초 상태 표본, 프레임별 의도/실제 key 상태와 이동/시야, 캡처 요청 단계/파일 확인 단계, 중간 summary를 보존한다. 실패 캡처도 유지한다. 캡처 완료·디코딩·육안 검토는 별도로 확인한다. 종료 초기화는 레벨/Brawler숙련/선택 학습 노드/해당 대검 GUID/프로필·phase/Discovery 범위이며 전체 아이템과 모든 기술을 검증했다고 확대하지 않는다.

부모 적용 후: read-only C++ 검토 → 빌드 → 새 label의 1차 실행 → 실제 실패 단계와 CSV 검토 → 필요한 정상 입력/연결 수정 → 새 label 재검증. 정상 보스 승리 실패를 수치 하향이나 setter로 통과시키지 않는다. 소프트웨어 설치·원격 push/병합은 하지 않는다.


## 실제 배치 NPC 연결 진단 패킷

정상 메뉴를 거친 A/B가 NPC 대화 단계에서 실패했고 원본 세이브는 보존했다. C 읽기 전용 런타임 진단에서 서비스320cm 안162.549cm, 실제DetectionArea650cm/query/PawnOverlap/GenerateOverlap=1/overlapping_pawn=1, pawn blocking=0/paused=0/local이동가능, widget=None을 확인했다. 임의 poll 또는 직접 대화 API로 통과시키지 않고 초기화·겹침·소유 이벤트 순서를 로그로 추적한다. 수정 필요 시 신규 핵심 규칙이 아니라 기존F NPC 경로 연결 복구에 한정한다. 원본CPP는 task/NPC_LIFECYCLE_ORIGINAL_1101.cpp로 byte복사하고 맵/자산은 변경하지 않는다. 완료조건은 정상StartHub NPC 안내+F대화+소모/종료경로와 기존39회귀, 루프driver 다음단계 진입이다.


확정 원인: BasicNpcTrace1101D에서실제BP가1.208초BeginOverlap→localController유효Activate→Widget생성→같은프레임Release. 캡슐 표면이650cm구체에 닿을 때 actor중심은650cm밖이므로 UpdateNearbyPlayer의중심거리분기가정상겹침을즉시해제하고타이머/위젯을없앤다. 이후구체안으로들어와도새BeginOverlap이없다. 제품수정은주변감지수명에actualDetectionAreaOverlap을사용하는것뿐이며320cm서비스거리/정책은유지한다. 임시추적CPP는task에보존후제거하고새poll/Tick/자산수정은추가하지않는다. 일반StartHub접근과외곽겹침회귀를별도로검증한다.


## 보스 입구 자동 경로 보정

E는NPC/포털/고정대검/실제처치XP/K학습을통과한뒤P=(13945.56,61296.51,9234.77)에서목표(15000,61000)으로가다3회정상점프후중단했다. 월드/세이브는보존. 읽기전용Editor검사에서입구벽Y62400/gapX13125..16875/기둥(15000,63700)을확인했고dirtyMap0이다. Python수직trace99개는BreakHitResult노출API불일치로좌표추출불가여서지형검증통과로쓰지않는다. 기존main road는(0,28000)→(15000,68000)의좁은gradedcorridor이며이전dogleg는그옆경사를가로질렀다. 맵/지형을바꾸지않고테스트waypoint를(13450,62100)→(13800,64000)→(14700,66000)으로보정해도로corridor와남문/기둥을통과하는지실제로검증한다. 재실패시currentfloor/forwardtrace를읽기로그로남기며이동조건·보스규칙·타이머는완화하지않는다.
