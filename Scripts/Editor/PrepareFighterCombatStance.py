"""Create and wire the separate fighter combat idle through the UE editor.

Default is a read-only reload audit. -RogueFighterStanceDiscover inspects graph
actions only. -RogueFighterStancePrepare creates a new sequence and reparents
the existing source AnimBP; it refuses to replace any existing stance package.
Original attack, locomotion, appearance, and gameplay Data Assets are never saved.
"""
import hashlib
import json
import math
import runpy
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
EVIDENCE = ROOT / 'Feature/doc/evidence/fighter-combat-stance-20261005'
SOURCE = '/Game/Rogue10m/Animation/Combat/FistAuthored/A_Unarmed_LeftJab'
MESH = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
FOLDER = '/Game/Rogue10m/Animation/Combat/FighterStance'
TARGET = FOLDER + '/A_Fighter_CombatIdle'
BLUEPRINT = '/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed'
PARENT = '/Script/Rogue10m.Rogue10mFighterCombatStanceAnimInstance'
DURATION = 2.0
SAMPLE_RATE = 120
BREATH_PITCH_DEGREES = 0.3
PIN = unreal.BlueprintGraphPinLibrary
INPUT = unreal.EdGraphPinDirection.EGPD_INPUT
OUTPUT = unreal.EdGraphPinDirection.EGPD_OUTPUT
IK = [('ik_hand_gun', 'hand_r'), ('ik_hand_l', 'hand_l'),
      ('ik_hand_r', 'hand_r'), ('ik_foot_l', 'foot_l'), ('ik_foot_r', 'foot_r')]
FEET = ['root', 'pelvis', 'thigh_l', 'calf_l', 'foot_l', 'ball_l',
        'thigh_r', 'calf_r', 'foot_r', 'ball_r']
LIMBS = [('upperarm_l', 'lowerarm_l'), ('lowerarm_l', 'hand_l'),
         ('upperarm_r', 'lowerarm_r'), ('lowerarm_r', 'hand_r'),
         ('thigh_l', 'calf_l'), ('calf_l', 'foot_l'),
         ('thigh_r', 'calf_r'), ('calf_r', 'foot_r')]


def require(path, cls=None):
    asset = unreal.load_asset(path)
    if asset is None or cls and not isinstance(asset, cls):
        raise RuntimeError('Missing/unexpected asset ' + path)
    return asset


def report(name, data):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')


def numbers(value, fields):
    return [float(getattr(value, field)) for field in fields]


def transform(value):
    out = {'p': numbers(value.translation, 'xyz'), 'q': numbers(value.rotation, 'xyzw'),
           's': numbers(value.scale3d, 'xyz')}
    if not all(math.isfinite(v) for values in out.values() for v in values):
        raise RuntimeError('Nonfinite bone transform')
    if abs(sum(v * v for v in out['q']) - 1.0) > 1e-4:
        raise RuntimeError('Unnormalized bone quaternion')
    return out


def native(value):
    out = unreal.Transform()
    out.translation = unreal.Vector(*value['p'])
    out.rotation = unreal.Quat(*value['q'])
    out.scale3d = unreal.Vector(*value['s'])
    return out


def quat_product(a, b):
    x, y, z, w = a
    u, v, s, t = b
    q = [w*u+x*t+y*s-z*v, w*v-x*s+y*t+z*u,
         w*s+x*v-y*u+z*t, w*t-x*u-y*v-z*s]
    norm = math.sqrt(sum(n*n for n in q))
    return [n/norm for n in q]


def inverse(q):
    return [-q[0], -q[1], -q[2], q[3]]


def rotate(q, p):
    # Unit quaternion rotation, without normalizing a pure-vector quaternion.
    u = q[:3]
    uv = [u[1]*p[2]-u[2]*p[1], u[2]*p[0]-u[0]*p[2], u[0]*p[1]-u[1]*p[0]]
    uuv = [u[1]*uv[2]-u[2]*uv[1], u[2]*uv[0]-u[0]*uv[2], u[0]*uv[1]-u[1]*uv[0]]
    return [p[i] + 2.0 * (q[3]*uv[i]+uuv[i]) for i in range(3)]


