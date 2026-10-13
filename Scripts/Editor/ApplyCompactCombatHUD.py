"""Build the lower, compact black-and-bronze combat HUD in the UE Editor.

Only five dedicated Compact parts and the existing Bottom/Main HUD Blueprints
are saved. Gameplay data, shared parts, menus and all raster art are preserved.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

import unreal
from editor_toolset.toolsets.object import ObjectTools

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from ApplyClassThemedBottomHUD import (
    add, clear_tree, compile_and_save, create_or_load_widget, load_widget,
    set_canvas_slot, umg, widget_infos,
    MAIN_HUD_PATH, BOTTOM_HUD_PATH,
)
from ApplyResponsiveCombatHUD import rect, fill, blueprint_class

PART_ROOT = "/Game/Widget/Parts/Compact"
VITAL_PATH = PART_ROOT + "/WBP_CompactVitalBar"
XP_PATH = PART_ROOT + "/WBP_CompactExperienceLine"
QUICK_PATH = PART_ROOT + "/WBP_CompactQuickSlot"
IDENTITY_PATH = PART_ROOT + "/WBP_CompactIdentity"
ACQUISITION_PATH = PART_ROOT + "/WBP_CompactAcquisitionLine"
OUTPUT_PATHS = (VITAL_PATH, XP_PATH, QUICK_PATH, IDENTITY_PATH,
                ACQUISITION_PATH, BOTTOM_HUD_PATH, MAIN_HUD_PATH)
IDENTITY_FALLBACK_PATH = "/Game/UI/Icons/T_Identity_StoneFist"
DEFAULT_SKILL_PATHS = tuple(
    "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_" + name
    for name in ("Jab", "Straight", "ChargedShockwave", "JumpSlam", "Dodge")
)
WHITE = unreal.LinearColor(1, 1, 1, 1)
TEXT = unreal.LinearColor(0.94, 0.92, 0.87, 1)
BRONZE = unreal.LinearColor(0.48, 0.34, 0.20, 1)
BRONZE_LIGHT = unreal.LinearColor(0.67, 0.49, 0.28, 1)
DARK = unreal.LinearColor(0.014, 0.015, 0.016, 0.96)
INSET = unreal.LinearColor(0.008, 0.009, 0.010, 0.98)
TRANSPARENT = unreal.LinearColor(0, 0, 0, 0)
DESIGN_SIZE = (1200, 196)
SIDE_MARGIN = 24
BOTTOM_MARGIN = 20
SLOT_SPACING = 20
MAIN_BOTTOM_HEIGHT = DESIGN_SIZE[1] + BOTTOM_MARGIN
PROPERTY_INSPECTIONS = []


def python_property_name(name):
    """Map Epic's reflected camelCase/bBool names to Unreal Python names."""
    if len(name) > 1 and name[0] == "b" and name[1].isupper():
        name = name[1:]
    name = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name).lower()


def checked_set_property(instance, python_name, value):
    """Discover exact schema name, read it through Epic, then set locally.

    Keep the list_properties -> get_properties sequence immediately before
    each UObject field mutation. Slate structs use the verified native header.
    """
    schema = json.loads(ObjectTools.list_properties(instance))
    if not isinstance(schema, dict):
        raise RuntimeError("ObjectTools did not return a property dictionary: " + instance.get_path_name())
    matches = [name for name in schema if python_property_name(name) == python_name]
    if len(matches) != 1:
        raise RuntimeError("Exact property discovery failed: " + instance.get_path_name()
                           + "." + python_name + " matches=" + str(matches))
    exact_name = matches[0]
    current = json.loads(ObjectTools.get_properties(instance, [exact_name]))
    if not isinstance(current, dict) or exact_name not in current:
        raise RuntimeError("ObjectTools could not read the discovered property: " + exact_name)
    PROPERTY_INSPECTIONS.append({"object": instance.get_path_name(),
                                 "schema_name": exact_name, "python_name": python_name})
    instance.set_editor_property(python_name, value)


