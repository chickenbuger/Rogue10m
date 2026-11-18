"""Offline PNG evidence helper. Writes fresh crops/metrics; never edits source PNGs.

This template check supplements visual review. It cannot prove that an arbitrary
background has no coincidental cross-shaped feature. Use the original PNGs too.
Requires Pillow; no network or engine access.
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


def read_text(path: Path) -> str:
    data = path.read_bytes()
    return data.decode("utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")


def assess(image: Image.Image, scale: float) -> dict:
    width, height = image.size
    pixels = image.load()
    expected = (width / 2.0, height / 2.0)

    def rgb(x: float, y: float) -> tuple[int, int, int]:
        return pixels[max(0, min(width - 1, int(math.floor(x)))), max(0, min(height - 1, int(math.floor(y))))]

    def cyan(value: tuple[int, int, int]) -> bool:
        red, green, blue = value
        return red >= 125 and green >= 150 and blue >= 160 and blue - red >= 8 and green >= red

    def dark(value: tuple[int, int, int]) -> bool:
        return max(value) <= 85

    candidates = []
    # Default native HUD: length16, gap6, thickness2 local Slate units.
    for oy in range(-12, 13):
        for ox in range(-12, 13):
            cx, cy = expected[0] + ox * 0.5, expected[1] + oy * 0.5
            arms, borders = [], []
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                middle, outline = [], []
                for step in range(12):
                    along = (8.0 + step) * scale
                    middle.append(cyan(rgb(cx + dx * along, cy + dy * along)))
                    # Native outline is two local units thicker than the color.
                    for side in (-1, 1):
                        normal = side * 1.65 * scale
                        outline.append(dark(rgb(cx + dx * along - dy * normal, cy + dy * along + dx * normal)))
                arms.append(sum(middle) / len(middle))
                borders.extend(outline)
            border_fraction = sum(borders) / len(borders)
            score = min(arms) * 0.7 + sum(arms) / 4.0 * 0.2 + border_fraction * 0.1
            candidates.append((score, cx, cy, arms, border_fraction))
    best = max(candidates, key=lambda item: item[0])
    peaks = [item for item in candidates if item[0] >= best[0] - 0.002]
    measured = (sum(item[1] for item in peaks) / len(peaks), sum(item[2] for item in peaks) / len(peaks))
    template_found = min(best[3]) >= 0.65 and best[4] >= 0.45
    return {
        "expected_center_px": list(expected),
        "estimated_cross_center_px": list(measured),
        "center_error_px": math.dist(expected, measured),
        "cyan_fraction_per_arm": best[3],
        "dark_outline_fraction": best[4],
        "template_score": best[0],
        "four_arm_outline_template_found": template_found,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--images", type=Path, required=True)
    parser.add_argument("--observations", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="Must not already exist.")
    args = parser.parse_args()
    rows = list(csv.DictReader(io.StringIO(read_text(args.observations))))
    if len(rows) != 5:
        raise ValueError("Expected five completed runtime observations.")
    args.output.mkdir(parents=True, exist_ok=False)
    records, failures = [], []
    for row in rows:
        name = row["screenshot"]
        if Path(name).name != name or not name.endswith(".png"):
            raise ValueError("Unexpected screenshot filename.")
        with Image.open(args.images / name) as original:
            original.load()
            image = original.convert("RGB")
        expected_size = (int(row["width"]), int(row["height"]))
        if image.size != expected_size:
            raise ValueError(f"PNG dimensions differ from runtime viewport: {name}")
        scale = float(row["ui_scale"])
        if not math.isfinite(scale) or not 0.1 <= scale <= 4:
            raise ValueError("Invalid recorded Slate scale.")
        result = assess(image, scale)
        visible = row["reticle_visible"] == "1"
        passed = (result["four_arm_outline_template_found"] and result["center_error_px"] <= 1.5) if visible else not result["four_arm_outline_template_found"]
        record = {"screenshot": name, "size": list(image.size), "expected_visible": visible,
                  "pixel_template_passed": passed, **result}
        records.append(record)
        if not passed:
            failures.append(name)
        # Keep raw crop as evidence. The marked version only adds viewport-center
        # guides in the margins; no guide is drawn over the reticle itself.
        cx, cy = image.width // 2, image.height // 2
        radius = max(32, math.ceil(30 * scale))
        crop = image.crop((cx - radius, cy - radius, cx + radius, cy + radius))
        enlarged = crop.resize((crop.width * 8, crop.height * 8), Image.Resampling.NEAREST)
        enlarged.save(args.output / (Path(name).stem + "_center_8x.png"))
        marked = enlarged.copy()
        draw = ImageDraw.Draw(marked)
        middle = marked.width // 2
        draw.line((middle, 0, middle, 10), fill=(255, 64, 32), width=2)
        draw.line((middle, marked.height - 11, middle, marked.height - 1), fill=(255, 64, 32), width=2)
        draw.line((0, middle, 10, middle), fill=(255, 64, 32), width=2)
        draw.line((marked.width - 11, middle, marked.width - 1, middle), fill=(255, 64, 32), width=2)
        marked.save(args.output / (Path(name).stem + "_center_guide_8x.png"))
    report = {"method": "default_native_cross_cyan_arms_black_outline_template",
              "scope": "offline_pixels_only_requires_visual_review_of_crops_and_original_PNGs",
              "checks": records, "failed_or_uncertain": failures}
    (args.output / "pixel-assessment.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"output": str(args.output), "failed_or_uncertain": failures}))
    return 2 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
