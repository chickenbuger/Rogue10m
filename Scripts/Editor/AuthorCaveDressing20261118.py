"""Pure dressing-plan generator and explicitly invoked, unsaved editor author.

Import is inert. The author adds only Q/Z dressing actors, ISMs and lights.
It never changes terrain, existing actors, materials, cameras, saves or assets.
All visible meshes ignore Pawn and block only Camera/Visibility queries.
"""
from __future__ import annotations

import argparse
from collections import OrderedDict
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import traceback

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'Feature/doc/evidence/caves20261118'
CAVE_PLAN = BASE / 'plan.json'
CAVE_SHA256 = '39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74'
PLAN = BASE / 'dressing-plan.json'
PLAN_SHA256 = '4177a9e1b8544e16cd61217cf64fd01a5c47da8e6ef3ee84dea25228107449db'
TARGET = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
OWN = '/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/'
TAG = 'Rogue10m.Caves20261118.Dressing'
LABELS = {s: 'CV18_' + s + '_Dressing' for s in ('Q', 'Z')}
ROLES = {'rock', 'timber', 'metal', 'lamp_emission_Q', 'lamp_emission_Z'}
MESHES = {'cube': '/Engine/BasicShapes/Cube',
          'boulder_medium': '/Game/Stylized_Village/Assets/Stones/Boulder/SM_Boulder_Medium_01'}


class CaveDressingError(RuntimeError):
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
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':'), allow_nan=False) + '\n').encode('utf-8')


def digest(value):
    return hashlib.sha256(serialized(value).rstrip(b'\n')).hexdigest()


def scalar(seed, lo, hi):
    value = int(hashlib.sha256(seed.encode('utf-8')).hexdigest()[:12], 16) / float(0xffffffffffff)
    return lo + (hi - lo) * value


def row(label, mesh, material, center, size, rotation=(0., 0., 0.), **extra):
    require(mesh in MESHES and material in ROLES, 'Unapproved dressing mesh/material role')
    require(all(math.isfinite(v) for v in (*center, *size, *rotation)) and min(size) > 0, 'Invalid dressing geometry')
    return {'label': label, 'mesh': mesh, 'material_role': material, 'bounds_center_cm': list(center),
            'dimensions_cm': list(size), 'rotation_pitch_yaw_roll_deg': list(rotation),
            'collision_role': 'camera_visibility_query_only_pawn_ignore', 'cast_shadow': True, **extra}


def world(site, cell, z=8610.):
    return [site['origin_cm'][0] + (cell[0] + .5) * 500.,
            site['origin_cm'][1] + (cell[1] + .5) * 500., z]


