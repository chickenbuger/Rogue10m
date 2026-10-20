# 길잡이 아린 — Refined v2

최종 전달 파일·16PNG·제작 소스·UE 저장 의존성의 해시와 검증 입력 동일성은 [final-handoff.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-handoff.json)에서 확인한다. [실제 모델 검토 시트](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/arin-quality-sheet.png)와 [2초 Idle 미리보기](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/arin-idle-preview.gif)는 최종 모델을 실제 렌더한 자료다.

시작마을 튜토리얼 안내인 아린의 Blender 제작·UE 전달용 산출물이다. 따뜻한 인상의 성인 여성, 밤색 옆땋기와 올리브색 외투를 입체 메시로 제작했다. [v2 콘셉트 보드](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/reference/arin-refinement-v2.png)는 imagegen으로 만든 형태·색상 참조이며, 이미지 투영으로 얼굴이나 몸을 구성하지 않았다.

2026-10-07 최종 Blender bake/export와 BLEND·FBX·GLB 독립 검사, 실제 FBX 22뷰 검토를 완료했다. UE5.8 안내인에 적용했고 최종 GUI 재로드·별도 Cmd 프로세스 저장 상태와 실제 PIE **67/67** 검사를 통과했다. Blender source LOD0는 **51,634삼각형**이며, UE 3개 LOD는 최종 1/.85/.65 설정으로 재생성했다. **최종 엔진 9뷰의 독립 시각 검토도 현재 정지형 튜토리얼 NPC 범위로 승인됐다.** [최종 독립 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-unreal-review.json)에 근거와 잔여 한계가 있다. 최신 산출물 SHA256가 보고서 입력 SHA와 일치할 때 그 상태를 사용하며, 재빌드·LOD 재조정 전 기록은 최종 근거와 구분한다. 전체 결과와 한계는 [기능 문서](D:/Project/Rogue10m/Feature/doc/2026-10-07_arin-quality-refinement.md)에 있다.

| 파일 | 용도 |
|---|---|
| [Arin.blend](D:/Project/Rogue10m/Art/NPC/Arin/Refined/Arin.blend) | 4개 atlas별 런타임 메시, 전용 리그·Idle, 렌더 스튜디오와 packed PBR 이미지 16장을 포함하는 최종 Blender 소스. |
| [Arin_EditableParts.blend](D:/Project/Rogue10m/Art/NPC/Arin/Refined/Arin_EditableParts.blend) | atlas 결합·bake 이전의 편집 가능한 174개 부위 메시와 palette 재질. 원형 수정은 이 단계와 제작 스크립트에서 수행한다. |
| [Arin.fbx](D:/Project/Rogue10m/Art/NPC/Arin/Refined/Arin.fbx) | 메시·제작 본 23개·2초 Idle을 함께 전달하는 결합 FBX. UE에서 하나의 Skeletal Mesh와 공유 Skeleton으로 가져오는 경로다. |
| [SK_Arin.fbx](D:/Project/Rogue10m/Art/NPC/Arin/Refined/SK_Arin.fbx) | 메시·리그 전달용 FBX. 애니메이션을 포함하지 않는다. |
| [A_Arin_Idle.fbx](D:/Project/Rogue10m/Art/NPC/Arin/Refined/A_Arin_Idle.fbx) | 같은 리그와 Idle을 전달하는 FBX. 메시를 포함하지 않는다. |
| [Arin.glb](D:/Project/Rogue10m/Art/NPC/Arin/Refined/Arin.glb) | 재질·리그·Idle의 선택적 비교용 GLB. UE 적용의 기준 파일은 FBX다. |
| [surface-manifest.json](D:/Project/Rogue10m/Art/NPC/Arin/Refined/surface-manifest.json) | atlas 구성, 채널 파일명, 해상도, PNG SHA256와 normal 규약. |

Blender 기준은 **미터 단위, Z up, front -Y, 발바닥 원점, 신장 1.70m(170cm)**이다. FBX에는 -Z forward/Y up 축 변환과 단위 정보가 들어간다. 최종 FBX 재수입 높이는 1.699999756m, UE 실제 bounds 높이는 170.000000749cm로 측정했다. Idle은 30fps, 1~61프레임의 2초 루프다. Blender의 제작 본은 23개이며 UE에는 Armature object root `Arin_Rig`가 추가된 24개 본이 있다. 메시와 Idle은 같은 전용 Skeleton을 참조한다. Manny/Quinn 자동 호환이나 자동 리타깃을 전제로 하지 않는다.

