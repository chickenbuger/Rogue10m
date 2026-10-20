# 격투가 스킬별 애니메이션 연결 결과

프로젝트 `D:\Project\Rogue10m\Rogue10m.uproject` / UE 5.8 / origin `https://github.com/chickenbuger/Rogue10m.git` / 브랜치 `Sprint#5-7-fighter-skill-animation-binding` / HEAD `b7ab6b6`.

현재 트리의 공격 13개 중 기존 전용 동작 9개를 보존하고 유운수·금강쇄·무극패왕권 3개에 새 전용 파생 시퀀스/몽타주를 연결했다. 용도에 맞는 연결은 12개다. 태산압정은 맞는 내려찍기 소스가 없어 기존 연결을 유지했다. 금강불괴는 상시 패시브로 공격 실행/몽타주가 없다. 소스가 없는 상태를 완료로 계산하지 않았다. commit/push는 하지 않았다.

## 실제 전체 매핑

두 프로필 `DA_SkillProfile_Unarmed`, `DA_SkillProfile_Combat_Knuckle`은 같은 14개 노드를 공유한다. 기본 바인딩/초기 잠금 해제 배열도 보존했다. 아래 이름은 실제 DA에서 읽었다. 전체 경로·소스·재생 배율·타수·간격은 [skill-mapping.csv](evidence/fighter-animation-binding-20261002/skill-mapping.csv)에 있다.

| 실제 스킬 | 변경 전 몽타주 | 최종 몽타주 | 첫 판정 지연(몽타주 초) | 상태 |
|---|---|---|---:|---|
| 연환권 | `AM_Knuckle_Jab` | `AM_Knuckle_Jab` | 0.150000 | 기존 전용 유지 |
| 붕권 | `AM_Knuckle_BodyStraight` | `AM_Knuckle_BodyStraight` | 0.160000 | 기존 전용 유지 |
| 승룡권 | `AM_Knuckle_Uppercut` | `AM_Knuckle_Uppercut` | 0.210000 | 기존 전용 유지 |
| 축경권 | `AM_Knuckle_ShortBlow` | `AM_Knuckle_ShortBlow` | 0.220000 | 기존 전용 유지 |
| 유운수 | `AM_Knuckle_Primary01` | `AM_Fighter_FlowingCloud_LeftCounter` | 0.050000 | 새 연결 |
| 금강쇄 | `AM_Knuckle_Primary03` | `AM_Fighter_IronChain_TwoPunch` | 0.273000 | 새 연결 |
| 백열난무 | `AM_Knuckle_HookFlurry` | `AM_Knuckle_HookFlurry` | 0.180000 | 기존 전용 유지 |
| 금강불괴 | `없음` | `없음` | 0.000000 | 상시 패시브·공격 동작 없음 |
| 태산압정 | `AM_Knuckle_Special` | `AM_Knuckle_Special` | 0.000000 | 소스 부족·기존 연결 유지 |
| 무극패왕권 | `AM_Knuckle_Charged` | `AM_Fighter_Overlord_PowerPunch` | 0.283333 | 새 연결 |
| 왼손 잽 | `AM_Unarmed_LeftJab` | `AM_Unarmed_LeftJab` | 0.340000 | 기존 전용 유지 |
| 오른손 잽 | `AM_Unarmed_RightJab` | `AM_Unarmed_RightJab` | 0.340000 | 기존 전용 유지 |
| 오른손 스트레이트 | `AM_Unarmed_Straight` | `AM_Unarmed_Straight` | 0.340000 | 기존 전용 유지 |
| 차지 오른손 훅 | `AM_Unarmed_Hook` | `AM_Unarmed_Hook` | 0.633333 | 기존 전용 유지 |

## 새 동작과 판정

새 에셋은 `/Game/Rogue10m/Animation/Combat/FighterSkills`에만 생성했다. 원본/Close Combat 프리뷰/FistAuthored 시퀀스와 몽타주는 보존했다.

