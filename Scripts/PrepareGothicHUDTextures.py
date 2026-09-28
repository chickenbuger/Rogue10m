"""Derive reusable transparent HUD pieces from the generated gothic frame source."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT_ROOT / "Content/UI/HUD/Gothic/T_HUD_GothicFrame.png"
OUTPUT_DIR = PROJECT_ROOT / "Content/UI/HUD/Gothic"

# Coordinates target the retained 1774x592 generated source. Each crop includes
# enough transparent padding for antialiased ornament edges.
CROPS = {
    "T_HUD_GothicBarLeft.png": (20, 218, 765, 355),
    "T_HUD_GothicBarRight.png": (1009, 218, 1754, 355),
    "T_HUD_GothicMedallion.png": (626, 0, 1148, 475),
    "T_HUD_GothicExperienceFrame.png": (35, 472, 1739, 592),
    "T_HUD_GothicSlotFrame.png": (96, 342, 252, 493),
}


def trim_alpha(image: Image.Image, padding: int = 4) -> Image.Image:
    alpha_bounds = image.getchannel("A").getbbox()
    if not alpha_bounds:
        raise RuntimeError("Crop contains no visible pixels")
    left, top, right, bottom = alpha_bounds
    return image.crop(
        (
            max(0, left - padding),
            max(0, top - padding),
            min(image.width, right + padding),
            min(image.height, bottom + padding),
        )
    )


def main() -> None:
    source = Image.open(SOURCE).convert("RGBA")
    if source.size != (1774, 592):
        raise RuntimeError(f"Unexpected source size: {source.size}")

    for filename, bounds in CROPS.items():
        output = trim_alpha(source.crop(bounds))
        output_path = OUTPUT_DIR / filename
        output.save(output_path)
        print(f"{filename}: {output.size}")


if __name__ == "__main__":
    main()
