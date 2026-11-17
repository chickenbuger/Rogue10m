"""Pure, inert geometry plan for the 2026-11-17 natural-field dressing pass.

Call build_plan(world_layout, existing_scene_plan) with parsed dictionaries.
This module does not read files, import Unreal, create assets, or mutate inputs.
All instance dimensions are full centimetres; Engine Cube/Cylinder use a 100 cm
reference size. Authoring must check actual native bounds before applying scale.
The collision descriptions are intentions, not calls to an Unreal collision API.
"""

from __future__ import annotations

import math


BASE_Z_CM = 9600.0
WORLD_ANCHOR_CM = (15000.0, 32550.0)
TAG = "Rogue10m.NaturalWorldSetDressing20261117"
MESHES = {"cube": "/Engine/BasicShapes/Cube", "cylinder": "/Engine/BasicShapes/Cylinder"}
_MATERIAL_ROOT = "/Game/Rogue10m/World/StartVillage/Materials/"
MATERIALS = {
    "wood": _MATERIAL_ROOT + "M_StartVillage_DarkTimber",
    "stone": _MATERIAL_ROOT + "M_StartVillage_WarmStone",
    "teal": _MATERIAL_ROOT + "M_StartVillage_Banner_Teal",
    "ochre": _MATERIAL_ROOT + "M_StartVillage_Banner_Ochre",
    "blue": _MATERIAL_ROOT + "M_StartVillage_Banner_Blue",
}
BOUNDARY_LOCAL_M = (
    (-455.0, -65.0), (-455.0, 270.0), (-430.0, 350.0),
    (-250.0, 428.0), (-70.0, 438.0), (75.0, 438.0),
    (260.0, 425.0), (455.0, 380.0), (485.0, 320.0),
    (485.0, 30.0), (370.0, -65.0), (120.0, -75.0),
    (-200.0, -75.0),
)
_HUBS = {
    "H": {"radius": 3800.0, "height": 260.0, "color": "teal", "kind": "hunter_timber"},
    "M": {"radius": 3800.0, "height": 180.0, "color": "ochre", "kind": "market_timber"},
    "T": {"radius": 3700.0, "height": 260.0, "color": "blue", "kind": "archive_stone"},
}
_PAD_RADII_CM = {"S": 1000.0, "B": 200.0, "H": 3500.0, "M": 3500.0, "T": 3400.0,
                 "P": 1800.0, "Q": 1800.0, "G": 1800.0, "O": 1800.0, "F": 1800.0,
                 "BM": 1800.0, "BW": 1800.0, "BE": 1800.0, "BG": 1800.0}


def _distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def _segment_distance(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    den = dx * dx + dy * dy
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / den)) if den else 0.0
    return _distance(p, (a[0] + t * dx, a[1] + t * dy))


def _inside(p, polygon):
    result = False
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        if ((a[1] > p[1]) != (b[1] > p[1])) and p[0] < (b[0] - a[0]) * (p[1] - a[1]) / (b[1] - a[1]) + a[0]:
            result = not result
    return result