def set_text_style(widget, size, color):
    # Font is a Slate struct. SetFont/SetColorAndOpacity/SetShadow* are verified
    # BlueprintCallable setters in UE 5.8 TextBlock.h.
    font = widget.get_editor_property("font")
    font.size = size
    widget.set_font(font)
    widget.set_color_and_opacity(unreal.SlateColor(specified_color=color))
    widget.set_shadow_offset(unreal.Vector2D(1, 1))
    widget.set_shadow_color_and_opacity(unreal.LinearColor(0, 0, 0, 0.94))


def texture(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError("Required existing HUD icon is missing: " + path)
    return asset


def part(path, native_name):
    bp = create_or_load_widget(path, "/Script/Rogue10m." + native_name)
    clear_tree(bp)
    return bp


def rounded_brush(color, radius=8, outline=BRONZE, width=1.5):
    """FSlateBrush/OutlineSettings fields verified against UE 5.8 SlateBrush.h."""
    brush = unreal.SlateBrush()
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.ROUNDED_BOX)
    brush.set_editor_property("tint_color", unreal.SlateColor(specified_color=color))
    settings = unreal.SlateBrushOutlineSettings()
    settings.set_editor_property("corner_radii", unreal.Vector4(radius, radius, radius, radius))
    settings.set_editor_property("color", unreal.SlateColor(specified_color=outline))
    settings.set_editor_property("width", width)
    settings.set_editor_property("rounding_type", unreal.SlateBrushRoundingType.FIXED_RADIUS)
    settings.set_editor_property("use_brush_transparency", False)
    brush.set_editor_property("outline_settings", settings)
    return brush


def rounded_image(bp, parent, name, bounds, color=DARK, radius=8,
                  outline=BRONZE, width=1.5, z=0, variable=False):
    widget, slot = add(bp, unreal.Image, name, parent, variable=variable)
    widget.set_brush(rounded_brush(color, radius, outline, width))
    widget.set_color_and_opacity(WHITE)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    rect(slot, *bounds, z=z)
    return widget


def icon_image(bp, parent, name, bounds, asset=None, z=1, variable=False):
    widget, slot = add(bp, unreal.Image, name, parent, variable=variable)
    if asset:
        widget.set_brush_from_texture(asset, False)
    widget.set_color_and_opacity(WHITE)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    rect(slot, *bounds, z=z)
    return widget


def diamond(bp, parent, name, center, diameter=14, color=BRONZE, z=2):
    side = diameter / (2 ** 0.5)
    widget = rounded_image(bp, parent, name, (center[0], center[1], side, side),
                           color, 0.5, BRONZE_LIGHT, 1, z)
    widget.set_render_transform_pivot(unreal.Vector2D(0.5, 0.5))
    widget.set_render_transform_angle(45)
    info = next(i for i in widget_infos(bp) if i.widget == widget)
    set_canvas_slot(info.slot, anchors=(0, 0, 0, 0),
                    offsets=(center[0], center[1], side, side),
                    alignment=(0.5, 0.5), z_order=z)
    return widget


def label(bp, parent, name, value, bounds, size=14, variable=False, left=False):
    widget, slot = add(bp, unreal.TextBlock, name, parent, variable=variable)
    widget.set_text(value)
    set_text_style(widget, size, TEXT)
    checked_set_property(widget, "justification", unreal.TextJustify.LEFT if left else unreal.TextJustify.CENTER)
    widget.set_auto_wrap_text(False)
    widget.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    rect(slot, *bounds, z=6)
    return widget


