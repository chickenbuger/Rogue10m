# 아린 Blender 품질 개선 결과

최종 전달의 파일·텍스처·제작 소스 동일성, 저장된 Unreal 의존성 24개의 해시는 [final-handoff.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-handoff.json)에 있다. [최종 실제 렌더 검토 시트](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/arin-quality-sheet.png)와 [2초 Idle GIF](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/arin-idle-preview.gif)를 함께 전달한다.

2026-10-07 기준으로 Refined v2의 Blender 제작, PBR bake, BLEND·FBX·GLB 독립 재수입 검사와 최종 FBX 22개 실제 렌더 검토를 완료했다. UE5.8 안내인에 전용 외형·PBR·Idle을 적용했고 최종 GUI 재로드, 별도 Cmd 프로세스 저장 상태, 실제 PIE **67/67** 검사를 통과했다. Blender LOD0는 **51,634삼각형**, 제작 리그는 **23본**, Idle은 **30fps·2초**다. UE LOD0~2는 최종 1/.85/.65 설정으로 재생성했고 **최종 9개 실제 엔진 화면의 독립 시각 검토도 승인됐다**. 완료 범위는 현재 정지형 시작마을 안내 NPC이며 [최종 독립 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-unreal-review.json)에 근거와 한계가 있다.

![최종 FBX를 독립 재수입한 실제 Blender 렌더](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/imported-pose-final/idle-oblique.png)

## 범위와 기준선

작업 브랜치는 `Sprint#5-17-arin-quality-refinement`이며 [설계 계획](D:/Project/Rogue10m/Feature/architect/2026-10-07_arin-quality-refinement.md)의 Q1~Q6 패킷을 따른다. 목표는 친근한 성인 여성 아린의 얼굴·머리·손·여행복을 개선하고 실제 UV/PBR과 엔진 전달 근거를 만드는 것이다. 초록·아이보리 의상, 옆땋기, 가방과 빨간 물약 2병의 정체성, 170cm 높이와 전용 리그·Idle을 유지했다.

기준선 산출물은 [Versions/v1](D:/Project/Rogue10m/Art/NPC/Arin/Versions/v1/README.md), 원본 소스·렌더·보고서는 [baseline](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/baseline)에 동결했다. [critic-head.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/critic-head.json)과 [body-review.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/body-review.json)에 수정 전 요구사항별 pass/fail/unclear를 기록했다. [v2 콘셉트 보드](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/reference/arin-refinement-v2.png)는 imagegen으로 만든 형태·색상 지침이다. 모델과 bake는 실제 입체 메시·shader에서 생성했으며 보드를 메시 표면에 투영하지 않았다.

## 실제 수치 변화

비교 근거는 [v1 독립 검사](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/baseline/fresh-metrics.json), [최종 빌드](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/model-build.json), [최종 BLEND 검사](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/fresh-blend.json)다. v1은 새 품질 계약의 UV/PBR 요구사항을 충족하지 않았으며, 기존 v1의 튜토리얼 동작 통과와는 별개다.

| 항목 | v1 | Refined v2 |
|---|---:|---:|
| 편집용 부위 메시 | 175 | 174 |
| 런타임 BLEND/FBX 메시 | 175 | 4 atlas 메시 |
| LOD0 삼각형 | 42,764 | 51,634 / 상한 55,000 |
| BLEND/FBX 정점 | 21,726 | 26,185 |
| 재질 | 15 palette | 4 PBR |
| UV 없는 런타임 메시 | 175 | 0 |
| 이미지 텍스처 | 0 | 16 PNG |
| 제작 본 | 23 | 23 |
| 최대 정점 영향 본 | 2 | 3 / 상한 4 |
| 무웨이트·비정규화 정점 | 각각 0 | 각각 0 |
| n-gon | 209 | 0 |
| invalid 정점·영면적 삼각형 | 각각 0 | 각각 0 |
| 높이·Idle | 1.70m·2초 | 1.70m·2초 유지 |

