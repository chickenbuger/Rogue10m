# 시야각 기반 상체 애니메이션 최종 소스 리뷰

- 검토일: 2026-10-13
- 역할: 기능 검증 1. 런타임 구현·QA 구현과 분리된 독립 검토자.
- 판정: 검토한 이번 증분에서 차단 결함을 발견하지 않았다. 현재 Manny source → Appearance retarget 경로에 적용할 수 있다.
- 수정한 파일: 본 리뷰 문서 1개. 엔진 실행, Git 상태 변경, 소스·에셋·저장 파일 수정은 수행하지 않았다.

## 범위와 증분 확인

검토 대상은 `Rogue10mFighterCombatStanceAnimInstance.h/.cpp`와 신규 `Rogue10mViewAlignedUpperBodyProxy.h/.cpp`다. 기존 Git diff에는 이전 작업이 포함되므로 `tmp/view-aim/checkpoint`의 시작 사본과 `source-before.json`을 기준으로 구분했다. 기존 가드·이동 공격 선택·회피 몽타주 캐시 코드는 이번 변경으로 다시 작성되지 않았다. 이번 AnimInstance 증분은 조정값, 관측값 초기화, native proxy 생성, 회피 기여 여부 조회다.

사용자 요구인 상체 시야각 추종은 기존 최종 그래프의 결과에 적용한다. 발과 보행을 유지하고 팔·상체의 회전 방향을 바꾸며, 머리 component 회전을 보존해 머리 카메라에 시야각이 다시 더해지는 것을 막는다.

## Unreal 5.8 계약 확인

| 검사 | 소스 근거와 판정 |
|---|---|
| 스레드 | UObject·컨트롤러·캐릭터 상태·메시 회전·튜닝은 proxy `PreUpdate`에서 읽는다. UE `UAnimInstance::PreUpdateAnimation`은 `GetProxyOnGameThread`를 통해 이 함수를 호출하며, 이 접근자는 기존 병렬 평가 task를 기다린다. `Evaluate_WithRoot`와 `ApplyViewAim`에는 소유자·월드·컨트롤러 조회, UObject cast, 에셋 로딩이 없다. 워커가 사용하는 신규 멤버는 값 타입과 compact bone index다. |
| 그래프 평가 1회 | UE `EvaluateAnimation_WithRoot`는 먼저 bone cache를 준비하고 native `Evaluate_WithRoot` 결과가 false일 때만 그래프를 평가한다. 이번 override는 실제 `GetRootNode()`일 때 `EvaluateAnimationNode_WithRoot`를 정확히 한 번 호출하고 true를 반환한다. 그 외 root는 false로 기본 경로에 맡긴다. 재귀하는 `EvaluateAnimation_WithRoot`를 override 안에서 호출하지 않는다. |
| cache·LOD | 이름 조회가 반환하는 mesh pose index를 `MakeCompactPoseIndex`로 변환한다. `FBoneContainer` serial이 바뀌면 다시 구성하고 `Initialize`에서 캐시를 무효화한다. 필수 spine·upperarm·head compact index가 없으면 기존 포즈를 그대로 반환한다. 평가 중 고정 skeleton index로 compact pose를 쓰지 않는다. |
| 수명·GC | native proxy는 `CreateAnimInstanceProxy`에서 생성되고 기본 `UAnimInstance::DestroyAnimInstanceProxy`의 delete로 삭제된다. UE proxy destructor는 virtual이다. 신규 proxy가 UObject를 장기 소유하거나 raw UObject 멤버를 추가하지 않는다. 기존 회피 캐시는 `TWeakObjectPtr`로 유지된다. |

직접 읽은 엔진 근거는 `Engine/Source/Runtime/Engine/Private/Animation/AnimInstance.cpp`의 `PreUpdateAnimation`·`DestroyAnimInstanceProxy`, `Public/Animation/AnimInstanceProxy.h`의 평가 override 계약과 virtual destructor, `Private/Animation/AnimInstanceProxy.cpp`의 평가 경로, `Public/BoneContainer.h`와 `Private/BoneContainer.cpp`의 compact 변환·serial 갱신, `Public/BonePose.h`의 부모 및 component/local 변환이다.

## 뼈와 좌표 계산 확인

