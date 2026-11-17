# 지팡이 실제 장착 및 대기 방향 구현

## 반영된 기능
- WeaponVisualComponent의 native StaffMesh·StaffGrip·upright 기본값을 실제 프로젝트에 연결했다. 실제 HumanMale BP가 override 없이 상속하는 것을 검증했다.
- 별도 finalized-bone delegate가 모델 양의 로컬Y 보석 방향을 대기 시 위로 보정하며 DefaultSlot 실제 global weight로 authored 손 회전에 블렌드한다. 소유권·메시·소켓·외부 transform takeover 확인 후에만 갱신한다. 무기 전환·해제·파괴 시 정리하며 새로운 component Tick은 없다.
- 원래 static grip의 위치·scale과 전투 판정·피해·입력·카메라·원본 에셋은 유지했다. policy 재활성화는 기존 RefreshWeaponVisuals 이벤트 경로를 따른다.

## 검증
- Editor 최종 native-default 빌드 성공53.54초. 앞선 fixture protected ItemSlots 접근 컴파일 실패는 public TryAddGridItem→TryEquipGridItem로 교정했고 실패 로그를 보존했다.
- StaffNativeResumeC: 실제 프로필/ASC HumanMale, 합성 QA DataAsset의 실제 grid 획득·정확 instance 장착, 연속461프레임·2클립·8장 callback 캡처·Sword/Knuckle 복귀, 실패0. 기존444저장과 보호25패키지 해시 동일.
- 8장 모두 직접 판독했다. 대기·회복에서 파란 보석이 위를 향하고, Strike/Shot 동안 원래 손 동작을 따라 움직인다.
- 첫 B는 수치만 통과했으나 원본 grip-spec의 잘못된 음의Y 주석을 따라 보석이 아래로 향했다. B는 시각 실패로 보존했고 실제 양의Y를 C에서 확인했다.
- 75입력조합의 upright 수학·유효성 자동화와 기존 무기 회귀는 별도 최신 automation 결과를 따른다.

## 사용 자산·보존
로컬 소유 Wizard for Battle Polyart의 /Game/Weapons/Staff20261110/SM_BattleWizardStaff01과 기존 StaffResumeC Manny Strike2.0초·Shot2.9초를 사용했다. 180cm 후보 scale과 기존 provisional grip 위치를 유지했다. 원본 애니메이션/메시·스킬 DA·몽타주 패키지를 저장하거나 변경하지 않았다. 공개 재배포 권한 미확인으로 commit/push하지 않는다.

## 범위와 다음 작업
실제 장착·native 표시·대기 방향의 구현 결과다. 클립 검수는 dynamic DefaultSlot이며 신규 Staff 모션을 실제 스킬 DA에 다시 연결하거나 피해 시점을 바꾼 결과는 아니다. 합성 QA 아이템, 고정 HumanMale, 프로그램 검수이므로 물리 UI 입력·production 아이템 저장재시작·다른 종족·이동/낙하 전체·정확 손가락 접촉·풀런·패키지는 별도다. 1인칭 신규 개발은 보류한다.

근거: Feature/doc/evidence/staff-equipment-20261110/{StaffIdleBuildA,StaffIdleResumeB,StaffNativeResumeC}, DevLog/20261110_StaffPresentation/StaffNativeResumeC.

무기 최종 회귀: StaffNativeUnitC 실제 report12/12성공·경고0·오류0, 저장444보존.75조건 StaffUprightMath 포함. 실행기 요약의 과거 개수11표기는 원본을 보존한 reconciled-summary.json으로 정정했고 검증 gate는 처음부터12개였다.
