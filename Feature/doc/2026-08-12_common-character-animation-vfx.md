# 공통 캐릭터 애니메이션·VFX 구축 결과

## 결과

걷기, 뛰기, 점프, 구르기, 주먹 공격을 공통 Manny 포즈 소스에서 재생하고 기존 IK Retarget AnimBP로 Human·Dwarf·Orc 남녀 외형에 전달하도록 구성했다. 같은 공격 스킬은 종족과 성별에 관계없이 동일한 원본 Montage를 사용한다.

## 동작 구성

| 기능 | 공통 모션 | Niagara 효과 | 런타임 방식 |
|---|---|---|---|
| 걷기 | `MF_Unarmed_Walk_*` | `NS_Motion_WalkDust` | 이동 입력과 속도 기반 타이머 |
| 뛰기 | `MF_Unarmed_Jog_*` | `NS_Motion_RunDust` | Sprint 상태와 속도 기반 타이머 |
| 점프 | `MM_Jump`, `MM_Fall_Loop`, `MM_Land` | 점프·2단 점프·착지 Burst | `OnJumped`, `Landed`, `JumpCurrentCount` 이벤트 |
| 구르기 | `AM_Dodge_Roll` | `NS_Motion_DodgeTrail` | Dodge 이벤트에서 공통 Montage 재생 |
| 주먹 | `AM_Punch_01~03`, `AM_Punch_Charged` | Swing·Charge·Release·Impact | 공격 Data Asset 기반 Cast/Charge/Impact 처리 |

## 주먹 스킬 방식

- 단일 스킬: Special 슬롯이 `AM_Punch_03`과 전용 Swing/Impact 효과를 사용한다.
- 차징 스킬: Charged 슬롯이 입력 유지 중 `NS_Punch_Charge`, 발동 시 `NS_Punch_ChargedRelease`, 적중 시 `NS_Punch_Impact`를 사용한다.
- 체인 스킬: Primary 슬롯이 `AM_Punch_01` → `AM_Punch_02` → `AM_Punch_03` 순서로 연결된다.
- 각 공격은 기존 Combo Window와 입력 유지 시간 판정을 유지하며, Data Asset에서 Montage와 Niagara를 교체할 수 있다.

## 종족 공통화

`ARogue10mStylizedCharacter`의 숨김 `AnimationSourceMesh`가 `/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed`와 공통 Montage를 재생한다. Human·Dwarf·Orc 남녀 6종은 기존 종족별 Retarget AnimBP를 통해 동일한 원본 포즈를 외형 Skeleton으로 변환한다.

## 점프 프로필

- `DA_SkillProfile_Unarmed`: `MaxJumpCount = 1`
- `DA_SkillProfile_StoneFist`: `MaxJumpCount = 2`

캐릭터가 활성 무기/기본 스킬 프로필의 `MaxJumpCount`를 적용하므로 이후 직업·기본 스킬 Data Asset에서 2단 점프 허용 여부를 정할 수 있다.

## 구현 위치

- 런타임 모션/VFX: `URogue10mCharacterAnimationComponent`
- 모션 설정: `URogue10mCharacterMotionDataAsset`
- 공격 설정: `URogue10mAttackSkillData`
- 공통 Animation Blueprint와 Montage: `/Game/Rogue10m/Animation/Common`
- Niagara System: `/Game/Rogue10m/VFX/Character`
- 공통 모션 Data Asset: `/Game/DataAsset/Character/Animation/DA_CommonCharacterMotion`
- 재구성/검증: `Scripts/Editor/ConfigureCommonCharacterAnimation.py`, `Scripts/Editor/ValidateCommonCharacterAnimation.py`

## 검증

- UE 5.8 `Rogue10mEditor Win64 Development` 빌드 성공
- Unreal Asset Validator 대상 30개 자산 통과
- 공통 구성 검증: `RESULT=PASSED locomotion=walk/run/jump/fall/land montages=5 niagara=12 races=6 attacks=8 jump_profiles=1/2`
- 공통 AnimGraph의 Locomotion State Machine과 Montage Slot 확인
- 6개 종족·성별 Blueprint의 공통 Source AnimBP 및 종족별 Retarget AnimBP 확인
- 8개 Unarmed 공격 Data Asset의 Montage, Cast/Charge/Impact 효과 및 체인 연결 확인

## 알려진 경고와 수동 QA

StylizedCharacter 공급 자산의 6개 Skeleton이 오래된 `/Engine/EngineMeshes/Humanoid` 의존성 경고를 출력한다. 이번 공통 애니메이션 참조와는 무관하며 자동 검증은 통과했다. PIE에서 각 체형의 손·발 미끄러짐, 구르기 충돌 이동과 이펙트 위치를 최종 육안 확인하는 것을 권장한다.

## PIE 직접 테스트

2026-08-12에 메뉴에서 Human Male 캐릭터로 실제 PIE 플레이를 두 차례 실행하고 `ShowDebug Animation`과 런타임 로그로 확인했다.

| 테스트 | 결과 | 런타임 근거 |
|---|---|---|
| 공통 AnimBP | 통과 | 플레이어 Source Mesh에 `ABP_Common_Unarmed_C` 표시 |
| Idle/Locomotion | 통과 | `Locomotion -> Idle`, `MM_Idle` 활성 확인 |
| 기본 주먹 | 통과 | `AM_Punch_01 / MM_Attack_01` Montage 재생 및 `NS_Punch_Charge`, `NS_Punch_Swing_01` 컴파일 |
| 단일 특수 주먹 | 통과 | `AM_Punch_03 / MM_Attack_03` Montage 재생 및 `NS_Punch_Swing_03` 컴파일 |
| 구르기 | 통과 | `AM_Dodge_Roll / MM_Dash` Montage 재생 및 `NS_Motion_DodgeTrail` 컴파일 |
| 점프 | 통과 | `DoJumpStart` 런타임 호출 성공 및 `NS_Motion_JumpBurst` 컴파일 |
| 차징·체인 전체 입력 | 부분 검증 | Data Asset/자동 검증은 통과했으나 UI 자동화가 마우스 버튼을 충분히 길게 유지하지 못해 Charged Montage와 2·3타 연계의 실시간 재생 캡처는 미완료 |
| 뛰기·2단 점프 | 부분 검증 | 공통 Locomotion/점프 프로필 참조 검증은 통과했으나 실제 지속 키 입력과 StoneFist 프로필 PIE 캡처는 미완료 |

테스트 중 Source AnimBP와 공격·구르기·점프 Niagara가 최초 사용 시 정상 컴파일됐으며 관련 런타임 크래시나 자산 로드 실패는 발생하지 않았다.
