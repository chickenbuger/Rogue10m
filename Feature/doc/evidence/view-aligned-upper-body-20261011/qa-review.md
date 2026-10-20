# 시선 기준 상체 애니메이션 독립 QA 리뷰

- 검토일: 2026-10-11, 2026-10-13 재개 후 실제 렌더 최종 확인
- 역할: 기능 검증 2. 구현 및 엔진 실행과 분리해 소스, 엔진 헤더, 실제 출력 증거를 검토했다.
- 상태: 원본 대조 제어·집중 런타임·부정 대조·실제 렌더·최종 Editor 빌드·이동/가드/카메라 회귀 독립 확인 완료. 루트가 마지막 일반 공격 회귀·기존 Editor 복원·최종 보존 검사도 통과한 기록을 추가했다.

## 구현과 대조의 구조

UE 5.8의 `FAnimInstanceProxy::EvaluateAnimation_WithRoot`는 캐시 처리 후 virtual `Evaluate_WithRoot`를 호출하고, false일 때만 원래 그래프를 평가한다. 새 proxy는 실제 main root에 대해서만 `EvaluateAnimationNode_WithRoot`를 한 번 호출하고 true를 반환한다. 연결 root의 기본 경로를 유지하며 재귀·중복 main graph 평가를 만들지 않는다.

조준 입력과 mesh/actor 회전은 game thread에서 값으로 복사한다. parallel pose 평가에서는 UObject·월드·컨트롤러를 읽지 않는다. 본 cache는 compact pose와 bone container serial을 기준으로 재생성한다. 완전한 상체 체인이 없으면 원래 pose를 보존한다. 실제 팔 회전은 actor 축을 mesh component 좌표로 바꿔 적용하며, bone local translation/scale을 보존한다. neck/head의 원래 component 회전을 복구하고 FK 손에 맞춰 손 IK helper를 갱신한다.

초기 fixture에는 실제 잔여 yaw가 없었고 공격 종류별 수락 검사가 부족했다. 독립 리뷰 뒤 ±30°와 ±60° 잔여 yaw, 예상 skill별 사용 횟수, 좌·우 잽 콤보, 실제 V키 입력과 고각 회피 검사가 추가됐다. 같은 pawn 소유 대조 mesh의 montage notify가 실제 전투에 간섭하지 않도록 notify dispatch를 억제했다. 회피는 설정된 montage instance에서 독립적으로 감지한다.

## 첫 실제 실행: aim-runtime02

실행은 21개 case, 1,575frame, 460,503assertion을 완료했지만 1,286failure로 **실패**했다. 엔진 실행 로그와 `tmp/view-aligned-upper-body/aim-runtime02`의 CSV 및 metric을 직접 확인했다. 최초 실행 결과를 전체 통과로 해석하지 않는다.

정지 case 중 neutral·moving·dodge를 제외한 17개 case의 case frame 12 이후, montage phase 오차 0.005초 미만인 1,071행을 별도로 집계했다. 원래 montage의 blend warmup까지 제거한 fixture 집계보다 넓은 표본이므로 이 수치는 성공한 정지 경로의 관측값으로만 사용한다.

| 관측 | 최대치 |
|---|---:|
| Source 하체 local 회전 차이 | 0.000002958° |
| Source render bone local translation 차이 | 0 cm |
| Source 머리 component 회전 차이 | 0.000004830° |
| Source 손목·손가락 local 회전 차이 | 0.000003818° |
| 양쪽 upperarm의 독립 세계 시선 오차 | 0.030731480° |
| FK·IK 손 위치 차이 | 0.000012346 cm |

실제 잔여 yaw ±30°에서 출력은 ±29.999998°였다. ±60° 입력은 ±44.999996°로 제한됐고 양쪽 팔의 세계 시선 오차는 0.000005° 미만이었다. 정지·가드·정지 공격의 상체 보정 방향은 실제 pose에서 맞는다.

