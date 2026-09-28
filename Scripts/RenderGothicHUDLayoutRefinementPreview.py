"""Render the applied 1920x1080 Gothic HUD layout for handoff review."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parents[1]
GOTHIC_DIR = PROJECT_ROOT / "Content/UI/HUD/Gothic"
REFERENCE = (
    PROJECT_ROOT
    / ".codex-remote-attachments/019fd19f-443e-7332-a6a9-febfaeb4e215"
    / "48c41036-3102-4496-94b5-14bbef3e1432/1-Photo-1.jpg"
)
if not REFERENCE.exists():
    REFERENCE = PROJECT_ROOT / "Saved/Screenshots/WindowsEditor/HighresScreenshot00003.png"
OUTPUT = PROJECT_ROOT / "Saved/GothicHUDLayoutRefinedPreview.png"
FONT = Path("C:/Windows/Fonts/malgun.ttf")
FONT_BOLD = Path("C:/Windows/Fonts/malgunbd.ttf")


def font(size: int, bold: bool = False):
    return ImageFont.truetype(str(FONT_BOLD if bold else FONT), size=size)


def texture(name: str) -> Image.Image:
    return Image.open(GOTHIC_DIR / name).convert("RGBA")


def paste(canvas: Image.Image, image: Image.Image, box: tuple[int, int, int, int]):
    x, y, width, height = box
    canvas.alpha_composite(
        image.resize((width, height), Image.Resampling.LANCZOS), (x, y)
    )


def centered(draw, xy, text, size, fill, bold=False):
    draw.text(
        xy,
        text,
        font=font(size, bold),
        fill=fill,
        anchor="mm",
        stroke_width=2,
        stroke_fill=(0, 0, 0, 230),
    )


def main() -> None:
    with Image.open(REFERENCE) as source:
        canvas = source.convert("RGBA").resize((1920, 1080), Image.Resampling.LANCZOS)
    canvas = Image.alpha_composite(canvas, Image.new("RGBA", canvas.size, (0, 0, 0, 76)))
    draw = ImageDraw.Draw(canvas, "RGBA")

    # Runtime vitals: health is red, stamina is warm gold.
    draw.rounded_rectangle((580, 909, 940, 925), 7, fill=(48, 7, 5, 245))
    draw.rounded_rectangle((580, 909, 875, 925), 7, fill=(194, 18, 12, 245))
    draw.rounded_rectangle((980, 909, 1340, 925), 7, fill=(45, 29, 3, 245))
    draw.rounded_rectangle((980, 909, 1225, 925), 7, fill=(224, 145, 7, 245))
    paste(canvas, texture("T_HUD_GothicBarLeft.png"), (540, 882, 420, 76))
    paste(canvas, texture("T_HUD_GothicBarRight.png"), (960, 882, 420, 76))
    centered(draw, (760, 916), "HEALTH  820 / 1000", 11, (244, 220, 190, 255), True)
    centered(draw, (1160, 916), "STAMINA  68 / 100", 11, (244, 220, 190, 255), True)

    # Empty outer areas are now full ornamental wings; existing five functional
    # slots remain toward the center on each side.
    paste(canvas, texture("T_HUD_GothicSkillWing.png"), (480, 936, 480, 104))
    paste(canvas, texture("T_HUD_GothicItemWing.png"), (960, 936, 480, 104))
    paste(canvas, texture("T_HUD_GothicMedallion.png"), (849, 812, 222, 197))

    # The XP strip occupies the complete viewport width and touches y=1080.
    draw.rounded_rectangle((96, 1049, 1896, 1062), 6, fill=(5, 30, 10, 246))
    draw.rounded_rectangle((96, 1049, 762, 1062), 6, fill=(10, 190, 43, 250))
    paste(canvas, texture("T_HUD_GothicXPBottomFrame.png"), (0, 1028, 1920, 52))
    centered(draw, (49, 1054), "37", 15, (246, 214, 132, 255), True)
    centered(draw, (1010, 1054), "EXPERIENCE  37 / 100", 10, (231, 221, 188, 255))

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(OUTPUT, quality=95, subsampling=0)
    print(f"rendered {OUTPUT}: {canvas.size}")


if __name__ == "__main__":
    main()
