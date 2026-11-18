# 렌더링 입력 비교와 첫 공격 프레임 검증

기존 설정 기능의 검증을 완결했다. 제품 입력·카메라·공격 규칙은 변경하지 않았다. 일반 D3D11 창과 운영체제 창 포커스를 분리한 D3D11 Offscreen에서 엄격한 ON/OFF 비교가 모두 통과했다. 첫 공격의 긴 프레임도 촬영 없이 두 모드에서 재현했다. 정확한 세부 원인은 별도 계측이 필요하다.

## 입력 해제의 진단과 경계

기존 RenderC는 한 구간에서 W/Shift가 동시에 해제돼5항목이 실패했다. 당시 origin/flush 기록이 없으므로 정확한 원인을 소급 단정하지 않는다. 메뉴 종료와 같은 프레임의 입력 주입은 앞선 묶음에서 .3초 뒤 새 입력으로 분리했으며, 이번에는 원래 속도·거리·키 유지·카메라·PP 판정을 유지한 채 관측과 오염 검출을 강화했다.

공식 `FPlayerInputDebugging::OnPlayerInputFlushed`, 소유 `GameViewport::OnInputKey`, Slate focus/app 변화 observer를 QA 파일에만 추가했다. 직접 Controller 주입과 viewport-origin 이벤트를 구분한다. 측정 중 flush, 별도 viewport key, .01도 초과 ControlRotation 편차 또는 기록 누락을 실패로 판정한다. 스택은 주소만 메모리에 저장하고 모든 측정이 끝난 뒤 해석한다. 키를 반복 재주입하거나 flush·시야 입력을 차단해서 성공시키지 않았다.

InpNativeA1029: **569 확인항목 PASS**, 1643프레임. InpOffscreenA1029: **565항목 PASS**, 1567프레임. 항목 수에는 프레임 반복 검사가 포함된다. 각 네 구간 OFF→ON→ON→OFF가 실제920cm/s 달리기·순XY1380cm 이상·grounded를 유지했다. 카메라/trace, FOV(OFF90/ON86), 기존 PP 전체 동일성도 통과했다. 측정 중 flush/viewport 키/시야편차/기록누락은 모두0이었다.

native의 별도 양성 대조군에서 프로그램으로 소유 viewport의 `LostFocus`를 호출했다. 두 키 상태가1/1→0/0이 되고 정확히1개의 Flush가 발생했다. 저장한 스택은 `ControlledFocusProbe → UGameViewportClient::LostFocus → UEnhancedPlayerInput::FlushPressedKeys → UPlayerInput::FlushPressedKeys`를 확인한다. `ReceivedFocus` 후 새 press가 실제 이동을 복구했다. **실제 엔진 포커스 경계의 정상 입력 보호 동작**을 확인한 것이며, 과거 RenderC가 반드시 이 경로였다는 증거나 사람이 OS 포커스를 조작한 검증은 아니다. 이번 측정 구간에서는 게임 입력 버그나 자발적인 해제를 재현하지 못했다.

Offscreen은 UE의 `-RenderOffScreen` 실행 옵션이다. 설치 소스의 WindowsPlatformApplicationMisc173/NullPlatformApplicationMisc19와 SlateRHIRenderer321의 offscreen texture 경로를 확인했고, 실제 D3D11 초기화·PNG 출력을 검증했다. 프로젝트 설정이나 사용자 시스템 설정을 변경하지 않는다. 일반 native 창의 PASS와 구분해 기록한다.

## 첫 사용 공격의 긴 프레임

이전 Weighted QA는 공격 수락 직후 `Photo(09_actual_knuckle_attack,0)`를 요청하므로 그 기록만으로 공격 자체의 정지를 단정할 수 없었다. 새 전용 probe는 공격 전.5초부터 첫 피해 후1초 및 실제 회복까지 사진·QA 파일 쓰기·해시 폴링·스택 심볼화를 하지 않는다. 기존 엔진/게임 logging은 유지한다. Begin/EndFrame wall clock, GFrameCounter, world pre/post DT, 요청·수락·첫 피해·montage rate·실제 피해를 메모리에 기록한다.

새 프로세스 OFF→ON→ON→OFF 네 번, 각 최초 공격과 실제 action/CD/combo 종료 후 추가2.1초 이상 기다린 같은 primary 재사용을 측정했다. 공격속도 attribute1, authored montage rate1.34, 동일 primary01을 사용했다. 정확히1회 수락·실제 피해·원래 데이터/속도·프로필 유지·기존 피해 시간 oracle를 **142항목 PASS**로 확인했다. 모든 공격 관측 구간의 flush·별도viewport key·시야편차는0이다. 50ms는 관측 분류이며 기능 PASS 조건을 낮춘 값이 아니다.