- 유운수: 보유 Close Combat `A_CC_LeftHook_Manny`의 왼 권 구간. 원본 25/60초에서 시작해 두 배로 진행하며 원본 31/60초의 전방 도달 포즈를 새 클립 0.05초에 둔다. 파리/가드 판정을 새로 추가하지 않았다. 기존 콤보 창 0.1초보다 50ms 앞서 판정하며 낮은 30Hz 프레임에서도 첫 판정 여유를 둔다. BlendIn 0.04초.
- 금강쇄: 안정화 `A_Knuckle_HookFlurry`의 처음 두 권 후 마지막 오른 권 회수를 잇는다. 두 번째/여섯 번째 접합 포즈의 최대 local 차이는 0.008303cm/0.015366도다. 첫 권 0.18초, 마무리 오른 권 0.273초. 기존 Single/HitCount=1과 피해량은 유지해 마무리에 기존 단발 피해 1회를 적용한다. BlendIn 0.10초.
- 무극패왕권: `MM_ChargedAttack`의 기존 전방 차지 펀치. 앞의 정지 준비 구간 0.85초를 제거하고 1.133333초의 전방 도달 포즈를 새 클립 0.283333초에 둔다. 파생본 root 이동/RM을 고정했다. BlendIn 0.10초.

- `FlowingCloud_LeftCounter`: 0.600초, head/pelvis 시작 대비 회전 25.136/19.230도, root 이동 0cm.
- `IronChain_TwoPunch`: 0.700초, head/pelvis 시작 대비 회전 0.000/0.000도, root 이동 0cm.
- `Overlord_PowerPunch`: 1.000초, head/pelvis 시작 대비 회전 46.005/92.632도, root 이동 0cm.

새 클립은 120Hz로 본 포즈를 파생했으며 길이는 기존 압축 프레임 간격에 맞췄다. 원본 Notify·곡선을 새 판정으로 사용하지 않도록 파생본에서 제거했다. 판정은 기존 타이머/ExecutionId 경로다. 첫 판정 시간은 `HitStartDelaySeconds/(AttackSpeedMultiplier*AnimationPlayRate)`이고 이후 간격은 기존 `HitInterval/AttackSpeedMultiplier`다. 피해량·자원·쿨다운·타수·간격·DA 재생 배율·콤보 창·범위·VFX와 전투/카메라 C++는 변경하지 않았다. 원본 대비 limb 비율은 새 클립 모두 0.99998~1.00001 이내다.

0.05/0.273/0.283333은 본 궤적에서 검토한 접촉 후보와 명시 시간 매핑을 맞춘 결과다. 새 3개 각각의 스킨/픽셀 접촉이나 실제 타깃 체력 감소가 모두 검증됐다는 뜻은 아니다.

## 최소 검증 결과와 한계

- Editor 적용/새 프로세스 재로드 후 14노드 전체 필드 비교: PASS, 실제 변경 DA 3개 × AttackMontage/HitStartDelaySeconds 두 필드뿐. 두 프로필 보존. 저장된 신규 시퀀스/몽타주 6개는 Manny skeleton/DefaultSlot/단일 세그먼트/길이·root·limb 검증 PASS. `binding-validation.json`, `bound-data.json`, `bound-poses.json` 참조.
- 게임의 실제 키 입력: `RESULT=FIGHTER_SKILL_TREE_FAILED mode=full assertions=107 active_executed=13 failures=1`. 13개 모두 실제 키 실행과 사용 횟수가 확인됐고, 12개 몽타주의 늦은 활성 검사도 PASS. 유운수 `actual_montage_DA_Attack_StoneFist_Jab` 1항목은 FAIL. 검사는 키 누름 약 0.45초 뒤 활성 여부를 확인하므로 짧은 0.6초 클립과 공격속도에 의해 검사 전에 끝났을 가능성이 있다. 이는 **추론**이며 그 항목을 PASS로 처리하지 않았다. 기존 C++ 검사를 고쳐 빌드하거나 임의 클립 길이를 늘리지 않았다.
- 기존 9개 실제 공격 회귀: `RESULT=FIST_COMBAT_PASSED`, 840프레임, failures=0. 고정 컨트롤러 방향의 실제 정면 타깃 체력 감소 14회, 백열난무 6회/나머지 각 1회, 첫 판정 지연, 본 길이, 별도 계산한 머리 카메라/PCM/그림자 포즈, 걷기·점프를 확인했다. 최대 카메라 회전 7.002087도(기존 cap 8도 이내). CSV 및 `combat-analysis.json` 참조. 새 StoneFist 세 스킬의 개별 실제 피해를 이 9개 fixture 결과로 대신 주장하지 않는다.
- 그래픽 실행은 셰이더 준비(PathTracingRG) 단계에서 3분 제한을 넘어 종료됐다. 이후 두 게임 검사는 `-nullrhi`로 실행했다. 게임 입력/포즈/카메라 cache/물리 타깃 체력 검증은 실제 실행했지만 **이번 실제 렌더·스크린샷·동작 자연스러움·픽셀 접촉은 미검증**이다. 화면 제어 도구는 없어 화면을 직접 봤다고 주장하지 않는다. 게임 로그에 엔진 ToolsetRegistry Python 초기화 오류도 기록돼 있으나 이번 DA 연결 오류나 미통과 한 항목의 원인으로 단정하지 않았다.
- 최초 생성 시도는 30Hz 압축과 맞지 않는 길이 및 보호된 Notify 속성 접근으로 실패했다. 당시 DA/새 에셋 파일 미저장을 확인한 뒤 엔진 공개 `AnimationLibrary` API와 유효한 길이로 수정했다. 최종 생성·조정은 exit 0. 실패 로그도 증거에 보관했다.

