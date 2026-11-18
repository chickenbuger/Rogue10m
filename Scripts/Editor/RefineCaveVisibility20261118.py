"""Explicit unsaved cave visibility repair; import is inert.

Adds bounded exposure components to the two new dressing actors, corrects only
their known BGR-swapped point-light colors, and extends only M_CaveSurface's
BaseColor graph. No level load, original-actor edit, terrain/camera edit or save.
"""
from __future__ import annotations
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import traceback

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'Feature/doc/evidence/caves20261118'
SPEC = BASE / 'visibility-spec.json'
SPEC_SHA256 = '5509ec2f171704b4b963cb82726c9660f4e2c5b05ac883394ef63e72de644d81'
CORE_SHA256 = '39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74'
DRESS_SHA256 = '4177a9e1b8544e16cd61217cf64fd01a5c47da8e6ef3ee84dea25228107449db'
MATERIAL_RECEIPT_SHA256 = 'c25447d13701e54ad906e9d7d4e65efd2c52b87b407792714de8918a41888c54'
TARGET = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
OWN = '/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/'
SURFACE = OWN + 'Caves20261118/M_CaveSurface'
TAG = 'Rogue10m.Caves20261118.Visibility'


class CaveVisibilityError(RuntimeError):
    def __init__(self, report):
        self.report = report
        super().__init__(json.dumps(report, ensure_ascii=False))


def require(value, message):
    if not value:
        raise RuntimeError(message)


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec); spec.loader.exec_module(value)
    return value


def path(value):
    return value.get_path_name() if value else None


def bytes_json(value):
    return (json.dumps(value, sort_keys=True, ensure_ascii=False, separators=(',', ':'), allow_nan=False) + '\n').encode('utf-8')


def digest(value):
    return hashlib.sha256(bytes_json(value).rstrip(b'\n')).hexdigest()


def read_checked(relative, sha):
    raw = (BASE / relative).read_bytes()
    require(hashlib.sha256(raw).hexdigest() == sha, 'Immutable input changed: ' + relative)
    return json.loads(raw)


def build_spec():
    core = read_checked('plan.json', CORE_SHA256)
    dressing = read_checked('dressing-plan.json', DRESS_SHA256)
    read_checked('author-live/materials.json', MATERIAL_RECEIPT_SHA256)
    sites = []
    for site in core['sites']:
        x0, y0, x1, y1 = site['roof_rect_cm']
        sites.append({'site_id': site['site_id'], 'actor_label': 'CV18_' + site['site_id'] + '_Dressing',
            'box_name': 'CV18_' + site['site_id'] + '_ExposureBounds',
            'post_process_name': 'CV18_' + site['site_id'] + '_LocalExposure',
            'box_center_cm': [(x0 + x1) / 2, (y0 + y1) / 2, 8800.],
            'box_extent_cm': [(x1 - x0) / 2, (y1 - y0) / 2, 400.],
            'box_collision': 'NoCollision', 'box_hidden_in_game': True, 'box_visible': False,
            'pp_unbound': False, 'pp_enabled': True, 'priority': 100., 'blend_radius_cm': 200., 'blend_weight': 1.,
            'auto_exposure_min_brightness': 3., 'auto_exposure_max_brightness': 3.,
            'override_names': ['bOverride_AutoExposureMaxBrightness', 'bOverride_AutoExposureMinBrightness'],
            'outside_camera_test_cm': [(x0 + x1) / 2, (y0 + y1) / 2, 9880.]})
    return {'schema': 'Rogue10m.CaveVisibilitySpec.20261118.v1', 'map': TARGET,
        'core_plan_sha256': CORE_SHA256, 'dressing_plan_sha256': DRESS_SHA256,
        'original_material_receipt_sha256': MATERIAL_RECEIPT_SHA256,
        'global_exposure_actor_label': 'WL20261112_Exposure', 'global_min_max_ev100': 11.5,
        'surface_material': SURFACE, 'side_color_linear_rgba': [.25, .22, .17, 1.],
        'slope_min': .5, 'slope_max': .9, 'new_surface_nodes': 5,
        'preserve_original_surface_nodes': 13, 'expected_surface_nodes_after': 18,
        'pixel_normal_channel': 'B', 'base_color_formula': 'lerp(SideColor,OldBaseColor,smoothstep(.5,.9,PixelNormalWS.B))',
        'new_actor_count': 0, 'new_component_count': 4, 'sites': sites,
        'light_contract': {'count': 12, 'intensity_lumens': 12000., 'attenuation_radius_cm': 2800.,
            'colors_by_site': {s['site_id']: s['lights'][0]['color_srgb'] for s in dressing['sites']},
            'allowed_before': 'Exact planned RGB or exact R/B-swapped known constructor result, alpha255.',
            'after': 'Exact planned RGB using named unreal.Color r/g/b/a fields.'},
        'box_bounds_source_note': 'UE5.8 PostProcessComponent.cpp EncompassesPoint falls back to Shape.CalcBounds when no physics object or collision-distance query exists.',
        'outdoor_camera_z_cm': 9880., 'highest_local_blend_z_cm': 9400.,
        'runtime_render_acceptance': 'Required; local exposure, material graph and color readbacks alone are not visual acceptance.'}


