"""Fit the generated transparent monster-info frame into an 840x136 UI canvas."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT_ROOT / "Content/UI/HUD/Gothic/Source/T_HUD_GothicMonsterInfoFrame_AlphaUntrimmed.png"
OUTPUT = PROJECT_ROOT / "Content/UI/HUD/Gothic/T_HUD_GothicMonsterInfoFrame.png"
TARGET_SIZE = (840, 136)
EDGE_PADDING = 8


def main() -> None:
    source = Image.open(SOURCE).convert("RGBA")
    bounds = source.getchannel("A").getbbox()
    if not bounds:
        raise RuntimeError("Generated monster-info frame contains no visible pixels")

    left, top, right, bottom = bounds
    crop_bounds = (
        max(0, left - EDGE_PADDING),
        max(0, top - EDGE_PADDING),
        min(source.width, right + EDGE_PADDING),
        min(source.height, bottom + EDGE_PADDING),
    )
    cropped = source.crop(crop_bounds)

    usable_width = TARGET_SIZE[0] - EDGE_PADDING * 2
    usable_height = TARGET_SIZE[1] - EDGE_PADDING * 2
    scale = min(usable_width / cropped.width, usable_height / cropped.height)
    fitted_size = (
        max(1, round(cropped.width * scale)),
        max(1, round(cropped.height * scale)),
    )
    fitted = cropped.resize(fitted_size, Image.Resampling.LANCZOS)
    final = Image.new("RGBA", TARGET_SIZE, (0, 0, 0, 0))
    position = (
        (TARGET_SIZE[0] - fitted.width) // 2,
        (TARGET_SIZE[1] - fitted.height) // 2,
    )
    final.alpha_composite(fitted, position)
    final.save(OUTPUT)

    corner_alpha = [
        final.getpixel((0, 0))[3],
        final.getpixel((final.width - 1, 0))[3],
        final.getpixel((0, final.height - 1))[3],
        final.getpixel((final.width - 1, final.height - 1))[3],
    ]
    visible_bounds = final.getchannel("A").getbbox()
    if any(corner_alpha) or not visible_bounds or final.size != TARGET_SIZE:
        raise RuntimeError("Final monster-info frame validation failed")

    print(
        {
            "source_size": source.size,
            "crop_bounds": crop_bounds,
            "cropped_size": cropped.size,
            "fitted_size": fitted_size,
            "final_size": final.size,
            "corner_alpha": corner_alpha,
            "visible_bounds": visible_bounds,
        }
    )


if __name__ == "__main__":
    main()