실패는 이동 두 case의 하체 local 회전 최대 18.96°, local translation 최대 13.90cm 등 대조 불일치에 집중됐다. proxy는 하체와 render translation을 수정하지 않는다. neutral case에서는 양쪽 보정이 모두 0인데도 finger 차이가 최대 0.30°였다. 원본 AnimBP 대조의 상태·시간·업데이트 순서가 일치하는지 확인하고, 실제 런타임 결함과 oracle 오차를 구별해야 한다. 단지 허용 오차를 확대해 통과시킬 수 없는 상태다.

고각 회피의 world head frame step 최대 119.81cm는 실제 DoDodge의 actor 이동을 포함한다. 해당 frame은 원본 대조와 source 머리 회전이 같았다. 이 절대 이동량을 신규 조준이 더한 카메라 흔들림으로 해석할 수 없다. actor 이동을 제거한 머리 이동 또는 조준 전후 paired pose 차이로 별도 판단한다.

## 원본 대조와 최종 집중 실행

초기 fixture의 원본 대조 불일치는 테스트 Source와 baseline을 모두 CharacterMovement 뒤에서 평가하도록 정렬하고, 초기 15fixed frame을 안정화한 뒤 두 그래프를 함께 초기화해 해결했다. 생산 Source의 tick prerequisite는 수정하지 않았으므로 생산 전체의 업데이트 순서까지 개선했다고 주장하지 않는다.

`aim-baseline03`은 두 Source 모두 조준 보정을 끄고 기존 보존 검사를 유지했다. 21개 case, 1,575frame, 최종 로그 459,385assertion과 실패 0으로 통과했다. Source 하체 회전 차이 최대 0.000002958°, local translation 차이 0cm, Appearance 하체 차이 최대 0.000003415°, 손가락 차이 최대 0.000003415°, 팔·다리 길이 차이 0%, 카메라 독립 oracle 오차 최대 0.000002415°였다. 원래 그래프의 정지·이동·공격·회피 대조를 같은 pose로 평가하는 제어가 통과했으므로, 이후 보정 유무의 비교는 유효하다.

`aim-runtime04`의 남은 실패 3개는 회피 복귀 목표 수렴을 너무 일찍 검사한 fixture 시간축 문제였다. 회피 blendout 중 원본 pose 비교 warmup으로 표본을 제외하면서 마지막 실제 회피 활성 frame도 제외했기 때문이다. 독립 montage 감지 직후마다 회피 활성 frame을 기록하도록 옮겼다. 12frame 안정화와 목표 오차 1° 기준, 실제 bone 보존 기준은 바꾸지 않았다.

최종 `aim-runtime05`는 21개 case, 1,575frame, 최종 로그 **460,488assertion과 실패 0으로 통과**했다. 실제 생산 pawn의 정지·가드·맨손 좌우 잽·우클릭 스트레이트/홀드 훅·이동 잽·너클·세계 yaw 회전·잔여 yaw ±30°/±60°·pitch ±85° 입력 제한·실제 V키·고각 회피 및 복귀가 포함된다. 몽타주 표본 수뿐 아니라 예상 skill의 실제 사용 횟수를 확인한다.

| 최종 집중 런타임 관측 | 최대치 |
|---|---:|
| Source 하체 local 회전 차이 | 0.000002958° |
| Source render bone local translation 차이 | 0 cm |
| Source 머리 component 회전 차이 | 0.000005663° |
| 양쪽 upperarm의 적용된 세계 시선 오차 | 0.000005123° |
| Appearance 하체 local 회전 차이 | 0.000003415° |
| Appearance 머리 component 회전 차이 | 0.000002415° |
| Appearance 손가락 local 회전 차이 | 0.000002415° |
| Appearance 팔·다리 길이 차이 | 0.000036886% |
| 비활성 머리 pose로 계산한 독립 카메라 회전 오차 | 0.000002415° |
| 조준 활성 상태의 FK·IK 손 위치 차이 | 0.000011401 cm |

