"""One unsaved, visual-only boss threshold on the saved rounded candidate.

Import is inert. Plan B explicitly preserves every existing boss support flag.
The editor owner calls author_boss_threshold(allow_editor_mutation=True) once,
after the height-only stage has passed and been saved. No existing actor,
material, Landscape, collision setting or package is changed or saved.
"""
from __future__ import annotations

from collections import OrderedDict
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import traceback


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117"
PLAN = BASE / "boss-threshold-plan20261118B.json"
PLAN_SHA256 = "7a267a6eecfc961a551215bc635a43e0efc4aedfd8018a973764f1bb624d09a0"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
LABEL = "NW18_BossThreshold"


class BossThresholdError(RuntimeError):
    def __init__(self, report):
        self.report = report
        super().__init__(json.dumps(report, ensure_ascii=False))


def _require(value, message):
    if not value:
        raise RuntimeError(message)


def _module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _path(value):
    return value.get_path_name() if value else None


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                    ensure_ascii=False, allow_nan=False).encode("utf-8")).hexdigest()


def _snapshot_actors(u, actors, dressing):
    """Observe existing actors/primitive settings and every old ISM transform."""
    result = {}
    for actor in actors:
        primitives = []
        for component in actor.get_components_by_class(u.PrimitiveComponent):
            row = {"path": _path(component), "class": _path(component.get_class()),
                "materials": [_path(component.get_material(i)) for i in range(component.get_num_materials())],
                "visible": bool(component.get_editor_property("visible")),
                "hidden_in_game": bool(component.get_editor_property("hidden_in_game")),
                "cast_shadow": bool(component.get_editor_property("cast_shadow")),
                "collision_profile": str(component.get_collision_profile_name()),
                "collision_enabled": str(component.get_collision_enabled()),
                "pawn_response": str(component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN))}
            if isinstance(component, u.StaticMeshComponent):
                row["mesh"] = _path(component.get_editor_property("static_mesh"))
            if isinstance(component, u.InstancedStaticMeshComponent):
                row["instances"] = [dressing._instance_transform(u, component, i)
                                    for i in range(component.get_instance_count())]
            primitives.append(row)
        result[_path(actor)] = {"class": _path(actor.get_class()), "label": actor.get_actor_label(),
            "transform": dressing._transform(actor.get_actor_transform()), "tags": list(map(str, actor.tags)),
            "folder": str(actor.get_folder_path()), "hidden_in_game": bool(actor.get_editor_property("hidden")),
            "spatially_loaded": bool(actor.get_editor_property("is_spatially_loaded")),
            "primitives": sorted(primitives, key=lambda value: value["path"])}
    return result


