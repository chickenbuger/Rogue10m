"""Read-only collision queries for the saved, reloaded native Landscape crop.

Import is inert. The editor owner calls validate(reload_confirmed=True) after
loading the full candidate region. No level load, save, asset edit, actor spawn,
PIE, Game, profile, collision rebuild or navigation operation is performed.
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


ROOT = Path(__file__).resolve().parents[2]
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
REPORT = ROOT / "Feature/doc/evidence/landscape-production-20261116/terrain-collision-check.json"
MAP_FILE = ROOT / "Content/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A.umap"
LEGACY_FILE = ROOT / "Scripts/Editor/ValidateWorldLayout20261112.py"
SLOPE_TARGET_DEGREES = 12.0
LEGACY_SLOPE_DEGREES = 13.0
RADIUS_CM, HALF_HEIGHT_CM, CENTER_CLEARANCE_CM = 42.0, 96.0, 110.0
RAY_TOP_CM, RAY_BOTTOM_CM = 50000.0, -20000.0
SUPPORT_LABEL = "WL26_PersistentBossGround"
BASE_TAG = "Rogue10m.WorldLayout20261112"


def _hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load_legacy():
    spec = importlib.util.spec_from_file_location("landscape_crop_legacy_query_helpers", LEGACY_FILE)
    if not spec or not spec.loader:
        raise RuntimeError("Legacy query helper is absent")
    module = importlib.util.module_from_spec(spec)
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    return module


def _surface(hit):
    point, normal = hit["impact_point_cm"], hit["impact_normal"]
    valid = (hit["blocking"] and not hit["initial_overlap"] and hit["is_landscape"]
             and point is not None and normal is not None
             and all(math.isfinite(v) for v in point + normal))
    if not valid:
        return None
    slope = math.degrees(math.acos(max(-1.0, min(1.0, normal[2]))))
    return {"height_cm": point[2], "normal": normal, "slope_degrees": slope,
            "source": "direct_landscape_ray", "observed_at_requested_xy": True}


def validate(*, reload_confirmed=False):
    """Write one exclusive report; query the exact clean editor world only.

    Height expectations come from actual physics observations. The historical
    procedural height discrepancy is diagnostic and never determines success.
    Four-neighbor seam support remains inferred; strict direct-observation and
    diagnostic support results are reported separately.
    """
    if reload_confirmed is not True:
        raise RuntimeError("Owner must save and independently reload the candidate first")
    if REPORT.exists():
        raise RuntimeError("Preserve the existing report; this invocation requires a fresh evidence path")
    import unreal

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor.get_editor_world()

    def dirty():
        return sorted({p.get_path_name() for p in
                       list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
                       + list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())})

    def guard():
        if Path(unreal.Paths.project_dir()).resolve() != ROOT.resolve():
            raise RuntimeError("Wrong project")
        if (not world or editor.get_editor_world() != world
                or world.get_path_name().split(".")[0] != TARGET
                or levels.is_in_play_in_editor()):
            raise RuntimeError("Exact Crop candidate with PIE stopped is required")
        if dirty():
            raise RuntimeError("Unsaved packages exist; owner must resolve them before read-only QA")

    guard()
    if not MAP_FILE.is_file():
        raise RuntimeError("Saved Crop candidate is absent")
    helper = getattr(unreal, "Rogue10mCrop20261114WorldAuthoring", None)
    if helper is None:
        raise RuntimeError("Exact-map native observer is unavailable; no fallback or guard change")
    inventory = json.loads(helper.inspect_candidate_terrain())
    expected_counts = ("component_count", "collision_component_count", "unique_component_count",
                       "unique_collision_component_count")
    if (inventory.get("success") is not True
            or inventory.get("retained_9_by_5_lattice_observed") is not True
            or not all(inventory.get(k) == 45 for k in expected_counts)
            or inventory.get("all_components_registered") is not True):
        raise RuntimeError("Native observer has not confirmed 45 registered render/collision pairs")
    guard()
    legacy = _load_legacy()
    plan = legacy.build_validation_plan()
    expected = {"edge_count": 29, "ground_sample_count": 2076,
                "centerline_substep_ground_sample_count": 6532, "bidirectional_sweep_count": 13006}
    if any(plan.get(k) != v for k, v in expected.items()):
        raise RuntimeError("Road sampling schema changed; review the plan before executing queries")
    terrain = legacy._load_terrain()
    edges = list(terrain.roadsegments())
    ignored = [a for a in actors.get_all_level_actors()
               if a.get_actor_label() == SUPPORT_LABEL and BASE_TAG in map(str, a.get_editor_property("tags"))]
    if len(ignored) != 1:
        raise RuntimeError("Expected one explicitly owned boss support; no broad terrain-ray ignore fallback")

    report = {"status": "started", "target": TARGET,
              "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "owner_confirmed_saved_reload": True, "native_inventory_before": inventory,
              "legacy_helper_sha256": _hash(LEGACY_FILE), "map_sha256_before": _hash(MAP_FILE),
              "historical_terrain_source_sha256": _hash(legacy.TERRAIN_MODULE),
              "layout_source_sha256": _hash(legacy.LAYOUT_JSON),
              "sampling_plan": {k: v for k, v in plan.items() if k != "target"},
              "assumptions": {"slope_target_degrees": SLOPE_TARGET_DEGREES,
                              "legacy_slope_degrees_diagnostic_only": LEGACY_SLOPE_DEGREES,
                              "capsule_radius_cm": RADIUS_CM, "capsule_half_height_cm": HALF_HEIGHT_CM,
                              "capsule_center_above_observed_ground_cm": CENTER_CLEARANCE_CM,
                              "capsule_bottom_clearance_cm": CENTER_CLEARANCE_CM - HALF_HEIGHT_CM,
                              "ray_top_cm": RAY_TOP_CM, "ray_bottom_cm": RAY_BOTTOM_CM},
              "terrain_ray_ignored_supports": [a.get_path_name() for a in ignored],
              "ground_samples": [], "substep_ground_samples": [], "sweeps": [],
              "seam_diagnostics": [], "road_results": [], "failures": [],
              "query_counts": {"primary_ground_rays": 0, "seam_neighbor_rays": 0, "capsule_sweeps": 0},
              "actual_walking": "not_run", "physical_input": "not_run", "pie_started": False,
              "game_started": False, "direct_60_second_playtest": "not_run",
              "growth_15_minute_playtest": "not_run", "ai_navigation_and_combat": "not_tested",
              "limitations": ["Editor traces do not simulate CharacterMovement, stepping, input, NavMesh or streaming play.",
                              "Only the centerline capsule is swept; 8m width is ground-sampled, not fully capsule-swept.",
                              "The 14cm bottom clearance can miss obstacles a grounded character would contact.",
                              "Seam support inferred from four neighbors is never a direct central ray observation.",
                              "Historical procedural height discrepancies are diagnostic, not preservation or pass criteria.",
                              "Map SHA and dirty-set checks do not certify every external asset byte or package preservation."]}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    with REPORT.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + "\n")

    def flush():
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    channel, draw = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, unreal.DrawDebugTrace.NONE

    def line(x, y, *, neighbor=False):
        report["query_counts"]["seam_neighbor_rays" if neighbor else "primary_ground_rays"] += 1
        return legacy._unpack_hit(unreal, unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, RAY_TOP_CM), unreal.Vector(x, y, RAY_BOTTOM_CM),
            channel, False, ignored, draw, True))

    def capsule(a, b):
        report["query_counts"]["capsule_sweeps"] += 1
        return legacy._unpack_hit(unreal, unreal.SystemLibrary.capsule_trace_single(
            world, unreal.Vector(*a), unreal.Vector(*b), RADIUS_CM, HALF_HEIGHT_CM,
            channel, False, [], draw, True))

    def ground(sample, edge_id, role):
        x, y = sample["xy_cm"]
        hit = line(x, y)
        surface = _surface(hit)
        direct = surface is not None
        diagnosis = None
        if not hit["blocking"] and not hit["initial_overlap"] and hit["actor_path"] is None:
            neighbors = []
            for ox, oy in ((1.0, 0.0), (-1.0, 0.0), (0.0, 1.0), (0.0, -1.0)):
                neighbor_hit = line(x + ox, y + oy, neighbor=True)
                neighbor_surface = _surface(neighbor_hit)
                neighbors.append({"offset_cm": [ox, oy], "hit": neighbor_hit,
                                  "surface": neighbor_surface,
                                  "passed": neighbor_surface is not None
                                  and neighbor_surface["slope_degrees"] <= LEGACY_SLOPE_DEGREES})
            heights = [n["surface"]["height_cm"] for n in neighbors if n["surface"] is not None]
            if len(heights) == 4:
                inferred = sum(heights) / 4
                # The legacy diagnostic requires an expected height. Use this
                # actual-neighbor mean, never the historical procedural field.
                result = legacy._diagnose_seam(hit, x, y, inferred, neighbors)
                diagnosis = result["seam_diagnostic"]
                diagnosis["height_reference"] = "actual_neighbor_mean_not_independent_height_validation"
                if result["passed"]:
                    surface = result["surface"]
                    surface["slope_degrees"] = math.degrees(math.acos(max(-1.0, min(1.0, surface["normal"][2]))))
            else:
                diagnosis = {"accepted": False, "central_hit_observed": False,
                             "diagnosis": "unresolved_ray_miss", "neighbor_observations": neighbors}
            report["seam_diagnostics"].append({"edge_id": edge_id, "role": role,
                                                "sample": sample["index"], **diagnosis})
        row = {**sample, "edge_id": edge_id, "sample_role": role, "hit": hit,
               "surface": surface, "direct_ground_observation": direct,
               "support_available": surface is not None,
               "passed_12_degree_support": surface is not None and surface["slope_degrees"] <= SLOPE_TARGET_DEGREES,
               "passed_legacy_13_degree_support": surface is not None and surface["slope_degrees"] <= LEGACY_SLOPE_DEGREES,
               "historical_procedural_height_error_cm": abs(surface["height_cm"] - sample["authored_height_cm"])
               if surface is not None else None}
        if diagnosis is not None:
            row["seam_diagnostic"] = diagnosis
        if not row["passed_12_degree_support"]:
            report["failures"].append({"kind": "ground", "edge_id": edge_id, "role": role,
                                        "index": sample["index"], "offset_cm": sample["offset_cm"]})
        return row

    try:
        nodes = terrain.nodes_world()
        start = ground({"index": 0, "xy_cm": [nodes["S"]["x"], nodes["S"]["y"]],
                        "offset_cm": 0, "authored_height_cm": nodes["S"]["z"]}, "S", "positive_control_ground")
        positive = None
        if start["surface"]:
            x, y = start["xy_cm"]
            z = start["surface"]["height_cm"]
            positive = capsule([x, y, z + 300], [x, y, z + 50])
        positive_passed = bool(positive and positive["blocking"] and not positive["initial_overlap"] and positive["is_landscape"])
        report["capsule_ground_positive_control"] = {"ground": start, "hit": positive, "passed": positive_passed}
        if not positive_passed:
            raise RuntimeError("Capsule positive control did not detect Landscape; do not interpret empty sweeps as clear")

        for edge in edges:
            guard()
            failure_start = len(report["failures"])
            samples, _ = legacy._edge_samples(edge, terrain)
            for sample in samples:
                report["ground_samples"].append(ground(sample, edge["id"], "5m_corridor"))
            substeps, intervals = legacy._centerline_substeps(edge, terrain)
            centers = [ground(sample, edge["id"], "50cm_centerline") for sample in substeps]
            report["substep_ground_samples"].extend(centers)
            for index in range(intervals):
                for reverse in (False, True):
                    a, b = (centers[index + 1], centers[index]) if reverse else (centers[index], centers[index + 1])
                    row = {"edge_id": edge["id"], "segment": index, "reverse": reverse}
                    if a["surface"] is None or b["surface"] is None:
                        row.update(passed=False, status="not_run_invalid_ground")
                    else:
                        source = a["xy_cm"] + [a["surface"]["height_cm"] + CENTER_CLEARANCE_CM]
                        destination = b["xy_cm"] + [b["surface"]["height_cm"] + CENTER_CLEARANCE_CM]
                        hit = capsule(source, destination)
                        row.update(passed=not hit["blocking"] and not hit["initial_overlap"],
                                   status="queried", start_cm=source, end_cm=destination, hit=hit)
                    report["sweeps"].append(row)
                    if not row["passed"]:
                        report["failures"].append({"kind": "capsule", **row})
            report["road_results"].append({"edge_id": edge["id"], "from": edge["from"], "to": edge["to"],
                                            "failure_count": len(report["failures"]) - failure_start})
            flush()

        guard()
        after = json.loads(helper.inspect_candidate_terrain())
        guard()
        report["native_inventory_after"] = after
        report["map_sha256_after"] = _hash(MAP_FILE)
        report["dirty_packages_after"] = dirty()
        samples = report["ground_samples"] + report["substep_ground_samples"]
        direct_only = all(s["direct_ground_observation"] for s in samples)
        checks = {"roads29_completed": len(report["road_results"]) == 29,
                  "all_ground_and_capsule_results_passed": not report["failures"],
                  "ground2076": len(report["ground_samples"]) == 2076,
                  "centerline6532": len(report["substep_ground_samples"]) == 6532,
                  "sweep_results13006": len(report["sweeps"]) == 13006,
                  "positive_control": positive_passed,
                  "all_ground_support_under12_degrees": all(s["passed_12_degree_support"] for s in samples),
                  "all_capsule_sweeps_queried_and_clear": all(s["passed"] and s["status"] == "queried" for s in report["sweeps"]),
                  "native_lattice45_stable": after.get("success") is True
                  and after.get("retained_9_by_5_lattice_observed") is True
                  and after.get("all_components_registered") is True
                  and all(after.get(k) == 45 for k in expected_counts),
                  "map_file_unchanged": report["map_sha256_after"] == report["map_sha256_before"],
                  "legacy_helper_unchanged": _hash(LEGACY_FILE) == report["legacy_helper_sha256"],
                  "historical_terrain_source_unchanged": _hash(legacy.TERRAIN_MODULE) == report["historical_terrain_source_sha256"],
                  "layout_source_unchanged": _hash(legacy.LAYOUT_JSON) == report["layout_source_sha256"],
                  "no_dirty_packages": not report["dirty_packages_after"]}
        report["checks"] = checks
        report["diagnostic_support_queries_passed"] = all(checks.values())
        report["all_ground_directly_observed_without_inference"] = direct_only
        report["ground_points_without_direct_observation"] = sum(not s["direct_ground_observation"] for s in samples)
        report["success"] = all(checks.values()) and direct_only
        report["status"] = ("passed_editor_queries_only" if report["success"] else
                            "diagnostic_support_passed_with_inferred_seams_strict_observation_incomplete"
                            if all(checks.values()) else "failed_editor_queries")
        report["failure_count"] = len(report["failures"]) + sum(not v for v in checks.values())
        report["accepted_seam_count"] = sum(bool(d["accepted"]) for d in report["seam_diagnostics"])
        report["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        flush()
        return report
    except Exception as error:
        report.update(status="blocked_or_failed_editor_queries", success=False,
                      error=str(error), traceback=traceback.format_exc(),
                      finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        flush()
        raise
