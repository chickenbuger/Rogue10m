# 정상 인벤토리 대검 장착 UI 검증 — 체크포인트

Sprint#9-22-gear-ui-input-qa, 기반3a306c8c. 이전 자동 플레이는 고정 대검을 획득했지만 너클로 계속 싸웠으므로 실제 인벤토리 타일 우클릭 경로를 검증한다. 제품 변경은 InventoryItemWidget의 const 식별 getter2개뿐이다. 테스트는 실제 획득 GUID/컨테이너, local window, Slate hit-test/ancestor/leaf, 우클릭, 같은 GUID의 MainWeapon 단일 장착과 grid제거, GreatSword 타입 및 실제 Primary 바인딩 전환을 모두 요구한다. handled reply만으로 성공 처리하지 않는다.

J는 메뉴·NPC F·필드·고정 대검 획득·자연 처치 경험치·UMG 기술 학습까지 진행했으나 gear_open_inventory에서 시간 초과했다. 인벤토리와 대검이 사진에 보이지만 click_routed0/equipped0이며 결과는 blocked이다. OSexit0이나 wrapper는1로 종료했다. 346개 기존 저장 모두 해시 유지, 새 QA2개만 추가되어348개. 보스·정산·장착 완료는 이번 실행에서 검증하지 않았다.

실패 원인 후보를 분리했다. UE는 hidden/minimized native window를 LocateWindowUnderMouse 대상에서 제외하며 J는 offscreen/Hidden이었다. 별도로 UUserWidget의 기본 SelfHitTestInvisible 수신자를 정확히 Visible만 허용하던 QA 조건은 합법적인 child-hit→bubble 수신을 제외할 수 있다. 당시 guard별 로그가 없어 J의 유일 원인을 확정하지 않는다.

후속 후보는 receiver 조건만 IsVisible로 정정하되 HitTestInvisible ancestor, 실제 native창 hit-path와 leaf/children/enabled, 고유 GUID,15초 stage제한, 실제 장착·바인딩 결과 검사는 유지한다. 거부 원인별2초 제한 로그와 RMB직전once hit-path 로그를 추가했다. 실제 포인터 검사를 보장하려면 launcher는 GearUI에 -Visible을 요구하며 native 숨김 가드를 우회하지 않는다.

최초 진단 빌드는 인자 Reason의 C4458 shadow로22.87초 후 실패했다. GuardReason 이름만 고쳐11.84초 실제 QA CPP 재컴파일 통과. 현재 K는 보이는 UE 창/단독 UI 제어 반환 전까지 미실행이다. 프로그램 입력을 물리 입력으로 표현하지 않는다.

별도 headless 회귀30개(공격4/성장3/필수조건4/쿨타임5/교체8/포기재화6)는 실패·경고·미실행0이었다. 이는 진단 전 같은 제품 코드의 빌드 기준이며 후속 QA-only 진단의 실제 창 검증과 분리한다. 원본31에셋/OpenRunMap 해시는 그대로다. 기존 새 owner-arms/view-framing은 기본OFF이며 그 UI 노출/화면 품질이 이 패킷으로 해결된 것은 아니다.

로비 강화 동반 검증은 2026-11-01_lobby-upgrade-roundtrip-qa.md 참조. 실패 사진과 보존 자료: DevLog/20261101_GearUi_Review/FailedJ 및 DevLog/BasicRunPlayQA/BasicRun1101J. 실제 사진2장을 열어 확인하고 decode/hash를 기록했다. 보스 승리·수동 완주·4액티브·60초 직행맵·패키징은 미완료다. 다음 재개 첫 단계는 UI 반환 확인 후 fresh K -PreparedBossStrategy -AnimationEvidence -GearUI -Visible 단일 실행이다. push/병합 없음.
