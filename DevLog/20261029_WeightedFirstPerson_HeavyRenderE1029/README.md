# 선택형 1인칭 무게감 프로토타입

검증 UTC: 2026-10-29T13:46:31.879582+00:00. 프로젝트 D:\Project\Rogue10m, 브랜치 Sprint#9-10-weighted-first-person. SHA는 verification.json 참조.

현재 기본은 기존 카메라다. 콘솔 `Rogue10m.FPHeavy 1`로 시험하고 `Rogue10m.FPHeavy 0`으로 돌아간다. `Rogue10m.FPHeavyMotionScale 0..1`과 `Rogue10m.FPHeavyFOV 70..110`으로 조절하며 FOV 0은 프로필86도를 사용한다. 수치는 초기 추천값이다. 기존 시점도 이미1인칭이며 이번 변경은 느낌과 안정화의 선택형 보완이다.

현재 맨손 표시를 유지한다는 사용자 답변을 반영했다. 너클 계열/시작 무기 데이터는 그대로며, 이후 장착 아이템별 모델·소켓 연결은 별도 확장 대상이다. 새 모델을 구매·생성·import하지 않았다.

Editor 빌드/회귀17개 PASS. 일반속도 렌더 QA 3400항목·2배속/30fps 상한 QA 1666항목 PASS(반복 프레임 검사 포함). 실제 DoAim 누적 yaw17.5/pitch10은 ON/OFF 동일, 실제 target 피해 각각 22.778320/21.682617, 첫 피해 오차 0.069214106/0.010696750초, 접촉 손 pitch0/0. 이는 프로그램 InputKey·DoAim 경로이며 물리 입력이나 수동 완주가 아니다.

PNG9장은 실제 UE D3D11 viewport이며 1280×720 열기·디코딩을 확인했다. 걷기/달리기/명중/메뉴/검사용3인칭/대검 교체/OFF 복원을 기록한다. 09는 공격 승인 직후 요청한 캡처이며 별도의 정확한 접촉 시각 사진이라고 주장하지 않는다. 최초 명중 표적은 근접 fixture이며, 명중 후 표적을450cm 전방/100cm 옆으로 옮겨 후속 화면을 가리지 않게 했다. 기존 달리기 blur/fringe가 남아 있다. 전체 벽/무기 clipping이나 최종 아트 품질 검증 자료가 아니다. Idle에는 기존 자연스러운 팔 내림 포즈가 유지된다.

재현: Scripts/VerifyWeightedFirstPerson.ps1 -Label <새값> -Rendered. 빠른공격은 -AttackSpeed 2 -MaxFPS 30. 세 종류 QA 슬롯과 전체 기존 저장 해시 감시를 사용한다. 기록516파일(원본세이브204+기존증거312) 해시 불변. 원시 로그/실패 기록은 Feature/doc/evidence/weighted-first-person-20261029/에 별도로 보존한다.

봉인된 로컬 커밋 `021921ffda2f100d1d14bbdb5dd10666c2e07e21`. 추적 파일 clean, UE QA 프로세스 없음. 원격 변경 없음.
