"""Trim and resize the generated transparent XP/level bar to 1920px wide."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT_ROOT / "Content/UI/HUD/Gothic/Source/T_HUD_GothicXPLevelBar_AlphaUntrimmed.png"
OUTPUT = PROJECT_ROOT / "Content/UI/HUD/Gothic/T_HUD_GothicXPLevelBar.png"
TARGET_WIDTH = 1920
PADDING = 8


def main() -> None:
    image = Image.open(SOURCE).convert("RGBA")
    alpha = image.getchannel("A")
    bounds = alpha.getbbox()
    if not bounds:
        raise RuntimeError("Generated XP/level bar contains no visible pixels")

    left, top, right, bottom = bounds
    padded_bounds = (
        max(0, left - PADDING),
        max(0, top - PADDING),
        min(image.width, right + PADDING),
        min(image.height, bottom + PADDING),
    )
    cropped = image.crop(padded_bounds)
    target_height = max(1, round(cropped.height * TARGET_WIDTH / cropped.width))
    final = cropped.resize((TARGET_WIDTH, target_height), Image.Resampling.LANCZOS)
    final.save(OUTPUT)

    corner_alpha = [
        final.getpixel((0, 0))[3],
        final.getpixel((final.width - 1, 0))[3],
        final.getpixel((0, final.height - 1))[3],
        final.getpixel((final.width - 1, final.height - 1))[3],
    ]
    visible_bounds = final.getchannel("A").getbbox()
    if any(corner_alpha):
        raise RuntimeError(f"Expected transparent corners, got alpha={corner_alpha}")
    if final.width != TARGET_WIDTH or not visible_bounds:
        raise RuntimeError("Final XP/level bar validation failed")

    print(
        {
            "source_size": image.size,
            "crop_bounds": padded_bounds,
            "final_size": final.size,
            "corner_alpha": corner_alpha,
            "visible_bounds": visible_bounds,
        }
    )


if __name__ == "__main__":
    main()
