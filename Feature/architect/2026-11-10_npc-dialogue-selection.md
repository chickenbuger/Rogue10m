# NPC 대화 선택 기능 점검과 수정

## 요청과 확인된 문제
사용자가 NPC 대화 선택이 불가능했다고 보고했다. 기존 대화는 TextBlock만 렌더링하고 루트 HitTestInvisible, 포커스 false, GameOnly 상태여서 마우스 선택이 구현되어 있지 않았다. 기존 NPC 회귀는 F 열고 닫기, 입력 충돌을 검사할 뿐 선택지를 클릭하지 않는다.

## Ultrawork Packets
1. UI: 실제 버튼 7개와 보급/닫기를 추가한다. 키보드 위아래/Enter/F/Esc는 같은 서비스 경로를 사용하고 반복 Enter 구매를 막는다. 완료 조건은 실제 Slate hit test 버튼 클릭과 선택·실행이다.
2. 입력 수명: 컨트롤러가 대화 소유 NPC와 이동/시점 ignore 이유를 소유한다. 대화 중 커서와 GameAndUI 포커스, 전투 차단을 제공한다. trait 자식창 복귀, 다른 메뉴, 범위 이탈, 종료/사망 시 자기 이유만 해제한다. 기존 사용자 변경과 다른 ignore 이유를 보존한다.
3. 검증: 신규 VerifyNpcDialogue.ps1은 실제 StartHub 안내인과 데이터, 격리된 신규 저장 슬롯만 사용해 3인칭 렌더, Slate 키/포인터 hit test, 구매/강화/난이도/보급/복귀/종료/범위/사망과 저장 roundtrip을 검증한다. 기존 NPC 입력 충돌과 TownServices write/read도 회귀한다. 기존 저장 전체 SHA256 전후 보존을 검사한다.

## 경계와 롤백
대화용 C++ UI/NPC와 컨트롤러의 국소 입력 코드, 새 QA 소스/스크립트/문서만 변경한다. 바이너리 에셋은 변경하지 않는다. 시작마을 대기 타이머는 기존 정책을 유지하며 새 게임 pause를 추가하지 않는다. 특성 자식창은 기존 자기 메뉴 pause를 인정한다. UI 테스트는 실제 Slate 이벤트이며 사람이 직접 입력했다거나 전체 필드 플레이가 완료됐다는 증거가 아니다.

## 빌드와 검증
공용 에디터 lease와 병렬 작업자 native freeze 후 Scripts/BuildEditor.ps1. 이후 VerifyNpcDialogue.ps1 -Label <fresh>, VerifyNpcInteraction.ps1 -Label <fresh>, ValidateTownServices.ps1 -Label <fresh>. 직접 OS 입력 점검은 별도 수동 모드와 실제 화면으로 확인한다. 최종 결과·미검증과 스크린샷은 짝 문서에 기록한다.

## 개발 재개 F2
- 기존 작업 브랜치와 미커밋 변경을 보존한다. 공유 Unreal/빌드 프로세스 없음 확인 후 단일 빌더로 진행한다.
- 중단된 F 빌드의 Town/Slot/Character 지역변수 C4458 세 건을 의미 변경 없이 교정한다. 기존 소스는 사용자 Temp에 별도 보관했다.
- Editor 통합 빌드, 실제 NPC 선택 QA, NPC 입력 충돌 및 마을 서비스 저장 왕복, 신규 카탈로그 포함 8개 단위 검사를 순서대로 실행한다. 수치와 실제 캡처를 구분한다.


## 통합 회귀에서 추가된 작업 단위
- 고정 슬롯 입력: 실제 Q 정상 실행 대조군이 사용/자원/쿨다운 모두0인 것을 근거로 Character의 누락된 Q/E/R press/release 연결만 추가한다. 기존 Combat의 슬롯·해금·뉴비·메뉴·사망 권위 검증과 modifier 거부를 유지한다. 완료 조건은 Q/E/R 각 실제1회, LeftShift 차단·해제후 정상, 메뉴중 입력과 닫은뒤 release-only의 추가 실행0이다. 롤백 경계는 Character.cpp의 고정3키 loop/작은 wrapper2개와 .h 선언이다.
- 저장 회귀 현대화: 로비 재화·내공/숙련 기반 성장으로 바뀐 현재 계약에 맞춰 과거 레벨 포인트·사용횟수만 채우던 QA를 교정한다. 예산 seed는 정확 격리QA 슬롯에서만 수행하고 synthetic 숙련 이벤트와 함께 비실플레이 fixture라고 기록한다. production 규칙은 변경하지 않고 정상 Attach/구매/해금/SaveNow/독립Read를 검증한다. 롤백 경계는 TownServicesRuntimeTest.cpp다.
- 최종 검증: F6 통합 Editor 빌드 → 최신 NPC 입력 회귀 → Town Write/Read → NPC 실제 서비스 선택 → 3인칭/마우스13경로 → 슬롯/카탈로그/성장/인간형8개 자동검사. 단계별 결과와 원본398저장 SHA-256을 별도 manifest에 남긴다.
