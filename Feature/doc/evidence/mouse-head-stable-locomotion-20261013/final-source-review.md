# 마우스 기준 머리 안정화·보행·안전 모션 독립 소스 리뷰

- 검토일: 2026-10-13
- 작업: Sprint#5-30-mouse-head-stable-locomotion
- 역할: 기능 검증 2. 기획·두 개발 담당과 분리된 소스 검토자.
- 소스 판정: 개발 1·2의 최종 ready 증분에서 미해결 차단 결함을 발견하지 않았다. 아래 발견 사항은 개발 담당이 수정했고 검토자가 최신 소스로 확인했다.
- 통합 판정: build2/3/4/5/6의 성공 로그를 직접 읽었다. 최신 fixture build6은 56.43초다. 안전 자산 application03 PASSED, 7 sequence·33 montage·74 DA의 gameplay field 보존과 native section/notify proof를 읽었다. 최종 production 순서의 runtime03·runtime04·Fist02와 Moving01 성공 로그, negative01의 예상 실패도 확인했다. render01은 입력 검증 5건 실패로 보존한다. 최종 render02의 시각·통합 판정은 별도 검증 1의 증거가 필요하며 이전 Sprint#5-29 결과를 이번 기능의 실행 증거로 대체하지 않는다.
- 검토자 수정: 본 문서만 작성했다. 엔진 실행, Git 명령, 소스·에셋·저장 파일 수정은 하지 않았다.

## 범위와 시작 상태

`tmp/stable-head/checkpoint`의 소스 사본과 `source-before.json`을 기준으로 이번 증분을 분리했다. 기존 머리 숨김·Shadow·V키 검사·원래 camera attachment 복원 코드는 이번 변경과 구별했다.

검토한 런타임은 AppearanceCameraComponent, ViewAlignedUpperBodyProxy, FighterCombatStanceAnimInstance, CharacterAnimationComponent, CombatComponent와 Character의 `PlayCommonMontage`다. 새 Editor helper 두 파일과 `ConfigureSafeHeadMotion.py`도 제한된 자산·graph 변경과 시간 계약을 검토했다. 실행 QA fixture 자체의 최종 성공 판정은 별도 검증 역할에 맡긴다.

## 발견 사항과 수정 확인