얼굴 표정·손가락 개별 동작·립싱크용 본이나 모프는 제공하지 않는다. 손은 손 전체 본으로 움직인다. 보행·전투·큰 관절 동작, 유연한 천과 물리 헤어의 준비 완료를 주장하지 않는다. 최종 form preview에서 하단 두피 피부 돌출, 목 상단 캡, 손의 링 단차·손톱, 무릎 자세의 큰 외투 관통과 뒤쪽 셔츠 돌출을 개선했지만, 별도 메시의 목 음영 경계와 근접 얼굴·헤어 품질 한계는 남아 있다.

| atlas | 해상도 | 채널 |
|---|---|---|
| Face | 2048×2048 | BaseColor, Roughness, Metallic, Normal |
| Hair | 2048×2048 | BaseColor, Roughness, Metallic, Normal |
| Cloth | 4096×4096 | BaseColor, Roughness, Metallic, Normal |
| Gear | 2048×2048 | BaseColor, Roughness, Metallic, Normal |

[Textures 폴더](D:/Project/Rogue10m/Art/NPC/Arin/Refined/Textures)의 PNG는 총 16장이다. 고유한 packed UV와 실제 shader/normal bake를 사용하며, 이름은 `T_Arin_<atlas>_<channel>.png`다. BaseColor만 sRGB로 읽는다. Roughness·Metallic은 sRGB를 끄고 UE `TC_MASKS`, Normal은 sRGB를 끄고 `TC_NORMALMAP`을 사용한다. Blender normal은 **OpenGL +Y tangent-space**이므로 UE texture의 **Flip Green Channel=True**가 필요하다. FBX normal 설정은 **Import Normals + MikkTSpace + Compute Weighted Normals=False**다. bake와 import의 normal/tangent 규약을 함께 유지한다.

FBX에는 LOD 체인이 포함되지 않는다. UE에서 imported LOD0를 보존한 **3개 LOD**를 별도로 생성했다. 처음 실제 1/.25/.125 설정은 ivory 의복 파편과 눈 디테일 손실로 시각 검토에 실패했고, 생성 LOD만 재조정했다. 최종 retained triangle target은 **1/.85/.65**, LOD1/2는 **Lock Edges=True**다. [LOD 재조정 보고서](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-lod-refinement.json)의 실제 정점은 **40,211 / 35,681 / 28,851**, 각각 4 material section이다. 정확한 UE runtime 삼각형 수는 해당 API가 노출하지 않아 null이며 target이나 정점 수로 추정하지 않는다. 최초 application 보고서의 1/.5/.25 요청과 이전 LOD 수치는 역사 기록이다. 최종 실제 캡처·독립 시각 판정과 저장 상태를 함께 읽어야 하며, 자동 LOD 전환·실기 성능 전체를 완료한 것은 아니다.

| 제작·검증 소스 | 역할 |
|---|---|
| [ArinHeadRefined.py](D:/Project/Rogue10m/Scripts/Blender/ArinHeadRefined.py), [ArinBodyRefined.py](D:/Project/Rogue10m/Scripts/Blender/ArinBodyRefined.py) | 얼굴·헤어 및 몸·손·의상 원형 생성. |
| [BuildRefinedArin.py](D:/Project/Rogue10m/Scripts/Blender/BuildRefinedArin.py), [BuildArin.py](D:/Project/Rogue10m/Scripts/Blender/BuildArin.py) | 조립, 공통 전용 리그·Idle, 저장·export·렌더. |
| [ArinCoatSkin.py](D:/Project/Rogue10m/Scripts/Blender/ArinCoatSkin.py), [ArinSurface.py](D:/Project/Rogue10m/Scripts/Blender/ArinSurface.py) | 외투 웨이트, UV·PBR bake·atlas 결합. |
| [PreviewRefinedArin.py](D:/Project/Rogue10m/Scripts/Blender/PreviewRefinedArin.py) | bake 전 실제 Blender form·관절 자세 확인. |
| [InspectArinQuality.py](D:/Project/Rogue10m/Scripts/Blender/InspectArinQuality.py), [RenderArinQuality.py](D:/Project/Rogue10m/Scripts/Blender/RenderArinQuality.py) | 독립 재수입의 치수·토폴로지·UV·재질·웨이트·동작 수치와 실제 다각도·검사 자세 렌더. |
| [ApplyRefinedArin.py](D:/Project/Rogue10m/Scripts/Editor/ApplyRefinedArin.py) | `/Game/Rogue10m/Characters/Arin/RefinedV2` 범위의 import, PBR, 공유 Skeleton·Idle, LOD 생성 및 안내인 적용. |
| [ValidateRefinedArinFresh.py](D:/Project/Rogue10m/Scripts/Editor/ValidateRefinedArinFresh.py), [ValidateRefinedArinPIE.py](D:/Project/Rogue10m/Scripts/Editor/ValidateRefinedArinPIE.py), [ReviewRefinedArinLOD.py](D:/Project/Rogue10m/Scripts/Editor/ReviewRefinedArinLOD.py) | 새 UE 프로세스의 저장 상태, PIE 튜토리얼 동작, 실제 LOD 검토. |
| [RefineArinGeneratedLODs.py](D:/Project/Rogue10m/Scripts/Editor/RefineArinGeneratedLODs.py) | imported LOD0·공유 Skeleton을 유지하는 최종 생성 LOD 재조정. |
| [AuditRefinedArinContent.py](D:/Project/Rogue10m/Scripts/AuditRefinedArinContent.py) | 적용 전후 Content SHA 비교와 변경 범위 확인. |