def component(local, names, parents):
    out = {}
    for name in names:
        value, parent = local[name], parents[name]
        if parent is None:
            out[name] = {key: list(values) for key, values in value.items()}
            continue
        basis = out[parent]
        offset = rotate(basis['q'], [value['p'][i]*basis['s'][i] for i in range(3)])
        out[name] = {'p': [basis['p'][i]+offset[i] for i in range(3)],
                     'q': quat_product(basis['q'], value['q']),
                     's': [basis['s'][i]*value['s'][i] for i in range(3)]}
    return out


def angle(a, b):
    norm = math.sqrt(sum(v*v for v in a)*sum(v*v for v in b))
    return math.degrees(2.0 * math.acos(min(1.0, abs(sum(x*y for x, y in zip(a, b))) / norm)))


def eval_options():
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', require(MESH, unreal.SkeletalMesh))
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    return options


def sample(sequence, time, options):
    return unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)


def bone(pose, name, space=unreal.AnimPoseSpaces.WORLD):
    return transform(unreal.AnimPoseExtensions.get_bone_pose(pose, name, space))


def hierarchy(sequence, pose):
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
    parents = {}
    for name in names:
        path = [str(n) for n in unreal.AnimationLibrary.find_bone_path_to_root(sequence, name)]
        if not path or path[0] != name:
            raise RuntimeError('Unexpected bone hierarchy ' + name)
        parents[name] = path[1] if len(path) > 1 else None
    # Ensure the reflected pose's order can be used for parent-first composition.
    seen = set()
    for name in names:
        if parents[name] is not None and parents[name] not in seen:
            raise RuntimeError('Unexpected bone evaluation order ' + name)
        seen.add(name)
    return names, parents


def protected_hashes():
    allowed = {ROOT/'Content/Rogue10m/Animation/Common/ABP_Common_Unarmed.uasset',
               ROOT/'Content/Rogue10m/Animation/Combat/FighterStance/A_Fighter_CombatIdle.uasset'}
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (ROOT/'Content').rglob('*.uasset') if p not in allowed}


def graph_context():
    bp = require(BLUEPRINT, unreal.AnimBlueprint)
    graph = next(g for g in unreal.BlueprintEditorLibrary.list_graphs(bp) if g.get_name() == 'AnimGraph')
    return bp, unreal.BlueprintGraphEditor.get_graph_editor(graph)


def pin_id(pin):
    return (PIN.get_owning_node(pin).get_name(), str(PIN.get_pin_name(pin)), str(PIN.get_pin_direction(pin)))


def edge(a, b):
    return tuple(sorted((pin_id(a), pin_id(b))))


def edges(nodes):
    return {edge(pin, other) for node in nodes for pin in unreal.BlueprintEditorLibrary.list_all_pins(node)
            for other in PIN.list_connected_pins(pin)}


def one_pin(node, name, direction):
    matches = [pin for pin in unreal.BlueprintEditorLibrary.list_all_pins(node)
               if str(PIN.get_pin_name(pin)) == name and PIN.get_pin_direction(pin) == direction]
    if len(matches) != 1:
        raise RuntimeError('Expected unique pin ' + node.get_name() + ':' + name)
    return matches[0]


def one_link(pin):
    links = list(PIN.list_connected_pins(pin))
    if len(links) != 1:
        raise RuntimeError('Expected one linked pin ' + str(pin_id(pin)))
    return links[0]


def unique(nodes, class_name):
    matches = [node for node in nodes if node.get_class().get_name() == class_name]
    if len(matches) != 1:
        raise RuntimeError('Expected unique graph node ' + class_name)
    return matches[0]