def set_defaults(path, bp, **properties):
    if not umg("CompileWidgetBlueprint", bp):
        raise RuntimeError("Widget compile failed before defaults: " + path)
    defaults = unreal.get_default_object(blueprint_class(path))
    defaults.modify()
    for name, value in properties.items():
        checked_set_property(defaults, name, value)
    compile_and_save(path, bp)
    saved = unreal.get_default_object(blueprint_class(path))
    for name, value in properties.items():
        actual = saved.get_editor_property(name)
        if isinstance(value, unreal.Vector2D):
            equal = abs(actual.x - value.x) < 0.01 and abs(actual.y - value.y) < 0.01
        elif isinstance(value, list):
            equal = list(actual) == value
        else:
            equal = actual == value
        if not equal:
            raise RuntimeError("Defaults did not survive Blueprint compile: " + path + "." + name)


def style_progress(progress, color, radius):
    style = progress.get_editor_property("widget_style")
    style.set_editor_property("background_image", rounded_brush(INSET, radius, TRANSPARENT, 0))
    style.set_editor_property("fill_image", rounded_brush(WHITE, radius, TRANSPARENT, 0))
    checked_set_property(progress, "widget_style", style)
    progress.set_percent(0)
    progress.set_fill_color_and_opacity(color)
    progress.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)


def build_vital():
    bp = part(VITAL_PATH, "Rogue10mVitalBarWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_CompactVitalCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    background = rounded_image(bp, canvas, "UI_BarBackground", (0, 0, 458, 30),
                               INSET, 10, BRONZE_LIGHT, 1.6)
    progress, slot = add(bp, unreal.ProgressBar, "UI_ValueProgressBar", canvas, variable=True)
    style_progress(progress, unreal.LinearColor(0.65, 0.055, 0.04, 1), 8)
    fill(slot, 3, 1)
    rounded_image(bp, canvas, "UI_BarInnerEdge", (3, 3, 452, 24),
                  TRANSPARENT, 8, unreal.LinearColor(0.74, 0.58, 0.35, 0.40), 0.8, 2)
    label(bp, canvas, "UI_ValueText", "", (6, 2, 446, 26), 18, True)
    set_defaults(VITAL_PATH, bp, show_vital_label=True)


def build_experience():
    bp = part(XP_PATH, "Rogue10mProgressionWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_CompactExperienceCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    rounded_image(bp, canvas, "UI_ExperienceTrack", (0, 0, 1032, 10),
                  INSET, 5, BRONZE_LIGHT, 1)
    progress, slot = add(bp, unreal.ProgressBar, "UI_ExperienceBar", canvas, variable=True)
    style_progress(progress, unreal.LinearColor(0.94, 0.62, 0.16, 1), 3)
    fill(slot, 2, 1)
    compile_and_save(XP_PATH, bp)


