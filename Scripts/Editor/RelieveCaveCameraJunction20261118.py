"""Inert, bounded two-rock translation after an actual camera-channel failure.

Only the two diagnosed Q boulder instances move 50 cm in world +X. The old
plans, all routes, physical walls, camera settings and acceptance gates stay
immutable. The editor owner invokes once; this helper never saves or retries.
"""
from __future__ import annotations
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import traceback

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'Feature/doc/evidence/caves20261118'
SPEC = BASE / 'camera-relief-spec.json'
SPEC_SHA256 = '92e2464e246528952718203c7e82ed7455c20102fa7b8c5586835229f02fb92f'
TARGET = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
COMPOSITE = '67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
ORIGINAL = '7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'
INPUTS = {
    'plan.json': '39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74',
    'dressing-plan.json': '4177a9e1b8544e16cd61217cf64fd01a5c47da8e6ef3ee84dea25228107449db',
    'author-live/exposure-bounds.json': 'f8f002c576404a05c9686ef83d16b027c939f5422b81bdd67837c55d27ced349',
    'author-live/exposure-save-result.json': 'e67ed87362f3382adb1adc042f32485c58bbe7ef1dce078f0e2877e770663ceb',
    'author-live/triplanar.json': 'cd3b1e386c2fba6f0d939b515be421bca5227ac0112cdf86c16fcd01f039d65e',
    'runtime-camera-final/camera-collision-diagnosis.json': '193960e1946265a7abf443b54ef0068daf8a19994ea74f6c0c9596d12bdbcb67',
    'qa-exposure-final-live/validation.json': 'a27007cf35f7547d27f9abd54b3776ec12ab4734760fd4bb32c9658f2d1fc6a3',
}


class CaveCameraReliefError(RuntimeError):
    def __init__(self, report):
        self.report = report
        super().__init__(json.dumps(report, ensure_ascii=False))


def require(value, message):
    if not value:
        raise RuntimeError(message)


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def path(value):
    return value.get_path_name() if value else None


def serialized(value):
    return (json.dumps(value, sort_keys=True, ensure_ascii=False, separators=(',', ':'), allow_nan=False) + '\n').encode('utf-8')


def digest(value):
    return hashlib.sha256(serialized(value)).hexdigest()


def read(name, sha=None):
    raw = (BASE / name).read_bytes()
    if sha:
        require(hashlib.sha256(raw).hexdigest() == sha, 'Immutable input changed: ' + name)
    return json.loads(raw)


def build_spec():
    predecessors = {name: read(name, sha) for name, sha in INPUTS.items()}
    saved = predecessors['author-live/exposure-save-result.json']
    qa = predecessors['qa-exposure-final-live/validation.json']
    require(saved['success'] and saved['native_save_returned'] and saved['dirty_packages'] == [] and qa['passed'], 'Saved, validated bounds predecessor required')
    diagnosis = predecessors['runtime-camera-final/camera-collision-diagnosis.json']
    require(diagnosis['read_only'] and diagnosis['sphere_radius_cm'] == 14. and diagnosis['hit_fields'][13] == 120 and
            abs(diagnosis['hit_fields'][3] - 139.5570068359375) < .001, 'Exact actual Camera14 item120 failure diagnosis required')
    dressing = read('author-live/dressing.json')
    site = next(s for s in dressing['sites'] if s['site_id'] == 'Q')
    component = next(c for c in site['components'] if c['path'].endswith('.Q_CaveDressingISM_00'))
    changes = []
    for index, label in [(120, 'Q_Rock_Q_Wall_V_03_060'), (121, 'Q_Rock_Q_Wall_V_03_061')]:
        row = next(r for r in component['instances'] if r['index'] == index)
        require(row['label'] == label, 'Exact diagnosed rock label required')
        before = row['world_transform']; after = copy.deepcopy(before); after['p'][0] += 50.
        changes.append({'site_id': 'Q', 'actor_path': site['actor_path'], 'component_path': component['path'],
                        'package': site['package'], 'index': index, 'label': label,
                        'before_world_transform': before, 'after_world_transform': after,
                        'world_translation_delta_cm': [50., 0., 0.]})
    return {'schema': 'Rogue10m.CaveCameraReliefSpec.20261118.v1', 'map': TARGET,
            'input_sha256': INPUTS, 'native_composite_blake3': COMPOSITE, 'original_layer_blake3': ORIGINAL,
            'expected_dirty_before': [], 'expected_dirty_after': [site['package']], 'instance_changes': changes,
            'reason': 'Actual R14 Camera trace reproduced PCM collapse on Q boulder item120; translate the paired wall skin behind the unchanged physical wall.',
            'unchanged_counts': {'actors': 1408, 'structural_instances': 286, 'dressing_instances': 426, 'ISM_components': 38, 'all_instances': 1425, 'point_lights': 12, 'new_material_assets': 7},
            'new_actors_components_instances_assets': 0, 'all_2016_capsule_cases_unchanged': True,
            'all_276_runtime_waypoints_unchanged': True, 'camera_minimum_distance_cm_unchanged': 140.,
            'camera_sustained_max_pull_in_cm_unchanged': 180., 'collision_and_materials_unchanged': True,
            'save_called': False, 'actual_camera_acceptance': 'Pending new actual camera sweep and complete unchanged route; pure box prediction is not acceptance.'}