삼각형은 8,870개 늘었고 얼굴·손·의복 원형에 밀도를 더 배정했다. 부위 편집성을 보존하는 `Arin_EditableParts.blend`와 4개 atlas 메시로 결합한 `Arin.blend`를 따로 저장했다. GLB 재수입은 51,634삼각형·39,908정점이다. 정점 수는 UV/normal seam 분할 등 포맷 표현에 따라 다르므로 BLEND/FBX 수치와 같은 값으로 보고하지 않는다.

Blender 좌표는 **Z up/front -Y, 미터 단위, 발바닥 원점**이다. 최종 FBX 프레임 1의 측정 높이는 1.699999756m다. Idle 프레임 1·16·31·46·61을 검사했고 root 행렬 변화는 0, 마지막 프레임의 메시별 시작점 변위는 0이었다. 프레임 16·46에서 실제 평가 메시가 변형됨을 확인했다. 이 수치는 연속 재생의 모든 관통이나 자연스러운 동작을 증명하지 않는다.

## critic 이후 수정과 실제 재검토

| 원인·문제 | 소스 수정 | 최종 근거와 남은 범위 |
|---|---|---|
| 독립 눈테·입술과 단순 얼굴 표면 | 연속 얼굴 메시의 eye aperture와 quad socket/lid loop, 통합 코·입 주변 윤곽, 더 부드러운 볼·턱과 tapered brow를 생성했다. 피부 색조 속성을 bake에 반영했다. | 정면·측면·clay에서 얼굴 형태가 유지된다. 인형 같은 단순 얼굴과 제한된 눈꺼풀 표정은 남아 있다. |
| 첫 후보의 눈구가 위 눈꺼풀 위로 돌출 | 실제 ellipsoid 중심·반경과 aperture·iris·pupil을 같은 파라미터로 조정해 두개 내부에 배치했다. 덮개 메시를 추가하지 않았다. | [눈 수정 근거](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/head-eye-repair.json)의 양쪽 각 2,728개 aperture 외부 표본에서 돌출 0. 최종 얼굴 렌더에서도 이전 흰 돌출이 보이지 않는다. |
| 하단 두피 경계의 작은 피부색 돌출 | 하단 측후면 scalp fit에 최대 1.2mm 여유를 추가했다. 눈 공간·얼굴·높이·토폴로지를 유지했다. | [두피 수정 근거](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-scalp-fit-review.json)의 측면 표본 누출 52/38개→양쪽 0개. 최종 profile에서 피부 노출이 사라졌다. 넓은 cap·굵은 ribbon·crown loop의 단순 헤어 표현은 남아 있다. |
| 목 상단 캡의 날카로운 겹침 | 몸 소스의 숨겨진 상단 목 링을 머리 안으로 넣었다. head 하단과 관절·높이는 유지했다. | 실제 profile에서 노출 캡이 줄었다. 별도 head/neck 메시의 음영 경계는 남아 있으며 welded 관절로 주장하지 않는다. |
| mantle를 뚫는 collar/sleeve와 몸 뒤 의복 돌출 | 숨겨진 collar·sleeve root를 축소하고 shoulder turnover·armhole을 조정했다. 바지 허리와 blouse 뒤쪽을 의상 내부로 넣었다. | 최종 앞·옆·뒤와 clay에서 이전 큰 ivory 패치와 뒤 slit의 흰 띠가 보이지 않는다. mantle는 넓고 뻣뻣한 구조형 의복으로 남아 있다. |
| 손가락 링 단차·두꺼운 네모 손톱 | 연속 ring taper와 반대 방향 엄지 곡선을 사용하고 얇은 둥근 nail shell을 손가락 표면에 맞췄다. | 이전 단차와 두꺼운 손톱 블록이 개선됐다. 단순한 손가락·관절·손톱 형태와 whole-hand 리그는 유지된다. |
| 무릎 굽힘에서 허벅지가 코트·앞치마를 크게 관통 | 6개 side tail/trim과 10개 중앙 apron/장식의 1,272정점에 thigh↔pelvis 웨이트를 적용했다. 중앙은 X 방향 bilateral blend를 사용하며 최대 3영향을 유지했다. | 최종 thigh -25°/calf 70° 검사 두 시점에서 큰 관통이 해소됐다. 반대 다리·양다리·보행 전체 주기는 검증하지 않았다. |

