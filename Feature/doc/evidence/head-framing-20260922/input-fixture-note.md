# 실제 회전 입력 fixture 수정 원인과 범위

첫 실행은 초기 ControlRotation·CameraComponent·렌더된 시야의 Pitch −30°를 확인했으나, 31번 프레임의 입력 변화 검사에서 실패했다. 생산 카메라 코드의 회귀가 아니라 테스트 입력 주입 시점 문제였다.

`APlayerController::AddPitchInput`과 `AddYawInput`은 `RotationInput`에 입력을 누적한다. UE 5.8의 `PlayerController.cpp`에서 `PostProcessInput`은 `IsLookInputIgnored()`가 참이면 이 값을 0으로 지운다. 테스트는 `OnWorldPostActorTick`에서 자체 ignore 스택을 잠시 풀고 입력을 넣은 뒤 즉시 ignore를 복원했으므로, 다음 컨트롤러 입력 처리 단계가 누적 입력을 소비하기 전에 지워졌다.

수정한 fixture는 자체 ignore를 해제한 상태에서 `AddPitchInput(3)`과 `AddYawInput(2)`를 호출하고, 실제 가상 `UpdateRotation(1/30)` 경로를 즉시 실행해 누적 입력을 소비한다. 결과가 유한하고 이전 각도와 다름을 검사한 뒤 ignore를 복원한다. 다음 프레임에는 그 소비 결과가 유지됨을 검사한다. 입력 스케일의 정확한 수치를 가정하지 않고 실제 결과를 이후 기대값으로 사용한다.

따라서 검증 범위는 게임의 입력 누적 API → 프로젝트 컨트롤러의 가상 UpdateRotation → 엔진 시야 처리 → 후속 틱·무기 전환·표현 갱신에서의 보존이다. 물리 마우스 장치 이벤트나 운영체제 입력 전달 자체를 자동화한 검사는 아니다. 생산 소스는 이 fixture 수정에서 변경하지 않았다.

로그에는 `INPUT_CONSUMED phase=explicit_UpdateRotation_after_PostActorTick`와 `INPUT_NEXT_TICK`를 남긴다. 최종 성공 여부는 수정 이후 재빌드·재실행 로그로 판단한다.
