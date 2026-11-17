"""Prepare a deterministic five-village graybox without importing Unreal.

The generated JSON is an authoring input, not a saved Unreal level.  No editor,
game, build, asset import, or gameplay service is started by this module.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
from html import escape
import json
import math
from pathlib import Path
import sys

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PLAN = ROOT / "Feature/doc/evidence/five-village-route-spec-20261110/route-plan.json"
DEFAULT_EVIDENCE = ROOT / "Feature/doc/evidence/five-village-graybox-20261110"
MAP_PACKAGE = "/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110"
MATERIAL_ROOT = "/Game/Rogue10m/World/Prototypes/FiveVillageGraybox20261110/Materials"
ROAD_WIDTH_M = 8.0
BUILDING_SIZE_M = (8.0, 10.0, 8.0)
BUILDING_RADIUS_M = math.hypot(BUILDING_SIZE_M[0], BUILDING_SIZE_M[1]) / 2.0
ROAD_CLEARANCE_M = 1.5
BUILDING_SEPARATION_M = 2.0
FLOOR_BOUNDS_M = (-400.0, -100.0, 400.0, 400.0)
VILLAGES = {
    "V1": ("LEAF VILLAGE", "낙엽촌", (0.22, 0.56, 0.35, 1.0)),
    "V2": ("WINDMILL MARKET", "풍차장터", (0.72, 0.48, 0.19, 1.0)),
    "V3": ("STONE SPRING", "돌샘촌", (0.27, 0.48, 0.70, 1.0)),
    "V4": ("RECORD VILLAGE", "기록촌", (0.57, 0.36, 0.70, 1.0)),
    "V5": ("EMBER VILLAGE", "잔불촌", (0.74, 0.29, 0.22, 1.0)),
}


def point_segment_distance(point, start, end):
    """Euclidean distance to a closed segment, including zero-length segments."""
    dx, dy = end[0] - start[0], end[1] - start[1]
    length_squared = dx * dx + dy * dy
    if length_squared <= 1e-18:
        return math.hypot(point[0] - start[0], point[1] - start[1])
    fraction = max(0.0, min(1.0, ((point[0] - start[0]) * dx + (point[1] - start[1]) * dy) / length_squared))
    return math.hypot(point[0] - start[0] - fraction * dx, point[1] - start[1] - fraction * dy)


def segment_distance(a, b, c, d):
    """Distance between closed 2D segments; handles crossings and degeneracy."""
    def orientation(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])

    oa, ob = orientation(a, b, c), orientation(a, b, d)
    oc, od = orientation(c, d, a), orientation(c, d, b)
    if oa * ob < 0.0 and oc * od < 0.0:
        return 0.0
    return min(point_segment_distance(a, c, d), point_segment_distance(b, c, d),
               point_segment_distance(c, a, b), point_segment_distance(d, a, b))


def _rounded(values):
    return [round(float(value), 6) for value in values]


def _record(label, kind, center_cm, size_cm=(0, 0, 0), *, yaw=0.0,
            collision=False, material=None, **extra):
    return {"label": label, "kind": kind, "center_cm": _rounded(center_cm),
            "size_cm": _rounded(size_cm), "yaw_degrees": round(float(yaw), 6),
            "collision": bool(collision), "material": material, **extra}


def _node_positions(plan):
    result = {}
    for node in plan["nodes"]:
        if node["id"] in result or len(node["xy"]) != 2 or not all(math.isfinite(float(v)) for v in node["xy"]):
            raise ValueError("Duplicate or invalid node: " + node["id"])
        result[node["id"]] = tuple(float(v) for v in node["xy"])
    if set(VILLAGES) != {node["id"] for node in plan["nodes"] if node["kind"] == "village"}:
        raise ValueError("Expected the five approved village IDs V1..V5")
    return result


def _building_centers(village_id, center, road_segments, placed):
    """Conservative bounding circles keep the entire building off every road."""
    candidates = []
    for radius in (22.0, 28.0, 35.0):
        for angle in range(0, 360, 10):
            rad = math.radians(angle)
            point = (center[0] + radius * math.cos(rad), center[1] + radius * math.sin(rad))
            road_gap = min(point_segment_distance(point, a, b) - BUILDING_RADIUS_M - ROAD_WIDTH_M / 2.0
                           for a, b in road_segments)
            floor_gap = min(point[0] - FLOOR_BOUNDS_M[0], FLOOR_BOUNDS_M[2] - point[0],
                            point[1] - FLOOR_BOUNDS_M[1], FLOOR_BOUNDS_M[3] - point[1]) - BUILDING_RADIUS_M
            if road_gap >= ROAD_CLEARANCE_M and floor_gap >= 5.0:
                candidates.append((radius, angle, point, road_gap))
    chosen = []
    for preference in (90.0, 210.0, 330.0):
        ranked = sorted(candidates, key=lambda item: (
            min(abs(item[1] - preference), 360.0 - abs(item[1] - preference)) + (item[0] - 22.0) * 0.7,
            -item[3], item[0], item[1]))
        selected = None
        for _, _, point, _ in ranked:
            if all(math.dist(point, other) >= 2.0 * BUILDING_RADIUS_M + BUILDING_SEPARATION_M
                   for other in placed + chosen):
                selected = point
                break
        if selected is None:
            raise ValueError("Could not safely place three buildings for " + village_id)
        chosen.append(selected)
    return chosen


def generate_scene(plan):
    """Return deterministic centimeters-based actor descriptions for the editor."""
    positions = _node_positions(plan)
    if plan["coordinates"]["units"] != "meters" or len(plan["edges"]) != 19:
        raise ValueError("Expected approved meter-based 19-edge route plan")
    road_segments = []
    edge_keys = set()
    for edge in plan["edges"]:
        key = tuple(sorted((edge["a"], edge["b"])))
        if key in edge_keys or not edge["bidirectional"] or edge["required_quests"] or edge["transition"] != "walk_same_world":
            raise ValueError("Unexpected gate or duplicate edge: " + str(key))
        edge_keys.add(key)
        road_segments.append((positions[edge["a"]], positions[edge["b"]]))
    materials = {
        "floor": {"color": [0.12, 0.15, 0.17, 1.0]},
        "road": {"color": [0.35, 0.39, 0.44, 1.0]},
        "direct": {"color": [0.78, 0.64, 0.24, 1.0]},
        "building": {"color": [0.40, 0.44, 0.49, 1.0]},
        "boss": {"color": [0.56, 0.16, 0.19, 1.0]},
        "marker_shop": {"color": [0.17, 0.66, 0.73, 1.0]},
        "marker_quest": {"color": [0.87, 0.76, 0.23, 1.0]},
        "marker_dungeon": {"color": [0.83, 0.39, 0.17, 1.0]},
    }
    materials.update({village_id: {"color": list(details[2])} for village_id, details in VILLAGES.items()})
    actors = [_record("GB_FLOOR", "mesh", (0, 15000, -100), (80000, 50000, 200),
                      collision=True, material="floor", mesh="cube", role="floor")]
    for edge, (start, end) in zip(plan["edges"], road_segments):
        central = edge["a"].startswith(("S", "J")) and edge["b"] in {"J80", "J185", "J230", "B"}
        length = math.dist(start, end)
        actors.append(_record("GB_ROAD_" + edge["a"] + "_" + edge["b"], "mesh",
                              ((start[0] + end[0]) * 50, (start[1] + end[1]) * 50, 2),
                              (length * 100, ROAD_WIDTH_M * 100, 2),
                              yaw=math.degrees(math.atan2(end[1] - start[1], end[0] - start[0])),
                              material="direct" if central else "road", mesh="cube", role="road",
                              edge=[edge["a"], edge["b"]]))
    for node in plan["nodes"]:
        node_id = node["id"]
        x, y = positions[node_id]
        if node["kind"] == "village":
            diameter, material = 5600, node_id
            title = node_id + " " + VILLAGES[node_id][0] + " | OPTIONAL"
        elif node["kind"] == "junction":
            diameter, material, title = 1200, "direct", node_id + " | BOTH WAYS"
        elif node_id == "B":
            diameter, material, title = 2000, "boss", "BOSS ENTRY | OPEN / NO BOSS ACTOR"
        else:
            diameter, material, title = 1600, "direct", "START | DIRECT 300m / 50s DESIGN ONLY"
        actors.append(_record("GB_NODE_" + node_id, "mesh", (x * 100, y * 100, 4),
                              (diameter, diameter, 2), material=material, mesh="cylinder",
                              node_id=node_id, role="node_pad"))
        actors.append(_record("GB_TEXT_" + node_id, "text", (x * 100, y * 100, 1200),
                              yaw=-90, text=title, text_color=[245, 245, 235, 255],
                              text_world_size_cm=220 if node_id.startswith("V") else 180, node_id=node_id))
    placed = []
    for village_id, details in VILLAGES.items():
        center = positions[village_id]
        building_centers = _building_centers(village_id, center, road_segments, placed)
        placed.extend(building_centers)
        for index, (building_center, service) in enumerate(zip(building_centers, ("shop", "quest", "dungeon")), 1):
            bx, by = building_center
            actors.append(_record(f"GB_{village_id}_BUILDING_{index:02d}", "mesh", (bx * 100, by * 100, 400),
                                  tuple(value * 100 for value in BUILDING_SIZE_M), collision=True,
                                  material="building", mesh="cube", role="building", node_id=village_id,
                                  conservative_radius_cm=BUILDING_RADIUS_M * 100))
            inward = ((center[0] - bx), (center[1] - by))
            magnitude = math.hypot(*inward)
            marker = (bx + inward[0] / magnitude * 8.0, by + inward[1] / magnitude * 8.0)
            actors.append(_record(f"GB_{village_id}_{service.upper()}_RESERVED", "mesh", (marker[0] * 100, marker[1] * 100, 75),
                                  (200, 200, 150), material="marker_" + service, mesh="cylinder",
                                  role="reserved_marker", service=service, node_id=village_id))
            actors.append(_record(f"GB_{village_id}_{service.upper()}_TEXT", "text", (marker[0] * 100, marker[1] * 100, 300),
                                  yaw=-90, text=village_id + " " + service.upper() + " RESERVED / NOT IMPLEMENTED",
                                  text_color=[240, 235, 215, 255], text_world_size_cm=95,
                                  node_id=village_id, service=service))
    actors.append(_record("GB_BOSS_ARENA_RESERVED", "mesh", (0, 33500, 2), (7000, 7000, 2),
                          material="boss", mesh="cylinder", role="reserved_arena"))
    for name, center, size in (
            ("LEFT", (-1000, 30000, 450), (100, 100, 900)),
            ("RIGHT", (1000, 30000, 450), (100, 100, 900)),
            ("TOP", (0, 30000, 925), (2100, 100, 50))):
        actors.append(_record("GB_BOSS_GATE_" + name, "mesh", center, size,
                              material="boss", mesh="cube", role="open_gate_marker"))
    actors.extend([
        _record("GB_PLAYER_START", "player_start", (0, 0, 100), yaw=90),
        _record("GB_DIRECTIONAL_LIGHT", "directional_light", (0, 15000, 6000), yaw=-35, pitch_degrees=-45),
        _record("GB_SKY_LIGHT", "sky_light", (0, 15000, 5000)),
    ])
    scene = {
        "schema_version": 1,
        "status": "preparation_only",
        "runtime_validation": "not_run",
        "map_package": MAP_PACKAGE,
        "material_root": MATERIAL_ROOT,
        "route_plan_sha256": hashlib.sha256(json.dumps(plan, sort_keys=True, ensure_ascii=False).encode("utf-8")).hexdigest(),
        "units": "centimeters",
        "materials": materials,
        "actors": actors,
        "nodes": plan["nodes"],
        "edges": plan["edges"],
        "expected_kind_counts": dict(sorted(Counter(actor["kind"] for actor in actors).items())),
        "layout_metadata": {
            "floor_bounds_m": list(FLOOR_BOUNDS_M), "road_width_m": ROAD_WIDTH_M,
            "building_size_m": list(BUILDING_SIZE_M), "building_road_clearance_minimum_m": ROAD_CLEARANCE_M,
            "building_separation_minimum_m": BUILDING_SEPARATION_M,
            "building_clearance_method": "conservative bounding circles around actual 8m x 10m rectangles",
            "direct_distance_m": 300.0, "direct_walk_seconds_assumed": 50.0,
            "walk_speed_mps_assumed": 6.0, "actual_walk_tested": False,
            "services": "reserved markers only; no NPC/shop/quest/dungeon gameplay",
            "world_anchor_applied": False, "game_mode_class": "/Script/Engine.GameModeBase",
            "node_marker_label_pattern": "GB_NODE_{node_id}",
            "road_label_pattern": "GB_ROAD_{a}_{b}",
        },
    }
    return scene


def validate_scene(scene, plan):
    """Validate the generated actor footprints, not just the source graph."""
    checks = []
    def check(name, passed, **detail):
        checks.append({"name": name, "passed": bool(passed), **detail})

    positions = _node_positions(plan)
    road_segments = [(positions[edge["a"]], positions[edge["b"]]) for edge in plan["edges"]]
    actors = scene["actors"]
    check("unique_actor_labels", len({actor["label"] for actor in actors}) == len(actors))
    check("ten_node_pads", sum(actor.get("role") == "node_pad" for actor in actors) == 10)
    check("nineteen_road_meshes", sum(actor.get("role") == "road" for actor in actors) == 19)
    check("fifteen_buildings", sum(actor.get("role") == "building" for actor in actors) == 15)
    check("fifteen_reserved_services", sum(actor.get("role") == "reserved_marker" for actor in actors) == 15)
    check("one_player_start", sum(actor["kind"] == "player_start" for actor in actors) == 1)
    for village_id in VILLAGES:
        check(village_id + "_three_buildings", sum(actor.get("role") == "building" and actor.get("node_id") == village_id for actor in actors) == 3)
        check(village_id + "_three_reserved_services", sum(actor.get("role") == "reserved_marker" and actor.get("node_id") == village_id for actor in actors) == 3)
    road_gaps = []
    floor_gaps = []
    buildings = [actor for actor in actors if actor.get("role") == "building"]
    for actor in actors:
        check(actor["label"] + "_finite_transform", all(math.isfinite(v) for v in actor["center_cm"] + actor["size_cm"] + [actor["yaw_degrees"]]))
        if actor["kind"] == "mesh":
            check(actor["label"] + "_positive_size", all(v > 0 for v in actor["size_cm"]))
            check(actor["label"] + "_material_exists", actor["material"] in scene["materials"])
            check(actor["label"] + "_collision_scope", actor["collision"] == (actor.get("role") in {"floor", "building"}))
            x, y = [v / 100 for v in actor["center_cm"][:2]]
            hx, hy = [v / 200 for v in actor["size_cm"][:2]]
            rad = math.radians(actor["yaw_degrees"])
            extent_x = abs(math.cos(rad)) * hx + abs(math.sin(rad)) * hy
            extent_y = abs(math.sin(rad)) * hx + abs(math.cos(rad)) * hy
            check(actor["label"] + "_footprint_inside_rendered_floor",
                  x - extent_x >= FLOOR_BOUNDS_M[0] - 1e-6 and x + extent_x <= FLOOR_BOUNDS_M[2] + 1e-6
                  and y - extent_y >= FLOOR_BOUNDS_M[1] - 1e-6 and y + extent_y <= FLOOR_BOUNDS_M[3] + 1e-6)
            if actor.get("role") == "road":
                a, b = [positions[node_id] for node_id in actor["edge"]]
                generated_a = (x - math.cos(rad) * hx, y - math.sin(rad) * hx)
                generated_b = (x + math.cos(rad) * hx, y + math.sin(rad) * hx)
                check(actor["label"] + "_actual_endpoints_match_nodes",
                      math.dist(generated_a, a) < 0.0001 and math.dist(generated_b, b) < 0.0001)
                check(actor["label"] + "_actual_width_and_top", abs(actor["size_cm"][1] - 800) < 1e-6
                      and abs(actor["center_cm"][2] + actor["size_cm"][2] / 2 - 3) < 1e-6)
        if actor.get("role") == "building":
            center = tuple(v / 100 for v in actor["center_cm"][:2])
            radius = math.hypot(actor["size_cm"][0], actor["size_cm"][1]) / 200.0
            gap = min(point_segment_distance(center, a, b) - radius - ROAD_WIDTH_M / 2 for a, b in road_segments)
            floor_gap = min(center[0] - FLOOR_BOUNDS_M[0], FLOOR_BOUNDS_M[2] - center[0],
                            center[1] - FLOOR_BOUNDS_M[1], FLOOR_BOUNDS_M[3] - center[1]) - radius
            road_gaps.append(gap)
            floor_gaps.append(floor_gap)
            check(actor["label"] + "_road_clearance", gap >= ROAD_CLEARANCE_M - 1e-7, clearance_m=round(gap, 6))
            check(actor["label"] + "_within_floor", floor_gap >= 5.0 - 1e-7, margin_m=round(floor_gap, 6))
    separations = []
    for index, a in enumerate(buildings):
        for b in buildings[index + 1:]:
            separation = math.dist(a["center_cm"][:2], b["center_cm"][:2]) / 100 - 2.0 * BUILDING_RADIUS_M
            separations.append(separation)
            check(a["label"] + "_separate_" + b["label"], separation >= BUILDING_SEPARATION_M - 1e-7,
                  gap_m=round(separation, 6))
    direct_edges = [actor for actor in actors if actor.get("role") == "road" and actor["material"] == "direct"]
    direct_distance = sum(actor["size_cm"][0] for actor in direct_edges) / 100
    check("direct_four_meshes_total_300m", len(direct_edges) == 4 and abs(direct_distance - 300) < 1e-6)
    check("nonblocking_boss_gate", all(not actor["collision"] for actor in actors if actor.get("role") == "open_gate_marker"))
    check("source_edges_still_optional", all(edge["bidirectional"] and not edge["required_quests"] for edge in plan["edges"]))
    check("degenerate_distance", abs(point_segment_distance((3, 4), (0, 0), (0, 0)) - 5) < 1e-9)
    check("crossing_segment_distance", segment_distance((0, 0), (2, 2), (0, 2), (2, 0)) == 0)
    check("collinear_overlap_distance", segment_distance((0, 0), (3, 0), (1, 0), (4, 0)) == 0)
    check("parallel_segment_distance", abs(segment_distance((0, 0), (3, 0), (0, 2), (3, 2)) - 2) < 1e-9)
    failures = [item for item in checks if not item["passed"]]
    return {
        "status": "preparation_only" if not failures else "preparation_failed",
        "static_validation": "passed" if not failures else "failed",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "runtime_validation": "not_run",
        "saved_unreal_level_created": False,
        "check_count": len(checks), "failure_count": len(failures),
        "actor_count": len(actors), "kind_counts": scene["expected_kind_counts"],
        "mesh_role_counts": dict(sorted(Counter(actor.get("role") for actor in actors if actor["kind"] == "mesh").items())),
        "minimum_building_road_clearance_m": round(min(road_gaps), 6),
        "minimum_building_separation_m": round(min(separations), 6),
        "minimum_building_floor_margin_m": round(min(floor_gaps), 6),
        "direct_distance_m": round(direct_distance, 6), "direct_walk_seconds_assumed": direct_distance / 6.0,
        "checks": checks,
        "limitations": ["Flat proposed geometry only; not placed in the production world.",
                        "Road collision clearance is conservative analytic geometry, not an Unreal sweep.",
                        "No saved map, NPC/quest/shop/dungeon gameplay, editor launch, or runtime playtest produced by this script."],
    }


def _svg_color(color):
    return "#" + "".join(f"{round(max(0, min(1, value)) * 255):02x}" for value in color[:3])


def render_svg(scene):
    """Render actual generated XY footprints as an inspectable standalone SVG."""
    scale = 1.4
    def xy(center_cm):
        return 80 + (center_cm[0] / 100 + 400) * scale, 100 + (400 - center_cm[1] / 100) * scale

    lines = ['<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="980" viewBox="0 0 1280 980">',
             '<rect width="1280" height="980" fill="#101519"/>',
             '<g font-family="Segoe UI,Malgun Gothic,sans-serif" fill="#edf0ef">',
             '<text x="80" y="39" font-size="25" font-weight="600">FIVE VILLAGES / ISOLATED GRAYBOX PREPARATION</text>',
             '<text x="80" y="66" font-size="15" fill="#b9c4cc">10 nodes · 19 two-way roads · 8m road width · no quest gates · north is up</text>',
             '<rect x="80" y="100" width="1120" height="700" fill="#1f272c" stroke="#697781"/>']
    mesh_actors = [actor for actor in scene["actors"] if actor["kind"] == "mesh" and actor.get("role") != "floor"]
    priorities = {"road": 0, "node_pad": 1, "reserved_arena": 1, "building": 2, "reserved_marker": 3, "open_gate_marker": 4}
    for actor in sorted(mesh_actors, key=lambda item: priorities.get(item.get("role"), 5)):
        x, y = xy(actor["center_cm"])
        sx, sy = [value / 100 * scale for value in actor["size_cm"][:2]]
        color = _svg_color(scene["materials"][actor["material"]]["color"])
        if actor["mesh"] == "cylinder":
            lines.append(f'<ellipse cx="{x:.3f}" cy="{y:.3f}" rx="{sx/2:.3f}" ry="{sy/2:.3f}" fill="{color}" opacity="0.82"><title>{escape(actor["label"])}</title></ellipse>')
        else:
            lines.append(f'<rect x="{x-sx/2:.3f}" y="{y-sy/2:.3f}" width="{sx:.3f}" height="{sy:.3f}" transform="rotate({-actor["yaw_degrees"]:.6f} {x:.3f} {y:.3f})" fill="{color}" stroke="#b0bac0" stroke-width="0.45"><title>{escape(actor["label"])}</title></rect>')
    for node in scene["nodes"]:
        x, y = xy([node["xy"][0] * 100, node["xy"][1] * 100, 0])
        village = node["id"] in VILLAGES
        title = node["id"] + (" " + VILLAGES[node["id"]][0] if village else "")
        offset = -57 if village else (-24 if node["id"] == "B" else 23)
        lines.append(f'<text x="{x:.2f}" y="{y+offset:.2f}" text-anchor="middle" font-size="14" font-weight="600" paint-order="stroke" stroke="#101519" stroke-width="4">{escape(title)}</text>')
    lines.extend([
        '<text x="686" y="398" font-size="13" fill="#ffe189" paint-order="stroke" stroke="#101519" stroke-width="3">DIRECT 300m / 50s *</text>',
        '<line x1="1110" y1="134" x2="1110" y2="184" stroke="#edf0ef" stroke-width="2"/>',
        '<path d="M1103 146 L1110 134 L1117 146" fill="none" stroke="#edf0ef" stroke-width="2"/>',
        '<text x="1125" y="155" font-size="16">N</text>',
        '<line x1="90" y1="780" x2="230" y2="780" stroke="#edf0ef" stroke-width="3"/>',
        '<text x="160" y="770" font-size="13" text-anchor="middle">100m</text>',
        '<text x="80" y="830" font-size="16" font-weight="600">RESERVED CONTENT / NO IMPLEMENTED SERVICES</text>',
        '<text x="80" y="856" font-size="14" fill="#b9c4cc">Gray buildings: collision blocks clear of every road. Colored service markers: nonblocking placeholders.</text>',
        '<text x="80" y="881" font-size="14" fill="#b9c4cc">Cyan = shop · yellow = quest · orange = dungeon · red = open boss entrance / reserved arena</text>',
        '<text x="80" y="912" font-size="14" fill="#ffe189">* Theoretical 6m/s flat walk. No saved UE map or actual 60-second / 15-minute test is implied.</text>',
        '<text x="80" y="942" font-size="13" fill="#b9c4cc">Production OpenRunMap / StartHub / portals / boss / NPC / saves remain outside this isolated prototype.</text>',
        '</g></svg>',
    ])
    return "\n".join(lines) + "\n"


def prepare(evidence_root=DEFAULT_EVIDENCE, plan_path=DEFAULT_PLAN):
    target = Path(evidence_root).resolve()
    allowed = DEFAULT_EVIDENCE.resolve()
    if not allowed.is_relative_to(ROOT.resolve()):
        raise ValueError("The configured evidence directory resolves outside the project")
    if target != allowed and not target.is_relative_to(allowed):
        raise ValueError("Evidence output must remain under " + str(allowed))
    plan = json.loads(Path(plan_path).read_text(encoding="utf-8-sig"))
    scene = generate_scene(plan)
    report = validate_scene(scene, plan)
    if report["failure_count"]:
        raise RuntimeError("Graybox analytic geometry validation failed: " + json.dumps([item for item in report["checks"] if not item["passed"]]))
    target.mkdir(parents=True, exist_ok=True)
    for name, value in (("scene-plan.json", scene), ("layout-validation.json", report)):
        (target / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (target / "layout-preview.svg").write_text(render_svg(scene), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare", action="store_true", help="write isolated JSON/SVG analytic preparation evidence")
    parser.add_argument("--evidence-root", type=Path, default=DEFAULT_EVIDENCE)
    parser.add_argument("--plan", type=Path, default=DEFAULT_PLAN)
    args = parser.parse_args()
    if not args.prepare:
        parser.error("Pass --prepare; this script never launches Unreal.")
    report = prepare(args.evidence_root, args.plan)
    print(json.dumps({key: report[key] for key in ("status", "static_validation", "runtime_validation", "actor_count", "kind_counts", "check_count", "failure_count", "minimum_building_road_clearance_m", "minimum_building_separation_m")}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
