# 머리 카메라의 초기 시선을 30도 아래로 조정

- 날짜: 2026-09-22
- 브랜치: Sprint#4-37-forward-head-framing
- 상태: 구현·빌드·초기 시선·비교 촬영·머리 추종 회귀 검증 완료
- 역할: 기획 1, 구현 이후 독립 소스 검증 1. 이 문서 작성자는 런타임 소스를 수정하지 않는다.

## 목표

사용자가 선호한 Boxing 직접 재생의 -40도 아래보기보다 10도 정면에 가까운 -30도 구도를 게임 시작 시 제공한다. 기본 시선만 한 번 설정하고 이후 마우스로 자유롭게 위아래를 본다. 현재 손을 내린 Idle, 공격 시 팔 올림, 전신 표시, 머리 움직임 추종을 유지한다. UI를 없애는 것은 비교 영상의 표현 선택이며 실제 게임 HUD 제거 요청으로 확장하지 않는다.

## 코드 근거와 진입점

FirstPersonCameraComponent는 bUsePawnControlRotation=true이고, FistAnimInstance의 FullBodyAim도 GetBaseAimRotation을 읽는다. CombatComponent는 CameraComponent의 위치·ForwardVector로 공격 방향을 정한다. 따라서 CameraManager 최종 POV에만 상수 -30도를 더하면 조준과 공격 방향이 벌어진다. 초기 ControlRotation.Pitch를 -30도로 설정하여 기존 카메라·조준·팔 조준 경로에 같은 입력을 전달한다. 머리 추종의 기존 부가 움직임은 별도 유지한다.

UE 5.8 소스를 확인했다. PlayerController::OnPossess는 Character::PossessedBy 이후 Pawn 회전으로 ControlRotation을 덮어쓴다. GameModeBase::FinishRestartPlayer는 Possess 뒤 ClientSetRotation, SetControlRotation(StartRotation)을 다시 호출한다. 따라서 Character::PossessedBy, PawnClientRestart, Controller::OnPossess에서 즉시 적용하면 정상 스폰 과정에서 취소될 수 있다. Controller::BeginPlay만 사용하면 Pawn 생성 전 호출 또는 후속 재생성을 처리하지 못한다.

권장 최소 진입점은 Rogue10mPlayerController::UpdateRotation의 Super 호출 전이다. 현재 로컬 게임 Pawn이 최초 평가 대상인지 확인해 한 번만 초기 시선을 설정한 뒤 Super::UpdateRotation을 호출한다. 기존 컨트롤러 갱신을 활용하므로 새 Tick이나 반복 타이머를 추가하지 않는다. Super가 해당 프레임의 마우스 RotationInput과 CameraManager 피치 제한을 처리하므로 최초 입력을 버리지 않는다. 이후 갱신은 초기화 조건이 거짓이어서 정상 입력만 처리한다.

## 정책과 경계

- 새 로컬 Rogue10mCharacter의 기본 수평 스폰 시선에만 적용한다. 메뉴·관전자·다른 Pawn·리모트 컨트롤러는 제외한다.
- 기본 설정은 활성화, InitialViewPitch=-30도이며 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)로 노출한다. NaN을 방어하고 현재 허용 피치 범위 안에서 설정한다.
- 현재 초기 피치가 명시적으로 0도가 아니면 맵/스폰/로드에서 제공한 방향으로 보고 보존하는 것을 권장한다. NormalizeAxis 후 작은 허용 오차로 수평을 판단한다. yaw와 roll은 기존 스폰 값을 보존한다. 별도 저장 시선 복원 기능이 추가되면 수평 저장값도 구분할 명시적 초기화 정책이 필요하다.
- 처리한 Pawn은 비소유 TWeakObjectPtr로 기록한다. 무기 교체·RefreshPresentation·UI 열고 닫기는 적용 조건이 아니며 시선을 덮어쓰지 않는다. 새 Pawn은 새 스폰으로 취급한다. 같은 Pawn을 다시 빙의하는 경우와 재시작하는 경우의 재적용 여부를 구현에서 명시하고 검증한다.
- 현재 GameMode는 싱글플레이 전용이다. 서버/원격 ClientRestart RPC의 지연 순서까지 지원한다고 주장하지 않는다. 향후 네트워크 지원 시 초기 시선의 서버·소유 클라이언트 정책을 함께 설계한다.
- FirstPersonPresentation의 카메라 mount 위치·높이·FOV, FullBodyAim 보정, 머리 추종 gain/limit, 공격 모션 및 피해 계산은 변경하지 않는다. -30도는 실제로 아래를 조준하는 초기 방향이므로 낮은 적/바닥 쪽을 본다는 결과를 그대로 보여준다.

