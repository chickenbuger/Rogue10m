"""Import the verified Manny additive charge brace into its project-owned package."""

import hashlib
import json
import math
import os
from pathlib import Path

import unreal


NAME = 'A_GreatSword_CapstoneChargeBrace'
DIRECTORY = '/Game/Rogue10m/Animation/Combat/GreatSword20261110'
PACKAGE = f'{DIRECTORY}/{NAME}'
OBJECT_PATH = f'{PACKAGE}.{NAME}'
SKELETON = '/Game/Characters/Mannequins/Meshes/SK_Mannequin'
SOURCE_SHA256 = 'd36664c4e902c84279df9e52c5d922099556e8699f540f640f83c8698c6ff7f5'
SAMPLE_RATE = 120
FRAME_INTERVALS = 88
PROTECTED = ('root', 'pelvis', 'foot_l', 'foot_r', 'ball_l', 'ball_r')


def import_animation(source, skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_properties({
        'automated_import_should_detect_type': False,
        'mesh_type_to_import': unreal.FBXImportType.FBXIT_ANIMATION,
        'original_import_type': unreal.FBXImportType.FBXIT_ANIMATION,
        'import_as_skeletal': True, 'import_mesh': False, 'import_animations': True,
        'import_materials': False, 'import_textures': False, 'create_physics_asset': False,
        'skeleton': skeleton, 'override_animation_name': NAME,
    })
    ui.get_editor_property('anim_sequence_import_data').set_editor_properties({
        'animation_length': unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
        'use_default_sample_rate': False, 'custom_sample_rate': SAMPLE_RATE, 'remove_redundant_keys': False,
        'convert_scene': True, 'convert_scene_unit': True,
    })
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        'filename': str(source), 'destination_path': DIRECTORY, 'destination_name': NAME,
        'automated': True, 'replace_existing': False, 'save': False, 'options': ui, 'factory': unreal.FbxFactory(),
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if list(task.get_editor_property('imported_object_paths')) != [OBJECT_PATH]:
        raise RuntimeError('Animation-only task must produce exactly its new project-owned destination')
    return unreal.load_asset(PACKAGE)


def verify(clip, skeleton):
    if not isinstance(clip, unreal.AnimSequence) or clip.get_path_name() != OBJECT_PATH:
        raise RuntimeError('The import is not the expected AnimSequence')
    if clip.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('The import did not bind the verified Manny skeleton')
    duration = clip.get_play_length()
    model = clip.get_editor_property('data_model_interface')
    rate = model.get_frame_rate()
    if (rate.numerator, rate.denominator) != (SAMPLE_RATE, 1) or model.get_number_of_frames() != FRAME_INTERVALS:
        raise RuntimeError('The import did not preserve the 89 half-frame samples')
    if abs(duration - FRAME_INTERVALS / SAMPLE_RATE) > .0002:
        raise RuntimeError('Unexpected duration for the held pose')
    options = unreal.AnimPoseEvaluationOptions()
    comparisons = []
    wrists = []
    for frame in range(FRAME_INTERVALS + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, min(frame / SAMPLE_RATE, duration), options)
        result = {}
        for name in PROTECTED:
            value = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
            position = value.translation.to_tuple()
            quat = (value.rotation.x, value.rotation.y, value.rotation.z, value.rotation.w)
            if not all(math.isfinite(v) for v in (*position, *quat)):
                raise RuntimeError(f'Nonfinite transform at {name}, frame {frame}')
            result[name] = (position, quat)
        comparisons.append(result)
        wrists.append(tuple(unreal.AnimPoseExtensions.get_bone_pose(
            pose, hand, unreal.AnimPoseSpaces.WORLD).translation.to_tuple() for hand in ('hand_l', 'hand_r')))
    first = comparisons[0]
    travel = max(math.dist(sample[name][0], first[name][0]) for sample in comparisons for name in PROTECTED)
    rotation_error = max(1.0 - abs(sum(a * b for a, b in zip(sample[name][1], first[name][1])))
                         for sample in comparisons for name in PROTECTED)
    if travel > .05 or rotation_error > .000001:
        raise RuntimeError(f'Root, pelvis or feet changed: {travel} cm / {rotation_error}')
    wrist_peak = max(math.dist(p, q) for pair in wrists for p, q in zip(pair, wrists[0]))
    if wrist_peak < 1.0:
        raise RuntimeError('The exported charge has no meaningful wrist preparation')
    return {'asset': OBJECT_PATH, 'duration_s': duration, 'sample_rate': SAMPLE_RATE,
            'intervals': FRAME_INTERVALS, 'protected_travel_cm': travel, 'protected_rotation_error': rotation_error,
            'wrist_peak_cm': wrist_peak}


def main():
    source = Path(os.environ['ROGUE_CAPSTONE_BRACE_FBX']).resolve(strict=True)
    report = Path(os.environ['ROGUE_CAPSTONE_BRACE_REPORT']).resolve()
    if report.exists() or unreal.EditorAssetLibrary.does_asset_exist(PACKAGE):
        raise RuntimeError('A fresh package and report path are required')
    if hashlib.sha256(source.read_bytes()).hexdigest() != SOURCE_SHA256:
        raise RuntimeError('Unexpected FBX; review its pose and roundtrip before importing')
    skeleton = unreal.load_asset(SKELETON)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError('The existing Manny skeleton is missing')
    clip = import_animation(source, skeleton)
    verified = verify(clip, skeleton)
    clip.set_editor_properties({
        'enable_root_motion': False,
        'additive_anim_type': unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE,
        'ref_pose_type': unreal.AdditiveBasePoseType.ABPT_LOCAL_ANIM_FRAME,
        'ref_frame_index': 0,
    })
    unreal.EditorAssetLibrary.set_metadata_tag(clip, 'Rogue10m.BlenderFBXSHA256', SOURCE_SHA256)
    if not unreal.EditorAssetLibrary.save_loaded_asset(clip, False):
        raise RuntimeError('Could not save the verified brace AnimSequence')
    verified['fbx_sha256'] = SOURCE_SHA256
    with report.open('x', encoding='utf-8') as output:
        output.write(json.dumps(verified, indent=2) + '\n')
    print('RESULT=CAPSTONE_CHARGE_BRACE_IMPORTED')


if __name__ == '__main__':
    main()
