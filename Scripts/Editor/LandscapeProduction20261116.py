"""Guarded native-Landscape inspection/loading/selection in the normal editor.

Run through Unreal's Python console with --phase inspect|load|baseline|scope|preview|select.
No terrain deletion, height/paint edit, package save, or level travel is exposed.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import unreal

ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OUT = ROOT / "Feature/doc/evidence/landscape-production-20261116/live-before"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / (name + ".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def native(name, method, *args):
    result = json.loads(method(*args))
    write(name, result)
    require(result.get("success") is True, name + ": " + str(result.get("error", result.get("errors", "native operation failed"))))
    return result


def valid_preview(result):
    # The native selection facade deliberately returns success=false for a
    # read-only preview to prevent confusing it with an applied selection.
    return (result.get("planned_keep_component_count") == 45
            and result.get("planned_outside_component_count") == 211
            and result.get("selection_performed") is False
            and result.get("confirmation_received") is False
            and result.get("dirty_package_names_unchanged") is True
            and result.get("error", "").startswith("Read-only selection preview."))


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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--phase", required=True, choices=("inspect", "load", "baseline", "scope", "preview", "select", "approved-select"))
    phase = parser.parse_args().phase
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong project")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not editor.get_game_world(), "PIE/game world must be stopped")
    world = editor.get_editor_world()
    require(world and world.get_path_name().split(".")[0] == MAP, "Wrong editor world")
    require(hasattr(unreal, "Rogue10mCrop20261114WorldAuthoring"), "Native Landscape helper missing")
    before = dirty()
    write(phase + "-context", {"project": str(ROOT), "map": MAP, "phase": phase,
          "commandLine": unreal.SystemLibrary.get_command_line(), "dirtyBefore": before,
          "observedUtc": datetime.now(timezone.utc).isoformat(), "pie": False})
    require(not before["maps"] and not before["content"], "Preserve existing dirty packages before continuing")
    helper = unreal.Rogue10mCrop20261114WorldAuthoring
    if phase == "inspect":
        native("initial-terrain", helper.inspect_candidate_terrain)
    elif phase == "load":
        native("load-region", helper.load_candidate_region)
    elif phase == "baseline":
        checkpoint = json.loads((OUT.parent / "baseline-files.json").read_text(encoding="utf-8-sig"))
        require(checkpoint["status"] == "CANDIDATE_PHYSICAL_BACKUP_SHA256_VERIFIED_PROTECTED_INPUTS_UNCHANGED", "Verified backup required")
        terrain = native("loaded-terrain", helper.inspect_candidate_terrain)
        require(terrain.get("original_layout_contract_passed"), "Uncropped original lattice must be fully loaded")
        write("actors", actor_inventory())
        native("retained-height-paint-layers", helper.snapshot_retained_terrain_data)
        native("retained-dependencies", helper.inspect_candidate_dependencies)
        preview = json.loads(helper.select_outside_components(False))
        write("outside-preview", preview)
        require(valid_preview(preview), "Unexpected crop preview")
    elif phase == "scope":
        terrain = json.loads((OUT / "loaded-terrain.json").read_text(encoding="utf-8-sig"))
        require(terrain.get("original_layout_contract_passed"), "Original lattice baseline required")
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        require(not registry.is_loading_assets(), "Wait for AssetRegistry discovery completion")
        options = unreal.AssetRegistryDependencyOptions()
        for property_name in ("include_soft_package_references", "include_hard_package_references",
                              "include_game_package_references", "include_editor_only_package_references",
                              "include_searchable_names", "include_soft_management_references",
                              "include_hard_management_references"):
            options.set_editor_property(property_name, True)
        package_names = sorted({MAP} | {p["package"] for p in terrain["proxies"]})
        require(len(package_names) == 18, "Candidate map plus17 terrain packages required")
        prefixes = ("/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/",
                    "/Game/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/")
        referencers = []
        external = []
        for package in package_names:
            refs = sorted(str(p) for p in registry.get_referencers(package, options))
            referencers.append({"targetPackage": package, "referencers": refs})
            external.extend({"targetPackage": package, "referencer": p} for p in refs
                            if p != MAP and not p.startswith(prefixes))
        owners = []
        all_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        component_count = 0
        for actor in all_actors:
            actor_type = actor.get_class().get_path_name()
            if re.search("PCG|Foliage|Spline|Water|LandscapeBlueprintBrush", actor_type, re.I):
                owners.append({"path": actor.get_path_name(), "class": actor_type, "kind": "actor"})
            for component in actor.get_components_by_class(unreal.ActorComponent):
                component_count += 1
                component_type = component.get_class().get_path_name()
                if re.search("PCG|Foliage|Spline|Water|LandscapeBlueprintBrush", component_type, re.I):
                    owners.append({"path": component.get_path_name(), "class": component_type, "kind": "component"})
        write("candidate-scoped-references", {"registryScanComplete": True, "allSevenReferenceOptions": True,
              "targetPackageCount": len(package_names), "packageReferencers": referencers,
              "externalPackageReferencers": external, "loadedActorCount": len(all_actors),
              "loadedActorComponentCount": component_count, "terrainAuthoringActorsAndComponents": owners,
              "boundary": "Loaded candidate actor/components and18 on-disk package referencers only. Global opaque/native/custom serialization remains unverified; original dependency failure is preserved."})
        require(not external and not owners, "New external package or terrain authoring dependency requires review")
    elif phase == "preview":
        # Safe read-only selection planning remains useful even if the separate
        # process-wide reference walker reported unsupported engine properties.
        # This does not convert that failed inspection to a success.
        terrain = json.loads((OUT / "loaded-terrain.json").read_text(encoding="utf-8-sig"))
        require(terrain.get("original_layout_contract_passed"), "Original lattice baseline required")
        preview = json.loads(helper.select_outside_components(False))
        write("outside-preview", preview)
        require(valid_preview(preview), "Unexpected preview; no selection/edit has been applied")
    elif phase in ("select", "approved-select"):
        preview = json.loads((OUT / "outside-preview.json").read_text(encoding="utf-8-sig"))
        require(valid_preview(preview), "Exact read-only baseline preview required")
        scoped = json.loads((OUT / "candidate-scoped-references.json").read_text(encoding="utf-8-sig"))
        scoped_complete = json.loads((OUT / "scope-complete.json").read_text(encoding="utf-8-sig"))
        require(scoped.get("registryScanComplete") and scoped.get("targetPackageCount") == 18
                and not scoped.get("externalPackageReferencers")
                and not scoped.get("terrainAuthoringActorsAndComponents")
                and scoped_complete.get("dirtyUnchanged"), "Scoped review prerequisites required")
        selection = native("outside-selected" if phase == "select" else "approved-outside-selected", helper.select_outside_components, True)
        require(selection.get("selection_performed") and selection.get("selection_matches_outside_set"), "Exact outside selection was not applied")
    after = dirty()
    write(phase + "-complete", {"phase": phase, "dirtyAfter": after, "dirtyUnchanged": before == after,
                              "terrainMutation": False, "packagesSaved": False})
    require(before == after, "Dirty package names changed; preserve and inspect")
    print("LANDSCAPE_20261116 " + phase + " SUCCESS; no terrain edit or save")


if __name__ == "__main__":
    main()
