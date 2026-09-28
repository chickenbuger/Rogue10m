# 장착 무기 기반 스킬트리 활성화 결과

## 결과

인벤토리에서 무기를 장착하거나 해제할 때 `ARogue10mCharacter::SetEquippedWeaponType`를 통해 `URogue10mCombatComponent`가 같은 무기 타입의 스킬 프로필을 즉시 활성화하도록 정리했다. 하단 HUD의 무기 테마와 전투 스킬 목록·퀵 슬롯이 같은 장착 무기 값을 사용하므로 서로 다른 직업 UI와 스킬트리가 표시되지 않는다.

## 주요 변경

- 장착 무기 변경 시 차징 입력, 콤보 창, 예약된 GAS 공격, 진행 중 다중 타격 타이머를 정리한 뒤 새 프로필을 적용한다.
- 활성 프로필의 스킬트리에 포함된 스킬만 현재 입력 슬롯에 지정할 수 있다.
- 이전 무기에서 해금한 스킬은 해금 이력으로 남을 수 있지만 다른 무기의 현재 슬롯에는 배치할 수 없다.
- 활성 프로필이 있는 동안 프로필에 정의되지 않은 과거 기본 공격의 fallback을 차단했다.
- `GetActiveSkillTreeWeaponType`, `IsSkillInActiveTree`를 Blueprint에서 조회할 수 있게 추가했다.
- 활성 무기의 해금 스킬 조회가 퀵 슬롯 일부가 아니라 현재 활성 트리 전체를 기준으로 동작하도록 수정했다.
- 프로필 누락 시 이전 무기의 슬롯을 유지하지 않고 슬롯을 비우며 경고 로그를 남긴다.

## 활 프로필 보강

인벤토리에서 장착 가능한 `Bow`에 전용 프로필이 없어 장착 시 빈 스킬트리가 되던 누락을 보완했다.

- `DA_SkillProfile_Combat_Bow` 추가
- 기본 3연계 공격, 특수 공격, 차징 공격으로 구성된 5개 스킬 추가
- 활 프로필용 Montage 5개와 Niagara 3개 추가
- `DA_Character_Default.WeaponSkillProfiles`에 활 프로필 등록
- 제작·검증 스크립트의 개수 판정을 고정값 대신 설정 목록 길이로 변경

## 자동 전환 범위

검증 대상은 `Dagger`, `Shuriken`, `Bow`, `DualDaggers`, `LongSword`, `GreatSword`, `DualBlades`, `Shield`, `SwordBuckler`, `Staff`, `Knuckle`의 11개 무기다. 무장 해제 상태는 기존 `Unarmed` 프로필을 계속 사용한다.

각 전환에서 다음을 확인했다.

- 장착 무기 타입과 활성 스킬트리 무기 타입 일치
- 활성 트리 스킬 5개와 초기 해금 스킬 5개 일치
- Primary, Special, Charged 기본 슬롯 연결
- 회피 프로필과 클래스별 점프 횟수 적용
- 이전 무기 스킬의 현재 슬롯 지정 거부
- 프로필에 없는 점프 공격 fallback 차단
- 무기 교체 전 콤보/공격 표시 상태 제거

## 검증

- UE 5.8 `Rogue10mEditor Win64 Development` 빌드 성공
- Data Asset Validator: `RESULT=PASSED profiles=11 attacks=55 montages=55 niagara=33 combos=11 first_person_safe=55`
- 전용 런타임 테스트: `RESULT=WEAPON_SKILL_TREE_PASSED weapons=11 failures=0`
- 11개 무기별 활성 트리 5개 및 기본 Primary 연결 확인
- `git diff --check` 통과

기존 클래스 전투 프레젠테이션 통합 테스트는 `-nullrhi` 환경에서 Niagara 컴포넌트 계수 항목만 실패하므로, 무기-스킬트리 기능 판정은 렌더링에 의존하지 않는 전용 테스트 `Rogue10m.TestWeaponSkillTreeSwitching`으로 분리했다.

## 관련 문서

- `Feature/architect/2026-08-19_weapon-driven-skill-tree.md`
- `DevLog/20260819.txt`
- `Docs/SprintChangeLog.md`
