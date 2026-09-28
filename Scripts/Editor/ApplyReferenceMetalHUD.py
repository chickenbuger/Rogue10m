"""Rebuild only the reference-metal HUD parts and the two owning HUD Blueprints.

Run inside the installed UE 5.8 Editor after compiling C++ and importing the
reference frame texture. Shared Responsive parts and gameplay assets are preserved.
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
from ApplyResponsiveCombatHUD import rect, fill, blueprint_class

PART_ROOT = "/Game/Widget/Parts/ReferenceMetal"
VITAL_PATH = PART_ROOT + "/WBP_ReferenceMetalVitalBar"
XP_PATH = PART_ROOT + "/WBP_ReferenceMetalExperienceLine"
QUICK_PATH = PART_ROOT + "/WBP_ReferenceMetalQuickSlot"
IDENTITY_PATH = PART_ROOT + "/WBP_ReferenceMetalIdentity"
IDENTITY_BAR_PATH = PART_ROOT + "/WBP_ReferenceMetalIdentityBar"
LOG_PATH = PART_ROOT + "/WBP_ReferenceMetalLogLine"
MONSTER_PATH = PART_ROOT + "/WBP_ReferenceMetalMonsterInfo"
OUTPUT_PATHS = (VITAL_PATH, XP_PATH, QUICK_PATH, IDENTITY_PATH,
                IDENTITY_BAR_PATH, LOG_PATH, MONSTER_PATH, BOTTOM_HUD_PATH, MAIN_HUD_PATH)
FRAME_PATH = "/Game/UI/HUD/ReferenceMetal/T_HUD_ReferenceFrame"
SLOT_FRAME_PATH = "/Game/UI/HUD/Gothic/T_HUD_GothicSlotFrame"
IDENTITY_FALLBACK_PATH = "/Game/UI/Icons/T_Identity_StoneFist"
DEFAULT_SKILL_PATHS = tuple(
    "/Game/UI/Icons/StoneFist/T_Skill_StoneFist_" + name
    for name in ("Jab", "Straight", "ChargedShockwave", "JumpSlam", "Dodge")
)
METAL = unreal.LinearColor(0.34, 0.34, 0.31, 1)
DARK = unreal.LinearColor(0.012, 0.014, 0.016, 0.96)
TEXT = unreal.LinearColor(0.92, 0.90, 0.84, 1)
WHITE = unreal.LinearColor(1, 1, 1, 1)


def texture(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError("Required HUD texture is missing: " + path)
    return asset


def part(path, native_name):
    bp = create_or_load_widget(path, "/Script/Rogue10m." + native_name)
    clear_tree(bp)
    return bp


def image_rect(bp, parent, name, bounds, color=WHITE, z=0, asset=None, variable=False):
    widget, slot = add(bp, unreal.Image, name, parent, variable=variable)
    if asset:
        widget.set_brush_from_texture(asset, False)
    widget.set_color_and_opacity(color)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    rect(slot, *bounds, z=z)
    return widget



def diamond(bp, parent, name, center, diameter, color, z, anchor_x=0):
    # A rotated square makes a crisp pointed cap without importing another asset.
    side = diameter / (2 ** 0.5)
    widget, slot = add(bp, unreal.Image, name, parent)
    widget.set_color_and_opacity(color)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    widget.set_render_transform_pivot(unreal.Vector2D(0.5, 0.5))
    widget.set_render_transform_angle(45)
    set_canvas_slot(slot, anchors=(anchor_x, 0, anchor_x, 0),
                    offsets=(center[0], center[1], side, side), alignment=(0.5, 0.5), z_order=z)
    return widget

def label(bp, parent, name, value, bounds, size=12, variable=False, left=False):
    widget, slot = add(bp, unreal.TextBlock, name, parent, variable=variable)
    widget.set_text(value)
    set_text_style(widget, size, TEXT)
    if left:
        widget.set_editor_property("justification", unreal.TextJustify.LEFT)
    widget.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    rect(slot, *bounds, z=6)
    return widget


def set_defaults(path, bp, **properties):
    if not umg("CompileWidgetBlueprint", bp):
        raise RuntimeError("Widget compile failed before defaults: " + path)
    defaults = unreal.get_default_object(blueprint_class(path))
    for name, value in properties.items():
        defaults.set_editor_property(name, value)
    defaults.modify()
    compile_and_save(path, bp)


def style_progress(progress):
    # UE 5.8 FProgressBarStyle exposes editable Slate brushes. An explicit flat
    # fill avoids the engine's rounded blue default track in this thin metal UI.
    style = progress.get_editor_property("widget_style")
    for name, color in (("background_image", DARK), ("fill_image", unreal.LinearColor(0.48, 0.48, 0.48, 1))):
        brush = unreal.SlateBrush()
        brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
        brush.set_editor_property("tint_color", unreal.SlateColor(specified_color=color))
        style.set_editor_property(name, brush)
    progress.set_editor_property("widget_style", style)
    progress.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)


def build_vital(path=VITAL_PATH, include_text=True):
    bp = part(path, "Rogue10mVitalBarWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_MetalVitalCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    background, slot = add(bp, unreal.Border, "UI_BarBackground", canvas)
    background.set_brush_color(METAL)
    fill(slot)
    progress, slot = add(bp, unreal.ProgressBar, "UI_ValueProgressBar", canvas, variable=True)
    style_progress(progress)
    progress.set_percent(0)
    progress.set_fill_color_and_opacity(unreal.LinearColor(0.65, 0.07, 0.055, 1))
    fill(slot, 2, 1)
    if include_text:
        text, slot = add(bp, unreal.TextBlock, "UI_ValueText", canvas, variable=True)
        text.set_text("")
        set_text_style(text, 14, TEXT)
        text.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
        fill(slot, 2, 3)
    set_defaults(path, bp, show_vital_label=include_text)


def build_experience():
    bp = part(XP_PATH, "Rogue10mProgressionWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_MetalExperienceCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    for name, color, insets, z in (
        ("UI_ExperienceMetalEdge", METAL, (5, 0, 5, 0), 0),
        ("UI_ExperienceTrack", DARK, (5, 1, 5, 1), 1),
    ):
        image, slot = add(bp, unreal.Image, name, canvas)
        image.set_color_and_opacity(color)
        image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        set_canvas_slot(slot, anchors=(0, 0, 1, 1), offsets=insets, z_order=z)
    for suffix, x, anchor in (("Left", 5, 0), ("Right", -5, 1)):
        diamond(bp, canvas, "UI_ExperienceMetal" + suffix, (x, 5), 10, METAL, 0, anchor)
        diamond(bp, canvas, "UI_ExperienceInset" + suffix, (x, 5), 8, DARK, 1, anchor)
    progress, slot = add(bp, unreal.ProgressBar, "UI_ExperienceBar", canvas, variable=True)
    style_progress(progress)
    progress.set_percent(0)
    progress.set_fill_color_and_opacity(unreal.LinearColor(0.28, 0.64, 0.22, 1))
    set_canvas_slot(slot, anchors=(0, 0, 1, 1), offsets=(6, 3, 6, 3), z_order=2)
    compile_and_save(XP_PATH, bp)


def build_quick_slot():
    bp = part(QUICK_PATH, "Rogue10mQuickSlotWidget")
    size, _ = add(bp, unreal.SizeBox, "UI_QuickSlotSize")
    size.set_width_override(64)
    size.set_height_override(84)
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_QuickSlotCanvas", size)
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    # The native Border communicates enabled/locked state; texture ornaments
    # are independent and remain a neutral metal color.
    frame, slot = add(bp, unreal.Border, "UI_SlotFrame", canvas, variable=True)
    frame.set_brush_color(METAL)
    rect(slot, 2, 2, 60, 60)
    image_rect(bp, canvas, "UI_SlotInset", (3, 3, 58, 58), DARK, 1)
    icon = image_rect(bp, canvas, "UI_IconImage", (6, 6, 52, 52), WHITE, 2, variable=True)
    icon.set_visibility(unreal.SlateVisibility.HIDDEN)
    shade = image_rect(bp, canvas, "UI_CooldownShade", (6, 6, 52, 52),
                       unreal.LinearColor(0, 0, 0, 0.68), 3, variable=True)
    shade.set_visibility(unreal.SlateVisibility.COLLAPSED)
    image_rect(bp, canvas, "UI_MetalSlotOrnament", (0, 0, 64, 64),
               WHITE, 1, texture(SLOT_FRAME_PATH))
    label(bp, canvas, "UI_KeyText", "", (0, 66, 64, 18), 10, True)
    label(bp, canvas, "UI_CooldownText", "", (4, 19, 56, 29), 21, True)
    label(bp, canvas, "UI_LockedText", "", (4, 24, 56, 20), 11, True)
    set_defaults(QUICK_PATH, bp, use_compact_input_labels=True)


def build_identity():
    bp = part(IDENTITY_PATH, "Rogue10mIdentityWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_MetalIdentityCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    image_rect(bp, canvas, "UI_IdentityFallbackIcon", (38, 32, 80, 80), WHITE, 1,
               texture(IDENTITY_FALLBACK_PATH), variable=True)
    icon = image_rect(bp, canvas, "UI_IdentityIcon", (38, 32, 80, 80), WHITE, 2, variable=True)
    icon.set_visibility(unreal.SlateVisibility.HIDDEN)
    label(bp, canvas, "UI_IdentityPercentText", "--", (24, 113, 108, 24), 17, True)
    # Kept available for the native contract without duplicating the ring text.
    mastery = label(bp, canvas, "UI_MasteryText", "", (24, 139, 108, 14), 9, True)
    mastery.set_visibility(unreal.SlateVisibility.COLLAPSED)
    set_defaults(IDENTITY_PATH, bp, use_circular_resource_gauge=True)


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


def build_monster():
    bp = part(MONSTER_PATH, "Rogue10mMonsterInfoWidget")
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_MetalMonsterCanvas")
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    name = label(bp, canvas, "UI_MonsterNameText", "", (12, 0, 536, 24), 16, True)
    name.set_auto_wrap_text(False)
    name.set_text_overflow_policy(unreal.TextOverflowPolicy.ELLIPSIS)
    image_rect(bp, canvas, "UI_MonsterMetalEdge", (10, 28, 540, 20), METAL)
    image_rect(bp, canvas, "UI_MonsterTrack", (12, 30, 536, 16), DARK, 1)
    for suffix, x in (("Left", 10), ("Right", 550)):
        diamond(bp, canvas, "UI_MonsterMetal" + suffix, (x, 38), 20, METAL, 0)
        inset_x = x + (2 if suffix == "Left" else -2)
        diamond(bp, canvas, "UI_MonsterInset" + suffix, (inset_x, 38), 16, DARK, 1)
    image_rect(bp, canvas, "UI_MonsterUpperHighlight", (12, 28, 536, 1),
               unreal.LinearColor(0.52, 0.51, 0.45, 1), 2)
    progress, slot = add(bp, unreal.ProgressBar, "UI_MonsterHealthBar", canvas, variable=True)
    style_progress(progress)
    progress.set_percent(0)
    progress.set_fill_color_and_opacity(unreal.LinearColor(0.62, 0.025, 0.02, 1))
    rect(slot, 14, 32, 532, 12, z=2)
    label(bp, canvas, "UI_MonsterHealthText", "", (12, 52, 536, 16), 10, True)
    compile_and_save(MONSTER_PATH, bp)


def build_bottom():
    bp = part(BOTTOM_HUD_PATH, "Rogue10mBottomHUDWidget")
    root, _ = add(bp, unreal.CanvasPanel, "UI_BottomHUDCanvas")
    scale, slot = add(bp, unreal.ScaleBox, "UI_CombatScaleBox", root, variable=True)
    scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    scale.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1), offsets=(24, -56, 24, 208), alignment=(0, 1))
    size, slot = add(bp, unreal.SizeBox, "UI_CombatDesignSize", scale, variable=True)
    size.set_width_override(1200)
    size.set_height_override(208)
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
    canvas, _ = add(bp, unreal.CanvasPanel, "UI_CombatCanvas", size)
    canvas.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    # UV regions preserve the original ornament proportions. In particular the
    # circular crest must never be stretched with the wide combat design space.
    atlas_class = unreal.load_class(None, "/Script/Rogue10m.Rogue10mAtlasImage")
    if not atlas_class:
        raise RuntimeError("Compile the native Rogue10mAtlasImage class first")
    frame_texture = texture(FRAME_PATH)
    for name, pixels, bounds in (
        ("UI_ReferenceMetalWingLeft", (0, 272, 800, 553), (0, 10, 600, 188)),
        ("UI_ReferenceMetalWingRight", (1120, 272, 1920, 553), (600, 10, 600, 188)),
        ("UI_ReferenceMetalCrest", (725, 104, 1195, 643), (512, 0, 176, 202)),
    ):
        art, slot = add(bp, atlas_class, name, canvas)
        art.set_brush_from_texture(frame_texture, False)
        art.set_editor_property("uv_min", unreal.Vector2D(pixels[0] / 1920.0, pixels[1] / 819.0))
        art.set_editor_property("uv_max", unreal.Vector2D(pixels[2] / 1920.0, pixels[3] / 819.0))
        art.apply_uv_region()
        art.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        rect(slot, *bounds, z=0)
    for name, bounds, visible in (
        ("HealthBarWidget", (60, 42, 420, 24), True),
        ("StaminaBarWidget", (720, 42, 420, 24), True),
        ("ManaBarWidget", (720, 42, 420, 24), False),
    ):
        cls = blueprint_class(VITAL_PATH)
        widget, slot = add(bp, cls, name, canvas, variable=True)
        rect(slot, *bounds, z=3)
        if not visible:
            widget.set_visibility(unreal.SlateVisibility.COLLAPSED)
    _, slot = add(bp, blueprint_class(IDENTITY_PATH), "IdentityWidget", canvas, variable=True)
    rect(slot, 522, 24, 156, 156, z=5)
    # Small ornament accents identify the class without tinting the full frame.
    for theme, color in (
        ("Wizard", (0.22, 0.45, 0.75, 1)), ("Warrior", (0.65, 0.24, 0.19, 1)),
        ("MartialArtist", (0.63, 0.49, 0.23, 1)), ("Rogue", (0.23, 0.56, 0.44, 1)),
    ):
        accent = image_rect(bp, canvas, f"UI_{theme}ThemeFrame", (563, 183, 74, 2),
                            unreal.LinearColor(*color), 4, variable=True)
        accent.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE
                              if theme == "MartialArtist" else unreal.SlateVisibility.COLLAPSED)
    quick = blueprint_class(QUICK_PATH)
    for name, x, count in (("SkillSlotContainer", 50, 5), ("ItemSlotContainer", 790, 4)):
        box, slot = add(bp, unreal.HorizontalBox, name, canvas, variable=True)
        rect(slot, x, 82, count * 64 + (count - 1) * 10, 84, z=4)
        for index in range(count):
            _, child_slot = add(bp, quick, f"UI_{name}Preview{index}", box)
            child_slot.set_padding(unreal.Margin(0, 0, 10 if index < count - 1 else 0, 0))
            child_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            child_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    label(bp, canvas, "UI_ThemeNameText", "", (534, 186, 132, 18), 10, True)
    label(bp, canvas, "UI_LevelText", "LV 1", (30, 180, 90, 20), 12, True, left=True)
    _, slot = add(bp, blueprint_class(XP_PATH), "ProgressionWidget", root, variable=True)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1), offsets=(24, -16, 24, 10), alignment=(0, 1), z_order=8)
    set_defaults(BOTTOM_HUD_PATH, bp,
                 bottom_quick_slot_widget_class=quick,
                 combat_design_size=unreal.Vector2D(1200, 208),
                 combat_side_margin=24.0, combat_bottom_margin=56.0,
                 quick_slot_spacing=10.0,
                 default_skill_icons=[texture(path) for path in DEFAULT_SKILL_PATHS])


def apply_main():
    bp = load_widget(MAIN_HUD_PATH)
    infos = widget_infos(bp)
    roots = [i.widget for i in infos if not i.parent and not i.named_slot_host]
    if len(roots) != 1 or not isinstance(roots[0], unreal.CanvasPanel):
        raise RuntimeError("Main HUD must have one CanvasPanel root")
    replaced = OLD_MAIN_BOTTOM_WIDGETS | {
        "BottomHUDWidget", "SystemLogPanelWidget", "ItemAcquisitionFeedWidget",
        "SystemLogContainer", "ItemAcquisitionContainer", "UI_RunTimerText", "MonsterInfoWidget",
    }
    for info in infos:
        if info.widget.get_name() in replaced:
            if not umg("RemoveWidget", bp, info.widget):
                raise RuntimeError("Could not remove old HUD child: " + info.widget.get_name())
    root = roots[0]
    _, slot = add(bp, blueprint_class(BOTTOM_HUD_PATH), "BottomHUDWidget", root, variable=True)
    set_canvas_slot(slot, anchors=(0, 1, 1, 1), offsets=(0, 0, 0, 264), alignment=(0, 1), z_order=6)
    for name, anchors, offsets, alignment in (
        ("SystemLogContainer", (0, 1, 0, 1), (24, -288, 360, 48), (0, 1)),
        ("ItemAcquisitionContainer", (1, 0.65, 1, 0.65), (-24, 0, 260, 72), (1, 0)),
    ):
        container, slot = add(bp, unreal.VerticalBox, name, root, variable=True)
        container.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
        set_canvas_slot(slot, anchors=anchors, offsets=offsets, alignment=alignment, z_order=5)
    timer, slot = add(bp, unreal.TextBlock, "UI_RunTimerText", root, variable=True)
    timer.set_text("")
    set_text_style(timer, 20, TEXT)
    timer.set_editor_property("justification", unreal.TextJustify.RIGHT)
    set_canvas_slot(slot, anchors=(1, 0, 1, 0), offsets=(-24, 24, 140, 32), alignment=(1, 0), z_order=5)
    _, slot = add(bp, blueprint_class(MONSTER_PATH), "MonsterInfoWidget", root, variable=True)
    set_canvas_slot(slot, anchors=(0.5, 0, 0.5, 0), offsets=(0, 24, 560, 68), alignment=(0.5, 0), z_order=4)
    set_defaults(MAIN_HUD_PATH, bp, log_line_widget_class=blueprint_class(LOG_PATH))


def main():
    # Fail before modifying any Blueprint if the art import has not completed.
    for path in (FRAME_PATH, SLOT_FRAME_PATH, IDENTITY_FALLBACK_PATH, *DEFAULT_SKILL_PATHS):
        texture(path)
    build_vital()
    build_vital(IDENTITY_BAR_PATH, include_text=False)
    build_experience()
    build_quick_slot()
    build_identity()
    build_log_line()
    build_monster()
    build_bottom()
    apply_main()
    from ValidateReferenceMetalHUD import main as validate
    validate()
    unreal.log("[Rogue10mReferenceMetalHUD] Application completed; nine explicit HUD assets saved")


if __name__ == "__main__":
    main()