def graph_dump(editor):
    rows = []
    for node in editor.list_all_nodes():
        row = {'name': node.get_name(), 'class': node.get_class().get_name(),
               'pins': [{'name': str(PIN.get_pin_name(p)), 'direction': str(PIN.get_pin_direction(p)),
                         'type': str(PIN.get_pin_type_display_string(p)), 'value': PIN.get_pin_value(p)}
                        for p in unreal.BlueprintEditorLibrary.list_all_pins(node)]}
        field = 'blend_node' if isinstance(node, unreal.AnimGraphNode_TwoWayBlend) else 'node'
        try:
            row['runtime'] = node.get_editor_property(field).export_text()
        except Exception:
            pass
        rows.append(row)
    return {'nodes': rows, 'edges': sorted(edges(list(editor.list_all_nodes())))}


def discover():
    bp, editor = graph_context()
    result = {'read_only': True, 'blueprint': BLUEPRINT,
              'parent': unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_path_name(),
              'new_parent_available': unreal.load_class(None, PARENT) is not None,
              'actions': list(editor.list_available_nodes([])), 'graph': graph_dump(editor)}
    report('graph-discovery.json', result)
    unreal.log('RESULT=FIGHTER_COMBAT_STANCE_DISCOVERY_PASSED')


def create_stance(options):
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        raise RuntimeError('Refusing to replace existing combat stance ' + TARGET)
    source = require(SOURCE, unreal.AnimSequence)
    if unreal.AnimationLibrary.get_animation_notify_events(source):
        raise RuntimeError('Combat guard source unexpectedly has notifies')
    pose = sample(source, 0.0, options)
    names, parents = hierarchy(source, pose)
    base = {name: bone(pose, name, unreal.AnimPoseSpaces.LOCAL) for name in names}
    base_world = component(base, names, parents)
    for name in names:
        observed = bone(pose, name)
        if math.dist(base_world[name]['p'], observed['p']) > .001 or angle(base_world[name]['q'], observed['q']) > .001:
            raise RuntimeError('Local-to-component composition differs from UE evaluation ' + name)
    frame_count = int(DURATION * SAMPLE_RATE)
    keys = {name: {'p': [], 'q': [], 's': []} for name in names}
    for frame in range(frame_count + 1):
        local = {name: {key: list(values) for key, values in value.items()} for name, value in base.items()}
        breath = math.radians(BREATH_PITCH_DEGREES * math.sin(2.0*math.pi*frame/frame_count))
        # A subtle thoracic breath; root, pelvis, legs, wrists and fingers keep
        # the exact source guard. The head retains its component orientation.
        delta = [math.sin(breath*.5), 0.0, 0.0, math.cos(breath*.5)]
        desired = quat_product(delta, base_world['spine_03']['q'])
        local['spine_03']['q'] = quat_product(inverse(base_world[parents['spine_03']]['q']), desired)
        world = component(local, names, parents)
        local['head']['q'] = quat_product(inverse(world[parents['head']]['q']), base_world['head']['q'])
        world = component(local, names, parents)
        # Synchronize anchors to final rendered FK. Parents of IK hands are
        # themselves IK anchors, so process their final world transforms first.
        final_ik = {anchor: world[goal] for anchor, goal in IK}
        for anchor, _ in IK:
            parent = parents[anchor]
            basis = final_ik[parent] if parent in final_ik else world[parent]
            local[anchor] = transform(unreal.MathLibrary.make_relative_transform(native(final_ik[anchor]), native(basis)))
        for name in names:
            keys[name]['p'].append(unreal.Vector(*local[name]['p']))
            keys[name]['q'].append(unreal.Quat(*local[name]['q']))
            keys[name]['s'].append(unreal.Vector(*local[name]['s']))
    target = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
    if not isinstance(target, unreal.AnimSequence):
        raise RuntimeError('Combat stance duplicate failed')
    controller = target.get_editor_property('controller')
    controller.open_bracket('Prepare stable fighter combat idle', False)
    try:
        controller.remove_all_bone_tracks(False)
        controller.set_frame_rate(unreal.FrameRate(SAMPLE_RATE, 1), False)
        controller.set_number_of_frames(unreal.FrameNumber(frame_count), False)
        for name, values in keys.items():
            if not controller.add_bone_curve(name, False) or not controller.set_bone_track_keys(
                    name, values['p'], values['q'], values['s'], False):
                raise RuntimeError('Cannot write combat idle bone ' + name)
    finally:
        controller.close_bracket(False)
    target.set_editor_property('enable_root_motion', False)
    target.set_editor_property('force_root_lock', True)
    # Existing guard has an authored root offset; lock to that first frame so
    # the combat idle cannot displace the character or change the attack seam.
    target.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    target.set_editor_property('rate_scale', 1.0)
    target.set_editor_property('loop', True)
    return target


