"""Prepare an additive two-actor quest placement plan without using Unreal.

This reads the retained scene JSON and local file bytes only. It never imports
Unreal, launches a process, changes a map, or overwrites an existing output.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
MAP_PACKAGE = "/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110"
ACTOR_CLASS = "/Script/Rogue10m.Rogue10mVillageQuestActor"
QUEST_ID = "V1_SupplyRecovery"
OBJECTIVE_ID = "V1_MissingSupply"
SCENE_REL = Path("Feature/doc/evidence/five-village-graybox-20261110/scene-plan.json")
MAP_REL = Path("Content/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110.umap")
POTION_REL = Path("Content/DataAsset/Item/Starter/DA_Item_HealthPotion.uasset")
EVIDENCE_REL = Path("Feature/doc/evidence/five-village-quests-20261111")
ACTOR_SOURCE_REL = Path("Feature/implementation/five-village-quests-20261111/Source/Rogue10m/World")
INTERACTION_RADIUS_CM = 250.0
CAPSULE_RADIUS_CM = 42.0
PLAYER_CENTER_Z_CM = 96.0
INWARD_DISTANCE_CM = 225.0
ROAD_HALF_WIDTH_CM = 400.0
EPSILON_CM = 0.000001
PLACEMENTS = (
    ("GB_V1_QUEST_RESERVED", "V1", "Board", 70.0, "VQ_V1_SupplyRecovery_Board"),
    ("GB_V3_DUNGEON_RESERVED", "V3", "Recovery", 35.0, "VQ_V3_MissingSupply_Recovery"),
)


class PlacementError(ValueError):
    pass


def finite_number(value):
    return type(value) in (int, float) and math.isfinite(value)


def vector(value, length, name):
    if not isinstance(value, list) or len(value) != length or not all(finite_number(x) for x in value):
        raise PlacementError("Nonfinite or malformed vector: " + name)
    return tuple(float(x) for x in value)


def point_aabb_distance(point, rect):
    """Exact 2D distance to an axis-aligned rectangle, not a bounding circle."""
    dx = max(rect[0] - point[0], 0.0, point[0] - rect[2])
    dy = max(rect[1] - point[1], 0.0, point[1] - rect[3])
    return math.hypot(dx, dy)


def point_segment_distance(point, start, end):
    dx, dy = end[0] - start[0], end[1] - start[1]
    length_squared = dx * dx + dy * dy
    if length_squared == 0.0:
        return math.dist(point[:2], start[:2])
    t = max(0.0, min(1.0, ((point[0] - start[0]) * dx + (point[1] - start[1]) * dy) / length_squared))
    return math.hypot(point[0] - start[0] - t * dx, point[1] - start[1] - t * dy)


def segment_aabb_distance(start, end, rect):
    """Closed segment/rectangle distance using slab intersection and edge extrema."""
    low, high = 0.0, 1.0
    intersects = True
    for axis, lower, upper in ((0, rect[0], rect[2]), (1, rect[1], rect[3])):
        delta = end[axis] - start[axis]
        if abs(delta) < 1e-15:
            if start[axis] < lower or start[axis] > upper:
                intersects = False
                break
        else:
            a, b = sorted(((lower - start[axis]) / delta, (upper - start[axis]) / delta))
            low, high = max(low, a), min(high, b)
            if low > high:
                intersects = False
                break
    if intersects:
        return 0.0
    corners = ((rect[0], rect[1]), (rect[0], rect[3]), (rect[2], rect[1]), (rect[2], rect[3]))
    return min(point_aabb_distance(start, rect), point_aabb_distance(end, rect),
               *(point_segment_distance(corner, start, end) for corner in corners))


def _scene_geometry(scene):
    if not isinstance(scene, dict) or scene.get("map_package") != MAP_PACKAGE:
        raise PlacementError("Only the reviewed five-village candidate map is allowed.")
    if scene.get("units") != "centimeters":
        raise PlacementError("Actor transforms must use centimeters.")
    actors = scene.get("actors")
    if not isinstance(actors, list) or not actors:
        raise PlacementError("Scene actors are missing.")
    by_label, buildings, floors = {}, [], []
    for actor in actors:
        if not isinstance(actor, dict) or not isinstance(actor.get("label"), str):
            raise PlacementError("Malformed scene actor.")
        label = actor["label"]
        if not label or label in by_label:
            raise PlacementError("Duplicate or empty scene actor label: " + label)
        by_label[label] = actor
        center = vector(actor.get("center_cm"), 3, label + ".center_cm")
        if "collision" in actor and type(actor["collision"]) is not bool:
            raise PlacementError("Collision flag must be a boolean: " + label)
        for field in ("yaw_degrees", "pitch_degrees", "roll_degrees"):
            if field in actor and not finite_number(actor[field]):
                raise PlacementError("Nonfinite actor rotation: " + label)
        if actor.get("kind") != "mesh":
            continue
        size = vector(actor.get("size_cm"), 3, label + ".size_cm")
        if min(size) <= 0.0:
            raise PlacementError("Mesh dimensions must be positive: " + label)
        if actor.get("collision") is not True:
            continue
        if actor.get("role") not in ("building", "floor") or actor.get("mesh") != "cube":
            raise PlacementError("Unsupported colliding geometry: " + label)
        yaw = float(actor.get("yaw_degrees", 0.0))
        quarter = round(yaw / 90.0)
        if abs(yaw - quarter * 90.0) > EPSILON_CM or any(abs(float(actor.get(f, 0.0))) > EPSILON_CM for f in ("pitch_degrees", "roll_degrees")):
            raise PlacementError("Collision footprint is not an axis-aligned rectangle: " + label)
        sx, sy = (size[1], size[0]) if quarter % 2 else (size[0], size[1])
        rect = (center[0] - sx / 2, center[1] - sy / 2, center[0] + sx / 2, center[1] + sy / 2)
        record = {"label": label, "aabb_xy_cm": rect, "z_range_cm": [center[2] - size[2] / 2, center[2] + size[2] / 2]}
        (floors if actor["role"] == "floor" else buildings).append(record)
    if not buildings or len(floors) != 1 or abs(floors[0]["z_range_cm"][1]) > EPSILON_CM:
        raise PlacementError("One flat ground-top-zero floor and existing building rectangles are required.")
    nodes = scene.get("nodes")
    if not isinstance(nodes, list):
        raise PlacementError("Scene node definitions are missing.")
    positions = {}
    for node in nodes:
        if not isinstance(node, dict) or not isinstance(node.get("id"), str) or node["id"] in positions:
            raise PlacementError("Duplicate or malformed scene node.")
        # The retained scene copies route-node XY in meters; actor transforms are cm.
        xy_m = vector(node.get("xy"), 2, node["id"] + ".xy")
        positions[node["id"]] = (xy_m[0] * 100.0, xy_m[1] * 100.0)
    for required in ("S", "B", "V1", "V3"):
        if required not in positions:
            raise PlacementError("Required route node is missing: " + required)
    if positions["S"] != (0.0, 0.0) or positions["B"] != (0.0, 30000.0):
        raise PlacementError("The reviewed central direct spine changed.")
    return by_label, buildings, floors[0], positions


def build_placement_plan(scene, provenance):
    """Pure static planning API. No file/editor/process operations."""
    by_label, buildings, floor, nodes = _scene_geometry(scene)
    if not isinstance(provenance, dict) or provenance.get("health_potion", {}).get("local_exists") is not True:
        raise PlacementError("The existing HealthPotion local asset is required before handoff.")
    result = []
    for marker_label, village_id, role, actor_z, label in PLACEMENTS:
        if marker_label not in by_label:
            raise PlacementError("Missing reserved marker: " + marker_label)
        if label in by_label:
            raise PlacementError("A planned actor label already exists: " + label)
        marker = by_label[marker_label]
        if marker.get("collision") is not False or marker.get("role") != "reserved_marker" or marker.get("node_id") != village_id:
            raise PlacementError("Reserved marker identity/collision is not the reviewed contract: " + marker_label)
        expected_service = "quest" if role == "Board" else "dungeon"
        if marker.get("service") != expected_service:
            raise PlacementError("Wrong reserved marker service: " + marker_label)
        center = vector(marker["center_cm"], 3, marker_label)
        xy, village = center[:2], nodes[village_id]
        dx, dy = village[0] - xy[0], village[1] - xy[1]
        length = math.hypot(dx, dy)
        if length <= EPSILON_CM:
            raise PlacementError("Inward approach direction is undefined: " + marker_label)
        stand_xy = (xy[0] + dx / length * INWARD_DISTANCE_CM, xy[1] + dy / length * INWARD_DISTANCE_CM)
        for point in (xy, stand_xy, village):
            f = floor["aabb_xy_cm"]
            if min(point[0] - f[0], f[2] - point[0], point[1] - f[1], f[3] - point[1]) <= CAPSULE_RADIUS_CM:
                raise PlacementError("Actor or inward approach falls outside the retained floor.")
        distances = [(point_aabb_distance(xy, b["aabb_xy_cm"]), b["label"]) for b in buildings]
        nearest_distance, nearest_building = min(distances)
        full_margin = nearest_distance - (INTERACTION_RADIUS_CM + CAPSULE_RADIUS_CM)
        stand_margin = min(point_aabb_distance(stand_xy, b["aabb_xy_cm"]) for b in buildings) - CAPSULE_RADIUS_CM
        path_margin = min(segment_aabb_distance(village, stand_xy, b["aabb_xy_cm"]) for b in buildings) - CAPSULE_RADIUS_CM
        actor_path_margin = min(segment_aabb_distance(stand_xy, xy, b["aabb_xy_cm"]) for b in buildings) - CAPSULE_RADIUS_CM
        distance_3d = math.dist((stand_xy[0], stand_xy[1], PLAYER_CENTER_Z_CM), (xy[0], xy[1], actor_z))
        if min(stand_margin, path_margin, actor_path_margin) <= EPSILON_CM or distance_3d > INTERACTION_RADIUS_CM:
            raise PlacementError("Blocked inward approach or out-of-range interaction: " + marker_label)
        direct_distance = point_segment_distance(xy, nodes["S"], nodes["B"])
        corridor_margin = direct_distance - ROAD_HALF_WIDTH_CM - CAPSULE_RADIUS_CM
        interaction_zone_margin = direct_distance - ROAD_HALF_WIDTH_CM - INTERACTION_RADIUS_CM - CAPSULE_RADIUS_CM
        if min(corridor_margin, interaction_zone_margin) <= EPSILON_CM:
            raise PlacementError("Quest interaction encroaches on the central direct corridor: " + marker_label)
        full_clear = full_margin > EPSILON_CM
        result.append({
            "label": label, "class_path": ACTOR_CLASS,
            "tags": ["Rogue10m.FiveVillageQuestSlice20261111", "Rogue10m.Quest." + QUEST_ID, "Rogue10m.QuestRole." + role],
            "folder": "FiveVillageQuests20261111/" + role,
            "transform": {"location_cm": [xy[0], xy[1], actor_z], "rotation_degrees": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0}, "scale": [1.0, 1.0, 1.0]},
            "properties": {"role": role, "quest_definition": None, "objective_id": OBJECTIVE_ID, "interaction_radius": INTERACTION_RADIUS_CM},
            "collision": {"actor_enabled": False, "marker_profile": "NoCollision", "prompt_profile": "NoCollision", "navigation_obstacle": False},
            "source_marker": {"label": marker_label, "original_center_cm": list(center), "preserved": True},
            "access": {"method": "actual_axis_aligned_building_rectangles_xy", "mode": "full_circle_clear" if full_clear else "inward_only_clear",
                "full_circle_radius_cm": INTERACTION_RADIUS_CM + CAPSULE_RADIUS_CM,
                "nearest_building": nearest_building, "nearest_building_distance_cm": nearest_distance,
                "full_circle_margin_cm": full_margin, "full_circle_clear": full_clear,
                "recommended_stand_cm": [stand_xy[0], stand_xy[1], PLAYER_CENTER_Z_CM],
                "interaction_distance_3d_cm": distance_3d,
                "stand_capsule_margin_cm": stand_margin, "village_to_stand_capsule_margin_cm": path_margin,
                "stand_to_actor_capsule_margin_cm": actor_path_margin,
                "central_corridor_margin_cm": corridor_margin, "interaction_zone_to_direct_corridor_margin_cm": interaction_zone_margin,
                "basis": "Whole-circle failure permits only the proven inward approach; marker XY is never relocated.",
                "thin_circle_margin": full_margin < 5.0},
        })
    return {
        "schema_version": 1, "status": "offline_placement_prepared_no_editor_execution",
        "map_package": MAP_PACKAGE, "actor_class": ACTOR_CLASS,
        "quest_id": QUEST_ID, "objective_id": OBJECTIVE_ID, "quest_definition_source": "shared_native_class_default_object",
        "actors": result, "additive_actor_count": 2,
        "existing_actor_updates": [], "existing_actor_removals": [], "world_settings_changes": [],
        "provenance": provenance,
        "canonical_scene_sha256": hashlib.sha256(json.dumps(scene, sort_keys=True, ensure_ascii=False, allow_nan=False).encode("utf-8")).hexdigest(),
        "geometry_basis": {"scene_node_xy_units": "meters", "actor_transform_units": "centimeters", "building_footprints": "exact axis-aligned rectangle distances; no bounding-circle substitute", "building_count": len(buildings), "capsule_radius_cm": CAPSULE_RADIUS_CM, "assumed_player_center_z_cm": PLAYER_CENTER_Z_CM, "interaction_radius_cm": INTERACTION_RADIUS_CM, "inward_distance_cm": INWARD_DISTANCE_CM, "vertical_building_projection": "conservative XY footprints"},
        "unreal_imported": False, "unreal_editor_executed": False, "shared_build_executed": False,
        "actual_actor_placement": "not_run", "live_map_geometry_verified": False,
        "limitations": ["Current map-file SHA is a static byte snapshot, not live actor/geometry authentication.", "Retained scene JSON may differ from the current live editor; owner must verify marker positions and collision after handoff.", "Geometry is a static AABB/footprint calculation, not engine traces, navigation, input, reward, or play validation.", "V1 full-circle clearance can be sub-centimeter; use the documented inward approach and verify actual capsule/wall/line-of-sight in Unreal.", "HealthPotion local presence does not establish current account ownership or runtime reward linkage."],
    }


def _file_record(root, path):
    if not path.is_file():
        raise PlacementError("Required local source/asset file is missing: " + str(path))
    data = path.read_bytes()
    return {"path": path.relative_to(root).as_posix(), "local_exists": True, "size_bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def prepare(project_root=ROOT, scene_path=None, output_path=None):
    root = Path(project_root).resolve()
    source = Path(scene_path) if scene_path is not None else root / SCENE_REL
    output = Path(output_path) if output_path is not None else root / EVIDENCE_REL / "placement-plan.json"
    allowed = root / EVIDENCE_REL
    if allowed.resolve() != allowed.absolute():
        raise PlacementError("The output evidence directory must not be a symlink/junction.")
    if output.suffix != ".json" or not output.resolve().is_relative_to(allowed.resolve()):
        raise PlacementError("Output must be a fresh JSON file under the dedicated quest evidence directory.")
    scene_bytes = source.read_bytes()
    scene = json.loads(scene_bytes.decode("utf-8-sig"))
    provenance = {"scene": {"path": str(source.resolve()), "sha256": hashlib.sha256(scene_bytes).hexdigest()},
                  "current_map_file": _file_record(root, root / MAP_REL),
                  "current_map_hash_scope": "static local file bytes only; not live geometry verification",
                  "health_potion": _file_record(root, root / POTION_REL),
                  "actor_sources": [_file_record(root, root / ACTOR_SOURCE_REL / name) for name in ("Rogue10mVillageQuestActor.h", "Rogue10mVillageQuestActor.cpp")]}
    plan = build_placement_plan(scene, provenance)
    plan["prepared_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8", newline="\n") as handle:
        json.dump(plan, handle, ensure_ascii=False, indent=2, allow_nan=False)
        handle.write("\n")
    return plan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--scene-path", type=Path)
    parser.add_argument("--output-path", type=Path)
    args = parser.parse_args()
    plan = prepare(args.project_root, args.scene_path, args.output_path)
    print(json.dumps({"status": plan["status"], "actor_count": len(plan["actors"]), "editor_executed": False,
                      "access_modes": {a["properties"]["role"]: a["access"]["mode"] for a in plan["actors"]}}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