def build_quick_slot():
    bp = part(QUICK_PATH, "Rogue10mQuickSlotWidget")
    size, _ = add(bp, unreal.SizeBox, "UI_QuickSlotSize")
    size.set_width_override(72)
    size.set_height_override(108)
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_QuickSlotCanvas", size)
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    frame, slot = add(bp, unreal.Border, "UI_SlotFrame", canvas, variable=True)
    frame.set_brush(rounded_brush(INSET, 8, BRONZE, 1.6))
    frame.set_brush_color(WHITE)
    rect(slot, 4, 4, 64, 64)
    rounded_image(bp, canvas, "UI_SlotInnerEdge", (7, 7, 58, 58),
                  TRANSPARENT, 5, unreal.LinearColor(0.22, 0.20, 0.17, 1), 1, 1)
    icon = icon_image(bp, canvas, "UI_IconImage", (10, 10, 52, 52), z=2, variable=True)
    icon.set_visibility(unreal.SlateVisibility.HIDDEN)
    shade = rounded_image(bp, canvas, "UI_CooldownShade", (7, 7, 58, 58),
                          unreal.LinearColor(0, 0, 0, 0.68), 5, TRANSPARENT, 0, 3, True)
    shade.set_visibility(unreal.SlateVisibility.COLLAPSED)
    ready = rounded_image(bp, canvas, "UI_ReadyHighlight", (4, 4, 64, 64),
                          TRANSPARENT, 8, unreal.LinearColor(1, 0.72, 0.20, 1), 2.4, 5, True)
    ready.set_visibility(unreal.SlateVisibility.COLLAPSED)
    # A native geometric lock avoids missing Unicode glyphs and raster scaling.
    lock, slot = add(bp, unreal.CanvasPanel, "UI_LockGlyph", canvas, variable=True)
    rect(slot, 23, 21, 26, 34, z=4)
    rounded_image(bp, lock, "UI_LockArch", (5, 0, 16, 21),
                  TRANSPARENT, 7, unreal.LinearColor(0.50, 0.50, 0.47, 1), 3)
    rounded_image(bp, lock, "UI_LockBody", (0, 13, 26, 20),
                  unreal.LinearColor(0.48, 0.48, 0.45, 1), 3, TRANSPARENT, 0, 1)
    rounded_image(bp, lock, "UI_LockKeyhole", (11, 19, 4, 8),
                  INSET, 2, TRANSPARENT, 0, 2)
    lock.set_visibility(unreal.SlateVisibility.COLLAPSED)
    key = label(bp, canvas, "UI_KeyText", "", (0, 66, 72, 42), 14, True)
    checked_set_property(key, "line_height_percentage", 0.85)
    checked_set_property(key, "apply_line_height_to_bottom_line", True)
    label(bp, canvas, "UI_CooldownText", "", (6, 23, 60, 30), 23, True)
    set_defaults(QUICK_PATH, bp, use_compact_input_labels=True, use_two_line_input_labels=True)


def build_identity():
    bp = part(IDENTITY_PATH, "Rogue10mIdentityWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_CompactIdentityCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    rounded_image(bp, canvas, "UI_IdentityBacking", (0, 0, 160, 160),
                  DARK, 80, BRONZE_LIGHT, 3)
    rounded_image(bp, canvas, "UI_IdentityInset", (9, 9, 142, 142),
                  INSET, 71, BRONZE, 1)
    icon_image(bp, canvas, "UI_IdentityFallbackIcon", (46, 27, 68, 68),
               texture(IDENTITY_FALLBACK_PATH), 1, True)
    icon = icon_image(bp, canvas, "UI_IdentityIcon", (46, 27, 68, 68), z=2, variable=True)
    icon.set_visibility(unreal.SlateVisibility.HIDDEN)
    label(bp, canvas, "UI_WeaponNameText", "", (22, 104, 116, 22), 14, True)
    label(bp, canvas, "UI_IdentityPercentText", "", (32, 129, 96, 16), 10, True)
    mastery = label(bp, canvas, "UI_MasteryText", "", (32, 146, 96, 10), 8, True)
    mastery.set_visibility(unreal.SlateVisibility.COLLAPSED)
    set_defaults(IDENTITY_PATH, bp, use_circular_resource_gauge=True,
                 circular_gauge_thickness=3.0,
                 circular_gauge_color=unreal.LinearColor(0.82, 0.56, 0.22, 1))


