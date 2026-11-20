"""Author the two inventory tooltip cards in their existing Widget Blueprint."""

from __future__ import annotations

import sys
from pathlib import Path

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from ApplyCompactCombatHUD import rounded_brush
from BuildMenuDesignerLayouts import (
    ASSETS,
    add,
    clear_tree,
    compile_and_save,
    require_asset,
    set_fill_alignment,
    validate_tree,
)


CARD_COLOR = unreal.LinearColor(0.012, 0.012, 0.012, 0.98)
BRASS = unreal.LinearColor(0.49, 0.32, 0.15, 1.0)
CREAM = unreal.LinearColor(0.92, 0.87, 0.79, 1.0)
GOLD = unreal.LinearColor(0.94, 0.68, 0.31, 1.0)
MUTED = unreal.LinearColor(0.60, 0.56, 0.50, 1.0)
RULE = unreal.LinearColor(0.37, 0.27, 0.16, 1.0)


def _card(bp, columns, name, width, left_gap=0.0, variable=False):
    frame, frame_slot = add(bp, unreal.Border, name, columns, variable=variable)
    frame_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
    frame_slot.set_padding(unreal.Margin(left_gap, 0.0, 0.0, 0.0))
    frame.set_brush(rounded_brush(CARD_COLOR, radius=8, outline=BRASS, width=1.6))
    frame.set_padding(unreal.Margin(18.0, 14.0, 18.0, 14.0))
    size, size_slot = add(bp, unreal.SizeBox, name + "Size", frame)
    size.set_width_override(width - 36.0)
    set_fill_alignment(size_slot)
    content, content_slot = add(bp, unreal.VerticalBox, name + "Content", size)
    set_fill_alignment(content_slot)
    return frame, content


def _text(bp, parent, name, sample, size, color=CREAM, *,
          variable=True, bold=False, wrap=False, top=0.0, bottom=0.0):
    widget, slot = add(bp, unreal.TextBlock, name, parent, variable=variable)
    widget.set_text(sample)
    widget.set_auto_wrap_text(wrap)
    font = widget.get_editor_property("font")
    font.size = size
    if bold:
        font.typeface_font_name = "Bold"
    widget.set_font(font)
    widget.set_color_and_opacity(unreal.SlateColor(specified_color=color))
    slot.set_padding(unreal.Margin(0.0, top, 0.0, bottom))
    return widget


def _rule(bp, parent, name, top, bottom):
    size, size_slot = add(bp, unreal.SizeBox, name + "Size", parent)
    size.set_height_override(1.0)
    size_slot.set_padding(unreal.Margin(0.0, top, 0.0, bottom))
    rule, rule_slot = add(bp, unreal.Image, name, size)
    rule.set_brush(rounded_brush(RULE, radius=0, outline=RULE, width=0.0))
    set_fill_alignment(rule_slot)


def _icon(bp, parent, name):
    image, slot = add(bp, unreal.Image, name, parent, variable=True)
    image.set_desired_size_override(unreal.Vector2D(52.0, 52.0))
    image.set_visibility(unreal.SlateVisibility.COLLAPSED)
    slot.set_padding(unreal.Margin(0.0, 0.0, 8.0, 0.0))


def build_inventory_concept_tooltip(bp):
    clear_tree(bp)
    size, _ = add(bp, unreal.SizeBox, "UI_ItemTooltipSize", variable=True)
    size.set_width_override(322.0)
    size.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    columns, columns_slot = add(bp, unreal.HorizontalBox, "UI_ItemTooltipColumns", size)
    set_fill_alignment(columns_slot)

    _, item = _card(bp, columns, "UI_ItemTooltipFrame", 322.0)
    header, _ = add(bp, unreal.HorizontalBox, "UI_ItemHeader", item)
    _icon(bp, header, "UI_ItemIcon")
    name_stack, name_slot = add(bp, unreal.VerticalBox, "UI_ItemIdentity", header)
    name_slot.set_size(unreal.SlateChildSize(value=1.0, size_rule=unreal.SlateSizeRule.FILL))
    _text(bp, name_stack, "UI_ItemNameText", "아이템 이름", 28, bold=True, wrap=True)
    kind = _text(bp, name_stack, "UI_ItemKindText", "", 19, top=3.0)
    kind.set_visibility(unreal.SlateVisibility.COLLAPSED)

    stats_section, _ = add(bp, unreal.VerticalBox, "UI_ItemStatsSection", item, variable=True)
    _rule(bp, stats_section, "UI_ItemStatsRule", 9.0, 7.0)
    add(bp, unreal.VerticalBox, "UI_ItemStatsContainer", stats_section, variable=True)

    action_section, _ = add(bp, unreal.VerticalBox, "UI_ItemActionSection", item, variable=True)
    _rule(bp, action_section, "UI_ItemActionRule", 8.0, 7.0)
    action = _text(bp, action_section, "UI_ItemActionHintText", "", 19, GOLD, bold=True)
    action.set_visibility(unreal.SlateVisibility.COLLAPSED)
    action_section.set_visibility(unreal.SlateVisibility.COLLAPSED)
    _text(bp, item, "UI_ItemDescriptionText", "", 12, MUTED, wrap=True, top=6.0)
    _text(bp, item, "UI_ItemWeightText", "", 12, MUTED, top=2.0)

    comparison_frame, comparison = _card(
        bp, columns, "UI_EquippedItemPanel", 234.0, left_gap=12.0, variable=True,
    )
    comparison_frame.set_visibility(unreal.SlateVisibility.COLLAPSED)
    _text(bp, comparison, "UI_EquippedItemTitleText", "현재 장착", 18, GOLD, variable=False)
    comparison_header, _ = add(bp, unreal.HorizontalBox, "UI_EquippedItemHeader", comparison)
    _icon(bp, comparison_header, "UI_EquippedItemIcon")
    equipped, equipped_slot = add(bp, unreal.VerticalBox, "UI_EquippedItemIdentity", comparison_header)
    equipped_slot.set_size(unreal.SlateChildSize(value=1.0, size_rule=unreal.SlateSizeRule.FILL))
    _text(bp, equipped, "UI_EquippedItemNameText", "장착 중인 장비", 24, wrap=True, top=5.0)
    _rule(bp, comparison, "UI_EquippedItemStatsRule", 9.0, 7.0)
    add(bp, unreal.VerticalBox, "UI_EquippedItemStatsContainer", comparison, variable=True)
    _text(bp, comparison, "UI_EquippedItemDescriptionText", "", 12, MUTED, wrap=True, top=6.0)
    _text(bp, comparison, "UI_ComparisonTitleText", "교체 시 변화", 12, GOLD,
          variable=False, top=5.0)
    add(bp, unreal.VerticalBox, "UI_ComparisonStatsContainer", comparison, variable=True)


def main():
    path = ASSETS["inventory_item_tooltip"]
    blueprint = require_asset(path)
    build_inventory_concept_tooltip(blueprint)
    validate_tree(blueprint, "inventory_item_tooltip", path)
    compile_and_save(blueprint, path)
    unreal.log("[Rogue10mInventoryConcept] Tooltip cards saved")


if __name__ == "__main__":
    main()
