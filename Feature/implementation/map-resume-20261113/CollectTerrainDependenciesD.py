"""Collect bounded candidate-only observations; incomplete references never authorize crop."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
OUT = ROOT / "Feature/doc/evidence/map-resume-20261113/TerrainDependencyD"
assert Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve()
OUT.mkdir(exist_ok=False)


def write(name, data):
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def dirty():
    return {"maps": sorted(p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()),
            "content": sorted(p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())}


assert dirty() == {"maps": [], "content": []}
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and world.get_path_name() == MAP + ".WorldLayoutCompact20261113A"
helper = unreal.Rogue10mCompactWorldAuthoring
loaded = json.loads(helper.load_candidate_region())
write("region.json", loaded)
assert loaded.get("success")
terrain = json.loads(helper.inspect_candidate_terrain())
write("terrain.json", terrain)
before = json.loads(helper.snapshot_retained_terrain_data())
write("semantic-before.json", before)
assert before.get("success")
observed = json.loads(helper.inspect_candidate_dependencies())
write("dependencies.json", observed)
after = json.loads(helper.snapshot_retained_terrain_data())
write("semantic-after.json", after)
stable_fields = ("texture_sources_unchanged", "layer_metadata_unchanged",
                 "landscape_selection_and_editing_layer_unchanged", "editor_pointer_unchanged",
                 "loaded_uobject_identities_and_flags_unchanged", "editor_object_selection_unchanged",
                 "transaction_queue_and_active_state_unchanged", "dirty_package_names_unchanged")
stability = {field: observed.get(field) is True for field in stable_fields}
semantic_equal = bool(after.get("success") and before["semantic_snapshot_digest"] == after["semantic_snapshot_digest"])
record = {"status": "bounded_inventory_collected_crop_not_authorized",
          "collector_reported_success": observed.get("success"),
          "raw_ba_complete": observed.get("raw_ba_complete"), "stability": stability,
          "semantic_unchanged": semantic_equal, "dirty": dirty(),
          "reference_walk_complete": observed.get("loaded_reflection_walk_complete"),
          "inspection_errors": observed.get("inspection_errors", []),
          "all_descriptors_loaded": observed.get("all_world_actor_descriptors_loaded"),
          "crop_reference_safety_verified": False, "crop_performed": False,
          "assets_saved": False, "gui_input_operations": 0}
write("result.json", record)
assert observed.get("raw_ba_complete") and all(stability.values()) and semantic_equal and dirty() == {"maps": [], "content": []}, "Bounded inventory failed raw-data/stability checks; inspect retained evidence"
unreal.log("R10_TERRAIN_DEPENDENCY_BOUNDED_OBSERVATIONS_RECORDED_NO_CROP_AUTHORIZATION")
