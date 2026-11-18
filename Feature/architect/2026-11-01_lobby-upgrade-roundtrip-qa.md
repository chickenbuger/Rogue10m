# 정상 UI 로비 강화 왕복 QA — 실행 전 초안

종료와 세이브 보존이 확인된 BasicRun1101H/I QA 프로필만, 종료 증거와 정확 namespace/FGuid/허용 신규 저장/해시를 확인한 뒤 새 QA namespace에 원본 바이트 그대로 복제한다. 원본 QA와 사용자 저장은 읽기 전용으로 보존한다. 복제 코드는 작성만 하며 task 준비 중 실행하지 않는다.

정상 L_Menu의 기존 캐릭터 선택→StartHub 도보 NPC F/Down3/Enter→실제 Power 카드/구매 버튼→정상 저장 gate→포털 도보→다음 런 LV1·Power 유지→설정 포기 확인→결과 저장·마을 복귀·Discovery 보존을 검사한다. 현재 비용/상한/저장 규칙을 읽으며 잔액이 부족하면 실패를 기록한다. 돈·XP·rank·HP·위치·무적 setter, 프로필 생성, 직접 구매/SaveNow/포기/여행 API는 호출하지 않는다.

UMG의 보이는 실제 버튼 delegate와 Controller.InputKey를 프레임 간 유지/해제하며 시야는 기존 Character DoAim 입력 경로를 사용한다. 물리 입력으로 표현하지 않는다. QA snapshot 주입 없이 이전 실제 정산 바이트 복제만 시드다. 스크린샷은 실제 게임뷰 요청이며 디코딩·육안 검증은 별도다.

launcher는 source metadata와 source 두 저장 파일을 교차 검증하고 신규 경로에 overwrite 없는 CreateNew를 사용한다. clone 초기 SHA 동일성은 UE 실행 이전에 기록한다. UI 실행 후 clone 변경은 정상 검사 대상이며, 기존 모든 원본 SHA는 그대로여야 한다. 본인 PID/starttime/path를 재검증한 bounded cleanup만 허용한다. 삭제/reset/원본 속성 변경은 하지 않는다.

Root integration: independent companion QA within the normal-UI packet branch. Existing gear files are not overwritten. Before execution, use the H/I proven normal walking waypoints around the central well; the direct NPC-to-portal chord is not used. No state/position injection or acceptance gate relaxation. Compile only while YouTube owns global UI; source QA copies and runtime remain unexecuted until handoff.
