# 길잡이 아린 — Reference Refined v3

**최종 제작·Unreal 적용·검증 완료.** 사용자가 제공한 실제 FBX와 T-pose 이미지를 기준으로 도너의 얼굴·물결 머리·옆땋기·후드·자수 의상 메시와 원래 2K Color/UV를 보존하면서 국소 표면 수리, 재질, rig·Idle을 개선했다. 시작마을의 기존 튜토리얼 길잡이에 적용했고 실제 PIE **67/67**, GUI 재로드, 별도 UE process 저장 검사와 세 LOD의 9개 실제 엔진 화면 검토를 통과했다. 적용 범위와 남은 한계는 [기능 문서](D:/Project/Rogue10m/Feature/doc/2026-10-09_arin-reference-refinement.md)에 있다.

![최종 아린 실제 Blender 렌더](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/candidate/idle-oblique.png)

| 파일 | 용도 | 최종 SHA256 시작값 |
|---|---|---|
| [Arin_SourceShader.blend](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Arin_SourceShader.blend) | **편집 작업 원본.** bake 이전 메시·UV·원본 Color·shader·전용 rig·Idle | `4dd540e180b4` |
| [Arin.blend](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Arin.blend) | bake된 단일 atlas, rig·Idle·스튜디오를 담은 최종 Blender 소스 | `56cc68fda9a3` |
| [Arin.fbx](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Arin.fbx) | 메시·rig·Idle 결합, **Unreal 적용 기준** | `45296f9fcbd0` |
| [SK_Arin.fbx](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/SK_Arin.fbx) | 메시·rig, animation 제외 | `90a1976d04a3` |
| [A_Arin_Idle.fbx](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/A_Arin_Idle.fbx) | 동일 rig의 Idle, 메시 제외 | `54f9efc2d457` |
| [Arin.glb](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Arin.glb) | 재질·rig·Idle 비교와 독립 재수입용 | `6280e8499385` |
| [Textures](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Textures) | Adventurer 4096×4096 BaseColor·Roughness·Metallic·Normal PNG 네 장 | surface manifest 참조 |

전체 bytes/SHA256·제작 script SHA는 [model-build.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/model-build.json), PNG 규약과 해시는 [surface-manifest.json](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/surface-manifest.json)에 있다. 최종 FBX 전체 SHA는 `45296f9fcbd0d182406c85d29f8627409e090dc0c97ec97815a8035221448f00`다. [native-blender-final.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/native-blender-final.json)은 실제 Blender에 최종 `56cc68...` 파일을 연 상태를 확인한다.

최종 원형은 **90,214삼각형·45,205정점·메시 1개·재질 1개·제작 본 23개·최대 influence 3개**다. GLB 정점은 UV/normal 경계 분할 후 110,855개다. 기준은 **미터, Z up, front -Y, 발바닥 Z=0, 신장 1.70m**이며 FBX는 -Z forward/Y up 축·단위 변환을 전달한다. Unreal imported bounds 높이는 **169.9999988009cm**, 본은 23개와 `Arin_Rig` root를 합한 **24개**다. Idle은 30fps/1~61프레임의 **2초 stationary loop**이며 mesh와 같은 전용 Skeleton을 사용한다.

![최종 FBX의 실제 2초 Idle](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/input-fbx/final-export-poses-v4/idle-motion/Arin_Idle_2s.gif)

위 GIF는 최종 FBX의 실제 13개 시점 렌더에서 중복 끝점을 제외한 **12프레임·2,000ms**다. [motion manifest](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/input-fbx/final-export-poses-v4/idle-motion/motion-manifest.json)가 최종 입력 SHA와 프레임을 기록한다.

네 PBR PNG는 원래 UV 위의 실제 Blender palette/roughness/metal 및 tangent micro-normal shader bake다. **색상 원본은 2048×2048이며 4096 출력이 없는 얼굴·자수 정보를 복원하지 않는다.** 첨부 정면 이미지를 얼굴이나 몸에 투영하지 않았다.

| 채널 | Unreal 설정 |
|---|---|
| `T_Arin_Adventurer_BaseColor.png` | sRGB=True, TC_DEFAULT, Color sampler |
| `T_Arin_Adventurer_Roughness.png` | sRGB=False, TC_MASKS, Masks sampler |
| `T_Arin_Adventurer_Metallic.png` | sRGB=False, TC_MASKS, Masks sampler |
| `T_Arin_Adventurer_Normal.png` | sRGB=False, TC_NORMALMAP, Normal sampler, **Flip Green Channel=True** |

Normal은 **OpenGL +Y tangent-space**다. mesh import는 **Import Normals + MikkTSpace + Compute Weighted Normals=False**로 bake와 일치시킨다. texture compilation 완료 뒤 네 실제 resource의 4096×4096 크기와 material 연결을 검증했다. 첫 Roughness/Metallic 32px proxy 중단은 기존 정확한 에셋 7개를 재사용해 property/PostEdit 재적용·완료·4K gate 뒤 새 재질 2개만 추가하는 방법으로 복구했다. reimport·덮어쓰기·삭제 없이 9개를 저장했다. [복구 결과](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-reference-prepared.json)에 근거가 있다.

