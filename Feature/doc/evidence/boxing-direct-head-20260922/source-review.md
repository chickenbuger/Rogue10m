# Boxing 원본 직접 재생 — 기획 후 독립 소스 검토

- 날짜: 2026-09-22
- 역할: 검증 1. 기획 문서 작성자는 임포트·런타임 소스를 수정하지 않고 두 개발자의 구현을 검토했다.
- 정적 판정: PASS. 원본 프레임0 머리 높이 기반 눈 기준점과 raw 머리 delta 대조까지 최종 확인했다. 빌드·프리뷰 실행·화면 품질은 별도 증거로 판단한다.
- 대상: `Scripts/Editor/PreviewBoxingRetarget.py`, `Source/Rogue10m/Tests/Rogue10mBoxingDirectPreviewRuntimeTest.cpp`, 기존 PresentationComponent의 SingleNode 머리 추종 소비 경로.

## 원본 및 에셋 경계

원본 SHA256 `7392B75DE2AE387B774DB980C294DA6B0A16180CC660D9E639CE505016E86BF1`를 직접 재확인했다. 임포트 스크립트는 알려진 두 스택 중 1.70~1.77초 길이의 유일한 AnimSequence를 선택한다. 실제 import-report에서 `SK_BoxingSource_Anim_mixamo_com`, 길이 1.733333349초를 확인했다. 정적인 3.333초 Take001을 재생하지 않는다.

모든 임포트/rig/retarget/output 저장은 `/Game/Rogue10m/Animation/Preview/Boxing/` 아래로 제한한다. 이미 존재하는 source 자산은 원본 해시 metadata가 다르면 재사용을 거부한다. 신규 source dependency를 명시적으로 저장하고, Manny는 로드/목표 메쉬 지정만 한다. 목표 스켈레톤 일치·길이·필요 뼈와 유한값 검사가 있다. 리타깃은 원본 애니메이션을 입력으로 사용하며 절차적 권사 포즈를 원본이라고 대체하지 않는다.

## 동작 보존 근거

`retarget-poses.json`의 53개 실제 평가 포즈와 SDK 원본을 비교한 `source-target-comparison.json`을 검토했다. 초기 어깨선 기준 전방 축을 source Y-up과 target Z-up에 맞춰 비교한다. 자기 어깨에 대한 손 위치를 사용하므로 골반 전진을 주먹 타격량으로 오인하지 않는다.

- 왼손 최대 전진은 source/target 모두 13번 프레임이며, 전방 궤적 상관 0.999781이다.
- 왼손 전방 범위 source 34.575cm / target 34.161cm, 오른손 source 3.893cm / target 3.695cm다. target 왼손 변화가 오른손의 약 9.24배로 왼손 직선·반대손 가드가 유지된다.
- 골반 전방·상하 궤적 상관은 각각 0.999996 / 0.999991이다. 골반 전방 이동 범위는 source 약 48.261cm / target 약 45.595cm로, 제자리로 지워지지 않았으며 체형 변환에 따른 차이가 있다.
- 발 비교도 확인했다. 왼발 초기 골반 대비 전방은 source19.46→target22.75cm, 오른발은 -34.31→-32.46cm다. 왼발 최대 전진62.828→60.302cm, 오른발21.700→18.434cm이며, 양발 전진/골반 상대 곡선 상관 모두0.997 이상이다. 왼발 선행과 전진 우세를 보존했다. 재현 스크립트 `Scripts/ValidateBoxingRetarget.py`는 이 값과 손/골반 방향·피크를 assert한다. 위치 상관 하나만으로 메시 변형이나 접지 품질까지 확정하지 않는다.

## 런타임·카메라 계약

프리뷰는 WITH_EDITOR의 standalone -game 전용이며 PIE/에디터 실행과 중복 실행을 거부한다. 실제 FirstPersonMesh에 target AnimSequence를 SingleNode로 직접 재생한다. sequence와 원래 animation class는 TStrongObjectPtr로 보존하며 월드·캐릭터·컨트롤러는 약한 참조로 관리한다. 워커 스레드에서 Actor 상태를 조회하는 새 경로가 없다.

원본 1배속 53 sample과 0.5배속 105 sample을 정해진 시각에 SetPosition→TickAnimation→RefreshBoneTransforms로 평가한 다음 UpdateCamera를 실행한다. 숨긴 head socket 위치를 카메라 원본으로 읽지 않고 기존 raw sequence 평가 경로를 그대로 이용한다. 실제 캡슐 transform, mesh transform, ControlRotation, 공격 AcceptedSerial이 유지되는지 검사한다.

