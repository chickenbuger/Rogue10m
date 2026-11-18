"""Explicit, unsaved authoring of the reviewed natural-world primitive plan.

Import is inert. Call build_dressing(dressing_plan, allow_editor_mutation=True)
in the exact idle Crop editor. Uses the public SubobjectDataSubsystem instance
API: UE5.8 itself adds instance components to the serialized actor array and
registers them. Actor.AddComponentByClass is deliberately not used because its
UFUNCTION has ScriptNoExport. No asset creation, save, terrain or old-actor edit.
An exception preserves all partial actors and exposes DressingBuildError.report.
"""
from __future__ import annotations

from collections import OrderedDict
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import traceback


ROOT = Path(__file__).resolve().parents[2]
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
BASE_TAG = "Rogue10m.WorldLayout20261112"
SCOPE_TAG = "Rogue10m.NaturalBoundary20261117"
TAG = "Rogue10m.NaturalWorldSetDressing20261117"
MESHES = {"/Engine/BasicShapes/Cube", "/Engine/BasicShapes/Cylinder"}
MATERIAL_ROOT = "/Game/Rogue10m/World/StartVillage/Materials/"
MATERIALS = {MATERIAL_ROOT + "M_StartVillage_" + value
             for value in ("DarkTimber", "WarmStone", "Banner_Teal", "Banner_Ochre", "Banner_Blue")}
ROLES = {"visual_only", "village_enclosure_pawn_block", "boundary_pawn_only_block"}
GROUP_IDS = {"H_Enclosure", "H_Identity", "M_Enclosure", "M_Identity", "T_Enclosure", "T_Identity",
             "W_AbandonedCamp", "A_OverturnedTransport", "R_RuinFragments", "ClosedBoundaryBackup"}


class DressingBuildError(RuntimeError):
    def __init__(self, report):
        self.report = report
        super().__init__(json.dumps(report, ensure_ascii=False))


def _require(condition, message):
    if not condition:
        raise RuntimeError(message)


def _path(value):
    return value.get_path_name() if value else None


def _xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def _transform(value):
    return {"p": _xyz(value.translation),
            "q": [float(getattr(value.rotation, axis)) for axis in "xyzw"],
            "s": _xyz(value.scale3d)}


def _dirty(u):
    return sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
                   + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})


def _guard(u):
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    _require(Path(u.Paths.project_dir()).resolve() == ROOT.resolve(), "Wrong project")
    _require(str(u.SystemLibrary.get_engine_version()).startswith("5.8."), "UE5.8 is required")
    _require(world and world.get_path_name().split(".")[0] == TARGET
             and not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor(),
             "Exact idle Crop editor world required; no map is switched")
    return world


def _validate_plan(plan):
    _require(isinstance(plan, dict) and plan.get("schema") == "rogue10m.natural-world-set-dressing/1",
             "Reviewed dressing schema required")
    groups = plan.get("groups", [])
    _require(plan.get("tag") == TAG and plan.get("base_z_cm") == 9600.0
             and len(groups) == plan.get("group_count") == 10
             and {g.get("id") for g in groups} == GROUP_IDS, "Exact ten reviewed groups required")
    labels, total, walls = set(), 0, 0
    for group in groups:
        _require(group.get("actor_label") == "NW17_" + group["id"]
                 and group.get("folder", "").startswith("NaturalWorld20261117/")
                 and group.get("tags") == [TAG, TAG + "." + group["id"]], "Unexpected group identity")
        _require(group.get("instances"), "Empty group")
        for row in group["instances"]:
            _require(row.get("mesh") in MESHES and row.get("material") in MATERIALS
                     and row.get("collision_role") in ROLES, "Unapproved mesh/material/collision role")
            _require(row.get("label") not in labels, "Duplicate instance label")
            labels.add(row["label"])
            vectors = [row.get(key) for key in ("location_cm", "dimensions_cm", "rotation_pitch_yaw_roll",
                                                "scale_for_verified_100cm_mesh")]
            _require(all(isinstance(v, list) and len(v) == 3 for v in vectors)
                     and all(isinstance(n, (float, int)) and not isinstance(n, bool) and math.isfinite(n)
                             for vector in vectors for n in vector), "Invalid instance vector")
            _require(min(row["dimensions_cm"]) > 0 and max(row["dimensions_cm"]) <= 100000
                     and all(abs(a / 100.0 - b) < 1e-8 for a, b in
                             zip(row["dimensions_cm"], row["scale_for_verified_100cm_mesh"])),
                     "Dimension/scale contract changed")
            x, y, z = row["location_cm"]
            _require(-50000 <= x <= 80000 and 20000 <= y <= 90000 and 9000 <= z <= 16000,
                     "Instance outside candidate bounds")
            boundary = row["collision_role"] == "boundary_pawn_only_block"
            _require(row.get("hidden_in_game") is boundary
                     and boundary == (group["id"] == "ClosedBoundaryBackup"), "Hidden boundary role mismatch")
            walls += boundary
            total += 1
    _require(total == plan.get("instance_count") == 708 and walls == 13,
             "Exact708 instances/13 closed-boundary segments required")
    return groups


