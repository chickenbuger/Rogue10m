"""Replace only the candidate's thin road visuals with 29 projected decals.

Import is inert. The editor owner calls preflight(), reviews it, then explicitly
opts into repair_road_visuals(). Existing road actors are retained and hidden;
Landscape, collision, buildings, markers, StartHub and vendor assets are untouched.
The new material belongs to the candidate's OWN namespace. No SaveAll or cleanup.
"""
from __future__ import annotations

import datetime
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
from collections import Counter


PROJECT = Path(__file__).resolve().parents[2]
MAP = '/Game/Rogue10m/Maps/Production/WorldLayout20261112'
OWN = '/Game/Rogue10m/World/Production/WorldLayout20261112'
BASE_TAG = 'Rogue10m.WorldLayout20261112'
SCENE_TAG = BASE_TAG + '.Scene'
REPAIR_TAG = BASE_TAG + '.RoadDecal'
SOURCE_PREFIX = 'WL26_Road_'
DECAL_PREFIX = 'WL26_RoadDecal_'
SOURCE_MATERIAL = '/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_EarthPath'
SOURCE_MESH = '/Engine/BasicShapes/Cube'
WIDTH_CM = 760.0
WORLD_UV_SCALE = 0.002
OPACITY = 0.9
ROUGHNESS = 0.9
PROJECTION_Z_MARGIN_CM = 300.0


def _module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if not spec or not spec.loader:
        raise RuntimeError('Missing pure planning module: ' + str(path))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def _run_paths(run_label):
    if not re.fullmatch('[A-Za-z0-9_-]{1,48}', run_label):
        raise ValueError('Use a fresh alphanumeric evidence label')
    return (OWN + '/Materials/RoadRepair20261112/M_RoadDecal_' + run_label,
            PROJECT / 'Feature/doc/evidence/world-map-production-20261112' / run_label / 'road-decal-repair.json')


def projection_plan():
    """Pure world-centimetre plan. DecalSize is UE's local half extent.

    DecalComponent.cpp::CalcBounds uses FBoxSphereBounds(0, DecalSize,...).
    At pitch=-90/yaw=edge angle, local X points down, local Z follows the
    edge and local Y spans its width. The sample range includes both ±380cm
    edges, not just a centerline average that could miss terrain crossfall.
    """
    terrain = _module('road_repair_terrain_20261112', PROJECT / 'Scripts/WorldLayoutTerrain20261112.py')
    scene = _module('road_repair_scene_20261112', PROJECT / 'Scripts/Editor/AuthorWorldLayoutScene20261112.py')
    source_plan = scene.build_placement_plan()
    expected_roads = {row['label']: row for row in source_plan['placements']
                      if row['kind'] == 'mesh' and row['label'].startswith(SOURCE_PREFIX)}
    edges = tuple(terrain.roadsegments())
    if len(edges) != 29 or not expected_roads:
        raise RuntimeError('Unexpected 29-edge/source-road plan')
    decals = []
    for edge in edges:
        a, b = edge['a'], edge['b']
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        if not math.isfinite(length) or length <= 0:
            raise RuntimeError('Invalid edge geometry: ' + edge['id'])
        nx, ny = -dy / length, dx / length
        heights = []
        count = max(1, math.ceil(length / 200.0))
        for i in range(count + 1):
            t = i / count
            for offset in (-WIDTH_CM / 2, 0.0, WIDTH_CM / 2):
                x, y = a[0] + dx * t + nx * offset, a[1] + dy * t + ny * offset
                heights.append(float(terrain.height_at(x, y)))
        if not all(math.isfinite(z) for z in heights):
            raise RuntimeError('Nonfinite road terrain samples')
        low, high = min(heights) - PROJECTION_Z_MARGIN_CM, max(heights) + PROJECTION_Z_MARGIN_CM
        yaw = math.degrees(math.atan2(dy, dx))
        decals.append({'edge_id': edge['id'], 'from': edge['from'], 'to': edge['to'],
                       'label': DECAL_PREFIX + edge['id'], 'center': [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (low + high) / 2],
                       'rotation': {'pitch': -90.0, 'yaw': yaw, 'roll': 0.0},
                       'local_half_extents': [(high - low) / 2, WIDTH_CM / 2, length / 2],
                       'world_projection_z': [low, high], 'terrain_sample_min_max': [min(heights), max(heights)],
                       'world_length_cm': length, 'world_width_cm': WIDTH_CM, 'a': list(a), 'b': list(b)})
    return {'decals': decals, 'expected_source_roads': expected_roads, 'source_road_count': len(expected_roads),
            'new_decal_count': len(decals), 'width_cm': WIDTH_CM, 'world_uv_scale': WORLD_UV_SCALE}