| 발견 사항 | 근거 | 최신 수정 확인 |
|---|---|---|
| 평가 이후 메시 회전과 새 mouse sample | 처음 구현은 현재 mesh world pose에서 계산한 motion에서 평가 당시 world aim을 그대로 제거했다. 같은 local pose라도 actor/component 회전이 달라지면 두 quaternion의 basis가 달라진다. | 실제 head pose를 작성할 때 world aim과 source component rotation을 저장한다. Camera는 `T=current source rotation × captured source rotation inverse`로 aim을 현재 basis에 운송한 뒤 제거한다. |
| 새 Source와 이전 Appearance의 혼합 | source의 PostEvaluate 값만 곧바로 읽으면 별도 Appearance가 아직 이전 평가 pose를 보유하는 query에 새 aim을 제거할 수 있었다. | Appearance의 BoneTransformsFinalized native delegate에서 해당 pose와 source snapshot을 짝지어 값·weak source를 캐시한다. 직접 source body는 bone wait 뒤 자신의 native snapshot을 읽는다. |
| nonfighter 차징 상한 | 초기 getter `IsFighterCombatChargeHeld`는 non-Unarmed/Knuckle NativeUpdate의 `ResetFighterCombatStance`가 매번 false로 지웠다. 달리는 GreatSword 차징은 첫 프레임 후 running 4도 cap로 돌아갈 수 있었다. | Combat의 새 `IsAttackChargeHeld`는 실제 held 입력/bound skill/BasicBrawler charging 상태를 읽는다. Camera가 이 getter를 사용하며 공격·차징·회피·공중의 3도 cap를 running보다 먼저 선택한다. |
| 알려진 preview 회전 클립 누락 | 초기 deny는 Martelo와 StraightKick을 포함했지만 실제 존재하는 LowRightThaiKick을 포함하지 않았다. | 알려진 LowRightThaiKick 및 extended audit의 전/후 evade 후보도 exact source deny에 포함됐다. |
| 파생본 root 원점과 신체 높이 | 첫 적용의 손 component 위치 오차 90.795174410cm는 source root 위치의 길이 90.795156117cm와 일치했다. 원본 root의 Z 90.767265320cm를 0으로 만들면 몸·발 전체 높이를 내려버릴 수 있다. root-relative 손 검사만 바꾸면 이 문제를 숨길 수 있다. | zero-origin 정책을 철회하고 첫 source root local T를 고정 보존한다. root-relative와 component-space 손의 전체 T/R/S를 모두 검사하고, 매 프레임 head/pelvis/feet 위치·높이도 별도로 검사한다. architect의 제작 중 수정 절과 최신 Python을 확인했다. |
| DuplicateAsset의 조기 저장 예외 | UE 5.8 AssetTools의 PerformDuplicateAsset는 SourceControl enabled일 때 duplicate 직후 package를 저장한다. 단순 duplicate 호출은 항상 memory-only라는 보장이 없다. | apply preflight에서 `unreal.SourceControl.is_enabled()`가 true면 duplicate 전에 중단한다. 실제 ScriptName과 BlueprintPure IsEnabled API를 engine header에서 확인했다. |
| Editor graph 열거 API | 첫 빌드는 FBlueprintEditorUtils::GetAllGraphs가 존재하지 않아 실패했다. | `Blueprint->GetAllGraphs(Graphs)`로 변경된 코드와 UE UBlueprint 선언을 확인했다. 재빌드가 성공했고 다른 graph authoring 동작은 변경되지 않았다. |
| Python의 section 반영 범위 | application02는 UPROPERTY만 선언된 CompositeSections를 Python get_editor_property로 읽다가 저장 전에 실패했다. | 공개 BlueprintPure GetNumSections/GetSectionName은 Python이 읽고, 전체 section·notify 보존은 native helper가 교체 전후 확인한다. 보존 proof 두 개가 true가 아니면 Python save loop로 진행하지 않는다. |
| montage blend-out 종료 프레임의 상한 | PreUpdate는 montage advance 이전이다. 마지막 공격 기여 프레임이 평가 전에 locomotion으로 바뀌면 source는 3도, Camera는 2도 budget을 선택할 수 있었다. | 공식 GT PreEvaluateAnimation에서 budget만 다시 캡처하고 head/neck smoothing 결과를 새 상한으로 최종 clamp한다. paired mouse/source basis와 실제 평가 snapshot publish는 유지된다. 최종 runtime03/Fist02의 성공 로그를 확인했다. |
| production source의 landing 순서 | 집중 fixture가 Source→Movement prerequisite를 삽입하여 실제 누락을 가렸다. Fist01 landing frame에서는 Source가 아직 airborne 3도를, 그 뒤 Camera가 grounded 2도를 선택했다. | StylizedCharacter의 PostInitializeComponents에서 Movement가 먼저 끝나도록 실제 Source prerequisite를 추가한다. 새 집중 fixture는 존재 assert만 하며 production source를 추가/제거하지 않는다. 최종 runtime03와 Fist02의 성공 로그를 확인했다. |

위 사항은 개발 담당에게 근거와 함께 전달했다. 검토자가 소스를 대신 수정하거나 원래 요구를 약화한 것은 아니다.

## 스레드·평가·수명

UE 5.8의 `UAnimInstance::PreUpdateAnimation`은 게임 스레드 proxy 접근을 통해 PreUpdate를 호출하고 기존 병렬 task를 기다린다. 이번 구현은 이 지점에서 controller, actor, mesh, 상태와 튜닝을 값 타입으로 복사한다. worker의 ApplyViewAim/ApplyMouseDrivenHead는 compact pose와 값만 사용하며 UObject·월드·컨트롤러를 조회하거나 변경하지 않는다.

실제 root에서 `EvaluateAnimationNode_WithRoot`를 한 번 호출한 뒤 상체·머리 보정을 적용하고 true를 반환한다. 다른 root는 false로 기본 평가에 맡긴다. 재귀할 수 있는 `EvaluateAnimation_WithRoot`를 override 안에서 호출하지 않는다. 기존 locomotion/montage/FootIK graph를 중복 평가하지 않는 계약을 유지한다.

