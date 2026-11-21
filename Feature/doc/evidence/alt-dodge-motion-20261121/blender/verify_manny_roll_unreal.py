"""Fresh Unreal FBX import and root/pose audit in the isolated Extract project."""

import json
import math
import os
from pathlib import Path

import unreal


OUT = Path(__file__).resolve().parent / os.getenv('ROGUE_SHOULDER_ROLL_LABEL', 'v2')
SKELETON = '/Game/Characters/Mannequins/Meshes/SK_Mannequin'
BONES = ('root', 'pelvis', 'spine_04', 'neck_01', 'neck_02', 'head', 'upperarm_l', 'upperarm_r',
         'hand_l', 'hand_r', 'calf_l', 'calf_r', 'foot_l', 'foot_r', 'ball_l', 'ball_r')


def main():
    if Path(unreal.Paths.project_dir()).name != 'Extract':
        raise RuntimeError('Fresh FBX import is restricted to the isolated Extract fixture')
    target = unreal.load_asset(SKELETON)
    if not isinstance(target, unreal.Skeleton):
        raise RuntimeError('Manny target skeleton is absent')
    options = unreal.FbxImportUI()
    options.set_editor_properties({
        'automated_import_should_detect_type': False,
        'mesh_type_to_import': unreal.FBXImportType.FBXIT_ANIMATION,
        'original_import_type': unreal.FBXImportType.FBXIT_ANIMATION,
        'import_mesh': False,
        'import_animations': True,
        'import_as_skeletal': True,
        'import_materials': False,
        'import_textures': False,
        'skeleton': target,
    })
    options.anim_sequence_import_data.set_editor_properties({
        'use_default_sample_rate': False,
        'custom_sample_rate': 60,
        'remove_redundant_keys': False,
        'import_bone_tracks': True,
    })
    task = unreal.AssetImportTask()
    task.automated = True
    task.filename = str(OUT / 'MannyShoulderRoll.fbx')
    task.destination_path = f'/Game/RollAudit/{OUT.name}'
    task.destination_name = 'A_MannyShoulderRoll'
    task.replace_existing = False
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sequences = [value for value in task.get_objects() if isinstance(value, unreal.AnimSequence)]
    if len(sequences) != 1:
        raise RuntimeError(f'Expected one imported sequence, got {task.imported_object_paths}')
    seq = sequences[0]
    length = seq.get_play_length()
    expected_length = 25 / 60
    if abs(length - expected_length) > .002:
        raise RuntimeError(f'Imported duration was {length}, expected {expected_length}')
    poses = []
    eval_options = unreal.AnimPoseEvaluationOptions()
    eval_options.set_editor_properties({'evaluation_type': unreal.AnimDataEvalType.RAW, 'should_retarget': False})
    for i in range(26):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, length * i / 25, eval_options)
        transforms = {}
        for bone in BONES:
            tr = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            transforms[bone] = {
                'p': [getattr(tr.translation, component) for component in 'xyz'],
                'q': [getattr(tr.rotation, component) for component in 'xyzw'],
            }
        poses.append(transforms)
    roots = [pose['root'] for pose in poses]
    drift = max(math.dist(root['p'], roots[0]['p']) for root in roots)
    if drift > .01:
        raise RuntimeError(f'Imported roll root drifted {drift} cm')
    if seq.get_editor_property('enable_root_motion'):
        raise RuntimeError('Imported animation root motion is enabled')
    endpoint = max(math.dist(poses[0][bone]['p'], poses[-1][bone]['p']) for bone in BONES)
    if endpoint > .1:
        raise RuntimeError(f'Imported endpoint mismatched {endpoint} cm')
    manifest = {'object': seq.get_path_name(), 'skeleton': target.get_path_name(), 'length_seconds': length,
                'root_drift_cm': drift, 'max_endpoint_bone_position_error_cm': endpoint, 'samples': poses}
    (OUT / 'unreal-reimport.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


main()
