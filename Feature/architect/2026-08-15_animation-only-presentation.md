# 애니메이션 단독 표시와 VFX 분리 계획

## 목표

- 플레이어의 이동·점프·구르기·공격 애니메이션은 유지한다.
- 현재 프리뷰에서는 이동 및 공격 Niagara를 전부 비활성화한다.
- 애니메이션과 파티클 자산 및 설정을 독립적으로 유지해 파티클을 나중에 다시 활성화할 수 있게 한다.

## 변경 범위

- `URogue10mCharacterMotionDataAsset`: 이동 파티클 표시 스위치 추가, 기본값 OFF.
- `URogue10mAttackSkillData`: 공격 파티클 표시 스위치 추가, 기본값 OFF.
- 이동·전투 컴포넌트: Niagara 생성 전에 해당 스위치를 검사한다.
- 구성·검증 스크립트: 두 스위치를 OFF로 저장하고 애니메이션 전용 프리뷰 상태를 검증한다.
- 자산 경로는 애니메이션 `/Game/Rogue10m/Animation`, 파티클 `/Game/Rogue10m/VFX`로 유지한다.

## 완료 조건

- 애니메이션 몽타주와 이동 동작 연결은 변경되지 않는다.
- 이동 및 공격 Niagara가 기본 실행 경로에서 생성되지 않는다.
- 파티클 자산은 삭제하지 않고 독립 폴더에 보존된다.
- UE 5.8 Editor target 빌드와 자동 자산 검증이 통과한다.

## 검증 명령

- `./Scripts/BuildEditor.ps1`
- Unreal Editor commandlet로 `ConfigureCommonCharacterAnimation.py` 실행
- Unreal Editor commandlet로 `ValidateCommonCharacterAnimation.py` 실행
- `./Scripts/CheckGeneratedChanges.ps1`
- `git diff --check`

## 롤백 경계

- 두 Data Asset의 표시 스위치를 다시 켜면 기존 파티클 연결과 튜닝을 그대로 재사용할 수 있다.