최종 파일·소스 SHA는 [model-build.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/model-build.json), PNG SHA는 surface manifest를 기준으로 확인한다. 독립 재수입 보고서는 [fresh-blend.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/fresh-blend.json), [fresh-fbx.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/fresh-fbx.json), [fresh-glb.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/fresh-glb.json)이며 [최종 실제 렌더 manifest](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/imported-pose-final/render-manifest.json)와 [v2 시각 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-visual-review-v2.json)를 함께 읽는다. 최종 FBX SHA는 `aa4298d51a6917f245f4be4f9c1dca3ae739ec80d7d4fb44b3157a0a2b8979c3`다.

[GUI 재로드](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-refined-reload.json)는 최종 texture 16장 원래 해상도·색공간·normal 규약·4재질·공유 Skeleton·최종 LOD를 확인했다. 초기 32px proxy는 일괄 속성 설정과 공식 `Editor.AsyncTextureCompilationFinishAll` 후 실제 원래 해상도로 읽혔다. [별도 Cmd 프로세스](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-fresh-process.json)는 NullRHI persistence만 검사했고, 렌더 검사와 구분한다. [최종 PIE](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/tutorial-runtime.json)는 외형·실제 Idle 움직임과 대화·정확한 1회 보급·가방 full 재시도·물약 실제 소모·포탈 후 보급 유지 등 67/67을 통과했다. 물리 F키 입력·한글 UI 배치 전체·네트워크·영구 프로필 저장은 범위 밖이다.

최종 UE 증거는 [LOD0](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-captures/refined/lod-0/manifest.json), [LOD1](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-captures/refined/lod-1/manifest.json), [LOD2](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-captures/refined/lod-2/manifest.json)의 front·face·oblique 총 9장이다. camera 2초 settle·실제 forced/predicted index를 확인했고 임시 background throttle·component·camera 상태를 복원했다. 초기 noisy 캡처와 첫 LOD 실패 자료를 보존했다. [최종 독립 엔진 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-unreal-review.json)는 이전 LOD1/2 의복 파편과 LOD2 홍채 소실이 해소됐고, 검토 화면에 새 심각한 관통·붕괴가 없다고 확인했다. [Content 보존](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/content-preservation.json)은 기존 6,328파일 SHA 동일, 안내인 참조 3개 변경·RefinedV2 24개 추가·삭제 0으로 통과했다. 최종 보수적 LOD는 형태를 유지하는 대신 정점 비용이 크며, 군중 규모 성능·메모리와 자동 LOD 전환 화면은 검증하지 않았다. 해당 보고서가 없거나 입력 SHA가 다르면 그 검증은 확인되지 않은 상태다.

품질 기준으로 설치된 [blender-character-workflow](C:/Users/PC/.codex/skills/blender-character-workflow/SKILL.md), [retopology](C:/Users/PC/.codex/skills/retopology/SKILL.md), [blender-asset-validation](C:/Users/PC/.codex/skills/blender-asset-validation/SKILL.md), [unreal-export](C:/Users/PC/.codex/skills/unreal-export/SKILL.md), [blender-iterative-refinement](C:/Users/PC/.codex/skills/blender-iterative-refinement/SKILL.md)를 참조했다. 핵심은 원형·변형·export·엔진 결과를 각각 증거로 확인하는 것이다. 수치 통과만으로 얼굴 품질이나 모든 자세의 관통 부재를 보장하지 않는다.

v1 산출물은 [Versions/v1](D:/Project/Rogue10m/Art/NPC/Arin/Versions/v1/README.md)에, v1 소스·렌더·UE 보고서는 [baseline](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/baseline)에 보존했다. 직전 export 후보는 [export-candidate-1](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/export-candidate-1)에 있다. rollback은 보존된 v1 파일과 적용 전 checkpoint의 안내인 참조를 사용한다. v2와 v1 전용 Skeleton·재질 참조를 섞지 않는다.