PostEvaluate는 UE `UAnimInstance::PostEvaluateAnimation`의 게임 스레드 proxy 접근 이후 호출된다. publish하는 값은 나중 PreUpdate의 입력이 아니라 worker가 실제 head pose를 쓸 때 저장한 EvaluatedHeadWorldAim/EvaluatedHeadSourceRotation이다. 실제 head가 없거나 적용되지 않으면 applied=false와 identity를 publish한다. 초기화도 flag·residual·평가 basis를 초기화한다.

추가 PreEvaluateAnimation override는 montage advance가 끝나고 실제 graph를 평가하기 직전의 GT hook이다. PreUpdate는 계속 mouse/mesh basis를 한 쌍으로 캡처하고, PreEvaluate는 GetHeadMotionSettings의 budget/weight/interpolation 값만 갱신한다. worker에서 상태나 UObject를 읽는 방식으로 바꾸지 않는다. 늦게 준비된 입력을 평가 snapshot인 것처럼 publish하지도 않는다. UE SkeletalMeshComponent의 DoInstancePreEvaluation 위치와 Proxy header의 GT 계약을 직접 확인했다.

Bone container serial 변화 시 compact index와 reference component 회전을 다시 구성한다. 필수 upper-body 체인이 없는 LOD에서는 상체 보정을 생략하며, head가 없는 compact pose는 mouse head applied를 주장하지 않는다. source/container의 bone API 사용은 immutable reference pose/parent index 조회이고 임의 UObject 조회와 구별한다.

새 cache는 값 타입·TWeakObjectPtr이고 Shadow의 기존 소유 참조는 UPROPERTY TObjectPtr이다. native CreateUObject delegate는 UObject 약한 수명을 사용하며 UFUNCTION이 필요한 dynamic delegate가 아니다. Restore, 메시 교체와 EndPlay에서 delegate를 해제하고 cache를 지운다. proxy는 virtual destructor를 가진 기본 AnimInstance destroy 경로에서 삭제된다. runtime에서 CDO를 변경하지 않고 constructor의 월드 조회·새 component Tick도 추가하지 않는다.

최신 StylizedCharacter 증분은 Super::PostInitializeComponents 뒤 실제 등록된 AnimationSourceMesh에 CharacterMovement prerequisite를 추가하고 필요한 IWYU include만 더한다. UE ACharacter의 같은 lifecycle 함수는 이미 GetMesh에 Movement prerequisite를 둔다. 기존 Body의 Source prerequisite를 합치면 실행 순서는 **Movement → Source → Appearance**다. 프로젝트와 UE MovementComponent/CharacterMovementComponent의 직접 prerequisite 설정에서 Source/Appearance → Movement 역방향은 발견하지 않았으므로 이번 증분이 dependency cycle을 만들지 않는다. transform attachment를 이동 상태 평가의 순서 보장으로 오인하지 않는다.

새 MouseHeadStable fixture는 이 실제 Source prerequisite를 FTickPrerequisite로 assert하고 production Source를 보정하지 않는다. 비교 전용 baseline은 자신의 Source/Movement 순서를 설정할 수 있으며 파괴된다. Finish에서 production prerequisite를 제거하는 코드도 사라졌다. 이 조건이 충족되지 않으면 fixture가 failure를 기록한다.

## 머리·카메라·본 계산

머리 기준은 skeleton reference component 회전이다. authored residual은 reference 대비 quaternion의 총 각도로 제한한다. 각 Euler 축에 독립 상한을 적용하여 대각선에서 상한이 늘어나는 방식이 아니다. 평활화 후에도 다시 clamp하므로 running 4도에서 attack 3도 상태로 전환할 때 이전 residual이 상한을 초과하지 않는다.

목은 mouse aim의 기본 35%, 머리는 전체 mouse aim을 component 공간에서 기록한다. 바뀐 부모의 역회전으로 local rotation을 계산하므로 상체 회전을 단순히 다시 누적하지 않는다. neck/head와 기존 상체 보정은 render local translation·scale을 쓰지 않는다. 하체와 손목·손가락의 기존 local 회전을 보존하며 손 IK helper는 최종 FK 손의 component 포즈와 맞춘다.

Camera는 실제 평가 pose에서 mouse basis를 제거한 residual을 현재 ControlRotation에 한 번 합성한다. aim을 이미 받은 source head에 residual weight를 다시 곱하지 않는다. 아직 Appearance finalization snapshot이 없거나 해당 pose에 head aim이 없으면 capped authored fallback을 사용한다. 현재 마우스 움직임 자체는 보간하지 않는다.

