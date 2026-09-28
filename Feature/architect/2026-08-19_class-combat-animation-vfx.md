# 직업·무기별 전투 애니메이션 및 1인칭 VFX 설계

## 목표

- 단검 도적, 표창 도적, 쌍단검 도적, 장검 전사, 대검 전사, 쌍검 전사, 방패 전사, 한손검·작은방패 전사, 마법사, 권사의 10개 전투 프로필을 구성한다.
- 각 프로필은 기본 3연계 공격, 단일 특수 공격, 차징 공격을 가진다.
- 같은 스킬 Data Asset은 Human/Dwarf/Orc 및 성별과 무관하게 동일한 Manny 원본 Montage를 사용한다.
- 참조 영상의 짧은 예고광, 무기 궤적, 적중 섬광을 Niagara로 계층화한다.
- 로컬 1인칭에서는 화면 중앙을 비우고 손 바깥쪽에 작은 효과만 표시하며, 3인칭은 기존 스케일을 유지한다.

## 범위와 제약

- Marketplace/Engine 원본 자산은 직접 수정하지 않고 `/Game/Rogue10m` 아래로 복제한다.
- 프로젝트에 없는 검·단검 전용 모션을 이름만 바꿔 재사용하지 않는다. 현재 확보된 Manny 전투·마법 모션을 조합하고 재생 속도, 콤보 창, 판정 형태를 프로필별로 조정한 첫 전투 세트를 만든다.
- 공격 애니메이션과 파티클은 별도 폴더와 별도 데이터 필드로 관리한다. 이동 파티클은 계속 비활성 상태를 유지한다.
- 실제 무기 메시 장착, 신규 무기 모델 제작, 신규 사운드 제작은 이번 범위에 포함하지 않는다.
- 상시 Tick을 추가하지 않는다. 공격 입력, 짧은 Timer, 적중 이벤트만 사용한다.

## Ultrawork Packets

### Packet 1 — 전투 타입과 데이터 계약

- 목표: 기존 직렬화 값을 보존하면서 10개 전투 타입을 식별하고 애니메이션/VFX 튜닝을 데이터화한다.
- 입력: `ERogue10mWeaponType`, `URogue10mAttackSkillData`, 기존 WeaponSkillProfile 구조
- 수정 위치: `Source/Rogue10m/Character/Rogue10mWeaponTypes.h`, `Source/Rogue10m/Data/Rogue10mAttackSkillData.h`, HUD 무기명 매핑
- 완료 조건: 신규 타입이 장비·프로필·HUD에서 구분되고, 공격별 재생 속도와 1인칭 효과 보정값을 에디터에서 조절할 수 있다.
- 검증 명령: `Scripts/BuildEditor.ps1`
- 롤백 경계: 신규 enum 항목과 AttackSkillData 표시 필드

### Packet 2 — 1인칭 안전 Niagara 런타임

- 목표: 공격 효과가 1인칭 조준 시야를 가리지 않도록 카메라 전용 위치·크기·지속시간을 적용한다.
- 입력: 로컬 제어 여부, FirstPersonMesh, AttackSkillData VFX 설정
- 수정 위치: `Rogue10mCombatComponent`
- 완료 조건: 로컬 1인칭 Cast/Charge는 손 소켓 바깥쪽 오프셋과 축소 배율을 사용하고, Impact는 대상 위치에서 짧게 재생된다. 원격·3인칭 효과는 기본값을 유지한다.
- 검증 명령: Editor 자동 검증 및 PIE 로그/스크린 확인
- 롤백 경계: CombatComponent의 Niagara 생성·보정 함수

### Packet 3 — 10개 프로필 자산 구성

- 목표: 프로필별 3연계, 특수, 차징 Montage/AttackSkill/Profile/Niagara를 생성하고 기본 캐릭터 데이터에 등록한다.
- 입력: Manny Unarmed 4종, CombatMagic 35종, 기존 프로젝트 Niagara 원본
- 수정 위치: `/Game/Rogue10m/Animation/Combat`, `/Game/Rogue10m/VFX/Character/Combat`, `/Game/DataAsset/AttackSkill`, `/Game/DataAsset/SkillProfile`, Editor Python 스크립트
- 완료 조건: 10개 프로필, 공격 스킬 50개, Montage 50개가 유효하게 연결되고 프로필별 판정·속도·색감 계열·1인칭 튜닝이 구분된다.
- 검증 명령: `UnrealEditor-Cmd` 구성 및 검증 스크립트
- 롤백 경계: 위 네 신규 Content 하위 폴더와 신규 Editor 스크립트

### Packet 4 — 직접 실행 검증

