"""Apply the inventory concept to the existing Inventory Widget Blueprints."""

from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import BuildInventoryConceptTooltip
import BuildMenuDesignerLayouts as menu


FRAME_NAME = "T_UI_InventoryConceptFrame"
TEXTURE_DIR = "/Game/UI/HUD/Gothic"
FRAME_WIDTH, FRAME_HEIGHT = 736.0, 856.0
CELL_SIZE = 58.0
GRID_EXTENT = CELL_SIZE * 10
GRID_POSITION = (77.0, 137.0)
GOLD = unreal.LinearColor(0.84, 0.62, 0.30, 1)
IVORY = unreal.LinearColor(0.96, 0.82, 0.56, 1)
DARK = unreal.LinearColor(0.008, 0.009, 0.011, 1)
BRASS = unreal.LinearColor(0.29, 0.17, 0.071, 1)
TRANSPARENT = unreal.LinearColor(0, 0, 0, 0)
WHITE = unreal.LinearColor(1, 1, 1, 1)


def import_frame():
    source = (
        Path(unreal.Paths.project_dir()).resolve()
        / "Feature/doc/evidence/inventory-ui-20261120"
        / f"{FRAME_NAME}.png"
    )
    path = f"{TEXTURE_DIR}/{FRAME_NAME}"
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not source.is_file():
            raise RuntimeError(f"Inventory frame source is missing: {source}")
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = TEXTURE_DIR
        task.destination_name = FRAME_NAME
        task.automated = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not texture:
        raise RuntimeError(f"Could not load inventory frame: {path}")
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property("never_stream", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save inventory frame: {path}")
    return texture


def place(slot, position, size, z=0):
    menu.set_canvas_layout(slot, position, size, anchors=(0, 0), alignment=(0, 0), z_order=z)


def get_widget(blueprint, name):
    for info in menu.umg("GetWidgets", blueprint).widgets:
        if info.widget and info.widget.get_name() == name:
            return info.widget, info.slot
    raise RuntimeError(f"Missing inventory designer widget: {name}")


def style_text(text, color, bold=False, align=unreal.TextJustify.LEFT):
    text.set_color_and_opacity(unreal.SlateColor(specified_color=color))
    text.set_editor_property("justification", align)
    text.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    if bold:
        font = text.get_editor_property("font")
        font.typeface_font_name = "Bold"
        text.set_editor_property("font", font)


def build_window(blueprint, texture):
    menu.clear_tree(blueprint)
    root, _ = menu.add(blueprint, unreal.CanvasPanel, "UI_InventoryCanvas")
    root.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    window, slot = menu.add(blueprint, unreal.CanvasPanel, "UI_WindowRoot", root, variable=True)
    place(slot, (56.0, 47.0), (FRAME_WIDTH, FRAME_HEIGHT))

    frame, slot = menu.make_border(blueprint, "UI_InventoryWindowFrame", window, WHITE, 0.0)
    frame.set_brush_from_texture(texture)
    place(slot, (0, 0), (FRAME_WIDTH, FRAME_HEIGHT))

    handle, slot = menu.add(blueprint, unreal.Border, "UI_WindowDragHandle", window, variable=True)
    handle.set_brush_color(TRANSPARENT)
    handle.set_padding(unreal.Margin())
    place(slot, (76, 49), (584, 56), z=10)
    title_row, row_slot = menu.add(blueprint, unreal.CanvasPanel, "UI_InventoryTitleRow", handle)
    menu.set_fill_alignment(row_slot)
    title_row.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    title, slot = menu.make_text(blueprint, "UI_InventoryTitleText", "인벤토리", title_row, font_size=29)
    place(slot, (48, 7), (430, 47))
    style_text(title, IVORY, bold=True)
    key_edge, slot = menu.make_border(blueprint, "UI_InventoryKeyEdge", title_row, GOLD, 1)
    place(slot, (514, 9), (41, 37))
    key_bg, slot = menu.make_border(blueprint, "UI_InventoryKeyBackground", key_edge, DARK, 0)
    menu.set_fill_alignment(slot)
    key, slot = menu.make_text(blueprint, "UI_InventoryKeyText", "I", key_bg, font_size=20)
    menu.set_fill_alignment(slot)
    menu.set_padding(slot, 0.0)
    style_text(key, IVORY, bold=True, align=unreal.TextJustify.CENTER)

    _, slot = menu.make_border(blueprint, "UI_InventoryGridFrame", window, DARK, 0)
    place(slot, GRID_POSITION, (GRID_EXTENT, GRID_EXTENT), z=1)
    grid, slot = menu.add(blueprint, unreal.UniformGridPanel, "UI_InventoryGrid", window, variable=True)
    grid.set_min_desired_slot_width(CELL_SIZE)
    grid.set_min_desired_slot_height(CELL_SIZE)
    place(slot, GRID_POSITION, (GRID_EXTENT, GRID_EXTENT), z=2)
    items, slot = menu.add(blueprint, unreal.CanvasPanel, "UI_InventoryItemCanvas", window, variable=True)
    items.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    place(slot, GRID_POSITION, (GRID_EXTENT, GRID_EXTENT), z=3)
    preview, slot = menu.add(blueprint, unreal.Border, "UI_InventoryDropPreview", window, variable=True)
    preview.set_visibility(unreal.SlateVisibility.COLLAPSED)
    place(slot, GRID_POSITION, (CELL_SIZE, CELL_SIZE), z=4)

    money, slot = menu.make_text(blueprint, "UI_InventoryMoneyText", "골드 0", window, variable=True, font_size=20)
    place(slot, (104, 754), (220, 36), z=2)
    style_text(money, IVORY, bold=True)
    weight, slot = menu.make_text(
        blueprint, "UI_InventoryWeightText", "무게 0.0 / 100.0 kg", window, variable=True, font_size=20
    )
    place(slot, (325, 754), (320, 36), z=2)
    style_text(weight, IVORY, align=unreal.TextJustify.RIGHT)


def build_cell(blueprint):
    menu.build_inventory_cell(blueprint)
    size, _ = get_widget(blueprint, "UI_InventoryCellSize")
    size.clear_width_override()
    size.clear_height_override()
    frame, _ = get_widget(blueprint, "UI_InventoryCellFrame")
    frame.set_brush_color(BRASS)
    frame.set_padding(unreal.Margin(0.0, 0.0, 2.0, 2.0))
    fill, _ = get_widget(blueprint, "UI_InventoryCellFill")
    fill.set_brush_color(DARK)


def build_item(blueprint):
    menu.build_inventory_item(blueprint)
    root, _ = get_widget(blueprint, "UI_InventoryItemRoot")
    root.set_column_fill(0, 1.0)
    root.set_row_fill(0, 1.0)
    quantity, slot = get_widget(blueprint, "UI_InventoryItemQuantityText")
    slot.set_padding(unreal.Margin(0, 0, 5, 2))
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
    style_text(quantity, WHITE, bold=True, align=unreal.TextJustify.RIGHT)

    marker, slot = menu.make_text(
        blueprint, "UI_InventoryItemPlacementMark", "", root, variable=True, font_size=18
    )
    slot.set_layer(3)
    slot.set_padding(unreal.Margin(0, 2, 5, 0))
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
    marker.set_shadow_offset(unreal.Vector2D(1, 1))
    marker.set_shadow_color_and_opacity(unreal.LinearColor(0, 0, 0, 0.9))
    marker.set_visibility(unreal.SlateVisibility.COLLAPSED)


def main():
    BuildInventoryConceptTooltip.main()
    texture = import_frame()
    for key, build in (("inventory_cell", build_cell), ("inventory_item", build_item)):
        path = menu.ASSETS[key]
        blueprint = menu.require_asset(path)
        build(blueprint)
        menu.validate_tree(blueprint, key, path)
        menu.compile_and_save(blueprint, path)

    path = menu.ASSETS["inventory"]
    window = menu.require_asset(path)
    build_window(window, texture)
    menu.validate_tree(window, "inventory", path)
    menu.compile_and_save(window, path)
    defaults = unreal.get_default_object(menu.load_blueprint_class(path))
    defaults.set_editor_property("inventory_cell_size", CELL_SIZE)
    defaults.set_editor_property("inventory_cell_widget_class", menu.load_blueprint_class(menu.ASSETS["inventory_cell"]))
    defaults.set_editor_property("inventory_item_widget_class", menu.load_blueprint_class(menu.ASSETS["inventory_item"]))
    menu.compile_and_save(window, path)
    unreal.log("ROGUE_INVENTORY_CONCEPT_ASSETS=OK")


if __name__ == "__main__":
    main()
