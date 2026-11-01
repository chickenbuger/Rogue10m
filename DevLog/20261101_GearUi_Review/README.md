# 대검 장착 UI 자동 입력 검증 — 진행 중

J는 인벤토리 열기까지 진행했으나 우클릭 전에 시간 초과했습니다. 실패 화면과 원기록은 FailedJ에 보존했으며 성공 증거로 분류하지 않습니다. 기존346개 저장 해시 모두 유지, 새 QA2개만 추가되어348개가 됐습니다. OS 종료코드는0이나 driver outcome은 blocked이고 launcher는1로 종료했습니다. 정상 메뉴·NPC F·필드 진입·고정 대검 획득·자연 처치 경험치·UMG 기술 학습까지 확인했고, 이번 실행의 보스/정산/장착은 미검증입니다.

Native 숨김 창은 UE의 실제 포인터 검사 대상에서 제외됩니다. 별도로 UUserWidget의 SelfHitTestInvisible 수신자를 정확히 Visible만 허용하던 QA 검사도 합법적인 child→bubble 경로를 제외할 수 있습니다. J에는 당시 guard별 런타임 로그가 없어 두 후보 중 유일 원인을 확정하지 않습니다. 제품 UI의 표시/장착 규칙은 변경하지 않았습니다.

다음 후보는 실제 hit leaf/ancestor/고유 GUID/실제 장착·스킬 전환 검사를 모두 유지하고, receiver 역할 검사와 거부·성공 경로 진단만 보정합니다. 첫 진단 빌드는 인자 이름 C4458로 실패했고 GuardReason 이름 정정 후11.84초 빌드 통과했습니다. 실제 native 창 검증은 -Visible을 요구합니다. K는 UI 단독 제어 반환 전까지 미실행입니다.

작업 사본: D:\Project\Rogue10m. 브랜치 Sprint#9-22-gear-ui-input-qa, 기반3a306c8cfdf68bef5c79ac64acae13e623ec7e9b. 물리 입력은 사용자가 추후 진행하기로 했습니다. 자동 입력을 수동 플레이/보스 승리/전체 완주로 표현하지 않습니다. 원격 push·병합 없음.
