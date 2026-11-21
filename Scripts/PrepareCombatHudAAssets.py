"""Extract the approved Layout A HUD artwork into a deterministic UI atlas."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
from pathlib import Path
from statistics import median

from PIL import Image, ImageDraw, ImageFilter, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Feature/doc/evidence/main-combat-hud-concept-20261121/layout-a-single-row.png"
SOURCE_SHA256 = "4d6578d6ea2d9d404521d86497365e5b62f083914a4ce11e23346f4be3943ac9"
OUTPUT = ROOT / "SourceArt/UI/CombatHudA"
CANVAS = (1672, 941)
ATLAS = (1024, 512)
PADDING = 4

# All positions are measured in pixels of the original accepted PNG.
HEALTH = (31, 27, 281, 39)
STAMINA = (31, 73, 281, 41)
LEVEL = (31, 120, 67, 29)
EXPERIENCE = (101, 123, 211, 21)
ALT = (418, 774, 67, 84)
ATTACKS = {
    "SlotQ": (537, 763, 85, 94),
    "SlotE": (627, 762, 86, 95),
    "SlotR": (716, 762, 87, 95),
    "SlotRMB": (808, 762, 86, 95),
    "SlotChord": (899, 762, 86, 95),
    "SlotCharge": (989, 762, 90, 95),
}
LOCKS = [(678 + 64 * index, 871, 59, 36) for index in range(4)]
IDENTITY = (1118, 737, 160, 160)
IDENTITY_CENTER = (1197.7, 816.6)
CTRL = (1265, 850, 59, 35)

# Each caption rectangle contains the original antialiased letters and their
# outline, but stays inside the metal frame. The adjacent sample supplies the
# same row lighting underneath the old letters.
BAKED_CAPTIONS = {
    "PlateExp": ((103, 126, 33, 16), (134, 126, 4, 16)),
    "SlotQ": ((570, 833, 19, 20), (550, 833, 13, 20)),
    "SlotE": ((662, 833, 17, 18), (641, 833, 13, 18)),
    "SlotR": ((752, 833, 18, 19), (731, 833, 13, 19)),
    "SlotRMB": ((830, 833, 45, 18), (819, 833, 9, 18)),
    "SlotChord": ((923, 833, 38, 18), (910, 833, 11, 18)),
    "SlotCharge": ((993, 833, 82, 18), (546, 833, 17, 18)),
    "SlotAlt": ((438, 834, 28, 18), (429, 834, 7, 18)),
    "PlateCtrl": ((1277, 857, 36, 20), (1273, 857, 5, 20)),
}


def box(rect):
    x, y, width, height = rect
    return x, y, x + width, y + height


def outside(rect, pad=PADDING):
    x, y, width, height = rect
    return x - pad, y - pad, width + pad * 2, height + pad * 2


def bevel(rect, corner):
    x, y, right, bottom = box(rect)
    right -= 1
    bottom -= 1
    return [(x + corner, y), (right - corner, y), (right, y + corner),
            (right, bottom - corner), (right - corner, bottom), (x + corner, bottom),
            (x, bottom - corner), (x, y + corner)]


def mask(rect, points=None, oval=None, round_radius=0):
    width, height = rect[2:]
    factor = 4
    result = Image.new("L", (width * factor, height * factor))
    draw = ImageDraw.Draw(result)
    x, y = rect[:2]
    if oval:
        coords = tuple(round((value - origin) * factor) for value, origin in zip(oval, (x, y, x, y)))
        draw.ellipse(coords, fill=255)
    elif round_radius:
        draw.rounded_rectangle((0, 0, width * factor - 1, height * factor - 1),
                               radius=round_radius * factor, fill=255)
    else:
        draw.polygon([(round((px - x) * factor), round((py - y) * factor))
                      for px, py in points], fill=255)
    return result.resize((width, height), Image.Resampling.LANCZOS)


def cutout(source, rect, points=None, oval=None):
    full_rect = outside(rect, 5)
    alpha = mask(full_rect, points=points, oval=oval)
    # A separate alpha shadow matches the reference's bevel relief without
    # carrying any opaque rendered ground or background into the UI texture.
    shifted = Image.new("L", alpha.size)
    shifted.paste(alpha, (0, 2))
    shadow = shifted.filter(ImageFilter.GaussianBlur(2.2)).point(lambda value: (value * 144) // 255)
    result = Image.new("RGBA", alpha.size, (12, 8, 5, 0))
    result.putalpha(shadow)
    foreground = source.crop(box(full_rect)).convert("RGBA")
    foreground.putalpha(alpha)
    result.alpha_composite(foreground)
    return full_rect, result


def replace_from_patch(target, source, rect, patch_rect, fill_mask=None, blur=0.5):
    """Clone local charcoal pixels over labels or filled portions of live plates."""
    tile = source.crop(box(patch_rect))
    size = rect[2:]
    field = Image.new("RGB", size)
    for row, top in enumerate(range(0, size[1], tile.height)):
        stripe = tile if row % 2 == 0 else ImageOps.flip(tile)
        reflected = ImageOps.mirror(stripe)
        for column, left in enumerate(range(0, size[0], tile.width)):
            field.paste(stripe if column % 2 == 0 else reflected, (left, top))
    if not fill_mask:
        fill_mask = Image.new("L", size, 255)
    target.paste(field, rect[:2], fill_mask.filter(ImageFilter.GaussianBlur(blur)))


def caption_ink(source, rect):
    area = source.crop(box(rect))
    pixels = area.load()
    ink = Image.new("L", area.size)
    for y in range(area.height):
        for x in range(area.width):
            pixel = pixels[x, y]
            if min(pixel) >= 88 and max(pixel) - min(pixel) < 32:
                ink.putpixel((x, y), 255)
    return ink


def shrink_caption(target, source, rect, patch_rect):
    ink = caption_ink(source, rect)
    if ink.getbbox() is None:
        raise ValueError(f"No caption pixels at {rect}")

    replace_from_patch(target, source, rect, patch_rect, ink.filter(ImageFilter.MaxFilter(7)), blur=1.0)
    image = source.crop(box(rect)).convert("RGBA")
    image.putalpha(ink.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(0.45)))
    image_bounds = image.getbbox()
    image = image.crop(image_bounds)
    height = image.height - 2
    width = round(image.width * height / image.height)
    scaled = image.resize((width, height), Image.Resampling.LANCZOS)
    left = rect[0] + round((image_bounds[0] + image_bounds[2] - scaled.width) / 2)
    top = rect[1] + round((image_bounds[1] + image_bounds[3] - scaled.height) / 2)
    target.paste(scaled, (left, top), scaled.getchannel("A"))


def bar_fill(source, rect, sample_x, exp=False, mana=False):
    colors = []
    for y in range(rect[1], rect[1] + rect[3]):
        color = tuple(int(median(source.getpixel((x, y))[channel] for x in sample_x))
                      for channel in range(3))
        if mana:
            value = max(color)
            color = (round(value * 0.36), round(value * 0.66), min(255, round(value * 1.09)))
        colors.append(color)

    x, y, width, height = rect
    # Only the left chamfer is fixed. Runtime clips the fill to the live
    # normalized value so the source's sample numbers never affect gameplay.
    radius = 4 if exp else 9
    outline = [(x + radius, y), (x + width - 1, y), (x + width - 1, y + height - 1),
               (x + radius, y + height - 1), (x, y + height - 1 - radius), (x, y + radius)]
    result = Image.new("RGBA", (width, height))
    draw = ImageDraw.Draw(result)
    for row, color in enumerate(colors):
        draw.line((0, row, width - 1, row), fill=(*color, 255))
    result.putalpha(mask(rect, points=outline))
    return rect, result


def angular_pixel(source, angle, radius):
    cx, cy = IDENTITY_CENTER
    sample_x = cx + math.sin(angle) * radius
    sample_y = cy - math.cos(angle) * radius
    x, y = math.floor(sample_x), math.floor(sample_y)
    dx, dy = sample_x - x, sample_y - y
    colors = [source.getpixel((x, y)), source.getpixel((x + 1, y)),
              source.getpixel((x, y + 1)), source.getpixel((x + 1, y + 1))]
    weights = [(1 - dx) * (1 - dy), dx * (1 - dy), (1 - dx) * dy, dx * dy]
    return tuple(round(sum(color[channel] * weight for color, weight in zip(colors, weights)))
                 for channel in range(3))


def identity_layers(source):
    x, y, width, height = IDENTITY
    cx, cy = IDENTITY_CENTER
    clean = source.copy()
    # Replace the illustration silhouette and sample text while retaining
    # the original blackened-metal face and the thin inner rim.
    sword_rect = (1165, 746, 65, 86)
    sword_outline = [(1195, 748), (1204, 754), (1207, 773), (1208, 795),
                     (1216, 800), (1214, 813), (1204, 815), (1202, 828), (1192, 828),
                     (1189, 814), (1178, 812), (1181, 801), (1191, 793), (1191, 756)]
    sword_mask = mask(sword_rect, points=sword_outline)
    replace_from_patch(clean, source, sword_rect, (1145, 776, 31, 78),
                       fill_mask=sword_mask.filter(ImageFilter.MaxFilter(7)), blur=2.1)
    caption = (1166, 822, 64, 63)
    caption_matte = (caption_ink(source, caption)
                     .filter(ImageFilter.MaxFilter(13))
                     .filter(ImageFilter.GaussianBlur(2.6)))
    # Sample the same distance from the center so the original radial
    # shading continues through the erased letters on the medallion face.
    caption_texture = Image.new("RGB", caption[2:])
    for py in range(caption[3]):
        for px in range(caption[2]):
            dx, dy = px + caption[0] - cx, py + caption[1] - cy
            radius = math.hypot(dx, dy)
            if radius < 67.9:
                sample_angle = 4.70 + dx * 0.013 + dy * 0.002
                caption_texture.putpixel((px, py), angular_pixel(source, sample_angle, radius))
            else:
                caption_matte.putpixel((px, py), 0)
    clean.paste(caption_texture, caption[:2], caption_matte)
    # Preserve the original clean rim outside the colored identity track.
    # The inactive sector above-left contains the authentic dark rim profile.
    ring_rect = outside(IDENTITY, 5)
    ring = Image.new("RGBA", ring_rect[2:])
    for py in range(y, y + height):
        for px in range(x, x + width):
            dx, dy = px - cx, py - cy
            radius = math.hypot(dx, dy)
            if not 67.9 < radius < 78.8:
                continue
            angle = math.atan2(dx, -dy) % math.tau
            if 0.025 < angle < 4.72:
                active = source.getpixel((px, py))
            else:
                active = angular_pixel(source, 1.03 + 0.21 * math.cos(angle * 3.0), radius)
            if angle < 4.88:
                clean_angle = 5.43 + 0.24 * math.cos(angle * 3.0)
                clean.putpixel((px, py), angular_pixel(source, clean_angle, radius))
            opacity = round(255 * min(1, radius - 67.9, 78.8 - radius))
            ring.putpixel((px - ring_rect[0], py - ring_rect[1]), (*active, opacity))
    plate = cutout(clean, IDENTITY, oval=(x, y, x + width - 1, y + height - 1))

    sword = source.crop(box(sword_rect)).convert("RGBA")
    # The source sword was rendered onto dark metal. Drop the nearly black
    # face pixels within its silhouette so it remains a standalone icon.
    sword_alpha = Image.new("L", sword.size)
    for py in range(sword.height):
        for px in range(sword.width):
            opacity = sword_mask.getpixel((px, py))
            r, g, b, _ = sword.getpixel((px, py))
            contrast = max(max(r, g, b) - 62, r - b - 22)
            sword_alpha.putpixel((px, py), min(opacity, max(0, contrast * 15)))
    sword.putalpha(sword_alpha)
    return plate, (ring_rect, ring), (sword_rect, sword)


def sprites(source):
    caption_face = source.copy()
    for rect, sample in BAKED_CAPTIONS.values():
        shrink_caption(caption_face, source, rect, sample)

    health_face = source.copy()
    hp_clean = (37, 33, 216, 29)
    replace_from_patch(health_face, source, hp_clean, (263, 34, 33, 28),
                       mask(hp_clean, points=[(45, 33), (252, 33), (252, 61),
                                             (45, 61), (37, 54), (37, 41)]))
    stamina_face = source.copy()
    stamina_clean = (36, 80, 218, 28)
    replace_from_patch(stamina_face, source, stamina_clean, (263, 80, 33, 28),
                       mask(stamina_clean, points=bevel(stamina_clean, 8)))
    level_face = source.copy()
    replace_from_patch(level_face, source, (41, 126, 46, 16), (542, 832, 23, 16),
                       mask((41, 126, 46, 16), round_radius=3))
    exp_face = caption_face.copy()
    replace_from_patch(exp_face, source, (144, 129, 72, 10), (246, 129, 48, 10),
                       mask((144, 129, 72, 10), round_radius=3), blur=0.4)

    yield "PlateHealth", cutout(health_face, HEALTH, points=bevel(HEALTH, 10))
    yield "PlateStamina", cutout(stamina_face, STAMINA, points=bevel(STAMINA, 10))
    yield "PlateLevel", cutout(level_face, LEVEL, points=bevel(LEVEL, 9))
    yield "PlateExp", cutout(exp_face, EXPERIENCE, points=bevel(EXPERIENCE, 7))
    yield "FillHealth", bar_fill(source, (37, 34, 268, 27), range(58, 95))
    yield "FillStamina", bar_fill(source, (37, 80, 268, 27), range(58, 95))
    yield "FillMana", bar_fill(source, (37, 80, 268, 27), range(58, 95), mana=True)
    yield "FillExp", bar_fill(source, (144, 129, 160, 10), range(157, 208), exp=True)

    for name, rect in ATTACKS.items():
        yield name, cutout(caption_face, rect, points=bevel(rect, 9))
    yield "SlotAlt", cutout(caption_face, ALT, points=bevel(ALT, 9))
    for index, rect in enumerate(LOCKS, start=1):
        yield f"SlotLock{index}", cutout(source, rect, points=bevel(rect, 9))
    yield "PlateCtrl", cutout(caption_face, CTRL, points=bevel(CTRL, 9))

    # Reuse the original Q tile's perimeter and key divider; its interior is
    # filled by native skill art, key captions, cooldown and lock overlays.
    generic_source = source.copy()
    replace_from_patch(generic_source, source, (543, 769, 72, 60), (1147, 787, 29, 60),
                       mask((543, 769, 72, 60), round_radius=4), blur=2.2)
    replace_from_patch(generic_source, source, (560, 832, 41, 22), (546, 832, 19, 22),
                       mask((560, 832, 41, 22), round_radius=3), blur=1.5)
    yield "SlotGeneric", cutout(generic_source, ATTACKS["SlotQ"], points=bevel(ATTACKS["SlotQ"], 9))

    lock_rect = (699, 876, 20, 25)
    lock_glyph = source.crop(box(lock_rect)).convert("RGBA")
    lock_alpha = Image.new("L", lock_glyph.size)
    for py in range(lock_glyph.height):
        for px in range(lock_glyph.width):
            light = max(lock_glyph.getpixel((px, py))[:3])
            lock_alpha.putpixel((px, py), max(0, min(255, round((light - 72) * 2.9))))
    lock_glyph.putalpha(lock_alpha)
    yield "GlyphLock", (lock_rect, lock_glyph)

    plate, ring, sword = identity_layers(source)
    yield "PlateIdentity", plate
    yield "RingIdentity", ring
    yield "IdentitySword", sword


def build():
    if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != SOURCE_SHA256:
        raise ValueError(f"The approved Layout A image has changed: {SOURCE}")
    with Image.open(SOURCE) as original:
        if original.size != CANVAS:
            raise ValueError(f"Unexpected reference canvas: {original.size}")
        source = original.convert("RGB")

    atlas = Image.new("RGBA", ATLAS)
    manifest = {"canvas": list(CANVAS), "atlas": list(ATLAS),
                "source": SOURCE.relative_to(ROOT).as_posix(), "source_sha256": SOURCE_SHA256, "sprites": {}}
    x = y = PADDING
    row_height = 0
    for name, (screen, sprite) in sprites(source):
        width, height = sprite.size
        if (width, height) != screen[2:]:
            raise ValueError(f"Unexpected pixel size for {name}: {sprite.size} versus {screen}")
        if x + width + PADDING > ATLAS[0]:
            x = PADDING
            y += row_height + PADDING
            row_height = 0
        if y + height + PADDING > ATLAS[1]:
            raise ValueError(f"Atlas overflow packing {name}")
        atlas.paste(sprite, (x, y))
        manifest["sprites"][name] = {"screen": list(screen), "atlas": [x, y, width, height]}
        x += width + PADDING
        row_height = max(row_height, height)

    png = io.BytesIO()
    atlas.save(png, format="PNG", optimize=True)
    layout = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
    return {"T_HudA_Atlas.png": png.getvalue(), "layout.json": layout}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--check", action="store_true", help="Verify retained outputs without rewriting them.")
    args = parser.parse_args()
    for name, asset in build().items():
        path = args.output / name
        if args.check:
            if not path.is_file() or path.read_bytes() != asset:
                raise SystemExit(f"Generated asset differs: {path}")
        else:
            args.output.mkdir(parents=True, exist_ok=True)
            path.write_bytes(asset)
        print(f"{name}: {len(asset)} bytes, SHA-256 {hashlib.sha256(asset).hexdigest()}")


if __name__ == "__main__":
    main()