def _new_component(u, sds, parent, component_class, name):
    handle, failure = sds.add_new_subobject(u.AddNewSubobjectParams(
        parent_handle=parent, new_class=component_class.static_class(), blueprint_context=None))
    lib = u.SubobjectDataBlueprintFunctionLibrary
    _require(lib.is_handle_valid(handle) and failure.is_empty(), "Component add failed: " + str(failure))
    _require(sds.rename_subobject(handle=handle, new_name=u.Text(name)), "Component rename failed: " + name)
    component = lib.get_associated_object(lib.get_data(handle))
    _require(component and isinstance(component, component_class), "Wrong newly-created component type")
    return handle, component


def _root_handle(u, sds, actor):
    lib = u.SubobjectDataBlueprintFunctionLibrary
    handles = list(sds.k2_gather_subobject_data_for_instance(actor))
    # EditorActorSubsystem uses UActorFactoryEmptyActor for Actor: that factory
    # already creates, serializes and registers DefaultSceneRoot (ActorFactory
    # .cpp:1318-1326). Adding another scene component attaches a child instead
    # of replacing this root. Reuse its exact public subobject handle.
    existing = actor.get_editor_property("root_component")
    if existing is not None:
        _require(existing.get_class() == u.SceneComponent.static_class(),
                 "Fresh empty Actor has an unexpected root component class")
        roots = [h for h in handles if lib.is_root_component(lib.get_data(h))
                 and lib.get_associated_object(lib.get_data(h)) == existing]
        _require(len(roots) == 1, "One exact existing root subobject handle required")
        _require(sds.rename_subobject(handle=roots[0], new_name=u.Text("NW17_Root")),
                 "Existing factory root rename failed")
        existing.set_mobility(u.ComponentMobility.STATIC)
        _require(actor.get_editor_property("root_component") == existing,
                 "Factory root changed during configuration")
        return roots[0], existing
    matches = [h for h in handles if lib.is_actor(lib.get_data(h))
               and lib.get_associated_object(lib.get_data(h)) == actor]
    _require(len(matches) == 1, "One actor subobject handle required")
    handle, component = _new_component(u, sds, matches[0], u.SceneComponent, "NW17_Root")
    component.set_mobility(u.ComponentMobility.STATIC)
    _require(actor.get_editor_property("root_component") == component, "New scene component did not become root")
    return handle, component


