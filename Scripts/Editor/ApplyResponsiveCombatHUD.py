"""Build the compact UE 5.8 combat HUD. Run in the Editor after compiling C++.

Only the two HUD Blueprints and six dedicated responsive parts are saved.
Existing shared part Blueprints, imported textures and gameplay assets are untouched.
"""
from __future__ import annotations

import sys
from pathlib import Path

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from ApplyClassThemedBottomHUD import (
    add, clear_tree, compile_and_save, create_or_load_widget, load_widget,
    set_canvas_slot, set_text_style, umg, widget_infos,
    MAIN_HUD_PATH, BOTTOM_HUD_PATH, OLD_MAIN_BOTTOM_WIDGETS,
)

PART_ROOT = "/Game/Widget/Parts/Responsive"
VITAL_PATH = PART_ROOT + "/WBP_ResponsiveVitalBar"
XP_PATH = PART_ROOT + "/WBP_ResponsiveExperienceLine"
QUICK_PATH = PART_ROOT + "/WBP_ResponsiveQuickSlot"
IDENTITY_PATH = PART_ROOT + "/WBP_ResponsiveIdentity"
IDENTITY_BAR_PATH = PART_ROOT + "/WBP_ResponsiveIdentityBar"
LOG_PATH = PART_ROOT + "/WBP_ResponsiveLogLine"
OUTPUT_PATHS = (VITAL_PATH, XP_PATH, QUICK_PATH, IDENTITY_PATH, IDENTITY_BAR_PATH, LOG_PATH, BOTTOM_HUD_PATH, MAIN_HUD_PATH)
GOLD = unreal.LinearColor(0.49, 0.39, 0.23, 1.0)
DARK = unreal.LinearColor(0.012, 0.016, 0.024, 0.94)
TEXT = unreal.LinearColor(0.91, 0.86, 0.74, 1.0)


def rect(slot, x, y, width, height, z=0):
    set_canvas_slot(slot, anchors=(0, 0, 0, 0), offsets=(x, y, width, height), z_order=z)


def fill(slot, margin=0, z=0):
    set_canvas_slot(slot, anchors=(0, 0, 1, 1), offsets=(margin,) * 4, z_order=z)


def image_rect(bp, parent, name, bounds, color, z=0):
    widget, slot = add(bp, unreal.Image, name, parent)
    widget.set_color_and_opacity(color)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    rect(slot, *bounds, z=z)
    return widget


def label(bp, parent, name, value, bounds, size=11, variable=False):
    widget, slot = add(bp, unreal.TextBlock, name, parent, variable=variable)
    widget.set_text(value)
    set_text_style(widget, size, TEXT)
    widget.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    rect(slot, *bounds, z=5)
    return widget


def part(path, native_name):
    bp = create_or_load_widget(path, "/Script/Rogue10m." + native_name)
    clear_tree(bp)
    return bp