원인 및 국소 수치 근거는 [garment fit](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/body-clearance-iteration-review.json), [coat/hand fit](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/coat-hand-fit-review.json), [rear shirt tuck](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/rear-shirt-tuck-review.json)에 보존했다. 이 중 이전 단계의 pending 표시는 해당 시점의 기록이며, 최종 시각 판정은 아래 v2 리뷰를 기준으로 한다.

## UV/PBR와 전달 파일

[surface-manifest.json](D:/Project/Rogue10m/Art/NPC/Arin/Refined/surface-manifest.json)에 Face 2048², Hair 2048², Cloth 4096², Gear 2048²의 BaseColor·Roughness·Metallic·Normal 총 16 PNG 이름·SHA256가 있다. 실제 packed UV와 Cycles shader/normal bake를 사용했다. 최종 4개 메시 모두 UV face 유효 비율 1.0, invalid loop 0, 0~1 밖 loop 0이다. inspector는 UV overlap·고유 texel 점유율을 계산하지 않았으므로 겹침이 수치로 완전히 배제됐다고 주장하지 않는다.

BaseColor만 sRGB로 읽는다. Roughness/Metallic은 non-sRGB `TC_MASKS`, Normal은 non-sRGB `TC_NORMALMAP`이다. tangent normal은 Blender **OpenGL +Y**이므로 UE의 **Flip Green Channel=True**를 사용한다. FBX import는 **Import Normals·MikkTSpace·Compute Weighted Normals=False**로 bake 규약과 맞춘다.

[산출물 README](D:/Project/Rogue10m/Art/NPC/Arin/Refined/README.md)에 파일별 사용법을 기록했다. `Arin.blend`는 4 runtime mesh와 packed PBR 16장, `Arin_EditableParts.blend`는 174부위와 bake 전 palette를 보존한다. `Arin.fbx`는 메시·23 제작 본·Idle을 함께 전달하고, `SK_Arin.fbx`는 애니메이션 없이 메시·리그, `A_Arin_Idle.fbx`는 메시 없이 리그·Idle을 전달한다. `Arin.glb`는 선택적 비교 산출물이다.

| 최종 검사 대상 | 입력 SHA256 | 결과 |
|---|---|---|
| BLEND | `3cde5f3c16d66ee3e1192db319c4a02d3b51991a8070b85e5c0cc75679b1c6df` | `passed_requires_visual_review` |
| FBX | `aa4298d51a6917f245f4be4f9c1dca3ae739ec80d7d4fb44b3157a0a2b8979c3` | `passed_requires_visual_review` |
| GLB | `bf2d64e591d9b9d95d4abf063ec92d5b7d73142bb98ec0cf886656e127407c89` | `passed_requires_visual_review` |

보고서는 [fresh-blend](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/fresh-blend.json), [fresh-fbx](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/fresh-fbx.json), [fresh-glb](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/fresh-glb.json)이며 각각 findings 0이다. FBX의 Blender 재수입 계약은 image-driven BaseColor/Normal을 확인하는 범위이며, Roughness/Metallic을 포함한 네 채널 엔진 전달은 별도 manifest와 UE texture/material 검증으로 확인해야 한다. GLB는 metallic/roughness packed 표현으로 이미지 12개를 참조한다. GLB의 seam 분할에서 생기는 boundary edge 25,352개는 별도 수치로 남기며, BLEND/FBX의 boundary edge 0과 혼동하지 않는다.

## Blender QA 결과

[최종 실제 FBX render manifest](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/imported-pose-final/render-manifest.json)의 입력 SHA와 위 FBX SHA가 일치한다. 독립 reviewer가 [final-visual-review-v2.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-visual-review-v2.json)에서 idle 정면·후면·좌우·oblique·top, 얼굴·profile·손·부츠, neutral, arm-bend, reach, torso-twist, knee-bend와 clay의 **22개 실제 이미지**를 열어 검토했다. 22개 PNG SHA도 manifest와 일치한다. 문서 작성 담당은 최종 oblique와 face를 추가로 열어 확인했다.

