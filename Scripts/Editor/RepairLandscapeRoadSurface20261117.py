"""Explicit read-only preflight and unsaved two-road visual prototype.

Import is inert. This module never loads a map, saves, deletes actors, edits
Landscape data, rebuilds collision, starts PIE or advances physics. The sole
editor owner calls preflight(), reviews its evidence, then prototype() explicitly.
"""
from __future__ import annotations

import datetime
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import sys
import traceback


ROOT = Path(__file__).resolve().parents[2]
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117"
MATERIAL = OWN + "/M_RoadSurface"
SOURCE_MATERIAL = "/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_EarthPath"
BASE_TAG = "Rogue10m.WorldLayout20261112"
SURFACE_TAG = "Rogue10m.LandscapeRoadSurface20261117"
PREFIX = "WL26_RoadSurface20261117_"
BUILDER = ROOT / "Scripts/Editor/RepairWorldLayoutRoadVisuals20261112.py"
LEGACY = ROOT / "Scripts/Editor/ValidateWorldLayout20261112.py"
LAYOUT = ROOT / "Docs/References/WorldMap/world-layout-20261112.json"
EVIDENCE = ROOT / "Feature/doc/evidence/landscape-road-surface20261117"
PREFLIGHT = EVIDENCE / "preflight.json"
PROTOTYPE = EVIDENCE / "prototype.json"
PROTOTYPE_EDGES = ("E04", "E11")
WIDTH_CM, SAMPLE_INTERVAL_CM, Z_MARGIN_CM = 800.0, 200.0, 300.0
RAY_TOP_CM, RAY_BOTTOM_CM, FALLBACK_RADIUS_CM = 50000.0, -20000.0, 1.0


def _hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if not spec or not spec.loader:
        raise RuntimeError("Missing query/material source: " + str(path))
    module = importlib.util.module_from_spec(spec)
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    return module


def _exclusive(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + "\n")


def _flush(path, report):
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _asset(value):
    return value.get_path_name().split(".")[0] if value else None


def _xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def _dirty(u):
    return sorted({p.get_path_name() for p in
                   list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
                   + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})


def _terrain(u):
    helper = getattr(u, "Rogue10mCrop20261114WorldAuthoring", None)
    if helper is None:
        raise RuntimeError("Exact-candidate native observer is unavailable")
    value = json.loads(helper.inspect_candidate_terrain())
    counts = ("component_count", "collision_component_count", "unique_component_count", "unique_collision_component_count")
    if (value.get("success") is not True or value.get("retained_9_by_5_lattice_observed") is not True
            or value.get("all_components_registered") is not True or any(value.get(k) != 45 for k in counts)):
        raise RuntimeError("Exact 45 registered Landscape/collision pairs required")
    return value


def _context(*, require_clean=True):
    import unreal as u
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
    world = editor.get_editor_world()
    if (Path(u.Paths.project_dir()).resolve() != ROOT.resolve() or not world
            or _asset(world) != TARGET or levels.is_in_play_in_editor()):
        raise RuntimeError("Exact Crop project/editor world with PIE stopped required; no world is switched")
    if require_clean and _dirty(u):
        raise RuntimeError("Resolve unsaved packages before this operation")
    inventory = _terrain(u)
    return u, world, u.get_editor_subsystem(u.EditorActorSubsystem), inventory


