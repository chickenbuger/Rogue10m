"""Explicit, unsaved creation of three village-earth decal floors.

Import is inert. The owner calls build_village_earth with both settled native
height hashes and allow_editor_mutation=True, after the saved height-only stage
and the one additive boss threshold. No terrain, old actor, source material,
collision setting, save, retry, rollback, map switch or editor lifecycle call.
Failure preserves partial work and exposes VillageEarthError.report.
"""
from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import traceback


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117"
PLAN = BASE / "village-earth-plan20261118.json"
PLAN_SHA256 = "e6af4032bdf2d1a12aa1a2f6a9a752842457c0c8de086e4dcb020718e0ca5e90"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117"
SOURCE = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117/M_RoadSurface"
MATERIAL = OWN + "/M_VillageEarth"
TAG = "Rogue10m.VillageEarthFloor20261118"
SCOPE_TAG = "Rogue10m.NaturalBoundary20261117"
BASE_TAG = "Rogue10m.WorldLayout20261112"
ROAD_TAG = "Rogue10m.LandscapeRoadSurface20261117"
FOLDER = "NaturalWorld20261117/VillageIdentity"


class VillageEarthError(RuntimeError):
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


def _sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _disk(package):
    _require(package.startswith("/Game/"), "Project package required")
    return ROOT / "Content" / (package[6:] + ".uasset")


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                    ensure_ascii=False, allow_nan=False).encode("utf-8")).hexdigest()


def _owned(package):
    return (package == TARGET or package.startswith(OWN + "/") or
            any(package.startswith(prefix + TARGET[6:] + "/")
                for prefix in ("/Game/__ExternalActors__/", "/Game/__ExternalObjects__/")))


def _native(u, composite, original):
    for value in (composite, original):
        _require(isinstance(value, str) and len(value) == 64
                 and all(c in "0123456789abcdef" for c in value.lower()), "Two explicit native hashes required")
    value = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
    _require(value.get("success") is True and value.get("read_only") is True
             and value.get("dirty_package_names_unchanged") is True
             and value.get("landscape_reports_up_to_date") is True
             and value.get("component_count") == value.get("collision_count") == 45
             and value.get("layer_count") == 2 and value.get("width") == 568 and value.get("height") == 316
             and value.get("composite_blake3", "").lower() == composite.lower()
             and value.get("original_layer_blake3", "").lower() == original.lower(),
             "Exact settled two-layer terrain/native hashes required")
    return value


def _definitions(u):
    return (
        ("coord", u.MaterialExpressionTextureCoordinate,
         {"coordinate_index": 0, "u_tiling": 1.0, "v_tiling": 1.0}),
        ("center", u.MaterialExpressionConstant2Vector, {"r": 0.5, "g": 0.5}),
        ("distance", u.MaterialExpressionDistance, {}),
        ("diameter", u.MaterialExpressionMultiply, {"const_b": 2.0}),
        ("falloff", u.MaterialExpressionSmoothStep, {"const_min": 0.975, "const_max": 1.0}),
        ("inside", u.MaterialExpressionOneMinus, {}),
        ("opacity", u.MaterialExpressionMultiply, {}),
    )


def _fingerprint(u, lib, material, reader):
    value = reader._fingerprint(u, lib, material)
    for node in lib.get_material_expressions(material):
        if isinstance(node, u.MaterialExpressionConstant2Vector):
            value["nodes"][node.get_name()]["properties"] = {
                key: float(node.get_editor_property(key)) for key in ("r", "g")}
    return value


