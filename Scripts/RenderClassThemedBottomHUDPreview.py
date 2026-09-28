"""Render deterministic 1920x1080 previews from the exact imported Bottom HUD sources."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path("D:/Project/Rogue10m")
REFERENCE = ROOT / "Content/UI/HUD/ClassThemes/Source/Rogue10mCombatHUDReference.png"
THEME_DIR = ROOT / "Content/UI/HUD/ClassThemes"
XP_FRAME = ROOT / "Content/UI/HUD/Gothic/T_HUD_GothicXPBottomFrame.png"
OUTPUT_DIR = ROOT / "Saved/ClassBottomHUDPreview"

THEMES = {
    "Wizard": {
        "label": "마법사",
        "resource": "MP",
        "resource_color": (42, 104, 238, 255),
        "identity": (122, 78, 236, 255),
        "slot": (80, 70, 180, 255),
    },
    "Warrior": {
        "label": "전사",
        "resource": "STAMINA",
        "resource_color": (220, 142, 28, 255),
        "identity": (190, 52, 26, 255),
        "slot": (154, 58, 34, 255),
    },
    "MartialArtist": {
        "label": "권사",
        "resource": "STAMINA",
        "resource_color": (224, 150, 26, 255),
        "identity": (44, 172, 100, 255),
        "slot": (158, 76, 40, 255),
    },
    "Rogue": {
        "label": "도적",
        "resource": "STAMINA",
        "resource_color": (116, 166, 34, 255),
        "identity": (50, 174, 92, 255),
        "slot": (88, 42, 112, 255),
    },
}


def load_font(size: int):
    candidates = [
        Path("C:/Windows/Fonts/malgun.ttf"),
        Path("C:/Windows/Fonts/arial.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


FONT_12 = load_font(12)
FONT_14 = load_font(14)
FONT_18 = load_font(18)


def draw_centered(draw, box, text, font, fill):
    left, top, right, bottom = box
    bbox = draw.textbbox((0, 0), text, font=font)
    width = bbox[2] - bbox[0]
    height = bbox[3] - bbox[1]
    draw.text(
        ((left + right - width) / 2, (top + bottom - height) / 2 - 1),
        text,
        font=font,
        fill=fill,
        stroke_width=1,
        stroke_fill=(0, 0, 0, 230),
    )


def draw_bar(draw, box, percent, fill, label):
    x, y, width, height = box
    draw.rounded_rectangle((x, y, x + width, y + height), radius=4, fill=(4, 5, 7, 230), outline=(96, 82, 54, 255), width=2)
    inset = 5
    fill_width = int((width - inset * 2) * percent)
    draw.rounded_rectangle(
        (x + inset, y + inset, x + inset + fill_width, y + height - inset),
        radius=3,
        fill=fill,
    )
    draw_centered(draw, (x, y, x + width, y + height), label, FONT_12, (240, 228, 198, 255))


def draw_slots(draw, start_x, y, color, theme, item=False):
    for index in range(5):
        x = start_x + index * 58
        draw.rounded_rectangle(
            (x, y, x + 48, y + 56),
            radius=3,
            fill=(7, 8, 10, 240),
            outline=(112, 91, 58, 255),
            width=2,
        )
        inner = (x + 6, y + 5, x + 42, y + 39)
        if item:
            bottle_color = [(176, 34, 28), (48, 82, 184), (170, 142, 82), (66, 154, 64), (112, 72, 152)][index]
            draw.rounded_rectangle(inner, radius=8, fill=(*bottle_color, 215))
            draw.rectangle((x + 20, y + 2, x + 28, y + 10), fill=(190, 166, 112, 255))
        else:
            r, g, b, _ = color
            if theme == "Wizard":
                draw.ellipse(inner, outline=(r, g, b, 255), width=3)
                draw.line((x + 24, y + 8, x + 24, y + 37), fill=(160, 154, 255, 255), width=2)
                draw.line((x + 10, y + 22, x + 38, y + 22), fill=(160, 154, 255, 255), width=2)
            elif theme == "Warrior":
                draw.polygon([(x + 24, y + 7), (x + 36, y + 16), (x + 32, y + 36), (x + 16, y + 36), (x + 12, y + 16)], fill=(r, g, b, 220))
                draw.line((x + 14, y + 34, x + 35, y + 10), fill=(236, 186, 86, 255), width=3)
            elif theme == "MartialArtist":
                draw.ellipse(inner, outline=(226, 166, 56, 255), width=3)
                draw.rounded_rectangle((x + 17, y + 15, x + 32, y + 33), radius=5, fill=(r, g, b, 235))
            else:
                draw.line((x + 12, y + 34, x + 34, y + 10), fill=(94, 202, 116, 255), width=4)
                draw.line((x + 14, y + 10, x + 36, y + 34), fill=(150, 86, 184, 255), width=4)
        draw_centered(draw, (x, y + 40, x + 48, y + 55), str(index + 1), FONT_12, (224, 188, 102, 255))


def render_theme(theme: str, spec) -> Path:
    scene = Image.open(REFERENCE).convert("RGBA").resize((1920, 1080), Image.Resampling.LANCZOS)
    darken = Image.new("RGBA", scene.size, (0, 0, 0, 0))
    dark_draw = ImageDraw.Draw(darken)
    dark_draw.rectangle((0, 812, 1920, 1080), fill=(0, 0, 0, 180))
    scene.alpha_composite(darken)

    frame = Image.open(THEME_DIR / f"T_HUD_Bottom_{theme}.png").convert("RGBA")
    scene.alpha_composite(frame, (0, 820))
    draw = ImageDraw.Draw(scene)

    draw_bar(draw, (150, 892, 650, 34), 0.82, (188, 24, 20, 255), "HP 820 / 1000")
    draw_bar(
        draw,
        (1120, 892, 650, 34),
        0.68,
        spec["resource_color"],
        f"{spec['resource']} 68 / 100",
    )
    draw_slots(draw, 270, 952, spec["slot"], theme, item=False)
    draw_slots(draw, 1350, 952, spec["slot"], theme, item=True)

    draw_centered(draw, (880, 982, 1040, 1004), spec["label"], FONT_14, (236, 196, 100, 255))
    draw_centered(draw, (320, 930, 520, 948), "SKILL", FONT_12, (206, 174, 106, 255))
    draw_centered(draw, (1400, 930, 1600, 948), "ITEM", FONT_12, (206, 174, 106, 255))

    xp_frame = Image.open(XP_FRAME).convert("RGBA").resize((1920, 52), Image.Resampling.LANCZOS)
    scene.alpha_composite(xp_frame, (0, 1028))
    draw = ImageDraw.Draw(scene)
    draw.rectangle((96, 1049, 1470, 1062), fill=(30, 208, 58, 240))
    draw_centered(draw, (8, 1034, 88, 1073), "LV 18", FONT_18, (246, 202, 92, 255))
    draw_centered(draw, (96, 1043, 1890, 1068), "7,420 / 10,000 XP", FONT_12, (226, 228, 202, 255))

    output = OUTPUT_DIR / f"Rogue10m_AppliedBottomHUD_{theme}.png"
    scene.convert("RGB").save(output, quality=95)
    return output


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    outputs = [render_theme(theme, spec) for theme, spec in THEMES.items()]

    comparison = Image.new("RGB", (1920, 1080), (4, 6, 7))
    draw = ImageDraw.Draw(comparison)
    for index, (theme, spec) in enumerate(THEMES.items()):
        preview = Image.open(outputs[index]).convert("RGB").resize((960, 540), Image.Resampling.LANCZOS)
        x = (index % 2) * 960
        y = (index // 2) * 540
        comparison.paste(preview, (x, y))
        draw.rounded_rectangle((x + 18, y + 16, x + 148, y + 50), radius=5, fill=(5, 7, 8, 218), outline=(154, 126, 72), width=2)
        draw_centered(draw, (x + 18, y + 16, x + 148, y + 50), spec["label"], FONT_18, (244, 210, 126))
    comparison_path = OUTPUT_DIR / "Rogue10m_AppliedBottomHUD_Comparison.png"
    comparison.save(comparison_path, quality=95)

    for output in outputs:
        print(output)
    print(comparison_path)


if __name__ == "__main__":
    main()