위치 필터는 mesh local reference head anchor에서 authored offset만 1/2/1.5cm 기본 상한으로 제한한다. 현재 mesh/capsule world 위치와 점프 이동은 즉시 적용한다. GFrameCounter cache로 같은 프레임의 camera/trace query가 위치 필터를 반복 진행시키지 않는다. eye offset도 현재 최종 시선으로 회전한다. reference mesh/bone 변경과 Restore에서 필터 초기값을 지운다.

유효하지 않은 character/controller, 사망, 차단 UI와 비유한 입력의 fallback은 소스로 확인했다. 이 상태 각각의 실제 실행·렌더를 시험했다고 주장하지 않는다. source head와 retargeted Appearance head가 같은 정량 상한을 실제로 지키는지는 신규 QA로 확인해야 한다.

## 모션 정책·시간·속도

현재 extended audit의 회전 후보 7개(source 5개와 전/후 evade 2개)는 runtime exact deny 목록에서 확인했다. Composite와 Montage의 실제 segment source를 재귀 검사하고 cycle/과도한 depth는 거절한다. Combat은 자원, 쿨다운, 타격 예약과 스킬 사용 기록 전에 거절하고 `PlayCommonMontage`도 같은 gate로 직접 재생 우회를 막는다.

안전 대체는 Editor에서 별도 sequence/montage를 만들고 DA의 AttackMontage를 정적으로 다시 연결한다. 따라서 기존 AcceptedSkill->AttackMontage와 실제 재생 montage 계약이 달라지지 않는다. 원본을 덮지 않고 공격 DA의 montage 외 값, 원래 attack 길이·slot/section/notify/policy를 검증한다. Uppercut의 실제 19/30초 접촉값과 Overlord 접촉값을 piecewise 시간 매핑에 사용하고 target contact에서 손 위치를 확인한다. Montage나 asset 저장 성공을 아직 소스 검토만으로 주장하지 않는다.

접촉 비교의 최신 구현은 source/target 각각의 hand와 root component transform으로 `MakeRelativeTransform(hand, root)`를 구한다. UE KismetMathLibrary의 인자 순서를 직접 확인했다. 비교에는 위치 0.20cm뿐 아니라 quaternion 회전 0.05도와 scale 0.0001 기준이 포함된다. 현재 두 주먹 소스는 root가 상수이므로 raw hand component T/R/S도 같은 기준으로 확인한다. root-first T를 고정하는 목적은 travel 억제이며 정적 스탠스 높이 제거가 아니다.

매 프레임 hand/head/pelvis/양발 component 위치를 source에서 실제 제거된 root travel만 보정한 위치와 0.01cm 기준으로 비교한다. root T는 zero가 아니라 source 첫 T와 0.001cm 기준으로 비교한다. root rotation/scale와 모든 non-root local T/R/S 보존 검사도 유지한다. 시작·접촉·종료 component 포즈와 보존한 root 원점을 결과에 남기므로 root-relative contact 통과만으로 높이 오류를 승인하지 않는다.

GroundLocomotionPlayRate는 실제 수평 속도에서 grounded·non-dodge 때만 계산하고, 새 getter는 기존 단일 Walk / Run BlendSpace의 PlayRate에만 연결한다. montage·jump time을 이 값으로 바꾸지 않는다. 현재 audited PalmReference BlendSpace는 Idle 0, Walk 300, Jog 600cm/s sample과 BSA_NONE을 갖는다. 600 이하에서는 기존 stride/sample blend를 유지하고 600 초과에서는 기본 최대 3배까지 cadence를 늘리는 설계다. 속도별 실제 foot travel/phase 개선은 runtime node/time 측정으로 확인해야 한다.

발자국은 같은 cadence로 interval을 나누어 one-shot event마다 다시 예약한다. 새 Tick을 만들지 않으며 이동 종료·jump·EndPlay의 기존 정리 경로를 유지한다. 공격의 기존 montage rate와 hit timer 계산은 이 증분에서 변경하지 않는다. 실제 공속 0.5/1/2배 검증과 안전 대체의 접촉 검사는 별도 통합 결과가 필요하다.

