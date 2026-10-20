"""Read-only source-pose and attack-direction audit; run only in this UE Editor.

Inventories all attack/profile packages, then samples the current Unarmed/Knuckle
profile union (including combo references) at 120 Hz on the actual Manny mesh.
Reports stay in camera-forward-attacks-20261009. No authoring helper main, save,
compile, binding, timing change, gameplay input or editor shutdown is performed.
Source poses do not certify the live Appearance retarget or camera/trace aim.
"""
import hashlib
import json
import math
from pathlib import Path
import runpy
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EXPECTED = Path(r'D:\Project\Rogue10m').resolve()
EVIDENCE = ROOT / 'Feature/doc/evidence/camera-forward-attacks-20261009'
PROFILE_SCOPE = {
    '/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed',
    '/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle',
}
HZ = 120
STEP = 1.0 / HZ
POSE_BONES = ['root', 'pelvis', 'head', 'upperarm_l', 'lowerarm_l', 'hand_l',
              'upperarm_r', 'lowerarm_r', 'hand_r']
OPTIONAL_BONES = ['middle_01_l', 'middle_01_r']
ASSET_FOLDERS = ['Content/DataAsset/AttackSkill', 'Content/DataAsset/SkillProfile',
                 'Content/Rogue10m/Animation/Combat', 'Content/Rogue10m/Animation/Common',
                 'Content/Characters/Mannequins/Anims/Unarmed']


def write(name, value):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


def hashes():
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for folder in ASSET_FOLDERS for p in (ROOT / folder).rglob('*.uasset')}


def package_path(path):
    return path.split('.')[0]


def content_assets(folder):
    return ['/Game/' + str(p.relative_to(ROOT / 'Content').with_suffix('')).replace('\\', '/')
            for p in sorted((ROOT / folder).rglob('*.uasset'))]


def inventory(api):
    skill_fields = api['property_fields']('Source/Rogue10m/Data/Rogue10mAttackSkillData.h',
                                         'URogue10mAttackSkillData')
    profile_fields = api['property_fields']('Source/Rogue10m/Data/Rogue10mSkillLoadoutDataAsset.h',
                                           'URogue10mWeaponSkillProfileDataAsset')
    profiles, skills, objects, references = {}, {}, {}, {}
    selected, reached, missing = set(), set(), []
    for path in content_assets('Content/DataAsset/AttackSkill'):
        asset = api['require'](path)
        if asset.get_class().get_name() != 'Rogue10mAttackSkillData':
            raise RuntimeError('Unexpected AttackSkill package class: ' + path)
        skills[path] = api['object_fields'](asset, skill_fields)
        objects[path] = asset
    for path in content_assets('Content/DataAsset/SkillProfile'):
        profile = api['require'](path)
        if profile.get_class().get_name() != 'Rogue10mWeaponSkillProfileDataAsset':
            raise RuntimeError('Unexpected skill profile package class: ' + path)
        profiles[path] = api['object_fields'](profile, profile_fields)
        candidates = list(profile.get_editor_property('skill_tree_skills'))
        candidates += list(profile.get_editor_property('initially_unlocked_skills'))
        candidates += list(profile.get_editor_property('default_skill_bindings').values())
        queue = [item for item in candidates if item]
        visited = set()
        while queue:
            item = queue.pop()
            key = package_path(item.get_path_name())
            if key in visited:
                continue
            visited.add(key)
            reached.add(key)
            if path in PROFILE_SCOPE:
                selected.add(key)
            references.setdefault(key, []).append(path)
            if key not in skills:
                missing.append({'profile': path, 'skill': key})
                continue
            next_combo = item.get_editor_property('next_combo_skill')
            if next_combo:
                queue.append(next_combo)
    if not PROFILE_SCOPE.issubset(profiles):
        raise RuntimeError('Expected current player profiles missing')
    if missing:
        raise RuntimeError('Profile references outside inventoried AttackSkill packages: ' + str(missing))
    return {'profiles': profiles, 'skills': skills, 'references': references,
            'all_reachable_skills': sorted(reached), 'selected_skills': sorted(selected),
            'unreferenced_skills': sorted(set(skills) - reached),
            'missing_profile_references': missing}, objects


