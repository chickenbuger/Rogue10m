# 맨손·너클 전용 모션 제작 코드 독립 검토

> 최종 가독성 v2 제작 코드·자산 감사 판정: **PASS**. 아래 최초 발견 3건(고정 Pole, 동일 잽/스트레이트, 연타 중 고정 Rest)은 수정 후 실제 Editor bake 자료로 해결을 확인했다. 9개 클립·14개 접촉 및 원본 48개 해시를 독립 재검사했다. 실제 1인칭/피해/6체형/영상 검증은 별도 진행 중이므로 이 판정에 포함하지 않는다.


- 검토일: 2026-09-29
- 역할: 검증 1. 상품 코드·자산 수정 없이 C++ 제작기, Python 검증기와 생성 결과를 독립 검토했다.
- 검토 C++ SHA-256: `5756b1bd884734667f8fb443539d9edd36bcb5274a432a48d9ea553ca2dfc419`
- 범위: `Source/Rogue10m/Editor/Rogue10mFistCombatAuthoring.cpp`, `Scripts/Editor/PrepareFistCombatAnimations.py`, `authoring-requests.txt`, `authored-animation-validation.json`.
- 최초 판정: 본 길이·기준 좌표 변환은 타당하며 2건 수정이 필요했다. 아래 최초 발견 근거를 보존한다. 두 건의 후속 코드 수정은 재검토하여 타당함을 확인했으며 신규 bake·실행 영상 수치는 아직 대기 중이다. 생성기 PASS와 자연스러움 PASS는 구분한다.

## 수정 필요 1: IK 시작·복귀에서 팔꿈치 방향 불연속

C++의 `A <= 0`에서는 가드를 그대로 유지하지만, `A > 0`에서는 고정 `Pole = Shoulder + (Sign*32, 8, -45)`로 두 본 IK를 즉시 적용한다. 손 위치의 quintic envelope가 0으로 수렴해도 팔꿈치 평면은 원본 평면으로 수렴하지 않는다. 시작·복귀에 팔꿈치가 튈 수 있으며, 6회 flurry에서는 같은 현상이 반복된다.

UE 5.8 `AnimationCore/Private/TwoBoneIK.cpp`의 실제 삼각식과 보존된 `animation-stability-20260928/boxing-stability.json`의 `appearance_left` 0초 component-space 좌표를 사용해 A→0+ 극한을 독립 계산했다.

| 팔 | 가드 손 거리 / 최대 팔 길이 | 팔꿈치 위치 변화 | 상완 방향 변화 | 전완 방향 변화 |
|---|---:|---:|---:|---:|
| 왼팔 | 0.567618 | 3.323cm | 6.861° | 6.992° |
| 오른팔 | 0.446133 | 9.100cm | 18.859° | 19.222° |

이 값은 역사적 원본 가드에서의 계산이며, 최종 생성기에서 현재 source guard와 전체 프레임을 다시 측정해야 한다. 다만 고정 pole의 불연속 자체는 현재 코드에서 확인된다. 권고는 매 프레임 원래 팔꿈치 위치를 출발 pole로 삼아 `Activity`에 따라 목표 pole로 보간하는 방식이다. 손 목표를 보존하면서 A=0 경계를 연결할 수 있다. 생성 후 상완·전완·손의 프레임 간 회전과 팔꿈치 위치 step을 원본/수정본에서 함께 비교해야 한다.

## 수정 필요 2: 오른잽과 스트레이트가 동일한 클립

manifest의 `Unarmed_RightJab`과 `Unarmed_Straight`는 길이 1.0초, 첫 접촉 0.34초, 1회 공격이다. 두 style 모두 같은 기본 Offset, 4도 몸 회전, 동일한 준비·복귀 envelope를 사용한다. 유일한 입력 차이인 spacing은 단타에서 사용되지 않는다.

실제 `authored-animation-validation.json`에서 두 클립의 좌·우 손 샘플 121개가 완전히 일치함을 확인했다. 코드상 다른 본 트랙 역시 동일한 계산을 거친다. 좌클릭 2타의 오른잽과 짧은 우클릭 스트레이트를 구분하려면 스트레이트의 몸 회전, 전진 거리, 준비 또는 회수 시간을 별도로 제작하고 생성 결과의 접촉·궤적 차이를 확인해야 한다. 피해·입력·타격 시각은 유지할 수 있다.