Editor helper는 WITH_EDITOR 안에서만 컴파일하고 단일 예상 AnimBP/player/slot/source folder를 확인한다. 기존 graph에 native rate getter 하나를 연결하고 compile/binding/node 보존을 검사하며 관련 없는 graph 변경은 Python 비교로 거절한다. 자산 저장은 pose·policy·gameplay field·graph 검증 뒤에 수행한다. 실패 시 원래 DA/motion 참조를 메모리에서 복원하고, 저장 도중 실패한 디스크 자산은 checkpoint를 Unreal Editor API로 복구해야 한다는 경계를 명시한다.

## 첫 적용 실패와 저장 경계의 직접 확인

`asset-application1.log`와 `safe-motion-failure.json`에서 최초 실패가 sequence 준비 단계의 접촉 검사임을 읽었다. native montage 변경·DA 재연결·graph 변경·명시적 save loop보다 앞이다. `asset-before.json`의 보호 원본 133개 SHA-256을 직접 다시 계산했고 변경 0개, HeadSafe 디스크 파일 0개를 확인했다. application1 로그에는 SourceControl disabled와 실패 후 editor 종료가 기록돼 있다. 따라서 이번 첫 실패에 한해서 원본·파생 디스크 자산의 저장 변경이 없음을 확인했다.

failure JSON의 `derivatives_in_memory`는 준비 함수가 성공 반환한 target 목록이므로, 검증 중 예외가 난 미반환 duplicate의 완전한 inventory는 아니다. 이 빈 배열만으로 memory object가 전혀 생성되지 않았다고 주장하지 않는다. 실제 디스크 해시·파일 검사와 해당 editor 종료를 저장 경계의 근거로 사용한다.

새 guard는 SourceControl enabled의 조기 저장 경로를 막는다. 모든 검증 후 명시적 save loop를 통과한 뒤에만 application PASSED를 기록한다. save loop 자체는 여러 package의 원자적 transaction이 아니므로 중간 저장 실패는 원본 참조를 memory에서 복원하는 것으로 끝났다고 간주할 수 없다. 이 경우 checkpoint 기반 Editor API 복구와 디스크 해시 확인이 필요하다. 후속 application03의 성공 기록은 아래 통합 증거에서 확인했다.

두 번째 적용(`asset_application02.log`, `asset_application02-failure.json`)도 저장 전에 중단됐다. 7개 sequence가 준비 함수에서 성공 반환된 뒤 첫 montage policy의 CompositeSections Python 접근에서 실패한 기록을 읽었다. 해당 종료 뒤 원본 133개 SHA-256 변경 0, HeadSafe 디스크 파일 0을 다시 확인했다. 아직 native montage 수정·DA 재연결·graph 변경·save loop 이전의 실패였다.

## Native section·notify 보존 증분

UE 5.8 AnimMontage header에서 CompositeSections는 UPROPERTY이지만 Python에 필요한 접근 노출이 없고, GetNumSections/GetSectionName은 BlueprintPure임을 확인했다. 최신 Python policy는 공개 count/name을 독립 확인한다. native helper는 source/clone의 보존을 substitution 전에 확인하고, substitution·RefreshCacheData·PostEditChange 후에도 다시 확인한다.

section proof에는 name, next section, 실제 GetTime, link method, slot/segment index와 각 metadata 객체의 class 및 non-transient reflected property 상태가 들어간다. notify proof는 전체 FAnimNotifyEvent struct export와 Notify/NotifyState 객체의 non-transient reflected property 상태를 기록한다. clone의 owner montage path와 새 segment sequence identity는 의도된 참조 교체로 정규화하며, gameplay property의 새 default를 만들어 비교하지 않는다. 객체 자체의 reflected 상태 검사이며 모든 외부 참조 asset의 payload를 재귀 인증한다는 뜻은 아니다.

native 결과의 before/after JSON과 두 preservation bool을 남긴다. Python은 native PASSED·예상 montage 개수·두 bool 모두 true를 요구한 뒤 blend/rate/slot/segment 공개 policy를 추가 확인한다. metadata·notify 검사를 삭제하거나 Python API 부재를 이유로 해당 보존 기준을 통과 처리하지 않는다. source/target 원본을 직접 저장하는 helper도 아니며 기존 SCC-disabled/save-loop 경계가 유지된다.

