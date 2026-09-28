# V키 3인칭 전신 확인

작성: 2026-09-28
브랜치: Sprint#4-40-third-person-inspection
상태: 구현·빌드·지정 범위 실행 검증 완료.

V키를 누르면 3인칭으로 현재 외형과 애니메이션을 확인하고 다시 누르면 기존 Appearance 머리 카메라로 돌아간다. 관찰 중에도 이동·공격·몽타주는 기존 경로를 유지한다.

## 구현

- AppearanceCameraComponent의 관찰 상태와 CalcCamera 최종 POV를 연결했다. 관찰 카메라는 캡슐 중심(높이 오프셋0cm)에서 시선 반대 방향300cm에 위치한다. 거리·높이·충돌 반경12cm는 편집 가능하다.
- 실제 FirstPersonCameraComponent는 계속 머리를 따라 계산한다. 공격 판정과 이펙트 기준을 바꾸지 않고 화면의 관찰 위치만 변경했다. 따라서 관찰 화면 중앙이 공격 조준점과 일치하지 않을 수 있다.
- 3인칭에서는 이 모드가 숨겼던 head와 얼굴·머리카락 OwnerNoSee를 복원한다. 1인칭 복귀 시 원래 가림 설정으로 돌아간다. 별도 FirstPersonMesh는 계속 비활성이다.
- ECC_Camera 구형 스윕으로 벽을 만났을 때 관찰 거리를 줄인다. 관찰 중심부터 이미 벽 내부인 경우에는 중심까지 당기는 제한이 있다.
- V Pressed를 기존 PlayerController 키 바인딩 방식에 연결했다. 인벤토리·설정 등 차단 UI, 사망·일시정지·다른 viewtarget에서는 전환하지 않는다. ControlRotation을 재설정하지 않는다.
- Pawn 소유 해제, 카메라 모드 해제, 종료 때 파츠 설정과 관찰 상태를 복원한다. 네트워크 동기화나 자유 궤도 관찰 기능은 추가하지 않았다.

## 변경 파일

- Source/Rogue10m/Components/Rogue10mAppearanceCameraComponent.h/.cpp
- Source/Rogue10m/Core/Rogue10mPlayerController.h/.cpp
- Source/Rogue10m/Character/Rogue10mCharacter.cpp
- Source/Rogue10m/Tests/Rogue10mThirdPersonInspectionRuntimeTest.cpp

## 검증

UE 5.8 Editor 최종 빌드12.43초 성공. 실행 중인 에디터는 종료하지 않았으며 엔진의 핫 리로드 모듈 빌드가 사용됐다. 새 게임 프로세스에서 최종 모듈을 검증했다. 기존 에디터에서 변경이 보이지 않으면 작업을 저장하고 에디터를 재시작한 뒤 플레이한다.

`Rogue10m.TestThirdPersonInspection`에서 280프레임, 실패0으로 종료했다.

| 항목 | 결과 |
| --- | --- |
| V키 입력 | PlayerController.InputKey(CreateSimulated) → 실제 바인딩, 왕복3회 |
| 1인칭/3인칭 자동 화면 검사 | 135 / 145샘플 |
| 실제 머리 카메라 위치/회전 오차 | 0cm / 0.00000418도 |
| 좌우 원본 잽 몽타주 | 각각12샘플, 공격2회 |
| 벽 충돌 | 10샘플, 거리300→128cm, 제거 후300cm복원 |
| 인벤토리 입력 차단 | 실제 V입력 및 직접 handler 호출 모두 전환되지 않음 |
| 외형/가림 | Source/Appearance AnimClass유지, 3인칭head표시, 파츠 OwnerNoSee왕복복원, 별도FP계속비활성 |
| 생성 경로/공백 검사 | 통과 |

초기 촬영은 몽타주가 끝난 뒤 공격 회복 상태를 잘못 실패 처리해8개 assertion이 발생했다. 활성 몽타주가 있으면 원본 클립을 검사하고 실제 샘플 수를 유지하며, 몽타주 없는 회복8프레임은 별도로 기록하도록 테스트만 수정했다. 다음 촬영은 외부 마우스 입력 유입으로 고정 시선 검사가 실패했다. 최종 테스트에서는 Pawn입력만 임시 비활성화하고 Controller V입력 경로는 유지해 분리했으며 종료 때 입력 상태를 복원했다. 게임의 일반 조작에는 이 격리를 적용하지 않는다. 최초 실패 로그도 증거 폴더에 보존했다.

첫 시각 확인에서 기본 높이60cm는 하체가 HUD에 가려, 최종 높이를0cm로 조정했다. 실제 최종 화면에서 머리·등·팔·다리와 공격 자세를 확인했다. 벽 가까이에서는 카메라가 당겨져 몸이 크게 보인다. HUD와 모션에 따라 발 일부가 가려질 수 있다.

[실제 V키 전환 영상](evidence/third-person-inspection-20260928/v-third-person-gameplay.mp4) — 1280×720,30fps,280프레임,9.33초.

증거: `evidence/third-person-inspection-20260928/verified.log`, `build-verified.log`, `source-review.md`, `final-source-hashes.json`, `contact-sheet.jpg`.

3인칭 중 공격 재생과 반복 시점 전환을 검증했다. 공격이 진행 중인 정확한 순간의 토글, 사망·재빙의·외형 교체·네트워크·이미 벽 안에서 시작하는 충돌 복구는 별도 실행 검증하지 않았다. 관찰 시점은 공격 판정 기준을 옮기지 않으므로 화면 중앙과 실제 공격 조준이 일치하는 새 3인칭 전투 카메라로 해석하면 안 된다.

 기존 작업 중인 파일과 애셋은 보존했으며 이번 기능은 바이너리 애셋 변경 없이 C++에서 구현했다. 커밋·푸시는 하지 않았다.