def material_state(u, obj, reader):
    lib = u.MaterialEditingLibrary; base = obj; parents = []; seen = set()
    while isinstance(base, u.MaterialInstance):
        require(path(base) not in seen, 'Material parent cycle'); seen.add(path(base))
        base = base.get_editor_property('parent'); require(base is not None, 'Missing material parent')
        parents.append(path(base))
    require(isinstance(base, u.Material), 'Concrete material required')
    value = {'path': path(obj), 'parent_chain': parents, 'base_material': path(base),
             'used_with_instanced_static_meshes': bool(base.get_editor_property('used_with_instanced_static_meshes')),
             'graph': reader._fingerprint(u, lib, base)}
    if isinstance(obj, u.MaterialInstance):
        value['scalar_values'] = {str(k): lib.get_material_instance_scalar_parameter_value(obj, k) for k in lib.get_scalar_parameter_names(base)}
        value['vector_values'] = {str(k): [float(getattr(lib.get_material_instance_vector_parameter_value(obj, k), a)) for a in 'rgba']
                                  for k in lib.get_vector_parameter_names(base)}
        value['texture_values'] = {str(k): path(lib.get_material_instance_texture_parameter_value(obj, k)) for k in lib.get_texture_parameter_names(base)}
    return value


def full_pp_state(u, actors):
    result = {}
    for actor in actors:
        if isinstance(actor, u.PostProcessVolume):
            settings = actor.get_editor_property('settings')
            text = settings.export_text()
            require(len(text) > 100 and 'AutoExposureMinBrightness' in text, 'Full post-process struct export unavailable')
            result[path(actor)] = {'label': actor.get_actor_label(), 'settings_export_text': text,
                'settings_sha256': hashlib.sha256(text.encode()).hexdigest(),
                'priority': float(actor.get_editor_property('priority')), 'unbound': bool(actor.get_editor_property('unbound')),
                'enabled': bool(actor.get_editor_property('enabled')), 'blend_radius_cm': float(actor.get_editor_property('blend_radius')),
                'blend_weight': float(actor.get_editor_property('blend_weight')),
                'min': float(settings.get_editor_property('auto_exposure_min_brightness')),
                'max': float(settings.get_editor_property('auto_exposure_max_brightness')),
                'override_min': bool(settings.get_editor_property('override_auto_exposure_min_brightness')),
                'override_max': bool(settings.get_editor_property('override_auto_exposure_max_brightness'))}
    return result


def full_camera_state(u, inspector):
    cdo = u.get_default_object(u.Rogue10mCharacter.static_class())
    result = {'native_defaults': inspector.camera_defaults(u), 'camera_post_process': {}}
    for component in cdo.get_components_by_class(u.CameraComponent):
        result['camera_post_process'][path(component)] = {
            'settings_export_text': component.get_editor_property('post_process_settings').export_text(),
            'blend_weight': float(component.get_editor_property('post_process_blend_weight'))}
    return result


