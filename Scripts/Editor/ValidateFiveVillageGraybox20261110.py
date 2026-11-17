"""Read-only editor collision checks for the saved five-village graybox.

Importing this module does not import Unreal or run validation. The editor owner
loads the already saved/reloaded candidate, then calls validate(). No PIE, pawn,
movement input, actor edits, asset saves, or production-level travel is performed.
"""
from __future__ import annotations

import datetime
import hashlib
import json
import math
from pathlib import Path
import traceback

ROOT = Path(__file__).resolve().parents[2]
TARGET = "/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110"
EVIDENCE = ROOT / "Feature/doc/evidence/five-village-graybox-20261110"
SCENE_PATH = EVIDENCE / "scene-plan.json"
REPORT_PATH = EVIDENCE / "collision-path-validation.json"
MAP_FILE = ROOT / "Content/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110.umap"
CAPSULE_RADIUS_CM = 42.0
CAPSULE_HALF_HEIGHT_CM = 96.0
CAPSULE_CENTER_Z_CM = 105.0
GROUND_SAMPLE_INTERVAL_CM = 500.0
GROUND_Z_TOLERANCE_CM = 0.5
POSITION_TOLERANCE_CM = 0.1


def _xyz(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def _unpack_hit(hit):
    """UE5.8 bool+OutHit packing is None or HitResult, never a bool tuple.

    PyGenUtil.cpp:1158-1171 strips the bool return. HitResult.to_tuple uses
    GameplayStatics::BreakHitResult (GameplayStatics.h:1078): blocking[0],
    initial overlap[1], impact point[5], impact normal[7], actor[9].
    """
    if hit is None:
        return {"blocking": False, "initial_overlap": False, "actor": None,
                "impact_point_cm": None, "impact_normal": None}
    if not hasattr(hit, "to_tuple"):
        raise RuntimeError("Unexpected trace return type: " + type(hit).__name__)
    fields = hit.to_tuple()
    if len(fields) < 11:
        raise RuntimeError("Unexpected NativeBreakHitResult tuple length: " + str(len(fields)))
    actor = fields[9]
    return {
        "blocking": bool(fields[0]), "initial_overlap": bool(fields[1]),
        "actor": actor.get_actor_label() if actor else None,
        "actor_path": actor.get_path_name() if actor else None,
        "impact_point_cm": _xyz(fields[5]), "impact_normal": _xyz(fields[7]),
    }


def validate():
    """Query current editor-world physics; write only the dedicated JSON report."""
    import unreal

    report = {
        "status": "started",
        "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "target": TARGET, "evidence_kind": "engine_editor_collision_queries",
        "pie_started": False, "actual_walking": "not_run", "physical_input": "not_run",
        "direct_60_second_playtest": "not_run", "content_15_minute_playtest": "not_run",
        "service_quest_gameplay": "reserved markers only; not implemented",
        "capsule_assumptions_cm": {"radius": CAPSULE_RADIUS_CM,
                                   "half_height": CAPSULE_HALF_HEIGHT_CM,
                                   "center_z": CAPSULE_CENTER_Z_CM,
                                   "bottom_ground_clearance": CAPSULE_CENTER_Z_CM - CAPSULE_HALF_HEIGHT_CM},
        "ground_sample_interval_cm": GROUND_SAMPLE_INTERVAL_CM,
        "sweep_results": [], "ground_samples": [], "road_geometry": [], "checks": [],
        "limitations": [
            "Editor physics queries are not CharacterMovement, navigation, or player-input tests.",
            "The capsule is swept 9cm above the flat floor; grounded motion and step handling are not tested.",
            "The 300m/6m-per-second value is arithmetic, not elapsed play time.",
            "Quest acceptance, service behavior, stream-out persistence, and boss combat are not tested.",
        ],
    }

    def check(name, passed, **detail):
        report["checks"].append({"name": name, "passed": bool(passed), **detail})

    def dirty_packages():
        return [str(p.get_path_name()) for p in
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())]

    try:
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if levels.is_in_play_in_editor():
            raise RuntimeError("PIE is active; preserve its owner and do not validate editor physics.")
        world = editor.get_editor_world()
        if not world or world.get_path_name().split(".")[0] != TARGET:
            raise RuntimeError("Load the saved candidate before validation; this script never switches maps.")
        if not MAP_FILE.is_file():
            raise RuntimeError("Candidate .umap is absent; prepared JSON is not a saved level.")
        report["dirty_packages_before"] = dirty_packages()
        if report["dirty_packages_before"]:
            raise RuntimeError("Unsaved packages exist; validate only a saved/reloaded checkpoint.")
        scene = json.loads(SCENE_PATH.read_text(encoding="utf-8-sig"))
        if scene["map_package"] != TARGET or len(scene["edges"]) != 19:
            raise RuntimeError("Unexpected scene package/edge count.")
        report["map_sha256_before"] = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest()
        report["scene_plan_sha256"] = hashlib.sha256(SCENE_PATH.read_bytes()).hexdigest()
        by_label = {}
        for actor in actors.get_all_level_actors():
            label = actor.get_actor_label()
            if label in by_label:
                raise RuntimeError("Duplicate actor label in candidate: " + label)
            by_label[label] = actor

        positions = {}
        for node in scene["nodes"]:
            label = "GB_NODE_" + node["id"]
            if label not in by_label:
                raise RuntimeError("Missing saved node marker: " + label)
            actual = _xyz(by_label[label].get_actor_location())
            expected_xy = [float(v) * 100 for v in node["xy"]]
            check(label + "_xy_matches", math.dist(actual[:2], expected_xy) <= POSITION_TOLERANCE_CM,
                  actual_xy_cm=actual[:2], expected_xy_cm=expected_xy)
            positions[node["id"]] = actual[:2]
        check("ten_actual_node_markers", len(positions) == 10)
        floor = by_label.get("GB_FLOOR")
        if not floor or not isinstance(floor, unreal.StaticMeshActor):
            raise RuntimeError("Saved collision floor is absent.")
        check("floor_BlockAll", str(floor.static_mesh_component.get_collision_profile_name()) == "BlockAll")

        channel = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1
        draw = unreal.DrawDebugTrace.NONE

        def capsule(start, end):
            return _unpack_hit(unreal.SystemLibrary.capsule_trace_single(
                world, unreal.Vector(*start), unreal.Vector(*end),
                CAPSULE_RADIUS_CM, CAPSULE_HALF_HEIGHT_CM, channel, False, [], draw, True))

        # A real blocking floor sweep prevents an always-empty query scene from passing.
        sx, sy = positions["S"]
        positive = capsule([sx, sy, 300.0], [sx, sy, 50.0])
        report["capsule_floor_positive_control"] = positive
        check("capsule_query_detects_floor_positive_control",
              positive["blocking"] and not positive["initial_overlap"] and positive["actor"] == "GB_FLOOR")

        lengths_cm = {}
        for edge in scene["edges"]:
            if editor.get_editor_world().get_path_name().split(".")[0] != TARGET or levels.is_in_play_in_editor():
                raise RuntimeError("Editor world or PIE state changed during validation.")
            a, b = edge["a"], edge["b"]
            start_xy, end_xy = positions[a], positions[b]
            distance_cm = math.dist(start_xy, end_xy)
            lengths_cm[tuple(sorted((a, b)))] = distance_cm
            label = "GB_ROAD_" + a + "_" + b
            road = by_label.get(label)
            if not road or not isinstance(road, unreal.StaticMeshActor):
                raise RuntimeError("Saved road mesh is absent: " + label)
            component = road.static_mesh_component
            mesh = component.get_editor_property("static_mesh")
            if not mesh:
                raise RuntimeError("Saved road has no mesh: " + label)
            bounds = mesh.get_bounds()
            scale = road.get_actor_scale3d()
            length = 2 * float(bounds.box_extent.x) * float(scale.x)
            width = 2 * float(bounds.box_extent.y) * float(scale.y)
            height = 2 * float(bounds.box_extent.z) * float(scale.z)
            center = _xyz(road.get_actor_location())
            yaw = math.radians(float(road.get_actor_rotation().yaw))
            endpoints = [[center[0] - math.cos(yaw) * length / 2, center[1] - math.sin(yaw) * length / 2],
                         [center[0] + math.cos(yaw) * length / 2, center[1] + math.sin(yaw) * length / 2]]
            endpoint_error = min(max(math.dist(endpoints[0], start_xy), math.dist(endpoints[1], end_xy)),
                                 max(math.dist(endpoints[1], start_xy), math.dist(endpoints[0], end_xy)))
            report["road_geometry"].append({"edge": [a, b], "actual_length_cm": length,
                                           "actual_width_cm": width, "actual_height_cm": height,
                                           "endpoint_max_error_cm": endpoint_error})
            check(label + "_actual_endpoints", endpoint_error <= POSITION_TOLERANCE_CM)
            check(label + "_actual_width_8m", abs(width - 800.0) <= POSITION_TOLERANCE_CM)
            check(label + "_visual_only", str(component.get_collision_profile_name()) == "NoCollision")
            for source, destination in ((a, b), (b, a)):
                point_a = [*positions[source], CAPSULE_CENTER_Z_CM]
                point_b = [*positions[destination], CAPSULE_CENTER_Z_CM]
                hit = capsule(point_a, point_b)
                clear = not hit["blocking"] and not hit["initial_overlap"]
                report["sweep_results"].append({"from": source, "to": destination,
                                                "start_cm": point_a, "end_cm": point_b,
                                                "clear": clear, "hit": hit})
                check("capsule_" + source + "_to_" + destination + "_clear", clear)
            intervals = max(1, math.ceil(distance_cm / GROUND_SAMPLE_INTERVAL_CM))
            for index in range(intervals + 1):
                fraction = index / intervals
                x = start_xy[0] + fraction * (end_xy[0] - start_xy[0])
                y = start_xy[1] + fraction * (end_xy[1] - start_xy[1])
                hit = _unpack_hit(unreal.SystemLibrary.line_trace_single(
                    world, unreal.Vector(x, y, 200.0), unreal.Vector(x, y, -50.0),
                    channel, False, [], draw, True))
                safe = (hit["blocking"] and not hit["initial_overlap"] and
                        hit["actor"] == "GB_FLOOR" and
                        hit["impact_normal"][2] >= 0.99 and
                        abs(hit["impact_point_cm"][2]) <= GROUND_Z_TOLERANCE_CM)
                report["ground_samples"].append({"edge": [a, b], "sample": index,
                                                  "xy_cm": [x, y], "flat_floor_found": safe,
                                                  "hit": hit})
                check("floor_" + a + "_" + b + "_" + str(index), safe)

        direct_nodes = ["S", "J80", "J185", "J230", "B"]
        direct_cm = sum(lengths_cm[tuple(sorted((a, b)))] for a, b in zip(direct_nodes, direct_nodes[1:]))
        report["actual_direct_centerline_m"] = direct_cm / 100.0
        report["direct_travel_seconds_arithmetic_at_6mps"] = direct_cm / 600.0
        check("actual_direct_centerline_300m", abs(direct_cm - 30000.0) <= POSITION_TOLERANCE_CM)
        check("all_19_edges_38_directions_swept", len(report["sweep_results"]) == 38)
        report["dirty_packages_after"] = dirty_packages()
        check("no_dirty_packages_created", report["dirty_packages_after"] == report["dirty_packages_before"])
        report["map_sha256_after"] = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest()
        check("candidate_map_file_unchanged", report["map_sha256_before"] == report["map_sha256_after"])
        failures = [row for row in report["checks"] if not row["passed"]]
        report["status"] = "passed" if not failures else "failed"
        report["check_count"] = len(report["checks"])
        report["failure_count"] = len(failures)
        report["floor_sample_count"] = len(report["ground_samples"])
        report["direction_sweep_count"] = len(report["sweep_results"])
        report["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        EVIDENCE.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        if failures:
            raise RuntimeError("Collision path validation failed; see " + str(REPORT_PATH))
        unreal.log("[FiveVillageGraybox] collision paths passed; " + str(REPORT_PATH))
        return report
    except Exception as error:
        if report["status"] != "failed":
            report["status"] = "blocked_or_failed"
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        report["failure_count"] = sum(not row["passed"] for row in report["checks"])
        report["check_count"] = len(report["checks"])
        report["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        EVIDENCE.mkdir(parents=True, exist_ok=True)
        REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        raise


if __name__ == "__main__":
    print("Prepared read-only collision validator. Load saved candidate and call validate() in the editor.")
