# 독립 소스 검토 — V키 3인칭 확인

작성: 2026-09-28 / 역할: 검증1
결론: 현재 요청 범위의 로컬 플레이 경로에서 차단 결함을 발견하지 못했다. 빌드·실제 입력·가시성·벽 충돌·런타임 검증은 별도 결과로 확정한다.

## 검토 범위

- Source/Rogue10m/Components/Rogue10mAppearanceCameraComponent.h
- Source/Rogue10m/Components/Rogue10mAppearanceCameraComponent.cpp
- Source/Rogue10m/Core/Rogue10mPlayerController.h
- Source/Rogue10m/Core/Rogue10mPlayerController.cpp
- Source/Rogue10m/Character/Rogue10mCharacter.cpp

신규 Inspection 상태/API/가시성/충돌, V 입력 바인딩과 차단 조건, CalcCamera 통합을 검토했다. 런타임 코드는 직접 수정하지 않았다.

## 확인 내용

### 입력과 상태 수명

V Pressed 바인딩은 로컬 gameplay world에만 설치된다. handler는 메뉴, 차단 UI, mouse cursor, look/move ignore, pause, 죽은 Pawn, 다른 viewtarget 상태를 거부한다. Appearance camera 활성 여부도 확인한다. 컴포넌트 초기 상태는 false이고 Toggle은 Refresh 후 유효한 활성 상태에서만 상태를 뒤집는다. Restore/EndPlay는 inspection 상태를 false로 지우고 파츠 상태를 복원한다.

### 카메라와 공격 기준

Character::CalcCamera는 기존처럼 head camera component를 먼저 갱신하고 Super에서 카메라 결과를 얻는다. 이후 ApplyInspectionView가 최종 OutPOV의 위치·회전만 덮는다. 관찰 위치를 실제 카메라 component에 쓰거나 ControlRotation, actor rotation, 몽타주 상태를 변경하지 않는다. 따라서 기존 CombatComponent가 쓰는 실제 head camera 위치/forward는 전환에 의해 옮겨지지 않는다.

관찰 pivot은 capsule 위치 + world-up 높이이며, ControlRotation 방향의 뒤쪽에 카메라를 배치한다. OutPOV rotation도 ControlRotation이므로 장애물이 없을 때 관찰 중심을 바라본다. 최종 기본 값은 거리 300cm, pivot 높이 0cm(캡슐 중심), sphere 반지름 12cm이다. 설정값의 유한성 확인과 범위 clamp가 있다.

### 벽 충돌

Pivot에서 Desired까지 ECC_Camera sphere sweep을 수행하고 owner Character를 무시한다. 충돌 시 hit sphere 중심으로 위치를 당기며, 시작점이 이미 penetration 상태면 Pivot으로 되돌린다. 따라서 일반적인 뒤쪽 벽에 대한 카메라 관통을 줄이는 경로는 타당하다. 시작 pivot 자체가 geometry 안에 있는 특수 상태에서 완전한 penetration 탈출까지 보장하는 구현은 아니며, 그런 상황을 실행 검증했다고 쓰면 안 된다.

### 머리와 외형 파츠

1인칭에서 본 컴포넌트가 숨겼던 head만 inspection 진입 시 UnHide한다. 원래부터 외부에서 숨긴 head를 무조건 드러내지 않는다. 복귀 시 기존 bHideHeadForOwner 정책을 다시 적용한다.

직접 부착된 skeletal cosmetic 파츠의 기존 OwnerNoSee를 최초 한 번 저장하고 inspection 동안 false로 둔다. FirstPersonMesh는 명시적으로 제외한다. 복귀와 Restore는 기존 bool 값을 복원하고 map을 비운다. Visibility/HiddenInGame을 임의로 켜지 않으므로 원래 없는/숨긴 머리카락 파츠를 새로 나타내지 않는다. 현재 Hair/Facial direct-child 구조와 맞는다.

FirstPersonMesh는 전환 중에도 hidden, pause, tick off를 유지한다. Appearance/Animation Source의 asset, AnimClass, montage position을 바꾸는 코드가 없다.

### Unreal 및 메모리

Inspector tuning은 EditDefaultsOnly/BlueprintReadOnly로 노출한다. 신규 non-owning 파츠 캐시는 TWeakObjectPtr로 보관하고 유효한 파츠만 복원한다. 신규 Tick을 추가하지 않았고 기존 camera evaluation 경로를 쓴다. 입력 핸들러는 UFUNCTION 선언과 일치한다. generated.h는 헤더 include 마지막이다.

## 실행에서 확정할 항목

1. 실제 V 입력으로 두 번 토글되고 차단 UI에서는 입력이 무시되는지.
2. 3인칭에서 머리·머리카락·얼굴·양손·양발이 실제 보이며, 복귀 시 1인칭 가림이 돌아오는지.
3. 공격 중 전환해도 몽타주 재생 시간이 초기화되지 않고, head camera transform과 공격 기준이 유지되는지.
4. 뒤쪽 벽에 가까워질 때 거리 축소와 벽을 벗어날 때 복원이 되는지.
5. 마우스 시선·무기 전환·이동 중 전환과 FP 비활성 상태가 유지되는지.

3인칭은 현재 동작 확인용이다. 화면 중앙과 head camera를 기준으로 하는 공격 경로에는 시차가 있을 수 있으며, 이 기능을 새로운 3인칭 조준 시스템으로 설명하면 안 된다. 네트워크·split-screen·외형 파츠 동적 재부착은 이번 소스 검토의 실행 증거 범위가 아니다.
## 최종 조정 재검토

첫 촬영에서 하체가 하단 HUD에 가려져 관찰 pivot의 높이만 +60cm에서 0cm로 낮췄다. 헤더 기본값과 비유한 값 fallback이 모두 0cm로 일치함을 확인했다. 거리·벽 스윕·가시성 복원·실제 head camera 및 공격 기준은 바꾸지 않은 범위다. 최종 `build-final.log`는 25.06초, Result: Succeeded를 확인했다.

런타임 fixture의 몽타주 종료 직후 recovery 구간은 current montage가 null일 수 있으므로, 실제 montage가 존재할 때 원본 클립을 검사하고 충분한 재생 샘플(최소 3개)을 요구하도록 수정했다는 root 보고를 받았다. 이는 테스트 조건 조정이며 실제 공격 런타임 변경이 아니다. 최종 실행 통과 여부와 시각 결과는 root의 결과 문서에서 별도로 확정한다.