def timing_metadata(skill, api):
    rate = float(skill.get_editor_property('animation_play_rate'))
    first = float(skill.get_editor_property('hit_start_delay_seconds'))
    interval = float(skill.get_editor_property('hit_interval'))
    count = int(skill.get_editor_property('hit_count'))
    single = skill.get_editor_property('hit_mode') == unreal.Rogue10mAttackHitMode.SINGLE
    montage = skill.get_editor_property('attack_montage')
    passive = skill.get_editor_property('skill_tree_node_type') == unreal.Rogue10mSkillTreeNodeType.PASSIVE_TECHNIQUE
    result = {'passive': passive, 'montage': api['serial'](montage), 'animation_play_rate': rate,
              'first_delay_montage_seconds': first, 'interval_real_seconds_at_attack_speed1': interval,
              'hit_count': 1 if single else max(1, min(64, count)),
              'errors': []}
    if not all(math.isfinite(v) for v in [rate, first, interval]) or rate <= 0 or first < 0 or interval < 0:
        result['errors'].append('invalid_timing_values')
    if passive:
        result['classification'] = 'passive_no_attack_pose_required'
        return result
    if montage is None:
        result['errors'].append('active_skill_missing_montage')
        return result
    result.update({'montage_duration': montage.get_play_length(),
                   'montage_rate_scale': float(montage.get_editor_property('rate_scale')),
                   'skeleton': api['serial'](montage.get_editor_property('skeleton')),
                   'tracks': api['montage_tracks'](montage)})
    # Combat captures the first delay/rate, while subsequent intervals scale by
    # attack speed only. Include the montage RateScale when mapping real pulses.
    montage_scale = result['montage_rate_scale']
    result['contacts_montage_seconds'] = [
        (first + index * max(.01, interval) * rate) * montage_scale
        for index in range(result['hit_count'])]
    if not math.isfinite(montage_scale) or montage_scale <= 0:
        result['errors'].append('unsupported_nonpositive_montage_rate_scale')
    if any(t < 0 or t > result['montage_duration'] + 1e-5 for t in result['contacts_montage_seconds']):
        result['errors'].append('damage_pulse_outside_montage_duration')
    return result


def mapped_sequence(metadata, montage_time, api):
    tracks = metadata['tracks']
    if len(tracks) != 1:
        return None, None, 'multiple_or_missing_slot_tracks'
    candidates = []
    for segment in tracks[0]['segments']:
        sequence = api['require'](segment['anim_reference'])
        if not isinstance(sequence, unreal.AnimSequence):
            return None, None, 'non_sequence_montage_segment'
        play_rate = float(segment['anim_play_rate']) * float(sequence.get_editor_property('rate_scale'))
        if abs(play_rate) < 1e-8:
            play_rate = 1.0  # FAnimSegment::GetValidPlayRate.
        span = float(segment['anim_end_time']) - float(segment['anim_start_time'])
        loops = int(segment['looping_count'])
        start = float(segment['start_pos'])
        end = start + loops * span / abs(play_rate) if span > 0 else start
        if span <= 0 or loops < 1:
            return None, None, 'invalid_sequence_segment_range'
        if start - 1e-5 <= montage_time <= end + 1e-5:
            unwrapped = (montage_time - start) * play_rate
            loop = min(math.floor(abs(unwrapped) / span), max(loops - 1, 0))
            point = float(segment['anim_start_time']) if play_rate >= 0 else float(segment['anim_end_time'])
            time = point + unwrapped - loop * span
            candidates.append((sequence, min(max(0.0, time), sequence.get_play_length())))
    if len(candidates) != 1:
        return None, None, 'ambiguous_or_missing_segment_at_damage_pulse'
    return candidates[0][0], candidates[0][1], None


def sequence_sampler(mesh, api):
    options = api['evaluation_options'](mesh)
    expected_skeleton = mesh.get_editor_property('skeleton')
    cache = {}
    def sample(sequence):
        path = sequence.get_path_name()
        if path in cache:
            return cache[path]
        if sequence.get_editor_property('skeleton') != expected_skeleton:
            raise RuntimeError('Selected source sequence has a different Manny skeleton: ' + path)
        length = sequence.get_play_length()
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, 0.0, options)
        names = {str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)}
        if any(n not in names for n in POSE_BONES):
            raise RuntimeError('Selected source sequence lacks required render bones: ' + path)
        bones = POSE_BONES + [n for n in OPTIONAL_BONES if n in names]
        frames = []
        for index in range(math.ceil(length * HZ) + 1):
            time = min(index * STEP, length)
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
            frames.append({'time': time, 'world': {n: api['bone'](pose, n)['p'] for n in bones}})
        cache[path] = {'sequence': path, 'duration': length, 'sample_hz': HZ, 'bones': bones, 'frames': frames}
        unreal.log('CAMERA_FORWARD_AUDIT sampled=' + path + ' frames=' + str(len(frames)))
        return cache[path]
    def exact(sequence, time):
        sample(sequence)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
        return {n: api['bone'](pose, n)['p'] for n in cache[sequence.get_path_name()]['bones']}
    return sample, exact, cache