def _allowed_package(name):
    name = str(name).split('.')[0]
    tail = MAP.removeprefix('/Game/')
    return (name == MAP or name.startswith(OWN + '/')
            or name.startswith('/Game/__ExternalActors__/' + tail + '/')
            or name.startswith('/Game/__ExternalObjects__/' + tail + '/'))


def _dirty(unreal):
    helper = unreal.EditorLoadingAndSavingUtils
    return {p.get_path_name(): p for p in list(helper.get_dirty_map_packages()) + list(helper.get_dirty_content_packages())}


def _asset_path(value):
    return value.get_path_name().split('.')[0] if value else None


def _resume_evidence(run_label, resume_from_label):
    """Only the observed zero-placement RoadA pin failure may be resumed."""
    if resume_from_label is None:
        return None
    if resume_from_label != 'RoadA' or run_label == resume_from_label:
        raise RuntimeError('Only RoadA may resume, with a fresh separate evidence label')
    material_path, source_report = _run_paths(resume_from_label)
    payload = source_report.read_bytes()
    data = json.loads(payload.decode('utf-8-sig'))
    if (data.get('status') != 'failed_partial_repair_requires_inspection_no_retry'
            or data.get('error') != "RuntimeError('Material connection failed: Input')"
            or data.get('map') != MAP or data.get('material') != material_path
            or data.get('new_decals') != [] or data.get('hidden_old_roads') != []
            or len(data.get('old_roads_before', [])) != 663):
        raise RuntimeError('RoadA evidence is not the observed pre-placement pin failure')
    return {'source_run_label': resume_from_label, 'source_report': str(source_report),
            'source_report_sha256': hashlib.sha256(payload).hexdigest(), 'material_path': material_path,
            'before': data['old_roads_before'], 'scope': 'exact_unsaved_owned_material_graph_only'}


def _check_resume_source(record):
    if record and hashlib.sha256(Path(record['source_report']).read_bytes()).hexdigest() != record['source_report_sha256']:
        raise RuntimeError('Original RoadA failure evidence changed; do not resume')


def _material_disk_path(material_path):
    if not material_path.startswith(OWN + '/Materials/RoadRepair20261112/'):
        raise RuntimeError('Partial material is outside the exact owned repair folder')
    return PROJECT / 'Content' / (material_path.removeprefix('/Game/') + '.uasset')


def _inspect_partial_material(unreal, material, texture):
    """Read-only fingerprint of the nine observed RoadA staging nodes."""
    if not isinstance(material, unreal.Material):
        raise RuntimeError('Exact partial RoadA Material is not loaded')
    if (material.get_editor_property('material_domain') != unreal.MaterialDomain.MD_DEFERRED_DECAL
            or material.get_editor_property('blend_mode') != unreal.BlendMode.BLEND_TRANSLUCENT):
        raise RuntimeError('Partial RoadA material domain/blend changed')
    lib = unreal.MaterialEditingLibrary
    nodes = list(lib.get_material_expressions(material))
    expected = {'MaterialExpressionWorldPosition': 1, 'MaterialExpressionComponentMask': 1,
                'MaterialExpressionScalarParameter': 3, 'MaterialExpressionMultiply': 2,
                'MaterialExpressionTextureSample': 1, 'MaterialExpressionVectorParameter': 1}
    counts = Counter(n.get_class().get_name() for n in nodes)
    if counts != expected or len(nodes) != 9:
        raise RuntimeError('Partial material is not the exact nine-node RoadA staging graph')
    scalar_values = {}
    mask = None
    for node in nodes:
        kind = node.get_class().get_name()
        if kind == 'MaterialExpressionScalarParameter':
            name = str(node.get_editor_property('parameter_name'))
            if name in scalar_values:
                raise RuntimeError('Duplicate partial-material scalar')
            scalar_values[name] = float(node.get_editor_property('default_value'))
        elif kind == 'MaterialExpressionVectorParameter':
            if str(node.get_editor_property('parameter_name')) != 'RoadTint':
                raise RuntimeError('Partial-material tint parameter changed')
        elif kind == 'MaterialExpressionTextureSample':
            if node.get_editor_property('texture') != texture:
                raise RuntimeError('Partial-material texture differs from inspected EarthPath diffuse')
        elif kind == 'MaterialExpressionComponentMask':
            mask = node
            if tuple(bool(node.get_editor_property(k)) for k in ('r', 'g', 'b', 'a')) != (True, True, False, False):
                raise RuntimeError('Partial-material world-XY mask changed')
    expected_scalars = {'WorldUVScale': WORLD_UV_SCALE, 'RoadOpacity': OPACITY, 'RoadRoughness': ROUGHNESS}
    if set(scalar_values) != set(expected_scalars) or any(abs(scalar_values[k] - v) > 1e-6 for k, v in expected_scalars.items()):
        raise RuntimeError('Partial-material scalar defaults changed')
    input_names = list(map(str, lib.get_material_expression_input_names(mask)))
    if len(input_names) != 1 or input_names[0] not in ('', 'None'):
        raise RuntimeError('Observed ComponentMask anonymous input schema changed: ' + repr(input_names))
    return {'expression_count': len(nodes), 'expression_classes': dict(counts),
            'component_mask_input_names': input_names, 'scalar_values': scalar_values,
            'expression_paths': [n.get_path_name() for n in nodes]}


