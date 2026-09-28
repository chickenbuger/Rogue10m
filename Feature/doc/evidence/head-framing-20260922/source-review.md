# 초기 30도 머리 시선 독립 소스 검토

- 날짜: 2026-09-22
- 역할: 검증 1, 런타임 소스 수정 없음
- 비교 기준: tmp/head-framing/before-runtime의 4파일
- 판정: 런타임 소스 검토 PASS. 빌드·전용 실행 fixture·실제 영상 검증은 별도 진행 중이며 이 판정으로 대체하지 않는다.

## 확인 범위

Rogue10mPlayerController.h/.cpp, Rogue10mFirstPersonPresentationComponent.h/.cpp의 이번 변경을 직접 비교했다. 플레이어 카메라, 캐릭터 빙의, Presentation 갱신, CombatComponent 공격 방향 및 UE 5.8 PlayerController/GameModeBase 생명주기를 함께 확인했다.

## 상태와 초기화

Controller::UpdateRotation은 로컬 Rogue10mCharacter의 Presentation에 1회 초기화 기회를 전달한 뒤 반드시 Super::UpdateRotation을 호출한다. 실제 방향 변경은 HasBegunPlay, 로컬 Controller, 소유 Pawn 일치, Character 로컬 제어를 확인한 뒤 수행된다. Pawn 소유 컴포넌트의 bInitialLocalViewHandled로 수명 동안 딱 한 번 소비하므로 weak Pawn 캐시가 필요하지 않다. 무기 교체·ResetCameraMotion·RefreshPresentation·RestorePresentation은 이 bool을 되돌리지 않는다. 같은 Pawn 재빙의도 재적용하지 않으며 새 Pawn은 새 컴포넌트로 시작한다.

비활성/비전신/사망/유효하지 않은 튜닝/명시적 스폰 피치도 초기 기회를 소비한다. 이후 무기를 복싱으로 바꾸거나 수평으로 시선을 돌렸을 때 지연 적용되어 갑자기 하향하는 문제가 없다. BeginPlay 이전 또는 비소유 호출은 소비하지 않는다. 현재 MenuGameMode는 DefaultPawnClass와 SpectatorClass가 null이므로 기존 메뉴 경로는 적용 대상을 갖지 않는다. 임의로 Rogue 캐릭터를 메뉴 컨트롤러에 빙의하는 확장까지 별도 메뉴 차단한다고 주장하지 않는다.

## 방향과 입력 계약

GetControlRotation의 피치를 NormalizeAxis 후 0.01도 이내 수평인지 판단한다. -30도 기본값을 [-60,0] 범위로 제한하고 유한값을 검사한다. yaw/roll은 변경하지 않는다. 회전 전체가 유효하지 않으면 설정을 건너뛴다. Super::UpdateRotation이 같은 프레임의 RotationInput과 CameraManager 제한을 이어 적용하므로 첫 프레임 마우스 입력을 버리지 않는다. 초기 설정은 카메라 POV만 기울이는 방식이 아니므로 CameraComponent와 FullBodyAim이 같은 기본 조준축을 사용한다. CombatComponent의 CameraComponent ForwardVector 기반 공격 계산을 변경하지 않는다.

UE GameModeBase::FinishRestartPlayer가 Possess 이후 스폰 방향을 다시 쓰는 점을 고려하면 동기식 싱글플레이의 첫 UpdateRotation 진입점은 적절하다. 현재 프로젝트의 싱글플레이 범위에서 확인했으며 원격 클라이언트 RPC 도착 순서까지 보장하는 네트워크 검증은 수행하지 않았다.

## Unreal 및 회귀

새 UObject 보유 필드가 없고 1회 bool은 소유 컴포넌트의 값 상태다. 새 Tick·타이머·애니메이션 worker 접근이 없다. 튜닝은 EditDefaultsOnly/BlueprintReadOnly UPROPERTY로 노출되어 있다. 필요한 PlayerController 헤더를 cpp에 추가했고 generated.h의 헤더 순서는 유지했다. 변경은 초기 ControlRotation만 다루며 카메라 mount, FOV, head-follow gain/limit, 팔 IK/손목/Idle/공격 모션, HUD, 피해 계산은 수정하지 않는다.

## 실행 검증 요청

- 실제 기본 Pawn의 첫 회전 갱신 후 -30도 및 yaw 보존.
- 비수평 스폰 피치 보존, 비활성/팔 전용 최초 시작이 나중에 재적용되지 않음.
- 최초 갱신 RotationInput과 이후 자유 시선이 유지됨.
- 무기 교체·표현 갱신·같은 Pawn 재호출 뒤 사용자 조준 보존.
- 새 Pawn 1회 적용, 잘못된 Controller/비소유 호출은 소비하지 않음.
- head-follow 강도 0일 때 카메라와 ControlRotation 일치, 기본 강도 복원 뒤 기존 머리 추종 유지.
- 직접 Boxing -30도 영상과 실제 게임의 -30도는 eye calibration이 다르다는 점을 구분해 확인.

## 검토한 SHA256

| 파일 | SHA256 |
| --- | --- |
| Rogue10mPlayerController.h | FD8ED4B8B3A8510BB65D4B90FB839840C7BA159EEBB9A37E01D85991D817CD00 |
| Rogue10mPlayerController.cpp | 593E1314DDF527381A7D54F4F8E3465FCA328225A8222AB731D78CADF6AA3680 |
| Rogue10mFirstPersonPresentationComponent.h | 0F6F23DF1A9B3763CAFF309B03C587B58E90344D9EE78578A47D5835227869EA |
| Rogue10mFirstPersonPresentationComponent.cpp | 5BAE0FAC12BD104B077087237BB3B3BD54E5FDA982B050376FE3FC65D579C701 |


