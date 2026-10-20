"""Read-only saved-asset checks for the compact HUD, with JSON evidence."""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal
from editor_toolset.toolsets.blueprint import BlueprintTools

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from ApplyCompactCombatHUD import (
    MAIN_HUD_PATH, BOTTOM_HUD_PATH, VITAL_PATH, XP_PATH, QUICK_PATH,
    IDENTITY_PATH, ACQUISITION_PATH, IDENTITY_FALLBACK_PATH,
    DEFAULT_SKILL_PATHS, OUTPUT_PATHS, DESIGN_SIZE, SIDE_MARGIN,
    BOTTOM_MARGIN, SLOT_SPACING, MAIN_BOTTOM_HEIGHT, PROPERTY_INSPECTIONS,
)
from ApplyClassThemedBottomHUD import load_widget, widget_infos

EVIDENCE_PATH = SCRIPT_DIR.parents[1] / "Feature/doc/evidence/compact-hud-20261005/setup-validation.json"


def require(condition, message):
    if not condition:
        raise RuntimeError("[Rogue10mCompactHUD] " + message)


def near(actual, expected):
    return abs(actual - expected) < 0.01


def defaults(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    require(cls is not None, "Missing compiled class: " + path)
    return unreal.get_default_object(cls)


def tree(path):
    infos = widget_infos(load_widget(path))
    result = {info.widget.get_name(): info for info in infos}
    require(len(result) == len(infos), "Duplicate widget names: " + path)
    require(len([i for i in infos if not i.parent and not i.named_slot_host]) == 1,
            "One root required: " + path)
    return result


def widgets(widgets_by_name, expected):
    for name, cls in expected.items():
        require(name in widgets_by_name, "Missing widget: " + name)
        require(isinstance(widgets_by_name[name].widget, cls), "Wrong widget type: " + name)


def canvas(info, bounds, anchors=(0, 0, 0, 0), alignment=(0, 0)):
    require(isinstance(info.slot, unreal.CanvasPanelSlot), "Canvas slot required: " + info.widget.get_name())
    a = info.slot.get_anchors()
    actual = (a.minimum.x, a.minimum.y, a.maximum.x, a.maximum.y)
    require(all(near(x, y) for x, y in zip(actual, anchors)), "Wrong anchors: " + info.widget.get_name())
    o = info.slot.get_offsets()
    actual = (o.left, o.top, o.right, o.bottom)
    require(all(near(x, y) for x, y in zip(actual, bounds)), "Wrong offsets: " + info.widget.get_name())
    a = info.slot.get_alignment()
    require(near(a.x, alignment[0]) and near(a.y, alignment[1]), "Wrong alignment: " + info.widget.get_name())


def brush(widget, radius=None, min_width=None):
    b = widget.get_editor_property("brush")
    require(b.get_editor_property("draw_as") == unreal.SlateBrushDrawType.ROUNDED_BOX,
            "Native rounded brush required: " + widget.get_name())
    require(b.get_editor_property("resource_object") is None,
            "HUD decoration must not use raster frames: " + widget.get_name())
    outline = b.get_editor_property("outline_settings")
    require(outline.get_editor_property("rounding_type") == unreal.SlateBrushRoundingType.FIXED_RADIUS,
            "Fixed radius required: " + widget.get_name())
    if radius is not None:
        r = outline.get_editor_property("corner_radii")
        require(all(near(x, radius) for x in (r.x, r.y, r.z, r.w)), "Wrong corner radius: " + widget.get_name())
    if min_width is not None:
        require(outline.get_editor_property("width") >= min_width, "Outline too thin: " + widget.get_name())


def font(widget, minimum):
    require(widget.get_editor_property("font").size >= minimum, "Font too small: " + widget.get_name())


def main(preserved_main_names=None):
    require(len(OUTPUT_PATHS) == 7 and len(set(OUTPUT_PATHS)) == 7, "Seven isolated HUD assets required")
    trees = {path: tree(path) for path in OUTPUT_PATHS}
    for path, native in (
        (VITAL_PATH, "Rogue10mVitalBarWidget"), (XP_PATH, "Rogue10mProgressionWidget"),
        (QUICK_PATH, "Rogue10mQuickSlotWidget"), (IDENTITY_PATH, "Rogue10mIdentityWidget"),
        (ACQUISITION_PATH, "Rogue10mLogLineWidget"),
        (BOTTOM_HUD_PATH, "Rogue10mBottomHUDWidget"), (MAIN_HUD_PATH, "Rogue10mMainHUDWidget"),
    ):
        asset = load_widget(path)
        # UBlueprint.ParentClass is not exposed through get_editor_property in
        # UE 5.8. Epic's BlueprintTools uses the public Blueprint extension API.
        require(BlueprintTools.get_parent(asset) == unreal.load_class(None, "/Script/Rogue10m." + native),
                "Wrong native parent: " + path)
        defaults(path)
    main_widgets = trees[MAIN_HUD_PATH]
    widgets(main_widgets, {"ItemAcquisitionContainer": unreal.VerticalBox})
    require("BottomHUDWidget" in main_widgets, "Main HUD missing BottomHUDWidget")
    canvas(main_widgets["BottomHUDWidget"], (0, 0, 0, MAIN_BOTTOM_HEIGHT), (0, 1, 1, 1), (0, 1))
    canvas(main_widgets["ItemAcquisitionContainer"], (-24, -64, 320, 144), (1, 1, 1, 1), (1, 1))
    require(main_widgets["ItemAcquisitionContainer"].widget.get_clipping() == unreal.WidgetClipping.CLIP_TO_BOUNDS,
            "Acquisition feed must clip only its final trailing spacer")
    if preserved_main_names:
        require(set(preserved_main_names).issubset(main_widgets), "Unrelated Main HUD widgets must survive")
    require(defaults(MAIN_HUD_PATH).get_editor_property("item_acquisition_line_widget_class")
            == unreal.EditorAssetLibrary.load_blueprint_class(ACQUISITION_PATH), "Dedicated acquisition class required")
    require(defaults(MAIN_HUD_PATH).get_editor_property("log_line_widget_class")
            != unreal.EditorAssetLibrary.load_blueprint_class(ACQUISITION_PATH), "System logs must retain their text row class")

    bottom = trees[BOTTOM_HUD_PATH]
    widgets(bottom, {
        "UI_BottomHUDCanvas": unreal.CanvasPanel, "UI_CombatScaleBox": unreal.ScaleBox,
        "UI_CombatDesignSize": unreal.SizeBox, "UI_CombatCanvas": unreal.CanvasPanel,
        "UI_CompactPanel": unreal.Image, "UI_LevelText": unreal.TextBlock,
        "SkillSlotContainer": unreal.HorizontalBox, "ItemSlotContainer": unreal.HorizontalBox,
    })
    require("IdentityBarWidget" not in bottom, "No duplicate linear identity gauge")
    require(not any(name.startswith("UI_ReferenceMetal") or "Gothic" in name for name in bottom),
            "Old gothic frame decorations must be absent")
    for child, parent in (
        ("UI_CombatScaleBox", "UI_BottomHUDCanvas"), ("UI_CombatDesignSize", "UI_CombatScaleBox"),
        ("UI_CombatCanvas", "UI_CombatDesignSize"), ("ProgressionWidget", "UI_CombatCanvas"),
    ):
        require(bottom[child].parent == bottom[parent].widget, "Wrong layout parent: " + child)
    scale = bottom["UI_CombatScaleBox"].widget
    require(scale.get_editor_property("stretch") == unreal.Stretch.SCALE_TO_FIT, "ScaleToFit required")
    require(scale.get_editor_property("stretch_direction") == unreal.StretchDirection.DOWN_ONLY, "DownOnly required")
    require(not scale.get_editor_property("ignore_inherited_scale"), "UMG DPI must be inherited once")
    canvas(bottom["UI_CombatScaleBox"], (SIDE_MARGIN, -BOTTOM_MARGIN, SIDE_MARGIN, DESIGN_SIZE[1]),
           (0, 1, 1, 1), (0, 1))
    size = bottom["UI_CombatDesignSize"].widget
    require(near(size.get_editor_property("width_override"), DESIGN_SIZE[0])
            and near(size.get_editor_property("height_override"), DESIGN_SIZE[1]), "Wrong combat design size")
    require(bottom["UI_CombatDesignSize"].slot.get_editor_property("horizontal_alignment")
            == unreal.HorizontalAlignment.H_ALIGN_CENTER, "Combat core must be centered")
    require(bottom["UI_CombatDesignSize"].slot.get_editor_property("vertical_alignment")
            == unreal.VerticalAlignment.V_ALIGN_BOTTOM, "Combat core must align to bottom")
    for name, bounds in (
        ("HealthBarWidget", (40, 26, 458, 30)), ("StaminaBarWidget", (702, 26, 458, 30)),
        ("ManaBarWidget", (702, 26, 458, 30)), ("IdentityWidget", (520, 6, 160, 160)),
        ("SkillSlotContainer", (48, 66, 440, 108)), ("ItemSlotContainer", (756, 66, 348, 108)),
        ("UI_LevelText", (48, 166, 70, 24)), ("ProgressionWidget", (128, 177, 1032, 10)),
        ("UI_CompactPanel", (12, 12, 1176, 178)),
    ):
        require(name in bottom, "Missing combat child: " + name)
        require(bottom[name].parent == bottom["UI_CombatCanvas"].widget, "Combat child must share scale: " + name)
        canvas(bottom[name], bounds)
    brush(bottom["UI_CompactPanel"].widget, 18, 1.5)
    font(bottom["UI_LevelText"].widget, 18)
    bcd = defaults(BOTTOM_HUD_PATH)
    design = bcd.get_editor_property("combat_design_size")
    require(near(design.x, DESIGN_SIZE[0]) and near(design.y, DESIGN_SIZE[1]), "Native design size must match tree")
    for prop, expected in (("combat_side_margin", SIDE_MARGIN), ("combat_bottom_margin", BOTTOM_MARGIN),
                           ("quick_slot_spacing", SLOT_SPACING)):
        require(near(bcd.get_editor_property(prop), expected), "Wrong native bottom default: " + prop)
    quick_cls = unreal.EditorAssetLibrary.load_blueprint_class(QUICK_PATH)
    require(bcd.get_editor_property("bottom_quick_slot_widget_class") == quick_cls, "Dedicated quick slot class required")
    icons = list(bcd.get_editor_property("default_skill_icons"))
    require([i.get_path_name().split(".")[0] for i in icons] == list(DEFAULT_SKILL_PATHS),
            "Existing ordered skill fallback icons required")
    for name, count in (("SkillSlotContainer", 5), ("ItemSlotContainer", 4)):
        children = [i for i in bottom.values() if i.parent == bottom[name].widget]
        require(len(children) == count, "Wrong designer slot count: " + name)
        require(all(i.widget.get_class() == quick_cls for i in children), "Wrong designer quick slot class")
        for index, info in enumerate(children):
            p = info.slot.get_editor_property("padding")
            require(near(p.right, SLOT_SPACING if index < count - 1 else 0), "Wrong designer slot spacing")

    vital = trees[VITAL_PATH]
    widgets(vital, {"UI_ValueProgressBar": unreal.ProgressBar, "UI_ValueText": unreal.TextBlock})
    require(defaults(VITAL_PATH).get_editor_property("show_vital_label"), "Vital labels must identify actual resources")
    font(vital["UI_ValueText"].widget, 18)
    require(str(vital["UI_ValueText"].widget.get_text()) == "", "No fake initial vital values")
    require(near(vital["UI_ValueProgressBar"].widget.get_editor_property("percent"), 0), "Empty initial vital fill required")
    fill_brush = vital["UI_ValueProgressBar"].widget.get_editor_property("widget_style").get_editor_property("fill_image")
    fill_color = fill_brush.get_editor_property("tint_color").get_editor_property("specified_color")
    require(all(near(c, 1) for c in (fill_color.r, fill_color.g, fill_color.b, fill_color.a)),
            "White fill tint required to preserve native resource color")

    xp = trees[XP_PATH]
    widgets(xp, {"UI_ExperienceBar": unreal.ProgressBar, "UI_ExperienceTrack": unreal.Image})
    canvas(xp["UI_ExperienceBar"], (2, 2, 2, 2), (0, 0, 1, 1))
    require(near(xp["UI_ExperienceBar"].widget.get_editor_property("percent"), 0), "No fake initial XP fill")
    brush(xp["UI_ExperienceTrack"].widget, 5, 1)

    quick = trees[QUICK_PATH]
    widgets(quick, {
        "UI_QuickSlotSize": unreal.SizeBox, "UI_SlotFrame": unreal.Border,
        "UI_IconImage": unreal.Image, "UI_CooldownShade": unreal.Image,
        "UI_KeyText": unreal.TextBlock, "UI_CooldownText": unreal.TextBlock,
        "UI_LockGlyph": unreal.CanvasPanel, "UI_ReadyHighlight": unreal.Image,
    })
    size = quick["UI_QuickSlotSize"].widget
    require(near(size.get_editor_property("width_override"), 72)
            and near(size.get_editor_property("height_override"), 108), "72 by 108 quick slots required")
    for name, bounds in (("UI_SlotFrame", (4, 4, 64, 64)), ("UI_ReadyHighlight", (4, 4, 64, 64)),
                         ("UI_IconImage", (10, 10, 52, 52)), ("UI_KeyText", (0, 66, 72, 42)),
                         ("UI_LockGlyph", (23, 21, 26, 34))):
        canvas(quick[name], bounds)
    font(quick["UI_KeyText"].widget, 14)
    require("UI_LockedText" not in quick, "Geometric lock should replace the repeated lock word")
    for name in ("UI_LockArch", "UI_LockBody", "UI_LockKeyhole"):
        require(name in quick and quick[name].parent == quick["UI_LockGlyph"].widget,
                "Lock geometry must share its native visibility binding")
        brush(quick[name].widget)
    for name in ("use_compact_input_labels", "use_two_line_input_labels"):
        require(defaults(QUICK_PATH).get_editor_property(name), "Missing compact input default: " + name)
    brush(quick["UI_ReadyHighlight"].widget, 8, 2)

    identity = trees[IDENTITY_PATH]
    widgets(identity, {
        "UI_IdentityIcon": unreal.Image, "UI_IdentityFallbackIcon": unreal.Image,
        "UI_WeaponNameText": unreal.TextBlock, "UI_IdentityPercentText": unreal.TextBlock,
    })
    brush(identity["UI_IdentityBacking"].widget, 80, 3)
    for name, bounds in (("UI_IdentityIcon", (46, 27, 68, 68)), ("UI_IdentityFallbackIcon", (46, 27, 68, 68)),
                         ("UI_WeaponNameText", (22, 104, 116, 22)), ("UI_IdentityPercentText", (32, 129, 96, 16))):
        canvas(identity[name], bounds)
    fallback = identity["UI_IdentityFallbackIcon"].widget.get_editor_property("brush").get_editor_property("resource_object")
    require(fallback == unreal.EditorAssetLibrary.load_asset(IDENTITY_FALLBACK_PATH), "Existing decorative crest required")
    require(identity["UI_MasteryText"].widget.get_visibility() == unreal.SlateVisibility.COLLAPSED,
            "Mastery text must not overlap weapon name/resource percent")
    require(defaults(IDENTITY_PATH).get_editor_property("use_circular_resource_gauge"), "Live circular resource gauge required")
    require(near(defaults(IDENTITY_PATH).get_editor_property("circular_gauge_thickness"), 3), "Compact ring thickness required")
    font(identity["UI_WeaponNameText"].widget, 14)

    acquisition = trees[ACQUISITION_PATH]
    widgets(acquisition, {
        "UI_AcquisitionLineSize": unreal.SizeBox, "UI_ItemIconImage": unreal.Image,
        "UI_ItemNameText": unreal.TextBlock, "UI_ItemCountText": unreal.TextBlock,
        "UI_ItemCountScale": unreal.ScaleBox,
        "UI_MessageText": unreal.TextBlock,
    })
    size = acquisition["UI_AcquisitionLineSize"].widget
    require(near(size.get_editor_property("width_override"), 320)
            and near(size.get_editor_property("height_override"), 50), "44px card with 6px spacer required")
    for name, bounds in (("UI_AcquisitionCard", (0, 0, 320, 44)), ("UI_ItemIconImage", (7, 6, 32, 32)),
                         ("UI_ItemNameText", (50, 9, 183, 26)), ("UI_ItemCountScale", (244, 9, 68, 26)),
                         ("UI_MessageText", (50, 10, 258, 24))):
        canvas(acquisition[name], bounds)
    require(acquisition["UI_ItemCountText"].parent == acquisition["UI_ItemCountScale"].widget,
            "Quantity must fit its reserved width without clipping digits")
    require(acquisition["UI_ItemCountScale"].widget.get_editor_property("stretch_direction")
            == unreal.StretchDirection.DOWN_ONLY, "Long actual quantities must shrink only when needed")
    brush(acquisition["UI_AcquisitionCard"].widget, 7, 1)
    for name in ("UI_ItemNameText", "UI_ItemCountText", "UI_MessageText"):
        require(str(acquisition[name].widget.get_text()) == "", "No fake initial acquisition data")
        font(acquisition[name].widget, 14)
    require(acquisition["UI_ItemNameText"].widget.get_editor_property("text_overflow_policy")
            == unreal.TextOverflowPolicy.ELLIPSIS, "Long item names must use ellipsis")
    require(acquisition["UI_MessageText"].widget.get_visibility() == unreal.SlateVisibility.COLLAPSED,
            "Generic messages are hidden until their live non-item entry arrives")
    require(acquisition["UI_MessageText"].widget.get_editor_property("text_overflow_policy")
            == unreal.TextOverflowPolicy.ELLIPSIS, "Generic acquisition messages must fit the card")

    evidence = {
        "result": "COMPACT_HUD_SETUP_PASSED", "assets": list(OUTPUT_PATHS),
        "design_size": list(DESIGN_SIZE), "side_margin": SIDE_MARGIN,
        "bottom_margin": BOTTOM_MARGIN, "slot_spacing": SLOT_SPACING,
        "xp_parent": "UI_CombatCanvas", "slot_counts": {"skills": 5, "items": 4},
        "preserved_main_widgets": preserved_main_names or sorted(main_widgets),
        "widget_counts": {path: len(t) for path, t in trees.items()},
        "raster_frames_used": False, "native_bindings_verified": True,
        "property_inspection_sequence": "ObjectTools.list_properties -> ObjectTools.get_properties -> native setter",
        "property_inspections": list(PROPERTY_INSPECTIONS),
    }
    EVIDENCE_PATH.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE_PATH.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("RESULT=COMPACT_HUD_ASSET_VALIDATION_PASSED assets=7 evidence=" + str(EVIDENCE_PATH))
    return evidence


if __name__ == "__main__":
    main()