def _source_roads(u, actor_system):
    actors = list(actor_system.get_all_level_actors())
    if any(a.get_actor_label().startswith(PREFIX) or SURFACE_TAG in map(str, a.get_editor_property("tags")) for a in actors):
        raise RuntimeError("Prior/partial road-surface candidate exists; inspect it instead of retrying")
    layout = json.loads(LAYOUT.read_text(encoding="utf-8-sig"))
    node = {n["id"]: n["positionM"] for n in layout["nodes"]}
    expected = set()
    for edge in layout["edges"]:
        a, b = node[edge["from"]], node[edge["to"]]
        length = 100 * math.hypot(a["x"] - b["x"], a["y"] - b["y"])
        for i in range(math.ceil(length / 500)):
            expected.add("WL26_Road_%s_%03d" % (edge["id"], i))
    if len(layout["edges"]) != 29 or len(expected) != 663:
        raise RuntimeError("Canonical 29-road/663-piece topology changed; review first")
    sources, snapshots = {}, []
    for actor in actors:
        label = actor.get_actor_label()
        if not label.startswith("WL26_Road_"):
            continue
        tags = sorted(map(str, actor.get_editor_property("tags")))
        if (label not in expected or label in sources or not isinstance(actor, u.StaticMeshActor)
                or not {BASE_TAG, BASE_TAG + ".Scene"} <= set(tags)
                or not re.fullmatch(r"WL26_Road_E\d{2}_\d{3}", label)):
            raise RuntimeError("Unexpected/duplicate/foreign source road: " + label)
        component = actor.get_component_by_class(u.StaticMeshComponent)
        if (not component or _asset(component.get_editor_property("static_mesh")) != "/Engine/BasicShapes/Cube"
                or str(component.get_collision_profile_name()) != "NoCollision"):
            raise RuntimeError("Source mesh/collision differs: " + label)
        if not component.get_editor_property("visible") or actor.get_editor_property("hidden"):
            raise RuntimeError("Source visibility changed; preserve it for separate review: " + label)
        rot = actor.get_actor_rotation()
        sources[label] = (actor, component)
        snapshots.append({"label": label, "actor_path": actor.get_path_name(),
                          "component_path": component.get_path_name(), "tags": tags,
                          "location_cm": _xyz(actor.get_actor_location()), "scale": _xyz(actor.get_actor_scale3d()),
                          "rotation_pitch_yaw_roll": [rot.pitch, rot.yaw, rot.roll],
                          "component_visible": True, "actor_hidden_in_game": False,
                          "collision_profile": "NoCollision",
                          "materials": [_asset(component.get_material(i)) for i in range(component.get_num_materials())]})
    if set(sources) != expected:
        raise RuntimeError("Load every source road before preflight; missing " + str(sorted(expected - set(sources))))
    return layout, actors, sources, sorted(snapshots, key=lambda x: x["label"])


def _source_appearance(u):
    material = u.load_asset(SOURCE_MATERIAL)
    if not isinstance(material, u.MaterialInstanceConstant):
        raise RuntimeError("Existing EarthPath MaterialInstanceConstant unavailable")
    lib = u.MaterialEditingLibrary
    texture = lib.get_material_instance_texture_parameter_value(material, "BaseColorTexture")
    if not isinstance(texture, u.Texture2D):
        raise RuntimeError("Existing BaseColorTexture did not resolve; no substitute")
    if "ColorTint" not in set(map(str, lib.get_vector_parameter_names(material))):
        raise RuntimeError("Existing ColorTint parameter unavailable; no guessed tint")
    tint = lib.get_material_instance_vector_parameter_value(material, "ColorTint")
    return texture, tint


