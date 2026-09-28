# 독립 소스 검토 — Appearance head camera

2026-09-26 / 검증1 / 검토 결과: 요청한 로컬 단일 플레이어 경로에서 현재 소스의 차단 결함을 발견하지 못함. 빌드·애셋 저장·실제 재생·시각 검증은 별도 실행 결과로 확정해야 한다.

## 범위

`tmp/appearance-head/before`의 이번 작업 직전 복사본을 기준으로 Character.cpp, CombatComponent.cpp, StylizedCharacter.cpp와 현재 코드를 비교했다. 신규 AppearanceCameraComponent.h/.cpp, Presentation/CameraManager 분기, Character.h 및 에디터용 AppearanceBoxingAuthoring.cpp를 읽었다. 기존 전투/머리 카메라 작업 전체를 새 변경으로 취급하지 않았다. 런타임 코드는 직접 수정하지 않았다.

## 확인 내용

- Appearance camera는 Character.GetMesh()를 표시와 카메라의 공통 기준으로 쓴다. FirstPersonMesh는 hidden, paused, tick disabled이며 PlayCommonMontage가 활성 Appearance 모드에서 FP 복제 재생을 생략한다. Combat effect는 실제 Appearance로 부착한다.
- Source ABP/Appearance Retarget ABP를 교체하는 코드가 새 카메라에 없다. StylizedCharacter.cpp는 기준 복사본과 동일하다. Character.GetAnimationPlaybackMesh의 기존 GetMesh fallback 및 Stylized의 숨긴 AnimationSource override가 유지된다.
- 카메라를 Appearance head에 attach하되 absolute location/rotation/scale로 만들어 숨긴 head socket의 scale 0을 받지 않는다. 실제 눈 위치는 최종 Appearance의 evaluated local bone chain으로 재구성한다. pelvis/spine/팔다리는 숨기지 않고 head만 숨긴다.
- head reference rotation과 현재 rotation의 delta를 mesh world basis로 변환한 뒤 ControlRotation에 합성한다. 이는 actor yaw를 단순 두 번 더하는 방식이 아니다. 눈 offset도 동일 animated head 방향에 따라 회전한다.
- 공격 hit pulse 직전에 같은 카메라 component를 갱신하고 그 위치/forward를 trace에 사용한다. 별도의 POV-only 머리 additive는 Appearance 활성 시 CameraManager에서 건너뛴다.
- 사망·unpossess·다른 viewtarget·외형 mesh 변경 시 restore/reference recache 경로가 있다. 원래 camera parent/socket/transform/absolute flags, body flags, FP visible/pause/tick을 보관·복원한다.
- UObject owning component는 UPROPERTY TObjectPtr, nonowning caches는 TWeakObjectPtr이다. 새 component는 Tick을 켜지 않고 CalcCamera/공격/갱신 호출을 사용한다.

## 엔진 소스로 확인한 본 가림과 프레임 순서

로컬 설치 UE 5.8 소스를 직접 확인했다.

- `Engine/Private/Components/SkeletalMeshComponent.cpp:2321`: ExcludeHiddenBones는 AlwaysTickPoseAndRefreshBones일 때 hidden bone도 RequiredBones에 유지한다.
- 같은 파일 `:5711`: GetBoneSpaceTransformsView는 진행 중 parallel animation evaluation을 완료시킨 뒤 BoneSpaceTransforms를 반환한다. visibility 축소가 적용된 component-space socket을 읽는 방식과 구분된다.
- 같은 파일 `:441`, `:2086`: 기본 skeletal tick은 TG_PrePhysics이며 늦은 animation evaluation 종료도 TG_PostPhysics이다.
- `Engine/Private/LevelTick.cpp:1832`, `:1847`: camera manager update는 actor/physics와 TimerManager 이후, TG_PostUpdateWork 전에 실행된다.
- 같은 파일 `:1906`: OnWorldPostActorTick은 CameraManager update 이후에 broadcast된다.

따라서 기본 source → retarget Appearance → timer hit camera → CameraManager CalcCamera 경로는 같은 프레임 평가 pose를 읽는다. PostActorTick fixture에서 강제로 CalcCamera한 결과만으로 실제 camera cache를 입증할 수 없으므로, 추가 검증에서는 fixture가 control/teleport를 바꾸기 전 PCM cache와 actual camera를 비교하는 것을 root에 권고했다.

## Boxing 제작 소스 검토

- 원본 A_BoxingJab_Manny를 새 왼쪽/오른쪽 sequence로 120fps 평가·bake한다. 오른쪽은 본 좌우 교환과 MirrorPose(EAxis::X)로 만든 파생이다.
- source 최대 신장 13/30초를 스킬의 HitStartDelaySeconds에 맞추고 나머지 source 구간을 montage length에 시간 재매핑한다. 팔·손·발 위치의 별도 시야용 보정을 추가하지 않는다.
- sequence 전체 transform/rotation의 유효성, Skeleton 일치, montage 단일 segment, 정수 프레임 길이 및 저장 전 최종 play length를 검사한다.
- hit runtime delay와 montage play rate가 모두 Skill.AnimationPlayRate × attack speed 기준을 쓰므로 시각과 판정의 시간 기준은 일치한다.
- 이 C++ 명령은 패키지를 dirty로 만들 뿐 자체 SavePackage는 하지 않는다. 실제 제작 실행·저장·DA 연결은 에디터 스크립트 및 저장 후 로드 증거로 별도 확인해야 한다.

## 실행 검증에서 반드시 확정할 사항

1. Fresh spawn pitch -30도. 기존 once 함수는 모드가 아직 활성화되지 않았을 때도 handled를 소비하므로 초기 possession/viewtarget 순서의 실제 결과를 확인해야 한다.
2. 실제 게임 CameraManager cache가 Appearance head pose와 맞는지 확인. fixture의 직접 CalcCamera만으로 대체하지 않는다.
3. 머리 안쪽·목 단면·가슴 시야 가림과 손/발 표시, raw motion 흔들림은 실제 영상으로 판단한다.
4. 좌클릭 2타의 실제 montage 경로와 damage timing, 우클릭 짧게/홀드 후 release, Idle 복귀, 무기 전환, mouse look, 걷기·점프를 확인한다.
5. 이번 리뷰는 로컬 단일 플레이어를 대상으로 한다. split-screen owner별 head hide, network remote aim 동기화는 검증하지 않았다.

## 잔존 FirstPersonMesh 판단

이번 표시에는 필요하지 않다. inherited Blueprint component 직렬화 및 다른 기존 API와의 호환을 위해 정의만 남겨 두는 것은 단계적 이전의 합리적 선택이다. 별도로 숨긴 AnimationSourceMesh는 현재 외형 Skeleton 리타깃의 입력으로 필요하며, 이 두 메시의 존재 이유를 사용자 설명에서 혼동하지 않아야 한다.