## 기존 회귀 fixture 변경의 읽기 검토

FistCombat의 이전 8도 authored camera oracle은 새 mouse-separated 계약으로 변경됐다. 실제 기록한 Appearance head/ref pose에서 residual을 계산하고, 평가 basis를 현재 source 회전으로 운송한 뒤 current control 한 번과 직접 합성한다. 독립적으로 공격·차징·회피·공중/달리기 상태 상한을 판정하고 source camera·실제 PlayerCameraManager cache·Shadow pose를 검사한다. 위치 검사는 정확히 고정한 anchor와의 equality가 아니라 최종 eye lever를 제거한 body-local offset의 상한 초과량이다. 이 의미는 CSV와 metric note에 명시돼 있다. 값 조정으로 2/4/3도 상한 검사를 완화하지 않았다.

MovingAttack의 NeutralAim 모드는 production source와 독립 baseline의 mouse-driven head를 둘 다 끈다. 원본 montage의 head까지 raw authored pose로 비교하는 회귀 목적을 유지하기 위한 조건이다. 초기 source reinitialize와 각 독립 movement case의 clock reset 뒤에도 같은 조건을 재적용하며, Finish에서 production source의 원래 옵션을 복원한다. baseline은 폐기된다. 하체·상체 몽타주 기여·실제 damage timing 검사 자체를 제거하지 않는다. 이 모드의 성공을 production mouse-head 안정화 성공으로 인용하지 않으며 새 head QA가 별도로 필요하다.

## 최신 저장된 통합 증거의 확인

- `safe-motion-application.json`: application03 PASSED, 7 sequence·33 montage, 74 DA의 montage 외 필드 보존, 저장 기록 77개. native 33개 montage 모두 section/notify preservation=true다. 검사자가 자산을 수정하거나 엔진을 실행한 결과는 아니다.
- `stable_runtime03.log`: 21 case·1,575 frame, 최종 log assertion 428,431, 실패 0. 별도 검증 1의 `independent-runtime03-analysis.json`도 production prerequisite를 삽입하지 않고 assert한 최종 build5의 성공으로 해석한다. metric/분석의 428,430은 마지막 종료 assertion 기록 시점과 구별한다.
- `stable_runtime04.log`: build6의 최종 24 case·1,800 frame·실패 0을 확인했다. frames/results CSV 행 수도 1,800/24이며 failure-reasons는 0 byte다. 최종 log assertion 489,429와 metrics의 489,428은 마지막 metrics 저장 assertion 시점 차이다. wrapper는 exit 0과 production save inventory 3개 불변을 기록한다. 이 확인을 아직 끝나지 않은 render02의 시각 판정으로 확대하지 않는다.
- `stable_fist02.log`: 840 frame, 실패 0. 실제 production 순서에서 landing 경계의 source/camera 상한 회귀가 해소된 로그다.
- `stable_moving01.log`: 1,100 frame, 실패 0. 이는 앞서 설명한 raw authored 회귀 목적과 조건으로 해석한다.
- `post_application_preservation02.json`: 원본 삭제·예상 밖 focused hash 변경 없음, 결과 PASSED. 앞선 실패 시점의 133개 동일 hash와 최종 적용에서 허용한 DA/graph 변경을 혼동하지 않는다.

`stable_negative01`은 21 case·1,575 frame의 head-disabled 부정 대조다. failure-reasons 원문과 `independent-negative01-render01-analysis.json`을 직접 읽었다. Source와 Appearance head cap가 각각 정확히 1,575회 실패하고, 평가 mouse basis가 없는 상태의 transport metamorphic 검사 1회가 추가돼 총 3,151회다. 그 밖의 failure reason은 없고 camera/position cap 위반도 0이다. 검사 민감도 증거이며 이 실행을 정상 gameplay PASSED로 간주하지 않는다.

`stable_render01`은 36 frame/case로 줄인 이전 Movie fixture의 756 frame 실행이다. 입력 수락 횟수 3건, 재생 관측 2건의 실제 실패를 그대로 보존한다. 이 실행의 head/camera/position cap와 bone 검사가 0건 실패한 사실만으로 전체를 승인하지 않는다. 짧아진 case가 실제 공속·cooldown·charge release 계약을 깨뜨린 진단 결과이며 최종 애니메이션 성공 영상으로 전달할 수 없다.

