"""Prepare magic VFX data and a clearly OFFLINE UV sheet, without Unreal or MCP."""
from pathlib import Path
import argparse
import contextlib
import hashlib
import io
import json
import textwrap

from PIL import Image, ImageDraw, ImageFont
import MagicParticleCandidateSpec20261110 as recipe


ROOT = Path(__file__).resolve().parents[1]


def font(size):
    path = Path("C:/Windows/Fonts/segoeui.ttf")
    return ImageFont.truetype(str(path), size) if path.is_file() else ImageFont.load_default()


def make_sheet(spec, path):
    image = Image.new("RGB", (940, 1020), (12, 18, 28))
    draw = ImageDraw.Draw(image)
    draw.text((22, 16), "OFFLINE UV FORMULA PREVIEW - NOT AN UNREAL CAPTURE", font=font(22), fill=(238, 193, 107))
    draw.text((22, 48), "Visual trial values only | no Niagara/shader compile | no skill binding or gameplay validation", font=font(15), fill=(178, 192, 210))
    ages = (.15, .35, .70)
    for column, age in enumerate(ages):
        draw.text((268 + column * 220, 80), f"normalized age {age:.2f}", font=font(16), fill=(195, 211, 231))
    for row, effect in enumerate(spec["effects"]):
        top = 112 + row * 220
        draw.text((22, top + 4), effect["id"].upper(), font=font(21), fill=(222, 235, 248))
        size = effect["sprite_size_cm"]
        draw.text((22, top + 38), f"Trial {size[0]:g} x {size[1]:g} cm / {effect['particle_lifetime_world_seconds']:.2f}s", font=font(15), fill=(161, 184, 204))
        description = effect["description"]
        for line_number, line in enumerate(textwrap.wrap(description, width=27)[:7]):
            draw.text((22, top + 69 + line_number * 18), line, font=font(14), fill=(137, 157, 180))
        tint = effect["material"]["tint_linear_rgb"]
        gain = effect["material"]["emissive_gain"]
        for column, age in enumerate(ages):
            panel = Image.new("RGB", (200, 200))
            pixels = []
            for y in range(200):
                for x in range(200):
                    alpha = recipe.evaluate_mask(effect["id"], (x + .5) / 200, (y + .5) / 200, age)
                    # Approximate display transfer for a diagnostic image; no UE exposure/bloom model.
                    pixels.append(tuple(round(min(1, .004 + color * gain * alpha) ** (1 / 2.2) * 255) for color in tint))
            panel.putdata(pixels)
            left = 268 + column * 220
            image.paste(panel, (left, top))
            draw.rectangle((left, top, left + 199, top + 199), outline=(46, 67, 87))
    draw.text((22, 1000), "Camera-facing sprite masks. Floating sigil is not a ground area, target indicator or damage boundary.", font=font(14), fill=(176, 192, 209))
    image.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    spec = recipe.build_spec(args.label)
    output = ROOT / "Feature/doc/evidence/magic-particle-candidates-20261110/preparation" / args.label
    if not output.resolve().is_relative_to(ROOT) or output.exists():
        raise RuntimeError("Use a fresh label inside this checkout; previous evidence is preserved.")
    log = io.StringIO()
    with contextlib.redirect_stdout(log):
        tests = recipe.run_tests()
    inputs = {}
    for kind in ("system", "material"):
        package = spec["source"][kind]
        file = ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")
        inputs[kind] = {"path": package, "file_exists": file.is_file(),
                        "sha256": hashlib.sha256(file.read_bytes()).hexdigest() if file.is_file() else None}
    output.mkdir(parents=True, exist_ok=False)
    (output / "prepared-spec.json").write_text(json.dumps(spec, indent=2), encoding="utf-8")
    (output / "preparation.log").write_text(log.getvalue(), encoding="utf-8")
    report = {"status": "offline_preparation_passed_editor_handoff_pending", "tests": tests,
              "unreal_imported": False, "editor_started": False, "assets_created": False,
              "hlsl_compiled": False, "screen_or_runtime_verified": False, "skill_connected": False,
              "source_file_snapshot_only": inputs,
              "spec_tool_sha256": hashlib.sha256(Path(recipe.__file__).read_bytes()).hexdigest(),
              "preview_tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "preview_meaning": "CPU evaluation of UV formulas, approximate display transfer; not an Unreal capture",
              "pending": ["Coordinator explicitly grants editor turn after map/build work",
                          "Fresh MCP reachability, project identity, skills, source topology and property schemas",
                          "Duplicate four materials/systems into fresh candidate path; compile, save, read back",
                          "Capture actual Niagara/editor and scene context; verify lifetime and screen readability"]}
    (output / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    make_sheet(spec, output / "offline-uv-sheet.png")
    print(log.getvalue().strip())
    print("PREPARED=" + str(output))


if __name__ == "__main__":
    main()
