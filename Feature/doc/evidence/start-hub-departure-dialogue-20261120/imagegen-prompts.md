# 대화 UI ImageGen 제작 프롬프트

Codex 내장 ImageGen, 불투명 16:9 시안 2종. 사용한 입력은 기존 마을과 아린의 실제 Unreal 캡처다.

## 북쪽 관문

- `Feature/doc/evidence/start-village-20261005/village-player-view.png`
- `DevLog/20261110_NpcDialogue/NpcResumeF9/02_selectable_dialogue.png`

```text
Use case: ui-mockup. Create one high-fidelity wide 16:9 in-game concept screenshot for Rogue10m, a stylized Unreal third-person fantasy action RPG, showing a Korean dialogue at the village's northern departure gate. Image 1 is an environment/style reference for its modest starter village, green tiled roofs, open air and ground palette. Image 2 is a camera and UI-family reference; redesign the UI completely while keeping its quiet brass and dark moss colors. Neither reference is a pixel edit target. Daylight gameplay camera behind the player, just off center, waist-high, with a village gate attendant waiting at the side, both visible above the UI. Stone uprights and a small green-tiled wooden gate sit at the end of the continuous path, with a faint blue-gray veil contained inside the opening and a hint of field road beyond. A grounded landmark, no dominant swirling energy, no huge neon ring, no castle scale. Put one readable compact centered dialogue card at the bottom, about 45% of screen width and 25% of height, leaving the gate and both characters clear. Layer charcoal-moss, a narrow brushed brass border, clear ivory modern Hangul, slight visual weight for focus without ornate decorations. The card contains this exact Korean text only: heading “북쪽 관문”, body “관문을 넘으면 20분 탐험이 시작됩니다.”; two vertically stacked choices, first “마을에 남기” with a visible gold focus bar and marker, second “탐험 출발” neutral; one aligned footer “↑↓ 선택 · Enter 확인 · Esc 닫기”. Use precise Hangul, no labels elsewhere, no logos, no editor or mockup presentation frame, no extra HUD labels. This is the dialogue-open state, so no approach prompt. Design for legibility at 1280×720.
```

## 길잡이 아린

- `DevLog/20261110_NpcDialogue/NpcResumeF9/02_selectable_dialogue.png`
- `Feature/doc/evidence/start-village-20261005/village-player-view.png`

```text
Use case: ui-mockup. Create one high-fidelity 16:9 in-game concept screenshot for the redesigned starter-village NPC dialogue in Rogue10m, a stylized Unreal third-person fantasy action RPG. Image 1 is visual reference for the existing Arin character clothing, position in the gameplay world, and muted moss/amber RPG UI family. Image 2 is environment reference for the village outdoor plaza and roofs. Both are style references, not edit targets. Retain the in-world Arin as a young adult guide in practical dark moss attire with a tied-back brown hairstyle. Show player from rear-left and live 3D Arin ahead and slightly to the left of center by the village well; both should be clearly visible, warmly lit in daylight. Use the right third for one compact mid-aligned dialogue sheet. Surface graphite/moss, thin brushed brass edge, ivory modern Korean sans, subtle selected-row tint plus gold focus bar, unobtrusive row icons if useful. Aim for sharp, production-like scale with white space; no giant portrait overlay, no full screen dimming, no extra badges or filler text. Exact Korean text: heading “길잡이 아린”, smaller role “출발 준비”, body “출발 전에 보급부터 챙기세요.” Then three roomy vertically stacked buttons “출발 보급품 받기” (first, focused), “장비와 물약”, “탐험 난이도”. Footer “↑↓ 선택 · Enter 확인 · Esc 닫기”. A tiny right chevron on the two submenu rows is fine. Keep all text in the card and ensure exact glyphs. No invented currencies, prices, goals, titles, watermarks, mockup framing, or unrelated menus. Show a polished in-game dialogue state, suitable for evaluation at 1280×720.
```

아린 시안에서 우물 뒤 강하게 빛나는 포탈만 마을 길로 바꿨다. 위 생성의 첫 결과를 편집 입력으로 사용했다.

```text
Use case: precise-object-edit. Image 1 is the edit target. Change only the small glowing neon blue arched doorway visible behind the roofed well. Replace the glowing doorway with the naturally lit cobblestone footpath, wooden fence and distant hillside consistent with the adjacent village backdrop so it reads as an ordinary town square. Preserve the well and roof, the two character poses and appearance, lighting, framing, the entire right dialogue panel with its Korean labels letter-for-letter, and the HUD exactly as seen. No other changes.
```
