# 아린 v3 — 사용자 FBX·이미지 기반 개선

**완료: Blender 편집 원본·Unreal용 FBX 제작과 기존 시작마을 튜토리얼 NPC 적용·검증을 마쳤다.** 사용자 제공 FBX의 실제 얼굴·물결 머리·옆땋기·후드·자수 의상 메시와 2K Color/UV를 기반으로 형상을 국소 수리하고 재질·리깅을 개선했다. 최종 파일은 2026-10-09 확정한 source/export SHA와 독립 재수입·실제 렌더·Unreal 저장·PIE 증거에 연결된다. 정지형 안내 NPC와 간단한 Idle 범위의 완료이며 보행·전투·표정 제작 완료를 의미하지 않는다.

작업 브랜치는 `Sprint#5-18-arin-reference-refinement`다. [설계 및 Ultrawork Packets](D:/Project/Rogue10m/Feature/architect/2026-10-09_arin-reference-refinement.md)에 입력 조사, 형상 수리, 표면, rig/export, 엔진 적용, 최종 검토의 범위와 롤백 경계를 기록했다. C++ 변경 없이 실제 에셋과 기존 NPC 런타임을 검증했다. 커밋·푸시는 하지 않았다.

## 결과와 입력

사용자가 제공한 [fantasy adventurer 3d model.fbx](<C:/Users/PC/Downloads/fantasy adventurer 3d model.fbx>)와 첨부 T-pose 이미지에 맞춰 캐릭터를 수정했다. 실제 입력은 단일 **90,186삼각형**, 임베드 **2048×2048 Color atlas**, 원래 UV, rig·weight·action 없음이었다. 입력 SHA256은 `9ec4b4fb034071c8b6c46d015991c636be4f11544f03f913d4697eed0b5ab6b9`다. [입력 조사](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/input-fbx/inventory.json)와 [실제 입력 비교 시트](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/input-fbx/canonical-views/contact-sheet.png)에 근거가 있다.

도너의 얼굴·헤어·의상 형태와 Color UV를 보존하면서 작은 정수리·헤어 틈, 일부 국소 구멍과 제한된 겹침 조각을 수리했다. 의상 밑단·겹친 천의 열린 경계를 일괄 봉합하지 않았다. 전체가 닫힌 manifold라는 주장은 하지 않는다. 정면 이미지에 없는 뒤쪽 형상은 실제 FBX를 기준으로 삼았다.

최종 원형은 **90,214삼각형, 45,205정점, 런타임 메시 1개, 재질 1개, 제작 본 23개, 최대 bone influence 3개**다. GLB의 정점은 UV/normal 경계 분할 후 **110,855개**이므로 원형 정점 수와 구분한다. 미터, Z up, front -Y, 발바닥 Z=0, 신장 **1.70m**를 사용하고 선형 skinning으로 바인딩했다. 기존 의미 이름을 유지한 rig를 도너 관절 위치에 맞췄다. Idle은 **30fps, 1~61프레임의 2초 stationary loop**이며 unweighted/unnormalized 정점, zero-area triangle과 loop endpoint displacement는 각각 0으로 검사됐다.

재질은 원래 2K 색상 atlas와 UV를 유지하면서 피부·천·가죽·금속 반응과 팔레트를 조정했다. 단일 `Adventurer` **4096×4096** atlas의 BaseColor·Roughness·Metallic·Normal 네 PNG를 실제 Blender shader/tangent micro-normal bake로 제작했다. 정면 참조 이미지를 얼굴이나 몸에 투영하지 않았다. **4K bake가 원래 2K Color에 없는 얼굴·자수 정보를 복원한 것은 아니다.**

## 편집·전달 파일

