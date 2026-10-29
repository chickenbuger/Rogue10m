# 입력 해제와 첫 공격 프레임 검증

목표는 새 기능을 추가하지 않고 1인칭 설정의 렌더링 비교를 완결하는 것이다. 기준은11a15011, Sprint#9-12-input-validation이며 세이브226개와 기존 증거를 합친663파일을 별도 task 보존 폴더에 복사·해시 검사했다. 원격 push/병합, 캐시 삭제, 설치, 생산 맵·세이브 형식 변경은 하지 않는다.

## 입력 진단 묶음

FirstPersonSettings QA에만 공식 PlayerInputDebugging Flush hook와 소유 viewport의 InputKey listener를 연결한다. 직접 QA주입도 따로 기록해 실제 release 이벤트와 일괄 flush를 구분한다. stage/segment/world/wall/frame/현재키/시뮬레이션 여부/포커스/viewport상태를 기록하고, 스택은 raw 주소를 메모리에 캡처한 뒤 측정 종료 후 해석한다. 기존 이동 속도·거리·ground·키 유지·카메라·PP assertions는 완화하지 않는다. 입력을 자동 재주입해 실패를 치유하거나 제품의 flush 설정을 끄지 않는다.

native window 진단과 엔진의 RenderOffScreen 경로를 구분해 비교한다. 후자는 Windows NullApplication와 Slate offscreen RHI texture 경로를 사용하는 QA 실행 선택이며 게임 카메라/입력 규칙 변경이 아니다. 실제 지원·RHI·PNG를 실행으로 확인하고 미지원이면 정확히 기록한다. 네 구간 ON/OFF와 실제 달리기를 유지하는지 검사하며, 포커스/외부 입력 혼입이 있으면 해당 표본을 성공 비교에서 제외하고 전체 실행을 실패/오염으로 기록한다.

## 첫 공격 계측 묶음

별도 작은 QA probe에서 새 프로세스의 첫 사용과 같은 기술 재사용을 OFF/ON으로 비교한다. 공격 직전0.5초부터 첫 피해 후1초까지 스크린샷·디스크쓰기·스택 심볼화를 하지 않는다. Begin/EndFrame wall time, world pre/post delta, 입력 요청·수락·첫 피해·montage rate를 메모리에 모아 종료 후 기록한다. 기존 정확히1회 수락·실제 피해·authored rate/data·첫 피해 timing oracle를 유지한다. 50ms 초과는 재현 관측이며 임의의 기능 PASS 임계값으로 쓰지 않는다. OS/DDC/driver cache를 지우지 않으므로 cold-cache 시험이라고 부르지 않는다.

## 검증·종료 경계

UE Editor 빌드, 진단 native 실행, 격리된 렌더링 ON/OFF 비교, 필요한 첫 공격 paired probe, 독립 코드·증거 검토를 수행한다. 실패 결과도 보존한다. DevLog 기능별 사진/README/verification과 Feature 결과 문서, SprintChangeLog를 갱신하고 소스·도구·문서만 로컬 커밋한다. 기준663파일 해시, QA namespace, 실행한 소스 해시와 소유 UE 프로세스 종료를 확인한다. 물리 입력·수동 필드 완주로 표현하지 않는다.