def build_dressing_plan():
    raw = CAVE_PLAN.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == CAVE_SHA256, 'Immutable structural plan changed')
    cave = json.loads(raw)
    baseline_meshes = json.loads((BASE / 'baseline-live/baseline-materials.json').read_text(encoding='utf-8-sig'))['meshes']
    sites = []
    for site in cave['sites']:
        key = site['site_id']
        instances = []
        for wall in (c for c in site['components'] if c['label'].startswith(key + '_Wall_')):
            normal_axis = 0 if wall['dimensions_cm'][0] == 80. else 1
            tangent_axis = 1 - normal_axis
            for tier in (0, 1):
                seed = wall['label'] + str(tier)
                size = [0., 0., scalar(seed + 'h', 325., 355.)]
                size[normal_axis] = scalar(seed + 'n', 125., 150.)
                size[tangent_axis] = scalar(seed + 'w', 520., 590.)
                center = list(wall['location_cm'])
                center[normal_axis] += scalar(seed + 'o', -5., 5.)
                center[tangent_axis] += scalar(seed + 't', -20., 20.)
                center[2] = (8750. if tier == 0 else 9060.) + scalar(seed + 'z', -12., 12.)
                rotation = (0., 0., scalar(seed + 'r', -6., 6.)) if normal_axis == 0 else (scalar(seed + 'r', -6., 6.), 0., 0.)
                protrusion = size[normal_axis] / 2 + abs(center[normal_axis] - wall['location_cm'][normal_axis]) - 40.
                require(protrusion <= 45., 'Rock skin exceeds wall-face visual clearance')
                instances.append(row(key + '_Rock_' + seed, 'boulder_medium', 'rock', center, size, rotation,
                                     source_wall=wall['label'], wall_normal_axis=normal_axis,
                                     maximum_wall_face_protrusion_cm=protrusion))
        for name, rect in sorted(site['rooms'].items()):
            x0, x1, y0, y1 = rect
            center = world(site, ((x0 + x1) / 2., (y0 + y1) / 2.), 9140.)
            instances.append(row(key + '_CeilingRock_' + name, 'boulder_medium', 'rock', center,
                                 (650., 680., 150.), (0., scalar(key + name, -25., 25.), 0.),
                                 minimum_visible_bottom_cm=9065.))

        cells = {tuple(c) for c in site['floor_cells']}
        graph = {c: [] for c in cells}
        for a, b in site['graph_edges']:
            graph[tuple(a)].append(tuple(b)); graph[tuple(b)].append(tuple(a))
        entry = tuple(site['entry_cell'])
        frames = [(entry, 'Y')]
        straight = []
        for c, neighbors in graph.items():
            if len(neighbors) == 2 and neighbors[0][0] == neighbors[1][0] == c[0]:
                straight.append((c, 'Y'))
            elif len(neighbors) == 2 and neighbors[0][1] == neighbors[1][1] == c[1]:
                straight.append((c, 'X'))
        for c, axis in sorted(straight, key=lambda v: scalar(key + str(v[0]), 0., 1.)):
            if min(math.dist(c, f[0]) for f in frames) >= 2.:
                frames.append((c, axis))
            if len(frames) == 4:
                break
        require(len(frames) == 4, 'Four separated support frames required')
        for i, (c, axis) in enumerate(frames):
            center = world(site, c)
            perpendicular = 0 if axis == 'Y' else 1
            for side in (-1, 1):
                post = list(center); post[perpendicular] += side * 190.; post[2] = 8890.
                instances.append(row(f'{key}_Frame{i}_Post{side}', 'cube', 'timber', post, (28., 28., 560.)))
            size = (428., 40., 40.) if axis == 'Y' else (40., 428., 40.)
            instances.append(row(f'{key}_Frame{i}_Lintel', 'cube', 'timber', (center[0], center[1], 9150.), size,
                                 passage_clear_width_cm=352., visible_bottom_cm=9130.))

        selected = [entry, tuple(site['book']['reservation_cell']), tuple(site['secret'][-1])]
        while len(selected) < 6:
            selected.append(max((c for c in sorted(cells) if c not in selected),
                                key=lambda c: (min(math.dist(c, p) for p in selected), c)))
        lights = []
        color = [255, 193, 116] if key == 'Q' else [155, 204, 255]
        for i, c in enumerate(selected):
            center = world(site, c)
            prefix = f'{key}_Lantern{i}'
            instances.extend([
                row(prefix + '_Bulb', 'cube', 'lamp_emission_' + key, (center[0], center[1], 9110.), (24., 24., 24.)),
                row(prefix + '_Cap', 'cube', 'metal', (center[0], center[1], 9128.), (40., 40., 12.)),
                row(prefix + '_Hanger', 'cube', 'metal', (center[0], center[1], 9164.), (8., 8., 60.)),
            ])
            lights.append({'label': prefix + '_PointLight', 'location_cm': [center[0], center[1], 9010.],
                           'color_srgb': color, 'intensity_lumens': 12000., 'attenuation_radius_cm': 2800.,
                           'cast_shadows': False, 'mobility': 'Movable', 'tick_enabled': False})

        def chest(name, center, small=False):
            x, y, z = center
            w, d, h = (110., 75., 65.) if small else (180., 110., 100.)
            instances.extend([
                row(name + '_Body', 'cube', 'timber', (x, y, z + h / 2), (w, d, h)),
                row(name + '_Lid', 'cube', 'timber', (x, y, z + h + 10), (w + 10, d + 8, 20.)),
                row(name + '_BandL', 'cube', 'metal', (x - w * .3, y, z + h / 2 + 6), (14., d + 12, h + 20)),
                row(name + '_BandR', 'cube', 'metal', (x + w * .3, y, z + h / 2 + 6), (14., d + 12, h + 20)),
                row(name + '_Lock', 'cube', 'metal', (x, y - d / 2 - 8, z + h * .65), (25., 12., 28.)),
            ])

        if key == 'Q':
            x, y, _ = world(site, (9., 10.2))
            chest(key + '_VaultChest_VisualOnly', (x, y, 8610.))
            x, y, _ = world(site, (6.5, 1.3))
            for side in (-1, 1):
                instances.append(row(key + '_TrainingRack_Post' + str(side), 'cube', 'timber',
                                     (x + side * 110., y, 8700.), (25., 25., 180.)))
            instances.append(row(key + '_TrainingRack_Rail', 'cube', 'timber', (x, y, 8780.), (245., 25., 28.)))
        else:
            x, y, _ = world(site, (12.15, 2.))
            instances.append(row(key + '_Worktable_Top', 'cube', 'timber', (x, y, 8790.), (115., 210., 25.)))
            for a in (-1, 1):
                for b in (-1, 1):
                    instances.append(row(f'{key}_Worktable_Leg{a}{b}', 'cube', 'timber',
                                         (x + a * 40., y + b * 82., 8700.), (22., 22., 180.)))
            chest(key + '_WorkshopChest_VisualOnly', (x, y, 8802.5), small=True)

        # A geometric relic, with no item ID, learning, discovery trigger or interaction.
        secret = site['rooms']['secret']
        x, y, _ = world(site, ((secret[0] + secret[1]) / 2., secret[3] + .2))
        instances.extend([
            row(key + '_SecretRelic_Plinth', 'cube', 'metal', (x, y, 8642.5), (90., 90., 65.)),
            row(key + '_SecretRelic_Pages', 'cube', 'metal', (x, y, 8683.), (62., 42., 12.), (0., 15., 0.)),
            row(key + '_SecretRelic_Cover', 'cube', 'timber', (x, y, 8691.), (68., 48., 5.), (0., 15., 0.)),
        ])
        maximum_protrusion = max(i.get('maximum_wall_face_protrusion_cm', 0.) for i in instances)
        # Props must remain on their intended room floors, inside the inner
        # wall faces. This catches outside-room placement before authoring.
        for item in instances:
            label = item['label']
            if '_VaultChest_' in label:
                room = 'vault'
            elif '_TrainingRack_' in label:
                room = 'training'
            elif '_Worktable_' in label or '_WorkshopChest_' in label:
                room = 'workshop'
            elif '_SecretRelic_' in label:
                room = 'secret'
            else:
                continue
            x0, x1, y0, y1 = site['rooms'][room]
            lo = (site['origin_cm'][0] + 500. * x0 + 40., site['origin_cm'][1] + 500. * y0 + 40.)
            hi = (site['origin_cm'][0] + 500. * (x1 + 1) - 40., site['origin_cm'][1] + 500. * (y1 + 1) - 40.)
            angle = math.radians(item['rotation_pitch_yaw_roll_deg'][1])
            sx, sy = item['dimensions_cm'][:2]
            half = ((abs(math.cos(angle)) * sx + abs(math.sin(angle)) * sy) / 2.,
                    (abs(math.sin(angle)) * sx + abs(math.cos(angle)) * sy) / 2.)
            require(all(lo[j] <= item['bounds_center_cm'][j] - half[j] and
                        item['bounds_center_cm'][j] + half[j] <= hi[j] for j in (0, 1)),
                    'Visual prop extends beyond its room inner wall faces: ' + label)
        nearest_light = max(min(math.dist(world(site, c)[:2], light['location_cm'][:2]) for light in lights) for c in cells)
        require(nearest_light < 2800., 'A floor cell lies outside all planned light radii')
        require(len({i['label'] for i in instances}) == len(instances), 'Duplicate dressing label')
        sites.append({'site_id': key, 'actor_label': LABELS[key], 'folder': 'Caves20261118/' + key + '/Dressing',
                      'instances': instances, 'lights': lights, 'instance_count': len(instances), 'light_count': len(lights),
                      'maximum_wall_face_protrusion_cm': maximum_protrusion,
                      'minimum_visual_straight_passage_cm': 420. - 2 * maximum_protrusion,
                      'minimum_ceiling_rock_bottom_cm': 9065., 'timber_frame_clear_width_cm': 352.,
                      'maximum_floor_cell_distance_to_light_cm': nearest_light,
                      'all_room_edge_props_inside_inner_wall_faces': True,
                      'prop_semantics': 'Purely visual; unconfirmed book/reward IDs remain unconnected.'})
    return {'schema': 'Rogue10m.CaveDressingPlan.20261118.v1', 'map': TARGET,
            'status': 'planned_not_authored', 'structural_plan_sha256': CAVE_SHA256,
            'tag': TAG, 'source_meshes': {k: baseline_meshes[k] for k in MESHES},
            'material_roles': sorted(ROLES), 'source_actors_preserved': 1404,
            'expected_structure_labels': ['CV18_Q_Structure', 'CV18_Z_Structure'],
            'new_actor_count': 2, 'instance_count': sum(s['instance_count'] for s in sites),
            'light_count': sum(s['light_count'] for s in sites), 'sites': sites,
            'collision_contract': 'QueryOnly: Camera and Visibility block; Pawn and all other channels ignore. No navigation contribution.',
            'camera_acceptance': 'Actual runtime observation/rendered inspection required; pure widths and planned collision are not a pass.',
            'materials_or_assets_created_by_helper': False, 'packages_saved_by_helper': False}


