"""Apply the integrated Gothic inventory, XP/level, and monster-info UI."""

from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path("D:/Project/Rogue10m")
SOURCE_DIR = PROJECT_ROOT / "Content/UI/HUD/Gothic"
DESTINATION_PATH = "/Game/UI/HUD/Gothic"

INVENTORY_PATH = "/Game/Widget/Component/Inventory/WBP_InventoryWindow"
INVENTORY_CELL_PATH = "/Game/Widget/Component/Inventory/WBP_InventoryCell"
LEVEL_PATH = "/Game/Widget/Parts/WBP_LevelExperiencePanel"
MONSTER_PATH = "/Game/Widget/Parts/WBP_MonsterInfo"
SKILL_PANEL_PATH = "/Game/Widget/Parts/WBP_SkillSlotPanel"
ITEM_PANEL_PATH = "/Game/Widget/Parts/WBP_ItemSlotPanel"
MAIN_HUD_PATH = "/Game/Widget/WBP_Rogue10mMainHUD"
SHOWCASE_PATH = "/Game/Widget/Debug/WBP_GothicUIShowcase"

TEXTURE_FILES = {
    "inventory": "T_UI_GothicInventoryFrame.png",
    "experience": "T_HUD_GothicXPBottomFrame.png",
    "monster": "T_HUD_GothicMonsterInfoFrame.png",
    "skill_wing": "T_HUD_GothicSkillWing.png",
    "item_wing": "T_HUD_GothicItemWing.png",
}

REQUIRED_BINDINGS = {
    INVENTORY_PATH: {
        "UI_WindowRoot",
        "UI_WindowDragHandle",
        "UI_InventoryGridFrame",
        "UI_InventoryGrid",
        "UI_InventoryItemCanvas",
        "UI_InventoryMoneyText",
        "UI_InventoryWeightText",
    },
    LEVEL_PATH: {"UI_LevelText", "UI_ExperienceBar", "UI_ExperienceText"},
    MONSTER_PATH: {
        "UI_MonsterInfoBackground",
        "UI_MonsterNameText",
        "UI_MonsterHealthBar",
        "UI_MonsterHealthText",
    },
    SKILL_PANEL_PATH: {
        "UI_GothicSkillWing",
        "UI_SkillSlotFrame1",
        "UI_SkillSlotFrame2",
        "UI_SkillSlotFrame3",
        "UI_SkillSlotFrame4",
        "UI_SkillSlotFrame5",
    },
    ITEM_PANEL_PATH: {
        "UI_GothicItemWing",
        "UI_ItemSlotFrame1",
        "UI_ItemSlotFrame2",
        "UI_ItemSlotFrame3",
        "UI_ItemSlotFrame4",
        "UI_ItemSlotFrame5",
    },
}

UMG_TOOL = unreal.get_default_object(unreal.UMGToolSet)


def umg(method: str, *args):
    return UMG_TOOL.call_method(method, args)


def load_widget(path: str):
    widget = unreal.EditorAssetLibrary.load_asset(path)
    if not widget:
        raise RuntimeError(f"Widget Blueprint not found: {path}")
    return widget


def widget_infos(widget_blueprint):
    return [info for info in umg("GetWidgets", widget_blueprint).widgets if info.widget]


def find_info(widget_blueprint, name: str):
    for info in widget_infos(widget_blueprint):
        if info.widget.get_name() == name:
            return info
    raise RuntimeError(f"Widget not found: {name}")


def find_widget(widget_blueprint, name: str):
    return find_info(widget_blueprint, name).widget


def configure_texture(texture) -> None:
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.modify()


