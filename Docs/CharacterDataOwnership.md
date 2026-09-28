# 캐릭터 데이터 소유 구조

## 목적

캐릭터 클래스에 기능을 누적하지 않고 지속 상태, 전투, 아이템, UI 상태를 각 책임 클래스에 분리한다.

## PlayerState

ARogue10mPlayerState는 플레이어에게 지속되는 상태를 소유한다.

- AbilitySystemComponent
- Rogue10mAttributeSet
- 캐릭터 표시 이름과 직업
- 현재 장착 무기 타입
- 캐릭터 사망 상태
- 아이덴티티 타입과 마나 활성 여부

레벨, 경험치, 체력, 스테미나, 마나, 아이덴티티 수치는 AttributeSet만 원본으로 사용한다.

## AttributeSet

URogue10mAttributeSet은 모든 전투/성장 수치의 단일 원본이다.

- Health / MaxHealth
- Stamina / MaxStamina
- Mana / MaxMana
- Identity / MaxIdentity
- PlayerLevel
- Experience
- ExperienceToNextLevel
- AttackPower
- Defense
- MoveSpeed
- CriticalChance / CriticalDamageMultiplier
- AttackSpeedMultiplier

플레이어는 PlayerState가 AttributeSet을 소유한다. 몬스터는 자신의 ASC와 AttributeSet을 직접 소유한다.

## CombatComponent

URogue10mCombatComponent는 공격 실행 전체를 담당한다.

- 공격 Data Asset 참조와 입력별 선택
- 차징 시간 측정
- 스킬 해금 상태
- GAS Ability 부여와 활성화
- 자원 비용 검사와 소비
- 카메라 기준 공격 판정과 피해 적용
- Montage 재생
- 콤보 입력 시간과 공유 쿨타임
- 공격 디버그와 전투 로그

Character는 입력을 CombatComponent에 전달하는 얇은 어댑터만 유지한다.

## Character

ARogue10mCharacter의 역할:

- 이동, 점프, 시점 입력
- 공격 입력 전달
- PlayerState의 ASC ActorInfo 초기화
- 피격과 사망 생명주기
- InventoryComponent와 CombatComponent 소유
- 카메라와 1인칭 Mesh 제공

## PlayerController

ARogue10mPlayerController의 역할:

- Enhanced Input Mapping Context
- Windows/Android 입력 모드
- UMG 메인 HUD 생성
- 인벤토리/아이템/스킬트리/설정 패널 상태
- 마우스 커서와 GameAndUI 입력 모드
- 감도와 FPS 제한
- 전투 로그, 획득 알림, 피해 피드백
- 조준 중인 몬스터 탐색

## UI

URogue10mRunHUD와 URogue10mMainHUDWidget은 PlayerState, Character, CombatComponent, PlayerController 데이터를 View 구조체로 변환한다. 화면 배치와 디자인은 Widget Blueprint가 담당한다.

## 기본 스탯 데이터 흐름

`URogue10mCharacterDataAsset`은 장비 적용 전 기본 체력·자원·공격·방어·치명타·공격 속도·이동 속도를 소유한다.

`URogue10mInventoryComponent`는 현재 장착된 Item Data Asset의 `FRogue10mEquipmentStatModifiers`를 합산한다. Character는 기본값과 장비 합계를 `FRogue10mCharacterStatSnapshot`으로 계산하고 최종값을 AttributeSet과 CharacterMovement에 반영한다.

- Character Data Asset: 변경 가능한 캐릭터 기준값
- InventoryComponent: 현재 장비 보너스 합계
- AttributeSet: 실제 전투에서 사용하는 최종 런타임 값
- EquipmentWindow: 동일 스냅샷을 읽는 표시 계층

장비 변경은 Character의 단일 갱신 진입점을 사용하며 최대 체력 변경 시 현재 체력 비율을 보존한다.

## 캐릭터 프로필과 외형 데이터 흐름

`URogue10mCharacterProfileSubsystem`은 GameInstance 수명 동안 최대 3개의 캐릭터 프로필과 현재 선택 프로필을 관리한다. `URogue10mCharacterProfileSaveGame`은 캐릭터 이름, 종족, 성별, 외형 선택값만 로컬 슬롯에 저장한다.

