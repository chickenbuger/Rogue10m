# 지팡이 대기 방향 및 장착 구현

## 확인과 범위
StaffGameReviewA의 8장 실제 이미지에서 공격 중 모션은 재생되지만 대기·회복 시 보석이 아래로 향한다. 기존 저장444개와 보호25개는 보존됐으며 원본 실패 후보를 유지한다. 생산 Staff 기본값은 우선 null로 유지한다.

## 구현 packet
- 목표: 3인칭 지팡이의 음의 로컬Y 보석을 대기 시 위로 유지하고 DefaultSlot 기여 가중치에 맞춰 원래 손 부착 공격 회전으로 블렌드한다.
- 영역: WeaponVisualComponent의 opt-in finalized-bone delegate. 캐릭터 pose/카메라/피해·원본 asset은 바꾸지 않는다. 소유권·외부 transform·무기 교체 때 delegate 해제한다. Tick은 추가하지 않는다.
- 장착: 실제 HumanMale의 합성 QA 아이템을 기존 TryEquipItemToSlot로 거래하고 정확 instance·weapon type을 검사한다. 생산 아이템/저장 재시작 검증과 구분한다.
- 검증: BuildEditor.ps1, VerifyStaffPresentation.ps1 fresh label, 8장 실제 판독, 기존 무기 자동화, CheckGeneratedChanges.ps1. 대기/회복 jewel dot>.995, pivot/scale 보존, montage 연속 재생, 원본25/저장444 해시 보존.
- 되돌림: tmp/staff-idle-resume-20261111/baseline의 해당 파일별 최소 변경만 되돌린다. 공유 작업은 폐기하지 않는다.
- 배포: 소유 Fab 파생 로컬 사용. 공개 원본 재배포 미확인으로 commit/push하지 않는다. 1인칭 신규 개발 보류 유지.

## 실제 렌더 피드백 후 통합
B의 수치 검사는 통과했으나 8장 실제 판독에서 기존 grip-spec의 음의Y 보석 주석이 틀렸음을 확인했다. B를 시각 실패로 보존하고 양의 로컬Y를 위로 교정한다. 수정 후 실제 HumanMale BP가 native StaffMesh/Grip/policy 기본값을 override 없이 상속하는 C 검증으로 전환한다. 로컬 production 구현을 먼저 반영하되 C 화면/무기 회귀를 통과하기 전 완료로 승인하지 않는다. 원본 에셋·첫인칭 기능·피해 타이밍은 보존한다. 실제 인벤토리는 public TryAddGridItem→TryEquipGridItem 경로를 사용한다.