def delta(a, b):
    return [x - y for x, y in zip(a, b)]


def heading(vector):
    return math.degrees(math.atan2(vector[0], vector[1]))


def hand_metrics(world, earlier, hand):
    side = hand[-1]
    reach = delta(world[hand], world['upperarm_' + side])
    forearm = delta(world[hand], world['lowerarm_' + side])
    approach = delta(world[hand], earlier[hand])
    value = {'hand': hand, 'shoulder_to_wrist_cm': reach,
             'forward_reach_cm': reach[1], 'shoulder_side_yaw_degrees': heading(reach),
             'forearm_side_yaw_degrees': heading(forearm),
             'contact_forward_of_head_cm': world[hand][1] - world['head'][1],
             'contact_height_from_head_cm': world[hand][2] - world['head'][2],
             'approach_window_displacement_cm': approach,
             'approach_forward_cm': approach[1], 'approach_rise_cm': approach[2]}
    middle = 'middle_01_' + side
    if middle in world:
        axis = delta(world[middle], world[hand])
        norm = math.sqrt(sum(v * v for v in axis))
        value['wrist_to_middle_mcp_axis'] = axis
        value['wrist_long_axis_forward_cosine'] = axis[1] / norm if norm > 1e-8 else None
        value['wrist_long_axis_side_yaw_degrees'] = heading(axis)
    return value


def inspect_skill(path, skill, metadata, known, sampler, exact, api):
    if metadata['passive']:
        return {'skill': path, 'status': 'PASSIVE', 'contacts': []}
    if metadata['errors']:
        return {'skill': path, 'status': 'TIMING_REVIEW_REQUIRED', 'errors': metadata['errors'], 'contacts': []}
    row = known.get(path, {})
    style = row.get('style', 'legacy_unclassified')
    shape = skill.get_editor_property('attack_shape')
    slot = skill.get_editor_property('input_slot')
    jump = slot in [unreal.Rogue10mAttackInputSlot.JUMP_PRIMARY, unreal.Rogue10mAttackInputSlot.JUMP_SPECIAL]
    area = shape == unreal.Rogue10mAttackShape.CIRCLE
    strict = path in known and not jump and not area
    selected_hands = row.get('active_hands', [row['bone']] if 'bone' in row else ['hand_l', 'hand_r'])
    contacts, issues = [], []
    for index, time in enumerate(metadata['contacts_montage_seconds']):
        sequence, sequence_time, error = mapped_sequence(metadata, time, api)
        if error:
            issues.append(error)
            continue
        frames = sampler(sequence)
        world = exact(sequence, sequence_time)
        earlier = exact(sequence, max(0.0, sequence_time - 5 * STEP))
        hands = [selected_hands[index % 2]] if style == 'flurry' else selected_hands
        if style == 'two_punch':
            hands = ['hand_r']  # The finishing right punch owns the existing single damage pulse.
        metrics = [hand_metrics(world, earlier, hand) for hand in hands]
        for value in metrics:
            limit = 65.0 if style in ['hook', 'flurry', 'two_punch', 'left_counter'] else 45.0
            if strict and (value['forward_reach_cm'] < 25.0 or abs(value['shoulder_side_yaw_degrees']) > limit):
                issues.append('known_fist_contact_outside_forward_style_gate:' + value['hand'])
        best = max(metrics, key=lambda v: v['forward_reach_cm'])
        if not area and best['forward_reach_cm'] <= 0:
            issues.append('all_measured_hands_behind_their_shoulders_at_damage_pulse')
        peaks = {}
        for hand in hands:
            peak = max(frames['frames'], key=lambda f: f['world'][hand][1] - f['world']['upperarm_' + hand[-1]][1])
            peaks[hand] = {'time': peak['time'], 'forward_reach_cm':
                          peak['world'][hand][1] - peak['world']['upperarm_' + hand[-1]][1]}
        contacts.append({'pulse': index + 1, 'montage_seconds': time,
                         'sequence': sequence.get_path_name(), 'sequence_seconds': sequence_time,
                         'approach_window_seconds': min(5 * STEP, sequence_time),
                         'hands': metrics, 'sequence_peak_reach': peaks,
                         'all_hands': [hand_metrics(world, earlier, h) for h in ['hand_l', 'hand_r']]})
    classification = 'area_or_jump_intent_requires_runtime_geometry_review' if area or jump else style
    return {'skill': path, 'status': 'DIRECTION_REVIEW_REQUIRED' if issues else 'SOURCE_POSES_MEASURED',
            'style': style, 'classification': classification, 'strict_known_fist_gate': strict,
            'attack_shape': str(shape), 'input_slot': str(slot), 'issues': sorted(set(issues)),
            'contacts': contacts}