## 남은 소스와 작업

태산압정에 맞는 공중 준비→하강→착지/내려찍기 펀치 클립이 로컬 확인 소스에 없다. 기존 `AM_Knuckle_Special/MM_Attack_03`은 남아 있으며 내려찍기 완료로 보고하지 않는다. Close Combat 공격 10개는 펀치/킥이므로, 맞지 않는 컷다운 킥·회전 앞차기나 구르기를 억지 연결하지 않았다. 보류한 `A_CC_StraightKick_Manny`도 사용하지 않았다.

후속으로 필요한 것은 유운수의 빠른 몽타주 실제 표시와 새 3개 동작의 렌더·고정 정면 타깃 접촉 QA, 태산압정용 소스 확보다. 이번 요청 범위를 더 넓히지 않았다. 결제·새 약관·캐릭터 업로드·에셋 삭제·전투/카메라 개편은 없다.

## 보존과 실행

기준 파일 5,701개와 기존 9월 28~30일 변경/앞선 Close Combat 준비를 보존한다. 허용 변경은 StoneFist DA 3개, append 문서 3개다. 이전 9월 30일 runtime-assertions는 검사 동안 백업 후 원래 바이트로 복구했다. 최종 SHA256 결과는 `preservation.json`에 기록한다. C++/Config/uproject/기존 9개 전용 에셋은 수정하지 않았다.

`Scripts/Editor/BindFighterSkillAnimations.py`는 기본 읽기 전용 검사다. 최초 연결 플래그는 출력 폴더가 빈 상태와 기준 DA 일치를 요구하며 기존 결과를 덮어쓰지 않는다. `-RogueAdjustFlowCounter`는 이 작업의 유운수 파생본 1회 조정만 허용한다. QA 스크립트 `Scripts/VerifyFighterSkillAnimations.ps1`은 격리 저장 슬롯과 고유 label을 사용하며 `-NullRHI` 경로를 명시적으로 지원한다.

체력 CSV는 매 프레임 관측 841행이며 그중 양의 체력 감소가 14회다. 타격 수는 양의 감소 행만 집계했다. 840프레임 타임라인과 초기/종료 관측을 구분했다. 최종 `git diff --check`와 Harness 경로 검사는 통과했다.


## 후속 진단과 최종 검사 — 유운수 미통과 해결

최종 승룡권은 **기존 `AM_Knuckle_Uppercut` / `A_Knuckle_Uppercut` 유지**다. Close Combat 어퍼컷은 프리뷰로만 남아 있으며 승룡권에 적용하지 않았다. 이 후속에서 기존/신규 모든 스킬 DA·시퀀스·몽타주는 변경하지 않았다.

기존 미통과는 연결/재생 결함이 아닌 검사 시점 문제였다. `fighter-animation-diagnosis-20261002-full.log`에서 유운수는 정상 가중치1.0으로 재생됐다. 439프레임에 DesiredWeight가1→0으로 바뀌며 블렌드 아웃에 진입했고 같은 프레임에 늦은 `Montage_IsActive` 검사가 실행돼 실패했다. 가중치가0.78→0.35→0.02로 줄어든 뒤443프레임에 정상 종료됐다. 엔진 `GetActiveInstanceForMontage`는 블렌드 아웃 이후 활성 map에서 빠진 인스턴스를 반환하지 않는다. 로그를 요약한 `flow-diagnosis-trace.txt` 참조.

