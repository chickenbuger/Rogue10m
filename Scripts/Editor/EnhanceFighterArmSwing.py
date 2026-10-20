"""Increase the existing fighter arm arcs without changing combat timing or data.

Default: capture a read-only 120 Hz baseline once.
-RogueFistSwingApply: bake the nine authored clips and the existing two-punch
derivative in memory, validate every clip, then save only those ten sequences.
-RogueFistSwingAudit: independently resample the saved clips in a fresh Editor.
All reports stay in this task's new evidence folder. No old authoring main runs.
"""
import hashlib
import json
import math
from pathlib import Path
import re
import runpy
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EXPECTED_ROOT = Path(r'D:\Project\Rogue10m').resolve()
EVIDENCE = ROOT / 'Feature/doc/evidence/fighter-arm-swing-20261005'
AUTHORED = '/Game/Rogue10m/Animation/Combat/FistAuthored/'
FIGHTER = '/Game/Rogue10m/Animation/Combat/FighterSkills/'
SOURCE = '/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingLeftJab_Appearance'
MANNY = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
BRAWLER = '/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_'
KNUCKLE = '/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_'
SPECS = [
    ('Unarmed_LeftJab', 'left_jab', BRAWLER + 'LeftJab'),
    ('Unarmed_RightJab', 'right_jab', BRAWLER + 'RightJab'),
    ('Unarmed_Straight', 'straight', BRAWLER + 'RightStraight'),
    ('Unarmed_Hook', 'hook', BRAWLER + 'RightHook'),
    ('Knuckle_Jab', 'right_jab', KNUCKLE + 'Primary01'),
    ('Knuckle_BodyStraight', 'body', KNUCKLE + 'Primary02'),
    ('Knuckle_HookFlurry', 'flurry', KNUCKLE + 'Primary03'),
    ('Knuckle_Uppercut', 'uppercut', KNUCKLE + 'Special'),
    ('Knuckle_ShortBlow', 'heavy', KNUCKLE + 'Charged'),
]
IRON_NAME = 'IronChain_TwoPunch'
IRON_SEQUENCE = FIGHTER + 'A_Fighter_' + IRON_NAME
IRON_MONTAGE = FIGHTER + 'AM_Fighter_' + IRON_NAME
IRON_SKILL = '/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_Straight'
PROFILES = ['/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed',
            '/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle']
LIMBS = [('upperarm_l', 'lowerarm_l'), ('lowerarm_l', 'hand_l'),
         ('upperarm_r', 'lowerarm_r'), ('lowerarm_r', 'hand_r'),
         ('thigh_l', 'calf_l'), ('calf_l', 'foot_l'),
         ('thigh_r', 'calf_r'), ('calf_r', 'foot_r')]
STANCE = ['root', 'pelvis', 'thigh_l', 'calf_l', 'foot_l', 'ball_l',
          'thigh_r', 'calf_r', 'foot_r', 'ball_r']
IK = [('ik_hand_gun', 'hand_r'), ('ik_hand_l', 'hand_l'),
      ('ik_hand_r', 'hand_r'), ('ik_foot_l', 'foot_l'), ('ik_foot_r', 'foot_r')]
ARM = ['upperarm_l', 'lowerarm_l', 'hand_l', 'upperarm_r', 'lowerarm_r', 'hand_r']
WORLD_BONES = sorted(set(STANCE + ARM + ['head', 'spine_03'] + [n for n, _ in IK]))
SEQUENCE_FIELDS = ['skeleton', 'rate_scale', 'enable_root_motion', 'force_root_lock',
                   'root_motion_root_lock', 'platform_target_frame_rate']
MONTAGE_FIELDS = ['skeleton', 'rate_scale', 'blend_in', 'blend_out',
                  'blend_out_trigger_time', 'enable_auto_blend_out']


def require(path, cls=None):
    asset = unreal.load_asset(path)
    if asset is None or (cls and not isinstance(asset, cls)):
        raise RuntimeError('Missing or unexpected asset: ' + path)
    return asset


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
    if hasattr(value, 'export_text'):
        return value.export_text()
    return str(value)


def write(name, value):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


def read(name):
    return json.loads((EVIDENCE / name).read_text(encoding='utf-8'))


