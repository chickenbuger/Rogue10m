# 아린 v3 Unreal 적용 계약 검토

검토 범위는 기존 v2 스크립트의 읽기 검토와 승인된 v3 스크립트 네 파일 작성이다. Blender 및 Unreal 실행, 기존 v2 파일 수정, 에셋 저장은 하지 않았다.

## 새 파일

- `Scripts/Editor/ApplyReferenceArin.py`: `Art/NPC/Arin/ReferenceRefined/Arin.fbx`와 단일 `Adventurer` 4096 atlas를 `/Game/Rogue10m/Characters/Arin/RefinedV3`에 적용한다. 기존 튜토리얼 가이드의 외형과 Idle만 바꾼다.
- `Scripts/Editor/ValidateReferenceArinFresh.py`: 별도 UnrealEditor-Cmd 프로세스에서 저장된 StartHub와 v3 의존성을 읽기 검증한다. NullRHI 검증이며 렌더링 검증으로 보고하지 않는다.
- `Scripts/Editor/ValidateReferenceArinPIE.py`: 기존 `ValidateTutorialNPC.py`의 실제 PIE 대화·아이템 지급·소비·포털 이동 검증을 재사용하고 v3 mesh, Idle, 정확한 rig, 재질, LOD를 추가 검증한다.
- `Scripts/AuditReferenceArinContent.py`: 새 Content SHA256 기준선과 비교해 새 v3 9개 패키지, 기존 가이드 DataAsset/Blueprint/가이드 소유 actor package 외 변경 및 모든 삭제를 실패로 처리한다.

## 소스 계약

`surface-manifest.json`은 기존 v2 구조를 유지하되 groups에는 `Adventurer` 하나만 있어야 한다. material은 `M_Arin_Adventurer`, resolution은 `4096` 또는 `[4096,4096]`, channels는 BaseColor/Roughness/Metallic/Normal이며 파일은 `T_Arin_Adventurer_{channel}.png`이다. files에 네 PNG의 실제 bytes/sha256이 있어야 한다. `image_projection=false`는 제공 이미지의 정면 투영을 하지 않았다는 의미이며 제공 FBX의 원래 UV 색상 atlas 재사용을 금지하지 않는다. 그 재사용 방식은 method와 모델 문서에 구체적으로 기록한다.

`Feature/doc/evidence/arin-reference-refinement-20261009/model-build.json`은 다음 필드를 반드시 기록한다.

- `bone_names`: FBX armature object인 `Arin_Rig`를 제외한 정확한 authored bone 명단. 기본 humanoid 23개와 실제 추가한 braid bone 명단을 모두 포함한다.
- `bones`: 명단의 길이. 중복 이름 및 필수 humanoid bone 누락은 실패한다.
- `head_joint_z_m`: canonical REST rig에서 측정한 head bone joint의 Z(m). 현재 모델이 170cm라는 계약에서 1.30~1.65m 범위를 요구한다. PIE에서는 component world origin 기준 head socket 높이가 해당 값±3cm여야 한다.
- `files["Arin.fbx"]`: 실제 FBX `bytes`와 `sha256`. 검증 시 파일을 다시 해시해 정확히 대조한다.

Unreal bone 목록은 authored 목록과 정확히 같거나 그 목록에 FBX armature root `Arin_Rig` 한 개만 추가되어야 한다. 24/27개라는 숫자만 맞춰 통과시키지 않는다.

## 유지한 중요한 조건

총 패키지는 core SK_Arin/SK_Arin_Skeleton/ArinIdle 3개, M_Arin_PBR 1개, MI_Arin_Adventurer 1개, Texture2D 4개로 정확히 9개다. 폴더에 부분 import나 출처가 다른 파일이 있으면 자동 삭제·덮어쓰기 없이 checkpoint를 남기고 실패한다.

모델 imported REST bounds 높이 170±1cm, Idle 2초, mesh와 Idle의 동일 Skeleton, stationary root motion 비활성화, imported normals + MikkTSpace + weighted normals 비활성화 조건을 유지한다. BaseColor는 sRGB/TC_DEFAULT, Roughness·Metallic은 linear/TC_MASKS, Blender +Y tangent Normal은 linear/TC_NORMALMAP 및 green flip이다. Texture2D 설정은 한 번의 set_editor_properties로 적용하고 `Editor.AsyncTextureCompilationFinishAll` 뒤 실제 크기를 검증한다.

LOD0 원본은 재생성하지 않으며 LOD1/2 목표는 0.85/0.65, BaseLOD0, 경계 고정, volume importance 1, 최대 bone influence 4다. native-backed source_models 배열의 nested 값 수정은 해당 index에 다시 쓰고 owning mesh에 알린다. 각 LOD는 실제 vertex 수가 감소하고 Adventurer section 하나만 있어야 한다. 목표 비율이나 vertex 수를 실제 runtime triangle 수로 보고하지 않는다.

가이드 대화, reward ID, 아이템·수량, interaction/detection radius, actor class/tag/location/rotation/scale, component transform 및 actor package 보호 조건은 v2와 같다. 새 physics asset을 만들지 않고 기존 detection sphere와 non-colliding guide mesh 구성을 유지한다.

## 렌더/움직임 증거에 필요한 별도 적응

기존 `Scripts/Blender/RenderArinQuality.py`는 bone 이름을 재사용하므로 기본 humanoid rig 명칭을 지키면 QA 회전은 재사용할 수 있다. 다만 face target Z=1.555, hand target=(0.29,-0.02,0.70) 및 68도 upperarm relaxed 자세는 v2 치수와 자세에 맞춘 값이다. 새 donor 관절 위치와 실제 손 위치를 확인해 동일 비교 프레임을 별도 v3 wrapper에서 고정해야 한다. 모델 생성 스크립트와 별도 process에서 FBX를 다시 읽어 front/back/profile/face/hands/boots, arm bend/reach/torso twist/knee bend 및 실제 2초 Idle을 렌더해야 한다. 수치상 normalized weights만으로 관절·망토·braid deformation을 통과시켜서는 안 된다.

`Scripts/CaptureArinQuality.py`와 `Scripts/Editor/ReviewRefinedArinLOD.py` 재사용 시 evidence 경로와 version import를 v3 소유 경로로 바꾸고 기존 v2 증거를 덮어쓰지 않는다. 실제 forced LOD와 predicted LOD가 일치한 settled 화면을 확인한 뒤 capture한다. 마지막에는 camera, forced LOD, editor animation, background throttle을 복구한다.

## 확인한 결과와 제한

새 네 파일의 AST 문법 검증이 통과했다. 기존 v2 전용 경로, 4개 재질 slot, 16 PNG, exact24 bones, saved24개 조건이 새 파일에 남지 않았음을 검색과 diff로 확인했다. PIE world origin API는 로컬 UE5.8 `Engine/Source/Runtime/Engine/Classes/Components/SceneComponent.h:661`의 `ScriptName="GetWorldLocation"`을 확인하여 `component.get_world_location()`을 사용한다.

이 결과는 정적 검토다. root가 실제 확정된 소스/manifest를 검토한 뒤 Unreal 적용, 새 editor 프로세스 검증, 실제 PIE 및 화면 검증을 실행해야 한다. 현재 시점에 v3 Unreal 적용 성공 또는 시각 품질 통과를 주장하지 않는다.
