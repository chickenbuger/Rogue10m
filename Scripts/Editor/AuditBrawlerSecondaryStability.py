"""Read-only RMB animation audit. No asset mutation or package save.
Run in Editor Python; compares original animation at captured clip_s against SourceABP.
"""
from pathlib import Path
import csv
import importlib.util
import json
import math
import unreal

PROJECT = Path(unreal.Paths.project_dir())
OUTPUT = PROJECT / 'Feature/doc/evidence/animation-stability-20260928/secondary-stability.json'
BASELINE = PROJECT / 'tmp/animation-stability/before'
spec = importlib.util.spec_from_file_location('boxing_audit_helpers', PROJECT / 'Scripts/Editor/AuditBoxingStability.py')
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
CLIPS = {
    'straight': '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02',
    'hook': '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03',
}
MONTAGES = {
    'straight': '/Game/Rogue10m/Animation/Common/AM_Punch_02',
    'hook': '/Game/Rogue10m/Animation/Common/AM_Punch_03',
}
BONES = ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'neck_01', 'head', 'upperarm_l', 'lowerarm_l', 'hand_l', 'upperarm_r', 'lowerarm_r', 'hand_r', 'thigh_l', 'calf_l', 'foot_l', 'thigh_r', 'calf_r', 'foot_r']


def serialize(value):
    if value is None or isinstance(value, (str, bool, int, float)): return value
    if isinstance(value, unreal.Object): return value.get_path_name()
    if hasattr(value, 'export_text'):
        try: return value.export_text()
        except Exception: pass
    try:
        if not isinstance(value, str): return [serialize(item) for item in value]
    except Exception: pass
    return str(value)


def properties(asset, names):
    result = {}
    for name in names:
        try: result[name] = serialize(asset.get_editor_property(name))
        except Exception as error: result[name] = {'unavailable': str(error)}
    return result


def vector(row, prefix, kind):
    return [float(row[prefix + '_' + kind + axis]) for axis in ('xyzw' if kind == 'q' else 'xyz')]


def main():
    with (BASELINE / 'frames.csv').open(encoding='utf-8-sig', newline='') as file: frames = list(csv.DictReader(file))
    with (BASELINE / 'poses.csv').open(encoding='utf-8-sig', newline='') as file:
        poses = {(int(row['frame']), row['bone'].casefold()): row for row in csv.DictReader(file) if row['stage'] == 'source'}
    mesh = helpers.require(helpers.MANNY)
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', mesh)
    report = {'baseline': str(BASELINE), 'read_only': True, 'notes': [
        'AnimPoseSpaces.WORLD is animation component space, compared to runtime raw_cs.',
        'clip_s is recorded montage segment time; SourceABP includes montage blending and graph adjustments.',
        'No assets are changed. Property unavailable entries are API limitations, not default values.'
    ], 'clips': {}}
    for label, path in CLIPS.items():
        sequence = helpers.require(path)
        montage = helpers.require(MONTAGES[label])
        sampled = helpers.sample(label, path, helpers.MANNY)
        record = {'sampled_30fps': sampled, 'sequence_metadata': properties(sequence, [
            'additive_anim_type', 'ref_pose_type', 'ref_pose_seq', 'ref_frame_index', 'enable_root_motion',
            'force_root_lock', 'root_motion_root_lock', 'rate_scale', 'interpolation', 'skeleton', 'data_model']),
            'montage_metadata': properties(montage, ['slot_anim_tracks', 'blend_in', 'blend_out',
            'blend_out_trigger_time', 'enable_auto_blend_out', 'rate_scale', 'composite_sections', 'sync_group']),
            'runtime_samples': []}
        record['sequence_metadata']['length_seconds'] = sequence.get_play_length()
        record['montage_metadata']['length_seconds'] = montage.get_play_length()
        try:
            model = sequence.get_data_model()
            record['sequence_metadata']['data_model_metadata'] = properties(model, ['frame_rate', 'number_of_frames', 'float_curves', 'transform_curves'])
        except Exception as error: record['sequence_metadata']['data_model_error'] = str(error)
        matching = [row for row in frames if row['clip'].split('.')[0] == path and float(row['clip_s']) >= 0]
        previous = None
        for row in matching:
            index, time = int(row['frame']), float(row['clip_s'])
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
            names = {str(name).casefold(): str(name) for name in unreal.AnimPoseExtensions.get_bone_names(pose)}
            item = {'frame': index, 'clip_s': time, 'montage_s': float(row['montage_s']), 'attack_elapsed': float(row['attack_elapsed']), 'bones': {}}
            for bone in BONES:
                if bone not in names or (index, bone) not in poses: continue
                raw = poses[index, bone]
                local = helpers.transform(unreal.AnimPoseExtensions.get_bone_pose(pose, names[bone], unreal.AnimPoseSpaces.LOCAL))
                world = helpers.transform(unreal.AnimPoseExtensions.get_bone_pose(pose, names[bone], unreal.AnimPoseSpaces.WORLD))
                data = {'clip_local': local, 'clip_cs': world,
                    'source_cs_rotation': vector(raw, 'raw_cs', 'q'), 'source_cs_position': vector(raw, 'raw_cs', 't'),
                    'source_local_rotation': vector(raw, 'local', 'q'),
                    'clip_to_source_cs_angle_deg': helpers.angle(world['rotation'], vector(raw, 'raw_cs', 'q')),
                    'clip_to_source_local_angle_deg': helpers.angle(local['rotation'], vector(raw, 'local', 'q')),
                    'clip_to_source_cs_distance_cm': math.dist(world['position'], vector(raw, 'raw_cs', 't'))}
                if previous and bone in previous['bones']:
                    old = previous['bones'][bone]
                    data['clip_step_deg'] = helpers.angle(world['rotation'], old['clip_cs']['rotation'])
                    data['source_step_deg'] = helpers.angle(data['source_cs_rotation'], old['source_cs_rotation'])
                item['bones'][bone] = data
            record['runtime_samples'].append(item)
            previous = item
        record['runtime_summary'] = {}
        for bone in BONES:
            rows = [(row['frame'], row['bones'][bone]) for row in record['runtime_samples'] if bone in row['bones']]
            if not rows: continue
            record['runtime_summary'][bone] = {}
            for metric in ['clip_to_source_cs_angle_deg', 'clip_to_source_local_angle_deg', 'clip_to_source_cs_distance_cm', 'clip_step_deg', 'source_step_deg']:
                available = [(frame, data[metric]) for frame, data in rows if metric in data]
                if available:
                    frame, value = max(available, key=lambda entry: entry[1])
                    record['runtime_summary'][bone][metric] = {'max': value, 'frame': frame}
        report['clips'][label] = record
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    compact = {'notes': report['notes'], 'clips': {label: {'sequence_metadata': data['sequence_metadata'],
        'montage_metadata': data['montage_metadata'], 'sampled_summary': data['sampled_30fps']['summary'],
        'runtime_summary': data['runtime_summary']} for label, data in report['clips'].items()}}
    OUTPUT.with_name('secondary-stability-summary.json').write_text(json.dumps(compact, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('SECONDARY_STABILITY_AUDIT_PASS ' + str(OUTPUT))


if __name__ == '__main__':
    try: main()
    finally: unreal.SystemLibrary.quit_editor()
