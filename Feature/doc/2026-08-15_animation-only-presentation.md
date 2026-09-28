# 애니메이션 단독 표시 및 파티클 분리 결과

## 결과

플레이어 캐릭터의 이동·점프·구르기·공격 애니메이션은 그대로 유지하고, 현재 프리뷰에서는 모든 이동 및 공격 Niagara가 생성되지 않도록 설정했습니다. 파티클 자산과 연결 정보는 삭제하지 않아 추후 독립적으로 다시 활성화할 수 있습니다.

## 애니메이션 정리

| 분류 | 자산/동작 | 상태 |
|---|---|---|
| 공통 AnimBP | `ABP_Common_Unarmed` | 6종족 공용 |
| 걷기·뛰기 | `BS_Idle_Walk_Run` | 상태 머신 연결 유지 |
| 점프 | `MM_Jump`, `MM_Fall_Loop`, `MM_Land` | 1단/2단 프로필 유지 |
| 구르기 | `AM_Dodge_Roll` | 몽타주 유지 |
| 주먹 | `AM_Punch_01`, `02`, `03`, `Charged` | 단일·체인·차징 연결 유지 |

- 프로젝트 공통 애니메이션 경로: `/Game/Rogue10m/Animation/Common`
- 이동 원본 경로: `/Game/Characters/Mannequins/Anims/Unarmed`
- 이동 데이터: `/Game/DataAsset/Character/Animation/DA_CommonCharacterMotion`

## 파티클 정리

| 분류 | 경로 | 수량 | 현재 표시 |
|---|---|---:|---|
| 이동 | `/Game/Rogue10m/VFX/Character/Movement` | 6 | OFF |
| 맨손 공격 | `/Game/Rogue10m/VFX/Character/Combat/Unarmed` | 6 | OFF |
| 변환 원본 | `/Game/Rogue10m/VFX/Character/Source` | 2 | 런타임 미사용 |

- `bEnableMotionEffects=false`: 걷기·뛰기·점프·착지·2단 점프·구르기 파티클 생성 차단
- `bEnableAttackEffects=false`: 시전·차징·타격 파티클 생성 차단
- 표시 스위치를 다시 켜면 기존 Niagara 참조와 크기·지속시간 튜닝을 그대로 재사용합니다.

## 구현

- `URogue10mCharacterMotionDataAsset`와 `URogue10mAttackSkillData`에 독립 VFX 표시 스위치를 추가했습니다.
- 이동 컴포넌트는 VFX가 꺼진 동안 발걸음 Timer를 만들지 않습니다.
- 구르기 및 공격 몽타주 재생은 VFX 표시 여부와 분리했습니다.
- 구성 스크립트가 이동 1개와 공격 8개 Data Asset을 애니메이션 전용 표시 상태로 저장합니다.

## 검증

- UE 5.8 Editor target 빌드: 성공
- Unreal 구성 스크립트: `RESULT=COMMON_CHARACTER_ANIMATION_CONFIGURED`
- Unreal 자동 검증: `RESULT=PASSED`
- 검증 요약: `presentation=animation_only motion_vfx=off attack_vfx=off`
- 애니메이션: 이동 상태 5종, 공통 몽타주 5개, 종족 6종 연결 확인
- 파티클: 이동 6개, 공격 6개, 변환 원본 2개를 독립 폴더에서 확인
- 알려진 비관련 경고: `AdvancedPortalsSystemVFX/SM_Plane`의 기존 convex collision 경고

## 화면 확인 참고

Windows 화면 자동 캡처 도구는 로컬 ACL 오류로 실행되지 않았습니다. 대신 UE Editor API에서 자산 값을 직접 읽어 검증했으며, `Lvl_FirstPerson` 게임 창을 실행해 사용자가 직접 애니메이션을 확인할 수 있게 했습니다.
