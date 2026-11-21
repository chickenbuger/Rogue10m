"""Import the Blender-authored, in-place Manny roll through Unreal Editor."""

import hashlib
import json
import math
import os
from pathlib import Path

import unreal


NAME = 'A_CommonShoulderRoll'
DIRECTORY = '/Game/Rogue10m/Animation/Common/Evade'
SKELETON = '/Game/Characters/Mannequins/Meshes/SK_Mannequin'


def import_roll(source, skeleton, sample_rate):
    ui = unreal.FbxImportUI()
    ui.set_editor_properties({
        'automated_import_should_detect_type': False,
        'mesh_type_to_import': unreal.FBXImportType.FBXIT_ANIMATION,
        'original_import_type': unreal.FBXImportType.FBXIT_ANIMATION,
        'import_as_skeletal': True,
        'import_mesh': False,
        'import_animations': True,
        'import_materials': False,
        'import_textures': False,
        'create_physics_asset': False,
        'skeleton': skeleton,
        'override_animation_name': NAME,
    })
    ui.get_editor_property('anim_sequence_import_data').set_editor_properties({
        'animation_length': unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
        'use_default_sample_rate': False,
        'custom_sample_rate': sample_rate,
        'remove_redundant_keys': False,
        'convert_scene': True,
        'convert_scene_unit': True,
    })
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        'filename': str(source),
        'destination_path': DIRECTORY,
        'destination_name': NAME,
        'automated': True,
        'replace_existing': False,
        'save': False,
        'options': ui,
        'factory': unreal.FbxFactory(),
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(DIRECTORY + '/' + NAME)


def verify_roll(clip, skeleton, sample_rate):
    if not isinstance(clip, unreal.AnimSequence) or clip.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('The imported roll must use the Manny skeleton')
    duration = clip.get_play_length()
    model = clip.get_editor_property('data_model_interface')
    frame_rate = model.get_frame_rate()
    frame_count = model.get_number_of_frames()
    if (frame_rate.numerator, frame_rate.denominator) != (sample_rate, 1) or frame_count < 12:
        raise RuntimeError(f'Unexpected roll sampling: {frame_rate}, {frame_count} frames')

    options = unreal.AnimPoseEvaluationOptions()
    roots = []
    for frame in range(frame_count + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, min(frame / sample_rate, duration), options)
        roots.append(unreal.AnimPoseExtensions.get_bone_pose(pose, 'root', unreal.AnimPoseSpaces.LOCAL))
    origin = roots[0]
    root_travel = max(math.dist(origin.translation.to_tuple(), sample.translation.to_tuple()) for sample in roots)
    origin_rotation = (origin.rotation.x, origin.rotation.y, origin.rotation.z, origin.rotation.w)
    rotation_error = max(1.0 - abs(sum(a * b for a, b in zip(origin_rotation,
        (sample.rotation.x, sample.rotation.y, sample.rotation.z, sample.rotation.w)))) for sample in roots)
    if not math.isfinite(root_travel) or root_travel > 0.05 or rotation_error > 0.000001:
        raise RuntimeError(f'The authored root must stay in place: {root_travel} cm, rotation error {rotation_error}')
    return {'asset': clip.get_path_name(), 'duration_s': duration, 'sample_rate': sample_rate,
            'frame_count': frame_count, 'root_travel_cm': root_travel, 'root_rotation_error': rotation_error}


def main():
    source = Path(os.environ['ROGUE_ROLL_FBX_PATH']).resolve(strict=True)
    report = Path(os.environ['ROGUE_ROLL_IMPORT_REPORT'])
    sample_rate = int(os.environ['ROGUE_ROLL_FPS'])
    skeleton = unreal.load_asset(SKELETON)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError('Manny skeleton could not be loaded')
    if unreal.EditorAssetLibrary.does_asset_exist(DIRECTORY + '/' + NAME) or report.exists():
        raise RuntimeError('Import requires a fresh asset and report path')

    clip = import_roll(source, skeleton, sample_rate)
    verification = verify_roll(clip, skeleton, sample_rate)
    clip.set_editor_property('enable_root_motion', False)
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    unreal.EditorAssetLibrary.set_metadata_tag(clip, 'Rogue10m.BlenderFBXSHA256', digest)
    if not unreal.EditorAssetLibrary.save_loaded_asset(clip):
        raise RuntimeError('Failed to save the verified roll')
    verification.update({'fbx_sha256': digest, 'extracted_root_motion': False})
    report.write_text(json.dumps(verification, indent=2) + '\n', encoding='utf-8')
    unreal.log(f'COMMON_ROLL_IMPORT {verification}')


main()