def property_fields(relative, class_name):
    text = (ROOT / relative).read_text(encoding='utf-8')
    text = text[text.index('class ROGUE10M_API ' + class_name):]
    # Match a complete field declaration, including nested template/map types.
    declarations = re.findall(r'^\s*[\w:]+(?:<[^;\n]+>)?\s+(\w+)\s*(?:=[^;]*)?;', text, re.M)
    def snake(name):
        if name.startswith('b') and len(name) > 1 and name[1].isupper():
            name = name[1:]
        return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()
    return [snake(name) for name in declarations]


def object_fields(asset, names):
    # Required fields fail closed instead of silently weakening the contract.
    return {name: serial(asset.get_editor_property(name)) for name in names}


def montage_tracks(montage):
    result = []
    for slot in montage.get_editor_property('slot_anim_tracks'):
        segments = slot.get_editor_property('anim_track').get_editor_property('anim_segments')
        result.append({'slot': str(slot.get_editor_property('slot_name')),
                       'segments': [object_fields(segment, ['anim_reference', 'start_pos',
                           'anim_start_time', 'anim_end_time', 'anim_play_rate', 'looping_count'])
                           for segment in segments]})
    return result


def asset_metadata(sequence, montage):
    # CompositeSections is not exposed to Python in UE 5.8. The reflected
    # BlueprintPure accessors expose count/order/names; the unchanged montage
    # package checksum additionally protects section timing and next links.
    section_count = montage.get_num_sections()
    sections = [str(montage.get_section_name(index)) for index in range(section_count)]
    package_path = montage.get_path_name().split('.')[0]
    package_file = ROOT / ('Content/' + package_path.removeprefix('/Game/') + '.uasset')
    return {'sequence': object_fields(sequence, SEQUENCE_FIELDS),
            'sequence_duration': sequence.get_play_length(),
            'sequence_notify_events': serial(unreal.AnimationLibrary.get_animation_notify_events(sequence)),
            'montage': object_fields(montage, MONTAGE_FIELDS),
            'montage_duration': montage.get_play_length(),
            'montage_sections': {'count': section_count, 'names': sections},
            'montage_package_sha256': hashlib.sha256(package_file.read_bytes()).hexdigest(),
            'tracks': montage_tracks(montage),
            'montage_notify_events': serial(unreal.AnimationLibrary.get_animation_notify_events(montage))}


def contract():
    keys = property_fields('Source/Rogue10m/Data/Rogue10mAttackSkillData.h', 'URogue10mAttackSkillData')
    profile_keys = property_fields('Source/Rogue10m/Data/Rogue10mSkillLoadoutDataAsset.h',
                                   'URogue10mWeaponSkillProfileDataAsset')
    if len(keys) < 50 or 'default_skill_bindings' not in profile_keys:
        raise RuntimeError('Incomplete C++ Data Asset field discovery')
    profiles, skills = {}, {}
    for path in PROFILES:
        profile = require(path)
        profiles[path] = object_fields(profile, profile_keys)
        candidates = list(profile.get_editor_property('skill_tree_skills'))
        candidates += list(profile.get_editor_property('initially_unlocked_skills'))
        candidates += list(profile.get_editor_property('default_skill_bindings').values())
        for skill in candidates:
            if skill:
                skills[skill.get_path_name()] = object_fields(skill, keys)
    for _, _, path in SPECS + [(IRON_NAME, 'two_punch', IRON_SKILL)]:
        skill = require(path)
        skills[skill.get_path_name()] = object_fields(skill, keys)
    return {'skill_fields': keys, 'profile_fields': profile_keys,
            'profiles': profiles, 'skills': skills}


