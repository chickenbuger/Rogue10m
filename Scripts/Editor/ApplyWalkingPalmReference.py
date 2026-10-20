"""Author/audit isolated Manny locomotion palm-roll derivatives in UE 5.8.

Default is read-only. `main(apply=True)` requires the native original-key export,
Content checkpoint and the independently inspected Walking.fbx policy. Never
imports over the supplied FBX, edits original clips or changes the combat graph.
"""
import hashlib
import json
import math
import runpy
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EVIDENCE = ROOT / 'Feature/doc/evidence/walking-palm-reference-20261009'
STAGING = ROOT / 'tmp/walking-palm-reference-20261009'
ORIGINAL_BS = '/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run'
FOLDER = '/Game/Rogue10m/Animation/Locomotion/PalmReference'
TARGET_BS = FOLDER + '/BS_Unarmed_PalmReference'
BLUEPRINT = '/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed'
SIGN = {'l': -1, 'r': 1}
IK_GOALS = [('ik_hand_gun', 'hand_r'), ('ik_hand_l', 'hand_l'), ('ik_hand_r', 'hand_r')]
API = runpy.run_path(str(ROOT / 'Scripts/Editor/PrepareFighterCombatStance.py'), run_name='walk_palm_helpers')


def require(path, cls=None):
    return API['require'](path, cls)


def write(name, value):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


def sub(a, b):
    return [x-y for x, y in zip(a, b)]


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def unit(a):
    n = math.sqrt(dot(a, a))
    if n < 1e-8:
        raise RuntimeError('Degenerate anatomical hand basis')
    return [v/n for v in a]


def axis_angle(axis, degrees):
    half = math.radians(degrees)/2
    return [v*math.sin(half) for v in unit(axis)] + [math.cos(half)]


def semantic(vector):
    # Proven from left/right shoulder positions and actual source component yaw -90.
    return [vector[1], -vector[0], vector[2]]


def hand_basis(pose, side):
    point = lambda name: API['bone'](pose, name+'_'+side)['p']
    axis = unit(sub(point('middle_01'), point('hand')))
    normal = [SIGN[side]*v for v in unit(cross(axis, sub(point('index_01'), point('pinky_01'))))]
    thumb = dot(sub(point('thumb_01'), point('middle_01')), normal)
    flexion = [dot(sub(point(f+'_03'), point(f+'_01')), normal) for f in ['index', 'middle', 'ring', 'pinky']]
    # Some unchanged Jog_Bwd finger endpoints extend ~0.39 cm past the palm
    # plane. Keep that source pose; endpoint extension cannot determine the
    # palmar sign. The thumb/base plane proves the fixed sign at every sample.
    if thumb <= 0:
        raise RuntimeError('Fixed palm anatomical sign invalid: ' + side)
    return axis, normal


def target_name(source):
    return FOLDER+'/'+source.get_name()+'_PalmReference'


def graph_snapshot(bp):
    return json.loads(json.dumps({g.get_path_name(): API['graph_dump'](unreal.BlueprintGraphEditor.get_graph_editor(g))
            for g in unreal.BlueprintEditorLibrary.list_graphs(bp)}))


def walk_node(bp):
    matches = [(g, n) for g in unreal.BlueprintEditorLibrary.list_graphs(bp)
               for n in unreal.BlueprintGraphEditor.get_graph_editor(g).list_all_nodes()
               if n.get_class().get_name() == 'AnimGraphNode_BlendSpacePlayer']
    if len(matches) != 1 or matches[0][0].get_name() != 'Walk / Run':
        raise RuntimeError('Expected one original Walk / Run BlendSpace player')
    return matches[0][1]


def content_hashes():
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (ROOT/'Content').rglob('*') if p.is_file()}


def protected_content():
    baseline = {p.replace('\\', '/'): h for p, h in json.loads((EVIDENCE/'content-before.json').read_text(encoding='utf-8')).items()}
    actual = content_hashes()
    allowed_bp = 'Content/Rogue10m/Animation/Common/ABP_Common_Unarmed.uasset'
    changed = [p for p, h in baseline.items() if actual.get(p) != h and p != allowed_bp]
    added = [p for p in actual if p not in baseline]
    invalid = [p for p in added if not p.startswith('Content/Rogue10m/Animation/Locomotion/PalmReference/') or not p.endswith('.uasset')]
    if changed or invalid:
        raise RuntimeError('Protected Content changed: ' + str(changed + invalid))
    return {'original_file_count': len(baseline), 'added': sorted(added),
            'protected_changed': changed, 'permitted_existing': allowed_bp}


