"""One explicit, unsaved construction of the reviewed two cave structures.

Import is inert. Materials must already be project-owned, ISM-ready derivatives.
The owner supplies the exact expected dirty-package set, including approved
terrain/material edits. This helper never edits terrain, existing actors,
materials, cameras, saves, retries, rolls back or deletes partial work.
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
BASE = ROOT / "Feature/doc/evidence/caves20261118"
PLAN = BASE / "plan.json"
PLAN_SHA256 = "39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/"
TAG = "Rogue10m.Caves20261118.Structure"
ROLES = {"stone_floor", "stone_wall", "grass_roof"}
LABELS = {site: "CV18_" + site + "_Structure" for site in ("Q", "Z")}


class CaveStructureError(RuntimeError):
    def __init__(self, report):
        self.report = report
        super().__init__(json.dumps(report, ensure_ascii=False))


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


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
                                    separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest()


def owned_package(name):
    return (name == TARGET or name.startswith(OWN) or
            any(name.startswith(prefix + TARGET[6:] + "/")
                for prefix in ("/Game/__ExternalActors__/", "/Game/__ExternalObjects__/")))


def instance_row(row):
    return {"location_cm": row["location_cm"],
            "rotation_pitch_yaw_roll": row["rotation_pitch_yaw_roll_deg"],
            "scale_for_verified_100cm_mesh": [value / 100.0 for value in row["dimensions_cm"]]}


def validate_plan(plan):
    require(plan.get("schema") == "Rogue10m.CaveLayoutPlan.20261118.v1" and plan.get("map") == TARGET
            and plan.get("units") == "world_centimetres" and plan.get("native_cube_size_cm") == 100.0,
            "Exact reviewed cave plan schema/map/units/cube required")
    sites = plan.get("sites", [])
    require(len(sites) == 2 and {site.get("site_id") for site in sites} == {"Q", "Z"}, "Exact Q/Z sites required")
    labels = set()
    for site in sites:
        rows = site.get("components", [])
        expected = 141 if site["site_id"] == "Q" else 145
        require(len(rows) == expected and site.get("geometry_counts", {}).get("components_total") == expected,
                "Exact reviewed structure count required")
        for row in rows:
            require(row.get("mesh") == "cube" and row.get("mesh_path") == "/Engine/BasicShapes/Cube"
                    and row.get("cube_base_size_cm") == 100.0 and row.get("collision") == "BlockAll"
                    and row.get("tick_enabled") is False and row.get("material_role") in ROLES,
                    "Unreviewed mesh/collision/tick/material role")
            require(row.get("label", "").startswith(site["site_id"] + "_") and row["label"] not in labels,
                    "Structure labels must be unique and site-scoped")
            labels.add(row["label"])
            for key in ("location_cm", "dimensions_cm", "rotation_pitch_yaw_roll_deg"):
                values = row.get(key)
                require(isinstance(values, list) and len(values) == 3 and all(
                    isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)
                    for value in values), "Finite three-component vector required: " + key)
            require(min(row["dimensions_cm"]) > 0 and max(row["dimensions_cm"]) <= 100000,
                    "Invalid structure dimensions")
            x, y, z = row["location_cm"]
            require(-33750 <= x <= 67500 and 22500 <= y <= 78750 and 5000 <= z <= 13000,
                    "Structure center outside reviewed candidate envelope")
        require(sum(row["label"] == site["site_id"] + "_Ramp" for row in rows) == 1, "One ramp per site required")
    return sites


def ramp_preflight(u, sites, dressing):
    """Verify native UE Transform conventions before creating any actor."""
    result = []
    for site in sites:
        row = next(row for row in site["components"] if row["label"] == site["site_id"] + "_Ramp")
        transform = dressing._native_transform(u, instance_row(row))
        fn = getattr(transform, "transform_location", None)
        if callable(fn):
            observed = [fn(u.Vector(x, 0, 50)) for x in (-50, 50)]
        else:
            fn = getattr(u.MathLibrary, "transform_location", None)
            require(callable(fn), "Public native TransformLocation unavailable")
            observed = [fn(transform, u.Vector(x, 0, 50)) for x in (-50, 50)]
        observed = sorted(([float(v.x), float(v.y), float(v.z)] for v in observed), key=lambda point: point[1])
        ramp = site["ramp"]
        expected = sorted(([ramp["x_cm"], ramp["top_y_cm"], 9600.0],
                           [ramp["x_cm"], ramp["bottom_y_cm"], 8600.0]), key=lambda point: point[1])
        error = max(abs(a - b) for actual, wanted in zip(observed, expected) for a, b in zip(actual, wanted))
        require(error <= 0.01, "Native ramp top-face endpoints differ: " + site["site_id"])
        result.append({"site": site["site_id"], "observed_top_face_endpoints_cm": observed,
                       "expected_top_face_endpoints_cm": expected, "max_error_cm": error, "passed": True})
    return result


def configure_collision(u, component):
    component.set_mobility(u.ComponentMobility.STATIC)
    component.set_editor_property("generate_overlap_events", False)
    component.set_collision_object_type(u.CollisionChannel.ECC_WORLD_STATIC)
    component.set_collision_profile_name("BlockAll", False)
    component.set_collision_enabled(u.CollisionEnabled.QUERY_AND_PHYSICS)
    component.set_editor_property("can_ever_affect_navigation", True)
    component.set_visibility(True, False)
    component.set_hidden_in_game(False, False)
    component.set_cast_shadow(True)
    component.set_component_tick_enabled(False)
    require(not component.is_component_tick_enabled(), "Structure component tick must stay disabled")
    require(component.get_collision_enabled() == u.CollisionEnabled.QUERY_AND_PHYSICS
            and str(component.get_collision_profile_name()) == "BlockAll"
            and all(component.get_collision_response_to_channel(channel) == u.CollisionResponseType.ECR_BLOCK
                    for channel in (u.CollisionChannel.ECC_PAWN, u.CollisionChannel.ECC_CAMERA,
                                    u.CollisionChannel.ECC_VISIBILITY)), "Pawn/Camera/Visibility must all block")


def author_cave_spaces(*, allow_editor_mutation=False, expected_plan_sha256,
                       expected_composite_blake3, expected_original_layer_blake3,
                       expected_dirty_packages, material_paths_by_role):
    report = {"schema": "Rogue10m.CaveStructureAuthor.20261118.v1", "success": False,
              "started_utc": datetime.now(timezone.utc).isoformat(), "mutation_started": False,
              "assets_saved": False, "save_called": False, "landscape_mutated": False,
              "existing_actor_changes": [], "materials_mutated": False, "automatic_retry": False,
              "automatic_rollback": False, "partial_work_deleted": False,
              "created_actor_paths": [], "packages": [], "sites": [], "instance_count": 0}
    try:
        require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required")
        require(expected_plan_sha256 == PLAN_SHA256 and hashlib.sha256(PLAN.read_bytes()).hexdigest() == PLAN_SHA256,
                "Reviewed immutable cave plan changed")
        plan = json.loads(PLAN.read_text(encoding="utf-8"))
        sites = validate_plan(plan)
        require(isinstance(expected_dirty_packages, (list, tuple)) and
                len(expected_dirty_packages) == len(set(expected_dirty_packages)) and
                all(isinstance(name, str) and owned_package(name) for name in expected_dirty_packages),
                "Exact unique candidate/OWN dirty-package allowlist required")
        require(isinstance(material_paths_by_role, dict) and set(material_paths_by_role) == ROLES
                and all(isinstance(value, str) and value.startswith(OWN) for value in material_paths_by_role.values()),
                "Exact three existing project-owned role materials required")
        import unreal as u
        dressing = module("cave_structure_dressing_api", "Scripts/Editor/NaturalWorldDressing20261117.py")
        inspector = module("cave_structure_baseline_reader", "Scripts/Editor/InspectCaveSites20261118.py")
        old_reader = module("cave_structure_old_actor_reader", "Scripts/Editor/AuthorNaturalBossThreshold20261118.py")
        dressing._guard(u)
        require(set(dressing._dirty(u)) == set(expected_dirty_packages), "Dirty-package set differs before cave authoring")
        report["dirty_before"] = dressing._dirty(u)
        report["binding_preflight"] = dressing._binding_preflight(u)
        require(callable(getattr(u.Actor, "set_actor_tick_enabled", None))
                and callable(getattr(u.Actor, "is_actor_tick_enabled", None))
                and callable(getattr(u.ActorComponent, "set_component_tick_enabled", None))
                and callable(getattr(u.ActorComponent, "is_component_tick_enabled", None)), "Public tick bindings unavailable")
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native.get("success") and native.get("landscape_reports_up_to_date") and native.get("layer_count") == 2
                and native.get("component_count") == native.get("collision_count") == 45
                and native.get("composite_blake3") == expected_composite_blake3
                and native.get("original_layer_blake3") == expected_original_layer_blake3,
                "Exact settled current terrain/original hashes required")
        baseline = json.loads((BASE / "baseline-live/baseline-actors.json").read_text(encoding="utf-8"))
        require(baseline.get("actor_count") == 1404 and len(baseline["actors"]) == 1404
                and digest(baseline["actors"]) == baseline["snapshot_sha256"]
                and digest(baseline["persistent_components"]) == baseline["component_snapshot_sha256"],
                "Exact immutable source1404 actor/component baseline required")
        actors_subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
        old_actors = list(actors_subsystem.get_all_level_actors())
        require(len(old_actors) == 1404 and {path(actor) for actor in old_actors} == set(baseline["actors"])
                and not any(actor.get_actor_label() in set(LABELS.values()) or TAG in list(map(str, actor.tags))
                            for actor in old_actors), "Exact original1404 scene required; no overwrite/retry")
        before = old_reader._snapshot_actors(u, old_actors, dressing)
        before_components = inspector.persistent_components(u, old_actors)
        require(before == baseline["actors"] and before_components == baseline["persistent_components"],
                "An original actor or persistent component differs from the captured baseline")
        report["existing_actor_snapshot_sha256_before"] = digest(before)
        report["existing_components_sha256_before"] = digest(before_components)
        mesh = u.load_asset("/Engine/BasicShapes/Cube")
        require(isinstance(mesh, u.StaticMesh), "Existing native Cube required")
        box = mesh.get_bounding_box()
        require(max(abs(value - wanted) for actual, wanted in ((dressing._xyz(box.min), -50),
                    (dressing._xyz(box.max), 50)) for value in actual) <= 0.01, "Centered100cm native Cube required")
        report["native_ramp_preflight"] = ramp_preflight(u, sites, dressing)
        materials, material_flags = {}, {}
        for role, package in material_paths_by_role.items():
            value = u.load_asset(package)
            require(isinstance(value, u.MaterialInterface), "Missing owned role material: " + role)
            base = value
            visited = set()
            while isinstance(base, u.MaterialInstance):
                require(path(base) not in visited, "Role material parent cycle")
                visited.add(path(base))
                base = base.get_editor_property("parent")
                require(base is not None, "Role material parent missing")
            require(isinstance(base, u.Material) and base.get_editor_property("used_with_instanced_static_meshes") is True,
                    "Role material must already be ISM-ready; no source auto-flag mutation allowed: " + role)
            require(base.get_editor_property("blend_mode") == u.BlendMode.BLEND_OPAQUE,
                    "Opaque cave collision/ceiling/roof surfaces required")
            materials[role] = value
            material_flags[role] = {"path": path(value), "base": path(base), "instancing": True,
                                    "blend_mode": str(base.get_editor_property("blend_mode"))}
        require(set(dressing._dirty(u)) == set(expected_dirty_packages), "Read-only preflight dirtied a source package")
        report.update(plan_sha256=PLAN_SHA256, materials=material_flags,
                      native_composite_before=expected_composite_blake3,
                      original_layer_blake3=expected_original_layer_blake3)
        sds = u.get_engine_subsystem(u.SubobjectDataSubsystem)
        require(sds is not None, "Public SubobjectDataSubsystem unavailable")
        report["mutation_started"] = True
        with u.ScopedEditorTransaction("Caves20261118 reviewed Q/Z structural shells"):
            for site in sites:
                site_id = site["site_id"]
                actor = actors_subsystem.spawn_actor_from_class(u.Actor, u.Vector(0, 0, 0),
                    u.Rotator(pitch=0, yaw=0, roll=0), False)
                require(actor is not None, "Cave structure actor spawn failed: " + site_id)
                report["created_actor_paths"].append(path(actor))
                report["packages"].append(actor.get_package().get_name())
                actor.set_actor_label(LABELS[site_id], True)
                actor.set_folder_path("Caves20261118/" + site_id)
                actor.set_editor_property("tags", [u.Name("Rogue10m.WorldLayout20261112"), u.Name(TAG),
                                                   u.Name(TAG + "." + site_id)])
                actor.set_editor_property("is_spatially_loaded", False)
                actor.set_actor_hidden_in_game(False)
                actor.set_actor_tick_enabled(False)
                require(not actor.is_actor_tick_enabled(), "Cave actor tick must stay disabled")
                require(actor.get_editor_property("is_spatially_loaded") is False
                        and actor.get_editor_property("hidden") is False, "Cave collision must remain always loaded and visible")
                parent, root_component = dressing._root_handle(u, sds, actor)
                actor.set_actor_transform(u.Transform(), False, True)
                site_report = {"site": site_id, "actor_path": path(actor), "label": actor.get_actor_label(),
                    "package": actor.get_package().get_name(), "folder": str(actor.get_folder_path()),
                    "root_component": path(root_component), "spatially_loaded": False, "tick_enabled": False,
                    "transform": dressing._transform(actor.get_actor_transform()), "components": []}
                report["sites"].append(site_report)
                bins = OrderedDict()
                for row in site["components"]:
                    bins.setdefault(row["material_role"], []).append(row)
                for bin_index, (role, rows) in enumerate(bins.items()):
                    _, component = dressing._new_component(u, sds, parent, u.InstancedStaticMeshComponent,
                        "CV18_" + site_id + "_ISM_" + str(bin_index).zfill(2))
                    component.set_material(0, materials[role])
                    component.set_static_mesh(mesh)
                    component.set_editor_property("component_tags", [u.Name(TAG), u.Name(TAG + "." + site_id),
                                                                   u.Name("CV18.Role." + role)])
                    configure_collision(u, component)
                    require(component.get_material(0) == materials[role], "Cave role material assignment differs")
                    cr = {"path": path(component), "name": component.get_name(), "role": role,
                          "mesh": path(mesh), "material": path(component.get_material(0)),
                          "collision_profile": str(component.get_collision_profile_name()),
                          "collision_enabled": str(component.get_collision_enabled()),
                          "pawn_response": str(component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)),
                          "camera_response": str(component.get_collision_response_to_channel(u.CollisionChannel.ECC_CAMERA)),
                          "visibility_response": str(component.get_collision_response_to_channel(u.CollisionChannel.ECC_VISIBILITY)),
                          "instances": []}
                    site_report["components"].append(cr)
                    for index, row in enumerate(rows):
                        value = dressing._native_transform(u, instance_row(row))
                        require(component.add_instance(value, world_space=True) == index, "Cave instance index differs")
                        observed = dressing._instance_transform(u, component, index)
                        expected = dressing._transform(value)
                        dressing._same_transform(expected, observed)
                        cr["instances"].append({"index": index, "label": row["label"],
                            "planned_world_transform": expected, "observed_world_transform": observed})
                        report["instance_count"] += 1
        after_actors = list(actors_subsystem.get_all_level_actors())
        require(len(after_actors) == 1406 and {path(actor) for actor in after_actors}
                == set(baseline["actors"]) | set(report["created_actor_paths"]), "Only two cave structure actors may be added")
        preserved = [actor for actor in after_actors if path(actor) in baseline["actors"]]
        require(old_reader._snapshot_actors(u, preserved, dressing) == before
                and inspector.persistent_components(u, preserved) == before_components,
                "An existing actor/component/material reference changed during additive cave authoring")
        after_native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(after_native.get("success") and after_native.get("composite_blake3") == expected_composite_blake3
                and after_native.get("original_layer_blake3") == expected_original_layer_blake3,
                "Terrain changed during structural authoring")
        after_dirty = set(dressing._dirty(u))
        new_allowed = set(report["packages"]) | {name for name in after_dirty
            if name.startswith("/Game/__ExternalObjects__/" + TARGET[6:] + "/")}
        require(set(expected_dirty_packages) <= after_dirty
                and after_dirty <= set(expected_dirty_packages) | new_allowed,
                "Unexpected dirty package change during cave authoring")
        require(report["instance_count"] == 286 and len(report["sites"]) == 2, "Exact286 instances/two structures required")
        report.update(success=True, existing1404_actor_snapshots_equal=True, existing_components_equal=True,
                      existing_actor_snapshot_sha256_after=digest(before), existing_components_sha256_after=digest(before_components),
                      dirty_after=sorted(after_dirty), actor_count_after=1406,
                      finished_utc=datetime.now(timezone.utc).isoformat(),
                      validation_pending=["ground and camera collision probes", "actual third-person traversal and captures",
                                          "bounded package save", "independent cold reload"])
        return report
    except Exception:
        report["traceback"] = traceback.format_exc()
        raise CaveStructureError(report) from None