def validate_stance(target, options):
    source = require(SOURCE, unreal.AnimSequence)
    guard = sample(source, 0.0, options)
    names, _ = hierarchy(source, guard)
    if target.get_editor_property('skeleton') != source.get_editor_property('skeleton'):
        raise RuntimeError('Combat stance skeleton differs')
    if abs(target.get_play_length()-DURATION) > 1e-5:
        raise RuntimeError('Combat stance duration differs')
    if target.get_editor_property('enable_root_motion') or not target.get_editor_property('force_root_lock'):
        raise RuntimeError('Combat stance root motion flags differ')
    if target.get_editor_property('root_motion_root_lock') != unreal.RootMotionRootLock.ANIM_FIRST_FRAME:
        raise RuntimeError('Combat stance must retain its fixed authored root offset')
    if unreal.AnimationLibrary.get_animation_notify_events(target):
        raise RuntimeError('Combat idle unexpectedly contains notifies')
    errors = {key: 0.0 for key in ['render_local_translation_cm', 'render_local_scale',
        'protected_local_rotation_deg', 'planted_position_cm', 'planted_rotation_deg',
        'limb_length_cm', 'head_rotation_deg', 'head_travel_cm', 'ik_position_cm', 'ik_rotation_deg',
        'loop_position_cm', 'loop_rotation_deg', 'guard_start_position_cm', 'guard_start_rotation_deg']}
    traces = []
    reference = {name: bone(guard, name) for name in names}
    reference_local = {name: bone(guard, name, unreal.AnimPoseSpaces.LOCAL) for name in names}
    first, last = sample(target, 0.0, options), sample(target, DURATION, options)
    for name in names:
        a, b = bone(first, name), bone(last, name)
        errors['loop_position_cm'] = max(errors['loop_position_cm'], math.dist(a['p'], b['p']))
        errors['loop_rotation_deg'] = max(errors['loop_rotation_deg'], angle(a['q'], b['q']))
        errors['guard_start_position_cm'] = max(errors['guard_start_position_cm'], math.dist(a['p'], reference[name]['p']))
        errors['guard_start_rotation_deg'] = max(errors['guard_start_rotation_deg'], angle(a['q'], reference[name]['q']))
    for frame in range(int(DURATION * SAMPLE_RATE) + 1):
        time = frame / SAMPLE_RATE
        pose = sample(target, time, options)
        world = {name: bone(pose, name) for name in names}
        for name in names:
            if name.startswith('ik_'):
                continue
            value, ref = bone(pose, name, unreal.AnimPoseSpaces.LOCAL), reference_local[name]
            errors['render_local_translation_cm'] = max(errors['render_local_translation_cm'], math.dist(value['p'], ref['p']))
            errors['render_local_scale'] = max(errors['render_local_scale'], math.dist(value['s'], ref['s']))
            if name not in ['spine_03', 'head']:
                errors['protected_local_rotation_deg'] = max(errors['protected_local_rotation_deg'], angle(value['q'], ref['q']))
        for name in FEET:
            value, ref = world[name], reference[name]
            errors['planted_position_cm'] = max(errors['planted_position_cm'], math.dist(value['p'], ref['p']))
            errors['planted_rotation_deg'] = max(errors['planted_rotation_deg'], angle(value['q'], ref['q']))
        for a, b in LIMBS:
            errors['limb_length_cm'] = max(errors['limb_length_cm'], abs(math.dist(world[a]['p'], world[b]['p'])-
                math.dist(reference[a]['p'], reference[b]['p'])))
        for anchor, goal in IK:
            errors['ik_position_cm'] = max(errors['ik_position_cm'], math.dist(world[anchor]['p'], world[goal]['p']))
            errors['ik_rotation_deg'] = max(errors['ik_rotation_deg'], angle(world[anchor]['q'], world[goal]['q']))
        errors['head_rotation_deg'] = max(errors['head_rotation_deg'], angle(world['head']['q'], reference['head']['q']))
        errors['head_travel_cm'] = max(errors['head_travel_cm'], math.dist(world['head']['p'], reference['head']['p']))
        traces.append({'time': time, 'head': world['head'], 'left': world['hand_l'], 'right': world['hand_r'],
                       'foot_left': world['foot_l'], 'foot_right': world['foot_r']})
    for key, value in errors.items():
        limit = .5 if key == 'head_travel_cm' else .02
        if not math.isfinite(value) or value > limit:
            raise RuntimeError('Invalid combat idle ' + key + '=' + str(value))
    hand_travel = {side: max(math.dist(row[side]['p'], traces[0][side]['p']) for row in traces)
                   for side in ['left', 'right']}
    if any(value < .01 or value > 1.25 for value in hand_travel.values()):
        raise RuntimeError('Combat idle breath motion exceeds planned amplitude ' + str(hand_travel))
    return {'sequence': target.get_path_name(), 'source': SOURCE, 'evaluation': 'Raw editor AnimPose',
            'duration_seconds': DURATION, 'sample_hz': SAMPLE_RATE, 'sample_count': len(traces),
            'breath_pitch_degrees': BREATH_PITCH_DEGREES, 'errors': errors,
            'hand_breath_travel_cm': hand_travel, 'traces': traces}