| 파일 | 용도 | 최종 SHA256 시작값 |
|---|---|---|
| [Arin_SourceShader.blend](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Arin_SourceShader.blend) | bake 이전 편집 메시·원래 UV·원본 Color와 shader·전용 rig·Idle 작업 원본 | `4dd540e180b4` |
| [Arin.blend](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Arin.blend) | bake된 단일 atlas, rig·Idle·스튜디오를 포함한 최종 Blender 소스 | `56cc68fda9a3` |
| [Arin.fbx](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Arin.fbx) | 메시·rig·Idle 결합 FBX, Unreal 적용 기준 | `45296f9fcbd0` |
| [SK_Arin.fbx](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/SK_Arin.fbx) | 메시·rig FBX, animation 제외 | `90a1976d04a3` |
| [A_Arin_Idle.fbx](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/A_Arin_Idle.fbx) | 같은 rig의 Idle FBX, 메시 제외 | `54f9efc2d457` |
| [Arin.glb](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Arin.glb) | 재질·rig·Idle 비교와 독립 재수입용 GLB | `6280e8499385` |
| [Textures](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/Textures) | `T_Arin_Adventurer_{BaseColor,Roughness,Metallic,Normal}.png` 네 장 | surface manifest 참조 |

전체 파일 bytes/SHA256와 제작 스크립트 SHA는 [model-build.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/model-build.json), PNG는 [surface-manifest.json](D:/Project/Rogue10m/Art/NPC/Arin/ReferenceRefined/surface-manifest.json)에서 확인한다. 최종 FBX SHA256은 `45296f9fcbd0d182406c85d29f8627409e090dc0c97ec97815a8035221448f00`다. [native-blender-final.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/native-blender-final.json)은 실제 Blender에 최종 `56cc68...` 소스를 연 상태를 확인한다.

## 실제 형상·동작 검토

첫 두 Idle 후보의 실제 렌더에서 어깨·팔·망토 부위 판정 때문에 spike와 소매·팔보호대 잔류가 발견됐다. 반복을 멈추고 설계·독립 진단으로 돌아가 어깨의 연속적인 보간과 전체 팔 단면 판정으로 수정했다. 첫 export는 수치 검사 뒤 실제 무릎 자세에서 앞치마 찢김과 반대편 coat shard가 발견되어 weight를 다시 수정했다. [preview-attempt1](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/preview-attempt1), [preview-attempt2](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/preview-attempt2), [first-build](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/first-build), [second-build](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/second-build)는 역사 기록으로 보존하며 최종 합격 근거로 사용하지 않는다.

네 번째 최종 후보는 목의 색상 threshold 대신 xyz 공간 범위와 부드러운 fade를 사용해 이전 목 전체의 큰 밝고 어두운 삼각형 모자이크를 줄였다. [독립 네 번째 후보 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/critic/fourth-candidate-review.json)는 실제 source 6뷰와 최종 FBX 새 재수입 8뷰에서 얼굴·목 개선, 주요 참조 형태 유지, 좌우 무릎의 큰 찢김 해소를 확인했다. 귀 뒤·턱 아래의 작은 낮은 대비 facet, 머리의 작은 어두운 crease와 원본의 넓은 밝은 하이라이트는 남은 근접 미술 한계로 기록했다.

세 번째 후보에서 이미 직접 검토한 팔 굽힘·reach·몸통 twist·clay 증거는 [semantic-equality.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/input-fbx/final-export-poses-v4/semantic-equality.json)의 정확한 canonical 비교로 최종본에 연결했다. raw 배열의 순서는 일부 다르지만 edge/face 연결과 winding, 대응 face corner의 UV·normal, positions·weights·bones·actions와 평가된 Idle 정점은 정확히 같다. canonical corner normal 차이는 **0**, nonzero component **0**이다. 재질 이미지·색은 이 비교에서 제외했으므로 최종 8뷰의 실제 재질 검토를 별도로 수행했다. binary 파일 전체가 동일하다고 주장하지 않는다.

[최종 실제 Idle GIF](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/input-fbx/final-export-poses-v4/idle-motion/Arin_Idle_2s.gif)는 최종 FBX에서 실제 13개 시점을 렌더한 뒤 중복 끝점을 제외한 **12프레임·2,000ms**다. [motion manifest](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/input-fbx/final-export-poses-v4/idle-motion/motion-manifest.json)가 최종 FBX SHA와 1~61프레임 입력을 기록한다. 정규화된 weight만으로 모든 관절 품질을 통과시킨 것이 아니라 실제 자세와 동작 증거를 함께 검토했다.

## Unreal 적용·저장·LOD