def _prepare(run_label, resume_from_label=None):
    import unreal
    material_path, evidence = _run_paths(run_label)
    resume = _resume_evidence(run_label, resume_from_label)
    if resume:
        material_path = resume['material_path']
    if Path(unreal.Paths.project_dir()).resolve() != PROJECT.resolve():
        raise RuntimeError('Wrong project')
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or not world or _asset_path(world) != MAP:
        raise RuntimeError('An idle exact WP candidate is required; no map is switched')
    if world.get_world_settings().get_editor_property('world_partition') is None:
        raise RuntimeError('World Partition candidate required')
    dirty = _dirty(unreal)
    if any(not _allowed_package(p) for p in dirty):
        raise RuntimeError('Unrelated dirty packages: ' + json.dumps(list(dirty)))
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    current = list(actors.get_all_level_actors())
    if not any(isinstance(a, unreal.Landscape) for a in current):
        raise RuntimeError('Candidate Landscape is absent')
    if any(REPAIR_TAG in map(str, a.get_editor_property('tags')) or a.get_actor_label().startswith(DECAL_PREFIX) for a in current):
        raise RuntimeError('Prior/partial decal pass exists; inspect it instead of rerunning')
    if not resume and unreal.EditorAssetLibrary.does_asset_exist(material_path):
        raise RuntimeError('New material path already exists; no overwrite: ' + material_path)
    if evidence.exists():
        raise RuntimeError('Evidence label already exists: ' + str(evidence))
    plan = projection_plan()
    sources, snapshots = {}, []
    for actor in current:
        label = actor.get_actor_label()
        if not label.startswith(SOURCE_PREFIX):
            continue
        tags = set(map(str, actor.get_editor_property('tags')))
        if not {BASE_TAG, SCENE_TAG} <= tags or not isinstance(actor, unreal.StaticMeshActor):
            raise RuntimeError('Road-looking actor lacks exact scene ownership: ' + label)
        if label not in plan['expected_source_roads'] or label in sources:
            raise RuntimeError('Unexpected/duplicate road label: ' + label)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if _asset_path(component.get_editor_property('static_mesh')) != SOURCE_MESH:
            raise RuntimeError('Road source mesh differs: ' + label)
        if str(component.get_collision_profile_name()) != 'NoCollision':
            raise RuntimeError('Refusing to hide collision-bearing road: ' + label)
        p = actor.get_actor_location()
        expected = plan['expected_source_roads'][label]['center']
        if max(abs(v - expected[i]) for i, v in enumerate((p.x, p.y, p.z))) > .25:
            raise RuntimeError('Road transform changed since scene plan: ' + label)
        sources[label] = (actor, component)
        snapshots.append({'label': label, 'actor_path': actor.get_path_name(), 'component_path': component.get_path_name(),
                          'component_visible': bool(component.get_editor_property('visible')),
                          'actor_hidden_in_game': bool(actor.get_editor_property('hidden')),
                          'collision_profile': str(component.get_collision_profile_name())})
    missing = set(plan['expected_source_roads']) - set(sources)
    if missing or len(sources) != plan['source_road_count']:
        raise RuntimeError('Load all candidate road actors first; missing labels: ' + json.dumps(sorted(missing)))
    source_material = unreal.load_asset(SOURCE_MATERIAL)
    if not isinstance(source_material, unreal.MaterialInstanceConstant):
        raise RuntimeError('EarthPath MaterialInstanceConstant is unavailable')
    lib = unreal.MaterialEditingLibrary
    texture = lib.get_material_instance_texture_parameter_value(source_material, 'BaseColorTexture')
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('EarthPath BaseColorTexture did not resolve; no guessed replacement')
    vector_names = set(map(str, lib.get_vector_parameter_names(source_material)))
    tint = lib.get_material_instance_vector_parameter_value(source_material, 'ColorTint') if 'ColorTint' in vector_names else unreal.LinearColor(1, 1, 1, 1)
    partial_material, partial_facts = None, None
    if resume:
        _check_resume_source(resume)
        if _material_disk_path(material_path).exists():
            raise RuntimeError('RoadA is now saved on disk; unsaved-staging resume is no longer permitted')
        if material_path not in dirty:
            raise RuntimeError('Expected exact unsaved dirty RoadA material is absent')
        if sorted(snapshots, key=lambda x: x['label']) != sorted(resume['before'], key=lambda x: x['label']):
            raise RuntimeError('Road actors changed after the original zero-placement failure')
        partial_material = unreal.load_asset(material_path)
        if _asset_path(partial_material) != material_path:
            raise RuntimeError('Loaded partial material path differs')
        partial_facts = _inspect_partial_material(unreal, partial_material, texture)
    return {'unreal': unreal, 'world': world, 'levels': levels, 'actors': actors, 'plan': plan, 'sources': sources,
            'before': sorted(snapshots, key=lambda x: x['label']), 'texture': texture, 'tint': tint,
            'material_path': material_path, 'evidence': evidence, 'initial_dirty': list(dirty),
            'resume': resume, 'partial_material': partial_material, 'partial_material_facts': partial_facts}