def _binding_preflight(u):
    """Read every required binding before spawning or editing an actor.

    PyGenUtil.cpp::IsScriptExposedFunction excludes BlueprintGetter/Setter,
    BlueprintInternalUseOnly and ScriptNoExport. In particular overlap-event
    access is through its exported property, not SetGenerateOverlapEvents.
    The remaining native calls were checked against their UE5.8 UFUNCTIONs.
    CDO property reads below never set defaults or create persistent assets.
    """
    specs = (
        (u.Actor, ("get_actor_label", "get_actor_transform", "get_class", "get_editor_property",
                   "get_folder_path", "get_package", "set_actor_hidden_in_game", "set_actor_label",
                   "set_actor_transform", "set_editor_property", "set_folder_path")),
        (u.SceneComponent, ("set_mobility", "get_class", "get_editor_property")),
        (u.InstancedStaticMeshComponent, ("add_instance", "get_collision_enabled", "get_collision_profile_name",
            "get_collision_response_to_channel", "get_editor_property", "get_instance_count", "get_instance_transform",
            "get_material", "get_name", "get_path_name", "set_cast_shadow", "set_collision_enabled",
            "set_collision_object_type", "set_collision_profile_name", "set_collision_response_to_all_channels",
            "set_collision_response_to_channel", "set_editor_property", "set_hidden_in_game", "set_material",
            "set_mobility", "set_static_mesh", "set_visibility")),
        (u.EditorActorSubsystem, ("get_all_level_actors", "spawn_actor_from_class")),
        (u.UnrealEditorSubsystem, ("get_editor_world",)),
        (u.LevelEditorSubsystem, ("is_in_play_in_editor",)),
        (u.SubobjectDataSubsystem, ("add_new_subobject", "rename_subobject", "k2_gather_subobject_data_for_instance")),
        (u.SubobjectDataBlueprintFunctionLibrary, ("get_data", "get_associated_object", "is_handle_valid",
                                                  "is_root_component", "is_actor")),
        (u.EditorLoadingAndSavingUtils, ("get_dirty_map_packages", "get_dirty_content_packages")),
        (u.StaticMesh, ("get_bounding_box",)),
        (u.Rotator, ("quaternion",)),
        (u.Text, ("is_empty",)),
    )
    result = {"read_only": True, "method_bindings": {}, "properties": [],
              "generate_overlap_events_access": "set_editor_property; native BlueprintSetter is not a Python method"}
    before = _dirty(u)
    for cls, methods in specs:
        class_name = cls.__name__
        found = {name: callable(getattr(cls, name, None)) for name in methods}
        result["method_bindings"][class_name] = found
        _require(all(found.values()), "Required Python binding unavailable before mutation: "
                 + class_name + ":" + ",".join(name for name, valid in found.items() if not valid))
    cdo = u.get_default_object(u.InstancedStaticMeshComponent.static_class())
    for name in ("generate_overlap_events", "cast_shadow", "visible", "hidden_in_game",
                 "can_ever_affect_navigation", "component_tags", "static_mesh"):
        cdo.get_editor_property(name)
        result["properties"].append(name)
    for cls, names in ((u.CollisionEnabled, ("NO_COLLISION", "QUERY_AND_PHYSICS")),
                       (u.CollisionResponseType, ("ECR_IGNORE", "ECR_BLOCK")),
                       (u.CollisionChannel, ("ECC_WORLD_STATIC", "ECC_PAWN")),
                       (u.ComponentMobility, ("STATIC",))):
        _require(all(hasattr(cls, name) for name in names), "Required public enum value unavailable")
    result["dirty_set_unchanged"] = before == _dirty(u)
    _require(result["dirty_set_unchanged"], "Binding inspection changed dirty packages")
    return result


def _native_transform(u, row):
    value = u.Transform()
    value.translation = u.Vector(*row["location_cm"])
    # Python's reflected struct positional field order is not the plan's
    # pitch/yaw/roll order. Name all axes so fence tangents and gates agree
    # with the independently computed geometry plan.
    pitch, yaw, roll = row["rotation_pitch_yaw_roll"]
    value.rotation = u.Rotator(pitch=pitch, yaw=yaw, roll=roll).quaternion()
    value.scale3d = u.Vector(*row["scale_for_verified_100cm_mesh"])
    return value


def _instance_transform(u, component, index):
    value = component.get_instance_transform(index, world_space=True)
    # UE Python promotes success/out-struct to Transform-or-None. Keep a
    # structural check for bindings that retain the native success tuple.
    if isinstance(value, tuple):
        _require(not any(item is False for item in value), "Instance transform read failed")
        values = [item for item in value if isinstance(item, u.Transform)]
        _require(len(values) == 1, "Ambiguous instance-transform output")
        value = values[0]
    _require(isinstance(value, u.Transform), "Instance transform missing")
    return _transform(value)


def _same_transform(a, b):
    _require(max(abs(x - y) for x, y in zip(a["p"], b["p"])) <= 0.05
             and max(abs(x - y) for x, y in zip(a["s"], b["s"])) <= 0.0001
             and abs(abs(sum(x * y for x, y in zip(a["q"], b["q"]))) - 1.0) <= 0.0001,
             "Instance transform readback differs from plan")


