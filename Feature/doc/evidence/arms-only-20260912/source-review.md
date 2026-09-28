# 팔 전용 1인칭 독립 소스 검증 1

- 검토일: 2026-09-12
- 역할: 기획 담당에서 독립 Reviewer로 전환. 구현 코드는 수정하지 않음.
- 범위: Rogue10mFirstPersonPresentationComponent.h/.cpp, Rogue10mFirstPersonArmsAuthoring.cpp, CreateFirstPersonArms.py, Rogue10m.Build.cs, Character 및 StylizedCharacter 표시 경로.
- 비교 기준: tmp/arms-only의 작업 시작 시점 Presentation 원본.
- 판정: 현재 검토한 변경에서 즉시 수정이 필요한 중대·중간 결함을 발견하지 못했다. 자산 생성·빌드·실게임·시각 완료를 뜻하지 않는다.

## 확인된 사항

메시 교체 전 원본 SkeletalMesh, material override, 애니메이션 class/mode, single-node asset/time/rate/play/loop, 명시적인 숨김 본, 소유자 표시 flags, primitive type, attachment/transform 및 카메라 설정을 저장한다. 원본 mesh·material·single-node asset는 UPROPERTY로 GC 추적된다. parent와 world body는 비소유 weak reference다. 복원은 원본 메시를 먼저 되돌려 그 메시를 부모로 갖는 원래 카메라 경로를 안전하게 복구한다. single-node 복원은 저장된 mode로 전환한 다음 재생 상태를 설정한다.

동일 무기의 중복 Refresh는 기존 활성 상태를 재저장하지 않으며, Unarmed/Knuckle의 종류가 바뀌면 원본 복원 후 다시 적용한다. 다른 무기와 비로컬 제어 상태에서는 복원 경로를 사용한다. world GetMesh의 OwnerNoSee를 적용 중 강제하고 해제 시 이전 값을 복원한다. 기존 Character는 이미 원래 world body OwnerNoSee와 first-person OnlyOwnerSee를 설정한다. StylizedCharacter의 애니메이션 source는 비표시다.

호환 검사에서 skeleton asset 동일성에 더해 모든 reference bone 수·이름·부모 인덱스·기준 포즈를 확인한다. 따라서 원래 손 IK chain과 본 인덱스 계약이 다른 메시를 적용하지 않는다. 메시 누락/비호환 시 기존 표시로 돌아가므로 충돌을 피하지만 팔 전용 요구를 달성한 것은 아니다. 최종 검증은 실제 활성 메시 경로를 확인해야 한다.

ApplyBodyMotionToView와 기존 Guard camera/FOV 값은 작업 시작본 대비 바뀌지 않았다. 공격 snapshot이나 피해·입력, 손 궤적을 수정하는 코드는 이번 Presentation 변경에 없다. 새 Tick이나 프레임별 asset load가 없다.

Editor helper는 원본과 별도인 지정 target만 Modify/Commit하며 source mesh description을 독립 복사한다. upperarm 자손 가중치가 절반 이상인 정점들로 이루어진 삼각형만 남긴다. 모든 source LOD를 처리하고 빌드된 render LOD 가중치까지 검사해 원본 body LOD의 재등장을 탐지한다. full refSkeleton과 material 슬롯을 원본과 비교한다. Python wrapper는 검증 성공 표식이 있을 때에만 지정 target asset을 저장한다. 원본 mesh·skeleton·animation·material은 저장하지 않는다. Editor 의존성은 Target.bBuildEditor, helper는 WITH_EDITOR 경계 안에 있다.

## 통합 QA에서 반드시 확인할 항목

1. 저장 후 새 프로세스에서 /Game/Rogue10m/Character/SK_FirstPersonArms가 실제 적용되는지 확인한다. fallback 경고가 있으면 팔 전용 구현은 미완료다.
2. full RefSkeleton 보존은 실제 compact-pose의 머리 본 평가와 다르다. UE 5.8 ComputeRequiredBones는 LOD의 RequiredBones에 physics 본을 더하고 숨김 본을 제외한다. AlwaysTickPoseAndRefreshBones에서는 숨김 본을 유지한다. geometry 제거 후에도 실제 GetHeadPoseDelta와 최종 POV 동조가 유지되는지 런타임 로그로 확인한다.
3. upperarm 50% 가중치 기준은 코드상 몸통 제거 방법이며 어깨 절단면의 시각 품질을 보장하지 않는다. 아래보기/차징/최대 신전/와이드에서 절단면·떠 있는 작은 면·팔 소실을 연속 관찰한다.
4. 생성 스크립트의 in-memory 검증 뒤 실제 저장·재로드와 모든 활성 LOD의 geometry가 일치하는지 확인한다. 기존 원본 자산은 hash나 Git 변경 목록으로 보존 여부를 확인한다.
5. Unarmed→다른 무기→Unarmed, Knuckle 전환 및 반복 Refresh에서 mesh/material/본 숨김/owner flag가 원래대로 복원되는지 검사한다. 기본 공격·어퍼컷·점프 기존 회귀 검사도 실행한다.

이 문서는 소스 검토 증거이며 외부 고릴라 암즈 영상을 관찰했다는 증거로 사용하지 않는다.


## 최종 아래보기 런타임 검사 재검토

2026-09-12 최종 Rogue10mArmsOnlyRuntimeTest.cpp와 tmp/arms-only/runtime.log를 직접 대조했다. 390프레임, failures=0, head_signal=1.267도, jump_height=89.989cm, max_hand_step=11.144cm를 확인했다. 실제 spawn/Knuckle/Unarmed 복귀에서 SK_FirstPersonArms 경로가 활성화됐으며 fallback 메시를 합격으로 오인하지 않는다.

