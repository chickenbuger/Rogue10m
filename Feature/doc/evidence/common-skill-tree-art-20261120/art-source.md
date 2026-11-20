# 공통 스킬트리 아트 원본

승인 시안을 ImageGen 편집의 단일 참조로 사용했다. 투명 배경을 요청해 반환된 1244×1264 RGBA 원본을 보존했다. 제작 스크립트로 16개 아이콘을 4×4 순서의 1024×1024 아틀라스에 정렬하고 가장자리 투명 여백을 맞췄다. 숯빛 바탕은 승인 시안의 오른쪽 위에서 글자와 장식이 없는 256×256 영역을 추출했다. [승인 시안](../common-skill-web-20261120/approved-concept.png) · [생성 원본](../../../../Art/UI/CommonSkillTree/Medallions.generated.png) · [등록 스크립트](../../../../Art/UI/CommonSkillTree/PackCommonSkillTreeArt.ps1)

| 원본 | SHA-256 |
| --- | --- |
| Medallions.generated.png | `8458dec2cb813a1efed94fcc0fc980713d26b1663823cedabcca8968a737eb36` |
| T_CommonSkillMedallions.png | `e6c6a41fa7e6278f964348cab6395906066fb1de7292f544fa96f5d52b80172c` |
| T_CommonSkillCharcoal.png | `5f52c0696dcf659f5c76582b7a89a36e0ea5a45fc60b0bc1bcc39b4654780538` |

## ImageGen 프롬프트

```text
Create production art for the supplied Rogue10m RPG concept: ONE SQUARE 4 by 4 sprite sheet of EXACTLY SIXTEEN isolated, ornate metal medallion skill buttons on a TRUE TRANSPARENT ALPHA BACKGROUND. Faithfully reproduce the detailed COINS in the reference: dark textured discs, burnished bronze and gold thin concentric beveled rims, small bright specular light along the top-left rim, silver/copper/teal inlaid raised pictograms with carefully rendered light and shadow, realistic tactile metallic material. Their classic gameplay silhouette is a perfect circle, viewed directly face on, centered exactly within each of 16 equal square cells. This is a texture atlas, not a UI screenshot, so remove all text, letters, words, captions, plaques, glow, route strokes, backgrounds, dividers, decoration outside circles and dialogue. 4 columns and 4 rows absolutely. Equal diameters approximately 77% of a cell with generous even transparent gutters. Align the medallion centers at X and Y positions 12.5%, 37.5%, 62.5%, 87.5% of the square canvas. Consistent artwork and size, separate medallions, no rotation or overlap.

Layout, strictly left-to-right:
ROW ONE — (1) gold open hand against dark umber; (2) pale icy blue sweeping feather and wind against midnight navy; (3) two small silver-blue boots in a rising leap and a bright blue takeoff sparkle, as in the large icon on the left of the reference; (4) a single silver-blue shoe with two upward arrows, navy.
ROW TWO — (5) tall pale blue winged boot pointing upward, navy; (6) silver mint low boot with three bright teal horizontal speed streaks, deep green; (7) two circular teal turning arrows, deep green; (8) three gold-metal-trimmed turquoise chevrons pointing right, deep green.
ROW THREE — (9) mint winged boot with sharp mint horizontal speed streaks, deep green; (10) vintage gold stopwatch with visible crown and tick marks, dark warm brown; (11) small orange figure curled into a forward roll enclosed in two smooth orange swooshes, dark warm brown; (12) golden amber running figure with a strong trailing streak, dark warm brown.
ROW FOUR — (13) vintage gold stopwatch inside a clockwise gold arrow, dark warm brown; (14) three sinuous cyan wind ribbons, dark midnight green; (15) an icy blue downward motion arrow above concentric shallow ripples, indigo; (16) a cyan running figure with vivid blue acceleration streaks, deep teal.
Do not place numerals or paint any backdrop other than transparency. Keep the pictogram clearly visible at small sizes. No camera perspective. Premium looking 3D engraved copper, bright fine edges and polished relief. Exact 4x4 grid with all sixteen coins.
```