def _configure(u, component, role, hidden):
    component.set_mobility(u.ComponentMobility.STATIC)
    # BlueprintSetter functions are excluded by UE's Python exporter.
    component.set_editor_property("generate_overlap_events", False)
    _require(component.get_editor_property("generate_overlap_events") is False,
             "Overlap-event property readback differs")
    component.set_collision_object_type(u.CollisionChannel.ECC_WORLD_STATIC)
    if role == "visual_only":
        component.set_collision_profile_name("NoCollision", False)
        component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    elif role == "village_enclosure_pawn_block":
        component.set_collision_profile_name("BlockAll", False)
        component.set_collision_enabled(u.CollisionEnabled.QUERY_AND_PHYSICS)
    else:
        component.set_collision_profile_name("Custom", False)
        component.set_collision_response_to_all_channels(u.CollisionResponseType.ECR_IGNORE)
        component.set_collision_response_to_channel(u.CollisionChannel.ECC_PAWN, u.CollisionResponseType.ECR_BLOCK)
        component.set_collision_enabled(u.CollisionEnabled.QUERY_AND_PHYSICS)
    component.set_editor_property("can_ever_affect_navigation", role != "visual_only")
    component.set_visibility(not hidden, False)
    component.set_hidden_in_game(hidden, False)
    component.set_cast_shadow(not hidden)
    _require(bool(component.get_editor_property("visible")) is not hidden
             and bool(component.get_editor_property("hidden_in_game")) is hidden, "Visibility readback differs")
    expected_pawn = u.CollisionResponseType.ECR_IGNORE if role == "visual_only" else u.CollisionResponseType.ECR_BLOCK
    if role != "visual_only":
        _require(component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN) == expected_pawn,
                 "Pawn collision is not blocking")
    if role == "boundary_pawn_only_block":
        channels = ["ECC_WORLD_STATIC", "ECC_WORLD_DYNAMIC", "ECC_VISIBILITY", "ECC_CAMERA",
                    "ECC_PHYSICS_BODY", "ECC_VEHICLE", "ECC_DESTRUCTIBLE"]
        channels += ["ECC_ENGINE_TRACE_CHANNEL" + str(i) for i in range(1, 7)]
        channels += ["ECC_GAME_TRACE_CHANNEL" + str(i) for i in range(1, 51)]
        for name in channels:
            channel = getattr(u.CollisionChannel, name, None)
            if channel is not None:
                _require(component.get_collision_response_to_channel(channel) == u.CollisionResponseType.ECR_IGNORE,
                         "Boundary must ignore non-Pawn channel: " + name)


