# 플레이어 공통 캐릭터 애니메이션·VFX 설계

## 목표

- Human, Dwarf, Orc가 동일한 공격 스킬을 사용하면 동일한 Manny 원본 모션을 재생한다.
- 걷기, 뛰기, 점프, 구르기, 주먹 공격을 기본 모션 세트로 구성한다.
- 캐릭터 기본 스킬 프로필의 `MaxJumpCount`에 따라 1단/2단 점프를 허용한다.
- 주먹 공격은 단일, 차징, 3단 체인 스킬 Data Asset으로 설정한다.
- UE 5.8의 Niagara System으로 이동·점프·회피·공격 시각 효과를 제공한다.
- 이후 권사, 검사, 도적, 마법사 모션이 종족이 아니라 스킬/직업 프로필을 기준으로 확장되도록 한다.

## 범위와 제약

- 기존 `/Game/Characters/Mannequins/Anims/Unarmed` 모션을 프로젝트 공통 `ABP_Common_Unarmed`의 원본으로 재사용한다.
- 종족별 외형 Skeleton은 기존 `ABP_Retarget_*`와 IK Retargeter를 유지한다.
- Marketplace 원본 애니메이션과 Niagara Asset을 직접 수정하지 않고 프로젝트 전용 폴더로 복제·설정한다.
- 애니메이션은 `AttackSkillData`가 소유한다. 종족/성별 Blueprint에는 공격별 몽타주를 중복 지정하지 않는다.
- 상시 Tick을 추가하지 않는다. 점프, 착지, 회피, 공격과 입력 상태 변경 이벤트 및 짧은 Timer만 사용한다.

## Ultrawork Packets

### Packet 1 — 공통 재생 경로

- 목표: 모든 종족이 같은 Manny 소스 AnimInstance에서 스킬 몽타주를 재생하도록 한다.
- 수정 위치: `Rogue10mCharacter`, `Rogue10mStylizedCharacter`, `Rogue10mCombatComponent`
- 완료 조건: 기본 캐릭터와 6개 종족·성별 자식이 공격 스킬의 동일 `AttackMontage`를 재생한다.
- 검증 명령: `Scripts/BuildEditor.ps1`, Editor Asset 검증 스크립트
- 롤백 경계: 애니메이션 재생 Mesh 선택 함수와 CombatComponent 호출부

### Packet 2 — 이동·점프·구르기 이벤트 VFX

- 목표: 걷기/뛰기 발걸음, 1단/2단 점프, 착지, 구르기 Niagara Cue를 데이터 기반으로 제공한다.
- 수정 위치: 새 `Rogue10mCharacterAnimationComponent`, `Rogue10mCharacterMotionDataAsset`, Character 이동 이벤트
- 완료 조건: Tick 없이 이동 상태와 점프/착지/회피 이벤트에서 해당 Niagara가 재생된다.
- 검증 명령: `Scripts/BuildEditor.ps1`, Editor Asset 검증 스크립트
- 롤백 경계: 새 컴포넌트와 Character 연결 호출

### Packet 3 — 주먹 스킬 모션·VFX

- 목표: 단일, 차징, 3단 체인 주먹 스킬이 공통 몽타주와 캐스트/차지/적중 Niagara를 실행한다.
- 수정 위치: `Rogue10mAttackSkillData`, `Rogue10mCombatComponent`, Unarmed Attack Data Asset
- 완료 조건:
  - 단일: 1회 입력으로 1회 모션과 판정
  - 차징: 누르는 동안 차지 VFX, 임계시간 후 해제 시 차징 모션
  - 체인: 공통 입력 창 안에서 1→2→3 모션 순차 실행
  - 적중: 피해가 실제 적용된 위치에 Impact Niagara 재생
- 검증 명령: `Scripts/BuildEditor.ps1`, Editor Asset 검증 스크립트
- 롤백 경계: AttackSkillData VFX 필드와 CombatComponent VFX 호출

### Packet 4 — 프로젝트 전용 에셋 구성

- 목표: 기본 모션 몽타주, 공통 AnimBP 복제본, Niagara System, Motion Data Asset을 생성·연결한다.
- 수정 위치: `/Game/Rogue10m/Animation/Common`, `/Game/Rogue10m/VFX/Character`, `/Game/DataAsset/Character/Animation`
- 완료 조건: 모든 필수 Asset이 로드되고 Skeleton/AnimClass/Data Asset 참조가 유효하다.
- 검증 명령: `UnrealEditor-Cmd` Python validation
- 롤백 경계: 위 세 프로젝트 전용 Content 폴더

### Packet 5 — 리뷰·문서화

- 목표: Unreal 수명주기, GC, Timer, Skeleton 호환성, 생성물 오염을 점검하고 결과를 기록한다.
- 수정 위치: `Feature/doc`, `Docs/SprintChangeLog.md`, `DevLog/20260812.txt`
- 완료 조건: 빌드/Asset 검증 결과, 조정값, 후속 직업 확장 규칙이 기록된다.
- 검증 명령: `Scripts/CheckGeneratedChanges.ps1`, `git diff --check`
- 롤백 경계: 문서 변경

## 런타임 구조

```text
AttackSkillData (모션/VFX 단일 원본)
  -> CombatComponent
     -> Character.GetAnimationPlaybackMesh()
        -> Human/Dwarf/Orc 공통 Manny AnimationSourceMesh
           -> 종족별 ABP_Retarget_* -> 외형 Mesh
     -> Niagara Cast/Charge/Impact Cue

CharacterMovement / 입력 이벤트
  -> CharacterAnimationComponent
     -> MotionDataAsset
        -> Walk/Run/Jump/DoubleJump/Land/Dodge Niagara Cue
```

## 기본 모션 매핑

| 기능 | 공통 원본 | 실행 방식 |
| --- | --- | --- |
| 걷기 | `MF_Unarmed_Walk_*` | `ABP_Common_Unarmed` Locomotion Blend Space |
| 뛰기 | `MF_Unarmed_Jog_*` | 이동 속도에 따른 Locomotion Blend Space |
| 점프 | `MM_Jump`, `MM_Fall_Loop`, `MM_Land` | CharacterMovement 공중 상태 |
| 구르기 | `MM_Dash` 기반 프로젝트 전용 Dodge Montage | 회피 시작 이벤트 |
| 주먹 | `MM_Attack_01/02/03`, `MM_ChargedAttack` 기반 Montage | AttackSkillData 기반 단일/차징/체인 |

## 데이터 소유권

- `URogue10mCharacterMotionDataAsset`: 이동 계열 공통 VFX와 구르기 몽타주를 소유한다.
- `URogue10mWeaponSkillProfileDataAsset`: `MaxJumpCount`로 2단 점프 가능 여부를 소유한다.
- `URogue10mAttackSkillData`: 공격 몽타주, 캐스트/차지/적중 Niagara와 체인 연결을 소유한다.
- `ARogue10mStylizedCharacter`: 종족 외형과 공통 Manny 소스→종족 Skeleton 리타기팅만 담당한다.

## 안전 원칙

- `UObject`/Niagara Component 참조는 `UPROPERTY`와 `TObjectPtr`로 추적한다.
- Niagara는 `UNiagaraFunctionLibrary`를 사용하고 Cascade fallback을 새로 만들지 않는다.
- 몽타주 Skeleton이 공통 Manny Skeleton과 호환되지 않으면 재생하지 않고 경고 로그를 남긴다.
- 차지 VFX는 입력 해제, 공격 취소, EndPlay에서 반드시 정리한다.
- 발걸음 VFX Timer는 이동 입력 종료, 공중 상태, 사망 시 중단한다.