- 목표: 각 전투 프로필의 기본·특수·차징·연계 실행과 1인칭 효과 크기를 실제 PIE에서 확인한다.
- 입력: 생성 완료된 프로필과 검증용 전투 프리뷰 경로
- 수정 위치: 검증 스크립트 및 필요 시 테스트 전용 런타임 설정
- 완료 조건: 10개 프로필의 Montage 재생, 연계 링크, Niagara 활성, 1인칭 보정 적용을 로그와 캡처로 확인한다.
- 검증 명령: PIE 자동화/직접 실행, Asset validation
- 롤백 경계: 테스트 전용 설정과 생성 캡처

### Packet 5 — 빌드·리뷰·문서화

- 목표: Unreal 수명주기, GC, Timer, Skeleton 호환성, 에셋 참조와 생성물 오염을 점검하고 결과를 기록한다.
- 입력: 코드/에셋 diff와 검증 결과
- 수정 위치: `Feature/doc`, `Docs/SprintChangeLog.md`, `DevLog/20260819.txt`
- 완료 조건: 빌드, 자산 검증, 직접 테스트 결과와 한계가 기록된다.
- 검증 명령: `Scripts/CheckGeneratedChanges.ps1`, `git diff --check`
- 롤백 경계: 문서 변경

## 전투 프로필 매핑

| 프로필 | 공격 성격 | 기본 판정 | 애니메이션 템포 | VFX 방향 | 2단 점프 |
| --- | --- | --- | --- | --- | --- |
| 단검 도적 | 빠른 찌르기·베기 | 좁은 직선/부채꼴 | 매우 빠름 | 청록 잔광·짧은 섬광 | 사용 |
| 표창 도적 | 손목 투척·연속 투사 | 투사체 경로 | 빠름 | 작은 청색 궤적·원거리 적중 | 사용 |
| 쌍단검 도적 | 좌우 교차 연계 | 짧은 다단히트 | 매우 빠름 | 양손 교차 잔광 | 사용 |
| 장검 전사 | 정석 횡베기·찌르기 | 중거리 부채꼴 | 보통 | 은청색 검광 | 미사용 |
| 대검 전사 | 큰 예비동작·강타 | 넓은 부채꼴/원형 | 느림 | 금적색 중량 검광 | 미사용 |
| 쌍검 전사 | 좌우 연속 베기 | 다단 부채꼴 | 빠름 | 양손 은백 궤적 | 미사용 |
| 방패 전사 | 방패 밀치기·충격 | 근거리 직선/원형 | 보통 | 짧은 충격파·불꽃 | 미사용 |
| 한손검·작은방패 | 베기와 방패 견제 | 중거리 직선/부채꼴 | 보통 | 검광+작은 충격파 | 미사용 |
| 마법사 | 지팡이 타격·마력 투사 | 투사체/원형 | 보통 | 보라·청색 룬/마력 섬광 | 사용 |
| 권사 | 잽·스트레이트·권압 | 짧은 직선/다단 | 빠름 | 황금 권압·집중광 | 사용 |

## 1인칭 VFX 안전 기준

- Cast/Charge 효과는 오른손 기준 바깥쪽·아래쪽으로 이동해 화면 중앙 조준 영역을 비운다.
- 1인칭 Cast 0.08~0.14배, Charge 0.06~0.11배, Impact 0.18~0.30배를 기본 범위로 사용한다.
- Cast 방출은 0.07~0.11초, Impact 방출은 0.09~0.14초로 제한한다.
- 불투명한 대형 원판, 화면 전체 왜곡, 지속 점광원은 사용하지 않는다.
- 쌍수 프로필은 양손 소켓 효과를 허용하되 각각의 크기를 단수 프로필보다 작게 한다.
- 이동 파티클은 이번 공격 프리뷰에서 계속 OFF로 유지한다.

## 런타임 구조

```text
Equipped WeaponType
  -> WeaponSkillProfile
     -> AttackSkillData (Montage + 판정 + 재생 속도 + Niagara)
        -> 공통 Manny AnimationSourceMesh
           -> 종족별 Retarget AnimBP
        -> CombatComponent
           -> Local FP: 작은 스케일 + 손 바깥쪽 오프셋 + 짧은 방출
           -> Remote/TP: 기본 스케일 + 소켓 부착
           -> Impact: 실제 피해 대상 위치
```

## 안전 원칙

- 신규 UObject/Component 참조는 `UPROPERTY`와 `TObjectPtr`로 추적한다.
- 차지 Niagara는 입력 해제, 공격 취소, 무기 변경, EndPlay에서 정리한다.
- 동일 소켓에 반복 생성되는 일회성 Niagara는 AutoRelease와 짧은 비활성 Timer를 사용한다.
- 기존 enum 값은 순서를 바꾸지 않고 신규 값만 뒤에 추가해 저장 데이터 호환성을 지킨다.
- 원본 Skeleton이 Manny와 호환되지 않는 자산은 구성 단계에서 제외하고 검증 실패로 처리한다.
