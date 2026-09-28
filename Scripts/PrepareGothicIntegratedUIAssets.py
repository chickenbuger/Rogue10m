"""Prepare runtime-sized Gothic inventory and experience UI textures."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
GOTHIC_DIR = PROJECT_ROOT / "Content/UI/HUD/Gothic"
SOURCE_DIR = GOTHIC_DIR / "Source"
INVENTORY_TRANSPARENT_SOURCE = SOURCE_DIR / "T_UI_GothicInventoryFrame_Transparent.png"
INVENTORY_OUTPUT = GOTHIC_DIR / "T_UI_GothicInventoryFrame.png"
XP_SOURCE = GOTHIC_DIR / "T_HUD_GothicXPLevelBar.png"
XP_COMPACT_OUTPUT = GOTHIC_DIR / "T_HUD_GothicXPLevelBarCompact.png"


def fit_inventory() -> None:
    with Image.open(INVENTORY_TRANSPARENT_SOURCE) as source:
        rgba = source.convert("RGBA")
    fitted = rgba.resize((1120, 1220), Image.Resampling.LANCZOS)
    fitted.save(INVENTORY_OUTPUT)


def build_compact_xp_bar() -> None:
    with Image.open(XP_SOURCE) as source:
        rgba = source.convert("RGBA")

    # Keep the level medallion square and scale the horizontal track independently.
    badge = rgba.crop((0, 0, 353, 353)).resize(
        (112, 112), Image.Resampling.LANCZOS
    )
    track = rgba.crop((250, 102, 1920, 262)).resize(
        (1828, 70), Image.Resampling.LANCZOS
    )

    output = Image.new("RGBA", (1920, 112), (0, 0, 0, 0))
    output.alpha_composite(track, (92, 21))
    output.alpha_composite(badge, (0, 0))
    output.save(XP_COMPACT_OUTPUT)


def validate() -> None:
    expected = {
        INVENTORY_OUTPUT: (1120, 1220),
        XP_COMPACT_OUTPUT: (1920, 112),
    }
    for path, size in expected.items():
        with Image.open(path) as image:
            if image.mode != "RGBA" or image.size != size:
                raise RuntimeError(
                    f"Unexpected image output {path}: mode={image.mode}, size={image.size}"
                )
            alpha = image.getchannel("A")
            minimum, maximum = alpha.getextrema()
            if minimum != 0 or maximum != 255:
                raise RuntimeError(f"Alpha range is invalid for {path}: {(minimum, maximum)}")
        print(f"validated {path.relative_to(PROJECT_ROOT)}: {size[0]}x{size[1]} RGBA")


def main() -> None:
    if not INVENTORY_TRANSPARENT_SOURCE.exists():
        raise RuntimeError(f"Missing transparent inventory source: {INVENTORY_TRANSPARENT_SOURCE}")
    if not XP_SOURCE.exists():
        raise RuntimeError(f"Missing XP source: {XP_SOURCE}")
    fit_inventory()
    build_compact_xp_bar()
    validate()


if __name__ == "__main__":
    main()