## 타당한 구현과 검증 범위

- component pose는 `Local * ParentCS`로 구성한다. UE Quaternion 조합 규칙에 맞춰 솔버의 상완·전완 회전을 `ParentCS.Inverse() * ChildCS`로 local에 되돌리는 식은 타당하다.
- UE 두 본 IK는 길이 변경을 끈 상태에서 상완·전완의 기존 방향을 목표 방향으로 회전한다. Manny의 상완→전완→손 직접 부모 관계와 정상 scale 조건에서 render-bone local translation을 유지해도 도달 가능한 목표 위치를 재현한다. 실제 검증기는 계획 좌표만 읽지 않고 생성 sequence를 평가해 손의 전진 거리와 접근 거리를 검사한다.
- 목표는 최대 길이의 94%로 제한한다. 현 가드 손 거리는 위 표처럼 94%보다 짧으므로 가드에서 이 상한 자체가 불연속 원인은 아니다.
- 모든 render bone의 local translation/scale을 보존하고 다리·골반·root를 고정한다. 9개 클립 실제 재평가 최대 본 길이 오차는 `7.46e-14cm`, render translation/scale 오차는 0이다. 이 수치는 Manny 원본 단계에 대한 결과이며 6개 Appearance 체형의 런타임 검증을 대신하지 않는다.
- IK 동기화는 `ik_hand_gun`을 먼저 갱신한 후 자식 hand IK를 재구성한다. foot IK도 실제 foot CS와 동기화한다. 생성 결과 최대 IK 위치 오차 `0.00004462cm`, 회전 오차 `0.00013797°`를 확인했다.
- spine_03의 작은 CS 회전 후 head local을 부모 역회전으로 보정한다. 따라서 head CS 방향이 가드와 유지된다. 재평가 최대 head travel은 `1.066997cm`, 방향 오차 `0.00004479°`이다. 머리의 실제 위치 이동은 제거하지 않는다.
- 손목의 가드 local 회전을 유지하면 임의 wrist local twist를 추가하지 않는다. 하지만 전완이 바뀌면 주먹의 공간 방향은 함께 바뀐다. 이 검사만으로 타격 순간 주먹 앞면 방향이나 격투 자세의 설득력을 보증할 수 없으므로 정면·측면·머리 카메라 영상을 확인해야 한다.
- 접촉 시간은 montage 시간 `FirstDelay + k * HitInterval * AnimationPlayRate`다. 기존 첫 delay가 재생률로 나뉘고 후속 hit interval이 실시간인 계약에 맞는다. 너클 3타의 6회·0.075초 간격은 기존 정책이다.
- 원본 공유 자산의 해시 보존, 보호된 피해·히트수·차징·콤보 필드 비교 후 파생 자산을 저장한다. 신규 9개 클립은 기존 가드에서 오프라인으로 제작한 별도 동작이며 새로운 모션캡처 원본을 가져온 것은 아니다.

## 후속 실행 확인

1. 위 2건 수정 후 새 생성 데이터와 해시로 본 문서의 미해결 판정을 갱신한다.
2. 팔꿈치 시작/복귀 step, 손 목표 오차, 좌우 손목 방향, 6연타 가독성을 검사한다. 단순 유한값·본 길이 유지로 자연스러움을 대체하지 않는다.
3. 기본 Idle↔공격, 빠른 좌클릭 연타, 짧은/긴 우클릭, 이동·점프, V 시점, 실제 head camera, 전신 그림자를 실행한다.
4. source→Appearance 리타깃의 6체형 길이·IK·발 회전·관통과 카메라 안정화 정책을 별도 fixture 및 영상에서 확인한다.

본 문서는 코드 리뷰와 생성된 pose 감사 결과다. 최종 런타임 또는 시각 품질 전체 PASS를 선언하지 않는다.

## 후속 코드 재검토: 두 발견 사항 수정