Source와 Appearance의 모든 평가 bone에 대한 유한성·회전 정규화 검사도 통과했다. arm geometry는 실제 적용된 보간 snapshot을 기준으로 확인하고, ControlRotation 목표 수렴은 별도 기준으로 확인하므로 정상 보간을 즉시 목표 도달로 오인하지 않는다. 원본 회피 중에는 보정이 0이며 복귀 후 목표로 수렴한다.

고각 ±75° 보정에서 머리 위치는 원본 대비 최대 14.594cm 이동했다. 보정은 머리 **회전**을 보존하며 위치까지 고정하지 않는다. 고각 회피 중 최대 head component frame step은 원본 대조와 보정 활성 실행 모두 20.298cm였다. 첫 회피 frame은 활성 15.310cm와 원본 11.789cm로, 보정 제거 전환의 증가분이 약 **3.521cm**였다. 원본 회피의 큰 물리 이동과 이 증가분을 구분해야 하며, 완전히 연속인 회피 카메라 위치라고 주장하지 않는다.

## 부정 대조와 실제 렌더 확인

`aim-negative06`은 원본 보정만 비활성화하고 동일한 21개 case를 끝까지 실행했다. 예상한 실패는 2,456개이며 `failure-reasons.txt`를 직접 확인했다. pitch 목표 수렴 1,087개, 잔여 yaw 목표 수렴 252개, 실제 세계 팔 방향 1,097개, Appearance 손 변화 없음 20개다. 이 네 종류 외 하체·머리·local 위치/스케일·카메라·bone 유한성·공격 입력 등 실패는 0개다. feature를 제거했을 때 의도한 결함만 검출하므로 부정 대조는 유효하다.

`aim-render07`은 D3D12 실제 렌더에서 21개 case·1,575frame·최종 로그 460,488assertion·실패 0으로 완료했다. 1280×720 PNG 1,575개가 존재한다. 렌더 metric도 집중 런타임과 같은 상체 방향, source/Appearance 본 보존 및 카메라 oracle 결과다.

2026-10-13에 다음 **실제 PNG**를 직접 열어 검토했다. 예측 이미지나 재생성 이미지는 사용하지 않았다.

| 관측 표본 | 확인 내용 |
|---|---|
| `frame_0397.png` | 1인칭 상향 잽. 주먹과 팔이 실제 화면에 보이며 시야의 상향 방향을 따른다. 빠른 동작의 motion blur가 있다. |
| `frame_0622.png` | 1인칭 하향 훅. 전경의 주먹이 하향 화면과 함께 배치된다. 원경 바닥과 주먹이 함께 보인다. |
| `frame_1072.png` | V키 상향 검사. 몸통·팔·무릎 체인에 분리되거나 반대로 꺾인 명확한 신규 형태는 관측하지 않았다. 낮은 후방 시점이라 머리가 어깨 뒤에 가려져 머리 skin 미관을 평가하는 표본으로는 부족하다. |
| `frame_1151.png` | V키 하향 검사. 전신의 상체·팔 방향과 하체의 유지 상태를 확인했다. |
| `frame_1508.png`~`frame_1523.png`, 연속 16개 | 고각 가드에서 회피로 전환되는 실제 표본. 회피 중 원본 전신 pose를 유지하는 수치와 대응한다. 화면 근접과 motion blur의 한계가 있다. |

이 표본에서 신규 상체 보정 때문에 팔·다리 체인이 늘어나거나 손목이 반대로 꺾인 차단 회귀의 근거는 찾지 못했다. 손가락의 세밀한 skin 변형이 모든 frame에서 아름답다고 보증하는 판정은 아니다.

상향 60° V검사의 회피 표본은 검사 카메라가 몸 아래 가까이 배치돼 허벅지·발이 전경을 크게 차지한다. `frame_1515.png`에는 삼각형으로 잘린 mesh 면처럼 보이는 화면이 있고, 회피 중 손·발이 화면 경계를 드나든다. inspection 카메라의 지면 충돌과 근접 클리핑으로 보이는 관측이며, 이번 작업은 inspection 위치 계산을 수정하지 않았다. 원본 대비 본 위치·회전 및 길이가 보존되는 수치와 이 화면 미관을 구분한다. **모든 검사 카메라 각도가 깨끗하다거나 clipping이 없다고 주장할 수 없다.** 이번 상체 보정의 차단 회귀로 분류하지는 않는다.