def owned_package(name):
    return (name == TARGET or name.startswith(OWN) or
            any(name.startswith(prefix + TARGET[6:] + '/') for prefix in ('/Game/__ExternalActors__/', '/Game/__ExternalObjects__/')))


def configure(u, component):
    component.set_mobility(u.ComponentMobility.STATIC)
    component.set_editor_property('generate_overlap_events', False)
    component.set_collision_object_type(u.CollisionChannel.ECC_WORLD_STATIC)
    component.set_collision_profile_name('Custom', False)
    component.set_collision_response_to_all_channels(u.CollisionResponseType.ECR_IGNORE)
    for channel in (u.CollisionChannel.ECC_CAMERA, u.CollisionChannel.ECC_VISIBILITY):
        component.set_collision_response_to_channel(channel, u.CollisionResponseType.ECR_BLOCK)
    component.set_collision_enabled(u.CollisionEnabled.QUERY_ONLY)
    component.set_editor_property('can_ever_affect_navigation', False)
    component.set_component_tick_enabled(False)
    component.set_visibility(True, False); component.set_hidden_in_game(False, False); component.set_cast_shadow(True)
    require(component.get_collision_enabled() == u.CollisionEnabled.QUERY_ONLY and
            component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN) == u.CollisionResponseType.ECR_IGNORE and
            all(component.get_collision_response_to_channel(c) == u.CollisionResponseType.ECR_BLOCK
                for c in (u.CollisionChannel.ECC_CAMERA, u.CollisionChannel.ECC_VISIBILITY)) and
            not component.get_editor_property('generate_overlap_events') and
            not component.get_editor_property('can_ever_affect_navigation') and not component.is_component_tick_enabled(),
            'Camera-only dressing settings readback differs')


