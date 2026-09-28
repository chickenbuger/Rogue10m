"""Build only the martial-arts skill-tree entry and window Widget Blueprints."""

import sys
from pathlib import Path

import unreal


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import BuildMenuDesignerLayouts as base


ENTRY_PATH = "/Game/Widget/Component/SkillTree/WBP_SkillTreeEntry"
WINDOW_PATH = "/Game/Widget/Component/SkillTree/WBP_SkillTreeWindow"
FRAME_PATH = "/Game/UI/MartialArts/T_UI_MartialArtsSkillTreeFrame"

IRON = unreal.LinearColor(0.018, 0.022, 0.021, 0.95)
IRON_RAISED = unreal.LinearColor(0.045, 0.052, 0.046, 0.94)
BRASS = unreal.LinearColor(0.42, 0.30, 0.13, 0.90)
JADE = unreal.LinearColor(0.20, 0.68, 0.42, 1.0)


def set_font_size(text, size: int) -> None:
    font = text.get_editor_property("font")
    font.size = size
    text.set_editor_property("font", font)


def set_text_color(text, color) -> None:
    text.set_editor_property("color_and_opacity", unreal.SlateColor(specified_color=color))


def add_canvas_text(widget, name, label, parent, position, size, font_size=16,
                    color=unreal.LinearColor(0.86, 0.83, 0.73, 1.0), variable=False,
                    alignment=(0.5, 0.5), z=3):
    text, slot = base.make_text(
        widget, name, label, parent, variable=variable, wrap=True, font_size=font_size
    )
    set_text_color(text, color)
    base.set_canvas_layout(slot, position, size, alignment=alignment, z_order=z)
    return text


def build_entry(widget) -> None:
    base.clear_tree(widget)
    size_box, _ = base.add(widget, unreal.SizeBox, "UI_SkillTreeEntrySize")
    size_box.set_width_override(156.0)
    size_box.set_height_override(118.0)
    outer, _ = base.make_border(widget, "UI_SkillTreeEntryOuter", size_box, BRASS, 2.0)
    frame, _ = base.make_border(widget, "UI_SkillTreeEntryFrame", outer, IRON_RAISED, 5.0)
    column, _ = base.add(widget, unreal.VerticalBox, "UI_SkillTreeEntryRoot", frame)

    tier, tier_slot = base.make_text(
        widget, "UI_SkillTierText", "입문", column, variable=True, font_size=10
    )
    set_text_color(tier, unreal.LinearColor(0.72, 0.58, 0.28, 1.0))
    tier.set_editor_property("justification", unreal.TextJustify.CENTER)
    base.set_fill_alignment(tier_slot)

    icon, icon_slot = base.add(
        widget, unreal.Image, "UI_SkillIconImage", column, variable=True
    )
    icon.set_desired_size_override(unreal.Vector2D(46.0, 46.0))
    if hasattr(icon_slot, "set_horizontal_alignment"):
        icon_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)

    name, name_slot = base.make_text(
        widget, "UI_SkillNameText", "연환권", column, variable=True, font_size=14
    )
    name.set_editor_property("justification", unreal.TextJustify.CENTER)
    set_text_color(name, unreal.LinearColor(0.94, 0.91, 0.80, 1.0))
    base.set_fill_alignment(name_slot)

    description, _ = base.make_text(
        widget, "UI_SkillDescriptionText", "무공 설명", column,
        variable=True, wrap=True, font_size=8
    )
    description.set_visibility(unreal.SlateVisibility.COLLAPSED)

    progress, progress_slot = base.add(
        widget, unreal.ProgressBar, "UI_SkillProgressBar", column, variable=True
    )
    progress.set_percent(0.35)
    progress.set_fill_color_and_opacity(JADE)
    base.set_padding(progress_slot, 2.0)

    status_row, _ = base.add(widget, unreal.HorizontalBox, "UI_SkillStatusRow", column)
    status, _ = base.make_text(
        widget, "UI_SkillLockText", "잠김", status_row, variable=True, font_size=9
    )
    set_text_color(status, unreal.LinearColor(0.72, 0.63, 0.44, 1.0))
    progress_text, _ = base.make_text(
        widget, "UI_SkillProgressText", "수련 35%", status_row, variable=True, font_size=9
    )
    set_text_color(progress_text, unreal.LinearColor(0.48, 0.78, 0.58, 1.0))