def action(editor, cls, fragments):
    available = list(editor.list_available_nodes([]))
    matches = [name for name in available if any(fragment in name.replace(' ', '').lower() for fragment in fragments)]
    if len(matches) != 1:
        report('unresolved-graph-actions.json', {'class': cls.get_name(), 'matches': matches, 'available': available})
        raise RuntimeError('Expected unique graph action for ' + cls.get_name() + ': ' + str(matches))
    return matches[0]


def wire_graph(target):
    bp, editor = graph_context()
    parent = unreal.load_class(None, PARENT)
    if parent is None:
        raise RuntimeError('Native combat stance class unavailable; build Editor first')
    old_nodes = list(editor.list_all_nodes())
    if any(isinstance(n, unreal.AnimGraphNode_TwoWayBlend) for n in old_nodes):
        raise RuntimeError('Refusing to replace an existing source stance blend')
    slot, rig, root = [unique(old_nodes, name) for name in
        ['AnimGraphNode_Slot', 'AnimGraphNode_ControlRig', 'AnimGraphNode_Root']]
    if str(slot.get_editor_property('node').get_editor_property('slot_name')) != 'DefaultSlot':
        raise RuntimeError('Unexpected source attack slot')
    if 'CR_Mannequin_FootIK' not in rig.get_editor_property('node').export_text():
        raise RuntimeError('Unexpected locomotion foot IK')
    slot_in = one_pin(slot, 'Source', INPUT)
    upstream = one_link(slot_in)
    if not PIN.is_same_native_pin(upstream, one_pin(rig, 'Pose', OUTPUT)):
        raise RuntimeError('Expected FootIK before DefaultSlot')
    if not PIN.is_same_native_pin(one_link(one_pin(root, 'Result', INPUT)), one_pin(slot, 'Pose', OUTPUT)):
        raise RuntimeError('Expected DefaultSlot to remain final')
    before = edges(old_nodes)
    properties = {node.get_name(): node.get_editor_property('node').export_text() for node in [slot, rig]}
    old_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_path_name()
    unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent)
    if not unreal.BlueprintEditorLibrary.compile_blueprint(bp):
        raise RuntimeError('Reparented source AnimBP compile failed')
    blend = editor.create_node_from_name(action(editor, unreal.AnimGraphNode_TwoWayBlend,
        ['twowayblend', '양방향블렌드', '투웨이블렌드']), unreal.Vector2D(350.0, 0.0), [], unreal.AnimGraphNode_TwoWayBlend)
    player = editor.create_node_from_name(action(editor, unreal.AnimGraphNode_SequencePlayer,
        ['|sequenceplayer', '|시퀀스플레이어']), unreal.Vector2D(0.0, 240.0), [], unreal.AnimGraphNode_SequencePlayer)
    if not isinstance(blend, unreal.AnimGraphNode_TwoWayBlend) or not isinstance(player, unreal.AnimGraphNode_SequencePlayer):
        raise RuntimeError('Unable to create combat stance animation nodes')
    runtime = player.get_editor_property('node')
    runtime.set_editor_property('sequence', target)
    runtime.set_editor_property('loop_animation', True)
    runtime.set_editor_property('play_rate', 1.0)
    player.set_editor_property('node', runtime)
    getter = editor.add_get_member_variable_node('FighterCombatStanceAlpha', PARENT)
    if not isinstance(getter, unreal.K2Node_VariableGet):
        raise RuntimeError('Unable to create native stance alpha getter')
    a, b, alpha = [one_pin(blend, name, INPUT) for name in ['A', 'B', 'Alpha']]
    blend_out = one_pin(blend, 'Pose', OUTPUT)
    pairs = [(upstream, a), (one_pin(player, 'Pose', OUTPUT), b),
             (one_pin(getter, 'FighterCombatStanceAlpha', OUTPUT), alpha), (blend_out, slot_in)]
    if not PIN.break_single_pin_link(upstream, slot_in):
        raise RuntimeError('Unable to split expected FootIK -> DefaultSlot edge')
    for left, right in pairs:
        if not PIN.try_create_connection(left, right):
            raise RuntimeError('Unable to create combat stance graph edge')
    expected = (before - {edge(upstream, slot_in)}) | {edge(left, right) for left, right in pairs}
    new_nodes = list(editor.list_all_nodes())
    if len(new_nodes) != len(old_nodes) + 3 or any(node not in new_nodes for node in old_nodes):
        raise RuntimeError('Unexpected source graph node churn')
    for node in [slot, rig]:
        if properties[node.get_name()] != node.get_editor_property('node').export_text():
            raise RuntimeError('Original slot or FootIK settings changed')
    if edges(new_nodes) != expected:
        raise RuntimeError('Unexpected source graph link churn')
    if not unreal.BlueprintEditorLibrary.compile_blueprint(bp) or editor.list_nodes_with_errors():
        raise RuntimeError('Integrated source stance graph compile failed')
    if edges(list(editor.list_all_nodes())) != expected:
        raise RuntimeError('Integrated source graph links changed during compile')
    return bp, {'old_parent': old_parent, 'parent': PARENT, 'alpha': 'FighterCombatStanceAlpha',
                'sequence_binding': 'Constant sequence asset, same path as native loaded stance animation',
                'added_nodes': [node.get_name() for node in [blend, player, getter]],
                'before_edges': sorted(before), 'after': graph_dump(editor), 'compiled': True,
                'warning_nodes': [node.get_name() for node in editor.list_nodes_with_warnings()]}