## 최종 빌드와 기존 기능 회귀

2026-10-13에 root가 실행한 최종 빌드 로그와 회귀 실제 로그·metric을 독립적으로 읽었다. 임시 복구 helper를 삭제한 최종 Editor 빌드 `build-editor6-final.log`는 Succeeded, 8.23초로 완료했다.

| 회귀 | 실제 결과 |
|---|---|
| `viewaim-moving-20261013` | 1,100frame, 최종 로그 258,066assertion, 실패 0. 하체 source/Appearance 비교와 실제 이동·공격·회피·점프·피해 타이밍 검사가 유지된다. |
| `viewaim-stance-20261013` | 640frame, `FIGHTER_COMBAT_STANCE_RUNTIME_PASSED`, 실패 0. |
| `viewaim-camera-20261013` | 1,572frame, 최종 로그 4,978assertion, 15/15case, 실패 0. 실제 공격 13종과 체력 피해 pulse 21개, 머리 기반 카메라 위치 계약이 유지된다. |
| 일반 공격 회귀·Editor 종료 상태 | Root가 마지막 실제 실행 및 종료 상태를 확인하고 추가 기록한다. 완료했다고 선행 주장하지 않는다. |

metric 파일의 assertion 수는 마지막 파일 저장 검사가 실행되기 전에 쓰여 최종 로그보다 작은 경우가 있다. 위 표는 최종 결과 로그 기준이다.

`preservation-after.json`도 확인했다. 시작 상태 source 451개 가운데 이번 범위의 변경 6개·추가 4개·삭제 0개로 기록됐다. Content 6,359개는 추가·삭제·size/mtime 변경이 0개이며 Common AnimBP SHA가 같다. 생산 Save 3개는 before/after SHA가 같다. 모든 Content의 SHA를 계산한 대조라고 확대하지 않는다. 이 보존 기록은 이후 마지막 회귀의 최종 상태 확인과 구별한다.

## 검증 범위의 한계

최종 추가 확인(루트, 2026-10-13): 일반 맨손 전투 회귀도 840프레임·실패0으로 통과했다(`regression-combat-wrapper.log`). 새 모듈로 기존 OpenRunMap을 다시 열고 Common AnimBP의 조준 기본 활성화와75/45도 한계·척추25%·보간18, game world 없음·dirty 콘텐츠/맵0을 확인했다(`editor-restored-state.json`). 원본 보존 검사와 범위 공백/생성 경로 검사도 최종 상태에서 통과했다. 일회 복구 도구는 Source에서 제거했다.

Fixture는 현재 생산 pawn의 Source→Appearance 경로를 사용하고 LOD 1로 고정한다. 21개 case의 입력·pose·camera 계약과 지정한 렌더 표본에서 이번 시선 보정의 차단 회귀를 찾지 못했다. 모든 외형 종류·장비·LOD 전환·모든 frame의 skin 세부 미관을 검증했다고 주장하지 않는다.

상체 분배에 따라 머리 위치가 최대 14.594cm 변하고, 회피 시작의 위치 변화 증가분이 약 3.521cm 존재한다. 머리 회전의 추가 시선 합성은 막았지만 머리 위치를 고정하거나 회피 카메라를 완전히 연속으로 만들지는 않았다. 고각 V 검사 화면의 지면 충돌·근접 클리핑과 빠른 동작의 motion blur는 관측한 화면 한계다.

차단 UI·사망·컨트롤러 부재·비정상 입력의 runtime gate는 소스로 검토했다. 집중 실행에서 이 경우들을 별도 조작해 시험했다고 주장하지 않는다. 점프/전신 회피의 원래 동작 계약은 관련 회귀의 관측 범위에서 확인하고, 새로운 모든 공중 공격을 제작·검증한 작업으로 확대하지 않는다.