def build_acquisition():
    bp = part(ACQUISITION_PATH, "Rogue10mLogLineWidget")
    size, _ = add(bp, unreal.SizeBox, "UI_AcquisitionLineSize")
    size.set_width_override(320)
    size.set_height_override(50)
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_AcquisitionCanvas", size)
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    rounded_image(bp, canvas, "UI_AcquisitionCard", (0, 0, 320, 44),
                  unreal.LinearColor(0.019, 0.019, 0.018, 0.91), 7, BRONZE, 1)
    rounded_image(bp, canvas, "UI_AcquisitionIconFrame", (4, 3, 38, 38),
                  INSET, 5, BRONZE, 1, 1)
    icon = icon_image(bp, canvas, "UI_ItemIconImage", (7, 6, 32, 32), z=2, variable=True)
    icon.set_visibility(unreal.SlateVisibility.HIDDEN)
    name = label(bp, canvas, "UI_ItemNameText", "", (50, 9, 183, 26), 14, True, True)
    name.set_text_overflow_policy(unreal.TextOverflowPolicy.ELLIPSIS)
    count_scale, slot = add(bp, unreal.ScaleBox, "UI_ItemCountScale", canvas)
    count_scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    count_scale.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
    rect(slot, 244, 9, 68, 26, z=6)
    count, slot = add(bp, unreal.TextBlock, "UI_ItemCountText", count_scale, variable=True)
    count.set_text("")
    set_text_style(count, 14, TEXT)
    checked_set_property(count, "justification", unreal.TextJustify.RIGHT)
    count.set_auto_wrap_text(False)
    count.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    message = label(bp, canvas, "UI_MessageText", "", (50, 10, 258, 24), 14, True, True)
    message.set_text_overflow_policy(unreal.TextOverflowPolicy.ELLIPSIS)
    message.set_visibility(unreal.SlateVisibility.COLLAPSED)
    compile_and_save(ACQUISITION_PATH, bp)


def build_bottom():
    bp = part(BOTTOM_HUD_PATH, "Rogue10mBottomHUDWidget")
    root, _ = add(bp, unreal.CanvasPanel, "UI_BottomHUDCanvas")
    scale, slot = add(bp, unreal.ScaleBox, "UI_CombatScaleBox", root, variable=True)
    scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    scale.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1),
                    offsets=(SIDE_MARGIN, -BOTTOM_MARGIN, SIDE_MARGIN, DESIGN_SIZE[1]), alignment=(0, 1))
    size, slot = add(bp, unreal.SizeBox, "UI_CombatDesignSize", scale, variable=True)
    size.set_width_override(DESIGN_SIZE[0])
    size.set_height_override(DESIGN_SIZE[1])
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_CombatCanvas", size)
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    rounded_image(bp, canvas, "UI_CompactPanel", (12, 12, 1176, 178), DARK, 18, BRONZE_LIGHT, 2)
    rounded_image(bp, canvas, "UI_CompactPanelInset", (16, 16, 1168, 170),
                  TRANSPARENT, 15, unreal.LinearColor(0.20, 0.17, 0.13, 1), 1, 1)
    diamond(bp, canvas, "UI_CompactLeftCap", (16, 46), 16)
    diamond(bp, canvas, "UI_CompactRightCap", (1184, 46), 16)
    vital = blueprint_class(VITAL_PATH)
    for name, bounds, visible in (
        ("HealthBarWidget", (40, 26, 458, 30), True),
        ("StaminaBarWidget", (702, 26, 458, 30), True),
        ("ManaBarWidget", (702, 26, 458, 30), False),
    ):
        widget, slot = add(bp, vital, name, canvas, variable=True)
        rect(slot, *bounds, z=3)
        if not visible:
            widget.set_visibility(unreal.SlateVisibility.COLLAPSED)
    _, slot = add(bp, blueprint_class(IDENTITY_PATH), "IdentityWidget", canvas, variable=True)
    rect(slot, 520, 6, 160, 160, z=5)
    for suffix, center in (("Top", (600, 6)), ("Left", (520, 86)),
                           ("Right", (680, 86)), ("Bottom", (600, 166))):
        diamond(bp, canvas, "UI_CompactCrest" + suffix, center, 12, z=6)
    quick = blueprint_class(QUICK_PATH)
    for name, x, count in (("SkillSlotContainer", 48, 5), ("ItemSlotContainer", 756, 4)):
        box, slot = add(bp, unreal.HorizontalBox, name, canvas, variable=True)
        rect(slot, x, 66, count * 72 + (count - 1) * SLOT_SPACING, 108, z=4)
        for index in range(count):
            _, child_slot = add(bp, quick, f"UI_{name}Preview{index}", box)
            child_slot.set_padding(unreal.Margin(0, 0, SLOT_SPACING if index < count - 1 else 0, 0))
            child_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            child_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    label(bp, canvas, "UI_LevelText", "LV 1", (48, 166, 70, 24), 18, True, True)
    _, slot = add(bp, blueprint_class(XP_PATH), "ProgressionWidget", canvas, variable=True)
    rect(slot, 128, 177, 1032, 10, z=8)
    set_defaults(BOTTOM_HUD_PATH, bp,
                 use_split_combat_layout=False,
                 bottom_quick_slot_widget_class=quick,
                 combat_design_size=unreal.Vector2D(*DESIGN_SIZE),
                 combat_side_margin=float(SIDE_MARGIN), combat_bottom_margin=float(BOTTOM_MARGIN),
                 quick_slot_spacing=float(SLOT_SPACING),
                 default_skill_icons=[texture(path) for path in DEFAULT_SKILL_PATHS])