def author_boss_threshold(*, allow_editor_mutation=False):
    report = {"schema": "Rogue10m.NaturalBossThresholdAuthor.20261118.v1", "success": False,
        "started_utc": datetime.now(timezone.utc).isoformat(), "assets_saved": False,
        "existing_actor_changes": [], "landscape_mutated": False, "materials_created": [],
        "created_actor_paths": [], "components": [], "packages": []}
    try:
        _require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required")
        _require(hashlib.sha256(PLAN.read_bytes()).hexdigest() == PLAN_SHA256, "Reviewed immutable Plan B changed")
        plan = json.loads(PLAN.read_text(encoding="utf-8"))
        _require(plan["schema"] == "Rogue10m.NaturalBossThresholdPlan.20261118.v1" and plan.get("revision") == "B"
                 and plan["map"] == TARGET and plan["instance_count"] == len(plan["instances"]) == 5,
                 "Exact five-part Plan B required")
        import unreal as u
        dressing = _module("boss_threshold_dressing_api", "Scripts/Editor/NaturalWorldDressing20261117.py")
        dressing._guard(u)
        report["binding_preflight"] = dressing._binding_preflight(u)
        _require(not dressing._dirty(u), "Saved clean candidate required before threshold authoring")
        subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
        actors = list(subsystem.get_all_level_actors())
        baseline = json.loads((BASE / "baseline-live/actors.json").read_text(encoding="utf-8"))
        authored = json.loads((BASE / "author-live/author-content.json").read_text(encoding="utf-8"))
        expected = {row["path"] for row in baseline} | set(authored["dressing"]["created_actor_paths"])
        _require(len(actors) == len(expected) == 1400 and {_path(a) for a in actors} == expected
                 and not any(a.get_actor_label() == LABEL for a in actors), "Exact1400 saved actors required; no overwrite")
        validator = _module("boss_threshold_baseline_qa", "Scripts/Editor/ValidateNaturalWorld20261117.py")
        qa = validator.validate_editor(require_saved=True, refinement_stage="height_only")
        _require(qa.get("passed") is True, "Height-only saved baseline structural QA failed")
        report["baseline_qa"] = {"passed": True, "stage": "height_only", "checks": qa["checks"]}
        report["baseline_actor_count"] = 1400
        before = _snapshot_actors(u, actors, dressing)
        report["existing_actor_snapshot_sha256_before"] = _digest(before)
        report["boss_ground_before"] = before[plan["boss_ground"]["actor_path"]]
        materials = {}
        for package, digest in plan["material_disk_sha256"].items():
            filename = ROOT / "Content" / (package[6:] + ".uasset")
            material = u.load_asset(package)
            _require(hashlib.sha256(filename.read_bytes()).hexdigest() == digest
                     and isinstance(material, u.Material)
                     and material.get_editor_property("used_with_instanced_static_meshes") is True,
                     "Saved owned ISM material identity/usage changed")
            materials[package] = material
        mesh = u.load_asset("/Engine/BasicShapes/Cube")
        box = mesh.get_bounding_box() if isinstance(mesh, u.StaticMesh) else None
        _require(box is not None and max(abs(v-w) for v, w in zip(dressing._xyz(box.min), [-50]*3)) < .01
                 and max(abs(v-w) for v, w in zip(dressing._xyz(box.max), [50]*3)) < .01, "Verified100cm Cube required")
        _require(not dressing._dirty(u), "Read-only threshold preflight dirtied a package")
        report["plan_sha256"] = PLAN_SHA256
        report["plan_file"] = PLAN.relative_to(ROOT).as_posix()
        report["dirty_before"] = []
        sds = u.get_engine_subsystem(u.SubobjectDataSubsystem)
        _require(sds is not None, "Public component-authoring subsystem unavailable")
        with u.ScopedEditorTransaction("Natural-world visual boss threshold 20261118"):
            actor = subsystem.spawn_actor_from_class(u.Actor, u.Vector(0, 0, 0),
                                                     u.Rotator(pitch=0, yaw=0, roll=0), False)
            _require(actor is not None, "Threshold actor spawn failed")
            report["created_actor_paths"].append(_path(actor))
            actor.set_actor_label(LABEL, True)
            actor.set_folder_path(plan["folder"])
            actor.set_editor_property("tags", [u.Name(value) for value in plan["tags"]])
            actor.set_editor_property("is_spatially_loaded", False)
            actor.set_actor_hidden_in_game(False)
            parent, root_component = dressing._root_handle(u, sds, actor)
            actor.set_actor_transform(u.Transform(), False, True)
            bins = OrderedDict()
            for row in plan["instances"]:
                bins.setdefault(row["material"], []).append(row)
            for bin_index, (material_path, rows) in enumerate(bins.items()):
                _, component = dressing._new_component(u, sds, parent, u.InstancedStaticMeshComponent,
                    "NW18_BossThreshold_ISM_" + str(bin_index).zfill(2))
                component.set_static_mesh(mesh)
                component.set_material(0, materials[material_path])
                component.set_editor_property("component_tags", [u.Name("Rogue10m.NaturalWorldSetDressing20261117"),
                    u.Name("Rogue10m.NaturalBoundary20261117"), u.Name("NW18.Role.visual_only"),
                    u.Name("NW18.Bin." + str(bin_index).zfill(2))])
                dressing._configure(u, component, "visual_only", False)
                cr = {"path": _path(component), "name": component.get_name(),
                      "mesh": _path(mesh), "material": _path(component.get_material(0)),
                      "collision_profile": str(component.get_collision_profile_name()),
                      "collision_enabled": str(component.get_collision_enabled()), "instances": []}
                report["components"].append(cr)
                for index, row in enumerate(rows):
                    value = dressing._native_transform(u, row)
                    _require(component.add_instance(value, world_space=True) == index, "Threshold instance index differs")
                    observed = dressing._instance_transform(u, component, index)
                    dressing._same_transform(dressing._transform(value), observed)
                    cr["instances"].append({"index": index, "label": row["label"],
                        "planned_world_transform": dressing._transform(value), "observed_world_transform": observed})
        after_actors = list(subsystem.get_all_level_actors())
        _require(len(after_actors) == 1401 and {_path(a) for a in after_actors} == expected | {_path(actor)},
                 "Only the one declared threshold actor may be added")
        after = _snapshot_actors(u, [a for a in after_actors if _path(a) in expected], dressing)
        _require(before == after, "An existing actor/component/material/ISM changed during additive threshold authoring")
        report["existing_actor_snapshot_sha256_after"] = _digest(after)
        report["existing1400_actor_snapshots_equal"] = True
        report["boss_ground_after"] = after[plan["boss_ground"]["actor_path"]]
        report["actor"] = {"path": _path(actor), "label": actor.get_actor_label(), "class": _path(actor.get_class()),
            "package": actor.get_package().get_name(), "root_component": _path(root_component),
            "transform": dressing._transform(actor.get_actor_transform()), "folder": str(actor.get_folder_path()),
            "tags": list(map(str, actor.tags)), "spatially_loaded": bool(actor.get_editor_property("is_spatially_loaded"))}
        report["packages"] = [actor.get_package().get_name()]
        report["dirty_after"] = dressing._dirty(u)
        _require(set(report["dirty_after"]) <= {TARGET, actor.get_package().get_name()}
                 | {name for name in report["dirty_after"] if name.startswith("/Game/__ExternalObjects__/" + TARGET[6:] + "/")},
                 "Unexpected dirty package after additive threshold authoring")
        report.update(success=True, actor_count_after=1401, instance_count=5,
                      finished_utc=datetime.now(timezone.utc).isoformat(),
                      validation_pending=["independent threshold gate/settings QA", "rendered Game View", "bounded save and cold reload"])
        return report
    except Exception:
        report["traceback"] = traceback.format_exc()
        raise BossThresholdError(report) from None
