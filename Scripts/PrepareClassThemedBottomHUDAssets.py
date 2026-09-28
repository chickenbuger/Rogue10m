"""Build fixed-layout transparent bottom-HUD frame textures for four combat themes."""

from __future__ import annotations

from pathlib import Path
from typing import Iterable

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps


PROJECT_ROOT = Path("D:/Project/Rogue10m")
GOTHIC_DIR = PROJECT_ROOT / "Content/UI/HUD/Gothic"
OUTPUT_DIR = PROJECT_ROOT / "Content/UI/HUD/ClassThemes"
CANVAS_SIZE = (1920, 208)

THEMES = {
    "Wizard": {
        "shadow": (8, 10, 18),
        "mid": (48, 54, 82),
        "highlight": (152, 128, 244),
        "accent": (108, 78, 230, 230),
        "secondary": (76, 174, 255, 225),
    },
    "Warrior": {
        "shadow": (16, 10, 8),
        "mid": (74, 46, 28),
        "highlight": (222, 164, 78),
        "accent": (184, 50, 30, 230),
        "secondary": (245, 178, 68, 230),
    },
    "MartialArtist": {
        "shadow": (8, 13, 10),
        "mid": (42, 66, 45),
        "highlight": (194, 162, 72),
        "accent": (56, 178, 116, 230),
        "secondary": (244, 180, 54, 230),
    },
    "Rogue": {
        "shadow": (8, 10, 10),
        "mid": (38, 56, 52),
        "highlight": (138, 166, 158),
        "accent": (72, 190, 104, 230),
        "secondary": (142, 74, 178, 225),
    },
}


def load_rgba(filename: str) -> Image.Image:
    path = GOTHIC_DIR / filename
    if not path.exists():
        raise FileNotFoundError(path)
    return Image.open(path).convert("RGBA")