def add_panel(widget, root, name, position, size):
    outer, outer_slot = base.make_border(widget, f"{name}Outer", root, BRASS, 2.0)
    base.set_canvas_layout(outer_slot, position, size, z_order=2)
    inner, _ = base.make_border(widget, name, outer, IRON, 12.0)
    column, _ = base.add(widget, unreal.VerticalBox, f"{name}Column", inner)
    return column


def build_window(widget) -> None:
    base.clear_tree(widget)
    root, _ = base.add(widget, unreal.CanvasPanel, "UI_SkillTreeCanvas")
    window_root, window_slot = base.add(
        widget, unreal.CanvasPanel, "UI_WindowRoot", root, variable=True
    )
    base.set_canvas_layout(window_slot, (0, 0), (1500, 840), z_order=0)

    background, background_slot = base.add(
        widget, unreal.Image, "UI_MartialArtsBackgroundImage", window_root
    )
    background.set_brush_from_texture(base.require_asset(FRAME_PATH), False)
    base.set_canvas_layout(background_slot, (0, 0), (1500, 840), z_order=0)

    drag, drag_slot = base.add(
        widget, unreal.Border, "UI_WindowDragHandle", window_root, variable=True
    )
    drag.set_brush_color(unreal.LinearColor(0.02, 0.02, 0.018, 0.32))
    drag.set_padding(unreal.Margin(0, 0, 0, 0))
    base.set_canvas_layout(drag_slot, (0, -375), (1380, 62), z_order=2)

    add_canvas_text(
        widget, "UI_SkillTreeTitleText", "권사 무공 수련", window_root,
        (0, -380), (700, 48), font_size=28,
        color=unreal.LinearColor(0.86, 0.72, 0.38, 1.0), variable=True, z=3
    )
    add_canvas_text(
        widget, "UI_SkillTreeGuideText",
        "수련과 토벌로 경맥을 열고, 해금한 공격 무공을 하단 HUD 슬롯으로 끌어 장착하세요.",
        window_root, (0, -342), (900, 28), font_size=13,
        color=unreal.LinearColor(0.64, 0.70, 0.62, 1.0), z=3
    )

    left = add_panel(widget, window_root, "UI_MasteryPanel", (-610, 25), (250, 660))
    heading, _ = base.make_text(widget, "UI_MasteryHeading", "권맥 경지", left, font_size=20)
    set_text_color(heading, unreal.LinearColor(0.86, 0.72, 0.38, 1.0))
    active, _ = base.make_text(
        widget, "UI_ActiveWeaponText", "활성 무기 · 권갑", left, variable=True, font_size=12
    )
    set_text_color(active, unreal.LinearColor(0.52, 0.82, 0.62, 1.0))
    stage, _ = base.make_text(
        widget, "UI_MartialArtStageText", "현재 경지 · 입문", left, variable=True, font_size=17
    )
    set_text_color(stage, unreal.LinearColor(0.92, 0.90, 0.82, 1.0))
    mastery, _ = base.add(
        widget, unreal.ProgressBar, "UI_MasteryProgressBar", left, variable=True
    )
    mastery.set_percent(0.10)
    mastery.set_fill_color_and_opacity(JADE)
    mastery_text, _ = base.make_text(
        widget, "UI_MasteryProgressText", "해금 무공 1 / 10", left, variable=True, font_size=12
    )
    set_text_color(mastery_text, unreal.LinearColor(0.62, 0.78, 0.66, 1.0))
    for title, body in (
        ("입문", "호흡과 연환의 기초"),
        ("초식", "붕권 · 파진각 · 철산고"),
        ("절기", "변화수와 연속 경력"),
        ("심법", "금강불괴와 문파 오의"),
    ):
        tier_title, _ = base.make_text(widget, f"UI_{title}Heading", title, left, font_size=15)
        set_text_color(tier_title, unreal.LinearColor(0.78, 0.62, 0.30, 1.0))
        tier_body, _ = base.make_text(widget, f"UI_{title}Body", body, left, wrap=True, font_size=11)
        set_text_color(tier_body, unreal.LinearColor(0.62, 0.62, 0.56, 1.0))

    tree_canvas, tree_slot = base.add(
        widget, unreal.CanvasPanel, "UI_SkillListContainer", window_root, variable=True
    )
    base.set_canvas_layout(tree_slot, (-15, 42), (850, 650), z_order=3)
    for label, y in (("심법", -255), ("절기", -85), ("초식", 80), ("입문", 235)):
        add_canvas_text(
            widget, f"UI_TierLabel_{label}", label, window_root,
            (-430, y), (58, 24), font_size=11,
            color=unreal.LinearColor(0.46, 0.38, 0.23, 0.9), z=2
        )

    right = add_panel(widget, window_root, "UI_DetailPanel", (610, 25), (250, 660))
    detail_heading, _ = base.make_text(widget, "UI_DetailHeading", "무공 비급", right, font_size=20)
    set_text_color(detail_heading, unreal.LinearColor(0.86, 0.72, 0.38, 1.0))
    selected_icon, selected_icon_slot = base.add(
        widget, unreal.Image, "UI_SelectedSkillIcon", right, variable=True
    )
    selected_icon.set_desired_size_override(unreal.Vector2D(82.0, 82.0))
    if hasattr(selected_icon_slot, "set_horizontal_alignment"):
        selected_icon_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    selected_name, _ = base.make_text(
        widget, "UI_SelectedSkillNameText", "연환권", right, variable=True, font_size=22
    )
    selected_name.set_editor_property("justification", unreal.TextJustify.CENTER)
    set_text_color(selected_name, unreal.LinearColor(0.94, 0.91, 0.80, 1.0))
    selected_tier, _ = base.make_text(
        widget, "UI_SelectedSkillTierText", "입문 · 공격 무공", right, variable=True, font_size=12
    )
    selected_tier.set_editor_property("justification", unreal.TextJustify.CENTER)
    set_text_color(selected_tier, unreal.LinearColor(0.64, 0.78, 0.62, 1.0))
    status, _ = base.make_text(
        widget, "UI_SelectedSkillStatusText", "● 해금 완료", right, variable=True, font_size=13
    )
    set_text_color(status, JADE)
    description, _ = base.make_text(
        widget, "UI_SelectedSkillDescriptionText",
        "호흡을 짧게 끊으며 좌우 권을 연달아 내지르는 권사의 입문 초식.",
        right, variable=True, wrap=True, font_size=12
    )
    set_text_color(description, unreal.LinearColor(0.78, 0.76, 0.68, 1.0))
    condition_heading, _ = base.make_text(widget, "UI_ConditionHeading", "전수 조건", right, font_size=14)
    set_text_color(condition_heading, unreal.LinearColor(0.86, 0.68, 0.32, 1.0))
    conditions, _ = base.make_text(
        widget, "UI_SelectedSkillConditionsText", "문파 입문 시 기본 전수",
        right, variable=True, wrap=True, font_size=12
    )
    set_text_color(conditions, unreal.LinearColor(0.60, 0.80, 0.64, 1.0))
    reward_heading, _ = base.make_text(widget, "UI_RewardHeading", "해금 효과", right, font_size=14)
    set_text_color(reward_heading, unreal.LinearColor(0.86, 0.68, 0.32, 1.0))
    reward, _ = base.make_text(
        widget, "UI_SelectedSkillRewardText", "권사 기본 공격으로 즉시 사용할 수 있다.",
        right, variable=True, wrap=True, font_size=12
    )
    set_text_color(reward, unreal.LinearColor(0.76, 0.76, 0.68, 1.0))


