"""Save only the exact inspected candidate's dirty native terrain packages."""
from datetime import datetime, timezone
import json
from pathlib import Path
import unreal

ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
EVIDENCE = ROOT / "Feature/doc/evidence/landscape-production-20261116"
OUT = EVIDENCE / "save-candidate"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def write(name, data):
    with (OUT / (name + ".json")).open("x", encoding="utf-8") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong project")
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
require(not editor.get_game_world(), "PIE must be stopped")
world = editor.get_editor_world()
require(world and world.get_path_name().split(".")[0] == MAP, "Wrong candidate")
comparison = json.loads((EVIDENCE / "after-native-refresh/comparison.json").read_text(encoding="utf-8-sig"))
require(comparison.get("passed") is True and comparison.get("componentCount") == 45, "Current preservation validation required")
OUT.mkdir(exist_ok=False)
helper = unreal.Rogue10mCrop20261114WorldAuthoring
terrain = json.loads(helper.inspect_candidate_terrain())
write("terrain-before-save", terrain)
require(terrain.get("success") and terrain.get("retained_9_by_5_lattice_observed"), "Exact cropped terrain required")
snapshot = json.loads(helper.snapshot_retained_terrain_data())
write("snapshot-before-save", snapshot)
baseline = json.loads((EVIDENCE / "live-before/retained-height-paint-layers.json").read_text(encoding="utf-8-sig"))
require(snapshot.get("success") and snapshot.get("semantic_snapshot_digest") == baseline["semantic_snapshot_digest"],
        "Fresh semantic snapshot changed; nothing saved")
allowed = {MAP} | {proxy["package"] for proxy in terrain["proxies"]}
packages = {p.get_name(): p for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
            + list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())}
write("request", {"utc": datetime.now(timezone.utc).isoformat(), "candidate": MAP,
                  "dirtyPackages": sorted(packages), "allowedTerrainPackages": sorted(allowed),
                  "nativeUndoCompositeIssuePreserved": True, "nativeRefreshRecoveryVerified": True})
require(packages and all(name in allowed for name in packages), "Unknown dirty package; save aborted")
success = unreal.EditorLoadingAndSavingUtils.save_packages(list(packages.values()), True)
remaining = sorted(p.get_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
                   + list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
write("result", {"utc": datetime.now(timezone.utc).isoformat(), "saveReturned": bool(success),
                 "savedPackageNames": sorted(packages), "remainingDirtyPackages": remaining,
                 "coldReloadVerified": False})
require(success and not remaining, "Save failed or packages remain dirty; inspect before reload")
print("LANDSCAPE_CROP_SAVE_20261116 SUCCESS " + str(len(packages)) + " exact terrain packages")