최종 `/Game/Rogue10m/Characters/Arin/RefinedV3`는 SK_Arin·SK_Arin_Skeleton·ArinIdle 3개, PBR parent 1개, Adventurer material instance 1개, Texture2D 4개로 **9개 저장 패키지**다. 기존 `SV_TutorialGuide_Arin`의 외형·Idle을 적용했고 대화, 포션 2개 지급, 1회 수령 기록, 만석 재시도, 상호작용 범위와 actor/component transform을 보존했다. [최종 적용](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-refined-application.json)에 source hash와 보호 설정이 있다.

최초 texture import 뒤 Roughness·Metallic이 32px proxy로 읽혀 중단됐다. 원래 중단 기록을 보존하고 정확한 **7개 기존 에셋을 재사용**, pending 작업 완료와 일괄 property/PostEdit 재적용 후 **실제 4096×4096** 확인을 통과한 다음 새 material 2개를 만들었다. reimport·덮어쓰기·삭제 없이 9개 의존성을 저장하고 먼저 guide unchanged로 준비를 종료한 뒤 최종 적용했다. [안전한 복구·준비 결과](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-reference-prepared.json)가 이 경계를 확인한다.

엔진의 실제 imported bounds 높이는 **169.9999988009cm**, bone은 제작 23개와 `Arin_Rig` armature root를 합한 **24개**, Idle은 동일 전용 Skeleton의 **2초**다. 재질 slot 1개에 네 실제 4K texture가 연결된다. BaseColor는 sRGB/TC_DEFAULT, Roughness·Metallic은 linear/TC_MASKS, Normal은 linear/TC_NORMALMAP과 **Flip Green Channel=True**다. Blender normal은 OpenGL +Y tangent-space이며 mesh import는 **Import Normals + MikkTSpace + Compute Weighted Normals=False**다.

FBX에 LOD 체인은 포함되지 않는다. UE imported LOD0를 보존하고 LOD1/2를 **BaseLOD0, Lock Edges=True**, retained triangle target **0.85/0.65**로 생성했다.

| 엔진 LOD | 실제 정점 | material section | retained triangle target |
|---|---:|---:|---:|
| LOD0 | 110,688 | 1 | 1.00 |
| LOD1 | 100,176 | 1 | 0.85 |
| LOD2 | 81,219 | 1 | 0.65 |

정확한 UE runtime 삼각형 수는 해당 Python API가 노출하지 않아 null이다. 목표 비율이나 정점 수로 추정하지 않는다. 기존 detection sphere와 non-colliding guide mesh를 유지하고 새 PhysicsAsset은 만들지 않았다.

[LOD0](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-captures/refined/lod-0/manifest.json), [LOD1](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-captures/refined/lod-1/manifest.json), [LOD2](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-captures/refined/lod-2/manifest.json)의 front·face·oblique **총 9개 실제 엔진 화면**을 확인했다. 카메라 적용 이후 실제 editor tick과 forced/predicted LOD를 확인하고 camera·component·animation·background throttle과 소유 callback을 복구했다. [독립 엔진 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/critic/unreal-engine-review.json)는 참조에 가까워진 외형과 샘플 LOD 연속성을 승인했다. baseline v2는 automatic predicted LOD1, v3는 각각 forced LOD이므로 같은 카메라 비교를 동일 LOD 성능 비교라고 해석하지 않는다.

## 최종 검증·보존