def options():
    result = API['eval_options']()
    result.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    # MF source clips have female-authored limb lengths. Sample native keys
    # without Manny translation retargeting (e.g. hand 26.14 -> 27.30 cm), then
    # let the unchanged runtime retarget pipeline handle appearance proportions.
    result.set_editor_property('should_retarget', False)
    return result


def prepare_clip(source, source_keys, angles, opts, resume=False):
    target_path = target_name(source)
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        baseline = {p.replace('\\', '/'): h for p, h in json.loads((EVIDENCE/'content-before.json').read_text(encoding='utf-8')).items()}
        target_file = 'Content/'+target_path.removeprefix('/Game/')+'.uasset'
        if not resume or target_file in baseline:
            raise RuntimeError('Refusing to replace an existing/pre-task derived clip ' + target_path)
    model = source.get_editor_property('data_model_interface')
    rate = model.get_frame_rate()
    frames = model.get_number_of_frames()
    fps = rate.numerator/rate.denominator
    if frames != source_keys['frames'] or abs(source.get_play_length()-frames/fps) > 1e-5:
        raise RuntimeError('Native export duration differs ' + source.get_name())
    first = API['sample'](source, 0, opts)
    names, parents = API['hierarchy'](source, first)
    keys = {n: {'p': [], 'q': [], 's': []} for n in ['hand_l', 'hand_r']+[a for a, _ in IK_GOALS]}
    metrics = []
    for frame in range(frames+1):
        pose = API['sample'](source, frame/fps, opts)
        world = {n: API['bone'](pose, n) for n in set(keys) | {parents[n] for n in keys} |
                 {n for n in ['hand_l', 'hand_r', 'middle_01_l', 'middle_01_r']}}
        row = {'frame': frame, 'phase': frame/frames}
        for side in ['l', 'r']:
            name = 'hand_'+side
            axis, normal = hand_basis(pose, side)
            degrees = float(angles[side])
            if abs(degrees) > 45.00001:
                raise RuntimeError('Palm twist exceeds bounded authoring envelope')
            delta = axis_angle(axis, degrees)
            old = world[name]
            desired = API['quat_product'](delta, old['q'])
            local_q = API['quat_product'](API['inverse'](world[parents[name]]['q']), desired)
            frozen = source_keys['bones'][name][frame]
            evaluated = API['bone'](pose, name, unreal.AnimPoseSpaces.LOCAL)
            if math.dist(evaluated['p'], frozen['p']) > .005 or API['angle'](evaluated['q'], frozen['q']) > .01:
                raise RuntimeError('Native raw keys/evaluated skeleton differ: '+source.get_name()+':'+name)
            keys[name]['p'].append(unreal.Vector(*frozen['p']))
            keys[name]['q'].append(unreal.Quat(*local_q))
            keys[name]['s'].append(unreal.Vector(*frozen['s']))
            corrected = API['rotate'](delta, normal)
            row[side] = {'degrees': degrees, 'normal_before': semantic(normal),
                         'normal_after': semantic(corrected), 'hand_long_axis': semantic(axis),
                         'axis_preservation_error': math.dist(axis, API['rotate'](delta, axis))}
        # Keep ALL original auxiliary hand IK channels. The source/destination
        # translation-retarget modes can ignore a changed child's compensated
        # local position: rotating the gun parent then moves ik_hand_l in game.
        # This locomotion refinement needs only FK wrist roll; foot IK and all
        # hand targets retain the source's own authored tracks/offsets.
        for anchor, _ in IK_GOALS:
            frozen = source_keys['bones'][anchor][frame]
            keys[anchor]['p'].append(unreal.Vector(*frozen['p']))
            keys[anchor]['q'].append(unreal.Quat(*frozen['q']))
            keys[anchor]['s'].append(unreal.Vector(*frozen['s']))
        metrics.append(row)
    target = unreal.load_asset(target_path) if resume and unreal.EditorAssetLibrary.does_asset_exist(target_path) else unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), target_path)
    if not isinstance(target, unreal.AnimSequence):
        raise RuntimeError('Clip duplicate failed '+target_path)
    controller = target.get_editor_property('controller')
    controller.open_bracket('Walking reference palm roll; retain all other original channels', False)
    try:
        for name, values in keys.items():
            if not controller.set_bone_track_keys(name, values['p'], values['q'], values['s'], False):
                raise RuntimeError('Unable to write selected palm track ' + name)
    finally:
        controller.close_bracket(False)
    return target, {'source': source.get_path_name(), 'target': target_path,
                    'rate': rate.export_text(), 'frames': frames, 'samples': metrics}