## 전용 fixture와 Boxing 30도 옵션 후속 검토

Rogue10mInitialHeadFramingRuntimeTest.cpp를 전체 확인했다. 최초 4초 동안 플레이어의 자연스러운 스폰·회전 갱신을 기다리고, fixture가 ControlRotation을 쓰기 전에 원래 ControlRotation과 CameraComponent 피치가 -30도인지 검사한다. 따라서 테스트 자체가 초기 각도를 만들고 성공을 선언하는 구조가 아니다. 이후 명시적인 사용자 시선 값(-12도, 기존 yaw+17도), 초기화 재호출, RefreshPresentation, Staff→Unarmed 복귀를 거쳐 그 방향이 유지되는지 검사한다. AddPitchInput/AddYawInput을 실제로 전달하고 다음 프레임에 방향이 바뀌는지도 확인한다. CameraComponent 전방과 ControlRotation 전방의 내적을 검사하므로 POV만 아래로 기울이는 잘못된 구현을 구분한다.

검증은 단독 Editor 게임에서만 실행되고 90프레임 후 종료한다. 원래 무기·피격 허용·시선·입력 무시 카운트·고정 시간 설정을 저장/복원하고 약한 World/Character/Controller 참조 및 delegate 정리를 사용한다. 소스 차단 이슈는 발견하지 못했다. 실제 실행 성공 여부는 별도 로그를 따른다.

이 fixture가 직접 실행하는 범위를 정확히 한정한다. 최초 명시적 비수평 스폰, disabled/legacy 최초 시작, NaN 튜닝, 새로운 Pawn 및 잘못된 Controller, 최초 회전 갱신 프레임의 마우스 입력, head-follow 강도 0 경로는 이 fixture에서 실행하지 않는다. 이미 처리된 Pawn의 명시적 사용자 시선 보존과 실제 최초 스폰 피치 보존은 서로 다른 경우다. 해당 경계는 현재 소스 검토 결과이며 전부 런타임 검증됐다고 설명하지 않는다. AddPitchInput 검사도 물리적 마우스 장치의 end-to-end 검증은 아니다.

BoxingDirectPreviewRuntimeTest의 -RogueBoxingForwardView는 -30도/noUI와 BoxingDirectPreviewForward30 출력 폴더를 선택한다. 기존 -RogueBoxingLookDown의 -40도/noUI와 기본 0도/UI 경로는 유지한다. 두 옵션을 동시에 넘기면 ForwardView가 우선한다. 원본 1.733초 sequence와 동일 샘플 평가, 실제 raw-head 대비 0.05cm/0.05도 오차, camera limit 미도달, ControlRotation 불변, 원상복구 검사를 유지하므로 비교 촬영 옵션이 원본 애니메이션을 바꾸지는 않는다. 이 fixture의 raw-head gain 1 및 eye calibration은 게임 기본값을 검증하는 수단과 구분해야 한다.

첫 빌드의 C4458은 지역 Character가 AController::Character 멤버를 가린 이름 문제였다. ControlledCharacter로 변수 이름만 수정한 현재 코드를 확인했으며 조건/호출/동작은 동일하다. 위 SHA256 표는 이 수정 후 Controller.cpp 해시로 갱신했다. 재빌드 및 실행 결과는 통합 담당이 확인 중이다.

| 후속 검토 파일 | SHA256 |
| --- | --- |
| Rogue10mInitialHeadFramingRuntimeTest.cpp | 91979FCD3BFF4A64B33BF5DF721D7BB9B9933FE356115A7447A087A0FF7B4787 |
| Rogue10mBoxingDirectPreviewRuntimeTest.cpp | 527F953D838D7DE70FC34367852B91AE9C8C2B9B35883DD74F3E9D69CC2E0EF7 |



## 회전 입력 fixture 순서 수정 검토

첫 실행에서는 실제 초기 -30도와 후속 방향 유지가 확인됐지만 frame31 입력 검사가 실패했다. 소스에서 원인을 확인했다. fixture 콜백은 OnWorldPostActorTick이므로 AddPitchInput/AddYawInput 뒤 곧바로 look-ignore를 true로 복원하면 다음 컨트롤러 tick의 APlayerController::PostProcessInput이 RotationInput을 0으로 지운다. 엔진 소스 PlayerController.cpp:2781의 명시적인 동작이며 초기 시선 런타임의 입력 강제 고정 문제가 아니다.

수정 fixture는 frame30에서 잠시 입력 무시를 해제하고 프로그램 입력을 누적한 다음, 실제 가상 PC->UpdateRotation(1/30)을 직접 호출한다. 바뀐 ControlRotation을 ExpectedControl로 저장한 뒤 입력 무시를 복원한다. frame31은 그 결과가 다음 일반 tick에도 보존되는지 검사한다. 실제 Rogue 컨트롤러 override와 Super 회전 처리를 통과하며 fixture의 보호 장치가 입력을 먼저 삭제하지 않도록 순서를 바로잡았다. 검사의 의미는 프로그램 입력→실제 UpdateRotation→다음 tick 유지이며 물리 마우스·OS 이벤트의 종단 검증은 아니다. 최초 스폰 프레임에 들어오는 입력을 직접 검증하는 것으로도 설명하지 않는다.

최신 테스트 소스 검토 PASS, 런타임 Controller.cpp 해시는 이전과 동일하다. 테스트 SHA256 표는 91979FCD3BFF4A64B33BF5DF721D7BB9B9933FE356115A7447A087A0FF7B4787로 갱신했다. 통합 담당의 최신 빌드 결과는 성공 11.22초이며 최종 실행 결과는 재실행 로그 확인 후 별도로 기록한다.