| 검증 | 최종 결과·근거 |
|---|---|
| BLEND·FBX·GLB 독립 새 process 검사 | [BLEND](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/fresh-blend.json), [FBX](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/fresh-fbx.json), [GLB](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/fresh-glb.json) 모두 `passed_requires_visual_review`, 실제 최종 입력 SHA 일치. 별도 시각 검토를 함께 수행. |
| source·재수입 형상/재질·무릎 및 carryover 검토 | 네 번째 후보와 semantic equality에 연결된 범위 통과. 근접 얼굴·헤어 한계 기록. |
| 실제 UE 저장·전용 rig/PBR/3LOD | 9개 의존성 저장, 170cm·24본·4장 실제 4K·slot1 검증 통과. |
| GUI map unload/reload | [unreal-gui-map-reload.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-gui-map-reload.json): L_Menu→StartHub 재로드 뒤 validate-only 통과. |
| 별도 UnrealEditor-Cmd process | [unreal-fresh-process.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/unreal-fresh-process.json): 실제 PID 38236, NullRHI 저장 검증 통과·정상 종료. 렌더 검사와 구분. |
| 실제 NPC PIE 회귀 | [tutorial-runtime.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/tutorial-runtime.json): **67/67** 통과. 외형·실제 Idle, 대화·정확한 1회 보급·만석 재시도·포션 소비·실제 포털 이동 뒤 잔여 보급 유지 확인. 소유 PIE 종료. |
| 실제 3LOD 엔진 화면 | 9뷰·복구 결과 및 독립 엔진 검토 통과. |
| Content SHA256 보존 | [content-preservation.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-reference-refinement-20261009/content-preservation.json): 기존 **6,355개 중 6,353개 동일**, guide DataAsset/Blueprint **2개 변경**, 새 v3 **9개 추가**, 삭제·예상 밖 변경 **0**. |

가이드의 external actor package는 저장했지만 bytes가 기준선과 같으므로 변경 3개로 세지 않는다. 기존 RefinedV2와 다른 Content는 SHA 감사에서 보존됐다. 다운로드 원본과 v2 제작 폴더는 새 버전 작업 대상으로 사용하지 않고 보존한다. 최종 source/export/PNG·제작 script provenance와 실제 입력 SHA를 함께 확인해야 하며, 파일을 다시 수정하면 현재 검증을 그대로 승계하지 않는다.

## 사용 범위·재작업

목표는 시작마을의 정지형 튜토리얼 길잡이와 간단한 Idle이다. 원래 2K 얼굴/Color 정보, 눈·입의 텍스처 의존, 귀 뒤·목 옆의 작은 facet, 머리 crease와 baked highlight, stiff cloth/bag 변형이 남는다. 마을 조명에서는 얼굴이 studio보다 어두우므로 대화용 근접 조명은 별도 조정 여지가 있다. 첨부 이미지와 완전한 동일성이나 하이퍼리얼 얼굴을 주장하지 않는다.

얼굴 표정·립싱크·개별 손가락 rig, Manny/Quinn 리타깃 호환, 천·헤어 simulation, 보행·전투 animation, 모든 임의 자세의 교차 부재, 자동 LOD 경계의 연속 pop, 군중/GPU/메모리 성능 및 packaged build 성능은 검증하거나 구현하지 않았다. PIE 검사는 실제 reflected API를 사용했으며 물리 F키 입력·한글 UI 배치 전체·네트워크·영구 프로필 저장을 승인하지 않는다.

형상·weight·shader 변경은 [BuildReferenceArin.py](D:/Project/Rogue10m/Scripts/Blender/BuildReferenceArin.py)와 편집 원본에서 시작하고 bake/export·독립 재수입·실제 자세·Unreal 검증을 다시 수행한다. engine 작업은 [ApplyReferenceArin.py](D:/Project/Rogue10m/Scripts/Editor/ApplyReferenceArin.py), [RecoverReferenceArinPreparation.py](D:/Project/Rogue10m/Scripts/Editor/RecoverReferenceArinPreparation.py), [ValidateReferenceArinFresh.py](D:/Project/Rogue10m/Scripts/Editor/ValidateReferenceArinFresh.py), [ValidateReferenceArinPIE.py](D:/Project/Rogue10m/Scripts/Editor/ValidateReferenceArinPIE.py), [CaptureReferenceArin.py](D:/Project/Rogue10m/Scripts/CaptureReferenceArin.py), [ReviewReferenceArinLOD.py](D:/Project/Rogue10m/Scripts/Editor/ReviewReferenceArinLOD.py), [AuditReferenceArinContent.py](D:/Project/Rogue10m/Scripts/AuditReferenceArinContent.py)로 범위를 분리했다.

롤백은 [보존된 v2](D:/Project/Rogue10m/Art/NPC/Arin/Refined/README.md)의 전용 mesh·Skeleton·Idle 참조를 기존 guide에 복원하는 방식이다. 버전 사이의 Skeleton·material 참조를 섞지 않는다.
