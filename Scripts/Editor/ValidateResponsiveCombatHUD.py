"""Read-only validation of the saved UE 5.8 responsive HUD asset contract."""
from __future__ import annotations

import sys
from pathlib import Path

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from ApplyClassThemedBottomHUD import load_widget, widget_infos, MAIN_HUD_PATH, BOTTOM_HUD_PATH

PART_ROOT = "/Game/Widget/Parts/Responsive"


def require(condition, message):
    if not condition:
        raise RuntimeError("[Rogue10mResponsiveHUD] " + message)


def tree(path):
    infos = widget_infos(load_widget(path))
    result = {info.widget.get_name(): info for info in infos}
    require(len(result) == len(infos), "Duplicate widget names: " + path)
    return result


def check_canvas(info, anchors, offsets):
    require(isinstance(info.slot, unreal.CanvasPanelSlot), "Canvas slot required: " + info.widget.get_name())
    actual = info.slot.get_anchors()
    values = (actual.minimum.x, actual.minimum.y, actual.maximum.x, actual.maximum.y)
    require(all(abs(a - b) < 0.01 for a, b in zip(values, anchors)), "Wrong anchors: " + info.widget.get_name())
    actual = info.slot.get_offsets()
    values = (actual.left, actual.top, actual.right, actual.bottom)
    require(all(abs(a - b) < 0.01 for a, b in zip(values, offsets)), "Wrong offsets: " + info.widget.get_name())


def require_widgets(widgets, expected):
    for name, class_type in expected.items():
        require(name in widgets, "Missing widget: " + name)
        require(isinstance(widgets[name].widget, class_type), "Wrong widget type: " + name)