def import_textures():
    tasks = []
    for filename in TEXTURE_FILES.values():
        source_path = SOURCE_DIR / filename
        if not source_path.exists():
            raise RuntimeError(f"Texture source missing: {source_path}")
        asset_path = f"{DESTINATION_PATH}/{source_path.stem}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source_path))
        task.set_editor_property("destination_path", DESTINATION_PATH)
        task.set_editor_property("destination_name", source_path.stem)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    textures = {}
    for key, filename in TEXTURE_FILES.items():
        path = f"{DESTINATION_PATH}/{Path(filename).stem}"
        texture = unreal.EditorAssetLibrary.load_asset(path)
        if not texture:
            raise RuntimeError(f"Texture import failed: {path}")
        configure_texture(texture)
        textures[key] = texture
    return textures


def set_font_size(text_block, size: int) -> None:
    font = text_block.get_editor_property("font")
    font.size = size
    text_block.set_editor_property("font", font)


def set_text_style(text_block, size: int, color: unreal.LinearColor) -> None:
    set_font_size(text_block, size)
    text_block.set_color_and_opacity(unreal.SlateColor(specified_color=color))
    text_block.set_shadow_offset(unreal.Vector2D(1.0, 1.0))
    text_block.set_shadow_color_and_opacity(unreal.LinearColor(0.0, 0.0, 0.0, 0.95))
    text_block.set_editor_property("justification", unreal.TextJustify.CENTER)


def set_canvas_slot(slot, *, anchors, offsets, alignment=(0.0, 0.0), z_order=0):
    slot.set_anchors(
        unreal.Anchors(
            minimum=unreal.Vector2D(anchors[0], anchors[1]),
            maximum=unreal.Vector2D(anchors[2], anchors[3]),
        )
    )
    slot.set_alignment(unreal.Vector2D(alignment[0], alignment[1]))
    slot.set_offsets(unreal.Margin(*offsets))
    slot.set_z_order(z_order)


def add(widget_blueprint, widget_class, name: str, parent=None, variable=False):
    info = umg("AddWidget", widget_blueprint, widget_class, name, parent, -1)
    if not info.widget:
        raise RuntimeError(f"Failed to add widget: {name}")
    if variable:
        umg("ToggleWidgetAsVariable", widget_blueprint, info.widget, True)
    return info.widget, info.slot


def clear_tree(widget_blueprint) -> None:
    roots = [
        info.widget
        for info in widget_infos(widget_blueprint)
        if not info.parent and not info.named_slot_host
    ]
    for root in roots:
        if not umg("RemoveWidget", widget_blueprint, root):
            raise RuntimeError(f"Failed to clear widget root: {root.get_name()}")


