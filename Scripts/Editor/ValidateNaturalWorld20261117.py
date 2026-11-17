"""Independent, inert structural QA for the explicitly changed natural world.

Owner loads the exact complete candidate and calls validate_editor(). No level/region load,
save, compile, actor/component/property mutation, callback or PIE control occurs.
The report covers CPU terrain, planned transforms, public collision settings and
quest payloads. Rendered appearance, physical traversal, closed-boundary sweeps,
NavMesh, performance and full gameplay remain separate owner-run checks.
"""
from __future__ import annotations

from collections import OrderedDict
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import struct
import traceback


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117"
ROAD_MATERIAL = OWN + "/M_RoadFeather"
QUEST = OWN + "/DA_IntroSupply_Flat"
MATERIAL_DERIVATIVES = {"/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_"+name:
    OWN+"/M_Natural_"+name for name in ("DarkTimber", "WarmStone", "Banner_Teal", "Banner_Ochre", "Banner_Blue")}
BASE_TAG = "Rogue10m.WorldLayout20261112"
SCOPE_TAG = "Rogue10m.NaturalBoundary20261117"
DRESS_TAG = "Rogue10m.NaturalWorldSetDressing20261117"
GATE_REFLOW_GROUP = "WorldLayout20261112/Architecture/WL26_T_OpenLibrary"
GATE_REFLOW_LABELS = {"WL26_T_OpenLibrary_" + suffix for suffix in
    ("Floor", "Post-11", "Roof1", "Roof-1", "Post11", "Post1-1", "Banner", "Post-1-1")}
GATE_REFLOW_REASON = ("Actual Pawn capsule sweep: T/E17 left approach initially overlaps OpenLibrary_Post11 by 8.2cm (r34), "
    "six r34/r42 ground/jump cases; move the complete eight-part library 200cm west, preserving its relative layout.")
ORIGINAL_AUTHOR_PLAN_SHA256 = "3692b80e790cb06d79d853239b7f3a7cff447e16ca09f3147c39d0580991618d"
READ_ONLY = ("texture_sources_unchanged", "layer_metadata_unchanged",
    "landscape_selection_and_editing_layer_unchanged", "editor_pointer_unchanged",
    "loaded_uobject_identities_and_flags_unchanged", "editor_object_selection_unchanged",
    "transaction_queue_and_active_state_unchanged", "transaction_handle_readable", "dirty_package_names_unchanged")


def read(value):
    return value if isinstance(value, dict) else json.loads(Path(value).read_text(encoding="utf-8-sig"))


def validate_gate_reflow_plan(original, revised):
    """Pure, exact eight-part content exception; never relax the capsule QA."""
    rows = [r for r in original["moves"] if r["group"] == GATE_REFLOW_GROUP]
    if len(rows) != 8 or {r["label"] for r in rows} != GATE_REFLOW_LABELS or len({r["path"] for r in rows}) != 8:
        raise RuntimeError("Original plan is not the exact eight-part open library")
    expected = copy.deepcopy(original)
    for row in expected["moves"]:
        if row["group"] == GATE_REFLOW_GROUP:
            row["after_location"][0] -= 200.0
    expected["building_xy_offsets_cm"] = {GATE_REFLOW_GROUP: [-200, 0]}
    expected["gate_reflow_reason"] = GATE_REFLOW_REASON
    if revised != expected:
        raise RuntimeError("Gate reflow permits only the declared eight X-200cm locations, exact offset and recorded capsule reason")
    return {"building_xy_offsets_cm": expected["building_xy_offsets_cm"],
            "reason": GATE_REFLOW_REASON, "actor_paths": sorted(r["path"] for r in rows),
            "actor_labels": sorted(GATE_REFLOW_LABELS), "actor_count": 8}


