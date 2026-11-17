"""Read the saved compact candidate through UE's Python commandlet; never save assets."""
from __future__ import annotations

import json
import os
from pathlib import Path
import re

import unreal


ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
LABEL = os.environ.get("R10_COMPACT_SNAPSHOT_LABEL", "TerrainSnapshotA")
assert re.fullmatch(r"TerrainSnapshot[A-Za-z0-9]+", LABEL), "Unsafe evidence label"
OUT = ROOT / "Feature/doc/evidence/map-resume-20261113" / LABEL


def write(name, value):
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def dirty():
    return {
        "maps": sorted(p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()),
        "content": sorted(p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()),
    }


assert Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Wrong project"
OUT.mkdir(exist_ok=False)
initial_dirty = dirty()
assert initial_dirty == {"maps": [], "content": []}, "Preexisting dirty packages"
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and world.get_path_name() == MAP + ".WorldLayoutCompact20261113A", "Wrong world"
write("identity.json", {"world": world.get_path_name(), "initial_dirty": initial_dirty,
                        "execution": "official Python commandlet, NullRHI, no GUI input"})
helper = unreal.Rogue10mCompactWorldAuthoring
loaded = json.loads(helper.load_candidate_region())
write("region.json", loaded)
inspection = json.loads(helper.inspect_candidate_terrain())
write("terrain.json", inspection)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
actor_rows = []
for actor in actors:
    location = actor.get_actor_location()
    origin, extent = actor.get_actor_bounds(False)
    actor_rows.append({"path": actor.get_path_name(), "label": actor.get_actor_label(),
                       "class": actor.get_class().get_path_name(),
                       "location_cm": [location.x, location.y, location.z],
                       "bounds_origin_cm": [origin.x, origin.y, origin.z],
                       "bounds_extent_cm": [extent.x, extent.y, extent.z]})
settings = world.get_world_settings()
mode = settings.get_editor_property("default_game_mode")
write("actors.json", {"default_game_mode": mode.get_path_name() if mode else None,
                      "loaded_actor_count": len(actor_rows), "actors": actor_rows,
                      "boundary": "Loaded editor actors; runtime spawn/physics/travel not tested"})
snapshot = json.loads(helper.snapshot_retained_terrain_data())
write("retained-data.json", snapshot)
preview = json.loads(helper.select_outside_components(False))
write("selection-preview.json", preview)
final_dirty = dirty()
passed = (loaded.get("success") is True and inspection.get("original_layout_contract_passed") is True
          and snapshot.get("success") is True and final_dirty == initial_dirty)
write("result.json", {"success": passed, "region_loaded": loaded.get("success"),
                      "original_layout_contract": inspection.get("original_layout_contract_passed"),
                      "semantic_snapshot_success": snapshot.get("success"),
                      "initial_dirty": initial_dirty, "final_dirty": final_dirty,
                      "packages_saved": False, "crop_performed": False,
                      "selection_performed": preview.get("selection_performed"),
                      "gui_input_operations": 0})
assert passed, "Read-only terrain snapshot incomplete; inspect retained evidence; nothing was saved"
unreal.log("R10_COMPACT_READONLY_SNAPSHOT_COMPLETE")