- CharacterProfileSubsystem: 프로필 생성·삭제·선택과 저장 진입점
- CharacterProfileSaveGame: 로컬 프로필 영속 데이터
- CharacterCustomization Data Asset: 종족·성별별 메시와 머티리얼 카탈로그
- CharacterLobbyWidget: 프로필 및 카탈로그를 화면 상태로 변환
- Character: 선택 프로필을 런타임 메시와 PlayerState 표시값에 적용

인벤토리, 경험치, 스탯, 월드 진행도는 외형 프로필 저장 경계에 포함하지 않는다. 이후 캐릭터 저장 기능은 `CharacterId`를 키로 사용해 진행 데이터를 별도 저장하고 외형 프로필과 결합한다.

## 다음 분리 후보

- 무기별 숙련도와 스킬 해금 상태를 전용 ProgressionComponent로 이전
- 공격 적중 시점을 Animation Montage Notify 또는 Gameplay Event로 전환
- 인벤토리 아이템 정의를 Item Data Asset으로 이전
## 상속 캐릭터 외형과 애니메이션 소유권

`ARogue10mCharacter`는 이동·전투·인벤토리·프로필 정체성을 담당하고, `ARogue10mStylizedCharacter`는 외형 Mesh와 애니메이션 리타기팅을 담당한다.

- `AnimationSourceMesh`: 숨김 Manny와 프로젝트 공통 `ABP_Common_Unarmed`를 실행하는 포즈 소스
- `Character Mesh`: 선택 종족 Skeleton과 종족별 Retarget AnimBP를 사용하는 실제 월드 외형
- `AppearanceHairMesh`, `AppearanceFacialMesh`: 동일 종족 전신 Skeleton의 Leader Pose를 사용하는 외형 파츠
- Character Customization Data Asset: 종족·성별별 자식 CharacterClass, IK Retargeter, Retarget AnimClass와 외형 옵션 소유
- GameMode: 선택 프로필로부터 CharacterClass를 결정하고 Pawn Spawn/Possess 수행
- CharacterProfileSubsystem: 선택 프로필과 캐릭터 생성 정보 소유

서로 다른 Skeleton 사이에는 Leader Pose를 사용하지 않는다. Manny 포즈는 `Retarget Pose From Mesh`를 통해 종족 Skeleton으로 변환하며, Leader Pose는 동일 Skeleton의 Hair·Facial 파츠에만 제한한다.
## 기본 직업과 스폰 로드아웃 소유권

`URogue10mCharacterDataAsset`은 캐릭터 원형의 기본 직업 `JobName`, 기본 무기 `DefaultWeaponType`, 무기별 Skill Profile 목록을 소유한다. 현재 기본 원형은 `모험가`와 `Unarmed`를 사용하며, Unarmed Profile의 Primary 슬롯은 `DA_Attack_Unarmed_Primary`를 참조한다. 기존 StoneFist Profile은 선택 가능한 후속 무기 데이터로 유지한다.

`ARogue10mPlayerState`는 Pawn 스폰·Possess 이후 실제 플레이 중인 캐릭터의 직업명과 장착 무기 상태를 소유한다. 외형 프로필의 종족명은 직업명을 덮어쓰지 않는다.

`ARogue10mCharacter`는 `PossessedBy`와 `OnRep_PlayerState`에서 `URogue10mCombatComponent::InitializeSpawnedLoadout()`을 호출한다. CombatComponent는 이 진입점에서 CharacterData, 활성 무기 Profile, GAS Ability를 순서대로 다시 적용한다. 따라서 선택 캐릭터 Pawn의 `BeginPlay`가 PlayerState 연결보다 먼저 실행되어도, Possess가 끝난 시점에 좌클릭 Primary 공격이 확정된다.

```text
CharacterData(모험가 / Unarmed / Skill Profiles)
-> Pawn Spawn
-> Controller Possess 또는 PlayerState Replication
-> CombatComponent.InitializeSpawnedLoadout
-> PlayerState 런타임 직업·무기 상태
-> LeftMouseButton Press/Release
-> Primary 슬롯의 맨손 주먹 공격
```
