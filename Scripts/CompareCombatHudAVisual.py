"""Check the approved, opaque HUD art against a ready-state 1672x941 runtime capture."""

import argparse
import json
from pathlib import Path
from statistics import correlation, mean

from PIL import Image

from PrepareCombatHudAAssets import BAKED_CAPTIONS, LIVE_CAPTION_BANDS, box

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "SourceArt/UI/CombatHudA"
STABLE_SPRITES = (
    "SlotQ", "SlotE", "SlotR", "SlotRMB", "SlotChord", "SlotCharge", "SlotAlt",
    "SlotLock1", "SlotLock2", "SlotLock3", "SlotLock4", "PlateCtrl",
)


def contains(rect, point):
    return rect is not None and rect[0] <= point[0] < rect[2] and rect[1] <= point[1] < rect[3]


def compare(capture_path, lock_badges, no_identity_key=False):
    layout = json.loads((ART / "layout.json").read_text(encoding="utf-8"))
    atlas = Image.open(ART / "T_HudA_Atlas.png").convert("RGBA")
    approved = Image.open(ROOT / layout["source"]).convert("RGB")
    capture = Image.open(capture_path).convert("RGB")
    if capture.size != tuple(layout["canvas"]):
        raise ValueError(f"Expected the approved canvas size, got {capture.size}")

    reference_pixels = atlas.convert("L").load()
    ready_pixels = capture.convert("L").load()
    opaque_count = 0
    edited_caption_pixels = 0
    live_count = 0
    live_exact = 0
    hidden_identity_pixels = 0
    hidden_identity_matches = 0
    samples = {}
    for name in STABLE_SPRITES:
        region = layout["sprites"][name]
        left, top, width, height = region["atlas"]
        screen_x, screen_y, _, _ = region["screen"]
        # The live corner badge covers a 20-by-24 rectangle in this slot.
        badge_left, badge_top = width - 26, 49
        caption = box(BAKED_CAPTIONS[name][0]) if name in BAKED_CAPTIONS else None
        live_caption = box(LIVE_CAPTION_BANDS[name]) if name in LIVE_CAPTION_BANDS else None
        show_sprite = not no_identity_key or name != "PlateCtrl"
        points, intensities = [], []
        for y in range(height):
            for x in range(width):
                atlas_point = (left + x, top + y)
                color = atlas.getpixel(atlas_point)
                if color[3] != 255:
                    continue
                screen_point = (screen_x + x, screen_y + y)
                if color[:3] != approved.getpixel(screen_point):
                    if not contains(caption, screen_point):
                        raise ValueError(f"{name}: atlas differs from approved art at {screen_point}")
                    edited_caption_pixels += 1
                else:
                    opaque_count += 1
                if not show_sprite:
                    hidden_identity_pixels += 1
                    hidden_identity_matches += color[:3] == capture.getpixel(screen_point)
                    continue
                if contains(live_caption, screen_point):
                    continue
                is_badge = (name in lock_badges and badge_left <= x < badge_left + 20
                            and badge_top <= y < badge_top + 24)
                if not is_badge:
                    live_count += 1
                    live_exact += color[:3] == capture.getpixel(screen_point)
                if x % 2 == 0 and y % 2 == 0:
                    points.append(screen_point)
                    intensities.append((reference_pixels[atlas_point], is_badge))
        if show_sprite:
            samples[name] = points, intensities
    print(f"Approved source pixels preserved in atlas: {opaque_count}")
    print(f"Intentionally changed opaque pixels inside edited captions: {edited_caption_pixels}")
    if no_identity_key:
        print(f"Hidden identity key original pixels visible: {hidden_identity_matches}/{hidden_identity_pixels}")
    print(f"Exact runtime opaque pixels outside live captions and active badges: {live_exact}/{live_count}")

    def similarity(name, dx, dy, include_badge=False):
        points, reference = samples[name]
        values = [(ready_pixels[x + dx, y + dy], value)
                  for (x, y), (value, is_badge) in zip(points, reference)
                  if include_badge or not is_badge]
        actual, baseline = zip(*values)
        return correlation(actual, baseline) if min(actual) != max(actual) else 0.0

    average, dx, dy = max(
        (mean(similarity(name, x, y) for name in samples), x, y)
        for x in range(-6, 7)
        for y in range(-6, 7)
    )
    print(f"Common offset ({dx:+d}, {dy:+d}) pixels; mean correlation {average:.4f}")
    passed = ((dx, dy) == (0, 0) and live_exact == live_count
              and (not no_identity_key or hidden_identity_matches < hidden_identity_pixels / 10))
    for name in samples:
        score = similarity(name, dx, dy)
        _, best_x, best_y = max(
            (similarity(name, x, y), x, y)
            for x in range(dx - 3, dx + 4)
            for y in range(dy - 3, dy + 4)
        )
        residual = best_x - dx, best_y - dy
        badge_report = (f" including live lock badge r={similarity(name, dx, dy, include_badge=True):.4f}"
                        if name in lock_badges else "")
        print(f"{name:11} r={score:.4f} residual={residual}{badge_report}")
        passed &= score >= 0.95 and max(map(abs, residual)) <= 1
    return passed


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--lock-badge", action="append", choices=STABLE_SPRITES[:6], default=[],
                        help="Report this slot's live lock badge separately from the fixed art.")
    parser.add_argument("--no-identity-key", "--no-ctrl", action="store_true",
                        help="Verify the key badge is hidden for a weapon without an active identity action.")
    args = parser.parse_args()
    raise SystemExit(0 if compare(args.capture, args.lock_badge, args.no_identity_key) else 1)