def source_hashes():
    protected = [ROOT / 'Content/DataAsset/AttackSkill', ROOT / 'Content/DataAsset/SkillProfile',
                 ROOT / 'Content/Rogue10m/Animation/Combat/Boxing',
                 ROOT / 'Content/Rogue10m/Animation/Common',
                 ROOT / 'Content/Characters/Mannequins/Anims/Unarmed',
                 ROOT / 'Content/Rogue10m/Animation/Preview/Boxing']
    # Other fighter derivatives remain untouched; IronChain is the sole exception.
    excluded = ROOT / ('Content/' + IRON_SEQUENCE.removeprefix('/Game/') + '.uasset')
    result = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
              for folder in protected for p in folder.rglob('*.uasset')}
    for path in (ROOT / 'Content/Rogue10m/Animation/Combat/FighterSkills').glob('*.uasset'):
        if path != excluded:
            result[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    excluded_authored = {'A_' + name + '.uasset' for name, _, _ in SPECS}
    for path in (ROOT / 'Content/Rogue10m/Animation/Combat/FistAuthored').glob('*.uasset'):
        if path.name not in excluded_authored:
            result[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def rows():
    result = []
    for name, style, skill_path in SPECS:
        skill = require(skill_path)
        sequence = require(AUTHORED + 'A_' + name, unreal.AnimSequence)
        montage = require(AUTHORED + 'AM_' + name, unreal.AnimMontage)
        if skill.get_editor_property('attack_montage') != montage:
            raise RuntimeError('Unexpected active montage binding: ' + name)
        rate = float(skill.get_editor_property('animation_play_rate'))
        count = int(skill.get_editor_property('hit_count'))
        first = float(skill.get_editor_property('hit_start_delay_seconds'))
        spacing = float(skill.get_editor_property('hit_interval')) * rate
        if count != (6 if style == 'flurry' else 1) or rate <= 0.0:
            raise RuntimeError('Unexpected hit contract: ' + name)
        result.append({'name': name, 'style': style, 'sequence': sequence, 'montage': montage,
                       'skill': skill, 'rate': rate, 'spacing': spacing,
                       'contacts': [first + i * spacing for i in range(count)],
                       'active_hands': ['hand_l', 'hand_r'] if style == 'flurry'
                                       else ['hand_l'] if style == 'left_jab' else ['hand_r']})
    skill = require(IRON_SKILL)
    sequence, montage = require(IRON_SEQUENCE, unreal.AnimSequence), require(IRON_MONTAGE, unreal.AnimMontage)
    if skill.get_editor_property('attack_montage') != montage or abs(sequence.get_play_length() - .7) > 1e-5:
        raise RuntimeError('Unexpected two-punch derivative contract')
    result.append({'name': IRON_NAME, 'style': 'two_punch', 'sequence': sequence, 'montage': montage,
                   'skill': skill, 'rate': float(skill.get_editor_property('animation_play_rate')),
                   'spacing': 0.0, 'contacts': [float(skill.get_editor_property('hit_start_delay_seconds'))],
                   'active_hands': ['hand_l', 'hand_r']})
    for row in result:
        tracks = montage_tracks(row['montage'])
        if len(tracks) != 1 or tracks[0]['slot'] != 'DefaultSlot' or len(tracks[0]['segments']) != 1:
            raise RuntimeError('Unexpected montage track: ' + row['name'])
        segment = tracks[0]['segments'][0]
        if segment['anim_reference'] != row['sequence'].get_path_name() or segment['anim_play_rate'] != 1.0:
            raise RuntimeError('Unexpected segment binding/rate: ' + row['name'])
    return result


def evaluation_options(mesh):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', mesh)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    return options


def transform(value):
    p, q, scale = value.translation, value.rotation, value.scale3d
    result = {'p': [p.x, p.y, p.z], 'q': [q.x, q.y, q.z, q.w],
              's': [scale.x, scale.y, scale.z]}
    if not all(math.isfinite(v) for values in result.values() for v in values):
        raise RuntimeError('Nonfinite bone transform')
    norm = sum(v * v for v in result['q'])
    if abs(norm - 1.0) > 1e-4:
        raise RuntimeError('Unnormalized bone quaternion')
    return result


def angle(a, b):
    norm = math.sqrt(sum(v * v for v in a) * sum(v * v for v in b))
    if norm < 1e-10:
        raise RuntimeError('Zero bone quaternion')
    return math.degrees(2 * math.acos(min(1.0, abs(sum(x * y for x, y in zip(a, b))) / norm)))


def bone(pose, name, space=unreal.AnimPoseSpaces.WORLD):
    return transform(unreal.AnimPoseExtensions.get_bone_pose(pose, name, space))


def sample(row, mesh):
    sequence = row['sequence']
    options = evaluation_options(mesh)
    length = sequence.get_play_length()
    frames_count = round(length * 120.0)
    if abs(frames_count / 120.0 - length) > 1e-5:
        raise RuntimeError('Non-120 Hz sequence duration: ' + row['name'])
    initial = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, 0.0, options)
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(initial)]
    if any(n not in names for n in WORLD_BONES):
        raise RuntimeError('Missing anatomical or IK bone: ' + row['name'])
    frames = []
    for index in range(frames_count + 1):
        time = min(index / 120.0, length)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
        frames.append({'time': time,
                       'world': {n: bone(pose, n) for n in WORLD_BONES},
                       'local': {n: bone(pose, n, unreal.AnimPoseSpaces.LOCAL) for n in names}})
    contacts = []
    for time in row['contacts']:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
        contacts.append({'time': time, 'world': {n: bone(pose, n) for n in ARM + ['head']}})
    hands = {}
    for hand in ['hand_l', 'hand_r']:
        points = [f['world'][hand]['p'] for f in frames]
        step = [math.dist(a, b) for a, b in zip(points, points[1:])]
        hands[hand] = {'path_travel_cm': sum(step), 'peak_speed_cm_s': max(step) * 120.0,
                       'max_excursion_cm': max(math.dist(p, points[0]) for p in points),
                       'axis_range_cm': [max(p[i] for p in points) - min(p[i] for p in points) for i in range(3)]}
    return {'name': row['name'], 'style': row['style'], 'sequence': sequence.get_path_name(),
            'metadata': asset_metadata(sequence, row['montage']), 'sample_hz': 120,
            'contacts': contacts, 'hands': hands, 'frames': frames}