## 최종 QA 시간표·wrapper 증분

최신 `Rogue10mMouseHeadStableRuntimeTest.cpp`에서 모든 모드의 CaseFrames는 75, fixed delta는 1/30초다. 훅 입력은 frame12에 누르고 frame43에 뗀다. Movie flag는 연속 screenshot 요청에만 사용하며 시간표·타격 검사·공속 검사 분기에 관여하지 않는다. 기본 -20도 idle/jab/inspection jab 3개를 끝에 추가해 전체 24 case·1,800 frame이 된다. 기존 source Movement dependency는 assert만 하며 production prerequisite를 fixture가 삽입하지 않는다.

실제 target damage는 Movie에서도 관측한다. 원래 정면 정지 jab의 공속 0.5/1/2/3배 case는 damage pulse 1회 및 첫 타격 시점 기준을 동일하게 요구한다. 모든 공격 case의 입력 수락 정확히 1회와 재생 관측, 실제 이동·jump, total quaternion 2/4/3도·위치 1/2/1.5cm, bone 및 late input 검사도 유지한다. 실패 건수를 지우거나 생산 cooldown을 줄여 렌더 실행을 통과시키는 변경은 없다.

`VerifyMouseHeadStableMotion.ps1`의 `r.MotionBlurQuality 0`은 render phase의 임시 QA 프로세스 ExecCmds에만 들어간다. production config나 자산을 수정하지 않는다. wrapper는 매 실행의 새 label을 요구하고 실패 exit/log를 보존하며 finally에서 비-QA save의 추가·삭제·SHA-256 변경까지 검사한다. negative wrapper의 단순 예상 FAILED 확인을 전체 원인 인증으로 확대하지 않으며, 위 negative01의 정확한 failure 분류는 CSV·failure-reasons를 확인한 별도 증거를 근거로 한다.

이 증분에서 기존 18개 검토 파일의 SHA-256은 모두 직전 리뷰와 동일했다. 바뀐 신규 fixture와 wrapper를 읽기 검토한 결과 소스 차단 결함은 없다. build6의 compile/link 성공과 최종 runtime04 성공 기록을 확인했다. 최종 render02 실행 및 메시 표면·deformation의 시각 판정은 검증 1과 통합 담당에게 남아 있다.

## 직접 확인한 UE 5.8 근거

- `Engine/Source/Runtime/Engine/Private/Animation/AnimInstance.cpp`: PreUpdateAnimation, PostEvaluateAnimation, DestroyAnimInstanceProxy.
- `Public/Animation/AnimInstanceProxy.h`와 `Private/Animation/AnimInstanceProxy.cpp`: virtual destructor, root override, cache 준비 후 단일 평가.
- `Public/BoneContainer.h`, `Private/BoneContainer.cpp`, `Public/BonePose.h`: mesh→compact 변환, serial, reference pose, parent/component/local 계약.
- `Private/Components/SkeletalMeshComponent.cpp`: GetBoneSpaceTransformsView의 병렬 wait/post-evaluation, 실제 평가 뒤 PostEvaluate, FinalizeBoneTransform의 native multicast broadcast와 delegate 해제 API.
- `Private/Components/SkeletalMeshComponent.cpp`의 DoInstancePreEvaluation와 `AnimInstanceProxy.h`: PreEvaluateAnimation GT hook 및 실제 평가 직전 호출.
- `Classes/Animation/AnimMontage.h`, `Classes/Animation/AnimLinkableElement.h`: section·notify 데이터와 실제 link/time API, Python 공개 section count/name.
- `Classes/Animation/AnimCompositeBase.h`, `Editor/AnimGraph/Public/AnimGraphNode_Base.h`, `AnimGraphRuntime/Public/AnimNodes/AnimNode_BlendSpacePlayer.h`: segment 교체·길이, PlayRate pin 노출과 BlendSpace player API.
- `Classes/Engine/Blueprint.h`, `Classes/Kismet/KismetMathLibrary.h`: UBlueprint graph 열거와 hand-to-root relative transform 인자 계약.
- `Developer/AssetTools/Private/AssetTools.cpp`, `Editor/UnrealEd/Private/Subsystems/EditorAssetSubsystem.cpp`, `Developer/SourceControl/Public/SourceControlHelpers.h`: duplicate의 SCC 저장 분기와 SourceControl Python API.
- `Private/Character.cpp`의 PostInitializeComponents와 `Private/Components/MovementComponent.cpp`, `CharacterMovementComponent.cpp`: 기본 Movement→GetMesh tick 순서와 이번 Source 종속성의 역방향 부재.

