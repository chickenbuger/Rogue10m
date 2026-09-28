"""Prepare transparent slot-wing and bottom XP assets for the refined Gothic HUD."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
GOTHIC_DIR = PROJECT_ROOT / "Content/UI/HUD/Gothic"
GENERATED_SHEET = (
    PROJECT_ROOT / "tmp/imagegen/gothic_hud_layout_refinement/slot_wings_alpha.png"
)


def trim_alpha(image: Image.Image, padding: int = 8) -> Image.Image:
    bounds = image.getchannel("A").getbbox()
    if not bounds:
        raise RuntimeError("Generated slot wing has no visible alpha pixels")
    left, top, right, bottom = bounds
    return image.crop(
        (
            max(0, left - padding),
            max(0, top - padding),
            min(image.width, right + padding),
            min(image.height, bottom + padding),
        )
    )


def save_slot_wings() -> None:
    source = Image.open(GENERATED_SHEET).convert("RGBA")
    regions = {
        "T_HUD_GothicSkillWing.png": (0, 300, 790, 642),
        "T_HUD_GothicItemWing.png": (884, 300, source.width, 642),
    }
    for filename, bounds in regions.items():
        wing = trim_alpha(source.crop(bounds)).resize(
            (960, 208), Image.Resampling.LANCZOS
        )
        # Rebalance the generated art to the existing five-slot bindings. The
        # decorative end occupies 200 logical pixels, while the functional five
        # slots retain their established 54px pitch in the remaining 280px.
        if filename == "T_HUD_GothicSkillWing.png":
            ornament = wing.crop((0, 0, 200, 208)).resize(
                (400, 208), Image.Resampling.LANCZOS
            )
            slots = wing.crop((200, 0, 960, 208)).resize(
                (560, 208), Image.Resampling.LANCZOS
            )
            wing = Image.new("RGBA", (960, 208), (0, 0, 0, 0))
            wing.alpha_composite(ornament, (0, 0))
            wing.alpha_composite(slots, (400, 0))
        else:
            slots = wing.crop((0, 0, 760, 208)).resize(
                (560, 208), Image.Resampling.LANCZOS
            )
            ornament = wing.crop((760, 0, 960, 208)).resize(
                (400, 208), Image.Resampling.LANCZOS
            )
            wing = Image.new("RGBA", (960, 208), (0, 0, 0, 0))
            wing.alpha_composite(slots, (0, 0))
            wing.alpha_composite(ornament, (560, 0))
        wing.save(GOTHIC_DIR / filename)


def save_bottom_xp_frame() -> None:
    source = Image.open(GOTHIC_DIR / "T_HUD_GothicXPLevelBarCompact.png").convert(
        "RGBA"
    )
    # The original 1920x112 asset already contains a full-width track and a level
    # medallion. Compress only the vertical dimension into the final 52px strip;
    # it preserves the full horizontal ornamental run while making the widget
    # itself touch the viewport bottom edge.
    compact = source.resize((1920, 52), Image.Resampling.LANCZOS)
    compact.save(GOTHIC_DIR / "T_HUD_GothicXPBottomFrame.png")


def main() -> None:
    if not GENERATED_SHEET.exists():
        raise RuntimeError(f"Missing generated sheet: {GENERATED_SHEET}")
    save_slot_wings()
    save_bottom_xp_frame()
    for name in (
        "T_HUD_GothicSkillWing.png",
        "T_HUD_GothicItemWing.png",
        "T_HUD_GothicXPBottomFrame.png",
    ):
        path = GOTHIC_DIR / name
        with Image.open(path) as image:
            print(f"{name}: {image.size}, mode={image.mode}")


if __name__ == "__main__":
    main()