def _binding_preflight(u, lib):
    """Only class/CDO reads; no duplicated asset or new actor until complete."""
    specs = (
        (u.MaterialEditingLibrary, ("get_material_expressions", "get_material_expression_input_names",
          "get_material_expression_output_names", "get_inputs_for_material_expression",
          "get_input_node_output_name_for_material_expression", "get_material_property_input_node",
          "get_material_property_input_node_output_name", "create_material_expression",
          "connect_material_expressions", "connect_material_property", "recompile_material")),
        (u.EditorAssetLibrary, ("does_asset_exist", "duplicate_asset")),
        (u.EditorActorSubsystem, ("get_all_level_actors", "spawn_actor_from_class")),
        (u.Actor, ("get_actor_transform", "get_actor_rotation", "get_actor_location", "get_actor_scale3d",
          "get_components_by_class", "get_component_by_class", "get_folder_path", "get_package",
          "get_actor_label", "get_editor_property", "set_editor_property", "set_actor_label", "set_folder_path")),
        (u.DecalComponent, ("set_decal_material", "get_decal_material", "set_fade_screen_size", "set_sort_order")),
        (u.PrimitiveComponent, ("get_collision_enabled", "get_collision_profile_name")),
        (u.Rogue10mNaturalBoundary20261117Authoring, ("inspect_candidate",)),
    )
    result = {"read_only": True, "methods": {}, "properties": {}, "pin_names": {}}
    for cls, methods in specs:
        found = {name: callable(getattr(cls, name, None)) for name in methods}
        result["methods"][cls.__name__] = found
        _require(all(found.values()), "Missing public Python API before mutation: " + cls.__name__)
    for role, cls, values in _definitions(u):
        cdo = u.get_default_object(cls.static_class())
        for key in values:
            cdo.get_editor_property(key)
        names = list(map(str, lib.get_material_expression_input_names(cdo)))
        result["properties"][role] = list(values)
        result["pin_names"][role] = names
        required = {"distance": {"A", "B"}, "diameter": {"A", "B"},
                    "falloff": {"Min", "Max", "Value"}, "opacity": {"A", "B"}}.get(role)
        _require(required is None or set(names) == required, "Unexpected expression pins: " + role)
        _require(role != "inside" or len(names) == 1, "OneMinus pin unavailable")
    for cls, fields in ((u.DecalComponent, ("decal_size", "sort_order", "fade_screen_size", "visible",
                                           "fade_start_delay", "fade_duration", "fade_in_duration", "fade_in_start_delay")),
                        (u.Actor, ("tags", "hidden", "is_spatially_loaded")),
                        (u.ArrowComponent, ("bIsEditorOnly",)), (u.BillboardComponent, ("bIsEditorOnly",))):
        cdo = u.get_default_object(cls.static_class())
        for field in fields:
            cdo.get_editor_property(field)
        result["properties"][cls.__name__] = list(fields)
    decal_cdo = u.get_default_object(u.DecalComponent.static_class())
    _require(all(float(decal_cdo.get_editor_property(k)) == 0.0 for k in
                 ("fade_start_delay", "fade_duration", "fade_in_duration", "fade_in_start_delay")),
             "Persistent native decal lifetime defaults required before mutation")
    return result


def _decal(u, actor):
    components = list(actor.get_components_by_class(u.DecalComponent))
    _require(len(components) == 1, "Expected one native decal component")
    component = components[0]
    primitives = []
    for primitive in actor.get_components_by_class(u.PrimitiveComponent):
        primitives.append({"name": primitive.get_name(), "class": _path(primitive.get_class()),
            "editor_only": bool(primitive.get_editor_property("bIsEditorOnly")),
            "collision_profile": str(primitive.get_collision_profile_name()),
            "collision_enabled": str(primitive.get_collision_enabled())})
    return component, {"path": _path(actor), "label": actor.get_actor_label(),
        "package": actor.get_package().get_name(), "class": _path(actor.get_class()),
        "location_cm": [float(v) for v in (actor.get_actor_location().x, actor.get_actor_location().y, actor.get_actor_location().z)],
        "rotation_pitch_yaw_roll": [float(getattr(actor.get_actor_rotation(), k)) for k in ("pitch", "yaw", "roll")],
        "quaternion_xyzw": [float(getattr(actor.get_actor_rotation().quaternion(), k)) for k in "xyzw"],
        "scale": [float(getattr(actor.get_actor_scale3d(), k)) for k in "xyz"],
        "half_extents_cm": [float(getattr(component.get_editor_property("decal_size"), k)) for k in "xyz"],
        "material": _path(component.get_decal_material()), "sort_order": int(component.get_editor_property("sort_order")),
        "fade_screen_size": float(component.get_editor_property("fade_screen_size")),
        "fade_times": [float(component.get_editor_property(k)) for k in
                       ("fade_start_delay", "fade_duration", "fade_in_duration", "fade_in_start_delay")],
        "visible": bool(component.get_editor_property("visible")), "hidden": bool(actor.get_editor_property("hidden")),
        "folder": str(actor.get_folder_path()), "tags": list(map(str, actor.tags)),
        "spatially_loaded": bool(actor.get_editor_property("is_spatially_loaded")),
        "editor_primitives": sorted(primitives, key=lambda v: v["name"])}


