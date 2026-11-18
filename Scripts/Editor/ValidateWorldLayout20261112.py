"""Read-only editor physics inspection of the saved 2026-11-12 WP candidate.

Import is inert. The sole editor owner loads the saved candidate and its full
editor region, then calls validate(reload_confirmed=True). This module never
creates actors, changes levels/properties, saves assets, starts PIE or builds.
Its only explicit writes are its own fresh evidence JSON and later updates to
that same report. These are editor queries, not actual CharacterMovement play.
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
TARGET = '/Game/Rogue10m/Maps/Production/WorldLayout20261112'
BASE_TAG = 'Rogue10m.WorldLayout20261112'
TERRAIN_MODULE = ROOT / 'Scripts/WorldLayoutTerrain20261112.py'
LAYOUT_JSON = ROOT / 'Docs/References/WorldMap/world-layout-20261112.json'
MAP_FILE = ROOT / 'Content/Rogue10m/Maps/Production/WorldLayout20261112.umap'
EVIDENCE = ROOT / 'Feature/doc/evidence/landscape-world-blockout-20261112'
SAMPLE_INTERVAL_CM = 500.0
CAPSULE_SUBSTEP_INTERVAL_CM = 50.0
LATERAL_OFFSETS_CM = (-400.0, 0.0, 400.0)
CAPSULE_RADIUS_CM = 42.0
CAPSULE_HALF_HEIGHT_CM = 96.0
CAPSULE_CENTER_ABOVE_GROUND_CM = 110.0
GROUND_HEIGHT_TOLERANCE_CM = 15.0
MAX_GROUND_SLOPE_DEGREES = 13.0
SEAM_DIAGNOSTIC_OFFSET_CM = 1.0
SEAM_MAX_HEIGHT_SPREAD_CM = 1.0
# This overlapping support intentionally duplicates the Landscape boss floor.
# Ignore only its explicitly owned label for TERRAIN height queries. Capsule
# sweeps still include it, and any other blocking actor remains a query result.
TERRAIN_QUERY_SUPPORT_LABELS = ('WL26_PersistentBossGround',)


def _xyz(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def _sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load_terrain():
    spec = importlib.util.spec_from_file_location('world_layout_physics_terrain_20261112', TERRAIN_MODULE)
    if not spec or not spec.loader:
        raise RuntimeError('Missing pure terrain module: ' + str(TERRAIN_MODULE))
    module = importlib.util.module_from_spec(spec)
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    for name in ('nodes_world', 'roadsegments', 'height_at'):
        if not callable(getattr(module, name, None)):
            raise RuntimeError('Missing terrain API: ' + name)
    return module


def _edge_samples(edge, terrain):
    a, b = edge['a'], edge['b']
    dx, dy = float(b[0] - a[0]), float(b[1] - a[1])
    length = math.hypot(dx, dy)
    if not math.isfinite(length) or length <= 0:
        raise RuntimeError('Invalid road length: ' + edge['id'])
    intervals = max(1, math.ceil(length / SAMPLE_INTERVAL_CM))
    perpendicular = (-dy / length, dx / length)
    rows = []
    for index in range(intervals + 1):
        fraction = index / intervals
        cx, cy = a[0] + dx * fraction, a[1] + dy * fraction
        for offset in LATERAL_OFFSETS_CM:
            x, y = cx + perpendicular[0] * offset, cy + perpendicular[1] * offset
            z = float(terrain.height_at(x, y))
            if not all(math.isfinite(v) for v in (x, y, z)):
                raise RuntimeError('Nonfinite road sample: ' + edge['id'])
            rows.append({'index': index, 'fraction': fraction, 'offset_cm': offset,
                         'xy_cm': [x, y], 'authored_height_cm': z})
    return rows, intervals


def _centerline_substeps(edge, terrain):
    """At-most-50cm XY steps; every point is ray-observed before sweeps."""
    a, b = edge['a'], edge['b']
    dx, dy = float(b[0] - a[0]), float(b[1] - a[1])
    length = math.hypot(dx, dy)
    if not math.isfinite(length) or length <= 0:
        raise RuntimeError('Invalid substep road length: ' + edge['id'])
    intervals = max(1, math.ceil(length / CAPSULE_SUBSTEP_INTERVAL_CM))
    rows = []
    for index in range(intervals + 1):
        fraction = index / intervals
        x, y = a[0] + dx * fraction, a[1] + dy * fraction
        z = float(terrain.height_at(x, y))
        if not all(math.isfinite(v) for v in (x, y, z)):
            raise RuntimeError('Nonfinite substep: ' + edge['id'])
        rows.append({'index': index, 'fraction': fraction, 'offset_cm': 0.0,
                     'xy_cm': [x, y], 'authored_height_cm': z})
    return rows, intervals


def build_validation_plan():
    """Pure sample counts; reads inputs but imports no Unreal and writes nothing."""
    terrain = _load_terrain()
    edges = list(terrain.roadsegments())
    if len(edges) != 29 or len({e['id'] for e in edges}) != 29:
        raise RuntimeError('Expected 29 unique road edges')
    rows = []
    for edge in edges:
        samples, _ = _edge_samples(edge, terrain)
        substeps, intervals = _centerline_substeps(edge, terrain)
        rows.append({'edge_id': edge['id'], 'from': edge['from'], 'to': edge['to'],
                     'ground_sample_count': len(samples), 'centerline_substep_ground_sample_count': len(substeps),
                     'sweep_segment_count': intervals * 2})
    return {'target': TARGET, 'edge_count': len(edges), 'offsets_cm': list(LATERAL_OFFSETS_CM),
            'interval_cm': SAMPLE_INTERVAL_CM, 'capsule_substep_interval_cm': CAPSULE_SUBSTEP_INTERVAL_CM, 'edges': rows,
            'ground_sample_count': sum(r['ground_sample_count'] for r in rows),
            'centerline_substep_ground_sample_count': sum(r['centerline_substep_ground_sample_count'] for r in rows),
            'bidirectional_sweep_count': sum(r['sweep_segment_count'] for r in rows),
            'unreal_imported': False, 'actual_walking': 'not_run'}


def _unpack_hit(unreal, hit):
    """UE5.8 bool+OutHit returns None or HitResult, not a bool/result tuple.

    BreakHitResult fields: blocking[0], initial overlap[1], impact point[5],
    impact normal[7], actor[9]. Preserve actor type for Landscape-only checks.
    """
    if hit is None:
        return {'blocking': False, 'initial_overlap': False, 'actor_label': None,
                'actor_path': None, 'actor_class': None, 'is_landscape': False,
                'impact_point_cm': None, 'impact_normal': None}
    if not hasattr(hit, 'to_tuple'):
        raise RuntimeError('Unexpected trace return type: ' + type(hit).__name__)
    fields = hit.to_tuple()
    if len(fields) < 11:
        raise RuntimeError('Unexpected UE5.8 HitResult packing: ' + str(len(fields)))
    actor = fields[9]
    return {'blocking': bool(fields[0]), 'initial_overlap': bool(fields[1]),
            'actor_label': actor.get_actor_label() if actor else None,
            'actor_path': actor.get_path_name() if actor else None,
            'actor_class': actor.get_class().get_path_name() if actor else None,
            'is_landscape': isinstance(actor, unreal.LandscapeProxy) if actor else False,
            'impact_point_cm': _xyz(fields[5]), 'impact_normal': _xyz(fields[7])}


def _evaluate_ground_hit(hit, authored_z):
    """Keep the original height/13-degree criteria for every observed ray."""
    normal, point = hit['impact_normal'], hit['impact_point_cm']
    finite_point = point is not None and all(math.isfinite(v) for v in point)
    error = abs(point[2] - authored_z) if finite_point else None
    normal_ok = (normal is not None and all(math.isfinite(v) for v in normal) and
                 normal[2] >= math.cos(math.radians(MAX_GROUND_SLOPE_DEGREES)))
    passed = (hit['blocking'] and not hit['initial_overlap'] and hit['is_landscape'] and normal_ok and
              error is not None and math.isfinite(error) and error <= GROUND_HEIGHT_TOLERANCE_CM)
    surface = {'height_cm': point[2], 'normal': list(normal), 'source': 'direct_landscape_ray',
               'observed_at_requested_xy': True} if passed else None
    return {'passed': bool(passed), 'height_error_cm': error, 'surface': surface}


def _diagnose_seam(raw_hit, x, y, authored_z, neighbors):
    """Only four successful +/-1cm physics observations can bridge a ray miss.

    No blocking/overlap/non-Landscape hit is ever replaced. All four neighbors
    must pass the unchanged terrain criteria and their measured heights must
    span strictly less than 1cm. The original missed ray remains in the report;
    the inferred center surface is explicitly NOT an observed central hit.
    """
    missing = not raw_hit['blocking'] and not raw_hit['initial_overlap'] and raw_hit['actor_path'] is None
    expected_offsets = {(SEAM_DIAGNOSTIC_OFFSET_CM, 0.0), (-SEAM_DIAGNOSTIC_OFFSET_CM, 0.0),
                        (0.0, SEAM_DIAGNOSTIC_OFFSET_CM), (0.0, -SEAM_DIAGNOSTIC_OFFSET_CM)}
    offsets = {tuple(row['offset_cm']) for row in neighbors}
    valid = missing and len(neighbors) == 4 and offsets == expected_offsets and all(row['passed'] for row in neighbors)
    heights = [row['surface']['height_cm'] for row in neighbors] if valid else []
    spread = max(heights) - min(heights) if heights else None
    mean_height = sum(heights) / len(heights) if heights else None
    center_error = abs(mean_height - authored_z) if mean_height is not None else None
    accepted = (valid and spread < SEAM_MAX_HEIGHT_SPREAD_CM and
                center_error <= GROUND_HEIGHT_TOLERANCE_CM)
    diagnostic = {'requested_xy_cm': [x, y], 'raw_hit': raw_hit, 'neighbor_observations': neighbors,
                  'neighbor_height_spread_cm': spread, 'inferred_center_height_cm': mean_height,
                  'inferred_center_height_error_cm': center_error,
                  'accepted': bool(accepted),
                  'diagnosis': 'inferred_mathematical_seam_miss_from_four_physics_neighbors' if accepted else 'unresolved_ray_miss',
                  'central_hit_observed': False}
    surface = None
    if accepted:
        # Retain the steepest observed neighbor normal, not a smoothed normal.
        steepest = min(neighbors, key=lambda row: row['surface']['normal'][2])
        surface = {'height_cm': mean_height, 'normal': list(steepest['surface']['normal']),
                   'source': 'four_neighbor_seam_diagnostic', 'observed_at_requested_xy': False,
                   'normal_source': 'steepest_of_four_observed_neighbor_normals',
                   'neighbor_height_spread_cm': spread}
    return {'passed': bool(accepted), 'height_error_cm': center_error, 'surface': surface,
            'seam_diagnostic': diagnostic}


def validate(*, reload_confirmed=False, run_label='PhysicsA', report_path=None,
             scene_plan=None, require_player_start=True, raise_on_failure=True):
    """Inspect saved/reloaded editor-world actors and physics; no asset mutation.

    reload_confirmed records the owner's actual reload, never performs one.
    scene_plan optionally accepts AuthorWorldLayoutScene20261112's pure plan.
    require_player_start=False permits an explicitly terrain-only inspection.
    Capsules follow freshly ray-observed ground in at-most-50cm substeps. They
    still do not simulate CharacterMovement; genuine obstructions remain fails.
    """
    if reload_confirmed is not True:
        raise RuntimeError('The editor owner must reload the saved candidate first and pass reload_confirmed=True')
    if not re.fullmatch('[A-Za-z0-9_-]{1,48}', run_label):
        raise ValueError('Use a fresh safe evidence label')
    target = (Path(report_path) if report_path else EVIDENCE / run_label / 'physics-validation.json').resolve()
    if not target.is_relative_to(EVIDENCE.resolve()) or target.suffix.lower() != '.json' or target.exists():
        raise RuntimeError('Validation evidence must be a fresh JSON inside ' + str(EVIDENCE))
    import unreal

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if Path(unreal.Paths.project_dir()).resolve() != ROOT.resolve():
        raise RuntimeError('Wrong project; no validation or evidence write performed')
    world = editor.get_editor_world()
    if not world or world.get_path_name().split('.')[0] != TARGET or levels.is_in_play_in_editor():
        raise RuntimeError('Load the saved candidate with PIE stopped; validation never changes maps')
    if not MAP_FILE.is_file():
        raise RuntimeError('Saved candidate .umap is absent')

    def dirty():
        return sorted({p.get_path_name() for p in
                       list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                       list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())})

    initial_dirty = dirty()
    if initial_dirty:
        raise RuntimeError('Unsaved packages exist; save/reload through the owner first: ' + json.dumps(initial_dirty))
    terrain = _load_terrain()
    edges, nodes = list(terrain.roadsegments()), terrain.nodes_world()
    if len(edges) != 29 or len({e['id'] for e in edges}) != 29:
        raise RuntimeError('Unexpected road schema')
    if scene_plan is not None and (scene_plan.get('map') != TARGET or not isinstance(scene_plan.get('placements'), list)):
        raise RuntimeError('Optional scene plan does not describe this candidate')
    report = {
        'status': 'started', 'target': TARGET, 'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'evidence_kind': 'read_only_editor_actor_and_physics_queries', 'owner_confirmed_saved_reload': True,
        'actual_walking': 'not_run', 'physical_input': 'not_run', 'pie_started': False,
        'direct_60_second_playtest': 'not_run', 'content_15_minute_playtest': 'not_run',
        'ai_navigation_and_combat': 'not_tested', 'scene_plan_validation': 'requested' if scene_plan is not None else 'not_requested',
        'terrain_module_sha256': _sha256(TERRAIN_MODULE), 'layout_json_sha256': _sha256(LAYOUT_JSON),
        'map_sha256_before': _sha256(MAP_FILE), 'dirty_packages_before': initial_dirty,
        'assumptions': {'sample_interval_cm': SAMPLE_INTERVAL_CM, 'lateral_offsets_cm': list(LATERAL_OFFSETS_CM),
                        'capsule_substep_interval_cm': CAPSULE_SUBSTEP_INTERVAL_CM,
                        'capsule_radius_cm': CAPSULE_RADIUS_CM, 'capsule_half_height_cm': CAPSULE_HALF_HEIGHT_CM,
                        'capsule_center_above_observed_ground_cm': CAPSULE_CENTER_ABOVE_GROUND_CM,
                        'capsule_bottom_clearance_cm': CAPSULE_CENTER_ABOVE_GROUND_CM - CAPSULE_HALF_HEIGHT_CM,
                        'ground_height_tolerance_cm': GROUND_HEIGHT_TOLERANCE_CM,
                        'maximum_ground_slope_degrees': MAX_GROUND_SLOPE_DEGREES,
                        'seam_neighbor_offset_cm': SEAM_DIAGNOSTIC_OFFSET_CM,
                        'seam_neighbor_height_spread_strictly_below_cm': SEAM_MAX_HEIGHT_SPREAD_CM},
        'method_version': 'ground_following_50cm_with_strict_four_neighbor_seam_diagnostic_v2',
        'checks': [], 'ground_samples': [], 'substep_ground_samples': [], 'seam_diagnostics': [],
        'sweeps': [], 'road_results': [], 'road_failures': [],
        'limitations': [
            'Line traces and capsule sweeps are editor physics queries, not CharacterMovement, input or NavMesh tests.',
            'Only the centerline capsule is swept; left/right samples check terrain, not full capsule clearance across the entire 8m width.',
            'Capsules use at-most-50cm substeps with ray-observed ground and 14cm bottom clearance; real grounded movement/steps are not simulated.',
            'A missed ray is usable only after four +/-1cm Landscape neighbor observations pass unchanged criteria and measured height spread is below 1cm; its inferred center is explicitly identified.',
            'Only the explicitly owned boss support floor is ignored by terrain rays; capsule sweeps include it.',
            'The owner reload confirmation is supplied evidence; this function never reloads or saves the map.',
            'Quest, NPC, item, skill, boss combat, streaming playthrough, run timing and packaged runtime remain separate verification.'
        ]}
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + '\n')

    def flush():
        target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    def check(name, passed, **detail):
        report['checks'].append({'name': name, 'passed': bool(passed), **detail})

    def guard():
        if editor.get_editor_world() != world or levels.is_in_play_in_editor():
            raise RuntimeError('Editor world or PIE state changed during read-only validation')
        if world.get_path_name().split('.')[0] != TARGET or dirty():
            raise RuntimeError('Candidate identity/clean checkpoint changed during validation')

    try:
        actors = list(actor_system.get_all_level_actors())
        by_label = {}
        for actor in actors:
            by_label.setdefault(actor.get_actor_label(), []).append(actor)
        landscapes = [a for a in actors if isinstance(a, unreal.Landscape)]
        proxies = [a for a in actors if isinstance(a, unreal.LandscapeStreamingProxy)]
        check('one_loaded_main_landscape', len(landscapes) == 1, count=len(landscapes))
        check('sixteen_loaded_streaming_proxies', len(proxies) == 16, count=len(proxies))
        for actor in landscapes + proxies:
            check('terrain_owned_' + actor.get_name(), BASE_TAG in map(str, actor.get_editor_property('tags')),
                  actor=actor.get_path_name())
        starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
        report['player_starts'] = [{'path': a.get_path_name(), 'position_cm': _xyz(a.get_actor_location()),
                                    'spatially_loaded': bool(a.get_editor_property('is_spatially_loaded'))} for a in starts]
        if require_player_start:
            check('one_saved_player_start', len(starts) == 1, count=len(starts))
            if len(starts) == 1:
                start_xyz = _xyz(starts[0].get_actor_location())
                check('player_start_at_field_S_xy', math.dist(start_xyz[:2], [nodes['S']['x'], nodes['S']['y']]) <= 1.0)
                check('player_start_always_loaded', not starts[0].get_editor_property('is_spatially_loaded'))
                report['player_start_above_authored_ground_cm'] = start_xyz[2] - terrain.height_at(*start_xyz[:2])
        settings = world.get_world_settings()
        mode = settings.get_editor_property('default_game_mode')
        report['world_settings_game_mode'] = mode.get_path_name() if mode else None
        if scene_plan is not None:
            report['scene_expected_count'] = len(scene_plan['placements'])
            for row in scene_plan['placements']:
                matches = by_label.get(row['label'], [])
                check('scene_label_' + row['label'], len(matches) == 1, count=len(matches))
                if len(matches) != 1:
                    continue
                actor = matches[0]
                if row['kind'] == 'marker':
                    check('marker_editor_only_' + row['label'], bool(actor.get_editor_property('is_editor_only_actor')))
                elif row['kind'] == 'mesh':
                    component = actor.get_component_by_class(unreal.StaticMeshComponent)
                    check('mesh_component_' + row['label'], component is not None)
                    if component:
                        actual_center, _ = actor.get_actor_bounds(False)
                        error = math.dist(_xyz(actual_center), row['center'])
                        check('mesh_bounds_center_' + row['label'], error <= 1.0, error_cm=error)
                        expected_profile = 'BlockAll' if row['collision'] else 'NoCollision'
                        check('mesh_collision_profile_' + row['label'], str(component.get_collision_profile_name()) == expected_profile)

        ignored_supports = [a for a in actors if a.get_actor_label() in TERRAIN_QUERY_SUPPORT_LABELS
                            and any(str(t).startswith(BASE_TAG) for t in a.get_editor_property('tags'))]
        report['terrain_ray_ignored_supports'] = [a.get_path_name() for a in ignored_supports]
        channel, draw = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, unreal.DrawDebugTrace.NONE
        query_counts = {'primary_ground_rays': 0, 'seam_neighbor_rays': 0, 'capsule_sweeps': 0}

        def line(x, y, authored_z):
            return _unpack_hit(unreal, unreal.SystemLibrary.line_trace_single(
                world, unreal.Vector(x, y, authored_z + 1500), unreal.Vector(x, y, authored_z - 3000),
                channel, False, ignored_supports, draw, True))

        def capsule(start, end):
            query_counts['capsule_sweeps'] += 1
            return _unpack_hit(unreal, unreal.SystemLibrary.capsule_trace_single(
                world, unreal.Vector(*start), unreal.Vector(*end), CAPSULE_RADIUS_CM, CAPSULE_HALF_HEIGHT_CM,
                channel, False, [], draw, True))

        def query_ground(sample, edge, role):
            x, y = sample['xy_cm']
            query_counts['primary_ground_rays'] += 1
            hit = line(x, y, sample['authored_height_cm'])
            evaluation = _evaluate_ground_hit(hit, sample['authored_height_cm'])
            if not hit['blocking'] and not hit['initial_overlap'] and hit['actor_path'] is None:
                neighbors = []
                for ox, oy in ((SEAM_DIAGNOSTIC_OFFSET_CM, 0.0), (-SEAM_DIAGNOSTIC_OFFSET_CM, 0.0),
                               (0.0, SEAM_DIAGNOSTIC_OFFSET_CM), (0.0, -SEAM_DIAGNOSTIC_OFFSET_CM)):
                    nx, ny = x + ox, y + oy
                    nz = float(terrain.height_at(nx, ny))
                    query_counts['seam_neighbor_rays'] += 1
                    neighbor_hit = line(nx, ny, nz)
                    neighbor = {'offset_cm': [ox, oy], 'xy_cm': [nx, ny], 'authored_height_cm': nz,
                                'hit': neighbor_hit, **_evaluate_ground_hit(neighbor_hit, nz)}
                    neighbors.append(neighbor)
                evaluation = _diagnose_seam(hit, x, y, sample['authored_height_cm'], neighbors)
                report['seam_diagnostics'].append({'edge_id': edge['id'], 'sample_role': role,
                                                   'sample': sample['index'], **evaluation['seam_diagnostic']})
            return {**sample, 'edge_id': edge['id'], 'from': edge['from'], 'to': edge['to'],
                    'sample_role': role, 'hit': hit, **evaluation}

        sx, sy = nodes['S']['x'], nodes['S']['y']
        sz = terrain.height_at(sx, sy)
        positive = capsule([sx, sy, sz + 300], [sx, sy, sz + 50])
        report['capsule_ground_positive_control'] = positive
        check('capsule_detects_landscape_positive_control', positive['blocking'] and not positive['initial_overlap'] and positive['is_landscape'])
        for edge_index, edge in enumerate(edges):
            guard()
            samples, _ = _edge_samples(edge, terrain)
            failures = []
            for sample in samples:
                row = query_ground(sample, edge, '5m_corridor')
                report['ground_samples'].append(row)
                check('terrain_%s_%d_%g' % (edge['id'], sample['index'], sample['offset_cm']), row['passed'])
                if not row['passed']:
                    failures.append({'kind': 'terrain', 'sample': sample['index'], 'offset_cm': sample['offset_cm'],
                                     'sample_role': '5m_corridor', 'height_error_cm': row['height_error_cm'],
                                     'hit': row['hit'], 'seam_diagnostic': row.get('seam_diagnostic')})
            substeps, intervals = _centerline_substeps(edge, terrain)
            centers = []
            for sample in substeps:
                row = query_ground(sample, edge, '50cm_centerline_substep')
                centers.append(row)
                report['substep_ground_samples'].append(row)
                check('substep_ground_%s_%d' % (edge['id'], sample['index']), row['passed'])
                if not row['passed']:
                    failures.append({'kind': 'terrain', 'sample': sample['index'], 'offset_cm': 0.0,
                                     'sample_role': '50cm_centerline_substep', 'height_error_cm': row['height_error_cm'],
                                     'hit': row['hit'], 'seam_diagnostic': row.get('seam_diagnostic')})
            sweep_failures = 0
            for index in range(intervals):
                a, b = centers[index], centers[index + 1]
                for reverse in (False, True):
                    source, destination = (b, a) if reverse else (a, b)
                    record = {'edge_id': edge['id'], 'segment': index, 'reverse': reverse,
                              'source_sample': source['index'], 'destination_sample': destination['index']}
                    if not source['passed'] or not destination['passed']:
                        record.update(passed=False, status='not_run_invalid_substep_ground')
                    else:
                        pa = [*source['xy_cm'], source['surface']['height_cm'] + CAPSULE_CENTER_ABOVE_GROUND_CM]
                        pb = [*destination['xy_cm'], destination['surface']['height_cm'] + CAPSULE_CENTER_ABOVE_GROUND_CM]
                        hit = capsule(pa, pb)
                        passed = not hit['blocking'] and not hit['initial_overlap']
                        record.update(passed=passed, status='queried', start_cm=pa, end_cm=pb, hit=hit,
                                      start_surface_source=source['surface']['source'], end_surface_source=destination['surface']['source'])
                    report['sweeps'].append(record)
                    check('capsule_%s_%d_%d' % (edge['id'], index, int(reverse)), record['passed'])
                    if not record['passed']:
                        sweep_failures += 1
                        failures.append({'kind': 'capsule', **record})
            result = {'edge_id': edge['id'], 'from': edge['from'], 'to': edge['to'],
                      'ground_sample_count': len(samples), 'substep_ground_sample_count': len(substeps),
                      'centerline_substep_terrain_failure_count': sum(not r['passed'] for r in centers),
                      'centerline_segment_count': intervals, 'sweep_failure_count': sweep_failures,
                      'passed': not failures}
            result['all_offset_terrain_failure_count'] = sum(f['kind'] == 'terrain' and f['sample_role'] == '5m_corridor' for f in failures)
            report['road_results'].append(result)
            if failures:
                report['road_failures'].append({'edge_id': edge['id'], 'from': edge['from'], 'to': edge['to'], 'failures': failures})
            if edge_index % 5 == 0:
                report['completed_edges'] = edge_index + 1
                flush()
        check('all_29_roads_queried', len(report['road_results']) == 29)
        check('all_29_road_corridors_clear', not report['road_failures'])
        guard()
        report['dirty_packages_after'] = dirty()
        check('no_dirty_packages_created', report['dirty_packages_after'] == initial_dirty)
        report['map_sha256_after'] = _sha256(MAP_FILE)
        check('candidate_map_file_unchanged', report['map_sha256_after'] == report['map_sha256_before'])
        check('terrain_source_unchanged', _sha256(TERRAIN_MODULE) == report['terrain_module_sha256'])
        check('layout_source_unchanged', _sha256(LAYOUT_JSON) == report['layout_json_sha256'])
        report['check_count'] = len(report['checks'])
        report['failure_count'] = sum(not c['passed'] for c in report['checks'])
        report['ground_sample_count'] = len(report['ground_samples'])
        report['substep_ground_sample_count'] = len(report['substep_ground_samples'])
        report['sweep_result_count'] = len(report['sweeps'])
        report['engine_query_counts'] = query_counts
        report['seam_diagnostic_query_count'] = len(report['seam_diagnostics'])
        report['accepted_seam_diagnostic_query_count'] = sum(d['accepted'] for d in report['seam_diagnostics'])
        report['completed_edges'] = len(report['road_results'])
        report['status'] = 'passed_editor_queries_only' if report['failure_count'] == 0 else 'failed_editor_queries'
        report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        flush()
        if report['failure_count'] and raise_on_failure:
            raise RuntimeError('Editor road/actor checks failed; inspect ' + str(target))
        return report
    except Exception as error:
        if report['status'] != 'failed_editor_queries':
            report['status'] = 'blocked_or_failed_editor_queries'
        report['error'] = str(error)
        report['traceback'] = traceback.format_exc()
        report['check_count'] = len(report['checks'])
        report['failure_count'] = sum(not c['passed'] for c in report['checks'])
        report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        flush()
        raise
