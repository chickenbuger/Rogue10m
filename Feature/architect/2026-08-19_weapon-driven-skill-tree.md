# 장착 무기 기반 스킬트리 활성화 설계

## 목표

인벤토리에서 장착 무기가 변경되면 전투 컴포넌트가 같은 무기 타입의 `URogue10mWeaponSkillProfileDataAsset`을 즉시 활성화하고, 스킬트리 목록·기본 입력 슬롯·회피·점프 설정을 한 번에 교체한다.

## 현재 구조와 보강점

- `ARogue10mPlayerState`가 현재 장착 무기 타입을 소유한다.
- `ARogue10mCharacter::SetEquippedWeaponType`가 인벤토리와 전투 컴포넌트 사이의 단일 변경 경로다.
- `URogue10mCombatComponent`는 장착 무기 타입으로 `WeaponSkillProfiles`에서 활성 프로필을 찾는다.
- 기존 연결은 프로필 재적용까지 수행하지만, 무기 교체 직전의 차징·콤보·예약 공격 상태를 정리하지 않으며 이전 무기에서 해금된 스킬을 현재 슬롯에 배치할 수 있다.

## Ultrawork Packets

### Packet 1 — 활성 프로필 경계 강화

- 목표: 장착 무기 변경 시 이전 전투 상태와 입력 바인딩을 정리한 뒤 새 프로필을 적용한다.
- 수정 위치: `Source/Rogue10m/Components/Rogue10mCombatComponent.*`
- 완료 조건: 활성 프로필 타입, 스킬트리, 기본 입력 슬롯, 회피, 점프가 장착 무기와 일치한다.
- 검증: Editor 빌드와 클래스 전투 런타임 테스트.
- 롤백 경계: 전투 컴포넌트의 프로필 활성화 메서드.

### Packet 2 — 교차 무기 스킬 차단

- 목표: 현재 활성 스킬트리에 포함되지 않은 공격 스킬은 입력 슬롯에 지정할 수 없게 한다.
- 수정 위치: `Source/Rogue10m/Components/Rogue10mCombatComponent.*`
- 완료 조건: 이전 무기 스킬 지정은 실패하고 현재 무기 스킬 지정은 성공한다.
- 검증: 클래스 전투 런타임 테스트에 전환/교차 지정 검증 추가.
- 롤백 경계: 활성 트리 포함 여부 검사와 슬롯 지정 조건.

### Packet 3 — 데이터와 문서 검증

- 목표: 활을 포함한 11개 무기 전투 프로필이 캐릭터 데이터에 등록됐는지 확인하고 결과를 기록한다.
- 수정 위치: 검증 스크립트, `Feature/doc`, `Docs/SprintChangeLog.md`, `DevLog/20260819.txt`.
- 완료 조건: 프로필 검증, 생성물 검사, 에디터 빌드가 통과한다.
- 검증: `ValidateClassCombatAnimationVFX.py`, `CheckGeneratedChanges.ps1`, `BuildEditor.ps1`.
- 롤백 경계: 문서와 검증 코드.

## 동작 흐름

`Inventory Equip` → `Character.SetEquippedWeaponType` → `PlayerState.EquippedWeaponType` 갱신 → `CombatComponent.HandleEquippedWeaponChanged` → 이전 전투 상태 취소 → 동일 무기 타입 프로필 적용 → 활성 스킬트리/기본 슬롯/HUD 갱신.

## 위험과 대응

- 프로필이 없는 무기: 이전 프로필을 유지하지 않고 활성 바인딩을 비우며 경고 로그를 남긴다.
- 무기 교체 중 공격: 차징, 콤보, 예약 GAS 공격 및 진행 중 다중 타격 타이머를 취소한다.
- 해금 데이터 공유: 해금 기록 자체는 유지하되 슬롯 지정 시 현재 활성 트리 소속 여부를 반드시 검사한다.
- 에셋 변경: `.uasset`을 직접 편집하지 않고 기존 Unreal Editor 생성/검증 스크립트로만 확인한다.