아래는 관측 창에 완전히 포함된 BeginFrame 간격의 최댓값이다. 경계 간격은 별도 보존한다. OS/DDC/driver cache를 지우지 않았으므로 ‘새 프로세스의 첫 사용’이며 완전히 비워진 cold-cache 시험은 아니다.

| 라벨 | 모드 | 확인항목 | 최초 최대ms | 재사용 최대ms | >50ms 최초/재사용 |
| --- | --- | --- | --- | --- | --- |
| AttackOffB1029 | OFF | 35 | 76.744 | 38.325 | 2/0 |
| AttackOnB1029 | ON | 36 | 76.264 | 18.401 | 2/0 |
| AttackOnC1029 | ON | 36 | 74.924 | 17.726 | 2/0 |
| AttackOffC1029 | OFF | 35 | 74.802 | 18.316 | 2/0 |

원본 frame ID로 좁혀 분석한 결과, 네 번 모두 약74.8~76.7ms 간격은 **실제 수락 프레임**의 Begin→End와 대응했고 약62.6~67.9ms는 **첫 피해 프레임**과 대응했다. BeginFrame 간격은 직전 프레임의 실행을 포함하므로 현재 행 번호를 원인 프레임으로 오인하지 않았다. 재사용 네 건은50ms 초과0이며 최대17.7~38.3ms였다. 실제 첫 피해 world-time 오차는약.96~15.76ms로 원래 oracle 범위 안이다.

피해 프레임의 Begin→End 벽시계 처리시간은62.6~67.8ms인 반면, 그 프레임에 전달된 World DT는약16.7ms였다. 서로 다른 측정값이며, 게임 시간 기반의 첫 피해 oracle 통과를 프레임 정지가 없다는 뜻으로 해석하지 않는다.

따라서 촬영 혼입 없이도 두 모드의 공통 첫 사용 경로에서 긴 프레임을 재현했다. 새 무게감 모드만의 회귀로 분류할 근거는 얻지 못했다. Cast/Impact 효과의 최초 로드·렌더 준비 등은 후보일 뿐, CPU/렌더 thread 내부 원인은 이 측정만으로 확정하지 않는다. GPU 성능·일반 필드 FPS 보장·성능 향상률을 주장하지 않는다. 후속 원인 규명은 공통 수락/첫 피해 경로의 세부 프로파일링으로 한정할 수 있다.

## 변경·보존·실행 증거

- 변경 코드4파일은 FirstPersonSettings QA/검증 스크립트와 새 FirstAttackFrameProbe/MeasureFirstAttackFrames뿐이다. 제품 Controller·카메라·공격·세이브·맵·Content는 변경하지 않았다.
- UE5.8.3 CL58210709, MSVC14.44/SDK10.0.22621.0. 빌드A37.74초, B10.67초, 최종C10.89초 모두 PASS. Native/Offscreen은A에서 최종 input QA를 실행했다. 이후 바뀐 것은 첫 공격 probe의 계측 보강과 QA 이름뿐이며 input QA 소스 해시는 유지된다. 공격4회는최종C에서 실행했다.
- 첫 `AttackOffA1029`는 QA 이름 FirstAttackQa13자가 기존12자 제한을 넘어 초기화5항목 중1실패했다. 이름을 AttackQa8자로 교정했다. 게임 validation은 바꾸지 않았으며 실패 증거도 보존한다.
- 세이브226개+증거437개=**663파일**을 독립 복사했고 원본·복사본 SHA256을 재확인했다. 새 QA 포함 현재 세이브238개도 삭제하지 않는다. 기준 manifest는 task의 INPUT_VALIDATION_BASELINE.json이 가리킨다.
- native8장+offscreen8장 모두1280×720 PNG 디코딩 확인. native 대표 설정/확인창/두 달리기 화면과 offscreen 대표 설정/달리기를 시각 검토했다. 기능 폴더 `DevLog/20261029_InputFrameValidation`에 실제 사진5장(원본과 동일해시), 재현 README, verification.json을 남긴다. 과거 실패 사진/로그를 덮어쓰지 않는다.

원시 결과: `Feature/doc/evidence/first-person-settings-20261029/{InpNativeA1029,InpOffscreenA1029}` 및 `first-attack-frames-20261029/{AttackOffB1029,AttackOnB1029,AttackOnC1029,AttackOffC1029}`. 종합 verification은 `Feature/doc/evidence/input-frame-validation-20261029/verification.json`이다. 물리 입력·수동 OpenRunMap 완주·완전 cold-cache·정밀한 첫 사용 내부 원인은 검증하지 않았다. 새 질문/승인 대기 없음. 로컬 커밋만 하며 원격 push/병합은 하지 않는다.