- controller와 actor의 상대 pitch/yaw를 정규화하고 기본 ±75도/±45도로 제한한다. `mesh inverse × actor` basis로 actor 공간의 전체 조준 quaternion을 mesh component 공간으로 옮긴다. Manny의 수입 방향을 월드 전방으로 오인하지 않는다.
- component 공간의 회전을 local pose에 반영하는 식은 `parent inverse × delta × parent × original local`이다. 척추 가중치 0.20/0.32/0.48의 합은 1이며 총 torso 25%를 나눈다. upperarm에는 나머지 75%를 적용한다. 동일한 조준 quaternion의 identity Slerp를 사용하므로 이 분배 자체가 서로 다른 pitch/yaw 축의 누적 오차를 만들지 않는다.
- root·pelvis·다리·발과 손목·손가락 local 회전은 수정하지 않는다. render bone의 local translation·scale도 수정하지 않는다. 수정 후 매번 원본 그래프에서 새 포즈를 평가하므로 보정이 프레임마다 누적되지 않는다.
- neck와 head의 원본 component 회전을 저장하고, 바뀐 부모 공간의 역회전으로 복원한다. neck를 먼저 복원한 다음 head를 복원하는 순서도 맞다. 목이나 머리의 위치를 강제로 고정하지 않아 실제 척추 움직임에 따른 머리 이동은 남는다.
- 손 IK는 gun → left → right 순서로 최종 FK 손의 component transform을 갱신된 helper 부모 공간으로 환산한다. helper 부모 체인이 root/ik_hand_root/ik_hand_gun 밖으로 들어가면 쓰기를 생략한다. helper translation 갱신이 render 손·팔 local translation 변경으로 이어지지 않는다.

## 유효하지 않은 문맥과 회피

비활성 옵션, 소유 캐릭터·컨트롤러·메시 부재, 사망, 차단 UI, 회피 몽타주 기여 시에는 보정을 비활성화하고 관측 pitch/yaw를 0으로 되돌린다. 입력 rotator·mesh quaternion·튜닝·delta의 비유한 값도 같은 경로로 보호한다. 제한·분배·평활화 수치는 실행 중에도 안전 범위로 clamp한다. 회피는 포인터뿐 아니라 캐시한 몽타주 경로로도 판별하고 blend-out weight까지 포함한다.

회피 중에는 기존 전신 포즈를 사용하며 종료 후에는 0에서 평활화하여 조준으로 돌아온다. 사망·UI·컨트롤러 부재·NaN gate는 이번 소스 리뷰로 확인했다. 이 조건 각각을 실제 플레이에서 시험했다고 주장하지 않는다.

## 실행 증거와 해석

독립 소스 리뷰와 함께 이미 저장된 runtime05·baseline03·render07·negative06의 결과 파일을 읽었다. 엔진을 다시 실행하거나 렌더 이미지를 이 역할에서 판정하지는 않았다.

| 증거 | 관측 |
|---|---|
| runtime05, render07 | 각각 21 case, 1,575 frame, 460,484 assertion, 실패 0 |
| paired baseline03 | 양쪽 aim 비활성에서 459,381 assertion, 실패 0. 대조 메시 동기화 오류가 최종 성공 결과를 만들지 않았음을 보완한다. |
| negative06 | aim 비활성 부정 대조에서 실패 2,456개, 조준 방향 오차 최대 75도. fixture가 기능 부재를 검출한다. |
| source 보존 | 하체 회전 오차 최대 0.000002958도, render local translation 오차 0cm, 머리 component 회전 오차 0.000005663도 |
| 실제 Appearance | 하체 회전 오차 최대 0.000003415도, 팔·다리 길이 변화 최대 0.000036886%, 독립 머리 카메라 oracle 회전 오차 최대 0.000002415도 |
| FK/IK | aimed 손 위치 오차 최대 0.000011401cm |

극단 조준에서 머리 위치 차이는 최대 14.594203191cm다. 회전 보존을 머리 위치 고정 또는 모든 카메라 흔들림 제거로 해석하지 않는다. metric의 회피 world head step에는 실제 actor 이동이 포함되므로 그 수치를 머리 카메라 떨림으로 해석하지 않는다.

검증은 현재 생산 pawn, Manny source, 실제 Appearance retarget와 고정 LOD 1의 해당 21 case에 한정된다. 모든 종족·다른 skeleton·LOD 전환·비균일 또는 음수 scale의 변형까지 통과했다고 일반화하지 않는다. 수치 보존과 skin deformation의 시각 품질은 구별한다.

## 검토한 최종 파일 SHA-256

| 파일 | SHA-256 |
|---|---|
| `Rogue10mFighterCombatStanceAnimInstance.h` | `8F00C2E3D76F8D2F9F289ABD851FFBDFCB221791E056D43A2DF2711153845F31` |
| `Rogue10mFighterCombatStanceAnimInstance.cpp` | `57D2FFF1C4EE3DA2907D76AAD7A9A5ECDBEECF4B4F864A33FC2371308A83E8A6` |
| `Rogue10mViewAlignedUpperBodyProxy.h` | `63EA6FC0148889433C535334A7B63CFCD820952E500049C84CC541AC9FF1BA78` |
| `Rogue10mViewAlignedUpperBodyProxy.cpp` | `755DCE8CE0D504EC33FC27665BE527212810B2D85F4FCCAF2D8DABAECC4C93F7` |

현 증분에 대해 소스 수정 요청이나 차단 finding은 없다. 상기 해시 이후 런타임 파일이 바뀌면 해당 증분은 추가 검토가 필요하다.
