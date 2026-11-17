"""Create one isolated route graybox after an explicit editor-owner handoff.

Importing this file never changes the editor. Call author(editor_handoff=...,
compilation_clear=True) only after the shared editor has been released.
No production map/portal/character/save asset is modified or saved.
"""
from __future__ import annotations

import datetime
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys
import traceback

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
TARGET = "/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110"
OWN = "/Game/Rogue10m/World/Prototypes/FiveVillageGraybox20261110"
TAG = "Rogue10m.FiveVillageGraybox20261110"
EVIDENCE = ROOT / "Feature/doc/evidence/five-village-graybox-20261110"
PLAN = ROOT / "Feature/doc/evidence/five-village-route-spec-20261110/route-plan.json"
PROTECTED = [ROOT / "Content/Rogue10m/Maps" / (name + ".umap")
             for name in ("StartHub", "OpenRunMap", "L_Menu")]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_layout():
    path = ROOT / "Scripts/FiveVillageGrayboxLayout20261110.py"
    spec = importlib.util.spec_from_file_location("five_village_graybox_layout", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def author(*, editor_handoff: str, compilation_clear: bool = False,
           restore_original: bool = True, resume_report: str | None = None):
    """Make/save/reload the fresh candidate; refuse existing targets or dirty WIP.

    editor_handoff records the actual coordinator handoff message, not elapsed
    time or a process-idle inference. This function does not acquire ownership.
    """
    if not editor_handoff.strip() or not compilation_clear:
        raise RuntimeError("Explicit editor handoff and completed build/QA are required.")
    import unreal

    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    report_path = EVIDENCE / ("editor-creation-" + stamp + ".json")
    report = {
        "status": "preflight", "editor_handoff": editor_handoff,
        "compilation_clear_confirmed_by_coordinator": compilation_clear,
        "target": TARGET, "created_assets": [], "created_actor_labels": [],
        "runtime_playtest": "not_run", "physical_input": "not_run",
        "direct_60_seconds": "not_measured", "content_15_minutes": "not_measured",
        "protected_map_sha256_before": {}, "protected_map_sha256_after": {},
    }
    EVIDENCE.mkdir(parents=True, exist_ok=True)

    def write_report():
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                               encoding="utf-8")

    def dirty_packages():
        utils = unreal.EditorLoadingAndSavingUtils
        return [str(p.get_path_name()) for p in
                list(utils.get_dirty_map_packages()) + list(utils.get_dirty_content_packages())]

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_system = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    def assert_target():
        world = editor_system.get_editor_world()
        if not world or world.get_path_name().split(".")[0] != TARGET:
            raise RuntimeError("Editor world changed; refusing writes outside candidate.")
        if levels.is_in_play_in_editor():
            raise RuntimeError("PIE is active; stopping candidate authoring.")
        return world

    def assert_no_unowned_dirty():
        dirty = dirty_packages()
        unexpected = [p for p in dirty if not (p == TARGET or p.startswith(OWN + "/"))]
        if unexpected:
            raise RuntimeError("Unrelated packages became dirty; not saving them: " + repr(unexpected))

    def vector(values):
        return unreal.Vector(*[float(v) for v in values])

    def owned(actor, record):
        assert_target()
        if not actor:
            raise RuntimeError("Could not spawn " + record["label"])
        actor.set_actor_label(record["label"])
        actor.set_folder_path("FiveVillageGraybox/" + record["kind"])
        tags = [TAG]
        if record.get("node_id"):
            tags.append(TAG + ".Node." + record["node_id"])
        if record.get("edge"):
            tags.append(TAG + ".Edge." + ".".join(record["edge"]))
        actor.set_editor_property("tags", [unreal.Name(t) for t in tags])
        report["created_actor_labels"].append(record["label"])
        return actor

    try:
        prior = None
        if resume_report:
            prior_path = Path(resume_report).resolve()
            if prior_path.parent != EVIDENCE.resolve():
                raise RuntimeError('Resume report must be in the candidate evidence directory.')
            prior = json.loads(prior_path.read_text(encoding='utf-8-sig'))
            expected_partial = [TARGET, OWN + '/Materials/MI_Graybox_floor']
            if (prior.get('status') != 'blocked_or_failed' or prior.get('target') != TARGET
                    or prior.get('created_assets') != expected_partial or prior.get('created_actor_labels')
                    or prior.get('route_plan_sha256') != sha256(PLAN)):
                raise RuntimeError('Only the recorded empty candidate/first-material failure can resume.')
            report['resume_source_report'] = str(prior_path)
        project_file = Path(unreal.Paths.convert_relative_path_to_full(
            unreal.Paths.get_project_file_path())).resolve()
        if project_file != (ROOT / "Rogue10m.uproject").resolve():
            raise RuntimeError("Unexpected project: " + str(project_file))
        if (ROOT / "Content").resolve() != ROOT / "Content":
            raise RuntimeError("Content resolves outside the physical project; inspect isolation.")
        if levels.is_in_play_in_editor():
            raise RuntimeError("PIE is active; preserve current QA session.")
        pending = dirty_packages()
        allowed_partial = {TARGET, OWN + '/Materials/MI_Graybox_floor'} if prior else set()
        if any(p not in allowed_partial for p in pending):
            raise RuntimeError("Unsaved work exists; candidate creation blocked: " + repr(pending))
        original_world = editor_system.get_editor_world()
        original = original_world.get_path_name().split(".")[0] if original_world else ""
        if prior:
            if original != TARGET:
                raise RuntimeError('Recorded partial candidate must still be the current world.')
            if any(a.get_class().get_name() not in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume', 'LevelBounds')
                   for a in actor_system.get_all_level_actors()):
                raise RuntimeError('Partial world contains unexpected actors; inspect instead of resuming.')
            original = prior['original_map']
        if not original.startswith("/Game/"):
            raise RuntimeError("A saved original /Game map is required for safe restoration.")
        report["original_map"] = original
        target_disk = ROOT / "Content/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110.umap"
        owned_disk = ROOT / "Content/Rogue10m/World/Prototypes/FiveVillageGraybox20261110"
        if not prior and (unreal.EditorAssetLibrary.does_asset_exist(TARGET) or target_disk.exists()):
            raise RuntimeError("Candidate map exists; inspect it instead of overwriting or retrying.")
        existing_assets = list(unreal.EditorAssetLibrary.list_assets(OWN, True, False))
        unexpected_assets = [p.split('.')[0] for p in existing_assets
                             if not prior or p.split('.')[0] != OWN + '/Materials/MI_Graybox_floor']
        if unexpected_assets or (not prior and owned_disk.exists() and any(owned_disk.rglob("*.uasset"))):
            raise RuntimeError("Owned destination has assets; refusing overwrite: " + repr(existing_assets))

        layout = load_layout()
        plan = json.loads(PLAN.read_text(encoding="utf-8-sig"))
        scene = layout.generate_scene(plan)
        geometry_report = layout.validate_scene(scene, plan)
        report["geometry_check_count"] = geometry_report["check_count"]
        report["geometry_failure_count"] = geometry_report["failure_count"]
        if geometry_report["failure_count"]:
            raise RuntimeError("Offline scene geometry is invalid; refusing editor authoring.")
        if scene["map_package"] != TARGET or scene["material_root"] != OWN + "/Materials":
            raise RuntimeError("Scene destinations do not match isolated candidate scope.")
        labels = [a["label"] for a in scene["actors"]]
        if len(labels) != len(set(labels)):
            raise RuntimeError("Duplicate scene actor labels.")
        report["route_plan_sha256"] = sha256(PLAN)
        report["scene_actor_count"] = len(scene["actors"])
        report["protected_map_sha256_before"] = {str(p.relative_to(ROOT)): sha256(p) for p in PROTECTED}
        if prior and report['protected_map_sha256_before'] != prior['protected_map_sha256_before']:
            raise RuntimeError('Protected maps differ from the partial-attempt baseline.')
        meshes = {key: unreal.load_asset("/Engine/BasicShapes/" + name)
                  for key, name in (("cube", "Cube"), ("cylinder", "Cylinder"))}
        if not all(meshes.values()):
            raise RuntimeError("Missing engine basic shapes.")

        # NewLevel closes the original without saving: dirty=0 was required above.
        if not prior and not levels.new_level(TARGET, False):
            raise RuntimeError("Could not create fresh candidate level.")
        report["created_assets"].append(TARGET)
        world = assert_target()
        game_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
        if not game_mode:
            raise RuntimeError("Engine GameModeBase is unavailable.")
        world.get_world_settings().set_editor_property("default_game_mode", game_mode)
        report["game_mode"] = "/Script/Engine.GameModeBase"
        report["gameplay_integration"] = "not_connected; default pawn is not the production walking fixture"
        assets = unreal.AssetToolsHelpers.get_asset_tools()
        materials = {}
        parent = unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')
        if not parent or 'Color' not in map(str, unreal.MaterialEditingLibrary.get_vector_parameter_names(parent)):
            raise RuntimeError('Expected reusable engine Color material parent is unavailable.')
        report['material_parent'] = parent.get_path_name()
        for key, record in scene["materials"].items():
            assert_target()
            if not key.replace("_", "").isalnum():
                raise RuntimeError("Unsafe material key: " + key)
            name = "MI_Graybox_" + key
            path = OWN + "/Materials/" + name
            reuse_partial = bool(prior and key == 'floor')
            if not reuse_partial and unreal.EditorAssetLibrary.does_asset_exist(path):
                raise RuntimeError("Unexpected material already exists: " + path)
            material = unreal.load_asset(path) if reuse_partial else assets.create_asset(
                name, OWN + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            if not material:
                raise RuntimeError("Could not create material: " + path)
            report["created_assets"].append(path)
            unreal.MaterialEditingLibrary.set_material_instance_parent(material, parent)
            # UE5.8 setter writes/updates successfully but always returns false
            # (MaterialEditingLibrary.cpp:1572-1581). Verify actual readback instead.
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
                material, 'Color', unreal.LinearColor(*record['color']))
            unreal.MaterialEditingLibrary.update_material_instance(material)
            observed_color = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material, 'Color')
            if max(abs(a - b) for a, b in zip(
                    (observed_color.r, observed_color.g, observed_color.b, observed_color.a), record['color'])) > 0.0001:
                raise RuntimeError('Candidate material Color readback mismatch: ' + path)
            materials[key] = material

        for record in scene["actors"]:
            assert_target()
            location = vector(record["center_cm"])
            rotation = unreal.Rotator(pitch=float(record.get("pitch_degrees", 0)),
                                     yaw=float(record.get("yaw_degrees", 0)), roll=0)
            kind = record["kind"]
            classes = {"mesh": unreal.StaticMeshActor, "text": unreal.TextRenderActor,
                       "player_start": unreal.PlayerStart,
                       "directional_light": unreal.DirectionalLight, "sky_light": unreal.SkyLight}
            actor = owned(actor_system.spawn_actor_from_class(classes[kind], location, rotation), record)
            if kind == "mesh":
                mesh = meshes[record["mesh"]]
                bounds = mesh.get_bounds()
                extents = [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]
                scales = [float(record["size_cm"][i]) / (2 * extents[i]) for i in range(3)]
                actor.set_actor_scale3d(vector(scales))
                # Engine basic shapes have centered origins. Do not silently shift future meshes.
                if max(abs(bounds.origin.x), abs(bounds.origin.y), abs(bounds.origin.z)) > 0.01:
                    raise RuntimeError("Unexpected basic-shape mesh origin.")
                component = actor.static_mesh_component
                component.set_static_mesh(mesh)
                component.set_mobility(unreal.ComponentMobility.STATIC)
                component.set_collision_profile_name("BlockAll" if record["collision"] else "NoCollision")
                component.set_editor_property("generate_overlap_events", False)
                component.set_material(0, materials[record["material"]])
            elif kind == "text":
                component = actor.get_component_by_class(unreal.TextRenderComponent)
                component.set_text(record["text"])
                component.set_world_size(float(record.get("text_world_size_cm", 250)))
                component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
                component.set_text_render_color(unreal.Color(*record.get("text_color", [240, 240, 240, 255])))
            elif kind == "directional_light":
                component = actor.get_component_by_class(unreal.DirectionalLightComponent)
                component.set_editor_property("intensity", 6000.0)
                component.set_mobility(unreal.ComponentMobility.MOVABLE)
                actor.set_actor_rotation(unreal.Rotator(pitch=-55, yaw=-30, roll=0), False)
            elif kind == "sky_light":
                component = actor.get_component_by_class(unreal.SkyLightComponent)
                component.set_mobility(unreal.ComponentMobility.MOVABLE)
                component.set_editor_property("intensity", 0.8)

        assert_target()
        assert_no_unowned_dirty()
        for path in report["created_assets"]:
            if path == TARGET:
                continue
            if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
                raise RuntimeError("Scoped asset save failed: " + path)
        if not levels.save_current_level():
            raise RuntimeError("Candidate map save failed.")
        if dirty_packages():
            raise RuntimeError("Dirty packages remain after scoped save; not discarding them.")
        report["status"] = "saved_pending_reload"
        write_report()
        if not levels.load_level(original):
            raise RuntimeError("Could not load original map for independent reload.")
        if dirty_packages():
            raise RuntimeError("Original map became dirty on load; preserving it.")
        if not levels.load_level(TARGET):
            raise RuntimeError("Could not reload saved candidate.")
        assert_target()
        owned_actors = {a.get_actor_label(): a for a in actor_system.get_all_level_actors()
                        if TAG in [str(t) for t in a.get_editor_property("tags")]}
        failures = []
        if set(owned_actors) != set(labels):
            failures.append("Owned actor labels/count changed on reload.")
        for record in scene["actors"]:
            actor = owned_actors.get(record["label"])
            if not actor:
                continue
            actual = actor.get_actor_location()
            observed = [actual.x, actual.y, actual.z]
            if math.dist(observed, record["center_cm"]) > 0.05:
                failures.append(record["label"] + ": location mismatch")
            if record["kind"] == "mesh":
                component = actor.static_mesh_component
                observed_yaw = float(actor.get_actor_rotation().yaw)
                yaw_delta = (observed_yaw - float(record.get("yaw_degrees", 0)) + 180) % 360 - 180
                if abs(yaw_delta) > 0.01:
                    failures.append(record["label"] + ": road/building yaw mismatch")
                expected_profile = "BlockAll" if record["collision"] else "NoCollision"
                if str(component.get_collision_profile_name()) != expected_profile:
                    failures.append(record["label"] + ": collision mismatch")
                if component.get_material(0) != materials[record["material"]]:
                    # Reloaded references may use new Python wrappers; compare stable package paths.
                    actual_material = component.get_material(0)
                    if not actual_material or actual_material.get_path_name().split(".")[0] != OWN + "/Materials/MI_Graybox_" + record["material"]:
                        failures.append(record["label"] + ": material mismatch")
                actual_mesh = component.get_editor_property("static_mesh")
                if not actual_mesh or actual_mesh.get_path_name() != meshes[record["mesh"]].get_path_name():
                    failures.append(record["label"] + ": mesh mismatch")
                expected_scale = [record["size_cm"][i] / (2 * [meshes[record["mesh"]].get_bounds().box_extent.x,
                                  meshes[record["mesh"]].get_bounds().box_extent.y,
                                  meshes[record["mesh"]].get_bounds().box_extent.z][i]) for i in range(3)]
                actual_scale = actor.get_actor_scale3d()
                if math.dist([actual_scale.x, actual_scale.y, actual_scale.z], expected_scale) > 0.001:
                    failures.append(record["label"] + ": scale mismatch")
        report["reload_owned_actor_count"] = len(owned_actors)
        report["reload_failures"] = failures
        report["protected_map_sha256_after"] = {str(p.relative_to(ROOT)): sha256(p) for p in PROTECTED}
        if report["protected_map_sha256_before"] != report["protected_map_sha256_after"]:
            failures.append("Protected map hash changed.")
        if failures:
            raise RuntimeError("Candidate reload validation failed: " + repr(failures))
        if dirty_packages():
            raise RuntimeError("Reloaded world has dirty packages; preserving state for inspection.")
        if restore_original:
            if not levels.load_level(original):
                raise RuntimeError("Candidate verified but original-map restoration failed.")
            report["restored_original_map"] = original
        report["status"] = "saved_and_reloaded"
        report["map_disk_exists"] = target_disk.exists()
        report["map_sha256"] = sha256(target_disk)
        report["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        write_report()
        unreal.log("[FiveVillageGraybox] " + json.dumps({"status": report["status"], "report": str(report_path)}))
        return report
    except Exception as error:
        report["status"] = "blocked_or_failed"
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        report["dirty_packages_at_stop"] = dirty_packages()
        write_report()
        unreal.log_error("[FiveVillageGraybox] stopped; inspect " + str(report_path))
        raise


if __name__ == "__main__":
    print("Prepared authoring script. Import and call author only after explicit editor handoff.")