def native_transform(u, value, bounds, dressing):
    size = [b - a for a, b in zip(bounds['bounds_min_cm'], bounds['bounds_max_cm'])]
    center = [(a + b) / 2 for a, b in zip(bounds['bounds_min_cm'], bounds['bounds_max_cm'])]
    scale = [a / b for a, b in zip(value['dimensions_cm'], size)]
    result = u.Transform()
    result.scale3d = u.Vector(*scale)
    p, y, r = value['rotation_pitch_yaw_roll_deg']
    result.rotation = u.Rotator(pitch=p, yaw=y, roll=r).quaternion()
    result.translation = u.Vector(0, 0, 0)
    fn = getattr(result, 'transform_location', None)
    offset = fn(u.Vector(*center)) if callable(fn) else u.MathLibrary.transform_location(result, u.Vector(*center))
    result.translation = u.Vector(*(a - b for a, b in zip(value['bounds_center_cm'], dressing._xyz(offset))))
    return result


def author_cave_dressing(*, allow_editor_mutation=False, expected_plan_sha256,
                        expected_composite_blake3, expected_original_layer_blake3,
                        expected_dirty_packages, material_paths_by_role, expected_actor_paths=None):
    report = {'schema': 'Rogue10m.CaveDressingAuthor.20261118.v1', 'success': False,
              'started_utc': datetime.now(timezone.utc).isoformat(), 'mutation_started': False,
              'assets_saved': False, 'save_called': False, 'landscape_mutated': False,
              'materials_mutated': False, 'existing_actor_changes': [], 'automatic_retry': False,
              'automatic_rollback': False, 'partial_work_deleted': False,
              'created_actor_paths': [], 'packages': [], 'sites': [], 'instance_count': 0, 'light_count': 0}
    try:
        require(allow_editor_mutation is True, 'Explicit allow_editor_mutation=True required')
        require(expected_plan_sha256 == PLAN_SHA256 and hashlib.sha256(PLAN.read_bytes()).hexdigest() == PLAN_SHA256,
                'Reviewed immutable dressing plan changed')
        require(hashlib.sha256(CAVE_PLAN.read_bytes()).hexdigest() == CAVE_SHA256, 'Structural plan changed')
        plan = json.loads(PLAN.read_text(encoding='utf-8'))
        require(plan['schema'] == 'Rogue10m.CaveDressingPlan.20261118.v1' and plan['map'] == TARGET and
                plan['structural_plan_sha256'] == CAVE_SHA256 and plan['new_actor_count'] == 2 and
                {s['actor_label'] for s in plan['sites']} == set(LABELS.values()), 'Exact two-site reviewed plan required')
        require(isinstance(expected_dirty_packages, (list, tuple)) and len(expected_dirty_packages) == len(set(expected_dirty_packages))
                and all(owned_package(n) for n in expected_dirty_packages), 'Exact owned dirty-package set required')
        require(isinstance(material_paths_by_role, dict) and set(material_paths_by_role) == ROLES and
                all(isinstance(p, str) and p.startswith(OWN) for p in material_paths_by_role.values()),
                'Exact five project-owned ISM material roles required')
        import unreal as u
        api = module('cave_dressing_api', 'Scripts/Editor/NaturalWorldDressing20261117.py')
        inspector = module('cave_dressing_inspector', 'Scripts/Editor/InspectCaveSites20261118.py')
        reader = module('cave_dressing_actor_reader', 'Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
        api._guard(u)
        require(set(api._dirty(u)) == set(expected_dirty_packages), 'Dirty set differs before dressing')
        report['dirty_before'] = api._dirty(u); report['binding_preflight'] = api._binding_preflight(u)
        require(all(callable(getattr(u.ActorComponent, name, None)) for name in ('set_component_tick_enabled', 'is_component_tick_enabled')),
                'Public component tick bindings unavailable')
        require(hasattr(u.LightUnits, 'LUMENS'), 'Public lumen light units unavailable')
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native.get('success') and native.get('landscape_reports_up_to_date') and native.get('layer_count') == 2 and
                native.get('component_count') == native.get('collision_count') == 45 and
                native.get('composite_blake3') == expected_composite_blake3 and
                native.get('original_layer_blake3') == expected_original_layer_blake3, 'Settled exact terrain hashes required')
        baseline = json.loads((BASE / 'baseline-live/baseline-actors.json').read_text(encoding='utf-8'))
        require(baseline['actor_count'] == len(baseline['actors']) == 1404 and
                digest(baseline['actors']) == baseline['snapshot_sha256'] and
                digest(baseline['persistent_components']) == baseline['component_snapshot_sha256'], 'Original1404 baseline signature mismatch')
        actor_subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
        actors = list(actor_subsystem.get_all_level_actors())
        actor_paths = {path(a) for a in actors}; original_paths = set(baseline['actors'])
        extras = [a for a in actors if path(a) not in original_paths]
        require(len(actors) == len(actor_paths) == 1406 and original_paths <= actor_paths and len(extras) == 2 and
                {a.get_actor_label() for a in extras} == set(plan['expected_structure_labels']) and
                not any(a.get_actor_label() in set(LABELS.values()) or TAG in list(map(str, a.tags)) for a in actors),
                'Exact original1404 plus two cave structures required; no overwrite/retry')
        if expected_actor_paths is not None:
            require(len(expected_actor_paths) == 1406 and set(expected_actor_paths) == actor_paths, 'Caller actor allowlist differs')
        before = reader._snapshot_actors(u, actors, api)
        before_components = inspector.persistent_components(u, actors)
        require({p: before[p] for p in original_paths} == baseline['actors'] and
                {p: before_components[p] for p in original_paths} == baseline['persistent_components'], 'An original actor/component changed')
        report['all1406_snapshot_sha256_before'] = digest(before)
        report['all1406_components_sha256_before'] = digest(before_components)
        meshes, materials = {}, {}
        mesh_editor = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
        require(mesh_editor is not None, 'Read-only static-mesh collision inspector unavailable')
        report['source_mesh_collision'] = {}
        for key, package in MESHES.items():
            mesh = u.load_asset(package); require(isinstance(mesh, u.StaticMesh), 'Missing existing mesh: ' + package)
            bounds = mesh.get_bounding_box(); expected = plan['source_meshes'][key]
            require(max(abs(a - b) for a, b in zip(api._xyz(bounds.min), expected['bounds_min_cm'])) < .01 and
                    max(abs(a - b) for a, b in zip(api._xyz(bounds.max), expected['bounds_max_cm'])) < .01, 'Source mesh bounds changed')
            if key != 'cube':
                require(hashlib.sha256((ROOT / expected['disk']['file']).read_bytes()).hexdigest() == expected['disk']['sha256'],
                        'Source boulder file changed')
            simple = int(mesh_editor.get_simple_collision_count(mesh)); convex = int(mesh_editor.get_convex_collision_count(mesh))
            require(simple > 0 or convex > 0, 'Camera-blocking source mesh has no verified simple/convex collision: ' + key)
            report['source_mesh_collision'][key] = {'simple': simple, 'convex': convex, 'complexity': str(mesh_editor.get_collision_complexity(mesh))}
            meshes[key] = mesh
        for role, package in material_paths_by_role.items():
            value = u.load_asset(package); require(isinstance(value, u.MaterialInterface), 'Missing owned material: ' + role)
            base, seen = value, set()
            while isinstance(base, u.MaterialInstance):
                require(path(base) not in seen, 'Material parent cycle'); seen.add(path(base)); base = base.get_editor_property('parent')
            require(isinstance(base, u.Material) and base.get_editor_property('used_with_instanced_static_meshes') is True,
                    'Role material must already support ISMs: ' + role)
            materials[role] = value
        # Prepare all native transforms before the first actor is created.
        transforms = {r['label']: native_transform(u, r, plan['source_meshes'][r['mesh']], api)
                      for s in plan['sites'] for r in s['instances']}
        require(set(api._dirty(u)) == set(expected_dirty_packages), 'Read-only dressing preflight dirtied a package')
        sds = u.get_engine_subsystem(u.SubobjectDataSubsystem); require(sds is not None, 'Public component authoring unavailable')
        report.update(plan_sha256=PLAN_SHA256, structural_plan_sha256=CAVE_SHA256, material_paths_by_role=material_paths_by_role)
        with u.ScopedEditorTransaction('QZ cave rockwork lighting and visual props 20261118'):
            report['mutation_started'] = True
            for site in plan['sites']:
                actor = actor_subsystem.spawn_actor_from_class(u.Actor, u.Vector(0, 0, 0), u.Rotator(pitch=0, yaw=0, roll=0), False)
                require(actor is not None, 'Dressing actor creation failed')
                report['created_actor_paths'].append(path(actor)); report['packages'].append(actor.get_package().get_name())
                actor.set_actor_label(site['actor_label'], True); actor.set_folder_path(site['folder'])
                actor.set_editor_property('tags', [u.Name(TAG), u.Name(TAG + '.' + site['site_id'])])
                actor.set_editor_property('is_spatially_loaded', False); actor.set_actor_hidden_in_game(False)
                actor.set_actor_tick_enabled(False)
                parent, root_component = api._root_handle(u, sds, actor)
                actor.set_actor_transform(u.Transform(), False, True)
                sr = {'site_id': site['site_id'], 'actor_path': path(actor), 'package': actor.get_package().get_name(),
                      'components': [], 'lights': [], 'always_loaded': not actor.get_editor_property('is_spatially_loaded'),
                      'tick_enabled': actor.is_actor_tick_enabled()}
                report['sites'].append(sr)
                bins = OrderedDict()
                for r in site['instances']:
                    bins.setdefault((r['mesh'], r['material_role']), []).append(r)
                for i, ((mesh_key, role), rows) in enumerate(bins.items()):
                    _, c = api._new_component(u, sds, parent, u.InstancedStaticMeshComponent,
                                             site['site_id'] + '_CaveDressingISM_' + str(i).zfill(2))
                    # Apply overrides before assigning a vendor mesh so its
                    # default material never receives a new ISM-usage request.
                    c.set_material(0, materials[role]); c.set_static_mesh(meshes[mesh_key]); c.set_material(0, materials[role])
                    c.set_editor_property('component_tags', [u.Name(TAG), u.Name('CaveVisual.CameraQueryOnly')])
                    configure(u, c)
                    cr = {'path': path(c), 'mesh': path(meshes[mesh_key]), 'material': path(c.get_material(0)),
                          'role': role, 'collision_enabled': str(c.get_collision_enabled()),
                          'pawn_response': str(c.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)),
                          'camera_response': str(c.get_collision_response_to_channel(u.CollisionChannel.ECC_CAMERA)), 'instances': []}
                    sr['components'].append(cr)
                    for j, r in enumerate(rows):
                        transform = transforms[r['label']]
                        require(c.add_instance(transform, world_space=True) == j, 'Dressing instance index mismatch')
                        observed = api._instance_transform(u, c, j); api._same_transform(api._transform(transform), observed)
                        cr['instances'].append({'index': j, 'label': r['label'], 'world_transform': observed})
                    report['instance_count'] += len(rows)
                for i, light in enumerate(site['lights']):
                    _, c = api._new_component(u, sds, parent, u.PointLightComponent,
                                             site['site_id'] + '_CavePointLight_' + str(i).zfill(2))
                    c.set_mobility(u.ComponentMobility.MOVABLE)
                    c.set_editor_property('relative_location', u.Vector(*light['location_cm']))
                    c.set_editor_property('intensity_units', u.LightUnits.LUMENS)
                    c.set_editor_property('use_inverse_squared_falloff', True)
                    c.set_editor_property('intensity', light['intensity_lumens'])
                    c.set_editor_property('attenuation_radius', light['attenuation_radius_cm'])
                    red, green, blue = light['color_srgb']
                    c.set_editor_property('light_color', u.Color(r=red, g=green, b=blue, a=255))
                    c.set_editor_property('cast_shadows', False); c.set_component_tick_enabled(False)
                    c.set_visibility(True, False); c.set_hidden_in_game(False, False)
                    require(not c.is_component_tick_enabled() and not c.get_editor_property('cast_shadows') and
                            c.get_editor_property('intensity_units') == u.LightUnits.LUMENS and
                            abs(c.get_editor_property('intensity') - light['intensity_lumens']) < .01 and
                            abs(c.get_editor_property('attenuation_radius') - light['attenuation_radius_cm']) < .01,
                            'Point-light settings readback differs')
                    actual_color = c.get_editor_property('light_color')
                    actual_rgb = [int(actual_color.r), int(actual_color.g), int(actual_color.b)]
                    require(actual_rgb == light['color_srgb'] and int(actual_color.a) == 255,
                            'Point-light actual RGB channels differ from reviewed plan')
                    sr['lights'].append({'path': path(c), 'label': light['label'], 'location_cm': api._xyz(c.get_editor_property('relative_location')),
                                         'intensity_lumens': float(c.get_editor_property('intensity')),
                                         'attenuation_radius_cm': float(c.get_editor_property('attenuation_radius')),
                                         'color_srgb': actual_rgb, 'cast_shadows': False, 'tick_enabled': False})
                    report['light_count'] += 1
                require(sr['always_loaded'] and not sr['tick_enabled'], 'Dressing actor must be always loaded and not ticking')
        after_actors = list(actor_subsystem.get_all_level_actors())
        require(len(after_actors) == 1408 and {path(a) for a in after_actors} == actor_paths | set(report['created_actor_paths']),
                'Only the two dressing actors may be added')
        retained = [a for a in after_actors if path(a) in actor_paths]
        after = reader._snapshot_actors(u, retained, api); after_components = inspector.persistent_components(u, retained)
        require(before == after and before_components == after_components, 'An existing actor/component changed during dressing')
        native_after = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native_after.get('success') and native_after.get('composite_blake3') == expected_composite_blake3 and
                native_after.get('original_layer_blake3') == expected_original_layer_blake3, 'Dressing changed terrain')
        dirty_after = api._dirty(u)
        allowed = set(expected_dirty_packages) | set(report['packages']) | {TARGET}
        extra = {n for n in dirty_after if n not in allowed}
        require(all(n.startswith('/Game/__ExternalObjects__/' + TARGET[6:] + '/') for n in extra), 'Unexpected foreign dirty package after dressing')
        require(report['instance_count'] == plan['instance_count'] and report['light_count'] == plan['light_count'] == 12,
                'Dressing/light count differs from reviewed plan')
        report.update(success=True, actor_count_before=1406, actor_count_after=1408,
                      all1406_existing_actor_and_component_snapshots_equal=True, original1404_baseline_preserved=True,
                      all1406_snapshot_sha256_after=digest(after), all1406_components_sha256_after=digest(after_components),
                      dirty_after=dirty_after, new_folder_external_packages=sorted(extra),
                      finished_utc=datetime.now(timezone.utc).isoformat(),
                      validation_pending=['Actual Camera/Pawn queries', 'rendered cave lighting/rockwork/camera views',
                                          'bounded save', 'independent cold reload'])
        return report
    except Exception:
        report['traceback'] = traceback.format_exc()
        raise CaveDressingError(report) from None


def main():
    parser = argparse.ArgumentParser(description='Pure-plan generation only; editor mutation requires explicit imported function call.')
    parser.add_argument('--write-plan', action='store_true')
    args = parser.parse_args()
    require(args.write_plan, 'Only --write-plan is supported by the command-line entry point')
    plan = build_dressing_plan()
    with PLAN.open('xb') as stream:
        stream.write(serialized(plan))
    print(json.dumps({'plan_sha256': hashlib.sha256(serialized(plan)).hexdigest(),
                      'actors': plan['new_actor_count'], 'instances': plan['instance_count'], 'lights': plan['light_count'],
                      'sites': [{k: v for k, v in s.items() if k not in ('instances', 'lights')} for s in plan['sites']]}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
