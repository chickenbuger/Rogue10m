"""Prepare forward-facing derivatives, preserve gameplay timing, and connect grounded cadence.

Default main(apply=False) only prepares a reviewable manifest. Applying requires
the current read-only audit and a complete pre-change checkpoint. Originals are
retained. New assets and affected Data Assets are saved only after validation.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
import runpy

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EVIDENCE = ROOT / 'Feature/doc/evidence/mouse-head-stable-locomotion-20261013'
FOLDER = '/Game/Rogue10m/Animation/Combat/HeadSafe'
AUTHORED = '/Game/Rogue10m/Animation/Combat/FistAuthored/'
BLUEPRINT = '/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed'
MOTION_DATA = '/Game/DataAsset/Character/Animation/DA_CommonCharacterMotion'
MESH = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
PIN = unreal.BlueprintGraphPinLibrary
API = runpy.run_path(str(ROOT / 'Scripts/Editor/PrepareFighterCombatStance.py'), run_name='safe_head_pose_helpers')


def write(name, value):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


def require(path, cls=None):
    return API['require'](path, cls)


def graph_snapshot(bp):
    return json.loads(json.dumps({g.get_path_name(): API['graph_dump'](unreal.BlueprintGraphEditor.get_graph_editor(g))
                                 for g in unreal.BlueprintEditorLibrary.list_graphs(bp)}))


def serial(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (list, tuple, unreal.Array)):
        return [serial(v) for v in value]
    try:
        return {str(k): serial(v) for k, v in value.items()}
    except (AttributeError, TypeError):
        pass
    return value.export_text() if hasattr(value, 'export_text') else str(value)


def policy(montage):
    keys = ['blend_in', 'blend_out', 'blend_out_trigger_time', 'enable_auto_blend_out',
            'rate_scale', 'sync_group', 'sync_slot_index']
    output = {key: serial(montage.get_editor_property(key)) for key in keys}
    # UE 5.8 exposes section count/name but not CompositeSections reflection.
    # Native helper verifies next links, actual times, slot/index and complete
    # section metadata and notifies before the caller can save any derivative.
    output['section_names'] = [str(montage.get_section_name(index)) for index in range(montage.get_num_sections())]
    output['notifies'] = [serial(event) for event in unreal.AnimationLibrary.get_animation_notify_events(montage)]
    output['tracks'] = []
    for slot in montage.get_editor_property('slot_anim_tracks'):
        segments = slot.get_editor_property('anim_track').get_editor_property('anim_segments')
        output['tracks'].append({'slot': str(slot.get_editor_property('slot_name')),
                                 'segments': [{key: serial(segment.get_editor_property(key)) for key in
                                               ['anim_reference', 'start_pos', 'anim_start_time', 'anim_end_time',
                                                'anim_play_rate', 'looping_count']} for segment in segments]})
    return output


def manifest(audit):
    unsafe = {row['sequence'] for row in audit['evaluated_ranges'] if row['turn_candidate']}
    montage_rows = {row['path']: row for row in audit['montages']}
    sequence_specs, montage_specs, bindings = {}, {}, []
    styles = {'MM_Attack_01': ('LeftJab', 'A_Unarmed_LeftJab'),
              'MM_Attack_02': ('RightJab', 'A_Unarmed_RightJab'),
              'MM_Attack_03': ('Hook', 'A_Unarmed_Hook'),
              'MM_Dash': ('ForwardShortBlow', 'A_Knuckle_ShortBlow')}
    for skill in audit['skills']:
        old_path = skill['fields']['attack_montage']
        if not old_path:
            continue
        old = montage_rows[old_path]
        segments = [s for slot in old['tracks'] for s in slot['segments']]
        if not any(s['anim_reference'] in unsafe for s in segments):
            continue
        if len(old['tracks']) != 1 or old['tracks'][0]['slot'] != 'DefaultSlot' or len(segments) != 1:
            raise RuntimeError('Unreviewed unsafe slot/segment layout: ' + old_path)
        segment = segments[0]
        if segment['start_pos'] != 0 or segment['anim_start_time'] != 0 or segment['anim_play_rate'] != 1 or segment['looping_count'] != 1:
            raise RuntimeError('Unreviewed unsafe segment timing: ' + old_path)
        basename = segment['anim_reference'].rsplit('.', 1)[-1]
        is_uppercut = skill['path'].rsplit('.', 1)[-1] in ['DA_BasicBrawler_RightUppercut', 'DA_BasicBrawler_ChargedRightUppercut']
        if is_uppercut:
            style, source = 'BrawlerUppercut', AUTHORED+'A_Knuckle_Uppercut'
            target_contact, source_contact = skill['fields']['hit_start_delay_seconds'], .21
            # Current audited DAs store frame 19 at 30 Hz, not rounded 0.63 s.
            if abs(target_contact-19/30.) > 1e-6:
                raise RuntimeError('Uppercut timing changed since reviewed baseline')
        elif basename == 'A_Fighter_Overlord_PowerPunch':
            style, source = 'OverlordForwardPunch', AUTHORED+'A_Knuckle_ShortBlow'
            target_contact, source_contact = skill['fields']['hit_start_delay_seconds'], .22
        elif basename in styles:
            style, source_name = styles[basename]
            source, target_contact, source_contact = AUTHORED+source_name, None, None
        else:
            raise RuntimeError('Measured turn has no reviewed forward replacement: ' + basename)
        spec = {'name': style, 'source': source, 'length': old['duration'], 'target_contact': target_contact,
                'source_contact': source_contact, 'target': FOLDER+'/A_Safe_'+style}
        if style in sequence_specs and sequence_specs[style] != spec:
            raise RuntimeError('Ambiguous safe sequence timing for ' + style)
        sequence_specs[style] = spec
        old_name = old_path.rsplit('.', 1)[-1]
        new_name = 'AM_Safe_Brawler_Uppercut' if is_uppercut else 'AM_Safe_'+old_name.removeprefix('AM_')
        new_path = FOLDER+'/'+new_name
        montage_specs[new_path] = {'original': old_path, 'target': new_path, 'sequence': spec['target'], 'dodge': False}
        bindings.append({'skill': skill['path'], 'original': old_path, 'target': new_path,
                         'fields_before': skill['fields']})
    dodge = audit['dodge_montage']
    if not dodge:
        raise RuntimeError('Missing reviewed common dodge montage')
    sequence_specs['DodgeStep'] = {'name': 'DodgeStep',
        'source': '/Game/Rogue10m/Animation/Locomotion/PalmReference/MF_Unarmed_Jog_Fwd_PalmReference',
        'length': .2, 'source_start': .18, 'source_end': .38, 'target_contact': None, 'source_contact': None,
        'target': FOLDER+'/A_Safe_Dodge_Step'}
    montage_specs[FOLDER+'/AM_Safe_Dodge_Step'] = {'original': dodge, 'target': FOLDER+'/AM_Safe_Dodge_Step',
        'sequence': FOLDER+'/A_Safe_Dodge_Step', 'dodge': True}
    return {'sequences': list(sequence_specs.values()), 'montages': list(montage_specs.values()), 'bindings': bindings,
            'dodge': {'motion_data': MOTION_DATA, 'original': dodge, 'target': FOLDER+'/AM_Safe_Dodge_Step'},
            'timing': 'All attack DA fields except AttackMontage stay exact. Attack length and positive contact timing preserved.',
            'dodge_timing': 'Roll replaced by a 0.2-second non-turning jog step; existing capsule displacement/duration remain unchanged.'}


def mapped_time(spec, source, time):
    start, end = spec.get('source_start', 0.), spec.get('source_end', source.get_play_length())
    contact = spec['target_contact']
    if contact is not None:
        source_contact = spec['source_contact']
        if not 0 < contact < spec['length'] or not start < source_contact < end:
            raise RuntimeError('Invalid reviewed contact time map')
        if time <= contact:
            return start + (source_contact-start)*time/contact
        return source_contact + (end-source_contact)*(time-contact)/(spec['length']-contact)
    return start + (end-start)*time/spec['length']


def prepare_sequence(spec, options):
    source = require(spec['source'], unreal.AnimSequence)
    if unreal.AnimationLibrary.get_animation_notify_events(source):
        raise RuntimeError('Safe source unexpectedly contains notify events')
    target = unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), spec['target'])
    if not isinstance(target, unreal.AnimSequence):
        raise RuntimeError('Failed new derivative creation')
    frames = round(spec['length']*120)
    if abs(frames/120.-spec['length']) > .0001 or abs(spec['length']*30-round(spec['length']*30)) > .001:
        raise RuntimeError('Derivative duration incompatible with reviewed compression rate')
    initial = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, mapped_time(spec, source, 0), options)
    root_origin = unreal.AnimPoseExtensions.get_bone_pose(initial, 'root', unreal.AnimPoseSpaces.LOCAL).translation
    root_origin_values = [root_origin.x, root_origin.y, root_origin.z]
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(initial)]
    keys = {name: {'p': [], 'q': [], 's': []} for name in names}
    for frame in range(frames+1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, mapped_time(spec, source, frame/120.), options)
        for name in names:
            value = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
            if name == 'root':
                # A constant source origin can supply the character's standing
                # height. Preserve it; remove only travel after this first pose.
                value.translation = root_origin
            API['transform'](value)
            keys[name]['p'].append(value.translation)
            keys[name]['q'].append(value.rotation)
            keys[name]['s'].append(value.scale3d)
    controller = target.get_editor_property('controller')
    controller.open_bracket('Forward-facing head-safe derivative, originals retained', False)
    try:
        controller.remove_all_bone_tracks(False)
        for kind in [unreal.RawCurveTrackTypes.RCT_FLOAT, unreal.RawCurveTrackTypes.RCT_TRANSFORM]:
            controller.remove_all_curves_of_type(kind, False)
        controller.set_frame_rate(unreal.FrameRate(120, 1), False)
        controller.set_number_of_frames(unreal.FrameNumber(frames), False)
        for name, row in keys.items():
            if not controller.add_bone_curve(name, False) or not controller.set_bone_track_keys(name, row['p'], row['q'], row['s'], False):
                raise RuntimeError('Safe derivative bone key write failed: ' + name)
    finally:
        controller.close_bracket(False)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(target)
    target.set_editor_property('enable_root_motion', False)
    target.set_editor_property('force_root_lock', True)
    target.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    target.set_editor_property('rate_scale', 1.)
    max_position = max_rotation = max_scale = max_root = max_structural_position = 0.
    height_samples = []
    audit = runpy.run_path(str(ROOT/'Scripts/Editor/AuditStableHeadMotion.py'), run_name='safe_head_range_helpers')
    for frame in range(frames+1):
        time = frame/120.
        actual = unreal.AnimPoseExtensions.get_anim_pose_at_time(target, time, options)
        expected = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, mapped_time(spec, source, time), options)
        actual_root_component, expected_root_component = API['bone'](actual, 'root'), API['bone'](expected, 'root')
        removed_travel = [expected_root_component['p'][i]-actual_root_component['p'][i] for i in range(3)]
        structural = {}
        for name in ['hand_r', 'head', 'pelvis', 'foot_l', 'foot_r']:
            a, b = API['bone'](actual, name), API['bone'](expected, name)
            expected_position = [b['p'][i]-removed_travel[i] for i in range(3)]
            max_structural_position = max(max_structural_position, math.dist(a['p'], expected_position))
            structural[name] = {'source_component': b, 'target_component': a,
                                'source_component_without_removed_travel_cm': expected_position}
        if frame in {0, frames, round((spec['target_contact'] or 0.)*120)}:
            height_samples.append({'target_seconds': time, 'source_seconds': mapped_time(spec, source, time),
                                   'removed_root_travel_cm': removed_travel, 'bones': structural})
        for name in names:
            a = API['bone'](actual, name, unreal.AnimPoseSpaces.LOCAL)
            b = API['bone'](expected, name, unreal.AnimPoseSpaces.LOCAL)
            if name == 'root':
                max_root = max(max_root, math.dist(a['p'], root_origin_values))
            else:
                max_position = max(max_position, math.dist(a['p'], b['p']))
            max_scale = max(max_scale, math.dist(a['s'], b['s']))
            max_rotation = max(max_rotation, API['angle'](a['q'], b['q']))
    range_report = audit['sample_range'](target, 0., target.get_play_length(), options)
    if max_position > .001 or max_scale > .0001 or max_rotation > .05 or max_root > .001 or max_structural_position > .01 or range_report['turn_candidate']:
        raise RuntimeError('Safe derivative failed pose/preservation/turn gate: '+str((max_position, max_rotation, max_scale, max_root)))
    contact_check = None
    if spec['target_contact'] is not None:
        actual = unreal.AnimPoseExtensions.get_anim_pose_at_time(target, spec['target_contact'], options)
        expected = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, spec['source_contact'], options)
        actual_hand, expected_hand = API['bone'](actual, 'hand_r'), API['bone'](expected, 'hand_r')
        actual_root, expected_root = API['bone'](actual, 'root'), API['bone'](expected, 'root')
        actual_relative = API['transform'](unreal.MathLibrary.make_relative_transform(
            API['native'](actual_hand), API['native'](actual_root)))
        expected_relative = API['transform'](unreal.MathLibrary.make_relative_transform(
            API['native'](expected_hand), API['native'](expected_root)))
        hand_error = math.dist(actual_relative['p'], expected_relative['p'])
        hand_rotation_error = API['angle'](actual_relative['q'], expected_relative['q'])
        hand_scale_error = math.dist(actual_relative['s'], expected_relative['s'])
        if hand_error > .20 or hand_rotation_error > .05 or hand_scale_error > .0001:
            raise RuntimeError('Safe time map moves the reviewed root-relative contact: '+str(
                (hand_error, hand_rotation_error, hand_scale_error)))
        component_hand_error = math.dist(actual_hand['p'], expected_hand['p'])
        component_hand_rotation_error = API['angle'](actual_hand['q'], expected_hand['q'])
        component_hand_scale_error = math.dist(actual_hand['s'], expected_hand['s'])
        if component_hand_error > .20 or component_hand_rotation_error > .05 or component_hand_scale_error > .0001:
            raise RuntimeError('Safe time map moves the reviewed component-space contact: '+str(
                (component_hand_error, component_hand_rotation_error, component_hand_scale_error)))
        removed_translation = [expected_root['p'][i]-actual_root['p'][i] for i in range(3)]
        contact_check = {'target_montage_seconds': spec['target_contact'], 'source_seconds': spec['source_contact'],
                         'space': 'Complete hand transform relative to root; source first root origin is preserved and only subsequent travel is removed.',
                         'mapped_hand_error_cm': hand_error, 'mapped_hand_rotation_error_deg': hand_rotation_error,
                         'mapped_hand_scale_error': hand_scale_error,
                         'source_hand_root_relative': expected_relative, 'target_hand_root_relative': actual_relative,
                         'source_root_component': expected_root, 'target_root_component': actual_root,
                         'removed_root_travel_cm': removed_translation,
                         'removed_root_travel_length_cm': math.dist(removed_translation, [0., 0., 0.]),
                         'raw_hand_component_difference_cm': component_hand_error,
                         'hand_component_rotation_error_deg': component_hand_rotation_error,
                         'hand_component_scale_error': component_hand_scale_error}
    return target, {'spec': spec, 'range': range_report, 'max_source_local_position_error_cm': max_position,
                    'max_source_local_rotation_error_deg': max_rotation, 'max_scale_error': max_scale,
                    'preserved_first_root_translation_cm': root_origin_values,
                    'max_root_origin_position_error_cm': max_root,
                    'max_source_component_structural_position_error_cm': max_structural_position,
                    'full_body_height_samples': height_samples, 'contact': contact_check}


def validate_graph_change(before, after):
    normalized = json.loads(json.dumps(after))
    getters = []
    for graph_name, row in normalized.items():
        for node in list(row['nodes']):
            if node['class'] == 'K2Node_VariableGet' and any(p['name'] == 'GroundLocomotionPlayRate' for p in node['pins']):
                getters.append((graph_name, node['name']))
                row['nodes'].remove(node)
        for node in row['nodes']:
            if node['class'] == 'AnimGraphNode_BlendSpacePlayer':
                node['pins'] = [p for p in node['pins'] if p['name'] != 'PlayRate']
    if len(getters) != 1:
        raise RuntimeError('Expected exactly one native grounded cadence getter')
    graph_name, getter_name = getters[0]
    normalized[graph_name]['edges'] = [edge for edge in normalized[graph_name]['edges']
                                       if not any(endpoint[0] == getter_name for endpoint in edge)]
    if normalized != before:
        write('ground-rate-unexpected-graph-change.json', {'before': before, 'after': after, 'normalized': normalized})
        raise RuntimeError('Grounded cadence changed unrelated graph nodes/settings/links')


def main(apply=False):
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('Stop PIE before authoring')
    audit = json.loads((EVIDENCE/'asset-motion-audit.json').read_text(encoding='utf-8'))
    plan = manifest(audit)
    write('safe-motion-plan.json', plan)
    if not apply:
        unreal.log('RESULT=SAFE_HEAD_MOTION_PLAN_PASSED bindings=%d montages=%d' % (len(plan['bindings']), len(plan['montages'])))
        return plan
    # UE DuplicateAsset can immediately save through Source Control when its
    # provider is enabled. Require the disabled provider to preserve this
    # workflow's explicit validation-before-save boundary.
    if unreal.SourceControl.is_enabled():
        raise RuntimeError('Source Control must be disabled: DuplicateAsset may otherwise save before validation')
    checkpoint = ROOT/'tmp/stable-head/checkpoint'
    if not checkpoint.is_dir() or not any(checkpoint.rglob('ABP_Common_Unarmed.uasset')):
        raise RuntimeError('Required pre-change Content checkpoint missing')
    for spec in plan['sequences'] + plan['montages']:
        if unreal.EditorAssetLibrary.does_asset_exist(spec['target']):
            raise RuntimeError('Refusing replacement of an existing derivative: '+spec['target'])
    for row in audit['skills']:
        skill = require(row['path'], unreal.Rogue10mAttackSkillData)
        if {key: serial(skill.get_editor_property(key)) for key in row['fields']} != row['fields']:
            raise RuntimeError('Data Asset changed after read-only audit: '+row['path'])
    motion = require(MOTION_DATA)
    old_dodge = motion.get_editor_property('dodge_montage')
    if serial(old_dodge) != plan['dodge']['original']:
        raise RuntimeError('Dodge data changed after read-only audit')
    blueprint = require(BLUEPRINT, unreal.AnimBlueprint)
    before_graph = graph_snapshot(blueprint)
    if before_graph != audit['graphs']:
        raise RuntimeError('AnimBP changed after read-only audit')
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', require(MESH, unreal.SkeletalMesh))
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('should_retarget', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('extract_root_motion', False)
    outputs, sequence_checks, montage_checks = [], [], []
    originals = {}
    try:
        for spec in plan['sequences']:
            target, check = prepare_sequence(spec, options)
            outputs.append(target); sequence_checks.append(check)
        for spec in plan['montages']:
            original = require(spec['original'], unreal.AnimMontage)
            target = unreal.EditorAssetLibrary.duplicate_asset(original.get_path_name(), spec['target'])
            if not isinstance(target, unreal.AnimMontage):
                raise RuntimeError('Montage clone failed')
            originals[spec['target']] = policy(original)
            outputs.append(target)
        requests = '\n'.join('|'.join([spec['original'], spec['target'], spec['sequence']]) for spec in plan['montages'])
        (EVIDENCE/'safe-montage-requests.txt').write_text(requests, encoding='utf-8')
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, 'Rogue10m.ConfigureSafeHeadMontages')
        native = json.loads((EVIDENCE/'safe-montage-native.json').read_text(encoding='utf-8-sig'))
        if native['result'] != 'PASSED' or len(native['montages']) != len(plan['montages']):
            raise RuntimeError('Native safe montage substitution failed: '+native.get('error', ''))
        if not all(row.get('sections_preserved') and row.get('notifies_preserved') for row in native['montages']):
            raise RuntimeError('Native section/notify preservation proof missing')
        for spec in plan['montages']:
            target = require(spec['target'], unreal.AnimMontage)
            actual, expected = policy(target), json.loads(json.dumps(originals[spec['target']]))
            segment = expected['tracks'][0]['segments'][0]
            sequence = require(spec['sequence'], unreal.AnimSequence)
            segment['anim_reference'] = sequence.get_path_name()
            segment['anim_end_time'] = sequence.get_play_length()
            if spec['dodge']:
                expected['blend_in'] = actual['blend_in']
                expected['blend_out'] = actual['blend_out']
                if abs(target.get_editor_property('blend_in').get_editor_property('blend_time')-.04) > .0001 or abs(target.get_editor_property('blend_out').get_editor_property('blend_time')-.08) > .0001:
                    raise RuntimeError('Dodge blend timing differs')
            # Section exports can contain the owning montage asset; normalize only that exact object path.
            actual_text = json.dumps(actual, ensure_ascii=False).replace(target.get_path_name(), spec['original'])
            if json.loads(actual_text) != expected:
                write('safe-montage-policy-mismatch.json', {'spec': spec, 'expected': expected, 'actual': actual})
                raise RuntimeError('Montage slot/sections/notifies/policy changed outside reviewed scope')
            montage_checks.append({'spec': spec, 'policy_preserved': True, 'length': target.get_play_length()})
        for row in plan['bindings']:
            require(row['skill']).set_editor_property('attack_montage', require(row['target'], unreal.AnimMontage))
        motion.set_editor_property('dodge_montage', require(plan['dodge']['target'], unreal.AnimMontage))
        for row in audit['skills']:
            actual = {key: serial(require(row['path']).get_editor_property(key)) for key in row['fields']}
            expected = dict(row['fields'])
            binding = next((b for b in plan['bindings'] if b['skill'] == row['path']), None)
            if binding:
                expected['attack_montage'] = require(binding['target']).get_path_name()
            if actual != expected:
                raise RuntimeError('Attack gameplay fields changed: '+row['path'])
        unreal.SystemLibrary.execute_console_command(world, 'Rogue10m.ConfigureGroundLocomotionRate')
        graph_native = json.loads((EVIDENCE/'ground-rate-native.json').read_text(encoding='utf-8-sig'))
        if graph_native['result'] != 'PASSED':
            raise RuntimeError('Grounded rate native compile failed: '+graph_native.get('error', ''))
        after_graph = graph_snapshot(blueprint)
        validate_graph_change(before_graph, after_graph)
        # Save new derivatives first, then the validated references and final AnimBP.
        for asset in outputs + [require(row['skill']) for row in plan['bindings']] + [motion, blueprint]:
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError('Validated asset save failed: '+asset.get_path_name())
        write('safe-motion-application.json', {'result': 'PASSED', 'plan': plan, 'sequence_checks': sequence_checks,
            'montage_checks': montage_checks, 'graphs_before': before_graph, 'graphs_after': after_graph,
            'attack_fields_preserved': len(audit['skills']), 'saved': [asset.get_path_name() for asset in outputs]
            + [row['skill'] for row in plan['bindings']] + [motion.get_path_name(), blueprint.get_path_name()]})
        unreal.log('RESULT=SAFE_HEAD_MOTION_APPLICATION_PASSED bindings=%d montages=%d sequences=%d' %
                   (len(plan['bindings']), len(plan['montages']), len(plan['sequences'])))
    except Exception as exc:
        # Keep the original gameplay references in memory even if authoring fails.
        # New derivatives are never written until all preceding checks pass.
        for row in plan['bindings']:
            require(row['skill']).set_editor_property('attack_montage', require(row['original']))
        motion.set_editor_property('dodge_montage', old_dodge)
        write('safe-motion-failure.json', {'error': str(exc), 'original_data_references_restored_in_memory': True,
            'derivatives_in_memory': [asset.get_path_name() for asset in outputs],
            'note': 'No originals are deleted. If failure occurred during saving, use the checkpoint through Unreal Editor APIs.'})
        raise


if __name__ == '__main__':
    main(apply=False)