## 검토한 최종 SHA-256

| 파일 | SHA-256 |
|---|---|
| FighterCombatStanceAnimInstance.h | `528793609DBF4135AE4D53EEE21FA0D1F8DA96B5DCB94B4EAF3E0EB5649A0529` |
| FighterCombatStanceAnimInstance.cpp | `FC3E2530D51CC3E56205C498B18CC73B59E74BEDB5C334375F9B3A4932207B5B` |
| ViewAlignedUpperBodyProxy.h | `E9184A1F095209E7AFD9104EA8B68B9D795CE9004432F275545F9CF5EFFBBAFE` |
| ViewAlignedUpperBodyProxy.cpp | `F2994EAFC898BCFA18F358FE199187657C1490F078DED3F63E902057E3F30A45` |
| AppearanceCameraComponent.h | `631C2896847111AFBD0E682E6C1A65254C173361607017D19085C87A0466BFF3` |
| AppearanceCameraComponent.cpp | `959694FFE4F6F914B8F490973580615EBCAA32666D353CD32352ABA743A102EE` |
| CharacterAnimationComponent.h | `D60BDA0BD1C804DCDCDDEB361B0BDC467786707A03BB4BE55CC969B4B6B03CA8` |
| CharacterAnimationComponent.cpp | `238094DD71424238A62B9E839723E1ACA59C76E49591437782C49E2B676D50AD` |
| CombatComponent.h | `DA48D20BBADB29545CE8E6B39655069AE86987256D8E61DCE60353C645C8D3B7` |
| CombatComponent.cpp | `EA32FB1BC27794CC98815DAE39F2C52457423CED363A63267F190B596868367E` |
| Character.cpp | `3938C91B27ADCCEE60C9F4CE40C5464323DD89020C49221644DDCFAB9CFE32FB` |
| StylizedCharacter.cpp | `7A147565F9F2FA5776B7D1DABB12FB5FEC3120B246D359183116D308A1B2FB49` |
| GroundLocomotionAuthoring.cpp | `E397EA975A813D5F41D8AFA5A7C2D510E37CA6C0F6EAB84CAA91D9D3D751C750` |
| SafeHeadMontageAuthoring.cpp | `275632A147E8806BD9E4ED156A6C10E5F14E1623AED5E46069D248D38D194710` |
| ConfigureSafeHeadMotion.py | `33AD941601EE5D2658691509B1169DCD27323961AED46D6A9C09277417E8D188` |
| InspectSafeHeadContact.py | `577262FAC183679B2276DAE8C1E2C0407015E49DC120667732949D7275CE0629` |
| FistCombatRuntimeTest.cpp | `FF3B12CDC94356E03AF84EA9E499136D922694479D2BE15B5C61FAE12C9E2013` |
| MovingAttackRuntimeTest.cpp | `F950D2A5C9923EB05821F479A56304F1637618FB36DA8936C0286119978B6F54` |
| MouseHeadStableRuntimeTest.cpp | `BD46D4567E815ED14B416E8DA44ED2AD50FB70FA34441FD46CE25F8B5666DF64` |
| VerifyMouseHeadStableMotion.ps1 | `BDCB021874138115F76872E63F1D8DFD6AC1F6F65BD76AF07429296FBF43E363` |

표의 C++ 파일명은 실제 `Rogue10m` 접두사를 생략했다. 해시 이후 통합 수정이 생기면 그 증분은 재검토해야 한다. 최신 production Movement prerequisite·실제 prerequisite assertion·24 case 시간표와 render wrapper를 포함한 20개 파일의 소스/정책 리뷰에 미해결 차단 finding은 없다. 소스 판정은 승인이다. build6, 안전 자산 적용, runtime03/runtime04/Fist02와 Moving01 회귀 로그는 통과이며 negative01은 예상 실패다. render01은 실패 진단으로 보존한다. 최종 render02의 독립 통합·시각 판정은 별도 증거가 필요하다.