피치 fixture는 production ViewPitchMin=-70도를 먼저 저장한다. -89도 강제 스트레스 화면에서만 해당 로컬 테스트 CameraManager의 최소 피치를 넓히고, 다음 정상 화면 및 Finish에서 원값을 되돌린다. 각 Capture는 ControlRotation을 정규화하여 요청 피치와 0.25도 이내인지 확인하고 실제 해상도도 검사한다. 최종 로그에서 1280×720과 1720×720 모두 -89/-70/0/+70도가 확인된다. 그러므로 기존에 -89 이름만 붙고 실제 -70으로 클램프되던 증거 문제는 이번 검사에서 해소됐다. 프로젝트의 production 카메라 피치 범위 자체를 바꾸는 수정은 아니다.

복원 검사는 최초 비활성화 후 얻은 원본 mesh를 Character class CDO의 FirstPersonMesh asset과 별도로 비교한다. 잘못 복원된 팔 메시를 원본 기대값으로 삼는 순환 검사를 방지한다. Staff 전환에서는 실제 메시, material override 수/포인터, mesh transform, 애니메이션 class/mode, first-person primitive type, first-person/world-body owner 표시 flags와 명시적 숨김 본 mask를 확인한다. 이후 Knuckle과 Unarmed로 돌아와 활성 메시·동일한 full refSkeleton·애니메이션을 다시 확인한다. 첫 복원 시점 이후 snapshot에 의존하는 설정도 있으므로 이 결과를 모든 임의의 Blueprint/single-node 구성의 복원 증명으로 확대하지 않는다.

새 검사에서 below-view 좌우 연타 accepted serial+2, 완충 유지와 어퍼컷 serial+3, 실제 점프, 머리 기반 카메라 source가 0이 아님, 프레임 간 손 이동 제한이 확인됐다. head_signal은 FistAnimInstance의 머리 기반 body motion 출력이며 최종 POV 자체를 계측한 값은 아니다. 기본 직업 및 Knuckle의 기존 회귀 검사가 최종 POV·피해·취소 등 넓은 범위를 보완해야 한다.

asset-result.txt의 LOD0/1/2 non_arm_vertices=0, skeleton_bones=89, material_slots=2와 assets.log의 FIRST_PERSON_ARMS_SAVED를 직접 확인했다. 별도 새 game 프로세스에서 해당 asset이 활성화된 로그는 저장 후 재로드 증거다. 원본 SKM_Manny_Simple.uasset의 현재 SHA256은 BE9F011191EE1887B97CD13BCE09886A6066C2FD62C619991070EC142184E1BF로 작업 전 snapshot과 같다. build-final.log는 최종 runtime test를 포함한 Editor 빌드 Succeeded, 10.22초를 기록한다.

이 후속 검토에서도 새 수정필수 결함은 발견하지 못했다. 정확한 -89도 재촬영의 직접 시각 판단, 전체 모션의 자연스러움과 기존 Basic/Knuckle 회귀 완료는 각각 해당 검증 결과를 따른다. 별도 시각 reviewer가 아직 갱신하지 않은 이전 문서의 -89도 미확인 상태를 로그만으로 시각 합격으로 바꾸지 않는다.


## 외부 마우스 입력 격리 검토

2026-09-12 세 런타임 검사(BasicBrawler, ArmsOnly, FirstPersonFist)에 추가된 SetIgnoreLookInput을 독립 검토했다. Prepare에서 실제 PlayerController에 true를 한 번 추가하고, Finish는 bOwnLookInputIgnore 및 weak controller 유효성을 확인해 false를 한 번만 호출한 뒤 소유 상태를 지운다. ResetIgnoreLookInput으로 다른 기능의 전체 스택을 지우지 않는다. UE Controller 구현에서 이 API는 카운터 증감이며 PlayerController의 AddPitch/Yaw/RollInput 누적만 차단한다. SetControlRotation은 이 플래그를 검사하지 않으므로 아래보기 fixture의 명시적인 각도 변경은 계속 작동한다.

Basic의 CameraComponent relative transform 및 ControlRotation 0.01 허용오차 assertion, 실제 머리 pose와 최종 PlayerCameraManager POV delta 측정은 그대로 남아 있다. 추가된 FIRST_AIM_FAILURE는 최초 불일치의 실제 회전·위치 delta와 look-ignore 상태만 출력하며 실패를 무시하거나 기대값을 갱신하지 않는다. Fist의 실제 POV, 카메라 복원·0강도·무기/뷰 전환 검사가 유지되며 직접 공격·점프·이동 경로도 막지 않는다. 따라서 현재 생산 코드의 직접 카메라 변경이나 SetControlRotation 오염을 잡는 검사를 약화시키지 않으면서 외부 마우스에 의한 비결정적 관찰을 격리하는 변경으로 판단한다.

범위 한계는 명확하다. 이 자동 검사는 물리 마우스 look 입력 반응을 검증하지 않으며, 만약 향후 연출 코드가 AddControllerYaw/PitchInput을 통해 잘못 조준을 바꾼다면 ignore 플래그가 그 경로를 가릴 수 있다. 현재 생산 Presentation 경로에는 그러한 호출이 없고 Character의 실제 look 처리에만 존재함을 확인했다. 기존 실패 원인을 외부 마우스로 최종 단정하려면 입력 격리 후 같은 검사에서 통과하는 재실행 결과가 필요하며, 이번 소스 검토만으로 재실행 성공을 주장하지 않는다.