검사를 실제 키 발동 수락 후 매프레임 관측으로 보강했다. 해당 스킬 사용횟수가 정확히1 증가한 실행에서만 올바른 몽타주의 양수 가중치와 두 개 이상 서로 다른 재생 위치를 요구한다. 늦은 한 시점의 활성 여부를 없애는 대신 실제 재생/진행 조건을 추가했으며 기준을 완화하지 않았다. 재검사에서 유운수는13샘플, 몽타주0.034667→0.450667초의 진행이 확인됐다. `RESULT=FIGHTER_SKILL_TREE_PASSED mode=full assertions=120 active_executed=13 failures=0`. 검사 C++만 바꾸고 에셋 길이를 늘리거나 판정/카메라 구조를 고치지 않았다.

새3개 실제 production DA를 독립된 Editor 전용 fixture에서 F6 키로 각각 실행했다. charged 무극패왕권은 기존 charge 요구시간을 실제로 기다렸다. 컨트롤러 방향을 고정하고135cm 정면의 실제 몬스터 capsule을 QueryOnly 타깃으로 사용했다. 기존 GAS·HitStartDelaySeconds/ExecutionId 타이머·판정 볼륨·피해 경로를 그대로 실행했다.

| 스킬 | 실제 피해 회수 | 실제 체력 감소 | 첫 판정 시간 / 예상(초) | 시간 오차(초) |
|---|---:|---:|---:|---:|
| 유운수 | 1 | 32.868164 | 0.066667 / 0.048077 | 0.018590 |
| 금강쇄 | 1 | 39.285156 | 0.266667 / 0.262500 | 0.004167 |
| 무극패왕권 | 1 | 56.941406 | 0.300000 / 0.272436 | 0.027564 |

`RESULT=STONE_FIST_ANIMATIONS_PASSED cases=3 assertions=1234 failures=0 camera_deg=7.002835`. 고정 컨트롤러 최대 오차0.000001708도, 기존 머리 카메라 최대 회전7.002835도(cap8도 이내). 세 몽타주는 각각 실제 재생·진행, 양수 가중치에서 피해, 사용횟수1/피해1, 캡슐 제자리 유지 검사를 통과했다. 피해량은 fixture 캐릭터 스탯을 반영한 **실제 타깃 HP 감소값**이며 DA 기본 피해를 변경한 값이 아니다.

추가한 첫 포즈 시간 검사는 acceptance를 PostActorTick에서 관측하면서 몽타주 위치는0으로 가정해 실패했다. 세 경우 모두 수락 프레임에 이미0.034667초 진행돼 있었다. 수정 검사는 이 **측정한 수락 위치**를 시간 원점으로 써서 실제 피해 시 몽타주 경과 시간과 타이머 경과 시간을 비교한다. 허용오차는 이전과 같은30Hz 한 프레임이며 두 프레임으로 넓히지 않았다. 원시 피해 시 몽타주 위치0.104/0.312/0.346667초 및 authored cue와의 원시 차이도 CSV/JSON에 함께 보관해 숨기지 않았다. 이 결과는 실제 타깃 판정/피해 및 두 시계의 진행 검증이지 스킨/픽셀 접촉이 정확히 일치한다는 주장과 다르다.

최종 Editor 증분빌드는 성공했다(최종 관측 원점 수정 빌드10.92초). 변경 코드는 기존 `Rogue10mFighterSkillTreeRuntimeTest.cpp`와 새 `Rogue10mStoneFistAnimationRuntimeTest.cpp`, 검사 실행 스크립트뿐이다. 프로덕션 전투·카메라·대미지·쿨다운·타수·조준 및 에셋은 그대로다. 기존 검사 실패 로그와 최초 포즈 원점 불일치 로그도 보존했다.

이제 유운수 미통과와 새3개 개별 실제 판정/피해 미검증은 해결됐다. NullRHI이므로 **실제 렌더·스킨/픽셀 접촉·동작 자연스러움은 여전히 미검증**이다. 셰이더 준비를 장시간 재시도하지 않았다. 태산압정 소스 부족은 그대로이며 새 소스 조사로 범위를 넓히지 않았다. commit/push 없음. 최종 근거는 `followup-validation.json`, 두 최종 검사 로그/CSV, `followup-preservation.json` 참조.