def _projected_hull(row):
    """Conservative XY footprint, including the full rotated cylinder box."""
    pitch, yaw, roll = (math.radians(v) for v in row["rotation_pitch_yaw_roll"])
    cp, sp, cy, sy, cr, sr = math.cos(pitch), math.sin(pitch), math.cos(yaw), math.sin(yaw), math.cos(roll), math.sin(roll)
    hx, hy, hz = (v / 2 for v in row["dimensions_cm"])
    cx, yy = row["location_cm"][:2]
    points = sorted(set((cx + cp * cy * x + (sr * sp * cy - cr * sy) * y + (-cr * sp * cy - sr * sy) * z,
                         yy + cp * sy * x + (sr * sp * sy + cr * cy) * y + (-cr * sp * sy + sr * cy) * z)
                        for x in (-hx, hx) for y in (-hy, hy) for z in (-hz, hz)))
    def cross(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    halves = []
    for sequence in (points, list(reversed(points))):
        half = []
        for p in sequence:
            while len(half) >= 2 and cross(half[-2], half[-1], p) <= 0:
                half.pop()
            half.append(p)
        halves.append(half[:-1])
    return halves[0] + halves[1]


def _segments_distance(a, b, c, d):
    def cross(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    overlapping_boxes = (max(min(a[0], b[0]), min(c[0], d[0])) <= min(max(a[0], b[0]), max(c[0], d[0]))
                         and max(min(a[1], b[1]), min(c[1], d[1])) <= min(max(a[1], b[1]), max(c[1], d[1])))
    if overlapping_boxes and cross(a, b, c) * cross(a, b, d) <= 0 and cross(c, d, a) * cross(c, d, b) <= 0:
        return 0.0
    return min(_segment_distance(a, c, d), _segment_distance(b, c, d),
               _segment_distance(c, a, b), _segment_distance(d, a, b))


def _hull_road_clearance(hull, edges):
    if any(_inside(e["a"], hull) or _inside(e["b"], hull) for e in edges):
        return 0.0
    return min(_segments_distance(a, b, e["a"], e["b"])
               for a, b in zip(hull, hull[1:] + hull[:1]) for e in edges)


def _hull_circle_clearance(hull, exclusion):
    center, radius, _ = exclusion
    if _inside(center, hull):
        return -radius
    return min(_segment_distance(center, a, b) for a, b in zip(hull, hull[1:] + hull[:1])) - radius


def _circle_point(center, radius, angle):
    return (center[0] + radius * math.cos(angle), center[1] + radius * math.sin(angle))


def _instance(label, mesh, material, center, dimensions, *, yaw=0.0, pitch=0.0, roll=0.0,
              collision_role="visual_only", hidden=False):
    values = tuple(center) + tuple(dimensions) + (yaw, pitch, roll)
    if mesh not in MESHES or material not in MATERIALS or not all(math.isfinite(v) for v in values) or min(dimensions) <= 0:
        raise ValueError("Invalid primitive instance: " + label)
    return {"label": label, "mesh": MESHES[mesh], "material": MATERIALS[material],
            "location_cm": list(center), "dimensions_cm": list(dimensions),
            "rotation_pitch_yaw_roll": [pitch, yaw, roll],
            "scale_for_verified_100cm_mesh": [v / 100.0 for v in dimensions],
            "collision_role": collision_role, "hidden_in_game": bool(hidden)}


def _group(key, kind):
    return {"id": key, "actor_label": "NW17_" + key, "kind": kind,
            "folder": "NaturalWorld20261117/" + kind, "tags": [TAG, TAG + "." + key],
            "authoring_hint": "one_actor_with_ISM_components_grouped_by_mesh_material_and_collision",
            "instances": []}


def _input_geometry(layout, scene):
    if not isinstance(layout, dict) or not isinstance(scene, dict):
        raise TypeError("Both canonical world-layout and existing pure scene-plan dictionaries are required")
    nodes = {n["id"]: (WORLD_ANCHOR_CM[0] + n["positionM"]["x"] * 100.0,
                       WORLD_ANCHOR_CM[1] + n["positionM"]["y"] * 100.0) for n in layout["nodes"]}
    edges = [{"id": e["id"], "from": e["from"], "to": e["to"],
              "a": nodes[e["from"]], "b": nodes[e["to"]]} for e in layout["edges"]]
    if len(nodes) != 21 or len(edges) != 29 or len({e["id"] for e in edges}) != 29:
        raise ValueError("Expected canonical 21-node / 29-edge graph")
    expected = {"S": (15000, 32550), "B": (15000, 62550), "H": (1000, 42550),
                "M": (29000, 42550), "T": (22000, 55550)}
    if any(_distance(nodes[k], p) > 0.001 for k, p in expected.items()):
        raise ValueError("World-anchor contract changed")
    if any(e.get("requiresQuest") or e.get("requiresSkill") for e in layout["edges"]):
        raise ValueError("The canonical road graph must remain ungated")
    sites = scene.get("sites")
    markers = scene.get("markers")
    if not isinstance(sites, list) or not isinstance(markers, list) or not sites or not markers:
        raise ValueError("Existing building reservations and marker exclusions are required")
    buildings = [(tuple(s["center"][:2]), float(s["radius_cm"]), s["name"])
                 for s in sites if s.get("inside_flat_pad")]
    npc_markers = [(tuple(m["center"][:2]), 800.0, m["label"])
                   for m in markers if m["marker_kind"] == "NPC"]
    discovery_markers = [(tuple(m["center"][:2]), 300.0, m["label"])
                         for m in markers if m["marker_kind"] in ("BOOK", "WEAPON")]
    if len(buildings) < 10 or not npc_markers:
        raise ValueError("Incomplete scene exclusion input")
    return nodes, edges, buildings, npc_markers, discovery_markers


def _road_distance(p, edges):
    return min(_segment_distance(p, e["a"], e["b"]) for e in edges)


def _find_site(node, preferred, radius, minimum_road_distance, nodes, edges, exclusions, polygon,
               minimum_radius=1400.0, maximum_radius=6200.0):
    angle = math.atan2(preferred[1], preferred[0])
    preferred_distance = math.hypot(*preferred)
    distances = sorted(set([preferred_distance] + [float(v) for v in range(int(minimum_radius), int(maximum_radius) + 1, 200)]),
                       key=lambda v: (abs(v - preferred_distance), v))
    for distance in distances:
        for step in (0,) + tuple(v for i in range(1, 25) for v in (i * 7.5, -i * 7.5)):
            p = _circle_point(node, distance, angle + math.radians(step))
            if not _inside(p, polygon) or min(_segment_distance(p, a, b) for a, b in zip(polygon, polygon[1:] + polygon[:1])) < radius + 1500:
                continue
            if _road_distance(p, edges) < minimum_road_distance + radius:
                continue
            if any(_distance(p, center) < radius + r + 100.0 for center, r, _ in exclusions):
                continue
            # All service pad footprints stay free of optional exterior props.
            if minimum_road_distance >= 1200 and any(_distance(p, nodes[k]) < radius + r for k, r in _PAD_RADII_CM.items() if k in nodes):
                continue
            return p
    raise ValueError("No road/building/marker-clear site near " + str(node))


def _enclosure(hub, center, config, edges):
    radius, height = config["radius"], config["height"]
    fence, identity = _group(hub + "_Enclosure", "VillageEnclosure"), _group(hub + "_Identity", "VillageIdentity")
    gate_rows = []
    post_width = 90.0 if hub == "T" else 40.0
    for e in edges:
        if hub not in (e["from"], e["to"]):
            continue
        other = e["b"] if e["from"] == hub else e["a"]
        theta = math.atan2(other[1] - center[1], other[0] - center[0]) % math.tau
        # Perpendicular distance from the radial road, not arc length. Square
        # endpoint posts align to the road, leaving exactly +/-700 cm clear.
        half = math.asin((700.0 + post_width / 2.0) / radius)
        gate_center = _circle_point(center, radius, theta)
        row = {"edge_id": e["id"], "hub": hub, "road_yaw_degrees": math.degrees(theta),
               "center_cm": [*gate_center, BASE_Z_CM], "clear_width_cm": 1400.0,
               "angle_radians": theta, "half_angle_radians": half, "moving_door": False,
               "post_centers_cm": []}
        for side in (-1, 1):
            xy = _circle_point(center, radius, theta + side * half)
            h = 340.0 if hub == "T" else height + 60.0
            identity["instances"].append(_instance(hub + "_Gate_" + e["id"] + ("_L" if side < 0 else "_R"),
                "cube", "stone" if hub == "T" else "wood", (*xy, BASE_Z_CM + h / 2),
                (post_width, post_width, h), yaw=math.degrees(theta), collision_role="village_enclosure_pawn_block"))
            row["post_centers_cm"].append([*xy, BASE_Z_CM])
        gate_rows.append(row)
    gates = sorted(gate_rows, key=lambda g: g["angle_radians"])
    if len(gates) != {"H": 4, "M": 4, "T": 5}[hub]:
        raise ValueError("Unexpected road incidence at " + hub)
    for index, gate in enumerate(gates):
        start = gate["angle_radians"] + gate["half_angle_radians"]
        following = gates[(index + 1) % len(gates)]
        end = following["angle_radians"] - following["half_angle_radians"]
        if index == len(gates) - 1:
            end += math.tau
        if end <= start:
            raise ValueError("Overlapping gate openings at " + hub)
        count = max(1, math.ceil((end - start) * radius / 280.0))
        points = [_circle_point(center, radius, start + (end - start) * i / count) for i in range(count + 1)]
        for i, (a, b) in enumerate(zip(points, points[1:])):
            length = _distance(a, b)
            yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
            xy = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            if hub == "T":
                fence["instances"].append(_instance(f"{hub}_Wall_{index}_{i}", "cube", "stone",
                    (*xy, BASE_Z_CM + height / 2), (length, 80, height), yaw=yaw,
                    collision_role="village_enclosure_pawn_block"))
            else:
                for rail in range(3):
                    z = height * (0.22 + rail * 0.30)
                    fence["instances"].append(_instance(f"{hub}_Rail_{index}_{i}_{rail}", "cube", "wood",
                        (*xy, BASE_Z_CM + z), (length, 22, 28), yaw=yaw,
                        collision_role="village_enclosure_pawn_block"))
        for i, p in enumerate(points[1:-1]):
            fence["instances"].append(_instance(f"{hub}_Post_{index}_{i}", "cube", "stone" if hub == "T" else "wood",
                (*p, BASE_Z_CM + height / 2), (90 if hub == "T" else 32, 90 if hub == "T" else 32, height),
                yaw=math.degrees(math.atan2(p[1] - center[1], p[0] - center[0])), collision_role="village_enclosure_pawn_block"))
    return fence, identity, gates


def _banner(group, hub, p, color):
    x, y = p
    # Distinct silhouettes identify hubs; existing hall/windmill/tower stay.
    h = {"H": 470.0, "M": 390.0, "T": 540.0}[hub]
    group["instances"].append(_instance(hub + "_IdentityPlinth", "cylinder", "stone", (x, y, BASE_Z_CM + 20), (220, 220, 40)))
    group["instances"].append(_instance(hub + "_IdentityMast", "cube", "stone" if hub == "T" else "wood",
                                        (x, y, BASE_Z_CM + h / 2), (24, 24, h)))
    group["instances"].append(_instance(hub + "_IdentityCrossbar", "cube", "wood", (x, y, BASE_Z_CM + h - 20), (240, 24, 24)))
    group["instances"].append(_instance(hub + "_IdentityBanner", "cube", color, (x, y + 15, BASE_Z_CM + h - 150), (200, 10, 230)))


def _narrative(kind, p):
    group = _group(kind, "OptionalNarrative")
    x, y = p
    def add(suffix, mesh, mat, offset, dimensions, **rotation):
        group["instances"].append(_instance(kind + "_" + suffix, mesh, mat,
            (x + offset[0], y + offset[1], BASE_Z_CM + offset[2]), dimensions, **rotation))
    if kind == "W_AbandonedCamp":
        add("ColdHearth", "cylinder", "stone", (0, 0, 16), (170, 170, 32))
        for i, yy in enumerate((-160, 150)):
            add("SleepingMat" + str(i), "cube", "teal", (-220, yy, 8), (210, 85, 16), yaw=20)
        add("Bench", "cube", "wood", (200, 0, 30), (75, 240, 60), yaw=-15)
        add("EmptySupplyBox", "cube", "wood", (130, 210, 45), (110, 100, 90))
    elif kind == "A_OverturnedTransport":
        add("OverturnedBed", "cube", "wood", (0, 0, 80), (440, 220, 28), roll=24)
        for i, yy in enumerate((-120, 120)):
            add("BrokenRail" + str(i), "cube", "wood", (0, yy, 120), (440, 25, 40), roll=24)
        for i, (xx, yy) in enumerate(((-250, 180), (170, -240), (260, 190))):
            add("EmptyCrate" + str(i), "cube", "wood", (xx, yy, 55), (110, 100, 110), yaw=i * 29)
    elif kind == "R_RuinFragments":
        for i, (xx, yy, h) in enumerate(((-250, -180, 160), (180, -150, 260), (250, 180, 95), (-180, 220, 60))):
            add("Stone" + str(i), "cube", "stone", (xx, yy, h / 2), (110, 140, h), yaw=12 + i * 17)
        add("FallenLintel", "cube", "stone", (20, 50, 45), (430, 100, 90), yaw=18)
    else:
        raise ValueError("Unknown narrative cluster")
    group.update({"functional": False, "reward_or_pickup": False, "collision_intent": "NoCollision",
                  "reservation_center_cm": [x, y, BASE_Z_CM], "reservation_radius_cm": 600.0})
    return group


def _boundary(polygon):
    group = _group("ClosedBoundaryBackup", "BoundaryCollision")
    for i, (a, b) in enumerate(zip(polygon, polygon[1:] + polygon[:1])):
        # End overlap closes corners; authoring hides this backup behind the
        # separately authored ridge, with Pawn Block and all other channels Ignore.
        group["instances"].append(_instance(f"Boundary_{i:02d}", "cube", "stone",
            ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 12000.0), (_distance(a, b) + 300.0, 150.0, 6000.0),
            yaw=math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])), collision_role="boundary_pawn_only_block", hidden=True))
    group["collision_intent"] = {"profile": "Custom", "Pawn": "Block", "all_other_channels": "Ignore",
                                  "bottom_z_cm": 9000.0, "top_z_cm": 15000.0,
                                  "visibility": "hidden_in_game_and_editor_render", "requires_actual_closed_boundary_QA": True}
    return group