def validate_clip(source, target, opts):
    source_model = source.get_editor_property('data_model_interface')
    target_model = target.get_editor_property('data_model_interface')
    # Markers/notifies/curves/attributes are not reflected for these UE 5.8
    # classes; the independent native key audit compares their exact metadata.
    for field in ['skeleton', 'enable_root_motion', 'force_root_lock', 'root_motion_root_lock',
                  'rate_scale', 'interpolation', 'loop']:
        if source.get_editor_property(field) != target.get_editor_property(field):
            # Struct arrays have wrapper identities; compare reflected text values.
            def stable(v):
                if hasattr(v, 'export_text'): return v.export_text()
                if hasattr(v, '__iter__') and not isinstance(v, str): return [stable(x) for x in v]
                return str(v)
            if stable(source.get_editor_property(field)) != stable(target.get_editor_property(field)):
                raise RuntimeError('Protected sequence setting changed '+field)
    if source_model.get_frame_rate().export_text() != target_model.get_frame_rate().export_text() or source_model.get_number_of_frames() != target_model.get_number_of_frames():
        raise RuntimeError('Source timing differs')
    names, parents = API['hierarchy'](source, API['sample'](source, 0, opts))
    allow_rotation = {'hand_l', 'hand_r'}
    allow_translation = set()
    errors = {'protected_local_rotation_deg': 0., 'protected_local_position_cm': 0., 'scale': 0.,
              'wrist_world_position_cm': 0., 'middle_mcp_world_position_cm': 0., 'ik_world_position_cm': 0.,
              'hand_long_axis': 0., 'added_hand_loop_rotation_deg': 0.}
    samples = []
    policy = json.loads((EVIDENCE/'reference/author-policy.json').read_text(encoding='utf-8'))
    reference = policy['reference_target_actor']
    direction = {side: {'before_errors': [], 'after_errors': [], 'inward_margin_min': 1., 'phase_error_increase_max_deg': 0.}
                 for side in ['l', 'r']}
    for i in range(33):
        t = source.get_play_length()*i/32
        a, b = [API['sample'](seq, t, opts) for seq in [source, target]]
        for name in names:
            la, lb = [API['bone'](p, name, unreal.AnimPoseSpaces.LOCAL) for p in [a, b]]
            if name not in allow_rotation:
                errors['protected_local_rotation_deg'] = max(errors['protected_local_rotation_deg'], API['angle'](la['q'], lb['q']))
            if name not in allow_translation:
                errors['protected_local_position_cm'] = max(errors['protected_local_position_cm'], math.dist(la['p'], lb['p']))
            errors['scale'] = max(errors['scale'], math.dist(la['s'], lb['s']))
        row = {'phase': i/32}
        for side in ['l', 'r']:
            axis_a, normal_a = hand_basis(a, side)
            axis_b, normal_b = hand_basis(b, side)
            errors['hand_long_axis'] = max(errors['hand_long_axis'], math.dist(axis_a, axis_b))
            for bone, field in [('hand_'+side, 'wrist_world_position_cm'), ('middle_01_'+side, 'middle_mcp_world_position_cm')]:
                errors[field] = max(errors[field], math.dist(API['bone'](a, bone)['p'], API['bone'](b, bone)['p']))
            row[side] = {'before': semantic(normal_a), 'after': semantic(normal_b)}
            before_n, after_n = row[side]['before'], row[side]['after']
            target_n = reference['Left' if side == 'l' else 'Right']
            error_a, error_b = [math.degrees(math.acos(max(-1., min(1., dot(n, target_n))))) for n in [before_n, after_n]]
            direction[side]['before_errors'].append(error_a)
            direction[side]['after_errors'].append(error_b)
            inward_a, inward_b = [n[1]*(1 if side == 'l' else -1) for n in [before_n, after_n]]
            direction[side]['inward_margin_min'] = min(direction[side]['inward_margin_min'], inward_b-min(inward_a, .35))
            direction[side]['phase_error_increase_max_deg'] = max(direction[side]['phase_error_increase_max_deg'], error_b-error_a)
        for name, _ in IK_GOALS:
            errors['ik_world_position_cm'] = max(errors['ik_world_position_cm'], math.dist(API['bone'](a, name)['p'], API['bone'](b, name)['p']))
        samples.append(row)
    # Measure the correction at both ends independently from the source's own
    # loop seam. A constant twist must not add a new first/last roll jump.
    for side in ['l', 'r']:
        endpoints = []
        for t in [0., source.get_play_length()]:
            qa, qb = [API['bone'](API['sample'](seq, t, opts), 'hand_'+side)['q'] for seq in [source, target]]
            endpoints.append(API['quat_product'](qb, API['inverse'](qa)))
        errors['added_hand_loop_rotation_deg'] = max(errors['added_hand_loop_rotation_deg'], API['angle'](*endpoints))
    # Interpolation of changed wrist rotations can move distal positions slightly
    # between original 30Hz keys. Auxiliary IK keeps every original channel.
    if errors['protected_local_rotation_deg'] > .001 or errors['protected_local_position_cm'] > .001 or errors['scale'] > .0001:
        raise RuntimeError('Protected evaluated local channels differ '+str(errors))
    if errors['wrist_world_position_cm'] > .001 or errors['middle_mcp_world_position_cm'] > .08 or errors['ik_world_position_cm'] > .15 or errors['hand_long_axis'] > .01:
        raise RuntimeError('Palm roll disturbs original reach/IK '+str(errors))
    if errors['added_hand_loop_rotation_deg'] > 1.:
        raise RuntimeError('Palm correction introduces a new loop seam '+str(errors))
    for side, measurements in direction.items():
        measurements['before_mean_reference_error_deg'] = sum(measurements.pop('before_errors'))/len(samples)
        measurements['after_mean_reference_error_deg'] = sum(measurements.pop('after_errors'))/len(samples)
        if measurements['inward_margin_min'] < -.01 or measurements['after_mean_reference_error_deg'] > measurements['before_mean_reference_error_deg']+.01:
            raise RuntimeError('Written palm roll fails reference/inward direction gate '+source.get_name()+':'+side+':'+str(measurements))
    return {'source': source.get_path_name(), 'target': target.get_path_name(), 'errors': errors,
            'reference_direction': direction, 'samples': samples}