def rgb(light):
    value = light.get_editor_property('light_color')
    return [int(value.r), int(value.g), int(value.b), int(value.a)]


def light_state(light, api):
    return {'color_rgba_srgb': rgb(light), 'relative_location_cm': api._xyz(light.get_editor_property('relative_location')),
            'intensity': float(light.get_editor_property('intensity')), 'radius_cm': float(light.get_editor_property('attenuation_radius')),
            'units': str(light.get_editor_property('intensity_units')), 'cast_shadows': bool(light.get_editor_property('cast_shadows')),
            'mobility': str(light.get_editor_property('mobility')), 'tick_enabled': light.is_component_tick_enabled(),
            'visible': bool(light.get_editor_property('visible')), 'hidden_in_game': bool(light.get_editor_property('hidden_in_game'))}


def settings_for(u, site):
    value = u.PostProcessSettings()
    for key, number in [('override_auto_exposure_min_brightness', True), ('override_auto_exposure_max_brightness', True),
                        ('auto_exposure_min_brightness', site['auto_exposure_min_brightness']),
                        ('auto_exposure_max_brightness', site['auto_exposure_max_brightness'])]:
        value.set_editor_property(key, number)
    text = value.export_text()
    enabled = sorted(name for name, v in re.findall(r'(bOverride_\w+)=(True|False)', text) if v == 'True')
    require(enabled == site['override_names'], 'Local settings enabled an unexpected override')
    return value, text