def build_plan(world_layout, existing_scene_plan):
    """Return new JSON-compatible data; caller owns serialization and authoring.

    Existing scene input supplies XY building/service exclusions only. Its old
    authored Z values are deliberately not copied into this flat-Z9600 plan.
    """
    nodes, edges, buildings, npcs, discoveries = _input_geometry(world_layout, existing_scene_plan)
    polygon = [(WORLD_ANCHOR_CM[0] + x * 100, WORLD_ANCHOR_CM[1] + y * 100) for x, y in BOUNDARY_LOCAL_M]
    groups, enclosures = [], []
    for hub, config in _HUBS.items():
        fence, identity, gates = _enclosure(hub, nodes[hub], config, edges)
        p = _find_site(nodes[hub], (0, 1800), 180, 700, nodes, edges,
                       buildings + npcs + discoveries + [(nodes[hub], 900, "service_core")], polygon,
                       minimum_radius=1400, maximum_radius=2800)
        _banner(identity, hub, p, config["color"])
        groups.extend((fence, identity))
        enclosures.append({"hub": hub, "center_cm": [*nodes[hub], BASE_Z_CM], "radius_cm": config["radius"],
                           "height_cm": config["height"], "style": config["kind"], "gates": gates,
                           "identity_center_cm": [*p, BASE_Z_CM], "existing_landmark_preserved": True})
    for node, kind, preferred in (("W", "W_AbandonedCamp", (-3600, -2800)),
                                  ("A", "A_OverturnedTransport", (3000, -3500)),
                                  ("R", "R_RuinFragments", (3000, 2500))):
        p = _find_site(nodes[node], preferred, 600, 1200, nodes, edges,
                       buildings + npcs + discoveries, polygon, minimum_radius=2400, maximum_radius=6200)
        groups.append(_narrative(kind, p))
    groups.append(_boundary(polygon))
    pad_clearances = {key: min(_segment_distance(p, a, b) for a, b in zip(polygon, polygon[1:] + polygon[:1])) - _PAD_RADII_CM.get(key, 0)
                      for key, p in nodes.items()}
    arena = (15000.0, 68000.0)
    arena_clearance = min(_segment_distance(arena, a, b) for a, b in zip(polygon, polygon[1:] + polygon[:1])) - 5500.0
    if not all(_inside(p, polygon) for p in nodes.values()) or not _inside(arena, polygon) or min(pad_clearances.values()) <= 1500 or arena_clearance <= 1500:
        raise ValueError("The boundary does not preserve required pads/arena plus the 15 m ridge half-width")
    labels = [i["label"] for g in groups for i in g["instances"]]
    if len(labels) != len(set(labels)):
        raise ValueError("Duplicate planned instance labels")
    clearance_rows = []
    for group in groups:
        required = 1200.0 if group["kind"] == "OptionalNarrative" else 700.0
        minimum = math.inf
        building_minimum = math.inf
        marker_minimum = math.inf
        for row in group["instances"]:
            hull = _projected_hull(row)
            road_clearance = _hull_road_clearance(hull, edges)
            minimum = min(minimum, road_clearance)
            if road_clearance < required - 1e-6:
                raise ValueError("Projected footprint enters the reserved road corridor: " + row["label"])
            if group["kind"] != "BoundaryCollision":
                building_minimum = min(building_minimum, min(_hull_circle_clearance(hull, b) for b in buildings))
                marker_minimum = min(marker_minimum, min(_hull_circle_clearance(hull, p) for p in npcs + discoveries))
                if building_minimum < -1e-6 or marker_minimum < -1e-6:
                    raise ValueError("Projected footprint enters a building or NPC/discovery reservation: " + row["label"])
        clearance_rows.append({"group_id": group["id"], "checked_road_edges": 29,
                               "required_road_clearance_cm": required,
                               "minimum_projected_road_clearance_cm": minimum,
                               "minimum_building_reservation_clearance_cm": building_minimum if math.isfinite(building_minimum) else None,
                               "minimum_NPC_or_discovery_reservation_clearance_cm": marker_minimum if math.isfinite(marker_minimum) else None})
    return {"schema": "rogue10m.natural-world-set-dressing/1", "status": "pure_plan_not_authored",
            "tag": TAG, "base_z_cm": BASE_Z_CM, "units": "world_centimetres_full_dimensions",
            "source_graph": {"nodes": 21, "edges": 29, "unchanged": True},
            "meshes": dict(MESHES), "materials": dict(MATERIALS), "new_vendor_assets": [],
            "groups": groups, "group_count": len(groups), "instance_count": len(labels), "enclosures": enclosures,
            "boundary_local_m": [list(p) for p in BOUNDARY_LOCAL_M], "boundary_world_cm": [list(p) for p in polygon],
            "geometry_checks": {"all_21_nodes_inside_boundary": True, "pad_to_boundary_centre_clearance_cm": pad_clearances,
                                "arena_to_boundary_centre_clearance_cm": arena_clearance,
                                "narrative_minimum_road_clearance_cm": 1200.0, "gate_clear_width_cm": 1400.0,
                                "projected_footprint_clearances": clearance_rows},
            "limitations": ["No editor, renderer, collision, ownership-account, NavMesh or gameplay validation has run.",
                            "Existing building/marker exclusions describe supplied authored XY reservations, not a live actor scan.",
                            "Terrain flattening/ridge/water/road-edge material and relocation of existing actors belong to separate packets.",
                            "Hidden collision must be validated as continuous with actual Pawn movement and channel readback.",
                            "Only primitive Engine meshes and existing project-owned materials are planned; no Fab import or new vendor placement."]}