def validate_graph(target):
    bp, editor = graph_context()
    if unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_path_name() != PARENT:
        raise RuntimeError('Source AnimBP native stance parent missing')
    nodes = list(editor.list_all_nodes())
    moving_layer = any(isinstance(n, unreal.AnimGraphNode_LayeredBoneBlend) for n in nodes)
    if moving_layer:
        import ConfigureMovingAttackUpperBody as moving
        moving_graph = moving.validate_graph()
        blend = moving.alpha_blend(nodes, 'FighterCombatStanceAlpha')
    else:
        blend = unique(nodes, 'AnimGraphNode_TwoWayBlend')
    slot = unique(nodes, 'AnimGraphNode_Slot')
    rig = unique(nodes, 'AnimGraphNode_ControlRig')
    root = unique(nodes, 'AnimGraphNode_Root')
    player = unique(nodes, 'AnimGraphNode_SequencePlayer')
    getter = PIN.get_owning_node(one_link(one_pin(blend, 'Alpha', INPUT)))
    links = [(one_pin(rig, 'Pose', OUTPUT), one_pin(blend, 'A', INPUT)),
             (one_pin(player, 'Pose', OUTPUT), one_pin(blend, 'B', INPUT)),
             (one_pin(getter, 'FighterCombatStanceAlpha', OUTPUT), one_pin(blend, 'Alpha', INPUT))]
    if not moving_layer:
        links.extend([(one_pin(blend, 'Pose', OUTPUT), one_pin(slot, 'Source', INPUT)),
                      (one_pin(slot, 'Pose', OUTPUT), one_pin(root, 'Result', INPUT))])
    for output, input_pin in links:
        if not PIN.is_same_native_pin(one_link(input_pin), output):
            raise RuntimeError('Unexpected stance integration edge ' + str(edge(output, input_pin)))
    runtime = player.get_editor_property('node')
    if runtime.get_editor_property('sequence') != target or not runtime.get_editor_property('loop_animation'):
        raise RuntimeError('Combat idle player binding or loop flag differs')
    if editor.list_nodes_with_errors():
        raise RuntimeError('Saved source graph has compiler errors')
    return {'blueprint': BLUEPRINT, 'parent': PARENT, 'sequence': target.get_path_name(),
            'graph': graph_dump(editor), 'default_slot_final': not moving_layer,
            'moving_attack_upper_body': moving_graph if moving_layer else None, 'read_only': True}