- 재검토 C++ SHA-256: `d6908db016a4db8eaae2dd714a27ba84954774c0b0ee4b277b8a236232c14db3` (이후 blend-in 조정은 별도 검증 대상).
- 고정 Pole 대신 현재 팔꿈치 `Lower.GetLocation()`에서 목표 Pole까지 Activity로 보간한다. A→0일 때 손 목표와 팔꿈치 평면이 모두 가드로 수렴하므로 확인된 유한 크기의 시작/복귀 불연속 원인을 해소한다. 삼각형의 유효 범위 및 본 길이 보존은 유지된다. 실제 전 프레임 step 감소량은 새 bake 결과에서 확인해야 한다.
- straight에 준비 .32초/복귀 .46초, 몸 회전 8도, 별도 전방 목표 (8,54,-8), 준비 구간의 작은 후퇴를 적용했다. 접촉 시각은 유지하고 가드 경계 및 접촉점에서 후퇴 기여는 0으로 수렴한다. 오른잽과 동일한 식을 사용하던 코드 문제가 해소됐다. 두 궤적의 실제 샘플 차이 및 시각적 구분은 새 bake에서 확인해야 한다.

## 기존 생성기 재실행 보호

이번 검토 후 부모 담당자의 승인 범위 내에서 아래 기존 생성기 4개만 수정했다.

- `ConfigureBasicBrawler.py`: 4개 스킬을 예전 AM_Punch로 되돌리는 경로.
- `PrepareAppearanceBoxing.py`: 2개 잽을 예전 AM_Boxing로 되돌리는 경로.
- `ConfigureClassCombatAnimationVFX.py`: 5개 너클 몽타주 및 밸런스를 옛 설정으로 되돌리는 경로.
- `CreateMartialArtistSkillTreeAssets.py`: 새 너클 기술 이름·설명을 옛 발차기·어깨치기 설정으로 되돌리는 경로.

각 main()의 첫 실행문에 읽기 전용 preflight를 넣었다. 해당 스킬의 현재 몽타주가 FistAuthored 폴더 소속이면 asset 생성·수정·저장 전에 명확한 오류로 중단하며 `PrepareFistCombatAnimations.py`를 안내한다. 기존 전체 생성기의 강제 재실행 성공을 주장하지 않으며, 다른 직업 또는 밸런스 재생성이 필요한 경우 별도 범위 검토가 필요하다. 신규 전용 생성기는 수정하지 않았다.

검증: 4개 파일 AST/문법 검사, main 첫 실행문 검사, 없는 스킬·기존 몽타주 허용, 전용 몽타주 거부 모의 검사를 모두 통과했다. `git diff --check` 통과. Unreal Editor에서 전체 구형 생성기를 실행하지 않았으므로 자산 변경은 없다.

`ConfigureBrawlerHookBlend.py`는 원래 source/자체 target 외 몽타주를 거부하는 guard가 있어 새 훅을 이미 보호한다. ApplyMartialArtsReadableVFX는 이번 9개와 별개인 StoneFist 기술만 변경한다. C++ CombatComponent는 SkillData.AttackMontage를 직접 PlayCommonMontage에 전달하고 BasicBrawlerComponent도 같은 DA 몽타주를 기록하므로 기존 몽타주로 하드코딩되는 경로는 없다.

## 연타 경계 실패의 추가 독립 진단

최초 Pole 보간 수정 후 새 bake의 flurry 경계 검사에서 elbow 1.719cm / 회전 3.488도 실패가 발생했다. 유한 120Hz 표본에 반대쪽 팔의 흉추 회전이 섞인 것인지 독립 계산으로 분리했다.

결론은 실제 불연속이다. 반대 팔이 흉추를 돌리는 동안 비활성 팔(A=0)은 현재 손 CS를 따르지만, A→0+에서 `Rest=GuardCS[H]`가 회전 전의 고정 손 위치를 목표로 사용한다. Pole만 보간해도 손 기준 좌표가 연결되지 않는다. `independent_flurry_boundary.py`는 엔진의 두 본 IK 삼각식과 보존된 source guard로 실패값 1.719247829cm를 재현했다.

| 계산 조건 | 12개 경계의 최대 120Hz 팔꿈치 step | 경계 ±0.000001초의 최대 step |
|---|---:|---:|
| 고정 GuardCS Rest | 1.719248cm | 1.695155cm |
| 현재 Hand CS Rest | 0.207506cm | 0.00003525cm |

따라서 경계 임계값을 넓히거나 보고 전용으로 바꾸는 대신 `Rest`를 흉추 회전을 반영한 현재 손 위치로 고치는 것이 적절하다. 현재 .6cm / 2.5도 경계 검사와 전체 120Hz step 검사도 유지한다. 원시 solver의 epsilon limit 검사를 추가한다면 반대팔이 움직여 흉추가 회전한 상태도 포함해야 한다. baked sequence는 키 사이 보간 자체가 연속이므로 sequence의 epsilon 표본만으로 원시 제작식의 불연속을 검출했다고 해석해서는 안 된다.

