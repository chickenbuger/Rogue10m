"""Read-only per-stage native crop evidence; never deletes, saves or travels."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import unreal

ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
EVIDENCE = ROOT / "Feature/doc/evidence/landscape-production-20261116"
BASE = EVIDENCE / "live-before"
PREFIXES = ("/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/",
            "/Game/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write(folder, name, value):
    with (folder / (name + ".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def dirty():
    return {"maps": sorted(p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()),
            "content": sorted(p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())}


def actor_inventory():
    rows = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        p, r, s = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
        rows.append({"path": actor.get_path_name(), "class": actor.get_class().get_path_name(),
                     "label": actor.get_actor_label(), "location": [p.x,p.y,p.z],
                     "rotation": [r.pitch,r.yaw,r.roll], "scale": [s.x,s.y,s.z],
                     "tags": [str(t) for t in actor.tags]})
    return sorted(rows, key=lambda row: row["path"])


def keyed(rows, key):
    return {row[key]: row for row in rows}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", required=True, choices=("before-delete", "after-delete", "after-undo", "after-redo", "after-native-refresh", "after-reload", "cold-reload"))
    stage = parser.parse_args().stage
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong normal project")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not editor.get_game_world(), "PIE must be stopped")
    world = editor.get_editor_world()
    require(world and world.get_path_name().split(".")[0] == MAP, "Wrong candidate world")
    folder = EVIDENCE / stage
    folder.mkdir(exist_ok=False)
    before = dirty()
    write(folder, "context", {"stage": stage, "project": str(ROOT), "map": MAP,
          "utc": datetime.now(timezone.utc).isoformat(), "dirtyBefore": before})
    require(all(p == MAP or p.startswith(PREFIXES) for p in before["maps"] + before["content"]),
            "Unrelated dirty package; preserve without saving")
    if stage == "before-delete":
        require(not before["maps"] and not before["content"], "Fresh pre-delete state must be clean")
    helper = unreal.Rogue10mCrop20261114WorldAuthoring
    terrain = json.loads(helper.inspect_candidate_terrain())
    write(folder, "terrain", terrain)
    require(terrain.get("success"), "Native terrain inspection failed")
    original = stage in ("before-delete", "after-undo")
    require(terrain.get("original_layout_contract_passed") if original else terrain.get("retained_9_by_5_lattice_observed"),
            "Unexpected original/retained lattice")
    if stage == "before-delete":
        require(terrain.get("selected_component_count") == 211, "Exact211 selection must still be present")
    snapshot = json.loads(helper.snapshot_retained_terrain_data())
    write(folder, "retained-height-paint-layers", snapshot)
    require(snapshot.get("success") and snapshot.get("all_requested_data_complete"),
            "Retained snapshot incomplete; snapshot_errors="
            + json.dumps(snapshot.get("snapshot_errors", []), ensure_ascii=False))
    dependencies = json.loads(helper.inspect_candidate_dependencies())
    write(folder, "retained-dependencies", dependencies)
    actors = actor_inventory()
    write(folder, "actors", actors)
    base_snapshot = read(BASE / "retained-height-paint-layers.json")
    base_dependencies = read(BASE / "retained-dependencies.json")
    base_actors = read(BASE / "actors.json")
    checks = {"metadataDigestUnchanged": snapshot["metadata_digest"] == base_snapshot["metadata_digest"],
              "semanticDigestUnchanged": snapshot["semantic_snapshot_digest"] == base_snapshot["semantic_snapshot_digest"],
              "compositeHeightAndPaintUnchanged": snapshot["composite"] == base_snapshot["composite"],
              "rawHeightAndPaintUnchanged": snapshot["raw_edit_layers"] == base_snapshot["raw_edit_layers"],
              "layerMetadataUnchanged": snapshot["edit_layers"] == base_snapshot["edit_layers"],
              "paintLayerDefinitionsUnchanged": keyed(snapshot["paint_layers"], "layer_info_path") == keyed(base_snapshot["paint_layers"], "layer_info_path"),
              "rawBAComplete": dependencies.get("raw_ba_complete") is True,
              "rawBAUnchanged": dependencies.get("raw_edit_layer_ba") == base_dependencies["raw_edit_layer_ba"],
              "rawBALayerMetadataUnchanged": dependencies.get("layer_metadata_digest") == base_dependencies["layer_metadata_digest"],
              "landscapeGuidUnchanged": snapshot["landscape_guid"] == base_snapshot["landscape_guid"],
              "landscapeActorUnchanged": snapshot["landscape_actor_path"] == base_snapshot["landscape_actor_path"],
              "materialUnchanged": snapshot["material_path"] == base_snapshot["material_path"],
              "retainedComponentsUnchanged": [c for c in snapshot["components"] if c["keep_in_9_by_5"]] == [c for c in base_snapshot["components"] if c["keep_in_9_by_5"]],
              "nonLandscapeActorsUnchanged": [a for a in actors if not a["class"].startswith("/Script/Landscape.")] == [a for a in base_actors if not a["class"].startswith("/Script/Landscape.")]}
    for key in ("texture_sources_unchanged", "layer_metadata_unchanged", "landscape_selection_and_editing_layer_unchanged",
                "editor_pointer_unchanged", "loaded_uobject_identities_and_flags_unchanged", "editor_object_selection_unchanged",
                "transaction_queue_and_active_state_unchanged", "transaction_handle_readable", "dirty_package_names_unchanged"):
        checks["dependencyReadOnly_" + key] = dependencies.get(key) is True
    # The UE5.8-compatible scanner must complete its loaded reflection walk.
    # Historical unsupported results remain evidence, not a future exception.
    checks["noUnexpectedDependencyError"] = (dependencies.get("success") is True
        and dependencies.get("loaded_reflection_walk_complete") is True)
    checks["referenceScanNotTruncated"] = dependencies.get("reference_scan_truncated") is False
    after = dirty()
    checks["inspectionDirtyUnchanged"] = after == before
    report = {"stage": stage, "passed": all(checks.values()), "checks": checks,
              "componentCount": terrain["component_count"], "collisionCount": terrain["collision_component_count"],
              "streamingProxyCount": terrain["streaming_proxy_count"], "dirtyAfter": after,
              "globalDependencyInspectionSuccess": dependencies.get("success"),
              "loadedReflectionWalkComplete": dependencies.get("loaded_reflection_walk_complete"),
              "verificationPolicy": "UE58_loaded_reflection_completion_required_20261116",
              "boundary": "Retained data/loaded actor preservation and complete loaded reflected-property walk only. Native/custom serializers, unloaded packages, transitive safety and runtime traversal are not certified."}
    write(folder, "comparison", report)
    print("LANDSCAPE_VERIFY_20261116 " + stage + " " + ("PASS" if report["passed"] else "FAIL")
          + " " + json.dumps({k:v for k,v in checks.items() if not v}))
    require(report["passed"], "Retained preservation failed; stop before save/promotion")


if __name__ == "__main__":
    main()
