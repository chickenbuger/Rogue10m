"""Read-only saved-asset contract checks for the UE 5.8 reference-metal HUD."""
from __future__ import annotations

import sys
from pathlib import Path

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from ApplyReferenceMetalHUD import (
    MAIN_HUD_PATH, BOTTOM_HUD_PATH, VITAL_PATH, XP_PATH, QUICK_PATH,
    IDENTITY_PATH, IDENTITY_BAR_PATH, LOG_PATH, MONSTER_PATH,
    FRAME_PATH, IDENTITY_FALLBACK_PATH, DEFAULT_SKILL_PATHS, OUTPUT_PATHS,
)
from ApplyClassThemedBottomHUD import load_widget, widget_infos


def require(condition, message):
    if not condition:
        raise RuntimeError("[Rogue10mReferenceMetalHUD] " + message)


def tree(path):
    infos = widget_infos(load_widget(path))
    result = {info.widget.get_name(): info for info in infos}
    require(len(result) == len(infos), "Duplicate widget names: " + path)
    return result


def defaults(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    require(cls is not None, "Missing compiled widget class: " + path)
    return unreal.get_default_object(cls)


def check_canvas(info, anchors, offsets, alignment=(0, 0)):
    require(isinstance(info.slot, unreal.CanvasPanelSlot), "Canvas slot required: " + info.widget.get_name())
    a = info.slot.get_anchors()
    values = (a.minimum.x, a.minimum.y, a.maximum.x, a.maximum.y)
    require(all(abs(x-y) < 0.01 for x, y in zip(values, anchors)), "Wrong anchors: " + info.widget.get_name())
    o = info.slot.get_offsets()
    require(all(abs(x-y) < 0.01 for x, y in zip((o.left, o.top, o.right, o.bottom), offsets)), "Wrong offsets: " + info.widget.get_name())
    a = info.slot.get_alignment()
    require(abs(a.x-alignment[0]) < 0.01 and abs(a.y-alignment[1]) < 0.01, "Wrong alignment: " + info.widget.get_name())


def require_widgets(widgets, expected):
    for name, class_type in expected.items():
        require(name in widgets, "Missing widget: " + name)
        require(isinstance(widgets[name].widget, class_type), "Wrong widget type: " + name)


def main():
    require(len(OUTPUT_PATHS) == 9 and len(set(OUTPUT_PATHS)) == 9, "Nine isolated HUD assets required")
    main_widgets = tree(MAIN_HUD_PATH)
    bottom = tree(BOTTOM_HUD_PATH)
    require_widgets(main_widgets, {
        "SystemLogContainer": unreal.VerticalBox,
        "ItemAcquisitionContainer": unreal.VerticalBox,
        "UI_RunTimerText": unreal.TextBlock,
    })
    for name, anchors, offsets, alignment in (
        ("BottomHUDWidget", (0, 1, 1, 1), (0, 0, 0, 264), (0, 1)),
        ("SystemLogContainer", (0, 1, 0, 1), (24, -288, 360, 48), (0, 1)),
        ("ItemAcquisitionContainer", (1, 0.65, 1, 0.65), (-24, 0, 260, 72), (1, 0)),
        ("UI_RunTimerText", (1, 0, 1, 0), (-24, 24, 140, 32), (1, 0)),
        ("MonsterInfoWidget", (0.5, 0, 0.5, 0), (0, 24, 560, 68), (0.5, 0)),
    ):
        require(name in main_widgets, "Missing peripheral widget: " + name)
        check_canvas(main_widgets[name], anchors, offsets, alignment)
    require("SystemLogPanelWidget" not in main_widgets and "ItemAcquisitionFeedWidget" not in main_widgets, "Legacy log placeholders must be absent")
    require(main_widgets["MonsterInfoWidget"].widget.get_class() == unreal.EditorAssetLibrary.load_blueprint_class(MONSTER_PATH), "Dedicated target widget required")
    require_widgets(bottom, {
        "UI_BottomHUDCanvas": unreal.CanvasPanel,
        "UI_CombatScaleBox": unreal.ScaleBox,
        "UI_CombatDesignSize": unreal.SizeBox,
        "UI_CombatCanvas": unreal.CanvasPanel,
        "UI_LevelText": unreal.TextBlock,
        "SkillSlotContainer": unreal.HorizontalBox,
        "ItemSlotContainer": unreal.HorizontalBox,
    })
    require("UI_SkillLabel" not in bottom and "UI_ItemLabel" not in bottom, "No redundant skill/item section headings")
    require("IdentityBarWidget" not in bottom, "Circular layout must not bind a duplicate linear identity bar")
    for child, parent in (
        ("UI_CombatScaleBox", "UI_BottomHUDCanvas"),
        ("UI_CombatDesignSize", "UI_CombatScaleBox"),
        ("UI_CombatCanvas", "UI_CombatDesignSize"),
        ("ProgressionWidget", "UI_BottomHUDCanvas"),
    ):
        require(bottom[child].parent == bottom[parent].widget, "Wrong parent: " + child)
    scale = bottom["UI_CombatScaleBox"].widget
    require(scale.get_editor_property("stretch") == unreal.Stretch.SCALE_TO_FIT, "ScaleToFit required")
    require(scale.get_editor_property("stretch_direction") == unreal.StretchDirection.DOWN_ONLY, "DownOnly required")
    require(not scale.get_editor_property("ignore_inherited_scale"), "UMG DPI must be inherited exactly once")
    check_canvas(bottom["UI_CombatScaleBox"], (0, 1, 1, 1), (24, -56, 24, 208), (0, 1))
    size = bottom["UI_CombatDesignSize"].widget
    require(size.get_editor_property("width_override") == 1200 and size.get_editor_property("height_override") == 208, "Combat design must be 1200 by 208")
    require(bottom["UI_CombatDesignSize"].slot.get_editor_property("horizontal_alignment") == unreal.HorizontalAlignment.H_ALIGN_CENTER, "Core must be centered")
    require(bottom["UI_CombatDesignSize"].slot.get_editor_property("vertical_alignment") == unreal.VerticalAlignment.V_ALIGN_BOTTOM, "Core must align to bottom")
    for name, bounds in (
        ("HealthBarWidget", (60, 42, 420, 24)),
        ("StaminaBarWidget", (720, 42, 420, 24)),
        ("ManaBarWidget", (720, 42, 420, 24)),
        ("IdentityWidget", (522, 24, 156, 156)),
        ("SkillSlotContainer", (50, 82, 360, 84)),
        ("ItemSlotContainer", (790, 82, 286, 84)),
        ("UI_LevelText", (30, 180, 90, 20)),
    ):
        require(name in bottom, "Missing bound child: " + name)
        require(bottom[name].parent == bottom["UI_CombatCanvas"].widget, "Child must share combat scale: " + name)
        check_canvas(bottom[name], (0, 0, 0, 0), bounds)
    check_canvas(bottom["ProgressionWidget"], (0, 1, 1, 1), (24, -16, 24, 10), (0, 1))
    frame_texture = unreal.EditorAssetLibrary.load_asset(FRAME_PATH)
    for name, pixels, bounds in (
        ("UI_ReferenceMetalWingLeft", (0, 272, 800, 553), (0, 10, 600, 188)),
        ("UI_ReferenceMetalWingRight", (1120, 272, 1920, 553), (600, 10, 600, 188)),
        ("UI_ReferenceMetalCrest", (725, 104, 1195, 643), (512, 0, 176, 202)),
    ):
        require(name in bottom, "Missing sliced reference art: " + name)
        art = bottom[name].widget
        require(art.get_class().get_name() == "Rogue10mAtlasImage", "Native UV image required")
        check_canvas(bottom[name], (0, 0, 0, 0), bounds)
        require(art.get_editor_property("brush").get_editor_property("resource_object") == frame_texture, "Wrong frame texture")
        uv_min, uv_max = art.get_editor_property("uv_min"), art.get_editor_property("uv_max")
        actual = (uv_min.x, uv_min.y, uv_max.x, uv_max.y)
        wanted = (pixels[0]/1920.0, pixels[1]/819.0, pixels[2]/1920.0, pixels[3]/819.0)
        require(all(abs(a-b) < 0.00001 for a,b in zip(actual,wanted)), "Wrong frame UV region")
    require("UI_ReferenceMetalFrame" not in bottom, "Do not stretch the complete atlas across the HUD")
    bottom_defaults = defaults(BOTTOM_HUD_PATH)
    design = bottom_defaults.get_editor_property("combat_design_size")
    require(design.x == 1200 and design.y == 208, "Native design defaults must match the asset")
    for prop, value in (("combat_side_margin", 24), ("combat_bottom_margin", 56), ("quick_slot_spacing", 10)):
        require(bottom_defaults.get_editor_property(prop) == value, "Wrong bottom default: " + prop)
    quick_cls = unreal.EditorAssetLibrary.load_blueprint_class(QUICK_PATH)
    require(bottom_defaults.get_editor_property("bottom_quick_slot_widget_class") == quick_cls, "Dedicated slot class required")
    icons = list(bottom_defaults.get_editor_property("default_skill_icons"))
    require([icon.get_path_name().split(".")[0] for icon in icons] == list(DEFAULT_SKILL_PATHS), "Five ordered existing skill fallback icons required")
    for name, count in (("SkillSlotContainer", 5), ("ItemSlotContainer", 4)):
        children = [i for i in bottom.values() if i.parent == bottom[name].widget]
        require(len(children) == count, "Wrong designer slot count: " + name)
        require(all(i.widget.get_class() == quick_cls for i in children), "Designer slots must use dedicated class")
        for index, info in enumerate(children):
            padding = info.slot.get_editor_property("padding")
            require(padding.right == (10 if index < count-1 else 0), "Wrong designer slot spacing")
    vital = tree(VITAL_PATH)
    require_widgets(vital, {"UI_ValueProgressBar": unreal.ProgressBar, "UI_ValueText": unreal.TextBlock})
    require(defaults(VITAL_PATH).get_editor_property("show_vital_label"), "Vital labels must identify real resources")
    xp = tree(XP_PATH)
    require_widgets(xp, {
        "UI_ExperienceBar": unreal.ProgressBar,
        "UI_ExperienceMetalEdge": unreal.Image, "UI_ExperienceTrack": unreal.Image,
        "UI_ExperienceMetalLeft": unreal.Image, "UI_ExperienceMetalRight": unreal.Image,
        "UI_ExperienceInsetLeft": unreal.Image, "UI_ExperienceInsetRight": unreal.Image,
    })
    require(xp["UI_ExperienceBar"].widget.get_editor_property("percent") == 0, "XP must start empty")
    check_canvas(xp["UI_ExperienceBar"], (0, 0, 1, 1), (6, 3, 6, 3))
    check_canvas(xp["UI_ExperienceMetalEdge"], (0, 0, 1, 1), (5, 0, 5, 0))
    check_canvas(xp["UI_ExperienceTrack"], (0, 0, 1, 1), (5, 1, 5, 1))
    require(len(xp) == 8, "XP must have only its track, metal caps and live fill")
    for suffix, x, anchor in (("Left", 5, 0), ("Right", -5, 1)):
        for prefix, diameter in (("UI_ExperienceMetal", 10), ("UI_ExperienceInset", 8)):
            cap = xp[prefix + suffix]
            side = diameter / (2 ** 0.5)
            check_canvas(cap, (anchor, 0, anchor, 0), (x, 5, side, side), (0.5, 0.5))
            require(abs(cap.widget.get_render_transform_angle() - 45) < 0.01, "Pointed XP cap rotation required")
    quick = tree(QUICK_PATH)
    require_widgets(quick, {
        "UI_QuickSlotSize": unreal.SizeBox, "UI_SlotFrame": unreal.Border,
        "UI_IconImage": unreal.Image, "UI_CooldownShade": unreal.Image,
        "UI_KeyText": unreal.TextBlock, "UI_CooldownText": unreal.TextBlock,
        "UI_LockedText": unreal.TextBlock,
    })
    quick_size = quick["UI_QuickSlotSize"].widget
    require(quick_size.get_editor_property("width_override") == 64 and quick_size.get_editor_property("height_override") == 84, "64px icon plus 20px key area required")
    check_canvas(quick["UI_KeyText"], (0, 0, 0, 0), (0, 66, 64, 18))
    check_canvas(quick["UI_CooldownShade"], (0, 0, 0, 0), (6, 6, 52, 52))
    require(quick["UI_MetalSlotOrnament"].slot.get_z_order() < quick["UI_IconImage"].slot.get_z_order(), "Opaque legacy slot art must be behind the live icon")
    require(defaults(QUICK_PATH).get_editor_property("use_compact_input_labels"), "Mouse input label mapping required")
    identity = tree(IDENTITY_PATH)
    require_widgets(identity, {"UI_IdentityIcon": unreal.Image, "UI_IdentityFallbackIcon": unreal.Image, "UI_IdentityPercentText": unreal.TextBlock})
    require(defaults(IDENTITY_PATH).get_editor_property("use_circular_resource_gauge"), "Live circular resource gauge required")
    check_canvas(identity["UI_IdentityIcon"], (0, 0, 0, 0), (38, 32, 80, 80))
    check_canvas(identity["UI_IdentityFallbackIcon"], (0, 0, 0, 0), (38, 32, 80, 80))
    fallback = identity["UI_IdentityFallbackIcon"].widget.get_editor_property("brush").get_editor_property("resource_object")
    require(fallback == unreal.EditorAssetLibrary.load_asset(IDENTITY_FALLBACK_PATH), "Existing decorative identity fallback required")
    require(identity["UI_IdentityFallbackIcon"].slot.get_z_order() < identity["UI_IdentityIcon"].slot.get_z_order(), "Live identity icon must be above its fallback")
    check_canvas(identity["UI_IdentityPercentText"], (0, 0, 0, 0), (24, 113, 108, 24))
    require("UI_MetalMedallion" not in identity, "Do not duplicate the central frame")
    identity_bar = tree(IDENTITY_BAR_PATH)
    require("UI_ValueProgressBar" in identity_bar and "UI_ValueText" not in identity_bar, "Supplementary resource strip must be text-free")
    log = tree(LOG_PATH)
    require_widgets(log, {"UI_LogLineSize": unreal.SizeBox, "UI_MessageText": unreal.TextBlock})
    require(log["UI_MessageText"].widget.get_editor_property("text_overflow_policy") == unreal.TextOverflowPolicy.ELLIPSIS, "Long logs must use ellipsis")
    require(defaults(MAIN_HUD_PATH).get_editor_property("log_line_widget_class") == unreal.EditorAssetLibrary.load_blueprint_class(LOG_PATH), "Dedicated log row class required")
    monster = tree(MONSTER_PATH)
    require_widgets(monster, {
        "UI_MonsterNameText": unreal.TextBlock,
        "UI_MonsterHealthBar": unreal.ProgressBar, "UI_MonsterHealthText": unreal.TextBlock,
        "UI_MonsterMetalLeft": unreal.Image, "UI_MonsterMetalRight": unreal.Image,
        "UI_MonsterInsetLeft": unreal.Image, "UI_MonsterInsetRight": unreal.Image,
    })
    for name, bounds in (
        ("UI_MonsterNameText", (12, 0, 536, 24)),
        ("UI_MonsterMetalEdge", (10, 28, 540, 20)),
        ("UI_MonsterTrack", (12, 30, 536, 16)),
        ("UI_MonsterUpperHighlight", (12, 28, 536, 1)),
        ("UI_MonsterHealthBar", (14, 32, 532, 12)),
        ("UI_MonsterHealthText", (12, 52, 536, 16)),
    ):
        check_canvas(monster[name], (0, 0, 0, 0), bounds)
    require(monster["UI_MonsterNameText"].widget.get_editor_property("text_overflow_policy") == unreal.TextOverflowPolicy.ELLIPSIS, "Long monster names must fit one line")
    require(monster["UI_MonsterHealthBar"].widget.get_editor_property("percent") == 0, "No fake initial monster health")
    for suffix, x in (("Left", 10), ("Right", 550)):
        for prefix, center_x, diameter in (
            ("UI_MonsterMetal", x, 20),
            ("UI_MonsterInset", x + (2 if suffix == "Left" else -2), 16),
        ):
            cap = monster[prefix + suffix]
            side = diameter / (2 ** 0.5)
            check_canvas(cap, (0, 0, 0, 0), (center_x, 38, side, side), (0.5, 0.5))
            require(abs(cap.widget.get_render_transform_angle() - 45) < 0.01, "Pointed monster bar cap rotation required")
    unreal.log("[Rogue10mReferenceMetalHUD] PASS: nine assets, native bindings, UV art, ratio-safe layout, five skills/four items, live gauge contract")


if __name__ == "__main__":
    main()