이 계산은 본 위치의 독립 수학 재현이다. 최종 Editor 재평가와 회전 step, 영상 확인은 아직 대기 중이며 숫자를 런타임 PASS로 사용하지 않는다.

후속 코드 확인: SHA-256 `97CAD2A1B7D35B4EED248B61038EBBC0B3649A56417D07BE8ED48F720E00054A`에서 Rest가 `Hand.GetLocation()`으로 수정됐으며, 흉추 회전이 적용된 매 프레임 CS에서 A=1e-6 limit를 검사한다. 현재 코드상 추가 blocker는 없다. epsilon probe는 Goal 생성식을 별도로 사용하므로 production의 Rest 기준 회귀까지 단독으로 보장하지 않는다. 기존 Python의 실제 생성 sequence 경계 gate를 유지해야 하며, 개발 담당자가 유지한 것을 확인했다. 새 Editor bake 수치는 후속 자료로 갱신한다.


## 최종 저장 자산 독립 재검사 — 가독성 v2

- 자료: 최신 `native-authoring.txt`, `authored-animation-validation.json`. native 및 Python 결과 PASS를 직접 읽고 수치·개수를 다시 검사했다. 이전 단락의 v1 수치는 문제 진단 이력이며 이 절이 최종 수치다.
- C++ SHA-256: `6430f50df3a922a13a7be44567c3fa31d9d7ab9e721d95360b3af839e2b7c383`.
- Python SHA-256: `d16fe331dfafbc306064bd6f89e6f9729f4381d682bb36238e9751b5a2929e7c`.
- 9개 생성 클립, 총 14개 접촉(단타 8개 + flurry 6회)을 확인했다. JSON에 기록된 원본 48개 파일을 디스크에서 독립 SHA-256 계산해 전부 일치했다.
- v2는 +Y 전방 / −X 오른어깨 좌표에 맞춰 오른 타격의 몸 회전을 음의 yaw로 바꿨다. 오른잽·훅·heavy는 −8도, straight는 −12도이며 손 목표 높이를 올렸다. body의 낮은 타점과 왼잽은 유지한다. 현재 손 CS Rest, Pole 보간, 길이·IK·head·경계 gate는 유지한다.
- 최종 flurry 경계: 최대 팔꿈치 `0.359295cm`, 회전 `0.627535°`. 기존 `.6cm / 2.5°` 임계값을 유지한 채 PASS했다. 전체 120Hz 최대 팔꿈치 step `5.594466cm`, 회전 step `10.914344°`는 기존 전체 gate `8cm / 30°` 이내다. v1보다 큰 움직임이지만 검사 임계값을 늘리지 않았다.
- epsilon A=1e-6 검사: 최대 팔꿈치 `0.000033059cm`, 회전 `0.000067895°`로 .01 / .01 기준을 통과했다.
- 오른잽과 스트레이트의 실제 생성 손 궤적 최대 차이 `11.515495cm`, RMS 차이 `5.646012cm`다.
- 9개 클립 최대 본 길이 오차 `7.11e-14cm`, render local translation/scale 오차 0, stance 위치 오차 `0.00001678cm`, IK 위치 오차 `0.00006439cm`, IK 회전 오차 `0.00012926°`다.
- head 최대 이동 `1.826934cm`, 방향 오차 `0.00004327°`다. 이는 클립 pose 평가 수치이며 실행 시 카메라 회전량과 동일한 지표가 아니다.
- 맨손 blend-in `.16초`, 너클 잽·body `.10초`, 나머지 `.12초`; 첫 실제 접촉을 넘겨 가리지 않는 검사가 통과했다.

판정 범위는 제작 수학, 저장된 Manny sequence pose 평가, 타격 시각 입력, 원본 보존과 기존 생성기 보호다. 9개 클립과 원본 48개 보존의 최종 PASS를 확인했다. 실제 표적의 피해 적용, 입력 정책, Appearance 리타깃 6체형, V 카메라와 그림자, 최종 동작의 시각적 자연스러움은 별도의 통합 실행 결과가 필요하다. v2 6체형 재실행은 현재 별도 담당자가 수행 중이며 이 문서에서 선제적으로 통과 판정을 내리지 않는다.