def validate(row, current, prior):
    if current['metadata'] != prior['metadata']:
        raise RuntimeError('Sequence/montage timing or metadata changed: ' + row['name'])
    frames, before = current['frames'], prior['frames']
    if len(frames) != len(before):
        raise RuntimeError('Frame count changed: ' + row['name'])
    errors = {'local_position_cm': 0.0, 'local_scale': 0.0, 'limb_length_cm': 0.0,
              'stance_position_cm': 0.0, 'stance_rotation_deg': 0.0, 'finger_rotation_deg': 0.0,
              'wrist_local_rotation_deg': 0.0, 'ik_position_cm': 0.0, 'ik_rotation_deg': 0.0,
              'head_travel_cm': 0.0, 'head_rotation_deg': 0.0,
              'max_elbow_step_cm': 0.0, 'max_arm_rotation_step_deg': 0.0}
    first_prior = before[0]['world']
    for index, (frame, old) in enumerate(zip(frames, before)):
        if frame['time'] != old['time'] or frame['local'].keys() != old['local'].keys():
            raise RuntimeError('Sampling or skeleton changed: ' + row['name'])
        local, world = frame['local'], frame['world']
        for name, value in local.items():
            previous = old['local'][name]
            if not name.startswith('ik_'):
                errors['local_position_cm'] = max(errors['local_position_cm'], math.dist(value['p'], previous['p']))
                errors['local_scale'] = max(errors['local_scale'], math.dist(value['s'], previous['s']))
            if re.match(r'^(thumb|index|middle|ring|pinky)_', name):
                errors['finger_rotation_deg'] = max(errors['finger_rotation_deg'], angle(value['q'], previous['q']))
            if name in ['hand_l', 'hand_r']:
                errors['wrist_local_rotation_deg'] = max(errors['wrist_local_rotation_deg'], angle(value['q'], previous['q']))
        for parent, child in LIMBS:
            now_length = math.dist(world[parent]['p'], world[child]['p'])
            old_length = math.dist(old['world'][parent]['p'], old['world'][child]['p'])
            errors['limb_length_cm'] = max(errors['limb_length_cm'], abs(now_length - old_length))
        for name in STANCE:
            errors['stance_position_cm'] = max(errors['stance_position_cm'], math.dist(world[name]['p'], old['world'][name]['p']))
            errors['stance_rotation_deg'] = max(errors['stance_rotation_deg'], angle(world[name]['q'], old['world'][name]['q']))
        for ik_name, goal in IK:
            errors['ik_position_cm'] = max(errors['ik_position_cm'], math.dist(world[ik_name]['p'], world[goal]['p']))
            errors['ik_rotation_deg'] = max(errors['ik_rotation_deg'], angle(world[ik_name]['q'], world[goal]['q']))
        errors['head_travel_cm'] = max(errors['head_travel_cm'], math.dist(world['head']['p'], first_prior['head']['p']))
        errors['head_rotation_deg'] = max(errors['head_rotation_deg'], angle(world['head']['q'], first_prior['head']['q']))
        if index:
            last = frames[index - 1]['world']
            for name in ARM:
                errors['max_arm_rotation_step_deg'] = max(errors['max_arm_rotation_step_deg'], angle(world[name]['q'], last[name]['q']))
                if name.startswith('lowerarm'):
                    errors['max_elbow_step_cm'] = max(errors['max_elbow_step_cm'], math.dist(world[name]['p'], last[name]['p']))
    for name, value in errors.items():
        limit = 5.0 if name == 'head_travel_cm' else .01 if name == 'head_rotation_deg' else 8.0 if name == 'max_elbow_step_cm' else 30.0 if name == 'max_arm_rotation_step_deg' else .02
        if value > limit or not math.isfinite(value):
            raise RuntimeError('Unsafe bone motion: ' + row['name'] + ' ' + name + '=' + str(value))
    # The original validated contact positions and times remain exact. Preparation
    # and follow-through gain expression, without shifting the damage cue.
    contact_deltas = []
    for new, old in zip(current['contacts'], prior['contacts']):
        deltas = {name: math.dist(new['world'][name]['p'], old['world'][name]['p']) for name in ARM + ['head']}
        contact_deltas.append({'time': new['time'], 'bone_position_delta_cm': deltas})
        if new['time'] != old['time'] or max(deltas.values()) > .05:
            raise RuntimeError('Contact pose changed: ' + row['name'] + ' ' + str(contact_deltas))
    comparisons = {}
    all_increased = True
    for hand in row['active_hands']:
        new, old = current['hands'][hand], prior['hands'][hand]
        path_gain = new['path_travel_cm'] / max(old['path_travel_cm'], .001)
        speed_gain = new['peak_speed_cm_s'] / max(old['peak_speed_cm_s'], .001)
        difference = max(math.dist(a['world'][hand]['p'], b['world'][hand]['p']) for a, b in zip(frames, before))
        meaningful = difference >= 1.0 and (
            path_gain >= 1.03 and new['path_travel_cm'] - old['path_travel_cm'] >= 1.0 or
            speed_gain >= 1.03 and new['peak_speed_cm_s'] - old['peak_speed_cm_s'] >= 10.0)
        all_increased &= meaningful
        comparisons[hand] = {'before': old, 'after': new, 'path_travel_ratio': path_gain,
                             'peak_speed_ratio': speed_gain, 'max_trajectory_difference_cm': difference,
                             'meaningful_increase': meaningful}
    if not all_increased:
        raise RuntimeError('Arm swing did not increase measurably: ' + row['name'] + ' ' + str(comparisons))
    return {'name': row['name'], 'result': 'PASS', 'errors': errors,
            'contact_deltas': contact_deltas, 'arm_comparison': comparisons,
            'limits': '120 Hz clip audit; actual Appearance retarget and rendered view need runtime verification.'}


