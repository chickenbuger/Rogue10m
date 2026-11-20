# 시작 마을 StartHub 콘셉트

2026-11-20 · ImageGen 기본 제공 도구

60×60m StartHub 계획을 바탕으로 남쪽 진입로, 중앙 우물 광장, 북쪽 출발 포탈을 조감도로 그렸다. 아린은 남쪽 길 동편에 서고, 우물 양옆에는 여유 있는 보행로를 뒀다. 장비점과 강화소는 입구 양쪽, 원정 접수소와 지도·기록소는 광장 북쪽 양쪽, 선택 수련장은 서쪽에 배치했다. 청록 기와와 목조 주택을 기본으로 하고, 장비점의 천막·물약, 강화소의 모루·화덕, 접수소의 원정 짐, 기록소의 펼친 지도와 두루마리로 용도를 구분했다.

두 번째 생성에서 아린의 남쪽 길 동편 위치와 올리브색 망토·갈색 땋은 머리·가죽 가방을 실제 프로젝트 자료에 맞췄다. 이미지에서 아린 한 명, 우물 양옆 보행로, 포탈 한 곳, 준비 구역 네 곳과 수련장을 확인했다. 그림 속 건물 간격과 지면 치수는 시각적 제안이며 Unreal 맵 적용과 실측은 후속 제작 범위다.

- [제작용 콘셉트 PNG](evidence/start-hub-concept-20261120/start-hub-concept.png)
- [시작 마을 제작 기준](../../Docs/WorldMapProductionGuide.md#시작-마을-starthub)
- [축소 후 배치와 동선](2026-11-18_start-hub-quarter.md)
- [기존 마을 외형 참조](evidence/start-village-20261005/village-overview-final.png)
- [아린 외형 참조](evidence/arin-reference-refinement-20261009/unreal-captures/refined/lod-0/village-front.png)

## 최초 생성 프롬프트

```text
Use case: stylized-concept
Asset type: one full-bleed, high-detail game environment concept plate for Rogue10m’s StartHub starter village.
Primary request: Visualize a welcoming and practical pre-expedition settlement. The walk from the southern arrival path past Arin and the central well to the northern field portal must be understandable at a glance. This is a new illustration based on the existing 60 m by 60 m village plan.
Composition: Wide landscape, near-isometric three-quarter bird’s-eye view from slightly east of due south, looking north at about 42 degrees down. South is lower foreground, north upper background, west at screen left. Fill the picture with the entire compact settlement inside the boundary, all four service areas visible, minimal horizon. Use the southern gap in a low timber rail and fieldstone boundary to lead into the frame. A single broad packed-earth lane turns into a circular cobblestone square around a modest, roofed stone well, with broad, visibly unobstructed paths on both sides. Beyond the well the paths reunite and reach one modest upright blue portal in a weathered stone arch, with a clear stone waiting apron. Keep the view of the portal legible from the arrival side. Buildings and tree crowns frame the path and never conceal the well or the portal.
Village: Roughly ten modest timber-frame cottages set compactly around the square and streets, mixed low rooflines at human scale. Place Arin, a small dark-haired guide in a blue cloak, on a paved pull-out just beside the southern lane, marking scale without making a character portrait. Southwest: practical open equipment stall with a short rack of starter gear, potion bottles, leather rolls and supply crates. Southeast: a permanent-upgrade workshop differentiated by a small anvil, workbench, warmer ember light and metal fittings. Northwest: sheltered expedition desk with bedrolls, route maps, travel packs and a muted ochre banner. Northeast: map and records house with parchment rolls, open shelves and a desaturated blue-green awning. Along the west edge: little fenced practice yard with two straw dummies, its entrance facing a side path. Keep objects at the front of service buildings so each purpose is visible at this camera angle.
Style and light: Grounded stylized 3D fantasy environment, painterly surface wear and clear gameplay silhouettes. Weathered matte blue-green shingle roofs mixed with a few warm terracotta accents, honey plaster, rough old oak beams, irregular pale gray fieldstone, patches of thyme and soft moss in paving joints, rosemary planters, lantern brackets and gently worn stoops. Make the stones, overlapping shingles, posts, tools, fabrics and modest gardens individually believable at close inspection. Warm clear morning light across the rooftops, soft cool shadows, fresh green woodland as a restrained perimeter frame, small localized firelight, one luminous deep-blue portal as the strongest color accent. A quiet and lived-in starting point, with exacting level-art detail and clean readable space.
Constraints: Single scene only. No collaged panels, text, letters, typography, HUD, arrows, borders, compass, callouts or watermarks. No combat, no monsters, no castle, no oversized fantasy structures, no floating diorama base, no added gateways. No traffic, piles of props, roofs or foliage blocking either well bypass or the portal approach.
```

## 최종 수정 프롬프트

```text
Use case: precise-object-edit
Asset type: final Rogue10m StartHub game environment concept plate.
Input images: Image 1 is the generated bird's-eye village illustration, the edit target. Image 2 is the actual Unreal screenshot of Arin, the character appearance reference only. Take character design from Image 2, never its camera, environment, interface, or lighting.
Change only guide NPC Arin and her position on Image 1. Remove the blue-caped figure from the foreground left and restore the small region of empty stone paving under her. Place the same single small human-scale guide across the arrival path on the foreground right (east), on the open stone verge beside the red-roof workshop, between the lane and anvil area, leaving the dirt route entirely clear. Arin is an adult female guide with brown hair in a side braid, a short muted olive hooded shoulder cape over a cream long-sleeved tunic, dark olive trousers, tall brown boots, and a worn leather cross-body satchel, consistent with Image 2. A slight welcoming gesture toward the village. Fit naturally into the exact existing art style, small enough that the village remains the subject, with an appropriate contact shadow. There must be only one figure in the final image.
Keep the exact original framing, aspect ratio, perspective, all buildings and roof finishes, shops and props, two training dummies, foreground boundary, plantings, scenery, light, central well and its two wide bypasses, northern blue portal, and wide quiet walk from arrival to portal. Do not recompose or restyle. No text, annotations, or interface.
```
