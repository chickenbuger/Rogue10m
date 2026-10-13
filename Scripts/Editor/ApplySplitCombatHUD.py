"""Apply the approved two-corner combat HUD through official UE UMG tools.

Only new Split parts/textures and the owning Bottom/Main Blueprints are saved.
Gameplay and the prior Compact parts remain available for rollback.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal
from editor_toolset.toolsets.texture import TextureTools
from editor_toolset.toolsets.object import ObjectTools

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
import ApplyCompactCombatHUD as compact
from ApplyClassThemedBottomHUD import (
    add, compile_and_save, load_widget, set_canvas_slot, umg, widget_infos,
    MAIN_HUD_PATH, BOTTOM_HUD_PATH,
)
from ApplyResponsiveCombatHUD import blueprint_class, rect, fill

ROOT = "/Game/Widget/Parts/Split"
TEXTURE_ROOT = "/Game/UI/HUD/Split"
FRAME = ROOT + "/WBP_SplitBeveledFrame"
VITAL = ROOT + "/WBP_SplitVitalBar"
XP = ROOT + "/WBP_SplitExperienceLine"
QUICK = ROOT + "/WBP_SplitQuickSlot"
IDENTITY = ROOT + "/WBP_SplitIdentity"
ACQUISITION = ROOT + "/WBP_SplitAcquisitionLine"
PARTS = (FRAME, VITAL, XP, QUICK, IDENTITY, ACQUISITION)
TEXTURES = (TEXTURE_ROOT + "/T_Identity_Unarmed", TEXTURE_ROOT + "/T_Identity_GreatSword")
OUTPUTS = (*TEXTURES, *PARTS, BOTTOM_HUD_PATH, MAIN_HUD_PATH)
PROJECT = Path(unreal.Paths.project_dir()).resolve()
EVIDENCE = PROJECT / "Feature/doc/evidence/split-hud-20261014"
LEFT_SIZE = (560, 180)
RIGHT_SIZE = (600, 270)
SIDE = 24
BOTTOM = 24
GOLD = unreal.LinearColor(0.55, 0.40, 0.22, 1)
PALE = unreal.LinearColor(0.82, 0.74, 0.54, 1)
TRANSPARENT = unreal.LinearColor(0, 0, 0, 0)
ORIGINAL_CHECKED_SET = compact.checked_set_property
ORIGINAL_QUICK_PATH = compact.QUICK_PATH
ORIGINAL_ACQUISITION_PATH = compact.ACQUISITION_PATH


def checked_set_property(instance, python_name, value):
    # UE exposes initialisms such as lODGroup; discover rather than guessing.
    schema = json.loads(ObjectTools.list_properties(instance))
    def normalized(name):
        if len(name) > 1 and name[0] == "b" and name[1].isupper():
            name = name[1:]
        return name.replace("_", "").casefold()
    matches = [name for name in schema if compact.python_property_name(name) == python_name
               or normalized(name) == normalized(python_name)]
    if len(matches) != 1:
        raise RuntimeError("Exact property discovery failed: " + instance.get_path_name() + "." + python_name)
    name = matches[0]
    current = json.loads(ObjectTools.get_properties(instance, [name]))
    if name not in current:
        raise RuntimeError("Could not read discovered field: " + name)
    compact.PROPERTY_INSPECTIONS.append({"object": instance.get_path_name(), "schema_name": name, "python_name": python_name})
    instance.set_editor_property(python_name, value)


def part(path, native):
    return compact.part(path, native)


def label(bp, parent, name, value, bounds, size=16, variable=False, left=False):
    return compact.label(bp, parent, name, value, bounds, size, variable, left)


def frame(bp, parent, name, bounds, shape=None, cut=12, z=3):
    widget, slot = add(bp, blueprint_class(FRAME), name, parent)
    rect(slot, *bounds, z=z)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    compact.checked_set_property(widget, "frame_shape", shape or unreal.Rogue10mHudFrameShape.BEVELED_BAR)
    compact.checked_set_property(widget, "corner_cut", float(cut))
    return widget


def import_identity_textures():
    for name in ("Unarmed", "GreatSword"):
        source = PROJECT / f"SourceArt/UI/Identity/2026-10-14/T_Identity_{name}_v2.png"
        destination = f"{TEXTURE_ROOT}/T_Identity_{name}"
        if not source.is_file():
            raise RuntimeError("Identity source is missing: " + str(source))
        if not unreal.EditorAssetLibrary.does_asset_exist(destination):
            # Official owning tool imports; source PNG and existing icons are unchanged.
            result = TextureTools.import_file(TEXTURE_ROOT, f"T_Identity_{name}", str(source))
            unreal.log(str(result))
        asset = compact.texture(destination)
        for field, value in (
            ("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
            ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
            ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
            ("srgb", True), ("never_stream", True),
            ("filter", unreal.TextureFilter.TF_BILINEAR),
        ):
            compact.checked_set_property(asset, field, value)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("Identity Texture save failed: " + destination)


def build_frame():
    bp = part(FRAME, "Rogue10mBeveledFrameWidget")
    add(bp, unreal.CanvasPanel, "UI_FrameCanvas")
    compact.set_defaults(FRAME, bp, frame_color=GOLD, corner_cut=12.0)


def build_vital():
    bp = part(VITAL, "Rogue10mVitalBarWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_SplitVitalCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    compact.rounded_image(bp, canvas, "UI_VitalInset", (10, 7, 438, 24),
                          compact.INSET, 3, TRANSPARENT, 0)
    progress, slot = add(bp, unreal.ProgressBar, "UI_ValueProgressBar", canvas, variable=True)
    compact.style_progress(progress, unreal.LinearColor(0.19, 0.80, 0.09, 1), 3)
    set_canvas_slot(slot, anchors=(0, 0, 1, 1), offsets=(15, 10, 15, 10), z_order=1)
    frame(bp, canvas, "UI_VitalBevel", (0, 0, 458, 38), cut=14, z=2)
    label(bp, canvas, "UI_ValueText", "", (28, 5, 402, 28), 18, True)
    compact.set_defaults(VITAL, bp, show_vital_label=True)


def build_xp():
    bp = part(XP, "Rogue10mProgressionWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_SplitExperienceCanvas")
    compact.rounded_image(bp, canvas, "UI_ExperienceTrack", (0, 0, 386, 10),
                          compact.INSET, 5, GOLD, 1)
    progress, slot = add(bp, unreal.ProgressBar, "UI_ExperienceBar", canvas, variable=True)
    compact.style_progress(progress, unreal.LinearColor(0.92, 0.63, 0.16, 1), 3)
    fill(slot, 2, 1)
    compile_and_save(XP, bp)


def build_quick():
    compact.QUICK_PATH = QUICK
    compact.build_quick_slot()
    bp = load_widget(QUICK)
    canvas = next(info.widget for info in widget_infos(bp) if info.widget.get_name() == "UI_QuickSlotCanvas")
    frame(bp, canvas, "UI_SplitSlotBevel", (0, 0, 72, 72),
          unreal.Rogue10mHudFrameShape.CLIPPED_SQUARE, 8, 1)
    compile_and_save(QUICK, bp)


def build_identity():
    bp = part(IDENTITY, "Rogue10mIdentityWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_SplitIdentityCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    compact.rounded_image(bp, canvas, "UI_IdentityBacking", (0, 0, 150, 150),
                          compact.DARK, 75, GOLD, 2)
    compact.rounded_image(bp, canvas, "UI_IdentityInset", (9, 9, 132, 132),
                          compact.INSET, 66, PALE, 0.8, 1)
    fallback = compact.icon_image(bp, canvas, "UI_IdentityFallbackIcon", (26, 5, 98, 106),
                                  compact.texture(compact.IDENTITY_FALLBACK_PATH), 2, True)
    fallback.set_render_scale(unreal.Vector2D(1.12, 1.12))
    icon = compact.icon_image(bp, canvas, "UI_IdentityIcon", (26, 5, 98, 106), z=3, variable=True)
    icon.set_render_scale(unreal.Vector2D(1.12, 1.12))
    icon.set_visibility(unreal.SlateVisibility.HIDDEN)
    label(bp, canvas, "UI_WeaponNameText", "", (14, 99, 122, 24), 15, True)
    label(bp, canvas, "UI_IdentityPercentText", "", (25, 123, 100, 18), 12, True)
    mastery = label(bp, canvas, "UI_MasteryText", "", (18, 100, 114, 14), 10, True)
    mastery.set_visibility(unreal.SlateVisibility.COLLAPSED)
    compact.set_defaults(IDENTITY, bp, use_circular_resource_gauge=True,
                         circular_gauge_thickness=4.0,
                         circular_gauge_color=unreal.LinearColor(0.93, 0.67, 0.23, 1))
    defaults = unreal.get_default_object(blueprint_class(IDENTITY))
    defaults.modify()
    compact.checked_set_property(defaults, "weapon_icon_overrides", {
        unreal.Rogue10mWeaponType.UNARMED: compact.texture(TEXTURES[0]),
        unreal.Rogue10mWeaponType.KNUCKLE: compact.texture(TEXTURES[0]),
        unreal.Rogue10mWeaponType.GREAT_SWORD: compact.texture(TEXTURES[1]),
    })
    compile_and_save(IDENTITY, bp)


def core(bp, root, side, size):
    scale, slot = add(bp, unreal.ScaleBox, f"UI_{side}CombatScaleBox", root, variable=True)
    scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    scale.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
    scale.set_ignore_inherited_scale(False)
    right = side == "Right"
    set_canvas_slot(slot, anchors=(1 if right else 0, 1, 1 if right else 0, 1),
                    offsets=(-SIDE if right else SIDE, -BOTTOM, *size),
                    alignment=(1 if right else 0, 1), z_order=2)
    box, box_slot = add(bp, unreal.SizeBox, f"UI_{side}CombatDesignSize", scale, variable=True)
    box.set_width_override(size[0])
    box.set_height_override(size[1])
    box_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT if right else unreal.HorizontalAlignment.H_ALIGN_LEFT)
    box_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
    canvas, _ = add(bp, unreal.CanvasPanel, f"UI_{side}CombatCanvas", box)
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    return canvas


def build_bottom():
    bp = part(BOTTOM_HUD_PATH, "Rogue10mBottomHUDWidget")
    root, _ = add(bp, unreal.CanvasPanel, "UI_BottomHUDCanvas")
    left = core(bp, root, "Left", LEFT_SIZE)
    right = core(bp, root, "Right", RIGHT_SIZE)
    compact.rounded_image(bp, left, "UI_LevelCrestBacking", (20, 29, 68, 130),
                          compact.DARK, 10, TRANSPARENT, 0)
    frame(bp, left, "UI_LevelCrest", (2, 16, 106, 160),
          unreal.Rogue10mHudFrameShape.DIAMOND, z=2)
    # A neutral geometric sigil does not pretend to be a second weapon gauge.
    frame(bp, left, "UI_LevelSigil", (32, 50, 46, 54),
          unreal.Rogue10mHudFrameShape.DIAMOND, z=3)
    label(bp, left, "UI_LevelText", "LV 1", (14, 111, 84, 30), 21, True)
    for name, y, visible in (("HealthBarWidget", 55, True), ("StaminaBarWidget", 101, True), ("ManaBarWidget", 101, False)):
        widget, slot = add(bp, blueprint_class(VITAL), name, left, variable=True)
        rect(slot, 102, y, 458, 38, 3)
        if not visible:
            widget.set_visibility(unreal.SlateVisibility.COLLAPSED)
    label(bp, left, "UI_ExperienceLabel", "EXP", (108, 145, 44, 20), 11, left=True)
    _, slot = add(bp, blueprint_class(XP), "ProgressionWidget", left, variable=True)
    rect(slot, 162, 153, 386, 10, 3)
    _, slot = add(bp, blueprint_class(IDENTITY), "IdentityWidget", right, variable=True)
    rect(slot, 442, 24, 150, 150, 3)
    for suffix, point in (("Top", (517, 24)), ("Left", (442, 99)), ("Right", (592, 99))):
        compact.diamond(bp, right, "UI_IdentityCap" + suffix, point, 10, GOLD, 4)
    quick = blueprint_class(QUICK)
    skills, slot = add(bp, unreal.HorizontalBox, "SkillSlotContainer", right, variable=True)
    rect(slot, 10, 82, 408, 108, 3)
    for index in range(5):
        _, child = add(bp, quick, f"UI_SkillPreview{index}", skills)
        child.set_padding(unreal.Margin(0, 0, 12 if index < 4 else 0, 0))
    item_scale, slot = add(bp, unreal.ScaleBox, "UI_ItemSlotScale", right)
    item_scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    item_scale.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
    rect(slot, 392, 200, 208, 70, 3)
    item_size, slot = add(bp, unreal.SizeBox, "UI_ItemSlotDesignSize", item_scale)
    item_size.set_width_override(324)
    item_size.set_height_override(108)
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
    items, _ = add(bp, unreal.HorizontalBox, "ItemSlotContainer", item_size, variable=True)
    for index in range(4):
        _, child = add(bp, quick, f"UI_ItemPreview{index}", items)
        child.set_padding(unreal.Margin(0, 0, 12 if index < 3 else 0, 0))
    compact.set_defaults(BOTTOM_HUD_PATH, bp,
        use_split_combat_layout=True,
        left_combat_design_size=unreal.Vector2D(*LEFT_SIZE),
        right_combat_design_size=unreal.Vector2D(*RIGHT_SIZE),
        split_minimum_center_gap=96.0, combat_side_margin=float(SIDE), combat_bottom_margin=float(BOTTOM),
        quick_slot_spacing=12.0, bottom_quick_slot_widget_class=quick,
        default_skill_icons=[compact.texture(path) for path in compact.DEFAULT_SKILL_PATHS])


def apply_main():
    bp = load_widget(MAIN_HUD_PATH)
    before = {i.widget.get_name() for i in widget_infos(bp)}
    bottom = next(i for i in widget_infos(bp) if i.widget.get_name() == "BottomHUDWidget")
    root = next(i.widget for i in widget_infos(bp) if not i.parent and not i.named_slot_host)
    # Recreate only the owned child so its new class defaults propagate. The
    # bound name and every unrelated peripheral/menu widget remain the same.
    if not umg("RemoveWidget", bp, bottom.widget):
        raise RuntimeError("Could not replace the owned Bottom HUD child")
    _, slot = add(bp, blueprint_class(BOTTOM_HUD_PATH), "BottomHUDWidget", root, variable=True)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1), offsets=(0, 0, 0, 294), alignment=(0, 1), z_order=6)
    compact.set_defaults(MAIN_HUD_PATH, bp,
                         item_acquisition_line_widget_class=blueprint_class(ACQUISITION))
    after = {i.widget.get_name() for i in widget_infos(bp)}
    if before != after:
        raise RuntimeError("Main HUD unrelated widget tree changed")
    return sorted(before)


def main():
    if str(PROJECT).replace("\\", "/").casefold() != "d:/project/rogue10m":
        raise RuntimeError("Wrong project")
    dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    if any(p.get_name() not in OUTPUTS for p in dirty):
        raise RuntimeError("Unrelated dirty packages must be preserved before HUD apply: " + str([p.get_name() for p in dirty]))
    defaults = unreal.get_default_object(unreal.load_class(None, "/Script/Rogue10m.Rogue10mBottomHUDWidget"))
    defaults.get_editor_property("use_split_combat_layout")
    compact.PROPERTY_INSPECTIONS.clear()
    import_identity_textures()
    build_frame()
    build_vital()
    build_xp()
    build_quick()
    build_identity()
    compact.ACQUISITION_PATH = ACQUISITION
    compact.build_acquisition()
    build_bottom()
    preserved = apply_main()
    report = {"saved_assets": list(OUTPUTS), "preserved_main_widgets": preserved,
              "left_size": LEFT_SIZE, "right_size": RIGHT_SIZE, "side_margin": SIDE, "bottom_margin": BOTTOM,
              "property_inspections": compact.PROPERTY_INSPECTIONS}
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / "apply.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[Rogue10mSplitHUD] RESULT=SPLIT_HUD_APPLIED")


if __name__ == "__main__":
    compact.checked_set_property = checked_set_property
    try:
        main()
    finally:
        compact.checked_set_property = ORIGINAL_CHECKED_SET
        compact.QUICK_PATH = ORIGINAL_QUICK_PATH
        compact.ACQUISITION_PATH = ORIGINAL_ACQUISITION_PATH