Blender 단계의 판정은 `accepted_as_a_stylized_gameplay_candidate_pending_unreal_runtime_validation`이었다. 검토한 자세의 큰 관통·붕괴가 없고, 이전 무릎의 허벅지/코트 관통은 같은 두 시점에서 해소됐다. 이후 UE 검증과 최종 독립 리뷰까지 완료했으며 아래에 결과를 기록한다. 얼굴·헤어·손의 근접 아트 한계와 rigid mantle는 남는다. 정적 이미지 검토는 모든 프레임의 관통 부재, 천·헤어 물리, 보행 품질을 증명하지 않는다.

## UE 적용·런타임 결과

[최초 적용 보고서](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-refined-application.json)는 `imported_and_applied`이며 `/Game/Rogue10m/Characters/Arin/RefinedV2`의 mesh·Skeleton·Idle, 16 texture와 4 material을 안내인에 연결했다. 이 보고서의 LOD 항목은 재조정 전 기록이다. 최종 LOD·재질·참조는 [GUI 재로드](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-refined-reload.json)의 `reload_verified`와 [별도 Cmd 프로세스](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-fresh-process.json)의 `passed`를 기준으로 읽는다. 세 보고서의 FBX SHA는 `aa4298d51a6917f245f4be4f9c1dca3ae739ec80d7d4fb44b3157a0a2b8979c3`로 최종 파일과 일치한다.

![UE StartHub에서 최종 LOD0를 표시한 실제 editor capture](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-captures/refined/lod-0/village-oblique.png)

| 항목 | 최종 확인 결과 |
|---|---|
| 전용 mesh·Idle | 같은 `SK_Arin_Skeleton` 참조, 2초 Idle, 4개 PBR slot 유지 |
| 본·치수 | 제작 23본에 Armature object root `Arin_Rig`가 더해진 UE 24본, 실제 bounds 높이 170.000000749cm |
| normal·texture | Import Normals/MikkTSpace/weighted=False, BaseColor sRGB, Roughness/Metallic TC_MASKS, Normal TC_NORMALMAP·green flip. 16장 모두 Face/Hair/Gear 2048²·Cloth 4096²로 읽힘 |
| GUI persistence | 최종 LOD와 재질·24개 dependency package, actor/CDO/data의 외형·Idle·보호된 안내인 설정 재로드 확인 |
| 새 프로세스 persistence | 별도 `UnrealEditor-Cmd`, NullRHI로 최종 저장 상태 통과. 렌더 검사는 수행하지 않는 범위 |
| PIE | 최종 실제 게임 world 67/67 통과. 전용 외형·Skeleton·2초 Idle·4 PBR slot·3 LOD·실제 head motion·stationary actor 확인 |
| Content 감사 | 기준 6,331파일 중 6,328개 SHA 동일, 허용된 안내인 참조 3개 변경, RefinedV2 자산 24개 추가, 삭제·예상 밖 변경 0 |

초기 일부 텍스처가 32×32 proxy로 읽히는 문제가 있었다. 최종 속성을 일괄 적용하고 공식 `Editor.AsyncTextureCompilationFinishAll` 완료 경로를 사용한 뒤 실제 16장 해상도와 색공간·압축·green flip을 확인했다. [texture compilation 소스 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/texture-compilation-source-review.json)에 실패한 저장 단독 증명과 공식 UE 소스 근거를 보존했다. 일괄 적용과 FinishAll 각각의 효과를 분리 측정한 것은 아니며, 최종 복합 조치·재로드·새 프로세스 결과가 완료 근거다.

### LOD 재조정과 실제 캡처

FBX에는 LOD 체인이 없다. 처음 생성된 native LOD의 실제 retained ratio는 **1/.25/.125**였다. 최초 적용 보고서의 요청값 1/.5/.25와 실제 reduction readback은 달랐으며, 최종값으로 혼용하지 않는다. [첫 LOD 시각 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-lod-first-visual-review.json)는 LOD1/2의 ivory 의복 파편과 LOD2 눈 디테일 손실을 실패로 판정했다. 원형·LOD0를 바꾸지 않고 생성 LOD만 보수적으로 재생성했다.

