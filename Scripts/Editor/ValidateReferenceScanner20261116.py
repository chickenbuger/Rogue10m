"""Bounded read-only verification of the saved candidate in a separate editor process."""
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import time
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[2]
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
command = unreal.SystemLibrary.get_command_line()
match = re.search(r"(?:^|\s)-RogueReferenceScanRun=([A-Za-z0-9-]+)(?=\s|$)", command)
if not match:
    raise RuntimeError("An explicit unique -RogueReferenceScanRun tag is required")
OUT = ROOT / "Feature/doc/evidence/reference-scanner-20261116" / match[1]
OUT.mkdir(parents=True, exist_ok=False)
state = {"started": time.monotonic(), "phase": "wait-registry", "handle": None}


def write(name, value):
    with (OUT / (name + ".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def tick(_delta):
    try:
        require(time.monotonic() - state["started"] < 150, "Reference validation timed out")
        require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong project")
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        require(not editor.get_game_world(), "No PIE/game session is permitted")
        world = editor.get_editor_world()
        require(world and world.get_path_name().split(".")[0] == MAP, "Wrong candidate world")
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        if registry.is_loading_assets():
            return
        helper = unreal.Rogue10mCrop20261114WorldAuthoring
        if state["phase"] == "wait-registry":
            write("context", {"project": str(ROOT), "map": MAP, "utc": datetime.now(timezone.utc).isoformat(),
                              "command_line": command, "assets_saved": False, "editor_process_only": True})
            result = json.loads(helper.load_candidate_region())
            write("load-region", result)
            require(result.get("success"), "Bounded region load failed: " + str(result.get("error")))
            state["phase"] = "inspect"
            return
        terrain = json.loads(helper.inspect_candidate_terrain())
        if terrain.get("component_count") != 45:
            return
        # Reading raw source mips requires the normal loaded Landscape state to have settled.
        snapshot = json.loads(helper.snapshot_retained_terrain_data())
        write("terrain", terrain)
        write("snapshot", snapshot)
        require(snapshot.get("success"), "Retained snapshot failed: " + str(snapshot.get("snapshot_errors", snapshot.get("error"))))
        result = json.loads(helper.inspect_candidate_dependencies())
        write("dependencies", result)
        require(result.get("success"), "Reference inspection failed: " + str(result.get("inspection_errors", result.get("error"))))
        require(result.get("loaded_reflection_walk_complete") is True, "Reflection scan is incomplete")
        require(not result.get("unsupported_reflected_properties"), "Unsupported properties remain")
        require(result.get("reference_scan_truncated") is False, "Reference scan was truncated")
        require(result.get("project_wide_dependency_complete") is False and result.get("crop_reference_safety_verified") is False,
                "Inspection must preserve the documented scope boundaries")
        checks = ("texture_sources_unchanged", "layer_metadata_unchanged", "landscape_selection_and_editing_layer_unchanged",
                  "editor_pointer_unchanged", "loaded_uobject_identities_and_flags_unchanged", "editor_object_selection_unchanged",
                  "transaction_queue_and_active_state_unchanged", "transaction_handle_readable", "dirty_package_names_unchanged")
        require(all(result.get(key) is True for key in checks), "Read-only preservation failed")
        write("result", {"passed": True, "unsupported_count": 0, "objects_scanned": result["reference_objects_scanned"],
                         "values_visited": result["reference_values_visited"], "preservation_checks": {key: result[key] for key in checks},
                         "supported_storage_counts": result["supported_reflected_storage_value_counts"],
                         "project_wide_dependency_complete": False, "assets_saved": False})
        print("ROGUE_REFERENCE_SCAN_VALIDATION PASS " + str(OUT))
    except Exception as exc:
        write("failure", {"error": str(exc), "traceback": traceback.format_exc(), "phase": state["phase"]})
        unreal.log_error("ROGUE_REFERENCE_SCAN_VALIDATION FAIL " + str(exc))
    else:
        # The explicit returns above keep the callback registered while preparing.
        pass
    unreal.unregister_slate_post_tick_callback(state["handle"])
    unreal.SystemLibrary.quit_editor()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