def build_vital(path=VITAL_PATH, include_text=True):
    bp = part(path, "Rogue10mVitalBarWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_ResponsiveVitalCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    background, slot = add(bp, unreal.Border, "UI_BarBackground", canvas)
    background.set_brush_color(GOLD)
    fill(slot)
    progress, slot = add(bp, unreal.ProgressBar, "UI_ValueProgressBar", canvas, variable=True)
    progress.set_percent(1.0)
    progress.set_fill_color_and_opacity(unreal.LinearColor(0.65, 0.07, 0.055, 1))
    fill(slot, 1, 1)
    if not include_text:
        compile_and_save(path, bp)
        return
    text, slot = add(bp, unreal.TextBlock, "UI_ValueText", canvas, variable=True)
    text.set_text("100 / 100")
    set_text_style(text, 12, TEXT)
    text.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    fill(slot, 1, 2)
    compile_and_save(path, bp)


def build_experience():
    bp = part(XP_PATH, "Rogue10mProgressionWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_ResponsiveExperienceCanvas")
    progress, slot = add(bp, unreal.ProgressBar, "UI_ExperienceBar", canvas, variable=True)
    progress.set_percent(0.0)
    progress.set_fill_color_and_opacity(unreal.LinearColor(0.4, 0.58, 0.31, 1))
    fill(slot)
    compile_and_save(XP_PATH, bp)


def build_quick_slot():
    bp = part(QUICK_PATH, "Rogue10mQuickSlotWidget")
    size, _ = add(bp, unreal.SizeBox, "UI_QuickSlotSize")
    size.set_width_override(56)
    size.set_height_override(56)
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_QuickSlotCanvas", size)
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    frame, slot = add(bp, unreal.Border, "UI_SlotFrame", canvas, variable=True)
    frame.set_brush_color(GOLD)
    fill(slot)
    image_rect(bp, canvas, "UI_SlotInset", (1, 1, 54, 54), DARK, 1)
    icon, slot = add(bp, unreal.Image, "UI_IconImage", canvas, variable=True)
    rect(slot, 3, 3, 50, 50, 2)
    image_rect(bp, canvas, "UI_KeyBackdrop", (2, 38, 52, 16), unreal.LinearColor(0, 0, 0, 0.7), 3)
    label(bp, canvas, "UI_KeyText", "Q", (2, 37, 52, 18), 11, True)
    label(bp, canvas, "UI_CooldownText", "", (2, 15, 52, 24), 16, True)
    label(bp, canvas, "UI_LockedText", "잠금", (2, 18, 52, 20), 10, True)
    if not umg("CompileWidgetBlueprint", bp):
        raise RuntimeError("Quick-slot compile failed")
    defaults = unreal.get_default_object(blueprint_class(QUICK_PATH))
    defaults.set_editor_property("use_compact_input_labels", True)
    defaults.modify()
    compile_and_save(QUICK_PATH, bp)


def build_identity():
    bp = part(IDENTITY_PATH, "Rogue10mIdentityWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_ResponsiveIdentityCanvas")
    # The class-specific medallion images are owned by BottomHUD's theme logic.
    # Keep a data-bound identity child without inheriting the old 96x116 frame.
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    icon, slot = add(bp, unreal.Image, "UI_IdentityIcon", canvas, variable=True)
    icon.set_visibility(unreal.SlateVisibility.HIDDEN)
    rect(slot, 18, 8, 40, 40, 1)
    label(bp, canvas, "UI_MasteryText", "", (0, 54, 76, 16), 9, True)
    compile_and_save(IDENTITY_PATH, bp)


def blueprint_class(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not cls:
        raise RuntimeError("Missing generated class: " + path)
    return cls


def build_bottom():
    bp = part(BOTTOM_HUD_PATH, "Rogue10mBottomHUDWidget")
    root, _ = add(bp, unreal.CanvasPanel, "UI_BottomHUDCanvas")
    scale, slot = add(bp, unreal.ScaleBox, "UI_CombatScaleBox", root, variable=True)
    scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    scale.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1), offsets=(24, -24, 24, 160), alignment=(0, 1))
    size, slot = add(bp, unreal.SizeBox, "UI_CombatDesignSize", scale, variable=True)
    size.set_width_override(1000)
    size.set_height_override(160)
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_CombatCanvas", size)
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    image_rect(bp, canvas, "UI_CombatPanel", (0, 0, 1000, 160), DARK)
    image_rect(bp, canvas, "UI_CombatTopRule", (0, 0, 1000, 1), GOLD, 1)
    image_rect(bp, canvas, "UI_CombatBottomRule", (0, 159, 1000, 1), GOLD, 1)
    image_rect(bp, canvas, "UI_CenterRuleLeft", (406, 42, 1, 90), GOLD, 1)
    image_rect(bp, canvas, "UI_CenterRuleRight", (593, 42, 1, 90), GOLD, 1)

    vital = blueprint_class(VITAL_PATH)
    for name, bounds, visible in (
        ("HealthBarWidget", (24, 14, 352, 20), True),
        ("StaminaBarWidget", (624, 14, 352, 20), True),
        ("ManaBarWidget", (624, 14, 352, 20), False),
        ("IdentityBarWidget", (440, 125, 120, 8), False),
    ):
        widget, slot = add(bp, blueprint_class(IDENTITY_BAR_PATH) if name == "IdentityBarWidget" else vital, name, canvas, variable=True)
        rect(slot, *bounds, z=3)
        if not visible:
            widget.set_visibility(unreal.SlateVisibility.COLLAPSED)

    identity, slot = add(bp, blueprint_class(IDENTITY_PATH), "IdentityWidget", canvas, variable=True)
    rect(slot, 462, 43, 76, 76, 5)
    medallion = unreal.EditorAssetLibrary.load_asset("/Game/UI/HUD/Gothic/T_HUD_GothicMedallion")
    if not medallion:
        raise RuntimeError("Existing Gothic medallion texture is required")
    for theme, color in (
        ("Wizard", (0.45, 0.68, 1, 1)), ("Warrior", (1, 0.4, 0.32, 1)),
        ("MartialArtist", (1, 0.78, 0.35, 1)), ("Rogue", (0.6, 0.85, 0.8, 1)),
    ):
        frame, slot = add(bp, unreal.Image, f"UI_{theme}ThemeFrame", canvas, variable=True)
        frame.set_brush_from_texture(medallion, False)
        frame.set_color_and_opacity(unreal.LinearColor(*color))
        frame.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE if theme == "MartialArtist" else unreal.SlateVisibility.COLLAPSED)
        rect(slot, 462, 43, 76, 76, 4)

    quick = blueprint_class(QUICK_PATH)
    for name, x, count in (("SkillSlotContainer", 24, 5), ("ItemSlotContainer", 664, 4)):
        box, slot = add(bp, unreal.HorizontalBox, name, canvas, variable=True)
        rect(slot, x, 64, count * 56 + (count - 1) * 8, 56, 4)
        for index in range(count):
            child, child_slot = add(bp, quick, f"UI_{name}Preview{index}", box)
            child_slot.set_padding(unreal.Margin(0, 0, 8 if index < count - 1 else 0, 0))
            child_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            child_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

    label(bp, canvas, "UI_SkillLabel", "SKILL", (24, 42, 312, 16), 9)
    label(bp, canvas, "UI_ItemLabel", "ITEM", (664, 42, 248, 16), 9)
    label(bp, canvas, "UI_ThemeNameText", "권사", (442, 15, 116, 20), 12, True)
    label(bp, canvas, "UI_ResourceTypeText", "스태미나", (840, 128, 136, 18), 9, True)
    label(bp, canvas, "UI_LevelText", "LV 1", (462, 138, 76, 18), 10, True)
    xp, slot = add(bp, blueprint_class(XP_PATH), "ProgressionWidget", root, variable=True)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1), offsets=(0, -4, 0, 4), z_order=8)
    # Set the override on the generated class after compiling the new tree.
    if not umg("CompileWidgetBlueprint", bp):
        raise RuntimeError("Bottom HUD compile failed")
    defaults = unreal.get_default_object(blueprint_class(BOTTOM_HUD_PATH))
    defaults.set_editor_property("bottom_quick_slot_widget_class", quick)
    defaults.modify()
    compile_and_save(BOTTOM_HUD_PATH, bp)


