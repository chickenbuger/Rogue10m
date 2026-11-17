"""Inert, read-only diagnostics of two preserved Crop collision ray misses.

The editor owner calls diagnose(). This never repairs terrain, changes physics,
saves assets, starts PIE/Game, or reclassifies the original failed QA report.
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
EVIDENCE = ROOT / "Feature/doc/evidence/landscape-production-20261116"
ORIGINAL = EVIDENCE / "terrain-collision-check.json"
REPORT = EVIDENCE / "ray-miss-diagnostic.json"
MAP_FILE = ROOT / "Content/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A.umap"
OFFSETS_CM = (0.0, -0.001, 0.001, -0.01, 0.01, -0.1, 0.1, -1.0, 1.0, -10.0, 10.0)
RADII_CM = (1.0, 5.0, 42.0)


def _hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _legacy():
    path = ROOT / "Scripts/Editor/ValidateWorldLayout20261112.py"
    spec = importlib.util.spec_from_file_location("crop_ray_miss_query_helpers", path)
    if not spec or not spec.loader:
        raise RuntimeError("Existing trace packing helper is absent")
    module = importlib.util.module_from_spec(spec)
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    return module


def _points(original):
    """Use exact recorded doubles and actual observed neighbor heights only."""
    rows = original["ground_samples"] + original["substep_ground_samples"]
    missed = [r for r in rows if not r["hit"]["blocking"]
              and not r["hit"]["initial_overlap"] and r["hit"]["actor_path"] is None]
    keys = {(r["edge_id"], r["sample_role"], r["index"], r["offset_cm"]) for r in missed}
    if len(missed) != 2 or keys != {("E19", "5m_corridor", 22, 0.0),
                                  ("E27", "50cm_centerline", 52, 0.0)}:
        raise RuntimeError("Preserved failure set changed; inspect instead of expanding this diagnostic")
    points = []
    for row in missed:
        xy = row["xy_cm"]
        neighbors = row["seam_diagnostic"]["neighbor_observations"]
        heights = [n["surface"]["height_cm"] for n in neighbors if n.get("surface") is not None
                   and n["hit"]["blocking"] and not n["hit"]["initial_overlap"] and n["hit"]["is_landscape"]]
        if len(heights) < 2 or not all(math.isfinite(v) for v in xy + heights):
            raise RuntimeError("Insufficient actual neighbor observations for bounded diagnostic rays")
        points.append({"edge_id": row["edge_id"], "sample_role": row["sample_role"], "index": row["index"],
                       "xy_cm": xy, "original_hit": row["hit"],
                       "original_neighbor_observations": neighbors,
                       "reference_height_cm": sum(heights) / len(heights),
                       "reference_height_source": "mean_of_recorded_actual_neighbor_hits_not_a_central_observation",
                       "reference_height_spread_cm": max(heights) - min(heights)})
    return points


def diagnose():
    """Write one exclusive diagnostic report and return observed query results.

    Short rays and sphere sweeps use recorded neighbor height +/-200cm only to
    bound their query extent. They do not verify the missing central ray, repair
    the terrain, or change the original acceptance criteria.
    """
    if REPORT.exists():
        raise RuntimeError("Preserve the existing diagnostic; this path is exclusive")
    original_hash = _hash(ORIGINAL)
    original = json.loads(ORIGINAL.read_text(encoding="utf-8-sig"))
    if original.get("success") is not False:
        raise RuntimeError("Expected the original failed static QA report")
    points = _points(original)
    top, bottom = original["assumptions"]["ray_top_cm"], original["assumptions"]["ray_bottom_cm"]
    if not all(math.isfinite(v) for v in (top, bottom)) or top <= bottom:
        raise RuntimeError("Invalid original full-ray bounds")
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
        if (Path(unreal.Paths.project_dir()).resolve() != ROOT.resolve() or not world
                or editor.get_editor_world() != world or world.get_path_name().split(".")[0] != TARGET
                or levels.is_in_play_in_editor() or dirty()):
            raise RuntimeError("Exact clean Crop editor world with PIE stopped is required")
        if _hash(ORIGINAL) != original_hash:
            raise RuntimeError("Original evidence changed; stop without overwriting it")

    guard()
    helper = getattr(unreal, "Rogue10mCrop20261114WorldAuthoring", None)
    if helper is None:
        raise RuntimeError("Exact-map native observer unavailable")
    inventory = json.loads(helper.inspect_candidate_terrain())
    count_fields = ("component_count", "collision_component_count", "unique_component_count",
                    "unique_collision_component_count")

    def exact45(value):
        return (value.get("success") is True and value.get("retained_9_by_5_lattice_observed") is True
                and value.get("all_components_registered") is True
                and all(value.get(k) == 45 for k in count_fields))

    if not exact45(inventory):
        raise RuntimeError("Native observer has not confirmed 45 registered render/collision pairs")
    guard()
    ignored = [a for a in actors.get_all_level_actors()
               if a.get_actor_label() == "WL26_PersistentBossGround"
               and "Rogue10m.WorldLayout20261112" in map(str, a.get_editor_property("tags"))]
    legacy = _legacy()
    report = {"status": "started_diagnostic_only", "target": TARGET,
              "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "original_report": str(ORIGINAL), "original_sha256_before": original_hash,
              "original_status": original.get("status"), "original_success": original["success"],
              "map_sha256_before": _hash(MAP_FILE), "native_inventory_before": inventory,
              "full_ray_z_bounds_cm": [top, bottom], "x_offsets_cm": OFFSETS_CM, "sphere_radii_cm": RADII_CM,
              "query_channel": "TraceTypeQuery1", "trace_complex": False,
              "ignored_supports": [a.get_path_name() for a in ignored], "points": [],
              "terrain_repair_performed": False, "original_failure_reclassified": False,
              "pie_started": False, "game_started": False, "asset_save_performed": False,
              "limitation": "Query diagnostics only. Neighbor height bounds, finite-radius hits and offset rays are not an exact central point-ray pass or evidence of a terrain repair."}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    with REPORT.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + "\n")

    def flush():
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def record(hit):
        return {"hit": hit, "valid_landscape_observation": bool(hit["blocking"]
                and not hit["initial_overlap"] and hit["is_landscape"]
                and hit["impact_point_cm"] is not None and hit["impact_normal"] is not None
                and all(math.isfinite(v) for v in hit["impact_point_cm"] + hit["impact_normal"]))}

    channel, draw = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, unreal.DrawDebugTrace.NONE
    try:
        for point in points:
            guard()
            x, y = point["xy_cm"]
            z = point["reference_height_cm"]
            row = {**point, "direct_rays": [], "sphere_sweeps": []}
            report["points"].append(row)
            for offset in OFFSETS_CM:
                for kind, ray_top, ray_bottom in (("original_full_extent", top, bottom),
                                                   ("neighbor_bounded_short_extent", z + 200, z - 200)):
                    start, end = [x + offset, y, ray_top], [x + offset, y, ray_bottom]
                    hit = legacy._unpack_hit(unreal, unreal.SystemLibrary.line_trace_single(
                        world, unreal.Vector(*start), unreal.Vector(*end), channel, False, ignored, draw, True))
                    row["direct_rays"].append({"extent_kind": kind, "x_offset_cm": offset,
                                                "start_cm": start, "end_cm": end, **record(hit)})
            for radius in RADII_CM:
                start, end = [x, y, z + 200], [x, y, z - 200]
                hit = legacy._unpack_hit(unreal, unreal.SystemLibrary.sphere_trace_single(
                    world, unreal.Vector(*start), unreal.Vector(*end), radius, channel, False, ignored, draw, True))
                row["sphere_sweeps"].append({"radius_cm": radius, "start_cm": start, "end_cm": end, **record(hit)})
            flush()
        guard()
        after = json.loads(helper.inspect_candidate_terrain())
        guard()
        report["native_inventory_after"] = after
        report["original_sha256_after"] = _hash(ORIGINAL)
        report["map_sha256_after"] = _hash(MAP_FILE)
        report["dirty_packages_after"] = dirty()
        report["observation_stability"] = {"native_lattice45": exact45(after),
            "original_evidence_unchanged": report["original_sha256_after"] == original_hash,
            "map_file_unchanged": report["map_sha256_after"] == report["map_sha256_before"],
            "dirty_empty": not report["dirty_packages_after"]}
        report["direct_ray_count"] = sum(len(p["direct_rays"]) for p in report["points"])
        report["sphere_sweep_count"] = sum(len(p["sphere_sweeps"]) for p in report["points"])
        report["status"] = ("diagnostic_completed_original_failure_preserved"
                            if all(report["observation_stability"].values()) else "diagnostic_state_changed_investigate")
        report["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        flush()
        return report
    except Exception as error:
        report.update(status="diagnostic_blocked_or_failed_original_failure_not_reclassified",
                      error=str(error), traceback=traceback.format_exc(),
                      finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        flush()
        raise