[unreal-lod-refinement.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-lod-refinement.json)의 최종 설정·실측은 다음과 같다. LOD0 정점·공유 Skeleton·재질·안내인 속성은 유지했고, 수정·저장 대상은 RefinedV2 `SK_Arin`뿐이다.

| UE LOD | retained triangle target | Lock Edges | 실제 정점 | material sections |
|---|---:|---|---:|---:|
| 0 | 1.00 | False / imported source 보존 | 40,211 | 4 |
| 1 | 0.85 | True | 35,681 | 4 |
| 2 | 0.65 | True | 28,851 | 4 |

현재 SkeletalMeshEditorSubsystem은 정확한 runtime triangle count를 노출하지 않아 보고서의 `triangles`는 null이다. 위 target이나 정점 수에서 삼각형을 추정하지 않는다. Blender source의 51,634삼각형도 UE 각 LOD의 측정값으로 바꿔 쓰지 않는다. 최종 보수적 LOD는 검증된 의상·얼굴 형태를 유지하는 대신 처음 목표보다 정점 비용이 크다. 군중 규모 frame-time/memory와 자동 screen-size LOD 전환은 별도 검증하지 않았다.

최종 엔진 증거는 [LOD0 manifest](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-captures/refined/lod-0/manifest.json), [LOD1 manifest](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-captures/refined/lod-1/manifest.json), [LOD2 manifest](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/unreal-captures/refined/lod-2/manifest.json)의 front·face·oblique **총 9장**이다. 실제 forced/predicted LOD index를 확인하고 camera를 고정한 뒤 2초 동안 실제 editor frame을 진행해 capture했다. [capture 소스 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/lod-capture-source-review.json)의 초기 noisy 증거와 첫 LOD 실패 자료는 보존했다. 캡처 중 임시 background throttle·component·camera 변경은 복원했고 자산을 저장하지 않았다.

[final-unreal-review.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/final-unreal-review.json)은 2026-10-07 13:23:49 UTC에 `accepted_for_current_stationary_tutorial_npc_scope_with_documented_limits`로 기록됐다. 독립 reviewer가 실제 9장을 모두 열어 최종 LOD1/2의 하얀 의복 파편이 사라졌고 LOD2 양쪽 홍채·눈꺼풀 형태가 유지됨을 확인했다. 최종 FBX 22뷰 해시, GUI 재로드, 새 Cmd PID 113212, 최종 PIE 67개 검사와 Content 감사도 함께 확인했다. 문서 작성 담당은 최종 UE LOD0 oblique를 추가로 열었다. 눈에 띄는 새 관통·붕괴는 발견되지 않았지만 얼굴·ribbon hair·손·mantle의 근접 단순성과 정적 editor capture의 증거 범위는 유지한다.

### 튜토리얼 동작과 보존 범위

[재조정 전 PIE](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/tutorial-runtime-before-lod-refinement.json)와 [최종 PIE](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/tutorial-runtime.json)는 각각 **67/67 passed**다. 최종 JSON은 2026-10-07 13:19:50 UTC에 기록됐고, LOD 재조정 전 보고서와 SHA가 다르다. 실제 reflected 대화·inventory API를 사용해 거리 밖 대화/수령 거부, 대화 UI와 이동/시야 유지, 정확한 물약 2개 1회 지급, 재방문 중복 지급 거부, 가방이 가득 찬 상태의 영수증 보존과 공간 확보 후 재시도, 실제 물약 사용과 1개 소모, 실제 포탈 이동 후 남은 보급품 유지·소모품 재생성 방지, 탐험 timer 시작을 확인했다. 개별 check의 `passed` 67개를 읽어 확인했다.

이는 물리 F키 입력과 한글 UI 배치 전체를 검사한 결과가 아니며, 네트워크 플레이·영구 캐릭터 프로필 저장은 범위 밖이다. PIE 데이터는 일회성 fixture이고 자산·맵을 저장하지 않았다. [Content 보존 보고서](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/content-preservation.json)는 `passed`이며 변경한 기존 3개는 `BP_StartVillageGuide`, `DA_StartVillageGuide`, StartHub 안내인 ExternalActor의 외형 참조다. 대사·보급 수량·reward ID, actor 위치·태그·650cm detection·320cm interaction과 기존 비충돌 외형 구성을 유지했다.