def main(apply=False, resume=False):
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('Stop PIE before locomotion authoring/audit')
    source_bs, bp = require(ORIGINAL_BS, unreal.BlendSpace), require(BLUEPRINT, unreal.AnimBlueprint)
    player = walk_node(bp)
    before = graph_snapshot(bp)
    if apply and resume and player.get_editor_property('node').get_editor_property('blend_space').get_path_name() == TARGET_BS+'.BS_Unarmed_PalmReference':
        # Resume only this task's already validated reference substitution.
        # Reconstruct its pre-substitution snapshot from the one changed field.
        before = json.loads(json.dumps(before).replace(TARGET_BS+'.BS_Unarmed_PalmReference', ORIGINAL_BS+'.BS_Idle_Walk_Run'))
    original_samples = list(source_bs.get_editor_property('sample_data'))
    clips = {s.get_editor_property('animation').get_path_name(): s.get_editor_property('animation') for s in original_samples}
    moving = {p: s for p, s in clips.items() if s.get_name() != 'MM_Idle'}
    opts = options()
    authoring = []
    if apply:
        if not (STAGING/'checkpoint/ABP_Common_Unarmed.uasset').is_file():
            raise RuntimeError('Original AnimBP checkpoint missing')
        current_bs = player.get_editor_property('node').get_editor_property('blend_space')
        if current_bs != source_bs and not (resume and current_bs.get_path_name() == TARGET_BS+'.BS_Unarmed_PalmReference'):
            raise RuntimeError('Original locomotion reference changed since discovery')
        if unreal.EditorAssetLibrary.does_asset_exist(TARGET_BS):
            baseline = {p.replace('\\', '/'): h for p, h in json.loads((EVIDENCE/'content-before.json').read_text(encoding='utf-8')).items()}
            if not resume or 'Content/'+TARGET_BS.removeprefix('/Game/')+'.uasset' in baseline:
                raise RuntimeError('Refusing to replace existing/pre-task palm BlendSpace')
        native_export = json.loads((STAGING/'source-keys.json').read_text(encoding='utf-8-sig'))
        if native_export['result'] != 'PASSED' or not native_export['read_only'] or len(native_export['sequences']) != 16:
            raise RuntimeError('Original native key export incomplete/failed')
        exported = {}
        for row in native_export['sequences']:
            name = row['asset'].rsplit('.', 1)[-1]
            exported[name] = {'frames': int(row['frames']), 'bones': {
                bone: [{'p': tracks['p'][i], 'q': tracks['q'][i], 's': tracks['s'][i]}
                       for i in range(int(row['frames'])+1)] for bone, tracks in row['tracks'].items()}}
        policy = json.loads((EVIDENCE/'reference/author-policy.json').read_text(encoding='utf-8'))
        targets = {}
        for path, source in moving.items():
            target, report = prepare_clip(source, exported[source.get_name()], policy['angles'][source.get_name()], opts, resume=resume)
            targets[path] = target
            authoring.append(report)
        target_bs = unreal.load_asset(TARGET_BS) if resume and unreal.EditorAssetLibrary.does_asset_exist(TARGET_BS) else unreal.EditorAssetLibrary.duplicate_asset(ORIGINAL_BS, TARGET_BS)
        if not isinstance(target_bs, unreal.BlendSpace):
            raise RuntimeError('BlendSpace duplicate failed')
        samples = [s.copy() for s in original_samples]
        for s in samples:
            old = s.get_editor_property('animation')
            if old.get_path_name() in targets:
                s.set_editor_property('animation', targets[old.get_path_name()])
        target_bs.set_editor_property('sample_data', samples)
        checks = [validate_clip(source, targets[path], opts) for path, source in moving.items()]
        runtime = player.get_editor_property('node')
        runtime.set_editor_property('blend_space', target_bs)
        player.set_editor_property('node', runtime)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        for graph in unreal.BlueprintEditorLibrary.list_graphs(bp):
            if unreal.BlueprintGraphEditor.get_graph_editor(graph).list_nodes_with_errors():
                raise RuntimeError('Locomotion Blueprint compilation failed')
    else:
        target_bs = require(TARGET_BS, unreal.BlendSpace)
        targets = {p: require(target_name(s), unreal.AnimSequence) for p, s in moving.items()}
        checks = [validate_clip(source, targets[path], opts) for path, source in moving.items()]
        before = json.loads((EVIDENCE/'graph-before.json').read_text(encoding='utf-8'))
    if player.get_editor_property('node').get_editor_property('blend_space') != target_bs:
        raise RuntimeError('Player does not reference dedicated palm locomotion')
    after = graph_snapshot(bp)
    # Every graph/pin/link/runtime setting stays exact except this asset path.
    expected = json.loads(json.dumps(before).replace(ORIGINAL_BS+'.BS_Idle_Walk_Run', TARGET_BS+'.BS_Unarmed_PalmReference'))
    if after != expected:
        raise RuntimeError('Unexpected Blueprint graph/settings changes')
    actual_samples = list(target_bs.get_editor_property('sample_data'))
    if len(actual_samples) != len(original_samples):
        raise RuntimeError('BlendSpace sample count differs')
    for i, (original, derived) in enumerate(zip(original_samples, actual_samples)):
        source = original.get_editor_property('animation')
        expected_text = original.export_text()
        if source.get_path_name() in targets:
            expected_text = expected_text.replace(source.get_path_name(), targets[source.get_path_name()].get_path_name())
        if derived.export_text() != expected_text:
            raise RuntimeError('BlendSpace coordinates/rate/options differ: '+str(i))
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    native_file = STAGING/'native-key-audit.json'
    old_mtime = native_file.stat().st_mtime_ns if native_file.is_file() else None
    unreal.SystemLibrary.execute_console_command(world, 'Rogue10m.AuditWalkPalmKeys')
    if not native_file.is_file() or native_file.stat().st_mtime_ns == old_mtime:
        raise RuntimeError('Native key audit did not produce fresh evidence')
    native_audit = json.loads(native_file.read_text(encoding='utf-8-sig'))
    if native_audit.get('only_hand_rotations_allowed') is not True or native_audit['result'] != 'PASSED' or len(native_audit['sequences']) != 16 or any(row['result'] != 'PASSED' for row in native_audit['sequences']):
        raise RuntimeError('Native key/metadata audit failed: '+native_audit.get('error', ''))
    protected = protected_content()
    if apply:
        # Only now commit the verified asset references to disk.
        for asset in list(targets.values())+[target_bs, bp]:
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError('Verified palm asset save failed '+asset.get_path_name())
        write('graph-before.json', before)
        write('authoring.json', authoring)
        protected = protected_content()
    write(('apply' if apply else 'reload')+'-audit.json', {'status': 'PASSED', 'apply': apply,
          'moving_clips': len(moving), 'sample_count': len(actual_samples), 'original_idle_preserved': True,
          'graph_after': after, 'clips': checks, 'content': protected})
    unreal.log('RESULT=WALK_PALM_'+('APPLY' if apply else 'RELOAD')+'_PASSED')


if __name__ == '__main__':
    main()