촬영에서만 HeadCameraRotationScale와 TranslationScale/CameraMotionScale을1, FollowSpeed0으로 설정한다. 위치80cm·pitch85/yaw180/roll60 제한을 사용하며, 실제 clip이 제한에 닿는지 CameraLimitFrames로 기록한다. 제한 미도달 확인을 전제로 감쇠 없는 머리 추종이라 설명할 수 있다. 기준점은 실제 애니메이션 프레임0 머리를 숨김 이전 compressed pose로 평가하고 MeshTransform으로 body 좌표에 옮긴 위치+(12,0,8)cm다. 웅크린 원본 자세와 무관한 기존 고정 높이를 사용하지 않는다. 머리 회전과 함께 eye lever도 회전한다. 매 시퀀스 프레임에서 원본 head quaternion delta와 eye lever를 포함한 offset을 별도 계산하여 실제 적용값의 오차가0.05도/0.05cm 미만인지 확인한다. CameraLimitFrames==0도 성공 조건이다. NativeStart에서 원래 카메라 위치와 gain을 복구한 뒤 native 가드로 돌아간다.

종료 및 월드 정리에서 timer/delegate를 제거하고 원래 AnimBP 모드/class, head-camera tuning, can-be-damaged, 자신이 올린 입력 ignore stack, ControlRotation, fixed timestep을 복구한다. 정상 종료에서는 preview target의 transform/이동 mode/컨트롤러도 복구한다. 설정 저장이나 공격 바인딩은 추가하지 않았다.

## 확인 범위

상태 복구와 클립 선택·단일 노드 재생·머리 소비 경로의 소스 계약은 통과했다. 실제 프리뷰 성공 마커, CameraLimitFrames=0, 렌더된 시점 변화와 메시 관통 여부는 통합 실행/시각 검증 결과를 결합해야 한다. 원본 자세에서 native 가드로 복귀하는 연결은 의도적으로 unblended이며 촬영 라벨에 명시한다. 원본 동작 전체를 게임 공격 입력에 연결했다고 설명하지 않는다.


## 검토 파일 고정 해시

- Runtime test SHA256: `52D3660ACBD0F98B32EDE9DABEDFBDE669052477D9AB51BB484709FA34EB0B3B`
- Import/retarget SHA256: `4068C871B99A95100705040E24CC82ECC483F6AB7CA2DEC6ABA3824BE9EB4A35`
- Source-target validator SHA256: `AEC72680D8E13D01C3D3DCCFE7E9B89946010DFCCB92A936CA785829DAE63F6F`

## 중간 -25도 아래보기 촬영 검토

중간 버전 `-RogueBoxingLookDown`은 fixture의 ControlRotation pitch만 -25도로 설정했다. 원본 AnimSequence와 포즈 평가 시각, 손/발/골반 변환 및 머리 추종 계수는 바꾸지 않는다. 기존 ControlRotation 복구 경로가 유지된다. 표적은 pitch를 제거한 yaw 전방240cm에 배치하므로 아래보기 때문에 땅으로 내려가지 않는다. 스크린샷 폴더를 `BoxingDirectPreviewLookDown`으로 분리하고 `view=look_down_25`를 기록해 정면 자료와 구분한다. 새 include는 CommandLine/Parse이며 프로덕션 입력 바인딩이나 런타임 컴포넌트 변경이 없다. 추가 변경 정적 검토 PASS.

실제 로그를 직접 확인했다. 당시 빌드는 Succeeded,7.91초였다(현재 같은 경로의 최종 빌드 로그는 아래 결과로 갱신됨). 정면 `preview.log`와 추가 `lookdown.log` 모두 BOXING_DIRECT_PREVIEW_PASSED,278프레임,실패0이다. 두 시점의 머리 이동67.494cm/yaw13.954도/pitch6.890도/roll4.539도, 제한 도달0프레임, raw offset오차0.000000cm/raw rotation오차0.000002도와 손·발 이동 수치가 같다. 렌더된 최대 회전은 시선 기준 차이로 정면13.954도/아래보기15.803도다. 아래보기의 화면 가독성과 관절 품질은 시각 검증2에서 별도 판단한다.

## 최종 -40도·UI 제외 보조 촬영

검증2에서 중간 -25도 화면의 HUD 가림이 확인되어 최종 보조 옵션은 ControlRotation pitch -40도로 바뀌었다. `FScreenshotRequest::RequestScreenshot`의 showUI 인자만 보조 촬영에서 false로 주어 HUD를 제외하며, 정면 촬영은 그대로 UI를 포함한다. 로그 라벨은 `view=look_down_40_no_ui`다. 새 변경은 시선 각도·캡처 UI·라벨뿐이며 원본 포즈/손 위치/골반/타이밍/머리 추종 계수와 표적의 yaw 전방240cm는 유지된다. 소스 검토 PASS. 정면 자료는 최초 재생 결과를 보존한다.

최신 `build-final.log`를 직접 확인했고 Succeeded,7.86초다. `lookdown40.log`는 BOXING_DIRECT_PREVIEW_PASSED,278프레임,실패0을 기록한다. 원본 머리 추종 offset67.494cm/yaw13.954도/pitch6.890도/roll4.539도, 제한0프레임, raw offset오차0.000000cm/raw rotation오차0.000002도와 손·발 이동 수치는 앞선 정면/25도와 같다. 최종 렌더된 최대 회전은17.503도다. 최종 무UI40도 화면의 가독성은 검증2의 별도 결과를 따른다.