def preflight():
    """Read current ground every <=2m across +/-400cm and record 29 boxes.

    A radius1cm actual sphere hit is permitted only for a direct ray miss and is
    identified as finite-footprint support for projection bounds, never as a
    strict central point-ray pass. Any unresolved sample stops this candidate.
    """
    if PREFLIGHT.exists() or PROTOTYPE.exists():
        raise RuntimeError("Preserve prior evidence; no automatic rerun")
    u, world, actor_system, inventory = _context()
    if u.EditorAssetLibrary.does_asset_exist(MATERIAL):
        raise RuntimeError("New owned material path exists; no overwrite")
    layout, actors, _, before = _source_roads(u, actor_system)
    texture, tint = _source_appearance(u)
    if _dirty(u):
        raise RuntimeError("Read-only source inspection unexpectedly dirtied packages")
    legacy = _module("road_surface_trace_packing", LEGACY)
    ignored = [a for a in actors if a.get_actor_label() == "WL26_PersistentBossGround"
               and BASE_TAG in map(str, a.get_editor_property("tags"))]
    report = {"status": "started_read_only_preflight", "target": TARGET,
              "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "source_sha256": {p.relative_to(ROOT).as_posix(): _hash(p) for p in (BUILDER, LEGACY, LAYOUT)},
              "material_path": MATERIAL, "source_material": SOURCE_MATERIAL, "texture": _asset(texture),
              "tint_rgba": [tint.r, tint.g, tint.b, tint.a], "source_roads": before,
              "native_inventory_before": inventory, "width_cm": WIDTH_CM, "z_margin_cm": Z_MARGIN_CM,
              "samples": [], "decals": [], "direct_ray_count": 0, "sphere_fallback_count": 0,
              "terrain_mutated": False, "collision_rebuilt": False, "packages_saved": [],
              "limitation": "Current physics projection bounds only; finite-radius fallback is not a strict point-ray pass. Visual review and cold reload are separate."}
    _exclusive(PREFLIGHT, report)
    try:
        nodes = {n["id"]: n["positionM"] for n in layout["nodes"]}
        for edge in layout["edges"]:
            if u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world() != world or _dirty(u):
                raise RuntimeError("World/clean checkpoint changed during read-only preflight")
            a, b = nodes[edge["from"]], nodes[edge["to"]]
            ax, ay, bx, by = 15000 + 100*a["x"], 32550 + 100*a["y"], 15000 + 100*b["x"], 32550 + 100*b["y"]
            dx, dy = bx-ax, by-ay
            length = math.hypot(dx, dy)
            count = max(1, math.ceil(length / SAMPLE_INTERVAL_CM))
            nx, ny = -dy/length, dx/length
            heights = []
            for i in range(count+1):
                for offset in (-WIDTH_CM/2, 0.0, WIDTH_CM/2):
                    x, y = ax+dx*i/count+nx*offset, ay+dy*i/count+ny*offset
                    start, end = u.Vector(x,y,RAY_TOP_CM), u.Vector(x,y,RAY_BOTTOM_CM)
                    hit = legacy._unpack_hit(u, u.SystemLibrary.line_trace_single(
                        world, start, end, u.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignored, u.DrawDebugTrace.NONE, True))
                    report["direct_ray_count"] += 1
                    raw, method = hit, "direct_landscape_ray"
                    if not hit["blocking"] and not hit["initial_overlap"] and hit["actor_path"] is None:
                        hit = legacy._unpack_hit(u, u.SystemLibrary.sphere_trace_single(
                            world, start, end, FALLBACK_RADIUS_CM, u.TraceTypeQuery.TRACE_TYPE_QUERY1,
                            False, ignored, u.DrawDebugTrace.NONE, True))
                        report["sphere_fallback_count"] += 1
                        method = "actual_radius1cm_sphere_impact_not_central_point_ray"
                    valid = (hit["blocking"] and not hit["initial_overlap"] and hit["is_landscape"]
                             and hit["impact_point_cm"] is not None
                             and all(math.isfinite(v) for v in hit["impact_point_cm"]))
                    report["samples"].append({"edge_id": edge["id"], "index": i, "offset_cm": offset,
                                               "xy_cm": [x,y], "raw_ray": raw, "hit": hit,
                                               "method": method, "valid": bool(valid)})
                    if not valid:
                        raise RuntimeError("Unresolved actual ground sample: %s/%d/%g" % (edge["id"], i, offset))
                    heights.append(hit["impact_point_cm"][2])
            low, high = min(heights)-Z_MARGIN_CM, max(heights)+Z_MARGIN_CM
            report["decals"].append({"edge_id": edge["id"], "label": PREFIX+edge["id"],
                                      "center_cm": [(ax+bx)/2,(ay+by)/2,(low+high)/2],
                                      "rotation": {"pitch": -90.0,"yaw": math.degrees(math.atan2(dy,dx)),"roll": 0.0},
                                      "local_half_extents_cm": [(high-low)/2,WIDTH_CM/2,length/2],
                                      "observed_height_min_max_cm": [min(heights),max(heights)],
                                      "projection_z_bounds_cm": [low,high],"length_cm": length})
            _flush(PREFLIGHT, report)
        _context()
        if len(report["decals"]) != 29:
            raise RuntimeError("Incomplete 29-edge projection plan")
        report.update(status="read_only_preflight_passed_prototype_not_authored",
                      native_inventory_after=_terrain(u), finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        _flush(PREFLIGHT, report)
        return report
    except Exception as error:
        report.update(status="preflight_failed_preserve_evidence_no_retry", error=repr(error), traceback=traceback.format_exc())
        _flush(PREFLIGHT, report)
        raise


def prototype(*, allow_editor_mutation=False):
    """Create exactly E04/E11 projectors and hide their 34 source pieces.

    No save or automatic cleanup. Failure preserves partial assets, original
    exception and changed actor lists for owner inspection; never rerun blindly.
    """
    if allow_editor_mutation is not True:
        raise RuntimeError("Owner must explicitly opt into the unsaved prototype")
    if PROTOTYPE.exists():
        raise RuntimeError("Prior/partial prototype evidence exists; no automatic retry")
    plan = json.loads(PREFLIGHT.read_text(encoding="utf-8-sig"))
    if plan.get("status") != "read_only_preflight_passed_prototype_not_authored" or plan.get("target") != TARGET:
        raise RuntimeError("A successful exact-candidate preflight is required")
    for path,digest in plan["source_sha256"].items():
        if _hash(ROOT/path) != digest:
            raise RuntimeError("Preflight source changed; review before authoring")
    u, world, actor_system, _ = _context()
    _, _, sources, before = _source_roads(u, actor_system)
    if before != plan["source_roads"] or u.EditorAssetLibrary.does_asset_exist(MATERIAL):
        raise RuntimeError("Source actors or new material destination changed")
    texture,tint = _source_appearance(u)
    if _asset(texture) != plan["texture"] or [tint.r,tint.g,tint.b,tint.a] != plan["tint_rgba"]:
        raise RuntimeError("Read-only source appearance changed")
    helper = u.Rogue10mCrop20261114WorldAuthoring
    terrain_before = json.loads(helper.snapshot_retained_terrain_data())
    if terrain_before.get("success") is not True or _dirty(u):
        raise RuntimeError("Clean successful retained terrain snapshot required before prototype")
    selected = [r for r in plan["decals"] if r["edge_id"] in PROTOTYPE_EDGES]
    selected_labels = [name for name in sorted(sources) if name.split("_")[2] in PROTOTYPE_EDGES]
    if len(selected) != 2 or len(selected_labels) != 34:
        raise RuntimeError("Expected exactly 2 projector roads and 34 source pieces")
    report = {"status": "started_unsaved_prototype", "target": TARGET, "material_path": MATERIAL,
              "preflight_sha256": _hash(PREFLIGHT), "prototype_edges": PROTOTYPE_EDGES,
              "created_actors": [], "hidden_old_roads": [], "packages_saved": [], "actors_deleted": [],
              "terrain_mutated": False, "collision_changed": False, "terrain_before": terrain_before}
    _exclusive(PROTOTYPE, report)
    try:
        builder = _module("road_surface_material_builder_only", BUILDER)
        # Only this constructor is reused. Its original map guard, preflight,
        # repair entry point and old partial material are never called/changed.
        # Installed UE5.8 MaterialEditingLibrary.h:267 declares RecompileMaterial
        # as TArray<FString>; the builder treats an empty error array as success.
        material = builder._make_material({"unreal":u,"resume":None,"partial_material":None,
                                           "material_path":MATERIAL,"texture":texture,"tint":tint})
        report["created_material"] = material.get_path_name()
        for row in selected:
            if u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world() != world:
                raise RuntimeError("Editor world changed during prototype")
            actor = actor_system.spawn_actor_from_class(u.DecalActor,u.Vector(*row["center_cm"]),u.Rotator(**row["rotation"]))
            if not actor:
                raise RuntimeError("Projector creation failed: "+row["edge_id"])
            record = {"path":actor.get_path_name(),"edge_id":row["edge_id"],"setup_completed":False}
            report["created_actors"].append(record)
            _flush(PROTOTYPE,report)
            actor.set_actor_label(row["label"])
            actor.tags = [u.Name(BASE_TAG),u.Name(SURFACE_TAG)]
            actor.set_folder_path("RoadSurface20261117/Prototype")
            component = actor.get_component_by_class(u.DecalComponent)
            if not component:
                raise RuntimeError("DecalComponent unavailable")
            component.set_decal_material(material)
            component.set_editor_property("decal_size",u.Vector(*row["local_half_extents_cm"]))
            component.set_fade_screen_size(0.0)
            if (component.get_decal_material() != material
                    or max(abs(a-b) for a,b in zip(_xyz(component.get_editor_property("decal_size")),row["local_half_extents_cm"])) > 0.01):
                raise RuntimeError("Projector property readback failed")
            record.update(label=row["label"],setup_completed=True,plan=row)
        for label in selected_labels:
            actor,component = sources[label]
            component.set_visibility(False,True)
            actor.set_actor_hidden_in_game(True)
            report["hidden_old_roads"].append(label)
            if (component.get_editor_property("visible") or not actor.get_editor_property("hidden")
                    or str(component.get_collision_profile_name()) != "NoCollision"):
                raise RuntimeError("Source hide/collision readback failed: "+label)
        after = json.loads(helper.snapshot_retained_terrain_data())
        # Compare the native retained data payload, excluding query-local state
        # which necessarily changes when the prototype creates editor objects.
        keys = ("metadata_digest","semantic_snapshot_digest","composite","raw_edit_layers",
                "edit_layers","paint_layers","landscape_guid","landscape_actor_path","material_path")
        if after.get("success") is not True or any(terrain_before.get(k) != after.get(k) for k in keys):
            raise RuntimeError("Retained terrain payload changed during visual-only prototype")
        allowed = (MATERIAL,TARGET,"/Game/__ExternalActors__/"+TARGET[6:]+"/","/Game/__ExternalObjects__/"+TARGET[6:]+"/")
        if any(not (p in allowed[:2] or p.startswith(allowed[2:])) for p in _dirty(u)):
            raise RuntimeError("Unexpected dirty package outside the candidate prototype")
        report.update(status="unsaved_prototype_authored_requires_visual_review_no_save",terrain_after=after,
                      native_inventory_after=_terrain(u),dirty_packages_after=_dirty(u),
                      finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        _flush(PROTOTYPE,report)
        return report
    except Exception as error:
        report.update(status="partial_prototype_failed_preserve_evidence_no_retry",error=repr(error),traceback=traceback.format_exc(),
                      dirty_packages_after=_dirty(u),finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        _flush(PROTOTYPE,report)
        raise