def rebuild_iron(row, flurry, mesh):
    target = row['sequence']
    if target.get_path_name().split('.')[0] != IRON_SEQUENCE:
        raise RuntimeError('Refusing a derivative outside IronChain')
    options = evaluation_options(mesh)
    def source_time(time):
        return time if time <= .273 else time + .372
    # Same finishing right-contact phase as the pre-task derivative. The source
    # has six strikes; this existing map retains the first two and final recovery.
    first = unreal.AnimPoseExtensions.get_anim_pose_at_time(flurry, 0.0, options)
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(first)]
    expected_parents = {'ik_hand_gun': 'ik_hand_root', 'ik_hand_l': 'ik_hand_gun',
                        'ik_hand_r': 'ik_hand_gun', 'ik_foot_l': 'ik_foot_root',
                        'ik_foot_r': 'ik_foot_root'}
    parents = {}
    for anchor, _ in IK:
        # UE 5.8 reflected read-only helper returns [bone, parent, ..., root].
        path = [str(n) for n in unreal.AnimationLibrary.find_bone_path_to_root(flurry, anchor)]
        if len(path) < 2 or path[0] != anchor or path[1] != expected_parents[anchor]:
            raise RuntimeError('Unexpected Manny IK hierarchy: ' + anchor + ' ' + str(path))
        parents[anchor] = path[1]
    join = [unreal.AnimPoseExtensions.get_anim_pose_at_time(flurry, time, options) for time in [.273, .645]]
    seam = {'local_position_cm': 0.0, 'local_rotation_deg': 0.0}
    for name in names:
        a, b = [bone(p, name, unreal.AnimPoseSpaces.LOCAL) for p in join]
        seam['local_position_cm'] = max(seam['local_position_cm'], math.dist(a['p'], b['p']))
        seam['local_rotation_deg'] = max(seam['local_rotation_deg'], angle(a['q'], b['q']))
    if seam['local_position_cm'] > .1 or seam['local_rotation_deg'] > .05:
        raise RuntimeError('Two-punch source join changed: ' + str(seam))
    keys = {name: {'p': [], 'q': [], 's': []} for name in names}
    frame_count = round(target.get_play_length() * 120.0)
    for index in range(frame_count + 1):
        time = source_time(index / 120.0)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(flurry, time, options)
        # Sampling the remapped source between its keys interpolates FK rotations
        # and IK translation independently. Rebuild the anchors from this frame's
        # actual FK after remapping, with parents resolved before their children.
        # Render-bone local translations, scales and closed-fist wrists stay exact.
        final_world = {anchor: unreal.AnimPoseExtensions.get_bone_pose(
            pose, goal, unreal.AnimPoseSpaces.WORLD) for anchor, goal in IK}
        synchronized_ik = {}
        for anchor, _ in IK:
            parent = parents[anchor]
            parent_world = final_world[parent] if parent in final_world else unreal.AnimPoseExtensions.get_bone_pose(
                pose, parent, unreal.AnimPoseSpaces.WORLD)
            synchronized_ik[anchor] = unreal.MathLibrary.make_relative_transform(final_world[anchor], parent_world)
        for name in names:
            value = synchronized_ik[name] if name in synchronized_ik else unreal.AnimPoseExtensions.get_bone_pose(
                pose, name, unreal.AnimPoseSpaces.LOCAL)
            keys[name]['p'].append(unreal.Vector(0.0, 0.0, 0.0) if name == 'root' else value.translation)
            keys[name]['q'].append(value.rotation)
            keys[name]['s'].append(value.scale3d)
    controller = target.get_editor_property('controller')
    controller.open_bracket('Expressive fighter two-punch derivative', False)
    try:
        controller.remove_all_bone_tracks(False)
        controller.set_frame_rate(unreal.FrameRate(120, 1), False)
        controller.set_number_of_frames(unreal.FrameNumber(frame_count), False)
        for name, values in keys.items():
            if not controller.add_bone_curve(name, False) or not controller.set_bone_track_keys(name, values['p'], values['q'], values['s'], False):
                raise RuntimeError('Cannot write derived bone keys: ' + name)
    finally:
        controller.close_bracket(False)
    seam['synchronized_ik_parents'] = parents
    seam['ik_method'] = 'Final sampled FK world transform relative to final IK parent, after existing source-time map.'
    return seam