def selected_author_plan():
    original_path = OUT / "author-plan.json"
    original = read(original_path)
    revised_path = OUT / "author-plan-gate-reflow.json"
    selection = {"original_file": str(original_path.relative_to(ROOT)),
                 "original_sha256": hashlib.sha256(original_path.read_bytes()).hexdigest()}
    if selection["original_sha256"] != ORIGINAL_AUTHOR_PLAN_SHA256:
        raise RuntimeError("The preserved pre-reflow author-plan.json changed")
    if revised_path.exists():
        revised = read(revised_path)
        selection.update(selected_file=str(revised_path.relative_to(ROOT)),
                         selected_sha256=hashlib.sha256(revised_path.read_bytes()).hexdigest(),
                         declared_horizontal_exception=validate_gate_reflow_plan(original, revised))
        return revised, selection
    selection.update(selected_file=selection["original_file"], selected_sha256=selection["original_sha256"],
                     declared_horizontal_exception=None)
    return original, selection


def path(value):
    return value.get_path_name() if value else None


def package(value):
    return path(value).split(".")[0] if value else None


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def close(a, b, tolerance=0.05):
    return len(a) == len(b) and all(math.isfinite(float(x)) and math.isfinite(float(y))
                                  and abs(float(x)-float(y)) <= tolerance for x, y in zip(a, b))


def basis(rotation):
    p, y, r = map(math.radians, rotation)
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    return [cp*cy, cp*sy, sp, sr*sp*cy-cr*sy, sr*sp*sy+cr*cy, -sr*cp,
            -(cr*sp*cy+sr*sy), cy*sr-cr*sp*sy, cr*cp]


def quat_basis(q):
    x, y, z, w = (float(getattr(q, key)) for key in "xyzw")
    length = math.sqrt(x*x+y*y+z*z+w*w)
    if not math.isfinite(length) or abs(length-1) > 0.001:
        raise RuntimeError("Invalid instance quaternion")
    return [1-2*(y*y+z*z), 2*(x*y+w*z), 2*(x*z-w*y),
            2*(x*y-w*z), 1-2*(x*x+z*z), 2*(y*z+w*x),
            2*(x*z+w*y), 2*(y*z-w*x), 1-2*(x*x+y*y)]


def strip_linked(value):
    if isinstance(value, dict):
        return {k: strip_linked(v) for k, v in value.items() if k != "metadata_linked_digest"}
    return [strip_linked(v) for v in value] if isinstance(value, list) else value


def grid_sha256(values):
    return hashlib.sha256(struct.pack("<" + str(len(values)) + "H", *values)).hexdigest()


def planned_checks(height_plan, author_plan):
    """Pure checks, with an independent uint16LE hash and bilinear grid reader."""
    heights = height_plan["target_heights"]
    w, h = height_plan["width"], height_plan["height"]
    bounds = height_plan["vertex_bounds"]
    checks = {"target_schema": height_plan["schema"] == "Rogue10m.NaturalBoundaryHeightPlan.20261117.v1",
              "target_map": height_plan["map"] == TARGET,
              "target_grid_shape": (w, h, bounds) == (568, 316, [315, 630, 882, 945]) and len(heights) == 179488,
              "target_grid_uint16": all(type(v) is int and 0 <= v <= 65535 for v in heights),
              "plan_actor_partition": (len(author_plan["moves"]), len(author_plan["road_decals"]),
                                       len(author_plan["unchanged_actor_paths"])) == (672, 29, 689),
              "plan_building_groups": len(author_plan["building_group_anchors"]) == 20,
              "plan_dressing_count": author_plan["dressing"]["group_count"] == 10
                                     and author_plan["dressing"]["instance_count"] == 708}
    if not all(checks.values()):
        return {"passed": False, "checks": checks}

    def sample(x, y):
        gx, gy = (x+90000)/(1250/7)-bounds[0], (y+90000)/(1250/7)-bounds[1]
        if not 0 <= gx <= w-1 or not 0 <= gy <= h-1:
            raise RuntimeError("Planned sample outside retained grid")
        ix, iy = min(w-2, math.floor(gx)), min(h-2, math.floor(gy))
        tx, ty = gx-ix, gy-iy
        a = heights[iy*w+ix]*(1-tx)+heights[iy*w+ix+1]*tx
        b = heights[(iy+1)*w+ix]*(1-tx)+heights[(iy+1)*w+ix+1]*tx
        return 2*((1-ty)*a+ty*b-32768)

    layout = read(ROOT / "Docs/References/WorldMap/world-layout-20261112.json")
    nodes = {n["id"]: (15000+n["positionM"]["x"]*100, 32550+n["positionM"]["y"]*100) for n in layout["nodes"]}
    road_heights = []
    for edge in layout["edges"]:
        a, b = nodes[edge["from"]], nodes[edge["to"]]
        dx, dy = b[0]-a[0], b[1]-a[1]
        length = math.hypot(dx, dy)
        count = max(1, math.ceil(length/500))
        for i in range(count+1):
            for offset in (-400, 0, 400):
                road_heights.append(sample(a[0]+dx*i/count-dy/length*offset, a[1]+dy*i/count+dx/length*offset))
    ridge_heights = []
    polygon = height_plan["polygon_world_cm"]
    for a, b in zip(polygon, polygon[1:]+polygon[:1]):
        count = max(1, math.ceil(math.dist(a, b)/500))
        ridge_heights += [sample(a[0]+(b[0]-a[0])*i/count, a[1]+(b[1]-a[1])*i/count) for i in range(count+1)]
    checks.update(target_road_8m_samples_flat=all(abs(v-9600) <= 2 for v in road_heights),
                  target_closed_polygon_ridge_samples=min(ridge_heights) >= 11000,
                  target_range=min(heights) == 37568 and max(heights) > 38000)
    return {"passed": all(checks.values()), "checks": checks, "height_uint16le_sha256": grid_sha256(heights),
            "road_sample_count": len(road_heights), "road_z_min_max": [min(road_heights), max(road_heights)],
            "ridge_sample_count": len(ridge_heights), "ridge_z_min_max": [min(ridge_heights), max(ridge_heights)],
            "boundary": "CPU planned grid checks; not collision or traversal evidence"}