def primitive_queries(u, actors):
    names = ['ECC_WORLD_STATIC', 'ECC_WORLD_DYNAMIC', 'ECC_PAWN', 'ECC_VISIBILITY', 'ECC_CAMERA', 'ECC_PHYSICS_BODY', 'ECC_VEHICLE', 'ECC_DESTRUCTIBLE']
    names += ['ECC_ENGINE_TRACE_CHANNEL' + str(i) for i in range(1, 7)]
    names += ['ECC_GAME_TRACE_CHANNEL' + str(i) for i in range(1, 51)]
    channels = [(n, getattr(u.CollisionChannel, n)) for n in names if getattr(u.CollisionChannel, n, None) is not None]
    return {path(c): {'object_type': str(c.get_collision_object_type()),
                     'responses': {n: str(c.get_collision_response_to_channel(channel)) for n, channel in channels},
                     'overlap': bool(c.get_editor_property('generate_overlap_events')),
                     'navigation': bool(c.get_editor_property('can_ever_affect_navigation')),
                     'tick': bool(c.is_component_tick_enabled())}
            for a in actors for c in a.get_components_by_class(u.PrimitiveComponent)}


def relieve_cave_camera_junction(*, allow_editor_mutation=False, expected_spec_sha256,
                                expected_composite_blake3, expected_original_layer_blake3, expected_dirty_packages):
    report = {'schema': 'Rogue10m.CaveCameraRelief.20261118.v1', 'success': False,
              'started_utc': datetime.now(timezone.utc).isoformat(), 'mutation_started': False,
              'assets_saved': False, 'save_called': False, 'created_actors': [], 'created_components': [],
              'created_assets': [], 'instance_changes': [], 'automatic_retry': False, 'automatic_rollback': False}
    try:
        require(allow_editor_mutation is True and expected_spec_sha256 == SPEC_SHA256, 'Explicit invocation and frozen spec required')
        spec = read('camera-relief-spec.json', SPEC_SHA256)
        previous = {name: read(name, sha) for name, sha in INPUTS.items()}
        require(spec['schema'] == 'Rogue10m.CaveCameraReliefSpec.20261118.v1' and spec['map'] == TARGET and
                list(expected_dirty_packages) == [] and expected_composite_blake3 == COMPOSITE and expected_original_layer_blake3 == ORIGINAL,
                'Exact clean settled predecessor required')
        import unreal as u
        api = module('relief_component_api', 'Scripts/Editor/NaturalWorldDressing20261117.py')
        inspector = module('relief_inspector', 'Scripts/Editor/InspectCaveSites20261118.py')
        reader = module('relief_graph_reader', 'Scripts/Editor/NaturalRoadMaterial20261117.py')
        actor_reader = module('relief_actor_reader', 'Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
        visibility = module('relief_visibility_state', 'Scripts/Editor/RefineCaveVisibility20261118.py')
        walls = module('relief_effect_state', 'Scripts/Editor/RefineCaveRockWalls20261118.py')
        api._guard(u); require(api._dirty(u) == [], 'Editor must be clean and idle')
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native.get('success') and native.get('landscape_reports_up_to_date') and native['composite_blake3'] == COMPOSITE and native['original_layer_blake3'] == ORIGINAL, 'Settled collar terrain required')
        subsystem = u.get_editor_subsystem(u.EditorActorSubsystem); actors = list(subsystem.get_all_level_actors())
        require(len(actors) == 1408, 'Exact1408 actors required')
        before = actor_reader._snapshot_actors(u, actors, api); before_components = inspector.persistent_components(u, actors)
        baseline = read('baseline-live/baseline-actors.json')
        require({p: before[p] for p in baseline['actors']} == baseline['actors'] and
                {p: before_components[p] for p in baseline['actors']} == baseline['persistent_components'], 'Original1404 state changed')
        components = {path(c): c for a in actors for c in a.get_components_by_class(u.ActorComponent)}
        authored = [('structure', read('author-live/structures.json')), ('dressing', read('author-live/dressing.json'))]
        expected_paths = set(baseline['actors']); checked_counts = {'structure': 0, 'dressing': 0}
        for kind, receipt in authored:
            require(receipt['success'], 'Successful author predecessor required')
            for site in receipt['sites']:
                expected_paths.add(site['actor_path'])
                for record in site['components']:
                    c = components.get(record['path'])
                    require(isinstance(c, u.InstancedStaticMeshComponent) and c.get_instance_count() == len(record['instances']), 'Exact frozen ISM component and instance count required')
                    expected_material = record['material']
                    if kind == 'structure' and record['role'] == 'stone_wall':
                        expected_material = next(r['material'] for r in site['components'] if r['role'] == 'stone_floor')
                    require(path(c.get_editor_property('static_mesh')) == record['mesh'] and path(c.get_material(0)) == expected_material,
                            'Latest exact mesh/material binding predecessor required')
                    for row in record['instances']:
                        expected = row['observed_world_transform'] if kind == 'structure' else row['world_transform']
                        api._same_transform(api._instance_transform(u, c, row['index']), expected)
                        checked_counts[kind] += 1
        require(set(before) == expected_paths and checked_counts == {'structure': 286, 'dressing': 426}, 'Exact old1404 plus four cave actors and all712 new instance matrices required')
        tri = previous['author-live/triplanar.json']; material_before = {p: visibility.material_state(u, u.load_asset(p), reader) for p in tri['expected_material_states_after']}
        require(material_before == tri['expected_material_states_after'], 'Exact43-node seven-material predecessor required')
        sources = read('baseline-live/baseline-materials.json')['materials']
        source_before = {p: inspector.material_state(u, u.load_asset(p), reader) for p in sources}
        require(len(source_before) == 40 and source_before == sources, 'Source40 material states changed')
        effects_before = walls.effect_state(u, actors, visibility, inspector, api)
        require(effects_before == previous['author-live/exposure-bounds.json']['effects_after'], 'Latest exact global/camera/12lights/localPP predecessor required')
        queries_before = primitive_queries(u, actors)
        for site in previous['author-live/exposure-bounds.json']['sites']:
            box = components[site['box_path']]; origin, extent, _ = u.SystemLibrary.get_component_bounds(box)
            require(box.get_collision_enabled() == u.CollisionEnabled.QUERY_ONLY and
                    all('ECR_IGNORE' in v for v in queries_before[site['box_path']]['responses'].values()) and
                    not any(queries_before[site['box_path']][k] for k in ('overlap', 'navigation', 'tick')) and
                    max(abs(a-b) for a,b in zip(api._xyz(origin),site['box_center_cm'])) < .01 and
                    max(abs(a-b) for a,b in zip(api._xyz(extent),site['box_extent_cm'])) < .01,
                    'Exact query-only nonblocking exposure bounds predecessor required')
        prepared = []
        for change in spec['instance_changes']:
            c = components[change['component_path']]
            require(path(c.get_owner()) == change['actor_path'] and callable(getattr(c, 'update_instance_transform', None)), 'Exact diagnosed component/public setter required')
            observed = api._instance_transform(u, c, change['index']); api._same_transform(observed, change['before_world_transform'])
            transform = c.get_instance_transform(change['index'], world_space=True)
            require(isinstance(transform, u.Transform), 'Public Transform return required')
            transform.translation = u.Vector(*change['after_world_transform']['p'])
            prepared.append((change, c, transform, observed))
        require(api._dirty(u) == [], 'Read-only preflight dirtied packages')
        with u.ScopedEditorTransaction('Relieve diagnosed Q cave camera junction two rocks +50cmX 20261118'):
            report['mutation_started'] = True
            for change, c, transform, old in prepared:
                c.get_owner().modify(); c.modify()
                require(c.update_instance_transform(change['index'], transform, True, True, True), 'Instance update failed; no retry')
                observed = api._instance_transform(u, c, change['index']); api._same_transform(observed, change['after_world_transform'])
                require(max(abs(a-b) for a,b in zip(observed['p'][1:], old['p'][1:])) < .01 and
                        max(abs(a-b) for a,b in zip(observed['q'], old['q'])) < 1.e-6 and observed['s'] == old['s'], 'Only worldX may change')
                report['instance_changes'].append({**change, 'observed_world_transform_before': old, 'observed_world_transform_after': observed})
        after_actors = list(subsystem.get_all_level_actors()); after = actor_reader._snapshot_actors(u, after_actors, api)
        normalized = copy.deepcopy(after)
        for change in spec['instance_changes']:
            primitive = next(p for p in normalized[change['actor_path']]['primitives'] if p['path'] == change['component_path'])
            old = next(p for p in before[change['actor_path']]['primitives'] if p['path'] == change['component_path'])
            primitive['instances'][change['index']] = old['instances'][change['index']]
        require(normalized == before and inspector.persistent_components(u, after_actors) == before_components, 'Any state besides exact two instance transforms changed')
        effects_after = walls.effect_state(u, after_actors, visibility, inspector, api)
        require(effects_after == effects_before and primitive_queries(u, after_actors) == queries_before, 'Collision/global/camera/lighting/PP state changed')
        require({p: visibility.material_state(u, u.load_asset(p), reader) for p in material_before} == material_before and
                {p: inspector.material_state(u, u.load_asset(p), reader) for p in source_before} == source_before, 'Material states changed')
        final_native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(final_native.get('success') and final_native['composite_blake3'] == COMPOSITE and final_native['original_layer_blake3'] == ORIGINAL, 'Terrain changed')
        require(set(api._dirty(u)) == set(spec['expected_dirty_after']), 'Only exact Q dressing package may become dirty')
        report.update(success=True, spec_sha256=SPEC_SHA256, input_sha256=INPUTS, dirty_before=[], dirty_after=api._dirty(u),
                      checked_instance_matrix_predecessors=checked_counts, actor_count=1408, changed_instance_count=2,
                      original1404_preserved=True, all_other_instances_and_components_preserved=True,
                      actor_snapshot_sha256_before=digest(before), actor_snapshot_sha256_after=digest(after),
                      normalized_actor_snapshot_sha256_after=digest(normalized), persistent_components_sha256=digest(before_components),
                      collision_query_settings_sha256=digest(queries_before), material_states_sha256=digest(material_before),
                      source40_material_states_sha256=digest(source_before), effects_before=effects_before, effects_after=effects_after,
                      composite_blake3=COMPOSITE, original_layer_blake3=ORIGINAL, thresholds_and_routes_unchanged=True,
                      actual_camera_pass_claimed=False, finished_utc=datetime.now(timezone.utc).isoformat(),
                      validation_pending=['same2016 capsule queries', 'actual Camera14 trace and full unchanged276 waypoint route with six reviewed images', 'one-package save and independent cold verification'])
        return report
    except Exception:
        report['traceback'] = traceback.format_exc()
        raise CaveCameraReliefError(report) from None


def main():
    parser = argparse.ArgumentParser(description='Pure diagnosed two-rock camera-relief spec generation only.')
    parser.add_argument('--write-spec', action='store_true'); args = parser.parse_args()
    require(args.write_spec, 'Explicit --write-spec required'); value = build_spec()
    with SPEC.open('xb') as stream:
        stream.write(serialized(value))
    print(json.dumps({'spec_sha256': digest(value), 'changed_instances': 2, 'world_delta_cm': [50, 0, 0], 'save_called': False}, indent=2))


if __name__ == '__main__':
    main()
