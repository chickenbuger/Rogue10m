"""Read-only editor inventory for the basic-map start-village task."""
import json
from pathlib import Path
import unreal


def main():
    root = Path(unreal.Paths.project_dir()).resolve()
    if str(root).replace("\\", "/").lower() != "d:/project/rogue10m":
        raise RuntimeError("This inspection belongs to the Rogue10m project only.")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    wanted = (
        "SM_Plaster_FullXFull_Plaster_FullXFull", "SM_Wood_Wall_BeamCross_FullXFull_LP",
        "SM_Wood_Wall_RoofFront_Large_LP", "SM_RoofTiles_Round_FullXFull", "SM_Door_01",
        "SM_Window_FrenchPanes_01_LP", "SM_StoneFloor_01_Full", "Tree_Medium_Green",
        "Tree_Large_Green", "SM_Boulder_Medium_01", "SM_Beam_Long_1", "SM_Plaster_Chimney_Thin",
        "SM_SM_Barrel_01", "SM_Crate_01", "SM_DoorFrame_01", "SM_Stone_Arch_Full", "BP_Lantern_Box",
    )
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    asset_data = registry.get_assets_by_path("/Game/Stylized_Village", recursive=True)
    catalog = []
    for data in asset_data:
        if str(data.asset_name) not in wanted:
            continue
        asset = data.get_asset()
        row = {"path": asset.get_path_name(), "class": asset.get_class().get_name()}
        if isinstance(asset, unreal.StaticMesh):
            bounds = asset.get_bounds()
            row["origin"] = [bounds.origin.x, bounds.origin.y, bounds.origin.z]
            row["size"] = [bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2]
            row["materials"] = [str(entry.material_interface.get_path_name()) if entry.material_interface else None
                                for entry in asset.get_editor_property("static_materials")]
        catalog.append(row)
    actor_rows = []
    for actor in actors:
        row = {"path": actor.get_path_name(), "label": actor.get_actor_label(),
               "class": actor.get_class().get_name(),
               "location": [actor.get_actor_location().x, actor.get_actor_location().y, actor.get_actor_location().z]}
        if isinstance(actor, unreal.StaticMeshActor):
            mesh = actor.static_mesh_component.static_mesh
            row["mesh"] = mesh.get_path_name() if mesh else None
        actor_rows.append(row)
    settings = world.get_world_settings()
    game_mode = settings.get_editor_property("default_game_mode")
    gm = unreal.get_default_object(game_mode) if game_mode else None
    result = {"world": world.get_path_name(), "actors": actor_rows, "catalog": catalog,
              "game_mode": game_mode.get_path_name() if game_mode else None,
              "gm_auto_run": gm.get_editor_property("start_run_on_start_play") if gm else None,
              "gm_prototype": gm.get_editor_property("use_prototype_run_duration") if gm else None}
    destination = root / "Feature/doc/evidence/start-village-20261005/inputs.json"
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[StartVillageInspection] " + json.dumps({"world": result["world"], "actors": len(actors),
                                                        "catalog": len(catalog), "game_mode": result["game_mode"]}))


if __name__ == "__main__":
    main()
