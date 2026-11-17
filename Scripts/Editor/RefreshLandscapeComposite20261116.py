"""One public native full recomposition of the exact cropped Landscape only.

Recover derived composite allocations after the observed native Undo bug.
Never import, collapse layers, author raw height/paint, save, or delete.
"""
from datetime import datetime, timezone
import json
from pathlib import Path
import unreal

ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
EVIDENCE = ROOT / "Feature/doc/evidence/landscape-production-20261116"
OUT = EVIDENCE / "native-composite-refresh"


def require(value, message):
    if not value:
        raise RuntimeError(message)


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write(name, value):
    with (OUT / (name + ".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong project")
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
require(not editor.get_game_world(), "PIE must be stopped")
world = editor.get_editor_world()
require(world.get_path_name().split(".")[0] == MAP, "Wrong world")
OUT.mkdir(exist_ok=False)
helper = unreal.Rogue10mCrop20261114WorldAuthoring
terrain = json.loads(helper.inspect_candidate_terrain())
write("terrain-before", terrain)
require(terrain.get("success") and terrain.get("retained_9_by_5_lattice_observed"), "Exact retained45 required")
current = json.loads(helper.snapshot_retained_terrain_data())
write("snapshot-before", current)
baseline = read(EVIDENCE / "live-before/retained-height-paint-layers.json")
require(current.get("raw_edit_layers") == baseline["raw_edit_layers"], "Raw edit layers changed; do not rebuild")
require(not current.get("success") and current.get("error", "").startswith("Shared seam sample differs"),
        "Expected observed composite seam failure; no speculative refresh")
deps = json.loads(helper.inspect_candidate_dependencies())
write("dependencies-before", deps)
require(deps.get("raw_ba_complete") and deps.get("raw_edit_layer_ba") == read(EVIDENCE / "live-before/retained-dependencies.json")["raw_edit_layer_ba"],
        "Raw B/A changed; do not rebuild")
landscape = unreal.find_object(None, terrain["landscape_actor_path"])
require(isinstance(landscape, unreal.Landscape), "Exact Landscape instance missing")
write("request", {"utc": datetime.now(timezone.utc).isoformat(), "method": "Landscape.force_layers_full_update",
                  "target": landscape.get_path_name(), "calls": 1, "rawAuthoring": False,
                  "reason": "Native Undo shared composite weightmap allocation fixup; raw layers are unchanged"})
landscape.force_layers_full_update()
write("returned", {"utc": datetime.now(timezone.utc).isoformat(), "returned": True,
                   "saved": False, "requiresFreshIndependentComparison": True})
print("LANDSCAPE_NATIVE_FULL_REFRESH_20261116 RETURNED; inspect before save")
