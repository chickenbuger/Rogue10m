"""Inert, read-only static Pawn-capsule QA for the reviewed natural candidate.

validate_capsules() returns a JSON-compatible report and writes nothing. It
does not load assets, move/spawn actors, save, compile, start PIE, draw or retry.
Jump-height cases are ballistic reference poses, not actual jump execution.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import traceback

ROOT = Path("D:/Project/Rogue10m")
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
BASE = ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117"
BP_CLASS = "/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C"


def _require(value, message):
    if not value:
        raise RuntimeError(message)


def _inside(p, polygon):
    answer = False
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        if (a[1] > p[1]) != (b[1] > p[1]) and p[0] < (b[0]-a[0])*(p[1]-a[1])/(b[1]-a[1])+a[0]:
            answer = not answer
    return answer


def _unit(x, y):
    length = math.hypot(x, y)
    _require(length > 0.0001, "Degenerate probe direction")
    return x / length, y / length


def build_cases(dressing, layout, *, jump_peaks_cm=(90.0, 180.0)):
    """Pure case builder; no IO or Unreal calls. Capsule endpoints are centers."""
    polygon = [tuple(map(float, p)) for p in dressing["boundary_world_cm"]]
    gates = [g for hub in dressing["enclosures"] for g in hub["gates"]]
    _require(len(polygon) == 13 and len(gates) == 13, "Exact13 boundary segments and13 gates required")
    _require(len(jump_peaks_cm) == 2 and all(math.isfinite(h) and 0 < h < 3000 for h in jump_peaks_cm), "Finite reference jump peaks required")
    levels = [("ground", 0.0), ("reference_jump1_peak", jump_peaks_cm[0]), ("reference_jump2_peak", jump_peaks_cm[1])]
    normals = []
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        n = _unit(-(b[1]-a[1]), b[0]-a[0])
        mid = ((a[0]+b[0])/2, (a[1]+b[1])/2)
        if _inside((mid[0]+n[0]*100, mid[1]+n[1]*100), polygon):
            n = (-n[0], -n[1])
        normals.append(n)
    cases = []

    def add(label, kind, center, direction, reach, bottom, radius, blocked, *, wall_only=False, lateral=0.0):
        px, py = center
        nx, ny = direction
        px, py = px-ny*lateral, py+nx*lateral
        z = bottom + 96.0
        cases.append({"id": label, "kind": kind, "radius_cm": radius, "half_height_cm": 96.0,
                      "capsule_bottom_cm": bottom, "start_cm": [px-nx*reach, py-ny*reach, z],
                      "end_cm": [px+nx*reach, py+ny*reach, z], "expected_blocking": blocked,
                      "requires_boundary_backup_actor": wall_only})

    for radius in (34.0, 42.0):
        for index, (a, b) in enumerate(zip(polygon, polygon[1:] + polygon[:1])):
            mid = ((a[0]+b[0])/2, (a[1]+b[1])/2)
            corner = polygon[index]
            outward = _unit(normals[index-1][0]+normals[index][0], normals[index-1][1]+normals[index][1])
            _require(_inside((corner[0]-outward[0]*4000, corner[1]-outward[1]*4000), polygon)
                     and not _inside((corner[0]+outward[0]*4000, corner[1]+outward[1]*4000), polygon), "Corner does not cross the boundary")
            for kind, center, normal in (("boundary_segment", mid, normals[index]), ("boundary_corner", corner, outward)):
                for level, offset in levels:
                    add(f"{kind}_{index:02d}_r{radius:g}_{level}", kind, center, normal, 4000, 9604+offset, radius, True)
                # Above the maximum authored ridge (12900cm) but inside the
                # native backup-wall vertical span [9000,15000].
                add(f"{kind}_{index:02d}_r{radius:g}_wall_isolation", kind, center, normal, 4000, 14000, radius, True, wall_only=True)
        for gate in gates:
            angle = math.radians(gate["road_yaw_degrees"])
            direction = (math.cos(angle), math.sin(angle))
            margin = gate["clear_width_cm"]/2-radius-2
            _require(margin > 0, "Gate is smaller than the test capsule")
            for side, lateral in (("center", 0), ("left", -margin), ("right", margin)):
                for level, offset in levels:
                    add(f"gate_{gate['hub']}_{gate['edge_id']}_r{radius:g}_{side}_{level}", "village_gate",
                        gate["center_cm"][:2], direction, 1200, 9604+offset, radius, False, lateral=lateral)

    nodes = {row["id"]: (15000+row["positionM"]["x"]*100, 32550+row["positionM"]["y"]*100) for row in layout["nodes"]}
    points = [("node_"+key, point) for key, point in sorted(nodes.items())]
    for edge in layout["edges"]:
        a, b = nodes[edge["from"]], nodes[edge["to"]]
        points.append(("road_mid_"+edge["id"], ((a[0]+b[0])/2, (a[1]+b[1])/2)))
    points.append(("final_arena_center", (15000.0, 68000.0)))
    for label, (x, y) in points:
        for radius in (34.0, 42.0):
            cases.append({"id": f"ground_{label}_r{radius:g}", "kind": "ground_support", "radius_cm": radius,
                "half_height_cm": 96.0, "capsule_bottom_cm": 9604.0,
                "start_cm": [x, y, 9700.0], "end_cm": [x, y, 9690.0], "expected_blocking": True,
                "requires_boundary_backup_actor": False})
    _require(len(cases) == 544 and len({case["id"] for case in cases}) == 544, "Unexpected/duplicate544 probe cases")
    for case in cases:
        _require(all(math.isfinite(v) for p in (case["start_cm"], case["end_cm"]) for v in p), "Nonfinite probe")
        _require(1 <= math.dist(case["start_cm"], case["end_cm"]) <= 10000, "Native sweep-length bound exceeded")
    return cases


def validate_capsules():
    """Read the exact current scene once per case; return failures without retry."""
    report = {"passed": False, "checks": {}, "results": [], "failures": [], "assets_saved": False,
              "physical_input": False, "actual_jump_executed": False, "gameplay_certified": False,
              "scope": "Static simple ECC_Pawn capsule sweeps at two radii/reference jump heights; finite sample coverage only."}
    try:
        import unreal as u
        editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
        world = editor.get_editor_world()
        _require(Path(u.Paths.project_dir()).resolve() == ROOT and world and world.get_path_name().split(".")[0] == TARGET
                 and not editor.get_game_world(), "Exact idle candidate editor required")
        native = u.Rogue10mNaturalBoundary20261117Authoring
        status = json.loads(native.inspect_candidate())
        _require(status.get("success") and status.get("landscape_reports_up_to_date") and status.get("layer_count") == 2, "Settled native two-layer candidate required")

        def dirty():
            return sorted({p.get_path_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
                           + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})

        def character_defaults(cls):
            cdo = u.get_default_object(cls)
            capsule = cdo.get_editor_property("capsule_component")
            movement = cdo.get_movement_component()
            return {"class": cdo.get_class().get_path_name(), "radius_cm": float(capsule.get_scaled_capsule_radius()),
                    "half_height_cm": float(capsule.get_scaled_capsule_half_height()),
                    "jump_z_velocity_cm_s": float(movement.get_editor_property("jump_z_velocity")),
                    "gravity_scale": float(movement.get_editor_property("gravity_scale")),
                    "jump_max_hold_time_s": float(cdo.get_editor_property("jump_max_hold_time"))}

        before = dirty()
        report['dirty_packages_before'] = before
        report['dirty_package_list_sha256'] = hashlib.sha256(json.dumps(before,separators=(',',':')).encode('utf-8')).hexdigest()
        defaults = character_defaults(u.Rogue10mCharacter)
        blueprint = u.find_object(None, BP_CLASS)
        bp = character_defaults(blueprint) if blueprint else None
        report["character_defaults"] = {"native": defaults, "already_loaded_blueprint": bp,
            "blueprint_asset_loaded_by_this_helper": False, "runtime_profile_jump_count_and_world_gravity_verification": "pending PIE"}
        _require(abs(defaults["radius_cm"]-34) < .01 and abs(defaults["half_height_cm"]-96) < .01, "Native capsule is no longer34x96")
        sample = bp or defaults
        _require(abs(sample["half_height_cm"]-96) < .01 and any(abs(sample["radius_cm"]-r) < .01 for r in (34,42)), "Loaded character geometry is outside the two tested shapes")
        _require(sample["jump_max_hold_time_s"] == 0 and math.isfinite(sample["gravity_scale"]) and sample["gravity_scale"] > 0, "Ballistic reference requires zero jump hold and positive gravity scale")
        one = sample["jump_z_velocity_cm_s"]**2/(2*980*sample["gravity_scale"])
        report["reference_jump_peaks_cm"] = [one, 2*one]
        report["reference_gravity_cm_s2"] = 980*sample["gravity_scale"]
        report["gravity_is_engine_default_assumption_not_runtime_measurement"] = True
        plan = json.loads((BASE / "author-plan.json").read_text(encoding="utf-8-sig"))
        layout = json.loads((ROOT / "Docs/References/WorldMap/world-layout-20261112.json").read_text(encoding="utf-8-sig"))
        cases = build_cases(plan["dressing"], layout, jump_peaks_cm=(one, 2*one))
        actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
        walls = [a for a in actors if a.get_actor_label(False) == "NW17_ClosedBoundaryBackup"]
        _require(len(walls) == 1, "One exact native backup-boundary actor required")
        boundary_path = walls[0].get_path_name()
        for case in cases:
            result = json.loads(native.probe_pawn_capsule(u.Vector(*case["start_cm"]), u.Vector(*case["end_cm"]), case["radius_cm"], case["half_height_cm"]))
            native_dirty=result.pop('dirty_packages',None)
            result['dirty_package_count'] = len(native_dirty) if isinstance(native_dirty,list) else None
            result['dirty_package_list_sha256'] = report['dirty_package_list_sha256']
            row = {"case": case, "query": result, "passed": False}
            report["results"].append(row)
            # FString::Sort is case-insensitive; Python's sorted(str) is not.
            # Order is not package identity. Require the exact unique names.
            _require(isinstance(native_dirty,list) and len(native_dirty)==len(before) and set(native_dirty)==set(before),
                     'Native probe dirty package identities differ from the recorded exact baseline')
            _require(result.get("success") and result.get("read_only") and result.get("dirty_package_names_unchanged"), "Native query failed; remaining cases are not run: " + case["id"])
            hit = result.get("hit") or {}
            passed = result.get("blocking_hit") is case["expected_blocking"]
            if case["requires_boundary_backup_actor"]:
                passed = passed and hit.get("actor") == boundary_path and not hit.get("start_penetrating", True)
            if case["kind"] == "ground_support":
                impact = hit.get("impact_point_cm", [None, None, None])
                impact_z = impact[2] if len(impact) == 3 else None
                passed = (passed and not hit.get("start_penetrating", True) and hit.get("impact_normal", [0,0,0])[2] >= .71
                    and isinstance(impact_z, (int, float)) and not isinstance(impact_z, bool)
                    and math.isfinite(impact_z) and 9599.0 <= impact_z <= 9603.0)
                row["ground_impact_z_check"] = {"expected_range_cm": [9599.0, 9603.0], "actual_cm": impact_z}
            row["passed"] = bool(passed)
            if not passed:
                report["failures"].append(case["id"])
        report["checks"] = {"all544_queries_completed": len(report["results"]) == 544,
            "all_exterior_segments_corners_and_gate_cases_passed": not report["failures"],
            "all52_high_wall_queries_hit_exact_backup_actor": sum(r["passed"] for r in report["results"] if r["case"]["requires_boundary_backup_actor"]) == 52,
            "dirty_package_names_unchanged": before == dirty()}
        report["passed"] = all(report["checks"].values())
    except Exception:
        report["traceback"] = traceback.format_exc()
        report["failures"].append("validation_stopped_without_retry")
    return report
