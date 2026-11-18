"""Pure, deterministic Q/Z cut-and-cover cave plan; never imports Unreal.

This module reads retained planning/checkpoint JSON and emits new planning JSON.
It does not operate the editor, mutate assets, launch processes, or save packages.
The native revision payload still requires fresh live-inspection hash/GUID guards.
"""
from __future__ import annotations

import argparse
from collections import deque
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/caves20261118'
SOURCE = ROOT / 'Feature/doc/evidence/landscape-natural-boundary20261117/target-height-plan-rounded-ridge.json'
SOURCE_FILE_SHA256 = '750abe7decd08564d610cfdb3f27b5182b1ea02d7d3271b0044ab8c91a1e8240'
SOURCE_HEIGHT_SHA256 = 'a7f20b232a14f887ba96387c828ed717c0b02f3065bb6e39ba1a3b60a2d37154'
MAP = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
TILE = 500.0
WALL = 80.0
FLOOR_Z = 8600.0
FLOOR_TOP = 8610.0
CEILING_UNDERSIDE = 9200.0
ROOF_TOP = 9602.0
SURFACE_Z = 9600.0
BLEND = 300.0
SPACING = 180000.0 / 1008.0
LANDSCAPE_ORIGIN = (-90000.0, -90000.0)
BOUNDS = (315, 630, 882, 945)
NEIGHBORS = ((1, 0), (-1, 0), (0, 1), (0, -1))

SITES = {
    'Q': {
        'label': '지하 훈련고', 'origin_cm': [-11000.0, 62050.0], 'size_cells': [12, 12],
        'entry_cell': [2, 0], 'main': [[2, 0], [2, 2], [8, 2], [8, 5], [5, 5], [5, 9], [9, 9]],
        'shortcut': [[9, 9], [2, 9], [2, 2], [2, 0]],
        'secret': [[2, 6], [0, 6], [0, 9]],
        'rooms': {'training': [5, 8, 1, 3], 'vault': [8, 10, 8, 10], 'secret': [0, 1, 8, 10]},
        'forced_closed': [[[1, y], [2, y]] for y in (8, 9, 10)],
        'ramp': {'x_cm': -9750.0, 'top_y_cm': 58050.0, 'bottom_y_cm': 62050.0, 'width_cm': 800.0},
        'book': {'catalog_id': 'stageplan.book.knuckle.k08', 'technique': '무극패왕권',
                 'site': '지하 훈련고 깊은 보관함', 'reservation_cell': [9, 9]},
    },
    'Z': {
        'label': '제련로 잔해 지하 작업함', 'origin_cm': [52000.0, 47650.0], 'size_cells': [14, 12],
        'entry_cell': [7, 11], 'main': [[7, 11], [7, 9], [2, 9], [2, 5], [6, 5], [6, 2], [11, 2]],
        'shortcut': [[11, 2], [11, 9], [7, 9], [7, 11]],
        'secret': [[11, 8], [13, 8], [13, 5]],
        'rooms': {'forge': [1, 4, 4, 6], 'workshop': [10, 12, 1, 3], 'secret': [12, 13, 4, 6]},
        'forced_closed': [[[11, y], [12, y]] for y in (4, 5, 6)] + [[[12, 3], [12, 4]]],
        'ramp': {'x_cm': 55750.0, 'top_y_cm': 57650.0, 'bottom_y_cm': 53650.0, 'width_cm': 800.0},
        'book': {'catalog_id': 'stageplan.book.greatsword.g07', 'technique': '대지파쇄',
                 'site': '제련로 잔해 지하 작업함', 'reservation_cell': [11, 2]},
    },
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def encoded_height(z):
    return round(32768 + z / 2.0)


def packed_heights(values):
    return struct.pack('<' + 'H' * len(values), *values)


def serialized(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':')) + '\n').encode('utf-8')


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def edge(a, b):
    return tuple(sorted((tuple(a), tuple(b))))


