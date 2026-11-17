"""Explicit read-only cave baseline in the already-loaded, idle Crop editor.

Import is inert. This helper never loads a level, creates an Unreal object,
changes a property, compiles, saves, starts PIE or registers a callback. It
loads only existing asset references for inspection and writes a fresh evidence
folder. Existing evidence is never overwritten.
"""
from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import traceback


ROOT = Path(__file__).resolve().parents[2]
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OUT = ROOT / "Feature/doc/evidence/caves20261118/baseline-live"
MESHES = {
    "cube": "/Engine/BasicShapes/Cube",
    "boulder_medium": "/Game/Stylized_Village/Assets/Stones/Boulder/SM_Boulder_Medium_01",
    "boulder_large": "/Game/Stylized_Village/Assets/Stones/Boulder/SM_Boulder_Large_01",
}
MATERIALS = (
    "/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_GroundDetail",
    "/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_Grass",
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def path(value):
    return value.get_path_name() if value else None


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
                                    separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest()


def dirty(u):
    return sorted({p.get_path_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
                   + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})


def disk(asset):
    package = asset.get_path_name().split(".")[0]
    if not package.startswith("/Game/"):
        return {"package": package, "engine_asset": True}
    file = ROOT / "Content" / (package[6:] + ".uasset")
    require(file.is_file(), "Missing existing project asset file: " + package)
    return {"package": package, "file": file.relative_to(ROOT).as_posix(), "size": file.stat().st_size,
            "sha256": hashlib.sha256(file.read_bytes()).hexdigest()}


def optional_property(value, name):
    try:
        result = value.get_editor_property(name)
        if isinstance(result, (bool, int, float, str)) or result is None:
            return result
        return str(result)
    except Exception as error:
        return {"unavailable": str(error)}


def persistent_components(u, actors):
    """Additional component state, including decals absent from PrimitiveComponent."""
    result = {}
    for actor in actors:
        rows = []
        for component in actor.get_components_by_class(u.ActorComponent):
            row = {"path": path(component), "class": path(component.get_class()),
                   "tags": list(map(str, component.get_editor_property("component_tags"))),
                   "editor_only": optional_property(component, "bIsEditorOnly")}
            if isinstance(component, u.SceneComponent):
                rotation = component.get_editor_property("relative_rotation")
                row.update(relative_location_cm=xyz(component.get_editor_property("relative_location")),
                           relative_rotation_pitch_yaw_roll=[float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
                           relative_scale=xyz(component.get_editor_property("relative_scale3d")),
                           mobility=str(component.get_editor_property("mobility")),
                           visible=bool(component.get_editor_property("visible")),
                           hidden_in_game=bool(component.get_editor_property("hidden_in_game")))
            if isinstance(component, u.DecalComponent):
                row.update(decal_material=path(component.get_decal_material()),
                           decal_size_cm=xyz(component.get_editor_property("decal_size")),
                           sort_order=int(component.get_editor_property("sort_order")),
                           fade_screen_size=float(component.get_editor_property("fade_screen_size")))
            if isinstance(component, u.LightComponentBase):
                row["light_properties"] = {key: optional_property(component, key) for key in
                                           ("intensity", "light_color", "cast_shadows", "affects_world")}
            rows.append(row)
        result[path(actor)] = sorted(rows, key=lambda row: row["path"])
    return result


def camera_defaults(u):
    """Observe loaded native defaults; this is not a runtime camera claim."""
    cdo = u.get_default_object(u.Rogue10mCharacter.static_class())
    result = {"native_character_cdo": path(cdo),
              "public_third_person_getter_available": callable(getattr(u.Rogue10mCharacter, "is_third_person_gameplay_active", None)),
              "third_person_default": optional_property(cdo, "bUseThirdPersonGameplay"),
              "spring_arms": [], "cameras": [], "runtime_observation": False}
    for component in cdo.get_components_by_class(u.SpringArmComponent):
        result["spring_arms"].append({"path": path(component), "properties": {
            key: optional_property(component, key) for key in
            ("target_arm_length", "target_offset", "socket_offset", "do_collision_test", "probe_channel",
             "probe_size", "enable_camera_lag", "enable_camera_rotation_lag", "use_pawn_control_rotation")}})
    for component in cdo.get_components_by_class(u.CameraComponent):
        result["cameras"].append({"path": path(component), "properties": {
            key: optional_property(component, key) for key in ("field_of_view", "use_pawn_control_rotation")}})
    return result


def material_state(u, asset, graph_reader):
    require(isinstance(asset, u.MaterialInterface), "Expected existing MaterialInterface: " + str(asset))
    result = {"path": path(asset), "class": path(asset.get_class()), "disk": disk(asset)}
    base = asset
    parents = []
    seen = set()
    while isinstance(base, u.MaterialInstance):
        require(path(base) not in seen, "Material parent cycle")
        seen.add(path(base))
        base = base.get_editor_property("parent")
        require(base is not None, "Missing material instance parent")
        parents.append(path(base))
    require(isinstance(base, u.Material), "No concrete Material parent")
    result.update(parent_chain=parents, base_material=path(base), base_disk=disk(base),
                  used_with_instanced_static_meshes=bool(base.get_editor_property("used_with_instanced_static_meshes")),
                  blend_mode=str(base.get_editor_property("blend_mode")),
                  material_domain=str(base.get_editor_property("material_domain")),
                  two_sided=bool(base.get_editor_property("two_sided")))
    lib = u.MaterialEditingLibrary
    result["scalar_parameter_names"] = list(map(str, lib.get_scalar_parameter_names(asset)))
    result["vector_parameter_names"] = list(map(str, lib.get_vector_parameter_names(asset)))
    result["texture_parameter_names"] = list(map(str, lib.get_texture_parameter_names(asset)))
    if isinstance(asset, u.MaterialInstance):
        result["scalar_values"] = {name: float(lib.get_material_instance_scalar_parameter_value(asset, name))
                                   for name in result["scalar_parameter_names"]}
        result["vector_values"] = {}
        for name in result["vector_parameter_names"]:
            value = lib.get_material_instance_vector_parameter_value(asset, name)
            result["vector_values"][name] = [float(value.r), float(value.g), float(value.b), float(value.a)]
        result["texture_values"] = {name: path(lib.get_material_instance_texture_parameter_value(asset, name))
                                    for name in result["texture_parameter_names"]}
    graph = graph_reader._fingerprint(u, lib, base)
    result["graph"] = graph
    result["graph_sha256"] = digest(graph)
    result["expression_classes"] = dict(Counter(row["class"] for row in graph["nodes"].values()))
    return result


def inspect_cave_sites():
    import unreal as u
    require(not OUT.exists(), "Preserve existing cave baseline; no automatic rerun")
    reader = module("cave_baseline_material_reader", "Scripts/Editor/NaturalRoadMaterial20261117.py")
    dressing = module("cave_baseline_dressing_reader", "Scripts/Editor/NaturalWorldDressing20261117.py")
    actor_reader = module("cave_baseline_actor_reader", "Scripts/Editor/AuthorNaturalBossThreshold20261118.py")
    reader._guard(u)
    require(ROOT == Path("D:/Project/Rogue10m") and not dirty(u), "Exact clean saved project required")
    OUT.mkdir(parents=True, exist_ok=False)

    def write(name, value):
        with (OUT / (name + ".json")).open("x", encoding="utf-8") as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
            stream.write("\n")

    try:
        before_dirty = dirty(u)
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(True))
        require(native.get("success") and native.get("read_only") and native.get("dirty_package_names_unchanged")
                and native.get("landscape_reports_up_to_date") and native.get("layer_count") == 2
                and native.get("component_count") == native.get("collision_count") == 45
                and len(native.get("composite_heights_uint16", [])) == 179488, "Exact settled full-grid native baseline required")
        write("baseline-native", native)
        actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
        require(len(actors) == 1404 and len({path(a) for a in actors}) == 1404, "Exact saved1404 actors required")
        snapshots = actor_reader._snapshot_actors(u, actors, dressing)
        components = persistent_components(u, actors)
        write("baseline-actors", {"schema": "Rogue10m.CaveActorBaseline.20261118.v1", "actor_count": len(actors),
              "snapshot_sha256": digest(snapshots), "component_snapshot_sha256": digest(components),
              "actors": snapshots, "persistent_components": components,
              "packages": {path(a): a.get_package().get_name() for a in actors}})
        write("baseline-camera-contracts", camera_defaults(u))

        material_paths = set(MATERIALS)
        used_refs = {}
        landscape = {}
        for actor in actors:
            refs = set()
            for component in actor.get_components_by_class(u.PrimitiveComponent):
                for i in range(component.get_num_materials()):
                    value = component.get_material(i)
                    if value:
                        refs.add(path(value))
            for component in actor.get_components_by_class(u.DecalComponent):
                value = component.get_decal_material()
                if value:
                    refs.add(path(value))
            if isinstance(actor, u.LandscapeProxy):
                value = actor.get_editor_property("landscape_material")
                hole = actor.get_editor_property("landscape_hole_material")
                landscape[path(actor)] = {"material": path(value), "hole_material": path(hole)}
                refs.update(path(item) for item in (value, hole) if item)
            used_refs[path(actor)] = sorted(refs)
            material_paths.update(refs)

        meshes = {}
        for key, package in MESHES.items():
            value = u.load_asset(package)
            require(isinstance(value, u.StaticMesh), "Required existing mesh missing: " + package)
            bounds = value.get_bounding_box()
            mats = [path(slot.get_editor_property("material_interface"))
                    for slot in value.get_editor_property("static_materials")]
            material_paths.update(p for p in mats if p)
            meshes[key] = {"path": path(value), "disk": disk(value),
                           "bounds_min_cm": xyz(bounds.min), "bounds_max_cm": xyz(bounds.max), "materials": mats}
        materials = {}
        for package in sorted(material_paths):
            value = u.load_asset(package)
            require(isinstance(value, u.MaterialInterface), "Used existing material missing: " + package)
            materials[package] = material_state(u, value, reader)
        write("baseline-materials", {"schema": "Rogue10m.CaveMaterialBaseline.20261118.v1", "meshes": meshes,
              "materials": materials, "actor_material_references": used_refs, "landscape_materials": landscape,
              "runtime_ownership_or_Fab_account_verified": False})

        after = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        final_actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
        require(after.get("success") and after.get("composite_blake3") == native["composite_blake3"]
                and after.get("original_layer_blake3") == native["original_layer_blake3"], "Inspection changed terrain")
        require(dirty(u) == before_dirty and after.get("dirty_package_names_unchanged"), "Read-only inspection dirtied packages")
        require(actor_reader._snapshot_actors(u, final_actors, dressing) == snapshots
                and persistent_components(u, final_actors) == components, "Read-only inspection changed actors/components")
        result = {"schema": "Rogue10m.CaveSiteInspection.20261118.v1", "passed": True,
                  "pid": os.getpid(), "utc": datetime.now(timezone.utc).isoformat(), "map": TARGET,
                  "actor_count": len(actors), "material_count": len(materials), "dirty_before": before_dirty,
                  "dirty_after": dirty(u), "actor_snapshot_sha256": digest(snapshots),
                  "component_snapshot_sha256": digest(components), "assets_saved": False,
                  "editor_asset_mutations": False, "PIE_started": False}
        write("result", result)
        print("CAVE_SITE_INSPECTION READ_ONLY_SUCCESS " + json.dumps(result, ensure_ascii=False))
        return result
    except Exception:
        failure = {"passed": False, "pid": os.getpid(), "utc": datetime.now(timezone.utc).isoformat(),
                   "traceback": traceback.format_exc(), "dirty": dirty(u), "assets_saved": False}
        write("failure", failure)
        u.log_error("CAVE_SITE_INSPECTION " + failure["traceback"])
        raise


if __name__ == "__main__":
    inspect_cave_sites()
