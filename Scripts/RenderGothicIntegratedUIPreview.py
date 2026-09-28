"""Render a clean 1920x1080 preview from the exact applied Gothic UI textures."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parents[1]
GOTHIC_DIR = PROJECT_ROOT / "Content/UI/HUD/Gothic"
ATTACHED_BACKGROUND = PROJECT_ROOT / ".codex-remote-attachments/019fd19f-443e-7332-a6a9-febfaeb4e215/48c41036-3102-4496-94b5-14bbef3e1432/1-Photo-1.jpg"
PROJECT_BACKGROUND = PROJECT_ROOT / "Saved/Screenshots/WindowsEditor/HighresScreenshot00003.png"
OUTPUT = PROJECT_ROOT / "Saved/GothicUIAppliedPreview.png"
FONT_PATH = Path("C:/Windows/Fonts/malgun.ttf")
FONT_BOLD_PATH = Path("C:/Windows/Fonts/malgunbd.ttf")


def font(size: int, bold: bool = False):
    path = FONT_BOLD_PATH if bold else FONT_PATH
    return ImageFont.truetype(str(path), size=size)


def load_rgba(name: str) -> Image.Image:
    return Image.open(GOTHIC_DIR / name).convert("RGBA")


def paste_scaled(canvas: Image.Image, image: Image.Image, box: tuple[int, int, int, int]):
    x, y, width, height = box
    resized = image.resize((width, height), Image.Resampling.LANCZOS)
    canvas.alpha_composite(resized, (x, y))


def centered_text(draw, xy, text, text_font, fill, stroke=2):
    draw.text(
        xy,
        text,
        font=text_font,
        fill=fill,
        anchor="mm",
        stroke_width=stroke,
        stroke_fill=(4, 3, 2, 230),
    )


def draw_inventory(canvas: Image.Image, draw: ImageDraw.ImageDraw):
    x, y, width, height = 42, 210, 560, 610
    frame = load_rgba("T_UI_GothicInventoryFrame.png")
    paste_scaled(canvas, frame, (x, y, width, height))

    grid_left, grid_top = x + 116, y + 126
    cell = 33
    for row in range(10):
        for column in range(10):
            left = grid_left + column * cell
            top = grid_top + row * cell
            draw.rectangle(
                (left, top, left + cell - 2, top + cell - 2),
                fill=(7, 8, 10, 84),
                outline=(118, 84, 38, 108),
                width=1,
            )

    centered_text(
        draw,
        (x + width // 2, y + 64),
        "인벤토리",
        font(24, bold=True),
        (224, 193, 125, 255),
    )
    draw.text(
        (x + 122, y + 541),
        "골드  12,480",
        font=font(16),
        fill=(214, 187, 126, 255),
        stroke_width=1,
        stroke_fill=(0, 0, 0, 255),
    )
    draw.text(
        (x + 122, y + 577),
        "무게  34.5 / 100.0 kg",
        font=font(15),
        fill=(205, 194, 172, 255),
        stroke_width=1,
        stroke_fill=(0, 0, 0, 255),
    )


def draw_monster_info(canvas: Image.Image, draw: ImageDraw.ImageDraw):
    x, y, width, height = 750, 40, 420, 68
    draw.rounded_rectangle(
        (x + 72, y + 43, x + 351, y + 53),
        radius=4,
        fill=(22, 4, 3, 220),
    )
    draw.rounded_rectangle(
        (x + 72, y + 43, x + 72 + int(279 * 0.68), y + 53),
        radius=4,
        fill=(146, 14, 10, 245),
    )
    frame = load_rgba("T_HUD_GothicMonsterInfoFrame.png")
    paste_scaled(canvas, frame, (x, y, width, height))
    centered_text(
        draw,
        (x + width // 2, y + 27),
        "타락한 기사  ·  LV.18",
        font(15, bold=True),
        (229, 200, 139, 255),
        stroke=1,
    )
    centered_text(
        draw,
        (x + width // 2, y + 49),
        "6,840 / 10,000",
        font(10),
        (237, 218, 189, 255),
        stroke=1,
    )


def draw_bottom_hud(canvas: Image.Image, draw: ImageDraw.ImageDraw):
    xp = load_rgba("T_HUD_GothicXPLevelBarCompact.png")
    paste_scaled(canvas, xp, (0, 968, 1920, 112))
    draw.rounded_rectangle((118, 1016, 691, 1031), radius=6, fill=(121, 18, 10, 220))
    centered_text(draw, (54, 1025), "37", font(18, bold=True), (244, 208, 119, 255), stroke=1)
    centered_text(draw, (1010, 1024), "경험치  37 / 100", font(12), (218, 194, 150, 255), stroke=1)

    left = load_rgba("T_HUD_GothicBarLeft.png")
    right = load_rgba("T_HUD_GothicBarRight.png")
    medallion = load_rgba("T_HUD_GothicMedallion.png")
    slot = load_rgba("T_HUD_GothicSlotFrame.png")

    draw.rounded_rectangle((670, 939, 1016, 954), radius=6, fill=(154, 19, 14, 235))
    draw.rounded_rectangle((1162, 939, 1505, 954), radius=6, fill=(13, 91, 161, 235))
    paste_scaled(canvas, left, (638, 912, 410, 76))
    paste_scaled(canvas, right, (1142, 912, 410, 76))

    for index in range(5):
        paste_scaled(canvas, slot, (760 + index * 54, 978, 52, 52))
        paste_scaled(canvas, slot, (1160 + index * 54, 978, 52, 52))
    paste_scaled(canvas, medallion, (985, 841, 222, 197))


def main() -> None:
    background_path = ATTACHED_BACKGROUND if ATTACHED_BACKGROUND.exists() else PROJECT_BACKGROUND
    with Image.open(background_path) as source:
        background = source.convert("RGBA").resize((1920, 1080), Image.Resampling.LANCZOS)
    darkness = Image.new("RGBA", background.size, (0, 0, 0, 148))
    canvas = Image.alpha_composite(background, darkness)
    draw = ImageDraw.Draw(canvas, "RGBA")

    draw_inventory(canvas, draw)
    draw_monster_info(canvas, draw)
    draw_bottom_hud(canvas, draw)

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(OUTPUT, quality=94, subsampling=0)
    print(f"rendered {OUTPUT}: {canvas.width}x{canvas.height}")


if __name__ == "__main__":
    main()