def capture(mesh):
    return {'contract': contract(), 'protected_package_sha256': source_hashes(),
            'clips': {row['name']: sample(row, mesh) for row in rows()}}


def check_contract(baseline):
    if contract() != baseline['contract']:
        raise RuntimeError('Attack Data Asset or profile contract changed')
    if source_hashes() != baseline['protected_package_sha256']:
        raise RuntimeError('A protected original/Data Asset package changed')


def audit(baseline, mesh, phase):
    check_contract(baseline)
    checks, actual = [], {}
    for row in rows():
        current = sample(row, mesh)
        actual[row['name']] = current
        checks.append(validate(row, current, baseline['clips'][row['name']]))
    if phase == 'reload':
        api = runpy.run_path(str(ROOT / 'Scripts/Editor/PrepareFistCombatAnimations.py'),
                             run_name='fighter_swing_reload_readonly_helpers')
        options = evaluation_options(mesh)
        guard = unreal.AnimPoseExtensions.get_anim_pose_at_time(require(SOURCE), 0.0, options)
        # Compression and a fresh load can affect boundary interpolation. Require
        # the same activation, contact direction and anatomical gates after save.
        write('reload-authored-contact-validation.json',
              [api['validate'](row, options, guard) for row in rows()[:-1]])
    write(phase + '-poses.json', actual)
    result = {'result': 'PASS', 'phase': phase, 'checks': checks,
              'data_asset_profile_contract_unchanged': True, 'protected_packages_unchanged': True,
              'changed_scope': 'Ten existing animation sequences; no montage or Data Asset metadata changed.',
              'limitations': 'Structural and clip-motion verification. Rendered Appearance retarget and live damage need runtime QA.'}
    write(phase + '-validation.json', result)
    return result