def refine_cave_visibility(*, allow_editor_mutation=False, expected_spec_sha256,
                          expected_composite_blake3, expected_original_layer_blake3, expected_dirty_packages):
    report = {'schema': 'Rogue10m.CaveVisibilityRefinement.20261118.v1', 'success': False,
        'started_utc': datetime.now(timezone.utc).isoformat(), 'mutation_started': False, 'assets_saved': False,
        'save_called': False, 'actors_created': [], 'created_component_paths': [], 'postprocess_sites': [],
        'lights_color_corrected': [], 'terrain_mutated': False, 'camera_properties_mutated': False,
        'automatic_retry': False, 'automatic_rollback': False, 'partial_work_deleted': False}
    try:
        require(allow_editor_mutation is True, 'Explicit allow_editor_mutation=True required')
        require(expected_spec_sha256 == SPEC_SHA256, 'Wrong reviewed visibility spec hash')
        spec = read_checked('visibility-spec.json', SPEC_SHA256)
        require(spec['schema'] == 'Rogue10m.CaveVisibilitySpec.20261118.v1' and spec['map'] == TARGET, 'Exact visibility spec required')
        core = read_checked('plan.json', CORE_SHA256); dressing_plan = read_checked('dressing-plan.json', DRESS_SHA256)
        old_receipt = read_checked('author-live/materials.json', MATERIAL_RECEIPT_SHA256)
        import unreal as u
        api = module('cave_visibility_component_api', 'Scripts/Editor/NaturalWorldDressing20261117.py')
        inspector = module('cave_visibility_inspector', 'Scripts/Editor/InspectCaveSites20261118.py')
        reader = module('cave_visibility_graph_reader', 'Scripts/Editor/NaturalRoadMaterial20261117.py')
        old_reader = module('cave_visibility_actor_reader', 'Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
        api._guard(u)
        require(len(expected_dirty_packages) == len(set(expected_dirty_packages)) and set(api._dirty(u)) == set(expected_dirty_packages),
                'Exact expected dirty set required')
        require(u.SystemLibrary.get_console_variable_int_value('r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange') == 1,
                'EV100 exposure range must be enabled')
        require(callable(getattr(u.SystemLibrary, 'get_component_bounds', None)) and
                callable(getattr(u.SceneComponent, 'get_attach_parent', None)), 'Public bounds/attachment bindings unavailable')
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native.get('success') and native.get('landscape_reports_up_to_date') and
                native['composite_blake3'] == expected_composite_blake3 and native['original_layer_blake3'] == expected_original_layer_blake3,
                'Exact settled terrain hashes required')
        subsystem = u.get_editor_subsystem(u.EditorActorSubsystem); actors = list(subsystem.get_all_level_actors())
        require(len(actors) == len({path(a) for a in actors}) == 1408, 'Exact1408 scene required')
        by_label = {a.get_actor_label(): a for a in actors}
        targets = {s['site_id']: by_label.get(s['actor_label']) for s in spec['sites']}
        require(all(a is not None and not a.get_editor_property('is_spatially_loaded') for a in targets.values()), 'Always-loaded dressing actors required')
        require(not any(c.get_name() in {s['box_name'], s['post_process_name']} for s in spec['sites']
                        for a in targets.values() for c in a.get_components_by_class(u.ActorComponent)), 'Visibility components already exist; no retry')
        before = old_reader._snapshot_actors(u, actors, api); before_components = inspector.persistent_components(u, actors)
        baseline = json.loads((BASE / 'baseline-live/baseline-actors.json').read_text(encoding='utf-8'))
        require({p: before[p] for p in baseline['actors']} == baseline['actors'] and
                {p: before_components[p] for p in baseline['actors']} == baseline['persistent_components'], 'Original1404 state changed')
        global_before = full_pp_state(u, actors); camera_before = full_camera_state(u, inspector)
        exposure = [v for v in global_before.values() if v['label'] == spec['global_exposure_actor_label']]
        require(len(exposure) == 1 and exposure[0]['min'] == exposure[0]['max'] == 11.5 and
                exposure[0]['override_min'] and exposure[0]['override_max'] and exposure[0]['unbound'] and exposure[0]['enabled'] and
                all(v['priority'] < 100 for v in global_before.values() if v['enabled']), 'Expected globalEV11.5 exposure diagnosis differs')
        report['global_postprocess_before'] = global_before; report['camera_cdo_postprocess_before'] = camera_before
        lights, planned_lights = {}, {}
        for site in dressing_plan['sites']:
            actual = list(targets[site['site_id']].get_components_by_class(u.PointLightComponent))
            require(len(actual) == 6, 'Exact six point lights required per site')
            for i, desired in enumerate(site['lights']):
                found = [c for c in actual if c.get_name() == site['site_id'] + '_CavePointLight_' + str(i).zfill(2)]
                require(len(found) == 1, 'Exact light component required')
                light = found[0]; state = light_state(light, api); wanted = desired['color_srgb'] + [255]
                require(state['color_rgba_srgb'] in (wanted, [wanted[2], wanted[1], wanted[0], 255]) and
                        state['intensity'] == desired['intensity_lumens'] == 12000. and
                        state['radius_cm'] == desired['attenuation_radius_cm'] == 2800. and
                        state['units'] == str(u.LightUnits.LUMENS) and not state['cast_shadows'] and not state['tick_enabled'] and
                        max(abs(a-b) for a,b in zip(state['relative_location_cm'], desired['location_cm'])) < .01,
                        'Light differs beyond the diagnosed exact RGB/BGR swap')
                lights[path(light)] = light; planned_lights[path(light)] = wanted
        require(len(lights) == 12, 'Exact twelve new lights required')
        report['lights_before'] = {p: light_state(c, api) for p, c in lights.items()}
        lib = u.MaterialEditingLibrary
        new_assets = {p: u.load_asset(p) for p in old_receipt['created_material_states']}
        material_before = {p: material_state(u, obj, reader) for p, obj in new_assets.items()}
        require(len(material_before) == 7 and all(v == {k:x for k,x in old_receipt['created_material_states'][p].items() if k != 'unsaved'}
                                                 for p,v in material_before.items()), 'Seven new material states differ from original receipt')
        surface = u.load_asset(SURFACE); require(isinstance(surface, u.Material), 'Owned surface Material required')
        surface_key = path(surface); surface_before = material_before[surface_key]['graph']
        require(len(surface_before['nodes']) == 13, 'Exact untouched13-node surface required')
        old_base = lib.get_material_property_input_node(surface, u.MaterialProperty.MP_BASE_COLOR)
        old_output = str(lib.get_material_property_input_node_output_name(surface, u.MaterialProperty.MP_BASE_COLOR))
        require(old_base is not None, 'Existing BaseColor connection required')
        report['material_states_before'] = material_before; report['surface_graph_before'] = surface_before
        prepared_settings = {s['site_id']: settings_for(u, s) for s in spec['sites']}
        require(set(api._dirty(u)) == set(expected_dirty_packages), 'Read-only visibility preflight dirtied a package')
        sds = u.get_engine_subsystem(u.SubobjectDataSubsystem); require(sds is not None, 'Public component authoring required')
        with u.ScopedEditorTransaction('Bounded cave visibility and slope-side repair 20261118'):
            report['mutation_started'] = True
            for p, light in lights.items():
                wanted = planned_lights[p]
                if rgb(light) != wanted:
                    light.set_editor_property('light_color', u.Color(r=wanted[0], g=wanted[1], b=wanted[2], a=255))
                    report['lights_color_corrected'].append(p)
                require(rgb(light) == wanted, 'Actual light RGB correction failed')
            for site in spec['sites']:
                actor = targets[site['site_id']]; libsub = u.SubobjectDataBlueprintFunctionLibrary
                root = actor.get_editor_property('root_component')
                handles = [h for h in sds.k2_gather_subobject_data_for_instance(actor)
                           if libsub.is_root_component(libsub.get_data(h)) and libsub.get_associated_object(libsub.get_data(h)) == root]
                require(len(handles) == 1, 'One unchanged existing root handle required')
                box_handle, box = api._new_component(u, sds, handles[0], u.BoxComponent, site['box_name'])
                report['created_component_paths'].append(path(box))
                box.set_mobility(u.ComponentMobility.STATIC); box.set_box_extent(u.Vector(*site['box_extent_cm']), False)
                box.set_editor_property('relative_location', u.Vector(*site['box_center_cm']))
                box.set_collision_profile_name('NoCollision', False); box.set_collision_response_to_all_channels(u.CollisionResponseType.ECR_IGNORE)
                box.set_collision_enabled(u.CollisionEnabled.NO_COLLISION); box.set_editor_property('generate_overlap_events', False)
                box.set_editor_property('can_ever_affect_navigation', False); box.set_component_tick_enabled(False)
                box.set_visibility(False, False); box.set_hidden_in_game(True, False)
                box.set_editor_property('component_tags', [u.Name(TAG), u.Name('LocalExposureBounds')])
                _, pp = api._new_component(u, sds, box_handle, u.PostProcessComponent, site['post_process_name'])
                report['created_component_paths'].append(path(pp))
                pp.set_editor_property('unbound', False); pp.set_editor_property('enabled', True)
                pp.set_editor_property('priority', site['priority']); pp.set_editor_property('blend_radius', site['blend_radius_cm'])
                pp.set_editor_property('blend_weight', site['blend_weight']); pp.set_editor_property('settings', prepared_settings[site['site_id']][0])
                pp.set_editor_property('relative_location', u.Vector(0,0,0)); pp.set_component_tick_enabled(False)
                pp.set_visibility(True, False); pp.set_hidden_in_game(False, False)
                pp.set_editor_property('component_tags', [u.Name(TAG), u.Name('BoundedLocalExposure')])
                require(pp.get_attach_parent() == box and box.get_attach_parent() == root, 'PostProcess must attach to its Box')
                bounds = u.SystemLibrary.get_component_bounds(box)
                require(isinstance(bounds, tuple) and len(bounds) == 3, 'Expected public Box world-bounds tuple')
                origin, extent, radius = bounds
                require(max(abs(a-b) for a,b in zip(api._xyz(origin), site['box_center_cm'])) < .01 and
                        max(abs(a-b) for a,b in zip(api._xyz(extent), site['box_extent_cm'])) < .01 and
                        pp.get_editor_property('settings').export_text() == prepared_settings[site['site_id']][1] and
                        box.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION and
                        not box.is_component_tick_enabled() and not pp.is_component_tick_enabled(), 'Local volume readback differs')
                report['postprocess_sites'].append({'site_id': site['site_id'], 'actor_path': path(actor),
                    'box_path': path(box), 'post_process_path': path(pp), 'parent_box_path': path(pp.get_attach_parent()),
                    'box_center_cm': api._xyz(origin), 'box_extent_cm': api._xyz(extent),
                    'box_world_bounds': {'origin_cm': api._xyz(origin), 'extent_cm': api._xyz(extent), 'sphere_radius_cm': float(radius)},
                    'box_collision_enabled': str(box.get_collision_enabled()), 'box_hidden_in_game': True, 'box_visible': False,
                    'pp_unbound': bool(pp.get_editor_property('unbound')), 'pp_enabled': bool(pp.get_editor_property('enabled')),
                    'priority': float(pp.get_editor_property('priority')), 'blend_radius_cm': float(pp.get_editor_property('blend_radius')),
                    'blend_weight': float(pp.get_editor_property('blend_weight')),
                    'settings_export_text': pp.get_editor_property('settings').export_text(), 'override_names': site['override_names']})
            surface.modify()
            nodes = {}
            for name, cls in [('pixel_normal_ws', u.MaterialExpressionPixelNormalWS), ('mask_z', u.MaterialExpressionComponentMask),
                              ('smooth', u.MaterialExpressionSmoothStep), ('color', u.MaterialExpressionConstant3Vector),
                              ('lerp', u.MaterialExpressionLinearInterpolate)]:
                nodes[name] = lib.create_material_expression(surface, cls, 700 + len(nodes)*180, 500)
                require(nodes[name] is not None, 'Surface slope node creation failed: ' + name)
            for channel in 'rgba': nodes['mask_z'].set_editor_property(channel, channel == 'b')
            nodes['smooth'].set_editor_property('const_min', .5); nodes['smooth'].set_editor_property('const_max', .9)
            nodes['smooth'].set_editor_property('const_value', 0.)
            nodes['color'].set_editor_property('constant', u.LinearColor(r=.25, g=.22, b=.17, a=1.))
            for key, value in [('const_a',0.),('const_b',1.),('const_alpha',.5)]: nodes['lerp'].set_editor_property(key,value)
            for source, output, dest, pin in [(nodes['pixel_normal_ws'],'',nodes['mask_z'],''),
                (nodes['mask_z'],'',nodes['smooth'],'Value'), (nodes['color'],'',nodes['lerp'],'A'),
                (old_base,old_output,nodes['lerp'],'B'), (nodes['smooth'],'',nodes['lerp'],'Alpha')]:
                reader._connect(lib, surface, source, dest, pin, output)
            require(lib.connect_material_property(nodes['lerp'],'',u.MaterialProperty.MP_BASE_COLOR), 'BaseColor slope output failed')
            lib.recompile_material(surface)
            report['surface_slope_nodes'] = {k:v.get_name() for k,v in nodes.items()}
            report['surface_added_nodes'] = [{'name':v.get_name(),'class':v.get_class().get_name(),
                'properties': reader._properties(v), 'inputs': reader._inputs(lib,surface,v)} for v in nodes.values()]
            report['surface_explicit_node_properties'] = {'color_constant_rgba':[float(getattr(nodes['color'].get_editor_property('constant'), a)) for a in 'rgba'],
                'lerp_const_a':float(nodes['lerp'].get_editor_property('const_a')), 'lerp_const_b':float(nodes['lerp'].get_editor_property('const_b')),
                'lerp_const_alpha':float(nodes['lerp'].get_editor_property('const_alpha'))}
        after_actors = list(subsystem.get_all_level_actors())
        require({path(a) for a in after_actors} == set(before), 'Visibility repair added/deleted an actor')
        after = old_reader._snapshot_actors(u,after_actors,api); after_components = inspector.persistent_components(u,after_actors)
        created = set(report['created_component_paths'])
        filtered_actors = copy.deepcopy(after); filtered_components = copy.deepcopy(after_components)
        for actor_path in filtered_actors:
            filtered_actors[actor_path]['primitives'] = [p for p in filtered_actors[actor_path]['primitives'] if p['path'] not in created]
            filtered_components[actor_path] = [p for p in filtered_components[actor_path] if p['path'] not in created]
            before_by_path = {p['path']:p for p in before_components[actor_path]}
            for component in filtered_components[actor_path]:
                if component['path'] in lights:
                    component['light_properties']['light_color'] = before_by_path[component['path']]['light_properties']['light_color']
        require(filtered_actors == before and filtered_components == before_components, 'Unapproved existing actor/component property changed')
        material_after = {p:material_state(u,obj,reader) for p,obj in new_assets.items()}
        surface_after = material_after[surface_key]['graph']
        require(len(surface_after['nodes']) == 18 and all(surface_after['nodes'][k] == v for k,v in surface_before['nodes'].items()), 'Original surface nodes changed')
        base_key = str(u.MaterialProperty.MP_BASE_COLOR.value)
        require(all(v == surface_after['outputs'][k] for k,v in surface_before['outputs'].items() if k != base_key) and
                surface_after['outputs'][base_key]['node'] == report['surface_slope_nodes']['lerp'], 'Only BaseColor output may change')
        for p,before_state in material_before.items():
            after_state=material_after[p]
            require({k:v for k,v in before_state.items() if k != 'graph'} == {k:v for k,v in after_state.items() if k != 'graph'}, 'New material parameters/parentage changed')
            if before_state['base_material'] == surface_key: require(after_state['graph'] == surface_after, 'Surface-derived graph mismatch')
            else: require(after_state['graph'] == before_state['graph'], 'Other new material graph changed')
        report['lights_after'] = {p:light_state(c,api) for p,c in lights.items()}
        for p,state in report['lights_after'].items():
            require(state['color_rgba_srgb'] == planned_lights[p] and
                    {k:v for k,v in state.items() if k != 'color_rgba_srgb'} ==
                    {k:v for k,v in report['lights_before'][p].items() if k != 'color_rgba_srgb'}, 'Light changed beyond RGB correction')
        global_after=full_pp_state(u,after_actors); camera_after=full_camera_state(u,inspector)
        require(global_after == global_before and camera_after == camera_before, 'Global post-process/camera settings changed')
        native_after=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native_after.get('success') and native_after['composite_blake3']==expected_composite_blake3 and
                native_after['original_layer_blake3']==expected_original_layer_blake3, 'Visibility repair changed terrain')
        allowed=set(expected_dirty_packages)|{SURFACE}|{a.get_package().get_name() for a in targets.values()}
        require(set(api._dirty(u))<=allowed, 'Visibility repair dirtied an unrelated package')
        report.update(success=True, spec_sha256=SPEC_SHA256, actor_count=1408, source_material_receipt_sha256=MATERIAL_RECEIPT_SHA256,
            global_postprocess_after=global_after,camera_cdo_postprocess_after=camera_after,
            surface_graph_after=surface_after,expected_material_states_after=material_after,
            all_seven_material_parameters_parentage_preserved=True,original1404_baseline_preserved=True,
            all1408_existing_geometry_snapshots_equal=True,existing_components_preserved_except_declared_light_rgb=True,
            no_pawn_blocking_components_added=True,dirty_before=list(expected_dirty_packages),dirty_after=api._dirty(u),
            finished_utc=datetime.now(timezone.utc).isoformat(),
            validation_pending=['actual bounded inside/outside exposure rendering','roof side appearance','runtime camera and Pawn queries','bounded save and cold reload'])
        return report
    except Exception:
        report['traceback']=traceback.format_exc()
        raise CaveVisibilityError(report) from None


def main():
    parser=argparse.ArgumentParser(description='Pure visibility spec generation; no editor invocation.')
    parser.add_argument('--write-spec',action='store_true');args=parser.parse_args()
    require(args.write_spec,'Explicit --write-spec required')
    value=build_spec()
    with SPEC.open('xb') as stream:stream.write(bytes_json(value))
    print(json.dumps({'spec_sha256':hashlib.sha256(bytes_json(value)).hexdigest(),'components':4,'new_nodes':5,'lights_rgb':12,'sites':value['sites']},indent=2))


if __name__=='__main__': main()