def build_log_line():
    bp = part(LOG_PATH, "Rogue10mLogLineWidget")
    size, _ = add(bp, unreal.SizeBox, "UI_LogLineSize")
    size.set_height_override(24)
    size.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    message, slot = add(bp, unreal.TextBlock, "UI_MessageText", size, variable=True)
    message.set_text("")
    set_text_style(message, 12, TEXT)
    message.set_editor_property("justification", unreal.TextJustify.LEFT)
    message.set_text_overflow_policy(unreal.TextOverflowPolicy.ELLIPSIS)
    message.set_auto_wrap_text(False)
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    compile_and_save(LOG_PATH, bp)


def apply_main():
    bp = load_widget(MAIN_HUD_PATH)
    infos = widget_infos(bp)
    roots = [i.widget for i in infos if not i.parent and not i.named_slot_host]
    if len(roots) != 1 or not isinstance(roots[0], unreal.CanvasPanel):
        raise RuntimeError("Main HUD must have one CanvasPanel root")
    replaced = OLD_MAIN_BOTTOM_WIDGETS | {
        "BottomHUDWidget", "SystemLogPanelWidget", "ItemAcquisitionFeedWidget",
        "SystemLogContainer", "ItemAcquisitionContainer", "UI_RunTimerText",
    }
    for info in infos:
        if info.widget.get_name() in replaced:
            if not umg("RemoveWidget", bp, info.widget):
                raise RuntimeError("Could not remove old HUD child")
    root = roots[0]
    _, slot = add(bp, blueprint_class(BOTTOM_HUD_PATH), "BottomHUDWidget", root, variable=True)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1), offsets=(0, 0, 0, 184), alignment=(0, 1), z_order=6)
    for name, anchors, offsets, alignment in (
        ("SystemLogContainer", (0, 1, 0, 1), (24, -208, 360, 48), (0, 1)),
        ("ItemAcquisitionContainer", (1, 0.65, 1, 0.65), (-24, 0, 260, 72), (1, 0)),
    ):
        container, slot = add(bp, unreal.VerticalBox, name, root, variable=True)
        container.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
        set_canvas_slot(slot, anchors=anchors, offsets=offsets, alignment=alignment, z_order=5)
    timer, slot = add(bp, unreal.TextBlock, "UI_RunTimerText", root, variable=True)
    timer.set_text("00:00")
    set_text_style(timer, 18, TEXT)
    timer.set_editor_property("justification", unreal.TextJustify.RIGHT)
    set_canvas_slot(slot, anchors=(1, 0, 1, 0), offsets=(-24, 24, 140, 32), alignment=(1, 0), z_order=5)
    for info in widget_infos(bp):
        if info.widget.get_name() == "MonsterInfoWidget":
            set_canvas_slot(info.slot, anchors=(0.5, 0, 0.5, 0), offsets=(0, 24, 420, 68), alignment=(0.5, 0), z_order=4)
    if not umg("CompileWidgetBlueprint", bp):
        raise RuntimeError("Main HUD compile failed")
    defaults = unreal.get_default_object(blueprint_class(MAIN_HUD_PATH))
    defaults.set_editor_property("log_line_widget_class", blueprint_class(LOG_PATH))
    defaults.modify()
    compile_and_save(MAIN_HUD_PATH, bp)

def main():
    build_vital()
    build_vital(IDENTITY_BAR_PATH, include_text=False)
    build_experience()
    build_quick_slot()
    build_identity()
    build_bottom()
    build_log_line()
    apply_main()
    from ValidateResponsiveCombatHUD import main as validate
    validate()
    unreal.log("[Rogue10mResponsiveHUD] Application completed; eight explicit HUD assets saved")


if __name__ == "__main__":
    main()