def main():
    if ROOT != EXPECTED:
        raise RuntimeError('Unexpected project checkout')
    before = hashes()
    api = runpy.run_path(str(ROOT / 'Scripts/Editor/EnhanceFighterArmSwing.py'),
                        run_name='camera_forward_readonly_helpers')
    # Only read helpers are invoked. Never call capture/audit/apply/main from the old module.
    inventory_before, objects = inventory(api)
    metadata = {path: timing_metadata(asset, api) for path, asset in objects.items()}
    known = {package_path(row['skill'].get_path_name()): row for row in api['rows']()}
    binding = runpy.run_path(str(ROOT / 'Scripts/Editor/BindFighterSkillAnimations.py'),
                            run_name='camera_forward_binding_readonly_constants')
    for spec in binding['SPECS']:
        if spec['skill'] not in known:
            known[spec['skill']] = {'style': 'left_counter' if spec['bone'] == 'hand_l' else 'power_punch',
                                   'bone': spec['bone']}
    mesh = api['require'](api['MANNY'], unreal.SkeletalMesh)
    sample, exact, poses = sequence_sampler(mesh, api)
    checks = [inspect_skill(path, objects[path], metadata[path], known, sample, exact, api)
              for path in inventory_before['selected_skills']]
    inventory_after, after_objects = inventory(api)
    metadata_after = {path: timing_metadata(asset, api) for path, asset in after_objects.items()}
    if inventory_after != inventory_before or metadata_after != metadata or hashes() != before:
        raise RuntimeError('Read-only audit changed a package, Data Asset, profile or montage timing contract')
    camera_class = unreal.load_class(None, '/Script/Rogue10m.Rogue10mAppearanceCameraComponent')
    camera_defaults = unreal.get_default_object(camera_class) if camera_class else None
    camera = {'loaded_native_cdo_eye_offset': api['serial'](camera_defaults.get_editor_property('eye_offset'))
              if camera_defaults else None,
              'live_actor_eye_offset_and_component_transform': 'requires separate root runtime capture'}
    issues = [check for check in checks if check.get('issues') or check.get('errors')]
    report = {'result': 'PASSED', 'audit_completed': True, 'read_only': True,
              'direction_result': 'REVIEW_REQUIRED' if issues else 'SOURCE_MEASUREMENTS_COMPLETED',
              'mesh': mesh.get_path_name(), 'source_forward_axis': '+Y', 'sample_hz': HZ,
              'profile_scope': sorted(PROFILE_SCOPE),
              'selected_skill_count': len(checks),
              'selected_active_skill_count': sum(not metadata[p]['passive'] for p in inventory_before['selected_skills']),
              'disk_attack_skill_count': len(objects), 'disk_profile_count': len(inventory_before['profiles']),
              'unique_sampled_sequences': len(poses), 'direction_findings_count': len(issues),
              'protected_package_count': len(before), 'protected_packages_unchanged': True,
              'data_profile_timing_contract_unchanged': True, 'camera': camera, 'checks': checks,
              'limitations': [
                  'Source-pose WORLD is skeletal component space; +Y must be transformed by the actual mesh component.',
                  'Actual hit queries use head camera forward/rotation, including control pitch and stabilized head motion.',
                  'Other weapon profiles receive reference/timing inventory, not source-direction certification.',
                  'Hook/uppercut/area/jump intent cannot use a universal straight-punch velocity gate.',
                  'Wrist-to-middle-MCP is an anatomical long-axis estimate, not a measured knuckle contact-surface normal.',
                  'Montage blends, moving upper-body graph, retargeted skin, aim pitch, target overlaps and visual captures require runtime QA.',
                  'Single damage pulses and locked MultiHit targets are unchanged; a second visual fist is not another damage pulse.',
              ]}
    write('inventory.json', {'inventory': inventory_before, 'timing_and_references': metadata})
    write('source-poses.json', poses)
    write('protected-packages.json', before)
    write('audit.json', report)
    unreal.log('RESULT=CAMERA_FORWARD_ATTACK_AUDIT_PASSED active_skills=' + str(report['selected_active_skill_count'])
               + ' sequences=' + str(len(poses)) + ' direction_findings=' + str(len(issues)))


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        write('audit.json', {'result': 'FAILED', 'read_only': True, 'error': str(exc)})
        unreal.log_error('RESULT=CAMERA_FORWARD_ATTACK_AUDIT_FAILED ' + str(exc))
        raise