def apply_main():
    bp = load_widget(MAIN_HUD_PATH)
    infos = widget_infos(bp)
    roots = [i.widget for i in infos if not i.parent and not i.named_slot_host]
    if len(roots) != 1 or not isinstance(roots[0], unreal.CanvasPanel):
        raise RuntimeError("Main HUD must have one CanvasPanel root")
    replaced = {"BottomHUDWidget", "ItemAcquisitionContainer", "ItemAcquisitionFeedWidget"}
    preserved = {info.widget.get_name() for info in infos if info.widget.get_name() not in replaced}
    for info in infos:
        if info.widget.get_name() in replaced:
            if not umg("RemoveWidget", bp, info.widget):
                raise RuntimeError("Could not replace HUD child: " + info.widget.get_name())
    root = roots[0]
    _, slot = add(bp, blueprint_class(BOTTOM_HUD_PATH), "BottomHUDWidget", root, variable=True)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1), offsets=(0, 0, 0, MAIN_BOTTOM_HEIGHT),
                    alignment=(0, 1), z_order=6)
    container, slot = add(bp, unreal.VerticalBox, "ItemAcquisitionContainer", root, variable=True)
    container.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    set_canvas_slot(slot, anchors=(1, 1, 1, 1), offsets=(-24, -64, 320, 144),
                    alignment=(1, 1), z_order=7)
    set_defaults(MAIN_HUD_PATH, bp, item_acquisition_line_widget_class=blueprint_class(ACQUISITION_PATH))
    after = {info.widget.get_name() for info in widget_infos(bp)}
    if not preserved.issubset(after):
        raise RuntimeError("Unrelated Main HUD widgets were removed: " + str(sorted(preserved - after)))
    return sorted(preserved)


def main():
    # Preflight before touching any Blueprint. New native fields require restart
    # after a full C++ build; root also checks them through ObjectTools reflection.
    PROPERTY_INSPECTIONS.clear()
    for path in (IDENTITY_FALLBACK_PATH, *DEFAULT_SKILL_PATHS):
        texture(path)
    for native, names in (
        ("Rogue10mQuickSlotWidget", ("use_two_line_input_labels",)),
        ("Rogue10mMainHUDWidget", ("item_acquisition_line_widget_class",)),
    ):
        cls = unreal.load_class(None, "/Script/Rogue10m." + native)
        if not cls:
            raise RuntimeError("Compile and restart the Editor first: " + native)
        defaults = unreal.get_default_object(cls)
        for name in names:
            defaults.get_editor_property(name)
    build_vital()
    build_experience()
    build_quick_slot()
    build_identity()
    build_acquisition()
    build_bottom()
    preserved = apply_main()
    from ValidateCompactCombatHUD import main as validate
    validate(preserved)
    unreal.log("RESULT=COMPACT_HUD_SETUP_PASSED assets=7")


if __name__ == "__main__":
    main()