Unreal 소유 경로는 `/Game/Rogue10m/Characters/Arin/RefinedV3`다. mesh·Skeleton·Idle 3개, parent와 material instance 2개, texture 4개로 정확히 **9개 패키지**다. 기존 guide의 외형·Idle을 적용하고 대화·포션 2개 지급·중복 방지·만석 재시도·상호작용 범위·transform을 보존했다. 기존 detection sphere와 non-colliding mesh를 사용하며 새 PhysicsAsset은 없다.

FBX에는 LOD 체인이 없다. 엔진에서 LOD0를 보존하고 LOD1/2를 BaseLOD0, Lock Edges=True, retained triangle target **0.85/0.65**로 생성했다.

| LOD | 실제 UE 정점 | material section | triangle target |
|---|---:|---:|---:|
| 0 | 110,688 | 1 | 1.00 |
| 1 | 100,176 | 1 | 0.85 |
| 2 | 81,219 | 1 | 0.65 |

정확한 runtime 삼각형 수는 API가 노출하지 않아 null이다. 목표 비율이나 정점 수로 추정하지 않는다. [엔진 독립 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/critic/unreal-engine-review.json)는 실제 세 LOD의 front·face·oblique 9뷰와 v2 baseline을 열어 외형 개선·샘플 LOD 연속성을 승인했다. 각 실제 forced/predicted LOD와 카메라 적용 이후 editor tick을 확인했고 임시 camera·component·animation·throttle·callback을 복구했다. baseline은 자동 LOD1이므로 이를 동일 LOD 성능 비교라고 읽지 않는다.

[BLEND](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/fresh-blend.json), [FBX](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/fresh-fbx.json), [GLB](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/fresh-glb.json) 독립 검사 모두 실제 최종 SHA로 `passed_requires_visual_review`를 통과했다. 별도 [네 번째 후보 시각 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/critic/fourth-candidate-review.json)는 source 6뷰와 최종 FBX 8뷰에서 넓은 목 색 모자이크 제거와 좌우 무릎의 큰 찢김 해소를 확인했다. 이전 팔·clay 검토의 승계는 [canonical semantic equality](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/input-fbx/final-export-poses-v4/semantic-equality.json)의 정확한 topology/UV/normal/weight/bone/action 비교에 한정한다. raw 배열 순서와 binary bytes 전체의 동일성을 주장하지 않는다. 실패한 초기 build의 수치 합격은 최종 결과에 승계하지 않았다.

[최종 적용](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-refined-application.json), [GUI L_Menu→StartHub reload](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-gui-map-reload.json), [별도 UnrealEditor-Cmd](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-fresh-process.json)의 저장 검사와 [실제 PIE 67/67](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/tutorial-runtime.json)이 통과했다. Cmd는 실제 PID38236의 NullRHI persistence 검사이며 렌더 검사와 구분한다. 소유 PIE는 종료됐다.

[Content SHA 감사](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/content-preservation.json)는 기존 **6,355개 중 6,353개 동일**, guide DataAsset/Blueprint **2개 변경**, v3 **9개 추가**, 삭제·예상 밖 변경 **0**을 확인했다. external actor package는 저장했지만 bytes가 같아 변경으로 세지 않는다. 기존 RefinedV2와 다른 Content는 보존됐다. 다운로드 원본과 [v2 제작 폴더](D:/Project/Rogue10m/Art/NPC/Arin/Refined/README.md)를 새 버전 작업 대상으로 편집하지 않았다.

얼굴·눈·입은 원래 2K Color에 크게 의존하며, 귀 뒤·목 옆의 작은 낮은 대비 facet, 머리의 작은 crease·넓은 baked highlight, 일부 stiff cloth/bag 변형은 남는다. 마을 조명에서 얼굴은 studio보다 어둡다. 첨부 이미지와 완전한 동일성이나 하이퍼리얼 얼굴을 주장하지 않는다.

얼굴 표정·립싱크·개별 손가락 rig, Manny/Quinn 호환, 천·헤어 simulation, 보행·전투 animation, 모든 임의 자세의 관통 부재, 자동 LOD 경계 pop과 군중/GPU/메모리/packaged build 성능은 구현하거나 검증하지 않았다. PIE 검사는 실제 reflected API를 사용했고 물리 F키 입력·한글 UI 전체 배치·네트워크·영구 프로필 저장을 승인하지 않는다.

재작업은 [BuildReferenceArin.py](D:/Project/Rogue10m/Scripts/Blender/BuildReferenceArin.py)와 편집 원본에서 시작한다. geometry·shader·weight를 변경하면 bake/export·독립 재수입·실제 자세·Unreal 검증을 다시 수행해야 한다. 롤백에는 보존된 v2의 전용 mesh·Skeleton·Idle·material 참조를 함께 사용하며 버전 사이의 참조를 섞지 않는다.