def preflight(*, run_label='RoadA', resume_from_label=None):
    """Read-only ownership, counts, texture and projector-axis inspection."""
    p = _prepare(run_label, resume_from_label)
    return {'status': 'ready_for_owner_review', 'map': MAP, 'source_road_count': p['plan']['source_road_count'],
            'new_decal_count': 29, 'material_path': p['material_path'], 'earthpath_material': SOURCE_MATERIAL,
            'reused_diffuse_texture': _asset_path(p['texture']), 'width_cm': WIDTH_CM,
            'local_axis_contract': 'X=-worldZ depth; Y=perpendicular width; Z=edge direction length; DecalSize=half extents',
            'decals': p['plan']['decals'], 'initial_dirty_candidate_packages': p['initial_dirty'],
            'resume': p['resume'], 'partial_material_facts': p['partial_material_facts']}


def _make_material(p):
    unreal, lib = p['unreal'], p['unreal'].MaterialEditingLibrary
    folder, name = p['material_path'].rsplit('/', 1)
    if p['resume']:
        _check_resume_source(p['resume'])
        if _material_disk_path(p['material_path']).exists():
            raise RuntimeError('Partial material was saved after preflight; graph reset refused')
        material = p['partial_material']
        _inspect_partial_material(unreal, material, p['texture'])
        # Explicit resume applies only to the exact inspected, unsaved RoadA
        # staging Material. Keep that asset object; do not create MaterialB.
        lib.delete_all_material_expressions(material)
        if list(lib.get_material_expressions(material)):
            raise RuntimeError('Exact owned partial graph did not clear')
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError('New owned material creation failed')
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)

    def node(klass, x, y, **properties):
        value = lib.create_material_expression(material, klass, x, y)
        if not value:
            raise RuntimeError('Material node creation failed: ' + str(klass))
        for key, item in properties.items():
            value.set_editor_property(key, item)
        return value

    def wire(a, output, b, pin):
        if not lib.connect_material_expressions(a, output, b, pin):
            raise RuntimeError('Material connection failed: ' + pin)

    world = node(unreal.MaterialExpressionWorldPosition, -1050, -100)
    xy = node(unreal.MaterialExpressionComponentMask, -850, -100, r=True, g=True, b=False, a=False)
    scale = node(unreal.MaterialExpressionScalarParameter, -850, 120, parameter_name='WorldUVScale', default_value=WORLD_UV_SCALE)
    uv = node(unreal.MaterialExpressionMultiply, -650, -100)
    diffuse = node(unreal.MaterialExpressionTextureSample, -430, -100, texture=p['texture'],
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                   sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    tint = node(unreal.MaterialExpressionVectorParameter, -430, 150, parameter_name='RoadTint', default_value=p['tint'])
    tinted = node(unreal.MaterialExpressionMultiply, -180, -100)
    opacity = node(unreal.MaterialExpressionScalarParameter, -180, 190, parameter_name='RoadOpacity', default_value=OPACITY)
    roughness = node(unreal.MaterialExpressionScalarParameter, -180, 350, parameter_name='RoadRoughness', default_value=ROUGHNESS)
    mask_inputs = list(map(str, lib.get_material_expression_input_names(xy)))
    if len(mask_inputs) != 1 or mask_inputs[0] not in ('', 'None'):
        raise RuntimeError('ComponentMask anonymous input schema differs: ' + repr(mask_inputs))
    # UE5.8 reports ["None"] for this FName NAME_None input. Empty string is
    # the MaterialEditingLibrary argument, not the nonexistent pin "Input".
    for a, output, b, pin in ((world, '', xy, ''), (xy, '', uv, 'A'), (scale, '', uv, 'B'),
                              (uv, '', diffuse, 'UVs'), (diffuse, 'RGB', tinted, 'A'), (tint, 'RGB', tinted, 'B')):
        wire(a, output, b, pin)
    for expression, prop in ((tinted, unreal.MaterialProperty.MP_BASE_COLOR), (opacity, unreal.MaterialProperty.MP_OPACITY),
                              (roughness, unreal.MaterialProperty.MP_ROUGHNESS)):
        if not lib.connect_material_property(expression, '', prop):
            raise RuntimeError('Material output connection failed: ' + str(prop))
    lib.layout_material_expressions(material)
    errors = list(lib.recompile_material(material))  # UE5.8 header: TArray<FString>.
    if errors:
        raise RuntimeError('New decal material compile errors: ' + json.dumps(errors))
    if material.get_editor_property('material_domain') != unreal.MaterialDomain.MD_DEFERRED_DECAL:
        raise RuntimeError('New material domain readback failed')
    return material


def repair_road_visuals(*, allow_editor_mutation=False, editor_handoff='', run_label='RoadA', save_scene=True,
                        resume_from_label=None):
    """Create one new material/29 decals, then hide only exact old road meshes.

    No actor deletion, terrain mutation or collision change. Ambiguous failures
    leave evidence and partial work for owner inspection; no automatic retry.
    resume_from_label='RoadA' explicitly permits only the observed unsaved,
    nine-node, zero-placement pin failure. Use a new run_label for its report.
    """
    if allow_editor_mutation is not True or not isinstance(editor_handoff, str) or not editor_handoff.strip():
        raise RuntimeError('Explicit mutation opt-in and editor-owner handoff required')
    p = _prepare(run_label, resume_from_label)
    unreal, world = p['unreal'], p['world']
    report = {'status': 'started', 'map': MAP, 'tag': REPAIR_TAG, 'editor_handoff': editor_handoff,
              'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'material': p['material_path'],
              'source_material_read_only': SOURCE_MATERIAL, 'diffuse_texture_read_only': _asset_path(p['texture']),
              'old_roads_before': p['before'], 'hidden_old_roads': [], 'new_decals': [],
              'world_uv_scale': WORLD_UV_SCALE, 'opacity': OPACITY, 'roughness': ROUGHNESS,
              'source_assets_saved': [], 'deleted_actors': [], 'collision_changed': False,
              'resume': p['resume'], 'partial_material_before': p['partial_material_facts'],
              'material_creation_mode': 'reset_exact_unsaved_RoadA_graph' if p['resume'] else 'new_owned_material',
              'limitations': ['Road color repair only; actual visual review/cold reload still required',
                              'No navigation, gameplay, NPC, AI or skill changes', 'Hard rectangle edges are a first-pass visual contract']}
    evidence = p['evidence']
    evidence.parent.mkdir(parents=True, exist_ok=True)
    with evidence.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + '\n')

    def flush():
        evidence.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    def guard():
        _check_resume_source(p['resume'])
        current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if current != world or _asset_path(current) != MAP or p['levels'].is_in_play_in_editor():
            raise RuntimeError('Editor context changed; stop without retry')
        if any(not _allowed_package(k) for k in _dirty(unreal)):
            raise RuntimeError('Unrelated dirty package appeared; no broad save')

    try:
        guard()
        material = _make_material(p)
        report['material_compile_errors'] = []
        guard()
        for row in p['plan']['decals']:
            rot = row['rotation']
            actor = p['actors'].spawn_actor_from_class(unreal.DecalActor, unreal.Vector(*row['center']),
                    unreal.Rotator(pitch=rot['pitch'], yaw=rot['yaw'], roll=rot['roll']))
            if not actor:
                raise RuntimeError('Decal spawn failed: ' + row['edge_id'])
            record = {**row, 'actor_path': actor.get_path_name()}
            report['new_decals'].append(record)
            actor.set_actor_label(row['label'])
            actor.set_folder_path('WorldLayout20261112/RoadDecals')
            actor.set_editor_property('tags', [unreal.Name(BASE_TAG), unreal.Name(SCENE_TAG), unreal.Name(REPAIR_TAG)])
            actor.set_editor_property('is_spatially_loaded', True)
            actor.set_actor_scale3d(unreal.Vector(1, 1, 1))
            component = actor.get_component_by_class(unreal.DecalComponent)
            if not component:
                raise RuntimeError('DecalComponent missing: ' + row['edge_id'])
            component.set_decal_material(material)
            component.set_editor_property('decal_size', unreal.Vector(*row['local_half_extents']))
            component.set_editor_property('sort_order', 1)
            component.set_fade_screen_size(0.0)
            size = component.get_editor_property('decal_size')
            if max(abs(v - row['local_half_extents'][i]) for i, v in enumerate((size.x, size.y, size.z))) > .1:
                raise RuntimeError('Decal half-extent readback failed')
            if _asset_path(component.get_decal_material()) != p['material_path']:
                raise RuntimeError('Decal material readback failed')
        guard()
        # Commit the hide only after all replacement projectors were assigned.
        # bVisible=False is serialized and affects editor/game; game-hidden is
        # also saved. No transient-only editor hiding or deletion is relied on.
        for label in sorted(p['sources']):
            actor, component = p['sources'][label]
            component.set_visibility(False, True)
            actor.set_actor_hidden_in_game(True)
            if component.get_editor_property('visible') or not actor.get_editor_property('hidden'):
                raise RuntimeError('Road hidden-state readback failed: ' + label)
            if str(component.get_collision_profile_name()) != 'NoCollision':
                raise RuntimeError('Road collision unexpectedly changed: ' + label)
            report['hidden_old_roads'].append(label)
        report['source_road_count'] = len(p['sources'])
        report['new_decal_count'] = len(report['new_decals'])
        guard()
        if save_scene:
            if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
                raise RuntimeError('New owned material save returned false')
            guard()
            if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
                raise RuntimeError('Candidate SaveMap returned false; inspect without retry')
            packages = _dirty(unreal)
            if any(not _allowed_package(k) for k in packages):
                raise RuntimeError('Unrelated dirty packages after SaveMap')
            if packages and not unreal.EditorLoadingAndSavingUtils.save_packages(list(packages.values()), True):
                raise RuntimeError('Scoped candidate package save returned false')
            if _dirty(unreal):
                raise RuntimeError('Dirty packages remain after scoped save')
            report['saved_packages'] = [p['material_path'], MAP] + list(packages)
            report['status'] = 'saved_pending_visual_review_and_cold_reload'
        else:
            report['status'] = 'authored_unsaved_pending_owner_inspection'
        report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        flush()
        return report
    except Exception as error:
        report['status'] = 'failed_partial_repair_requires_inspection_no_retry'
        report['error'] = repr(error)
        report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        flush()
        raise


if __name__ == '__main__':
    raise RuntimeError('Import and call preflight()/repair_road_visuals() explicitly from the editor owner.')