실행·검증 소스는 [ApplyRefinedArin.py](D:/Project/Rogue10m/Scripts/Editor/ApplyRefinedArin.py), [ValidateRefinedArinFresh.py](D:/Project/Rogue10m/Scripts/Editor/ValidateRefinedArinFresh.py), [ValidateRefinedArinPIE.py](D:/Project/Rogue10m/Scripts/Editor/ValidateRefinedArinPIE.py), [RefineArinGeneratedLODs.py](D:/Project/Rogue10m/Scripts/Editor/RefineArinGeneratedLODs.py), [ReviewRefinedArinLOD.py](D:/Project/Rogue10m/Scripts/Editor/ReviewRefinedArinLOD.py), [AuditRefinedArinContent.py](D:/Project/Rogue10m/Scripts/AuditRefinedArinContent.py)다. 실제 Blender/UE 실행은 root가 수행했고 문서 작성 담당은 보고서·증거를 읽어 정리했다.

## 제작 소스·기준과 남은 한계

원형은 [ArinHeadRefined.py](D:/Project/Rogue10m/Scripts/Blender/ArinHeadRefined.py)·[ArinBodyRefined.py](D:/Project/Rogue10m/Scripts/Blender/ArinBodyRefined.py), 조립·export는 [BuildRefinedArin.py](D:/Project/Rogue10m/Scripts/Blender/BuildRefinedArin.py)·[BuildArin.py](D:/Project/Rogue10m/Scripts/Blender/BuildArin.py), UV/PBR는 [ArinSurface.py](D:/Project/Rogue10m/Scripts/Blender/ArinSurface.py), 코트 웨이트는 [ArinCoatSkin.py](D:/Project/Rogue10m/Scripts/Blender/ArinCoatSkin.py)다. 검사와 재현 렌더는 [InspectArinQuality.py](D:/Project/Rogue10m/Scripts/Blender/InspectArinQuality.py)·[RenderArinQuality.py](D:/Project/Rogue10m/Scripts/Blender/RenderArinQuality.py)를 사용한다. 소스별 SHA는 최종 model-build 보고서에 있다.

설치된 [blender-character-workflow](C:/Users/PC/.codex/skills/blender-character-workflow/SKILL.md), [retopology](C:/Users/PC/.codex/skills/retopology/SKILL.md), [blender-asset-validation](C:/Users/PC/.codex/skills/blender-asset-validation/SKILL.md), [unreal-export](C:/Users/PC/.codex/skills/unreal-export/SKILL.md), [blender-iterative-refinement](C:/Users/PC/.codex/skills/blender-iterative-refinement/SKILL.md)를 적용했다. [character form review](C:/Users/PC/.codex/skills/blender-character-workflow/references/character-form-review.md)와 [facial retopo](C:/Users/PC/.codex/skills/retopology/references/facial-retopo.md)의 원형·관절·눈/입 loop 기준을 참고하고, 실패 후보를 보존한 뒤 원인 수정→같은 실제 검사→최종 export의 순서를 사용했다. quad 중심 제작은 full facial rig나 모든 관절의 production retopology 완료를 뜻하지 않는다.

골격은 아린 전용 23본이며 Manny/Quinn 자동 호환을 제공하지 않는다. 손은 whole-hand 본으로 움직이고 개별 손가락·표정·lip-sync용 본/모프가 없다. mantle·strap·braid는 뻣뻣한 단순 변형을 유지하며, 보행·전투·양다리 연속 주기와 cloth/hair simulation은 구현·검증하지 않았다. 원본 콘셉트와 동일한 얼굴이나 출시용 캐릭터 완성도를 주장하지 않는다.

복구 기준은 보존된 v1 산출물·소스와 적용 전 안내인 참조 checkpoint다. 직전 후보는 [export-candidate-1](D:/Project/Rogue10m/Feature/doc/evidence/arin-quality-20261007/export-candidate-1)에 보존했다. v1/v2의 전용 Skeleton·재질 참조를 섞지 않는다. 커밋·푸시는 수행하지 않았다.