def apply(baseline, mesh):
    if (EVIDENCE / 'apply-validation.json').exists():
        raise RuntimeError('Apply already completed; use the read-only audit phase')
    check_contract(baseline)
    before = capture(mesh)
    if before != baseline:
        raise RuntimeError('Animation baseline changed; capture/review actual state before applying')
    targets = rows()
    lines = []
    for row in targets[:-1]:
        lines.append('|'.join(str(v) for v in [row['name'], row['style'], row['sequence'].get_play_length(),
                         row['contacts'][0], row['spacing'], len(row['contacts'])]))
    (EVIDENCE / 'authoring-requests.txt').write_text('\n'.join(lines), encoding='utf-8')
    result_path = EVIDENCE / 'native-authoring.txt'
    result_path.write_text('PENDING', encoding='utf-8')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'Rogue10m.AuthorFistSwing')
    native = result_path.read_text(encoding='utf-8-sig')
    if 'SWING_PROFILE=EXPRESSIVE' not in native or 'RESULT=FIST_COMBAT_AUTHORING_PASSED' not in native:
        raise RuntimeError('Native expressive bake failed: ' + native)
    guards = re.findall(r'continuity_elbow_cm=([0-9.]+) continuity_rotation_deg=([0-9.]+)', native)
    if len(guards) != 9 or any(float(a) >= .01 or float(b) >= .01 for a, b in guards):
        raise RuntimeError('Native guard continuity checks missing/failed')
    seam = rebuild_iron(targets[-1], targets[6]['sequence'], mesh)
    write('two-punch-source-join.json', seam)
    # Reuse only the original read-only validator: runpy's non-main run_name skips
    # main(), its save/DA binding loop, old report paths and quit_editor().
    api = runpy.run_path(str(ROOT / 'Scripts/Editor/PrepareFistCombatAnimations.py'),
                         run_name='fighter_swing_readonly_validation_helpers')
    options = evaluation_options(mesh)
    guard = unreal.AnimPoseExtensions.get_anim_pose_at_time(require(SOURCE), 0.0, options)
    native_checks = [api['validate'](row, options, guard) for row in targets[:-1]]
    write('authored-contact-validation.json', native_checks)
    audit(baseline, mesh, 'in-memory')
    # There is no save before every contact, timing, anatomy and comparison gate
    # passes. In-memory failure exits without committing any package changes.
    for row in targets:
        sequence = row['sequence']
        if sequence.get_path_name() not in [r['sequence'].get_path_name() for r in targets]:
            raise RuntimeError('Unexpected save target')
        if not unreal.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False):
            raise RuntimeError('Sequence save failed: ' + sequence.get_path_name())
    check_contract(baseline)
    write('apply-validation.json', {'result': 'PASS', 'saved_sequences': [r['sequence'].get_path_name() for r in targets],
                                  'all_validation_completed_before_save': True, 'native_guard_checks': guards,
                                  'next_required_phase': 'Fresh-editor -RogueFistSwingAudit'})
    unreal.log('RESULT=FIGHTER_ARM_SWING_APPLY_PASSED sequences=10')


def main():
    if ROOT != EXPECTED_ROOT:
        raise RuntimeError('Unexpected project checkout')
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    mesh = require(MANNY, unreal.SkeletalMesh)
    switches = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())[1]
    applying, auditing = 'RogueFistSwingApply' in switches, 'RogueFistSwingAudit' in switches
    if applying and auditing:
        raise RuntimeError('Choose exactly one phase')
    if applying:
        apply(read('baseline.json'), mesh)
    elif auditing:
        if read('apply-validation.json')['result'] != 'PASS':
            raise RuntimeError('Successful apply report required before saved-asset audit')
        audit(read('baseline.json'), mesh, 'reload')
        unreal.log('RESULT=FIGHTER_ARM_SWING_RELOAD_PASSED sequences=10')
    else:
        if (EVIDENCE / 'baseline.json').exists():
            raise RuntimeError('Baseline already exists; preserve it and use an explicit phase')
        baseline = capture(mesh)
        write('baseline.json', baseline)
        write('baseline-summary.json', {'result': 'CAPTURED', 'sample_hz': 120,
              'data_asset_count': len(baseline['contract']['skills']),
              'profile_count': len(baseline['contract']['profiles']),
              'clips': {name: {'metadata': row['metadata'], 'hands': row['hands']}
                        for name, row in baseline['clips'].items()}})
        unreal.log('RESULT=FIGHTER_ARM_SWING_BASELINE_CAPTURED sequences=10')


if __name__ == '__main__':
    try:
        main()
    finally:
        unreal.SystemLibrary.quit_editor()