def apply_inventory(textures) -> None:
    inventory = load_widget(INVENTORY_PATH)
    frame = find_widget(inventory, "UI_InventoryWindowFrame")
    frame.set_brush_from_texture(textures["inventory"])
    frame.set_brush_color(unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    frame.set_padding(unreal.Margin(0.0, 0.0, 0.0, 0.0))

    drag_handle = find_widget(inventory, "UI_WindowDragHandle")
    drag_handle.set_brush_color(unreal.LinearColor(0.0, 0.0, 0.0, 0.0))

    grid_frame = find_widget(inventory, "UI_InventoryGridFrame")
    grid_frame.set_brush_color(unreal.LinearColor(0.008, 0.01, 0.014, 0.30))
    grid_frame.set_padding(unreal.Margin(4.0, 4.0, 4.0, 4.0))

    for name in ("UI_InventoryTitleText", "UI_InventoryMoneyText", "UI_InventoryWeightText"):
        text = find_widget(inventory, name)
        set_text_style(
            text,
            20 if name == "UI_InventoryTitleText" else 13,
            unreal.LinearColor(0.84, 0.72, 0.48, 1.0),
        )

    cell = load_widget(INVENTORY_CELL_PATH)
    find_widget(cell, "UI_InventoryCellFrame").set_brush_color(
        unreal.LinearColor(0.30, 0.22, 0.10, 0.88)
    )
    fill = find_widget(cell, "UI_InventoryCellFill")
    fill.set_brush_color(unreal.LinearColor(0.01, 0.012, 0.016, 0.72))

    compile_and_save(INVENTORY_CELL_PATH, cell)
    compile_and_save(INVENTORY_PATH, inventory)


def build_level_panel(textures) -> None:
    panel = load_widget(LEVEL_PATH)
    clear_tree(panel)
    canvas, _ = add(panel, unreal.CanvasPanel, "UI_LevelExperienceCanvas")

    frame, frame_slot = add(panel, unreal.Image, "UI_GothicXPLevelFrame", canvas)
    frame.set_brush_from_texture(textures["experience"], True)
    frame.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    set_canvas_slot(
        frame_slot,
        anchors=(0.0, 0.0, 1.0, 1.0),
        offsets=(0.0, 0.0, 0.0, 0.0),
        z_order=0,
    )

    progress, progress_slot = add(
        panel, unreal.ProgressBar, "UI_ExperienceBar", canvas, variable=True
    )
    progress.set_percent(0.37)
    progress.set_fill_color_and_opacity(unreal.LinearColor(0.04, 0.78, 0.16, 0.96))
    set_canvas_slot(
        progress_slot,
        anchors=(0.0, 0.0, 1.0, 0.0),
        offsets=(96.0, 21.0, 24.0, 13.0),
        z_order=1,
    )

    level, level_slot = add(panel, unreal.TextBlock, "UI_LevelText", canvas, variable=True)
    level.set_text("LV 1")
    set_text_style(level, 16, unreal.LinearColor(0.95, 0.78, 0.40, 1.0))
    set_canvas_slot(
        level_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(8.0, 6.0, 80.0, 36.0),
        z_order=2,
    )

    experience, experience_slot = add(
        panel, unreal.TextBlock, "UI_ExperienceText", canvas, variable=True
    )
    experience.set_text("37 / 100 XP")
    set_text_style(experience, 10, unreal.LinearColor(0.88, 0.84, 0.70, 1.0))
    set_canvas_slot(
        experience_slot,
        anchors=(0.0, 0.0, 1.0, 0.0),
        offsets=(98.0, 17.0, 26.0, 22.0),
        z_order=2,
    )

    compile_and_save(LEVEL_PATH, panel)


def apply_slot_wing(panel_path: str, texture, *, is_skill: bool) -> None:
    panel = load_widget(panel_path)
    canvas = next(
        info.widget
        for info in widget_infos(panel)
        if not info.parent and info.widget.get_class().get_name() == "CanvasPanel"
    )
    background_name = "UI_GothicSkillWing" if is_skill else "UI_GothicItemWing"
    for info in widget_infos(panel):
        if info.widget.get_name() == background_name:
            if not umg("RemoveWidget", panel, info.widget):
                raise RuntimeError(f"Failed to replace {background_name}")
            break

    background, background_slot = add(panel, unreal.Image, background_name, canvas)
    background.set_brush_from_texture(texture, True)
    background.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    set_canvas_slot(
        background_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(0.0, 0.0, 480.0, 104.0),
        z_order=0,
    )

    if is_skill:
        slot_box = find_info(panel, "UI_SkillSlotBox")
        set_canvas_slot(
            slot_box.slot,
            anchors=(0.0, 0.0, 0.0, 0.0),
            offsets=(200.0, 22.0, 280.0, 60.0),
            z_order=1,
        )
        for index in range(5):
            frame = find_info(panel, f"UI_SkillSlotFrame{index + 1}")
            set_canvas_slot(
                frame.slot,
                anchors=(0.0, 0.0, 0.0, 0.0),
                offsets=(204.0 + index * 54.0, 24.0, 48.0, 56.0),
                z_order=2,
            )
    else:
        slot_box = find_info(panel, "UI_ItemSlotBox")
        set_canvas_slot(
            slot_box.slot,
            anchors=(0.0, 0.0, 0.0, 0.0),
            offsets=(8.0, 24.0, 272.0, 56.0),
            z_order=2,
        )

    compile_and_save(panel_path, panel)


def apply_monster(textures) -> None:
    monster = load_widget(MONSTER_PATH)
    background = find_widget(monster, "UI_MonsterInfoBackground")
    background.set_brush_from_texture(textures["monster"])
    background.set_brush_color(unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    background.set_padding(unreal.Margin(0.0, 0.0, 0.0, 0.0))

    name = find_widget(monster, "UI_MonsterNameText")
    set_text_style(name, 16, unreal.LinearColor(0.90, 0.76, 0.48, 1.0))
    health = find_widget(monster, "UI_MonsterHealthBar")
    health.set_percent(0.68)
    health.set_fill_color_and_opacity(unreal.LinearColor(0.52, 0.025, 0.018, 0.92))
    health_text = find_widget(monster, "UI_MonsterHealthText")
    set_text_style(health_text, 11, unreal.LinearColor(0.92, 0.82, 0.66, 1.0))
    compile_and_save(MONSTER_PATH, monster)


def apply_main_hud_layout() -> None:
    main_hud = load_widget(MAIN_HUD_PATH)
    fixed_layout = {
        "HealthBarWidget": ((580.0, 900.0, 360.0, 34.0), 2),
        "StaminaBarWidget": ((980.0, 900.0, 360.0, 34.0), 2),
        "IdentityWidget": ((912.0, 874.0, 96.0, 116.0), 4),
        "SkillSlotPanelWidget": ((480.0, 936.0, 480.0, 104.0), 2),
        "ItemSlotPanelWidget": ((960.0, 936.0, 480.0, 104.0), 2),
        "UI_GothicBarLeftFrame": ((540.0, 882.0, 420.0, 76.0), 0),
        "UI_GothicBarRightFrame": ((960.0, 882.0, 420.0, 76.0), 0),
        "UI_GothicMedallionFrame": ((849.0, 812.0, 222.0, 197.0), 1),
    }
    for name, (layout, z_order) in fixed_layout.items():
        info = find_info(main_hud, name)
        set_canvas_slot(
            info.slot,
            anchors=(0.0, 0.0, 0.0, 0.0),
            offsets=layout,
            z_order=z_order,
        )

    # The shared vital widget receives its true fill color from runtime data.
    # These overlays make the two roles immediately readable in Designer while
    # remaining hit-test invisible; runtime bars render above them dynamically.
    preview_overlays = {
        "UI_HealthRolePreview": (
            (581.0, 908.0, 292.0, 16.0),
            unreal.LinearColor(0.82, 0.025, 0.018, 0.84),
        ),
        "UI_StaminaRolePreview": (
            (981.0, 908.0, 244.0, 16.0),
            unreal.LinearColor(0.90, 0.52, 0.015, 0.84),
        ),
    }
    for name, (layout, color) in preview_overlays.items():
        for info in widget_infos(main_hud):
            if info.widget.get_name() == name:
                if not umg("RemoveWidget", main_hud, info.widget):
                    raise RuntimeError(f"Failed to replace {name}")
                break
        preview, preview_slot = add(
            main_hud,
            unreal.Border,
            name,
            find_info(main_hud, "HealthBarWidget").parent,
        )
        preview.set_brush_color(color)
        preview.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        set_canvas_slot(
            preview_slot,
            anchors=(0.0, 0.0, 0.0, 0.0),
            offsets=layout,
            z_order=1,
        )

    progression = find_info(main_hud, "ProgressionWidget")
    set_canvas_slot(
        progression.slot,
        anchors=(0.0, 1.0, 1.0, 1.0),
        offsets=(0.0, -52.0, 0.0, 52.0),
        z_order=6,
    )

    old_frame = None
    for info in widget_infos(main_hud):
        if info.widget.get_name() == "UI_GothicExperienceFrame":
            old_frame = info.widget
            break
    if old_frame and not umg("RemoveWidget", main_hud, old_frame):
        raise RuntimeError("Failed to remove superseded Gothic experience ornament")

    if not unreal.EditorAssetLibrary.save_loaded_asset(
        main_hud, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Widget Blueprint save failed: {MAIN_HUD_PATH}")
    unreal.log(
        "[Rogue10mGothicUI] Applied and saved main HUD; compile it after any "
        "active Hot Reload session is cleared"
    )


def create_or_load_showcase():
    if unreal.EditorAssetLibrary.does_asset_exist(SHOWCASE_PATH):
        showcase = unreal.EditorAssetLibrary.load_asset(SHOWCASE_PATH)
        if showcase:
            return showcase
    parent_class = unreal.load_class(None, "/Script/UMG.UserWidget")
    if not parent_class:
        raise RuntimeError("UMG UserWidget parent class not found")
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    showcase = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WBP_GothicUIShowcase",
        SHOWCASE_PATH.rsplit("/", 1)[0],
        unreal.WidgetBlueprint,
        factory,
    )
    if not showcase:
        raise RuntimeError("Failed to create WBP_GothicUIShowcase")
    return showcase


def build_showcase() -> None:
    showcase = create_or_load_showcase()
    clear_tree(showcase)
    canvas, _ = add(showcase, unreal.CanvasPanel, "UI_ShowcaseCanvas")

    background, background_slot = add(showcase, unreal.Border, "UI_ShowcaseBackground", canvas)
    background.set_brush_color(unreal.LinearColor(0.012, 0.018, 0.016, 1.0))
    set_canvas_slot(
        background_slot,
        anchors=(0.0, 0.0, 1.0, 1.0),
        offsets=(0.0, 0.0, 0.0, 0.0),
        z_order=0,
    )

    main_class = unreal.EditorAssetLibrary.load_blueprint_class(MAIN_HUD_PATH)
    inventory_class = unreal.EditorAssetLibrary.load_blueprint_class(INVENTORY_PATH)
    if not main_class or not inventory_class:
        raise RuntimeError("Showcase child widget class is missing")

    _, main_slot = add(showcase, main_class, "UI_ShowcaseMainHUD", canvas)
    set_canvas_slot(
        main_slot,
        anchors=(0.0, 0.0, 1.0, 1.0),
        offsets=(0.0, 0.0, 0.0, 0.0),
        z_order=1,
    )
    _, inventory_slot = add(showcase, inventory_class, "UI_ShowcaseInventory", canvas)
    set_canvas_slot(
        inventory_slot,
        anchors=(0.0, 0.0, 1.0, 1.0),
        offsets=(0.0, 0.0, 0.0, 0.0),
        z_order=10,
    )
    compile_and_save(SHOWCASE_PATH, showcase)


def validate_bindings(path: str) -> None:
    widget = load_widget(path)
    names = {info.widget.get_name() for info in widget_infos(widget)}
    missing = sorted(REQUIRED_BINDINGS[path] - names)
    if missing:
        raise RuntimeError(f"Required UMG bindings missing in {path}: {', '.join(missing)}")


def compile_and_save(path: str, widget_blueprint) -> None:
    if not umg("CompileWidgetBlueprint", widget_blueprint):
        raise RuntimeError(f"Widget Blueprint compile failed: {path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        widget_blueprint, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Widget Blueprint save failed: {path}")
    unreal.log(f"[Rogue10mGothicUI] Applied and compiled: {path}")


def main() -> None:
    if not hasattr(unreal, "UMGToolSet"):
        raise RuntimeError("UE5.8 UMGToolSet plugin is not loaded")
    textures = import_textures()
    apply_inventory(textures)
    build_level_panel(textures)
    apply_slot_wing(SKILL_PANEL_PATH, textures["skill_wing"], is_skill=True)
    apply_slot_wing(ITEM_PANEL_PATH, textures["item_wing"], is_skill=False)
    apply_monster(textures)
    apply_main_hud_layout()
    for path in REQUIRED_BINDINGS:
        validate_bindings(path)
    build_showcase()
    for texture in textures.values():
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("[Rogue10mGothicUI] Integrated Gothic UI application completed")


if __name__ == "__main__":
    main()