## 작업 단위와 검증

1. 런타임: PlayerController.h/.cpp에 초기 시선 튜닝·Pawn 처리 상태·1회 적용을 추가한다. 완료 조건은 새 수평 스폰 -30도, yaw 보존, 후속 입력 자유, 무기 교체 유지다. 롤백 경계는 해당 초기화 코드와 프로퍼티다.
2. 검증: 기존 Editor 빌드 Scripts/BuildEditor.ps1 후 전용 런타임 fixture로 초기 방향, 명시적 스폰 피치 보존, 프레임 진행 및 무기 교체 후 사용자 시선 유지, 활성/비활성, 메뉴/다른 Pawn 제외를 확인한다. 원래 fixture가 첫 프레임에 시선을 0도로 덮는 경우에는 기본 스폰 구도 검증 자료로 쓰지 않는다. 실제 Mouse/RotationInput과 CameraComponent 전방 일치도 확인한다. 머리 추종 gain 0일 때 POV와 ControlRotation 일치를 확인하고, gain 복원 후 기존 head-follow 동작을 확인한다.
3. 시각 검증: 실제 플레이의 -30도 Idle/좌잽/오른잽/스트레이트/훅을 동일 해상도에서 확인한다. 사용자 선호 -40도 Boxing 원본 비교가 필요하면 동일 직접 재생 fixture에서 -30도 보조 촬영을 한다. 게임 기본 카메라와 직접 재생의 fixture eye calibration은 다르므로 두 결과가 픽셀 단위로 같다고 설명하지 않는다.
4. 결과: Feature/doc, DevLog/20260922.txt, Docs/SprintChangeLog.md는 통합 담당이 변경 범위·빌드·실행·영상 확인을 함께 기록한다. 승인 없는 커밋은 하지 않는다.

## 리뷰에서 확인할 위험

첫 프레임 전에 이미 사용자가 제공한 시선이나 컷신 방향을 덮어쓰는지, 무기 재진입 시 -30도로 튀는지, 새 Pawn 기록이 dangling reference가 되는지, 실제 공격 방향과 기본 카메라 축이 어긋나는지 확인한다. 카메라 부가 움직임에 의한 기존 POV 차이와 이번 초기 상수 오프셋을 구분한다. -30도 시야에 손이 더 잘 보이더라도 이전 모션의 손목/손 경로 검증 결과를 대체하지 않는다.

## 구현 후 독립 소스 검토

초기 방향 튜닝과 1회 소비 상태는 Pawn 소유 FirstPersonPresentationComponent로 확정했다. Controller는 UpdateRotation에서 요청만 전달하고 Super로 정상 입력을 처리한다. 계획의 weak Pawn 캐시 대신 컴포넌트 bool을 사용하므로 같은 Pawn의 무기 교체·표현 갱신·재빙의에는 다시 적용하지 않고 새 Pawn에서 새로 시작한다. 초기 비활성/비전신/명시 피치도 소비하여 뒤늦은 시선 재설정을 막는다. 기본 -30도, 유효 범위 [-60,0], 수평 판단 0.01도다. 4파일 독립 소스 검토에서 차단 이슈는 없었으며 빌드·실행·영상 결과는 후속 증거로 기록한다.

최종 머리 카메라 회귀 검사: 합성 포즈 1,446샘플, 실패0, 최대 각도 변화3도·후방 내적-1.0. 이 결과는 기존 머리 회전 처리의 수치 검증이며 새로운180도 공격 모션을 제작했다는 의미는 아니다.
