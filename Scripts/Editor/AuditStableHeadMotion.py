"""Read-only audit of current player bindings, evaluated montage ranges and locomotion graph.

Run through the project's UE editor Python entry point. No assets are modified,
compiled or saved. The caller decides whether to close the editor afterwards.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
import re
import runpy

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EVIDENCE = ROOT / 'Feature/doc/evidence/mouse-head-stable-locomotion-20261013'
BLUEPRINT = '/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed'
MANNY = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'


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


def skill_fields():
    text = (ROOT / 'Source/Rogue10m/Data/Rogue10mAttackSkillData.h').read_text(encoding='utf-8')
    text = text[text.index('class ROGUE10M_API URogue10mAttackSkillData'):]
    names = re.findall(r'^\s*(?:\w+[\w:<>]*)\s+(\w+)\s*(?:=|;)', text, re.M)
    def snake(name):
        if name.startswith('b') and len(name) > 1 and name[1].isupper():
            name = name[1:]
        return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()
    return [snake(name) for name in names]


def vec(value):
    return [float(getattr(value, c)) for c in 'xyz']


def quat(value):
    return [float(getattr(value, c)) for c in 'xyzw']


def angle(a, b):
    norm = math.sqrt(sum(v*v for v in a) * sum(v*v for v in b))
    if norm < 1e-9:
        raise RuntimeError('Invalid pose quaternion')
    return math.degrees(2*math.acos(min(1., abs(sum(x*y for x, y in zip(a, b))) / norm)))


def forward_yaw(q):
    # Manny model-space forward is +Y; source component yaw -90 maps it to actor +X.
    x, y, z, w = q
    forward = [2*(x*y-z*w), 1-2*(x*x+z*z), 2*(y*z+x*w)]
    return math.degrees(math.atan2(-forward[0], forward[1]))


def sample_range(sequence, start, end, options):
    start, end = float(start), float(end)
    if not isinstance(sequence, unreal.AnimSequence) or end < start or end-start > 20:
        raise RuntimeError('Unexpected montage sequence range')
    samples = max(2, min(2401, int(math.ceil((end-start)*120))+1))
    output = {'sequence': sequence.get_path_name(), 'start': start, 'end': end,
              'duration': sequence.get_play_length(), 'samples': samples,
              'root_motion': bool(sequence.get_editor_property('enable_root_motion')),
              'bones': {name: {'max_rotation_from_start_deg': 0., 'max_position_from_start_cm': 0.,
                               'max_rotation_step_deg': 0., 'yaw_min_deg': 0., 'yaw_max_deg': 0.}
                        for name in ['root', 'pelvis', 'head']}}
    initial, previous, yaw, last_raw = {}, {}, {}, {}
    for i in range(samples):
        time = start + (end-start)*i/(samples-1)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
        names = {str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)}
        for name, measurements in output['bones'].items():
            if name not in names:
                raise RuntimeError('Required Manny bone missing: ' + name)
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
            p, q = vec(transform.translation), quat(transform.rotation)
            if not all(math.isfinite(v) for v in p+q) or abs(sum(v*v for v in q)-1) > .001:
                raise RuntimeError('Invalid evaluated pose: ' + sequence.get_path_name() + ':' + name)
            raw_yaw = forward_yaw(q)
            if i == 0:
                initial[name] = (p, q)
                yaw[name] = 0.
                last_raw[name] = raw_yaw
            else:
                step = (raw_yaw-last_raw[name]+180.) % 360. - 180.
                yaw[name] += step
                last_raw[name] = raw_yaw
                measurements['max_rotation_step_deg'] = max(measurements['max_rotation_step_deg'], angle(previous[name][1], q))
            measurements['max_rotation_from_start_deg'] = max(measurements['max_rotation_from_start_deg'], angle(initial[name][1], q))
            measurements['max_position_from_start_cm'] = max(measurements['max_position_from_start_cm'], math.dist(initial[name][0], p))
            measurements['yaw_min_deg'] = min(measurements['yaw_min_deg'], yaw[name])
            measurements['yaw_max_deg'] = max(measurements['yaw_max_deg'], yaw[name])
            previous[name] = (p, q)
    # This is a measured candidate flag, not a name-based action classifier.
    output['turn_candidate'] = any(max(abs(row['yaw_min_deg']), abs(row['yaw_max_deg'])) > 90.
                                   or row['max_rotation_from_start_deg'] > 90.
                                   for name, row in output['bones'].items() if name in ['pelvis', 'head'])
    return output


def main():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('Stop PIE before read-only asset inspection')
    mesh = unreal.load_asset(MANNY)
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError('Missing animation source mesh')
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', mesh)
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('extract_root_motion', False)
    profile_paths = unreal.EditorAssetLibrary.list_assets('/Game/DataAsset/SkillProfile', recursive=True, include_folder=False)
    profiles = []
    for path in sorted(profile_paths):
        profile = unreal.load_asset(path)
        if not profile or profile.get_class().get_name() != 'Rogue10mWeaponSkillProfileDataAsset':
            continue
        keys = ['skill_tree_skills', 'default_skill_bindings', 'initially_unlocked_skills']
        profiles.append({'path': profile.get_path_name(), 'fields': {key: serial(profile.get_editor_property(key)) for key in keys}})
    skills, montages, ranges = [], {}, {}
    keys = skill_fields()
    motion = unreal.load_asset('/Game/DataAsset/Character/Animation/DA_CommonCharacterMotion')
    dodge_montage = motion.get_editor_property('dodge_montage') if motion else None
    attack_paths = sorted(unreal.EditorAssetLibrary.list_assets('/Game/DataAsset/AttackSkill', recursive=True, include_folder=False))
    assets = [(path, unreal.load_asset(path)) for path in attack_paths]
    assets.append(('Dodge', None))
    for path, skill in assets:
        if path != 'Dodge' and not isinstance(skill, unreal.Rogue10mAttackSkillData):
            continue
        if skill:
            skills.append({'path': skill.get_path_name(), 'fields': {key: serial(skill.get_editor_property(key)) for key in keys}})
        montage = skill.get_editor_property('attack_montage') if skill else dodge_montage
        if not montage or montage.get_path_name() in montages:
            continue
        tracks = []
        for slot in montage.get_editor_property('slot_anim_tracks'):
            segments = []
            for segment in slot.get_editor_property('anim_track').get_editor_property('anim_segments'):
                sequence = segment.get_editor_property('anim_reference')
                row = {key: serial(segment.get_editor_property(key)) for key in
                       ['anim_reference', 'start_pos', 'anim_start_time', 'anim_end_time', 'anim_play_rate', 'looping_count']}
                segments.append(row)
                range_key = (sequence.get_path_name(), row['anim_start_time'], row['anim_end_time']) if sequence else None
                if range_key and range_key not in ranges:
                    ranges[range_key] = sample_range(sequence, row['anim_start_time'], row['anim_end_time'], options)
            tracks.append({'slot': str(slot.get_editor_property('slot_name')), 'segments': segments})
        montages[montage.get_path_name()] = {'path': montage.get_path_name(), 'duration': montage.get_play_length(),
                                          'rate_scale': montage.get_editor_property('rate_scale'), 'tracks': tracks}
    for path in sorted(unreal.EditorAssetLibrary.list_assets('/Game/Rogue10m/Animation/Preview/CloseCombat', recursive=True, include_folder=False)):
        sequence = unreal.load_asset(path)
        if isinstance(sequence, unreal.AnimSequence) and 'Evade' in sequence.get_name():
            key = (sequence.get_path_name(), 0., sequence.get_play_length())
            ranges[key] = sample_range(sequence, 0., sequence.get_play_length(), options)
    helpers = runpy.run_path(str(ROOT / 'Scripts/Editor/PrepareFighterCombatStance.py'), run_name='stable_head_readonly_helpers')
    bp = unreal.load_asset(BLUEPRINT)
    graphs = {g.get_path_name(): helpers['graph_dump'](unreal.BlueprintGraphEditor.get_graph_editor(g))
              for g in unreal.BlueprintEditorLibrary.list_graphs(bp)}
    players = [n for g in unreal.BlueprintEditorLibrary.list_graphs(bp)
               for n in unreal.BlueprintGraphEditor.get_graph_editor(g).list_all_nodes()
               if n.get_class().get_name() == 'AnimGraphNode_BlendSpacePlayer']
    blends = []
    for player in players:
        blend = player.get_editor_property('node').get_editor_property('blend_space')
        blends.append({'node': player.get_path_name(), 'blend_space': serial(blend),
                       'axis_to_scale_animation': serial(blend.get_editor_property('axis_to_scale_animation')),
                       'parameters': [value.export_text() for value in blend.get_editor_property('blend_parameters')],
                       'samples': [value.export_text() for value in blend.get_editor_property('sample_data')],
                       'optional_pins': [value.export_text() for value in player.get_editor_property('show_pin_for_properties')]})
    report = {'read_only': True, 'profiles': profiles, 'skills': skills, 'montages': list(montages.values()),
              'dodge_motion_data': serial(motion), 'dodge_montage': serial(dodge_montage),
              'evaluated_ranges': list(ranges.values()), 'graphs': graphs, 'locomotion_players': blends,
              'limitations': ['Turn flags come from evaluated component-space yaw. Manual action/presentation review is still required.',
                              'This audit does not claim rendered quality, target contact or gameplay use of every profile.']}
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / 'asset-motion-audit.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('RESULT=STABLE_HEAD_MOTION_AUDIT_PASSED skills=%d montages=%d ranges=%d' % (len(skills), len(montages), len(ranges)))
    return report


if __name__ == '__main__':
    main()