def validate_editor(*, native_before=None, apply_report=None, author_report=None, material_repair_report=None, require_saved=False):
    """Return JSON-compatible structural QA; failures retain diagnostics, no edits."""
    import unreal as u
    report = {"schema": "Rogue10m.NaturalWorldStructuralQA.20261117.v1", "passed": False,
              "started_utc": datetime.now(timezone.utc).isoformat(), "checks": {}, "diagnostics": [],
              "assets_saved": False, "editor_mutated": False, "full_gameplay_accepted": False,
              "boundary": "Planned CPU grid, original layer, actor/ISM settings and quest payloads only"}
    checks = report["checks"]

    def check(name, condition, detail=None):
        checks[name] = bool(condition)
        if not condition:
            report["diagnostics"].append({"check": name, "detail": detail})

    def dirty():
        lib = u.EditorLoadingAndSavingUtils
        return sorted({p.get_path_name() for p in list(lib.get_dirty_map_packages())+list(lib.get_dirty_content_packages())})

    try:
        editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
        world = editor.get_editor_world()
        if Path(u.Paths.project_dir()).resolve() != ROOT.resolve() or package(world) != TARGET or u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError("Exact loaded idle Crop candidate required")
        dirty_before = dirty()
        plan, selection = selected_author_plan()
        height = read(OUT/"target-height-plan.json")
        report["author_plan_selection"] = selection
        old = read(OUT/"baseline-live/snapshot.json")
        old_dependencies = read(OUT/"baseline-live/dependencies.json")
        baseline = read(OUT/"baseline-live/actors.json")
        before = read(native_before or OUT/"author-live/native-before.json")
        applied = read(apply_report or OUT/"author-live/apply-height.json")
        authored = read(author_report or OUT/"author-live/author-content.json")
        report["author_report_schema"] = authored.get("schema")
        repair = read(material_repair_report or OUT/"author-live/ism-material-repair.json")
        repaired = repair.get("materials", [])
        check("owned_ISM_material_repair_receipt", repair.get("schema") == "Rogue10m.NaturalDressingMaterialDerivatives20261117.v1"
              and repair.get("success") is True and len(repaired) == 5
              and {r.get("source_package") for r in repaired} == set(MATERIAL_DERIVATIVES)
              and {r.get("destination_package") for r in repaired} == set(MATERIAL_DERIVATIVES.values()))
        material_checks = []
        for source_package, destination in MATERIAL_DERIVATIVES.items():
            receipts = [r for r in repaired if r.get("source_package") == source_package]
            source_file = ROOT/"Content"/(source_package[6:]+".uasset")
            source_material, clone_material = u.load_asset(source_package), u.load_asset(destination)
            valid = len(receipts) == 1
            source_sha = hashlib.sha256(source_file.read_bytes()).hexdigest()
            row = receipts[0] if receipts else {}
            valid &= (row.get("destination_package") == destination and row.get("source_sha256_before") == row.get("source_sha256_after")
                      and source_sha.lower() == str(row.get("source_sha256_before", "")).lower()
                      and row.get("source_dirty_after_reload") is False and source_package not in dirty()
                      and isinstance(source_material, u.Material) and isinstance(clone_material, u.Material)
                      and path(source_material.get_class()) == path(clone_material.get_class()) == "/Script/Engine.Material"
                      and bool(clone_material.get_editor_property("used_with_instanced_static_meshes")) is True
                      and bool(source_material.get_editor_property("used_with_instanced_static_meshes")) == row.get("source_used_with_instanced_static_meshes_after_reload")
                      and isinstance(row.get("source_graph_before"), dict) and bool(row["source_graph_before"])
                      and row.get("source_graph_before") == row.get("source_graph_after_reload") == row.get("clone_graph_after")
                      and row.get("clone_compile_errors") == [])
            material_checks.append({"source": source_package, "destination": destination, "source_sha256": source_sha,
                                    "source_dirty": source_package in dirty(), "passed": bool(valid)})
        report["owned_ISM_material_derivatives"] = material_checks
        check("exact5_owned_ISM_materials_and_source_preservation", all(r["passed"] for r in material_checks))
        reload = repair.get("reload", {})
        check("original5_material_packages_reloaded", set(reload.get("packages", [])) == set(MATERIAL_DERIVATIVES)
              and reload.get("any_packages_reloaded") is True and reload.get("error_message") in ("", None))
        raw_plan = planned_checks(height, plan)
        report["planned_grid"] = raw_plan
        check("independent_raw_plan_checks", raw_plan["passed"])
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(True))
        report["native"] = native
        values = native.get("composite_heights_uint16", [])
        check("native_exact45_registered_and_settled", native.get("success") is True and native.get("component_count") == 45
              and native.get("collision_count") == 45 and native.get("layer_count") == 2 and native.get("landscape_reports_up_to_date") is True)
        check("native_read_only", native.get("read_only") is True and native.get("dirty_package_names_unchanged") is True)
        check("full_target_grid_exact", (native.get("width"), native.get("height")) == (568, 316) and values == height["target_heights"])
        numeric_grid = len(values) == 179488 and all(isinstance(v, (int, float)) and not isinstance(v, bool)
                       and math.isfinite(v) and v == int(v) and 0 <= v <= 65535 for v in values)
        if numeric_grid:
            report["actual_height_uint16le_sha256"] = grid_sha256([int(v) for v in values])
            check("independent_target_SHA256", report["actual_height_uint16le_sha256"] == raw_plan.get("height_uint16le_sha256"))
        else:
            check("independent_target_SHA256", False, "Observed grid is not a full uint16 lattice")
        check("original_layer_native_hash_preserved", before.get("success") is True and before.get("layer_count") == 1
              and native.get("original_layer_blake3") == before.get("original_layer_blake3")
              and native.get("source_layer_guid") == height["source_layer_guid"])
        check("height_mutation_exact_apply_target", applied.get("success") is True and applied.get("original_layer_preserved") is True
              and applied.get("target_composite_blake3") == native.get("composite_blake3")
              and native.get("composite_blake3") != before.get("composite_blake3"))
        crop = u.Rogue10mCrop20261114WorldAuthoring
        terrain = json.loads(crop.inspect_candidate_terrain())
        snapshot = json.loads(crop.snapshot_retained_terrain_data())
        dependencies = json.loads(crop.inspect_candidate_dependencies())
        report.update(terrain=terrain, snapshot=snapshot, dependencies=dependencies)
        check("crop_exact_lattice", terrain.get("success") is True and terrain.get("retained_9_by_5_lattice_observed") is True
              and terrain.get("all_components_registered") is True and all(terrain.get(k) == 45 for k in
              ("component_count", "collision_component_count", "unique_component_count", "unique_collision_component_count")))
        check("terrain_identity_and_layout", all(snapshot.get(k) == old[k] for k in ("landscape_guid", "landscape_actor_path",
              "actor_origin_cm", "actor_scale", "actor_rotation_pitch_yaw_roll", "material_path", "layout", "components", "paint_layers")))
        guid = height["source_layer_guid"]
        original_raw = [r for r in snapshot.get("raw_edit_layers", []) if r["edit_layer_guid"] == guid]
        original_ba = [r for r in dependencies.get("raw_edit_layer_ba", []) if r["edit_layer_guid"] == guid]
        check("original_raw_RG_paint_preserved", len(original_raw) == 1 and strip_linked(original_raw[0]) == strip_linked(old["raw_edit_layers"][0]))
        check("original_raw_BA_preserved", len(original_ba) == 1 and strip_linked(original_ba[0]) == strip_linked(old_dependencies["raw_edit_layer_ba"][0]))
        check("composite_paint_preserved", strip_linked(snapshot.get("composite", {}).get("paint_weights")) == strip_linked(old["composite"]["paint_weights"]))
        check("new_snapshot_complete", snapshot.get("success") is True and snapshot.get("all_requested_data_complete") is True
              and snapshot.get("composite_settled_after_read") is True and len(snapshot.get("raw_edit_layers", [])) == 2)
        check("strict_loaded_reference_walk", dependencies.get("success") is True and dependencies.get("loaded_reflection_walk_complete") is True
              and dependencies.get("reference_scan_truncated") is False and all(dependencies.get(k) is True for k in READ_ONLY))
        check("dependency_limits_retained", dependencies.get("project_wide_dependency_complete") is False and dependencies.get("crop_reference_safety_verified") is False)

        actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
        actual = {path(a): a for a in actors}
        check("unique_actor_paths", len(actual) == len(actors))
        old_by_path = {r["path"]: r for r in baseline}
        moves = {r["path"]: r for r in plan["moves"]}
        roads = {r["path"]: r for r in plan["road_decals"]}
        check("all_original_actors_preserved", set(old_by_path) <= set(actual))
        failures, group_deltas, group_xy_deltas = [], {}, {}
        for actor_path, old_actor in old_by_path.items():
            actor = actual.get(actor_path)
            if actor is None:
                continue
            r = actor.get_actor_rotation()
            common = (actor.get_actor_label() == old_actor["label"] and path(actor.get_class()) == old_actor["class"]
                      and list(map(str, actor.tags)) == old_actor["tags"] and str(actor.get_folder_path()) == old_actor["folder"]
                      and close(xyz(actor.get_actor_scale3d()), old_actor["scale"], .0001)
                      and close(basis([r.pitch, r.yaw, r.roll]), basis(old_actor["rotation"]), .0001))
            expected = list(old_actor["location"])
            if actor_path in moves:
                move = moves[actor_path]
                expected = move["after_location"]
                offset = plan.get("building_xy_offsets_cm", {}).get(move["group"], [0, 0])
                expected_xy = [old_actor["location"][i]+offset[i] for i in range(2)]
                common &= close(move["before_location"], old_actor["location"], .0001) and close(expected[:2], expected_xy, .0001)
                common &= abs(expected[2]-old_actor["location"][2]-(move["new_ground_reference"]-move["old_ground_reference"])) <= .0001
                if move["group"]:
                    group_deltas.setdefault(move["group"], []).append(actor.get_actor_location().z-old_actor["location"][2])
                    group_xy_deltas.setdefault(move["group"], []).append(
                        [xyz(actor.get_actor_location())[i]-old_actor["location"][i] for i in range(2)])
            elif actor_path in roads:
                expected[2] = 9600.0
            common &= close(xyz(actor.get_actor_location()), expected)
            if "mesh" in old_actor:
                component = actor.get_component_by_class(u.StaticMeshComponent)
                common &= bool(component) and path(component.get_editor_property("static_mesh")) == old_actor["mesh"]
                if component:
                    common &= ([path(component.get_material(i)) for i in range(component.get_num_materials())] == old_actor["materials"]
                        and bool(component.get_editor_property("visible")) == old_actor["visible"]
                        and bool(actor.get_editor_property("hidden")) == old_actor["hidden"]
                        and str(component.get_collision_profile_name()) == old_actor["collision"])
            if not common:
                failures.append(old_actor["label"])
        check("original_actor_exact_allowed_changes", not failures, failures[:30])
        check("twenty_building_groups_rigid_vertical", len(group_deltas) == 20 and all(max(v)-min(v) <= .1 for v in group_deltas.values()))
        report["building_vertical_deltas"] = {k: [min(v), max(v)] for k, v in group_deltas.items()}
        horizontal = plan.get("building_xy_offsets_cm", {})
        check("twenty_building_groups_exact_declared_horizontal", len(group_xy_deltas) == 20
              and all(all(close(value, horizontal.get(group, [0, 0])) for value in values)
                      for group, values in group_xy_deltas.items()))
        report["building_horizontal_deltas"] = {group: {"x_min_max": [min(v[0] for v in values), max(v[0] for v in values)],
            "y_min_max": [min(v[1] for v in values), max(v[1] for v in values)]} for group, values in group_xy_deltas.items()}
        layout = read(ROOT/"Docs/References/WorldMap/world-layout-20261112.json")
        nodes = {n["id"]: n["positionM"] for n in layout["nodes"]}
        lengths = {e["id"]: 100*math.hypot(nodes[e["from"]]["x"]-nodes[e["to"]]["x"], nodes[e["from"]]["y"]-nodes[e["to"]]["y"]) for e in layout["edges"]}
        road_failures = []
        for actor_path, row in roads.items():
            actor = actual.get(actor_path)
            component = actor.get_component_by_class(u.DecalComponent) if actor else None
            edge = row["label"].rsplit("_", 1)[1]
            if not component or package(component.get_decal_material()) != ROAD_MATERIAL or not close(xyz(component.get_editor_property("decal_size")), [500, 400, lengths[edge]/2], .02) or component.get_editor_property("fade_screen_size") != 0:
                road_failures.append(row["label"])
        check("roads29_flat_8m_new_material", len(roads) == 29 and not road_failures, road_failures)

        dressing = plan["dressing"]
        groups = dressing["groups"]
        wanted_labels = {g["actor_label"] for g in groups}
        new_actors = [a for p, a in actual.items() if p not in old_by_path]
        check("new_actors_exact10_dressing", len(new_actors) == 10 and {a.get_actor_label() for a in new_actors} == wanted_labels)
        by_label = {a.get_actor_label(): a for a in new_actors}
        instances, component_rows, instance_failures = {}, [], []
        editor_visualizers = []
        for group in groups:
            actor = by_label.get(group["actor_label"])
            if not actor:
                instance_failures.append(group["id"]+":missing actor")
                continue
            hidden_actor = group["id"] == "ClosedBoundaryBackup"
            actor_rotation = actor.get_actor_rotation()
            check("group_identity_"+group["id"], path(actor.get_class()) == "/Script/Engine.Actor"
                  and set(map(str, actor.tags)) == {BASE_TAG, SCOPE_TAG, *group["tags"]}
                  and str(actor.get_folder_path()) == group["folder"] and close(xyz(actor.get_actor_location()), [0, 0, 0])
                  and close(xyz(actor.get_actor_scale3d()), [1, 1, 1], .0001)
                  and close(basis([actor_rotation.pitch, actor_rotation.yaw, actor_rotation.roll]), basis([0, 0, 0]), .0001)
                  and bool(actor.get_editor_property("is_spatially_loaded")) is not hidden_actor
                  and bool(actor.get_editor_property("hidden")) is hidden_actor)
            components = list(actor.get_components_by_class(u.InstancedStaticMeshComponent))
            # ActorFactoryEmptyActor's native DefaultSceneRoot can create one
            # transient editor sprite (SceneComponent.cpp::CreateSpriteComponent).
            # It is not gameplay geometry; every other extra primitive fails.
            extras = [c for c in actor.get_components_by_class(u.PrimitiveComponent) if c not in components]
            sprite_rows = [{"path": path(c), "class": path(c.get_class()),
                            "editor_only": bool(c.get_editor_property("is_editor_only")),
                            "hidden_in_game": bool(c.get_editor_property("hidden_in_game")),
                            "collision_profile": str(c.get_collision_profile_name()),
                            "collision_enabled_none": c.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION}
                           for c in extras]
            editor_visualizers.append({"group": group["id"], "components": sprite_rows})
            allowed_sprites = len(sprite_rows) <= 1 and all(r["class"] == "/Script/Engine.BillboardComponent"
                and r["editor_only"] and r["hidden_in_game"] and r["collision_profile"] == "NoCollision"
                and r["collision_enabled_none"] for r in sprite_rows)
            bins = OrderedDict()
            for row in group["instances"]:
                bins.setdefault((row["mesh"], row["material"], row["collision_role"], row["hidden_in_game"]), []).append(row)
            check("group_components_"+group["id"], len(components) == len(bins) and allowed_sprites,
                  {"ism_count": len(components), "planned_bins": len(bins), "extra_primitives": sprite_rows})
            for index, ((mesh, material, role, hidden), rows) in enumerate(bins.items()):
                selected = [c for c in components if "NW17.Bin."+str(index).zfill(2) in map(str, c.get_editor_property("component_tags"))]
                if len(selected) != 1:
                    instance_failures.append(group["id"]+":missing/duplicate bin "+str(index))
                    continue
                c = selected[0]
                mesh_object = c.get_editor_property("static_mesh")
                box = mesh_object.get_bounding_box() if mesh_object else None
                valid = (material in MATERIAL_DERIVATIVES and package(c.get_editor_property("static_mesh")) == mesh
                         and package(c.get_material(0)) == MATERIAL_DERIVATIVES[material]
                         and c.get_instance_count() == len(rows) and bool(c.get_editor_property("visible")) is not hidden
                         and bool(c.get_editor_property("hidden_in_game")) is hidden
                         and set(map(str, c.get_editor_property("component_tags"))) == {DRESS_TAG, SCOPE_TAG, "NW17.Role."+role, "NW17.Bin."+str(index).zfill(2)})
                valid &= box is not None and close(xyz(box.min), [-50, -50, -50], .01) and close(xyz(box.max), [50, 50, 50], .01)
                visual = role == "visual_only"
                valid &= c.get_collision_enabled() == (u.CollisionEnabled.NO_COLLISION if visual else u.CollisionEnabled.QUERY_AND_PHYSICS)
                valid &= str(c.get_collision_profile_name()) == ("NoCollision" if visual else "BlockAll" if role == "village_enclosure_pawn_block" else "Custom")
                if not visual:
                    valid &= c.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN) == u.CollisionResponseType.ECR_BLOCK
                responses = {}
                if role == "boundary_pawn_only_block":
                    for name in dir(u.CollisionChannel):
                        channel = getattr(u.CollisionChannel, name)
                        if not name.startswith("ECC_") or not isinstance(channel, u.CollisionChannel) or not 0 <= channel.value < 64:
                            continue
                        response = c.get_collision_response_to_channel(channel)
                        responses[name] = str(response)
                        valid &= response == (u.CollisionResponseType.ECR_BLOCK if channel == u.CollisionChannel.ECC_PAWN else u.CollisionResponseType.ECR_IGNORE)
                component_rows.append({"group": group["id"], "component": path(c), "count": c.get_instance_count(), "public_channel_responses": responses})
                for i, row in enumerate(rows):
                    tr = c.get_instance_transform(i, world_space=True)
                    if isinstance(tr, tuple):
                        tr = next((v for v in tr if isinstance(v, u.Transform)), None)
                    ok = (isinstance(tr, u.Transform) and close(xyz(tr.translation), row["location_cm"])
                          and close(xyz(tr.scale3d), row["scale_for_verified_100cm_mesh"], .0001)
                          and close(quat_basis(tr.rotation), basis(row["rotation_pitch_yaw_roll"]), .0002))
                    if not ok:
                        instance_failures.append(row["label"])
                    else:
                        instances[row["label"]] = {"center": xyz(tr.translation), "basis": quat_basis(tr.rotation), "dimensions": row["dimensions_cm"]}
                if not valid:
                    instance_failures.append(group["id"]+":bin settings "+str(index))
        report["dressing_components"] = component_rows
        report["native_editor_visualizers"] = editor_visualizers
        check("exact708_instance_transforms_and_settings", len(instances) == 708 and not instance_failures, instance_failures[:30])
        gate_rows = []
        for enclosure in dressing["enclosures"]:
            for gate in enclosure["gates"]:
                labels = [enclosure["hub"]+"_Gate_"+gate["edge_id"]+suffix for suffix in ("_L", "_R")]
                normal = (-math.sin(math.radians(gate["road_yaw_degrees"])), math.cos(math.radians(gate["road_yaw_degrees"])))
                intervals = []
                for label in labels:
                    row = instances.get(label)
                    if not row:
                        continue
                    ex = sum(abs(row["basis"][axis*3]*normal[0]+row["basis"][axis*3+1]*normal[1])*row["dimensions"][axis]/2 for axis in range(3))
                    offset = (row["center"][0]-gate["center_cm"][0])*normal[0]+(row["center"][1]-gate["center_cm"][1])*normal[1]
                    intervals.append((offset-ex, offset+ex))
                intervals.sort()
                width = intervals[1][0]-intervals[0][1] if len(intervals) == 2 else None
                gate_rows.append({"hub": enclosure["hub"], "edge": gate["edge_id"], "observed_inner_width_cm": width})
        report["gate_bounds"] = gate_rows
        check("thirteen_actual_gate_inner_bounds_14m", len(gate_rows) == 13 and all(r["observed_inner_width_cm"] is not None and abs(r["observed_inner_width_cm"]-1400) <= .2 for r in gate_rows))

        quest_rows = []
        for row in read(OUT/"baseline-live/directors.json"):
            director = actual.get(row["actor"])
            clone = director.get_editor_property("quest_definition") if director else None
            source = u.load_asset(row["definition"])
            original_file = ROOT/row["source_file"]
            good = (clone is not None and package(clone) == QUEST and path(clone.get_class()) == path(source.get_class())
                    and hashlib.sha256(original_file.read_bytes()).hexdigest() == row["source_sha256"])
            if clone:
                origin = xyz(clone.get_editor_property("layout_origin_cm"))
                good &= close(origin, [row["origin"][0], row["origin"][1], 9600], .0001)
                good &= clone.get_editor_property("supported_world") == source.get_editor_property("supported_world")
                a, b = clone.get_editor_property("layout_rotation"), source.get_editor_property("layout_rotation")
                good &= close([a.pitch, a.yaw, a.roll], [b.pitch, b.yaw, b.roll], .0001)
                clone_quests = [q.export_text() for q in clone.get_editor_property("quests")]
                source_quests = [q.export_text() for q in source.get_editor_property("quests")]
                good &= clone_quests == source_quests and clone.get_editor_property("optional_story_text").export_text() == source.get_editor_property("optional_story_text").export_text()
                quest_rows.append({"director": row["actor"], "clone": path(clone), "source": path(source), "origin": origin,
                    "quest_row_count": len(clone_quests), "complete_row_export_sha256": hashlib.sha256(json.dumps(clone_quests, ensure_ascii=False).encode()).hexdigest(), "passed": bool(good)})
            else:
                quest_rows.append({"director": row["actor"], "passed": False})
        report["quests"] = quest_rows
        check("quest_clone_only_originZ_and_binding", len(quest_rows) == 1 and all(r["passed"] for r in quest_rows))
        check("read_only_dirty_set", dirty() == dirty_before)
        if require_saved:
            check("saved_clean_state", not dirty())
        report.update(passed=all(checks.values()), status="structural_pass" if all(checks.values()) else "structural_fail",
                      dirty_after=dirty(), finished_utc=datetime.now(timezone.utc).isoformat(),
                      limitations=["No new rendered/GPU, finite-capsule collision, physical input, NavMesh or complete gameplay certification.",
                                   "Collision responses cover public enum entries only; hidden native channels are not independently enumerated.",
                                   "Original metadata-linked digests are excluded where new layer metadata intentionally changes; raw semantic/B/A digests and native original-layer hash are compared."])
        return report
    except Exception:
        report.update(status="validator_exception_preserve_diagnostics", traceback=traceback.format_exc(),
                      dirty_after=dirty(), finished_utc=datetime.now(timezone.utc).isoformat())
        return report
