from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(unreal.Paths.project_dir()).resolve() / "Scripts" / "Editor"))
import ApplyInventoryConcept as design
import ValidateMenuWidgetAssets as menu_checks

menu_checks.main()
blueprint = design.menu.require_asset(design.menu.ASSETS["inventory"])
frame, _ = design.get_widget(blueprint, "UI_InventoryWindowFrame")
texture = frame.get_editor_property("background").get_editor_property("resource_object")
assert texture.get_name() == design.FRAME_NAME
assert (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) == (1162, 1353)
assert texture.get_editor_property("lod_group") == unreal.TextureGroup.TEXTUREGROUP_UI
_, grid = design.get_widget(blueprint, "UI_InventoryGrid")
_, items = design.get_widget(blueprint, "UI_InventoryItemCanvas")
assert grid.get_position() == items.get_position()
assert grid.get_size() == items.get_size() == unreal.Vector2D(580, 580)
defaults = unreal.get_default_object(design.menu.load_blueprint_class(design.menu.ASSETS["inventory"]))
assert defaults.get_editor_property("inventory_cell_size") == 58.0
cell = design.menu.require_asset(design.menu.ASSETS["inventory_cell"])
cell_frame, _ = design.get_widget(cell, "UI_InventoryCellFrame")
padding = cell_frame.get_editor_property("padding")
assert (padding.right, padding.bottom) == (2.0, 2.0)
unreal.log("RESULT=INVENTORY_CONCEPT_COLD_COMPILE_OK")