def expand(polyline):
    output = [tuple(polyline[0])]
    for start, finish in zip(polyline, polyline[1:]):
        require(start[0] == finish[0] or start[1] == finish[1], 'Non-axis-aligned cell route')
        dx, dy = finish[0] - start[0], finish[1] - start[1]
        steps = abs(dx) + abs(dy)
        require(steps > 0, 'Repeated route vertex')
        sx, sy = (dx // abs(dx) if dx else 0), (dy // abs(dy) if dy else 0)
        output.extend((start[0] + sx * i, start[1] + sy * i) for i in range(1, steps + 1))
    return output


def room_cells(rect):
    x0, x1, y0, y1 = rect
    return {(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)}


def cell_world(site, cell, z=FLOOR_TOP):
    return [site['origin_cm'][0] + TILE * (cell[0] + .5),
            site['origin_cm'][1] + TILE * (cell[1] + .5), z]


def graph_for(cells, closed):
    return {c: sorted(n for dx, dy in NEIGHBORS
                      if (n := (c[0] + dx, c[1] + dy)) in cells and edge(c, n) not in closed)
            for c in sorted(cells)}


def shortest(graph, start, goal):
    start, goal = tuple(start), tuple(goal)
    parents, pending = {start: None}, deque([start])
    while pending:
        current = pending.popleft()
        if current == goal:
            result = []
            while current is not None:
                result.append(current)
                current = parents[current]
            return list(reversed(result))
        for n in graph[current]:
            if n not in parents:
                parents[n] = current
                pending.append(n)
    raise ValueError('Unreachable graph goal: ' + str(goal))


def component(label, role, location, dimensions, rotation=(0., 0., 0.)):
    require(all(math.isfinite(v) for v in (*location, *dimensions, *rotation)), 'Nonfinite component')
    require(min(dimensions) > 0, 'Nonpositive geometry dimension')
    return {'label': label, 'mesh': 'cube', 'mesh_path': '/Engine/BasicShapes/Cube',
            'material_role': role, 'location_cm': list(location), 'dimensions_cm': list(dimensions),
            'rotation_pitch_yaw_roll_deg': list(rotation), 'collision': 'BlockAll',
            'cube_base_size_cm': 100.0, 'tick_enabled': False}


def site_plan(key, source):
    s = copy.deepcopy(source)
    paths = {name: expand(s[name]) for name in ('main', 'shortcut', 'secret')}
    ordinary_rooms = set().union(*(room_cells(r) for name, r in s['rooms'].items() if name != 'secret'))
    base_cells = set(paths['main']) | set(paths['shortcut']) | ordinary_rooms
    secret_cells = (set(paths['secret']) | room_cells(s['rooms']['secret'])) - base_cells
    cells = base_cells | secret_cells
    require(all(0 <= x < s['size_cells'][0] and 0 <= y < s['size_cells'][1] for x, y in cells), 'Cell outside pit')
    closed = {edge(*e) for e in s['forced_closed']}
    require(all(abs(a[0] - b[0]) + abs(a[1] - b[1]) == 1 for a, b in closed), 'Invalid forced wall edge')
    graph = graph_for(cells, closed)
    for path in paths.values():
        require(all(b in graph[a] for a, b in zip(path, path[1:])), 'Declared route crosses a wall')
    entry = tuple(s['entry_cell'])
    goal = tuple(s['book']['reservation_cell'])
    reachable = {tuple(c) for c in shortest_walk_all(graph, entry)}
    require(reachable == cells, 'Disconnected occupied cave cell')
    interfaces = sorted((a, b) for a in base_cells for b in graph[a] if b in secret_cells)
    require(len(interfaces) == 1, 'Secret area must have exactly one clued dogleg entrance')
    disconnected_graph = {c: [n for n in ns if edge(c, n) != edge(*interfaces[0])] for c, ns in graph.items()}
    hidden_reachable = set(shortest_walk_all(disconnected_graph, entry))
    require(not (hidden_reachable & secret_cells), 'Secret room bypasses its dogleg')

    parts = [component(f'{key}_Floor_{x:02d}_{y:02d}', 'stone_floor', cell_world(s, (x, y), FLOOR_Z),
                       (TILE, TILE, 20.0)) for x, y in sorted(cells, key=lambda c: (c[1], c[0]))]
    walls = set()
    for x, y in sorted(cells):
        for dx, dy in NEIGHBORS:
            n = (x + dx, y + dy)
            if n in cells and edge((x, y), n) not in closed:
                continue
            if dx:
                walls.add(('V', x + int(dx > 0), y))
            else:
                walls.add(('H', x, y + int(dy > 0)))
    opening = ('H', entry[0], 0 if key == 'Q' else s['size_cells'][1])
    require(opening in walls, 'Expected single exterior entry wall missing')
    walls.remove(opening)
    ox, oy = s['origin_cm']
    for axis, x, y in sorted(walls):
        if axis == 'V':
            location, size = (ox + x * TILE, oy + (y + .5) * TILE, 8900.), (WALL, TILE, 600.)
        else:
            location, size = (ox + (x + .5) * TILE, oy + y * TILE, 8900.), (TILE, WALL, 600.)
        parts.append(component(f'{key}_Wall_{axis}_{x:02d}_{y:02d}', 'stone_wall', location, size))
    center = (ox + s['size_cells'][0] * TILE / 2, oy + s['size_cells'][1] * TILE / 2)
    roof_size = (s['size_cells'][0] * TILE + 1000, s['size_cells'][1] * TILE + 1000)
    # Split the same402cm roof envelope so its underground face is stone,
    # while the flush outdoor cover uses grass; no grass-painted cave ceiling.
    parts.append(component(f'{key}_CeilingStone', 'stone_wall', (*center, 9210.), (*roof_size, 20.)))
    parts.append(component(f'{key}_RoofCover', 'grass_roof', (*center, 9411.), (*roof_size, 382.)))
    ramp = s['ramp']
    dy = ramp['bottom_y_cm'] - ramp['top_y_cm']
    require(abs(dy) == 4000., 'Ramp length contract changed')
    pitch = -math.atan(.25)
    yaw = 90. if dy > 0 else -90.
    # Unreal positivePitch raises local+X. Use a negativePitch and yaw +/-90
    # for the down-ramp longitudinal localXaxis; localY is the8m width.
    # The top face follows the declared walk plane. Subtract its transformed
    # localZnormal*half-thickness when placing the cube center.
    normal_y = -math.sin(pitch) * (1. if dy > 0 else -1.)
    location = (ramp['x_cm'], (ramp['bottom_y_cm'] + ramp['top_y_cm']) / 2 - normal_y * 10,
                9100. - math.cos(pitch) * 10)
    parts.append(component(f'{key}_Ramp', 'stone_floor', location,
                           (math.hypot(4000., 1000.), ramp['width_cm'], 20.),
                           (math.degrees(pitch), yaw, 0.)))
    main_length = (len(paths['main']) - 1) * TILE
    shortcut_length = (len(paths['shortcut']) - 1) * TILE
    require(shortcut_length < main_length, 'Return shortcut must be physically shorter than main route')
    approach = [[ramp['x_cm'], ramp['top_y_cm'] + dy * t, 9600. - 1000. * t]
                for t in (0., .25, .5, .75, 1.)]
    main_ground = approach + [cell_world(s, c) for c in paths['main']]
    short_ground = [cell_world(s, c) for c in paths['shortcut']] + list(reversed(approach))
    to_branch = shortest(graph, entry, paths['secret'][0])
    secret_ground = approach + [cell_world(s, c) for c in to_branch + paths['secret'][1:]]
    s.update(site_id=key, floor_cells=[list(c) for c in sorted(cells, key=lambda c: (c[1], c[0]))],
             graph_edges=[[list(a), list(b)] for a in sorted(graph) for b in graph[a] if a < b],
             forced_closed_edges=[[list(a), list(b)] for a, b in sorted(closed)],
             secret_interface_edges=[[list(a), list(b)] for a, b in interfaces],
             components=parts,
             route_metrics={'main_length_cm': main_length, 'shortcut_return_length_cm': shortcut_length,
                            'actual_shortest_entry_to_book_cm': (len(shortest(graph, entry, goal)) - 1) * TILE,
                            'all_cells_connected': True, 'secret_dogleg_only_entry': True,
                            'shortcut_is_shorter': True, 'physical_door_or_quest_gate': False},
             runtime_routes={
                 'main_forward_ground_cm': main_ground, 'main_reverse_ground_cm': list(reversed(main_ground)),
                 'shortcut_return_ground_cm': short_ground, 'shortcut_reverse_ground_cm': list(reversed(short_ground)),
                 'secret_from_surface_ground_cm': secret_ground, 'secret_to_surface_ground_cm': list(reversed(secret_ground))},
             book_reservation_world_cm=cell_world(s, goal),
             roof_rect_cm=[ox - 500, oy - 500, ox + s['size_cells'][0] * TILE + 500,
                           oy + s['size_cells'][1] * TILE + 500],
             pit_rect_cm=[ox, oy, ox + s['size_cells'][0] * TILE, oy + s['size_cells'][1] * TILE],
             geometry_counts={'floor_cells': len(cells), 'walls': len(walls), 'roof_and_ceiling': 2,
                              'ramps': 1, 'components_total': len(parts)})
    return s


def shortest_walk_all(graph, start):
    visited, pending = {tuple(start)}, deque([tuple(start)])
    while pending:
        for n in graph[pending.popleft()]:
            if n not in visited:
                visited.add(n)
                pending.append(n)
    return sorted(visited)


def rect_distance(x, y, rect):
    x0, y0, x1, y1 = rect
    return math.hypot(max(x0 - x, 0., x - x1), max(y0 - y, 0., y - y1))


def fade(distance):
    if distance >= BLEND:
        return 0.
    t = 1. - distance / BLEND
    return t * t * (3. - 2. * t)


def cave_height_at(x, y, sites):
    result = SURFACE_Z
    for s in sites:
        pit_weight = fade(rect_distance(x, y, s['pit_rect_cm']))
        result = min(result, SURFACE_Z - (SURFACE_Z - FLOOR_Z) * pit_weight)
        ramp = s['ramp']
        y0, y1 = sorted((ramp['top_y_cm'], ramp['bottom_y_cm']))
        rect = (ramp['x_cm'] - ramp['width_cm'] / 2, y0, ramp['x_cm'] + ramp['width_cm'] / 2, y1)
        ramp_weight = fade(rect_distance(x, y, rect))
        t = max(0., min(1., (y - ramp['top_y_cm']) / (ramp['bottom_y_cm'] - ramp['top_y_cm'])))
        # A maximum40cm recess beneath the collision slab avoids coplanar
        # terrain flicker. Clamp at8600 so the pit floor remains the minimum.
        recess = max(FLOOR_Z, SURFACE_Z - 1000. * t - 40.)
        result = min(result, SURFACE_Z - (SURFACE_Z - recess) * ramp_weight)
    return result


def segment_distance(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0., min(1., ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def bilinear(values, x, y):
    gx = (x - LANDSCAPE_ORIGIN[0]) / SPACING - BOUNDS[0]
    gy = (y - LANDSCAPE_ORIGIN[1]) / SPACING - BOUNDS[1]
    ix, iy = math.floor(gx), math.floor(gy)
    fx, fy = gx - ix, gy - iy
    require(0 <= ix < 567 and 0 <= iy < 315, 'Sample outside retained height grid')
    a, b, c, d = (values[iy * 568 + ix], values[iy * 568 + ix + 1],
                  values[(iy + 1) * 568 + ix], values[(iy + 1) * 568 + ix + 1])
    return 2 * ((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy - 32768)


def build_plan():
    raw = SOURCE.read_bytes()
    require(digest(raw) == SOURCE_FILE_SHA256, 'Frozen rounded height-plan file signature changed')
    source = json.loads(raw)
    require(source['map'] == MAP and source['vertex_bounds'] == list(BOUNDS) and
            source['width'] == 568 and source['height'] == 316, 'Source grid contract mismatch')
    old = source['target_heights']
    require(len(old) == 179488 and digest(packed_heights(old)) == SOURCE_HEIGHT_SHA256,
            'Rounded source179488 height samples mismatch')
    sites = [site_plan(k, v) for k, v in SITES.items()]
    target, changed = list(old), []
    layout = json.loads((ROOT / 'Docs/References/WorldMap/landscape-production-20261116.json').read_text(encoding='utf-8-sig'))
    nodes = {n['id']: (n['worldCm']['x'], n['worldCm']['y']) for n in layout['nodes']}
    roads = [(e['id'], nodes[e['from']], nodes[e['to']]) for e in layout['edges']]
    require(len(roads) == 29 and len(nodes) == 21, 'Protected graph changed')
    hubs = [('H', nodes['H'], 3800.), ('M', nodes['M'], 3800.), ('T', nodes['T'], 3700.)]
    nearest_road = (float('inf'), None, None)
    nearest_village = (float('inf'), None, None)
    for iy in range(BOUNDS[1], BOUNDS[3] + 1):
        y = LANDSCAPE_ORIGIN[1] + iy * SPACING
        for ix in range(BOUNDS[0], BOUNDS[2] + 1):
            x = LANDSCAPE_ORIGIN[0] + ix * SPACING
            requested = cave_height_at(x, y, sites)
            if requested == SURFACE_Z:
                continue
            i = (iy - BOUNDS[1]) * 568 + ix - BOUNDS[0]
            require(old[i] == encoded_height(SURFACE_Z), 'Cave cut touches nonflat original ridge/sample')
            value = encoded_height(requested)
            if value == old[i]:
                continue
            road_distance, road_id = min((segment_distance((x, y), a, b), name) for name, a, b in roads)
            # Include one full grid step as a sampling collar: unchanged
            # neighboring triangle vertices cannot project into the19m road zone.
            require(road_distance >= 1900. + SPACING, 'Cave cut enters protected road interpolation collar: ' + road_id)
            village_distance, village_id = min((math.hypot(x - p[0], y - p[1]) - radius, name)
                                                for name, p, radius in hubs)
            require(village_distance >= SPACING, 'Cave cut touches protected village collar: ' + village_id)
            if road_distance < nearest_road[0]:
                nearest_road = road_distance, road_id, [x, y]
            if village_distance < nearest_village[0]:
                nearest_village = village_distance, village_id, [x, y]
            target[i] = value
            changed.append(i)
    require(changed, 'No height revisions generated')
    unchanged_nodes = {name: {'before_cm': bilinear(old, *p), 'after_cm': bilinear(target, *p)} for name, p in nodes.items()}
    require(all(v['before_cm'] == v['after_cm'] == SURFACE_Z for v in unchanged_nodes.values()), 'Node height changed')
    road_samples = []
    for name, a, b in roads:
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        for step in range(math.ceil(length / 400.) + 1):
            t = min(1., step * 400. / length)
            for side in (-1900., 0., 1900.):
                x, y = a[0] + dx * t - dy / length * side, a[1] + dy * t + dx / length * side
                before, after = bilinear(old, x, y), bilinear(target, x, y)
                require(before == after, 'Protected road height sample changed: ' + name)
                road_samples.append((name, before, after))
    native = copy.deepcopy(source)
    native.update(schema='Rogue10m.NaturalBoundaryHeightRevision.20261118.v1', target_heights=target,
                  profile_revision='20261118-QZ-cut-and-cover-caves',
                  coordinate_contract='Only retained-flat9600cm samples inside two declared pits/ramps+300cm blend are revised; same2layers/45components.',
                  requires_fresh_live_guards=['expected_composite_blake3', 'expected_original_layer_blake3', 'expected_new_layer_guid'])
    changed_set = set(changed)
    preflight = {'schema': 'Rogue10m.CaveLayoutPreflight.20261118.v1', 'passed': True,
        'unreal_calls': False, 'asset_writes': False, 'source_height_samples': len(old),
        'source_file_sha256': SOURCE_FILE_SHA256, 'source_uint16le_sha256': SOURCE_HEIGHT_SHA256,
        'target_uint16le_sha256': digest(packed_heights(target)), 'changed_samples': len(changed),
        'outside_changed_index_set_exactly_preserved': all(target[i] == v for i, v in enumerate(old) if i not in changed_set),
        'all_changed_source_samples_exact9600': all(old[i] == encoded_height(SURFACE_Z) for i in changed),
        'target_min_cm': 2 * (min(target) - 32768), 'target_max_cm': 2 * (max(target) - 32768),
        'nearest_changed_sample_to_road_cm': nearest_road, 'road_protection_half_width_cm': 1900.,
        'road_interpolation_collar_cm': SPACING, 'protected_road_samples': len(road_samples),
        'all_29_road_sample_heights_preserved': True, 'protected21nodes': unchanged_nodes,
        'nearest_changed_sample_to_village_boundary_cm': nearest_village,
        'all_three_village_circles_and_grid_collar_preserved': True,
        'geometry_counts': {s['site_id']: s['geometry_counts'] for s in sites},
        'graph_checks': {s['site_id']: s['route_metrics'] for s in sites},
        'camera_validation': 'Required in rendered runtime; no camera pass inferred from this pure plan.'}
    plan = {'schema': 'Rogue10m.CaveLayoutPlan.20261118.v1', 'map': MAP, 'status': 'planned_not_authored',
        'units': 'world_centimetres', 'tile_cm': TILE, 'wall_thickness_cm': WALL,
        'floor_cube_center_z_cm': FLOOR_Z, 'floor_walk_top_z_cm': FLOOR_TOP,
        'ceiling_underside_z_cm': CEILING_UNDERSIDE, 'roof_top_z_cm': ROOF_TOP,
        'interior_vertical_clearance_cm': CEILING_UNDERSIDE - FLOOR_TOP,
        'minimum_straight_passage_clear_width_cm': TILE - WALL,
        'ramp_plane_grade': .25, 'ramp_angle_degrees': math.degrees(math.atan(.25)),
        'ramp_clearance_at_500cm_roof_border_cm': 475., 'cut_blend_cm': BLEND,
        'native_cube_size_cm': 100., 'sites': sites,
        'source_height_plan': SOURCE.relative_to(ROOT).as_posix(),
        'target_height_plan': (EVIDENCE / 'target-height-plan.json').relative_to(ROOT).as_posix(),
        'source_file_sha256': SOURCE_FILE_SHA256,
        'target_height_plan_sha256': digest(serialized(native)),
        'native_live_guard_contract': native['requires_fresh_live_guards'],
        'camera_clearance_contract': {
            'required': True, 'mode': 'actual_existing_runtime_third_person_camera_and_spring_arm',
            'preserve_existing_camera_properties': True, 'capsules_alone_certify_camera': False,
            'routes': 'Each site runtime_routes includes every cell center and both directions for main,shortcut,secret.',
            'mandatory_observations': ['surface/ramp mouth transition under475cm clearance',
                'right-angle turns forward and reverse', 'secret dogleg and room entrance forward and reverse',
                'deep room and return shortcut', 'camera location/rotation and spring-arm obstruction/pull-in',
                'rendered viewport captures at entrance,corners,secret room'],
            'new_camera_offsets_or_collision_bypass': False},
        'acquisition_contract': {'fixed_world_discovery': True, 'runtime_item_ids_unconfirmed': True,
            'auto_learn': False, 'auto_equip': False, 'normal_learning_prerequisites': True,
            'requires_reward_technique_to_reach': False, 'Z_fire_magic_required': False,
            'npc_guaranteed_completion_reward': False, 'run_timer_refill_on_entry': False,
            'same_field_world': True},
        'architecture_corrections': [{'site': 'Z', 'additional_closed_edge': [[12, 3], [12, 4]],
            'reason': 'Workshop and secret room otherwise share a direct edge bypassing the clued dogleg.',
            'root_accepted_before_authoring': True}],
        'preflight_summary': {k: v for k, v in preflight.items() if k not in ('protected21nodes',)}}
    return plan, native, preflight


def guarded_revision(native, *, expected_composite_blake3, expected_original_layer_blake3, expected_new_layer_guid):
    guards = {'expected_composite_blake3': expected_composite_blake3,
              'expected_original_layer_blake3': expected_original_layer_blake3,
              'expected_new_layer_guid': expected_new_layer_guid}
    require(all(re.fullmatch('[0-9a-fA-F]{64}' if 'blake3' in k else '[0-9a-fA-F]{32}', v)
                for k, v in guards.items()), 'Fresh native guards have invalid formats')
    result = copy.deepcopy(native)
    result.update(guards)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=EVIDENCE)
    args = parser.parse_args()
    destination = args.output_dir.resolve()
    require(destination.is_relative_to(ROOT), 'Planning evidence must stay inside project')
    plan, native, preflight = build_plan()
    destination.mkdir(parents=True, exist_ok=True)
    paths = [(destination / name, value) for name, value in (
        ('plan.json', plan), ('target-height-plan.json', native), ('preflight.json', preflight))]
    require(not any(p.exists() for p, _ in paths), 'Refuse to replace retained planning evidence')
    for p, value in paths:
        with p.open('xb') as stream:
            stream.write(serialized(value))
    print(json.dumps({'passed': preflight['passed'], 'source_samples': preflight['source_height_samples'],
        'changed_samples': preflight['changed_samples'], 'target_sha256': preflight['target_uint16le_sha256'],
        'plan_sha256': digest(serialized(plan)), 'output': str(destination),
        'geometry_counts': preflight['geometry_counts'], 'graph_checks': preflight['graph_checks']}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