def resize(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    return image.resize(size, Image.Resampling.LANCZOS)


def tint_texture(image: Image.Image, shadow, mid, highlight) -> Image.Image:
    alpha = image.getchannel("A")
    gray = ImageOps.grayscale(image)
    gray = ImageEnhance.Contrast(gray).enhance(1.18)
    colored = ImageOps.colorize(gray, black=shadow, mid=mid, white=highlight)
    colored.putalpha(alpha)
    return colored


def composite_scaled(
    canvas: Image.Image,
    source: Image.Image,
    box: tuple[int, int, int, int],
    palette,
) -> None:
    x, y, width, height = box
    texture = tint_texture(
        resize(source, (width, height)),
        palette["shadow"],
        palette["mid"],
        palette["highlight"],
    )
    canvas.alpha_composite(texture, (x, y))


def draw_glow_line(
    canvas: Image.Image,
    points: Iterable[tuple[int, int]],
    color: tuple[int, int, int, int],
    width: int = 2,
) -> None:
    points = list(points)
    glow = Image.new("RGBA", CANVAS_SIZE, (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)
    glow_draw.line(points, fill=(*color[:3], 86), width=width + 8, joint="curve")
    glow = glow.filter(ImageFilter.GaussianBlur(6))
    canvas.alpha_composite(glow)
    draw = ImageDraw.Draw(canvas)
    draw.line(points, fill=color, width=width, joint="curve")


def draw_diamond(draw: ImageDraw.ImageDraw, center, radius, color, width=2) -> None:
    x, y = center
    points = [(x, y - radius), (x + radius, y), (x, y + radius), (x - radius, y)]
    draw.line(points + [points[0]], fill=color, width=width, joint="curve")


def draw_wizard_emblem(canvas: Image.Image, palette) -> None:
    draw = ImageDraw.Draw(canvas)
    accent = palette["accent"]
    secondary = palette["secondary"]
    for radius, alpha in ((53, 110), (43, 150), (31, 210)):
        draw.ellipse((960 - radius, 82 - radius, 960 + radius, 82 + radius), outline=(*accent[:3], alpha), width=2)
    draw_diamond(draw, (960, 82), 28, secondary, 3)
    draw.line((960, 44, 960, 120), fill=accent, width=2)
    draw.line((922, 82, 998, 82), fill=accent, width=2)


def draw_warrior_emblem(canvas: Image.Image, palette) -> None:
    draw = ImageDraw.Draw(canvas)
    accent = palette["accent"]
    secondary = palette["secondary"]
    shield = [(960, 42), (992, 56), (986, 103), (960, 129), (934, 103), (928, 56)]
    draw.line(shield + [shield[0]], fill=secondary, width=4, joint="curve")
    draw.line((960, 36, 960, 124), fill=accent, width=5)
    draw.polygon([(960, 30), (953, 44), (967, 44)], fill=secondary)
    draw.line((944, 66, 976, 66), fill=secondary, width=4)


def draw_martial_artist_emblem(canvas: Image.Image, palette) -> None:
    draw = ImageDraw.Draw(canvas)
    accent = palette["accent"]
    secondary = palette["secondary"]
    draw.ellipse((915, 37, 1005, 127), outline=secondary, width=4)
    draw.arc((924, 46, 996, 118), 32, 218, fill=accent, width=5)
    palm = (942, 68, 980, 111)
    draw.rounded_rectangle(palm, radius=11, outline=secondary, width=4)
    for index in range(4):
        x = 941 + index * 10
        draw.rounded_rectangle((x, 49 - index * 2, x + 9, 79), radius=4, fill=(*accent[:3], 205))
    draw.line((948, 107, 936, 122), fill=accent, width=5)


def draw_rogue_emblem(canvas: Image.Image, palette) -> None:
    draw = ImageDraw.Draw(canvas)
    accent = palette["accent"]
    secondary = palette["secondary"]
    draw.arc((916, 38, 1004, 126), 205, 335, fill=accent, width=4)
    draw.arc((916, 38, 1004, 126), 25, 155, fill=secondary, width=4)
    for direction in (-1, 1):
        draw.line((960 - 30 * direction, 49, 960 + 28 * direction, 116), fill=secondary, width=5)
        draw.polygon(
            [
                (960 + 28 * direction, 116),
                (960 + 18 * direction, 101),
                (960 + 36 * direction, 103),
            ],
            fill=accent,
        )


def build_theme(name: str, palette) -> Path:
    canvas = Image.new("RGBA", CANVAS_SIZE, (0, 0, 0, 0))

    skill_wing = load_rgba("T_HUD_GothicSkillWing.png")
    item_wing = load_rgba("T_HUD_GothicItemWing.png")
    bar_left = load_rgba("T_HUD_GothicBarLeft.png")
    bar_right = load_rgba("T_HUD_GothicBarRight.png")
    medallion = load_rgba("T_HUD_GothicMedallion.png")

    # Wide outer wings intentionally fill the space outside the functional slots.
    composite_scaled(canvas, skill_wing, (36, 102, 924, 106), palette)
    composite_scaled(canvas, item_wing, (960, 102, 924, 106), palette)
    composite_scaled(canvas, bar_left, (74, 52, 786, 76), palette)
    composite_scaled(canvas, bar_right, (1060, 52, 786, 76), palette)
    composite_scaled(canvas, medallion, (849, 0, 222, 197), palette)

    accent = palette["accent"]
    secondary = palette["secondary"]
    draw_glow_line(canvas, [(116, 116), (740, 116), (856, 152)], accent, 2)
    draw_glow_line(canvas, [(1064, 152), (1180, 116), (1804, 116)], secondary, 2)

    draw = ImageDraw.Draw(canvas)
    draw_diamond(draw, (78, 118), 9, accent, 2)
    draw_diamond(draw, (1842, 118), 9, secondary, 2)
    for x in (120, 174, 228, 282, 336, 1584, 1638, 1692, 1746, 1800):
        draw.line((x, 184, x + 20, 184), fill=(*palette["highlight"], 120), width=2)

    if name == "Wizard":
        draw_wizard_emblem(canvas, palette)
    elif name == "Warrior":
        draw_warrior_emblem(canvas, palette)
    elif name == "MartialArtist":
        draw_martial_artist_emblem(canvas, palette)
    else:
        draw_rogue_emblem(canvas, palette)

    output_path = OUTPUT_DIR / f"T_HUD_Bottom_{name}.png"
    canvas.save(output_path, optimize=True)
    return output_path


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, palette in THEMES.items():
        output = build_theme(name, palette)
        image = Image.open(output).convert("RGBA")
        alpha = image.getchannel("A")
        print(
            f"{output.name}: size={image.size}, alpha={alpha.getextrema()}, "
            f"bbox={alpha.getbbox()}"
        )


if __name__ == "__main__":
    main()