def build_dressing(plan, *, allow_editor_mutation=False):
    """Create10 group actors/708 instances once; return a JSON-compatible report.

    The caller owns checkpoint, full-region loading, dirty-package scope,
    collision/play/visual QA and later bounded package saving. Existing actors
    and vendor assets are never changed. A partial failure is never retried.
    """
    report = {"schema": "Rogue10m.NaturalWorldDressingAuthor.20261117.v1", "success": False,
              "map": TARGET, "started_utc": datetime.now(timezone.utc).isoformat(),
              "assets_saved": False, "groups": [], "created_actor_paths": [], "packages": [],
              "instance_count": 0, "component_count": 0}
    try:
        _require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required")
        groups = _validate_plan(plan)
        report["plan_sha256"] = hashlib.sha256(json.dumps(plan, sort_keys=True, ensure_ascii=False,
                                                         separators=(",", ":")).encode("utf-8")).hexdigest()
        import unreal as u
        _guard(u)
        report["binding_preflight"] = _binding_preflight(u)
        actor_subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
        _require(not [a for a in actor_subsystem.get_all_level_actors()
                      if a.get_actor_label().startswith("NW17_") or TAG in list(map(str, a.tags))],
                 "Dressing actor already exists; this is not an overwrite/retry helper")
        sds = u.get_engine_subsystem(u.SubobjectDataSubsystem)
        _require(sds is not None, "Public SubobjectDataSubsystem unavailable")
        meshes = {}
        for package in sorted(MESHES):
            mesh = u.load_asset(package)
            _require(isinstance(mesh, u.StaticMesh), "Missing Engine primitive: " + package)
            box = mesh.get_bounding_box()
            _require(max(abs(v - expected) for values, expected in ((_xyz(box.min), -50), (_xyz(box.max), 50))
                         for v in values) < 0.01, "Engine primitive is no longer centered100cm: " + package)
            meshes[package] = mesh
        report["dirty_before"] = _dirty(u)
        # Source materials must never acquire ISM usage flags from our new
        # components. Prepare project-owned shader variants before spawning.
        material_spec = importlib.util.spec_from_file_location("natural_dressing_material_derivatives",
            ROOT / "Scripts/Editor/NaturalDressingMaterialDerivatives20261117.py")
        material_module = importlib.util.module_from_spec(material_spec)
        material_spec.loader.exec_module(material_module)
        materials, report["material_derivatives"] = material_module.create_derivatives(allow_editor_mutation=True)
        _require(set(materials) == MATERIALS, "Exact five owned dressing variants required")
        with u.ScopedEditorTransaction("Natural world 20261117 reviewed primitive dressing"):
            for group in groups:
                actor = actor_subsystem.spawn_actor_from_class(u.Actor, u.Vector(0, 0, 0),
                                                               u.Rotator(pitch=0, yaw=0, roll=0), False)
                _require(actor is not None, "Group actor spawn failed: " + group["id"])
                report["created_actor_paths"].append(_path(actor))
                actor.set_actor_label(group["actor_label"], True)
                actor.set_folder_path(group["folder"])
                actor.set_editor_property("tags", [u.Name(t) for t in [BASE_TAG, SCOPE_TAG] + group["tags"]])
                boundary = group["id"] == "ClosedBoundaryBackup"
                actor.set_editor_property("is_spatially_loaded", not boundary)
                actor.set_actor_hidden_in_game(boundary)
                parent_handle, root = _root_handle(u, sds, actor)
                actor.set_actor_transform(u.Transform(), False, True)
                group_report = {"id": group["id"], "actor_path": _path(actor), "actor_label": actor.get_actor_label(),
                    "actor_class": _path(actor.get_class()), "package": actor.get_package().get_name(),
                    "tags": list(map(str, actor.tags)), "folder": str(actor.get_folder_path()),
                    "actor_transform": _transform(actor.get_actor_transform()), "root_component": _path(root),
                    "spatially_loaded": bool(actor.get_editor_property("is_spatially_loaded")),
                    "hidden_in_game": bool(actor.get_editor_property("hidden")), "components": []}
                report["groups"].append(group_report)
                bins = OrderedDict()
                for row in group["instances"]:
                    key = (row["mesh"], row["material"], row["collision_role"], row["hidden_in_game"])
                    bins.setdefault(key, []).append(row)
                for bin_index, ((mesh_path, material_path, role, hidden), rows) in enumerate(bins.items()):
                    name = "NW17_" + group["id"] + "_ISM_" + str(bin_index).zfill(2)
                    _, component = _new_component(u, sds, parent_handle, u.InstancedStaticMeshComponent, name)
                    component.set_static_mesh(meshes[mesh_path])
                    component.set_material(0, materials[material_path])
                    component.set_editor_property("component_tags", [u.Name(TAG), u.Name(SCOPE_TAG),
                        u.Name("NW17.Role." + role), u.Name("NW17.Bin." + str(bin_index).zfill(2))])
                    _configure(u, component, role, hidden)
                    cr = {"path": _path(component), "name": component.get_name(), "mesh": _path(component.get_editor_property("static_mesh")),
                        "material": _path(component.get_material(0)), "collision_role": role,
                        "hidden_in_game": bool(component.get_editor_property("hidden_in_game")),
                        "visible": bool(component.get_editor_property("visible")),
                        "collision_profile": str(component.get_collision_profile_name()),
                        "collision_enabled": str(component.get_collision_enabled()),
                        "pawn_response": str(component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)),
                        "component_tags": list(map(str, component.get_editor_property("component_tags"))), "instances": []}
                    group_report["components"].append(cr)
                    report["component_count"] += 1
                    for index, row in enumerate(rows):
                        value = _native_transform(u, row)
                        observed_index = component.add_instance(value, world_space=True)
                        _require(observed_index == index, "Unexpected ISM instance index")
                        observed = _instance_transform(u, component, index)
                        expected = _transform(value)
                        _same_transform(expected, observed)
                        cr["instances"].append({"index": index, "label": row["label"],
                            "planned_world_transform": expected, "observed_world_transform": observed})
                        report["instance_count"] += 1
                    _require(component.get_instance_count() == len(rows), "ISM instance count differs")
                _require(group_report["spatially_loaded"] is not boundary, "Boundary must always load")
                report["packages"].append(group_report["package"])
        _require(len(report["created_actor_paths"]) == 10 and report["instance_count"] == 708,
                 "Final actor/instance count differs")
        report["packages"] = sorted(set(report["packages"]))
        report["dirty_after"] = _dirty(u)
        report["success"] = True
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        report["validation_pending"] = ["saved cold-reload instance persistence", "actual Pawn boundary and gate sweeps",
                                         "rendered appearance", "PIE movement", "NavMesh/performance"]
        return report
    except Exception:
        report["traceback"] = traceback.format_exc()
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        raise DressingBuildError(report) from None