def validate(widget, required):
    tree = base.umg("GetWidgets", widget)
    names = {entry.widget.get_name() for entry in tree.widgets if entry.widget}
    missing = sorted(required - names)
    if missing:
        raise RuntimeError(f"Missing widgets: {', '.join(missing)}")


def main() -> None:
    entry = base.require_asset(ENTRY_PATH)
    window = base.require_asset(WINDOW_PATH)
    build_entry(entry)
    build_window(window)
    validate(entry, {
        "UI_SkillIconImage", "UI_SkillNameText", "UI_SkillDescriptionText", "UI_SkillLockText",
        "UI_SkillTierText", "UI_SkillProgressText", "UI_SkillProgressBar",
    })
    validate(window, {
        "UI_WindowRoot", "UI_WindowDragHandle", "UI_SkillListContainer",
        "UI_SelectedSkillNameText", "UI_SelectedSkillConditionsText", "UI_MasteryProgressBar",
    })
    base.compile_and_save(entry, ENTRY_PATH)
    base.compile_and_save(window, WINDOW_PATH)
    window_class = base.load_blueprint_class(WINDOW_PATH)
    entry_class = base.load_blueprint_class(ENTRY_PATH)
    unreal.get_default_object(window_class).set_editor_property(
        "skill_tree_entry_widget_class", entry_class
    )
    base.compile_and_save(window, WINDOW_PATH)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("RESULT=MARTIAL_ARTIST_SKILL_TREE_WIDGETS_BUILT entry=1 window=1 nodes=dynamic")


if __name__ == "__main__":
    main()