def main():
    line = unreal.SystemLibrary.get_command_line()
    if '-RogueFighterStanceDiscover' in line:
        discover()
        return
    original = protected_hashes()
    options = eval_options()
    apply = '-RogueFighterStancePrepare' in line
    if apply:
        if not (EVIDENCE/'checkpoint/Content/Rogue10m/Animation/Common/ABP_Common_Unarmed.uasset').is_file():
            raise RuntimeError('Expected source AnimBP checkpoint missing')
        target = create_stance(options)
        pose_result = validate_stance(target, options)
        bp, graph_result = wire_graph(target)
        validate_graph(target)
        # All in-memory checks have succeeded before either permitted package save.
        for asset in [target, bp]:
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError('Unable to save verified combat stance ' + asset.get_path_name())
        phase = 'apply'
    else:
        target = require(TARGET, unreal.AnimSequence)
        pose_result = validate_stance(target, options)
        graph_result = validate_graph(target)
        # The existing generator must recognize the new intermediate stance
        # blend without rewiring locomotion IK or the final montage slot.
        runpy.run_path(str(ROOT/'Scripts/Editor/ConfigureBrawlerSourceFootIKOrder.py'),
                       run_name='rogue_stance_order_audit')['main'](apply=False)
        graph_result['legacy_foot_ik_order_audit_passed'] = True
        phase = 'reload'
    if protected_hashes() != original:
        raise RuntimeError('An original attack/idle/appearance/Data Asset package changed')
    report(phase+'-validation.json', {'result': 'PASSED', 'phase': phase,
           'protected_package_count': len(original), 'unchanged_protected_packages': True,
           'pose': pose_result, 'graph': graph_result})
    marker = 'PREPARE' if apply else 'AUDIT'
    unreal.log('RESULT=FIGHTER_COMBAT_STANCE_'+marker+'_PASSED')


if __name__ == '__main__':
    try:
        main()
    finally:
        unreal.SystemLibrary.quit_editor()