def main():
    main_widgets = tree(MAIN_HUD_PATH)
    bottom = tree(BOTTOM_HUD_PATH)
    require_widgets(main_widgets, {
        "SystemLogContainer": unreal.VerticalBox,
        "ItemAcquisitionContainer": unreal.VerticalBox,
        "UI_RunTimerText": unreal.TextBlock,
    })
    for name, anchors, offsets, expected_alignment in (
        ("SystemLogContainer", (0, 1, 0, 1), (24, -208, 360, 48), (0, 1)),
        ("ItemAcquisitionContainer", (1, 0.65, 1, 0.65), (-24, 0, 260, 72), (1, 0)),
        ("UI_RunTimerText", (1, 0, 1, 0), (-24, 24, 140, 32), (1, 0)),
        ("MonsterInfoWidget", (0.5, 0, 0.5, 0), (0, 24, 420, 68), (0.5, 0)),
    ):
        require(name in main_widgets, "Missing anchored peripheral widget: " + name)
        check_canvas(main_widgets[name], anchors, offsets)
        actual_alignment = main_widgets[name].slot.get_alignment()
        require(actual_alignment.x == expected_alignment[0] and actual_alignment.y == expected_alignment[1], "Wrong peripheral alignment: " + name)
    require("SystemLogPanelWidget" not in main_widgets and "ItemAcquisitionFeedWidget" not in main_widgets, "Legacy placeholder log panels must be removed")
    log = tree(PART_ROOT + "/WBP_ResponsiveLogLine")
    require_widgets(log, {"UI_LogLineSize": unreal.SizeBox, "UI_MessageText": unreal.TextBlock})
    require(log["UI_LogLineSize"].widget.get_editor_property("height_override") == 24, "Log row height must be 24")
    require(log["UI_MessageText"].widget.get_editor_property("text_overflow_policy") == unreal.TextOverflowPolicy.ELLIPSIS, "Long log rows must use ellipsis")
    main_cls = unreal.EditorAssetLibrary.load_blueprint_class(MAIN_HUD_PATH)
    log_cls = unreal.EditorAssetLibrary.load_blueprint_class(PART_ROOT + "/WBP_ResponsiveLogLine")
    require(unreal.get_default_object(main_cls).get_editor_property("log_line_widget_class") == log_cls, "Main HUD must use its compact log-line class")
    require("BottomHUDWidget" in main_widgets, "Main HUD missing BottomHUDWidget")
    check_canvas(main_widgets["BottomHUDWidget"], (0, 1, 1, 1), (0, 0, 0, 184))
    require_widgets(bottom, {
        "UI_BottomHUDCanvas": unreal.CanvasPanel,
        "UI_CombatScaleBox": unreal.ScaleBox,
        "UI_CombatDesignSize": unreal.SizeBox,
        "UI_CombatCanvas": unreal.CanvasPanel,
        "UI_LevelText": unreal.TextBlock,
        "SkillSlotContainer": unreal.HorizontalBox,
        "ItemSlotContainer": unreal.HorizontalBox,
    })
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
    require(not scale.get_editor_property("ignore_inherited_scale"), "DPI scale must be inherited")
    check_canvas(bottom["UI_CombatScaleBox"], (0, 1, 1, 1), (24, -24, 24, 160))
    size = bottom["UI_CombatDesignSize"].widget
    require(size.get_editor_property("width_override") == 1000, "Design width must be 1000")
    require(size.get_editor_property("height_override") == 160, "Design height must be 160")
    require(bottom["UI_CombatDesignSize"].slot.get_editor_property("horizontal_alignment") == unreal.HorizontalAlignment.H_ALIGN_CENTER, "Combat core must be centered")
    require(bottom["UI_CombatDesignSize"].slot.get_editor_property("vertical_alignment") == unreal.VerticalAlignment.V_ALIGN_BOTTOM, "Combat core must align to bottom")
    for name, bounds in (
        ("HealthBarWidget", (24, 14, 352, 20)),
        ("StaminaBarWidget", (624, 14, 352, 20)),
        ("ManaBarWidget", (624, 14, 352, 20)),
        ("IdentityWidget", (462, 43, 76, 76)),
        ("SkillSlotContainer", (24, 64, 312, 56)),
        ("ItemSlotContainer", (664, 64, 248, 56)),
    ):
        require(name in bottom, "Missing bound child: " + name)
        require(bottom[name].parent == bottom["UI_CombatCanvas"].widget, "Child must share combat scale: " + name)
        check_canvas(bottom[name], (0, 0, 0, 0), bounds)
    check_canvas(bottom["ProgressionWidget"], (0, 1, 1, 1), (0, -4, 0, 4))
    for theme in ("Wizard", "Warrior", "MartialArtist", "Rogue"):
        check_canvas(bottom[f"UI_{theme}ThemeFrame"], (0, 0, 0, 0), (462, 43, 76, 76))

    vital = tree(PART_ROOT + "/WBP_ResponsiveVitalBar")
    require_widgets(vital, {"UI_ValueProgressBar": unreal.ProgressBar, "UI_ValueText": unreal.TextBlock})
    xp = tree(PART_ROOT + "/WBP_ResponsiveExperienceLine")
    require_widgets(xp, {"UI_ExperienceBar": unreal.ProgressBar})
    require(xp["UI_ExperienceBar"].widget.get_editor_property("percent") == 0.0, "Experience line must start empty until live data arrives")
    check_canvas(xp["UI_ExperienceBar"], (0, 0, 1, 1), (0, 0, 0, 0))
    require(len(xp) == 2, "Experience line must not contain a fixed-width background or text")
    quick = tree(PART_ROOT + "/WBP_ResponsiveQuickSlot")
    require_widgets(quick, {
        "UI_QuickSlotSize": unreal.SizeBox, "UI_SlotFrame": unreal.Border,
        "UI_IconImage": unreal.Image, "UI_KeyText": unreal.TextBlock,
        "UI_CooldownText": unreal.TextBlock,
    })
    quick_size = quick["UI_QuickSlotSize"].widget
    require(quick_size.get_editor_property("width_override") == 56 and quick_size.get_editor_property("height_override") == 56, "Quick slot must be 56 square")
    quick_cls = unreal.EditorAssetLibrary.load_blueprint_class(PART_ROOT + "/WBP_ResponsiveQuickSlot")
    bottom_cls = unreal.EditorAssetLibrary.load_blueprint_class(BOTTOM_HUD_PATH)
    require(unreal.get_default_object(bottom_cls).get_editor_property("bottom_quick_slot_widget_class") == quick_cls, "Bottom HUD must use its compact quick-slot override")
    for name, count in (("SkillSlotContainer", 5), ("ItemSlotContainer", 4)):
        children = [info for info in bottom.values() if info.parent == bottom[name].widget]
        require(len(children) == count, "Wrong designer slot count: " + name)
        require(all(info.widget.get_class() == quick_cls for info in children), "Designer slots must use compact class")
    require(unreal.get_default_object(quick_cls).get_editor_property("use_compact_input_labels"), "Compact input labels must be enabled")
    identity = tree(PART_ROOT + "/WBP_ResponsiveIdentity")
    require_widgets(identity, {"UI_IdentityIcon": unreal.Image, "UI_MasteryText": unreal.TextBlock})
    identity_bar = tree(PART_ROOT + "/WBP_ResponsiveIdentityBar")
    require("UI_ValueProgressBar" in identity_bar and "UI_ValueText" not in identity_bar, "Identity resource strip must be text-free")
    for info in (main_widgets["BottomHUDWidget"], bottom["UI_CombatScaleBox"]):
        alignment = info.slot.get_alignment()
        require(alignment.x == 0 and alignment.y == 1, "Bottom alignment required: " + info.widget.get_name())
    unreal.log("[Rogue10mResponsiveHUD] PASS: hierarchy, anchors, uniform scaling, compact parts, bindings and isolated XP line")


if __name__ == "__main__":
    main()