def build_village_earth(*, allow_editor_mutation=False, expected_composite_blake3=None,
                       expected_original_layer_blake3=None):
    """Return JSON report. Explicit owner invocation only; never saves anything."""
    report = {"schema": "Rogue10m.VillageEarthFloorAuthor.20261118.v1", "success": False,
        "started_utc": datetime.now(timezone.utc).isoformat(), "step": "preflight",
        "assets_saved": False, "save_called": False, "terrain_mutated": False,
        "existing_actor_changes": [], "source_materials_mutated": [],
        "created_material": None, "created_actor_paths": [], "actors": [], "automatic_retry": False,
        "automatic_rollback": False, "partial_work_deleted": False,
        "visual_validation_passed": False, "render_settle_pending": True}
    try:
        _require(allow_editor_mutation is True, "Explicit allow_editor_mutation=True required")
        _require(_sha(PLAN) == PLAN_SHA256, "Reviewed immutable village-earth plan changed")
        plan = json.loads(PLAN.read_text(encoding="utf-8"))
        _require(plan["schema"] == "Rogue10m.VillageEarthFloor.20261118.v1"
                 and plan["map"] == TARGET and plan["source_material"] == SOURCE
                 and plan["material"] == MATERIAL and len(plan["decals"]) == 3, "Wrong village-earth plan")
        import unreal as u
        reader = _module("village_earth_graph_reader", "Scripts/Editor/NaturalRoadMaterial20261117.py")
        dressing = _module("village_earth_actor_reader", "Scripts/Editor/NaturalWorldDressing20261117.py")
        boss = _module("village_earth_boss_reader", "Scripts/Editor/AuthorNaturalBossThreshold20261118.py")
        world = reader._guard(u)
        lib = u.MaterialEditingLibrary
        dirty_before = reader._dirty(u)
        _require(all(_owned(p) for p in dirty_before), "Unexpected dirty package outside candidate/OWN before mutation")
        report["binding_preflight"] = _binding_preflight(u, lib)
        report["native_before"] = _native(u, expected_composite_blake3, expected_original_layer_blake3)
        subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
        actors = list(subsystem.get_all_level_actors())
        baseline = json.loads((BASE / "baseline-live/actors.json").read_text(encoding="utf-8"))
        authored = json.loads((BASE / "author-live/author-content.json").read_text(encoding="utf-8"))
        bosses = [a for a in actors if a.get_actor_label() == "NW18_BossThreshold"]
        expected = {row["path"] for row in baseline} | set(authored["dressing"]["created_actor_paths"])
        _require(len(bosses) == 1 and len(expected) == 1400 and len(actors) == 1401
                 and {_path(a) for a in actors} == expected | {_path(bosses[0])}, "Exact1401 boss-only actors required")
        expected |= {_path(bosses[0])}
        labels = {row["label"] for row in plan["decals"]}
        _require(not any(a.get_actor_label() in labels or TAG in map(str, a.tags) for a in actors),
                 "A floor actor already exists; inspect partial work, never retry")
        _require(any(str(a.get_folder_path()) == FOLDER for a in actors), "Reuse existing VillageIdentity folder only")
        roads = [a for a in actors if ROAD_TAG in map(str, a.tags)]
        _require(len(roads) == 29 and all(isinstance(a, u.DecalActor) for a in roads), "Exact29 road decals required")
        road_before = {_path(a): _decal(u, a)[1] for a in roads}
        maximum = max(v["sort_order"] for v in road_before.values())
        _require(maximum < 100, "Road sort order at cap; cannot guarantee floor draws after roads")
        sort_order = maximum + 1
        _require(not u.EditorAssetLibrary.does_asset_exist(MATERIAL) and not _disk(MATERIAL).exists()
                 and u.find_object(None, MATERIAL + ".M_VillageEarth") is None, "Destination exists; no overwrite/retry")
        _require(_sha(_disk(SOURCE)) == plan["source_sha256"] and SOURCE not in dirty_before,
                 "Original saved nine-node road material changed")
        source = u.load_asset(SOURCE)
        _, source_before = reader._source_contract(u, lib, source)
        originals = {p: _sha(_disk(p)) for p in plan["protected_material_sha256"]}
        _require(originals == plan["protected_material_sha256"], "One of the five original village materials changed")
        original_objects = {p: u.find_object(None, p + "." + p.rsplit("/", 1)[1]) for p in originals}
        original_usage = {p: bool(o.get_editor_property("used_with_instanced_static_meshes"))
                          for p, o in original_objects.items() if o is not None}
        existing_before = boss._snapshot_actors(u, actors, dressing)
        _require(reader._dirty(u) == dirty_before and reader._guard(u) == world,
                 "Read-only preflight changed dirty packages/world")
        report.update(plan_file=PLAN.relative_to(ROOT).as_posix(), plan_sha256=PLAN_SHA256,
            dirty_before=dirty_before, actor_count_before=1401, source_before=source_before,
            source_sha256_before=plan["source_sha256"], original_village_material_sha256_before=originals,
            original_village_material_usage_before=original_usage, existing_actor_snapshot_sha256_before=_digest(existing_before),
            road_decals_before=road_before, road_max_sort_order=maximum, floor_sort_order=sort_order)
        report["step"] = "duplicate and configure owned floor material"
        material = u.EditorAssetLibrary.duplicate_asset(SOURCE, MATERIAL)
        _require(isinstance(material, u.Material) and _path(material) == MATERIAL + ".M_VillageEarth", "Unexpected duplicate")
        report["created_material"] = _path(material)
        scalars, duplicate_before = reader._source_contract(u, lib, material)
        _require(duplicate_before == source_before, "Duplicate changed original source graph")
        tint = next(n for n in lib.get_material_expressions(material) if isinstance(n, u.MaterialExpressionVectorParameter))
        tint.set_editor_property("default_value", u.LinearColor(r=0.70, g=0.54, b=0.34, a=1.0))
        scalars["RoadOpacity"].set_editor_property("default_value", 0.97)
        created = {}
        for i, (name, cls, values) in enumerate(_definitions(u)):
            node = lib.create_material_expression(material, cls, -1000 + (i % 4)*250, 650 + (i//4)*250)
            _require(node is not None, "Failed creating circle node " + name)
            created[name] = node
            for key, value in values.items():
                node.set_editor_property(key, value)
        links = (("coord", "distance", "A"), ("center", "distance", "B"), ("distance", "diameter", "A"),
                 ("diameter", "falloff", "Value"), ("falloff", "inside", ""), ("inside", "opacity", "B"))
        for left, right, pin in links:
            reader._connect(lib, material, created[left], created[right], pin)
        reader._connect(lib, material, scalars["RoadOpacity"], created["opacity"], "A")
        _require(lib.connect_material_property(created["opacity"], "", u.MaterialProperty.MP_OPACITY), "Opacity output failed")
        report["compile_errors"] = list(map(str, lib.recompile_material(material)))
        _require(report["compile_errors"] == [], "Owned floor material shader compile failed")
        graph = _fingerprint(u, lib, material, reader)
        classes = reader.ORIGINAL_CLASSES + Counter(cls.__name__ for _, cls, _ in _definitions(u))
        _require(len(graph["nodes"]) == 16 and Counter(v["class"] for v in graph["nodes"].values()) == classes,
                 "Exact16-node floor graph required")
        wanted_original = json.loads(json.dumps(source_before["nodes"]))
        wanted_original[tint.get_name()]["properties"]["default_value"] = [0.70, 0.54, 0.34, 1.0]
        wanted_original[scalars["RoadOpacity"].get_name()]["properties"]["default_value"] = 0.97
        for name, row in wanted_original.items():
            actual = graph["nodes"][name]
            if name in (tint.get_name(), scalars["RoadOpacity"].get_name()):
                _require(actual["class"] == row["class"] and actual["inputs"] == row["inputs"]
                         and actual["properties"].get("parameter_name") == row["properties"]["parameter_name"],
                         "Duplicate tint/opacity node identity changed")
                av = actual["properties"]["default_value"]
                bv = row["properties"]["default_value"]
                _require(all(abs(a-b) < 1e-6 for a, b in zip(av, bv)) if isinstance(av, list)
                         else abs(av-bv) < 1e-6, "Tint/opacity default readback differs")
            else:
                _require(actual == row, "An original WorldXY/gravel/roughness node changed: " + name)
        for name, _, values in _definitions(u):
            for key, value in values.items():
                _require(abs(float(created[name].get_editor_property(key))-float(value)) < 1e-6,
                         "Circle property differs after compile: " + name + "/" + key)
        exact_inputs = {name: {} for name in created}
        for left, right, pin in links:
            exact_inputs[right][pin] = created[left]
        exact_inputs["opacity"]["A"] = scalars["RoadOpacity"]
        for name, node in created.items():
            reader._require_inputs(lib, material, node, exact_inputs[name])
        expected_outputs = dict(source_before["outputs"])
        expected_outputs[str(u.MaterialProperty.MP_OPACITY.value)] = {"node": created["opacity"].get_name(),
            "output": str(list(lib.get_material_expression_output_names(created["opacity"]))[0])}
        _require(graph["outputs"] == expected_outputs and graph["domain"] == source_before["domain"]
                 and graph["blend"] == source_before["blend"], "Output/domain/blend changed outside requested Opacity")
        report["material"] = {"path": _path(material), "package": MATERIAL, "graph": graph,
            "node_count": 16, "original_node_count": 9, "new_node_count": 7,
            "formula": "RoadOpacity*(1-smoothstep(.975,1,2*distance(TexCoord.xy,float2(.5,.5))))",
            "tint_rgba": [0.70, 0.54, 0.34, 1.0], "opacity": 0.97}
        report["step"] = "spawn exactly three shallow floor decals"
        with u.ScopedEditorTransaction("Village brown soil floors 20261118"):
            for row in plan["decals"]:
                actor = subsystem.spawn_actor_from_class(u.DecalActor, u.Vector(*row["center_cm"]),
                    u.Rotator(pitch=-90.0, yaw=0.0, roll=0.0), False)
                _require(actor is not None, "Floor spawn failed: " + row["label"])
                report["created_actor_paths"].append(_path(actor))
                actor.set_actor_label(row["label"], True)
                actor.set_folder_path(FOLDER)
                actor.set_editor_property("tags", [u.Name(t) for t in (BASE_TAG, SCOPE_TAG, TAG, TAG + "." + row["village"])])
                actor.set_editor_property("is_spatially_loaded", False)
                component, _ = _decal(u, actor)
                component.set_editor_property("decal_size", u.Vector(*row["half_extents_cm"]))
                component.set_sort_order(sort_order)
                component.set_fade_screen_size(0.0)
                component.set_decal_material(material)
                _, actual = _decal(u, actor)
                expected_q = u.Rotator(pitch=-90.0, yaw=0.0, roll=0.0).quaternion()
                expected_q = [float(getattr(expected_q, key)) for key in "xyzw"]
                _require(actual["class"] == "/Script/Engine.DecalActor" and actual["label"] == row["label"]
                         and actual["package"].startswith("/Game/__ExternalActors__/" + TARGET[6:] + "/")
                         and max(abs(a-b) for a, b in zip(actual["location_cm"], row["center_cm"])) < .01
                         and max(abs(a-b) for a, b in zip(actual["half_extents_cm"], row["half_extents_cm"])) < .01
                         and abs(abs(sum(a*b for a, b in zip(actual["quaternion_xyzw"], expected_q)))-1) < 1e-5
                         and max(abs(v-1) for v in actual["scale"]) < 1e-6
                         and actual["material"] == MATERIAL + ".M_VillageEarth" and actual["sort_order"] == sort_order
                         and actual["fade_screen_size"] == 0 and actual["fade_times"] == [0.0]*4
                         and actual["visible"] and not actual["hidden"] and not actual["spatially_loaded"]
                         and actual["folder"] == FOLDER, "Floor decal readback differs: " + row["label"])
                primitives = actual["editor_primitives"]
                _require(len(primitives) == 2 and {(p["name"], p["class"]) for p in primitives} == {
                    ("ArrowComponent0", "/Script/Engine.ArrowComponent"), ("Sprite", "/Script/Engine.BillboardComponent")}
                    and all(p["editor_only"] is True and p["collision_profile"] == "NoCollision"
                            and p["collision_enabled"] == str(u.CollisionEnabled.NO_COLLISION) for p in primitives),
                    "Only native editor visualization primitives with NoCollision are permitted")
                report["actors"].append(actual)
        report["step"] = "preservation and scope verification"
        after_actors = list(subsystem.get_all_level_actors())
        _require(len(after_actors) == 1404 and {_path(a) for a in after_actors}
                 == expected | set(report["created_actor_paths"]), "Only3 declared actors may be added")
        existing_after = boss._snapshot_actors(u, [a for a in after_actors if _path(a) in expected], dressing)
        _require(existing_after == existing_before, "An existing actor/primitive/material/ISM changed")
        _require({_path(a): _decal(u, a)[1] for a in roads} == road_before, "Existing29 road decals changed")
        _require(reader._fingerprint(u, lib, source) == source_before and _sha(_disk(SOURCE)) == plan["source_sha256"],
                 "Original source graph/disk SHA changed")
        _require({p: _sha(_disk(p)) for p in originals} == originals
                 and {p: bool(o.get_editor_property("used_with_instanced_static_meshes"))
                      for p, o in original_objects.items() if o is not None} == original_usage,
                 "An original village material changed")
        report["native_after"] = _native(u, expected_composite_blake3, expected_original_layer_blake3)
        dirty_after = reader._dirty(u)
        created_packages = {a["package"] for a in report["actors"]}
        _require(set(dirty_before) <= set(dirty_after) and all(_owned(p) for p in dirty_after)
                 and set(dirty_after)-set(dirty_before) <= created_packages | {MATERIAL, TARGET}
                 and reader._guard(u) == world and not _disk(MATERIAL).exists(),
                 "Unexpected dirty package/world/disk save during floor authoring")
        report.update(success=True, status="unsaved_floor_material_and_three_decals_requires_visual_save_cold_qa",
            step="complete", actor_count_after=1404, packages=sorted(created_packages | {MATERIAL}),
            dirty_after=dirty_after, source_sha256_after=_sha(_disk(SOURCE)), source_after=source_before,
            original_village_material_sha256_after=originals, existing1401_actor_snapshots_equal=True,
            existing_actor_snapshot_sha256_after=_digest(existing_after),
            native_height_hashes_equal=True, finished_utc=datetime.now(timezone.utc).isoformat(),
            validation_pending=["render settle and actual village/road-edge Game View", "bounded save", "cold reload validation"],
            render_scope="Shallow decal projection can tint any receiver within +/-16cm; not a mesh-ground replacement")
        return report
    except Exception as error:
        report.update(status="failed_partial_work_preserved_no_retry", error=repr(error),
                      traceback=traceback.format_exc(), finished_utc=datetime.now(timezone.utc).isoformat())
        raise VillageEarthError(report) from error
