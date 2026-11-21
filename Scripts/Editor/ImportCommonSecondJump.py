"""Import the Blender-authored, in-place Manny second-jump pose through Unreal Editor."""

import hashlib
import json
import math
import os
from pathlib import Path

import unreal


NAME = "A_CommonSecondJumpImpulse"
DIRECTORY = "/Game/Rogue10m/Animation/Common/Airborne"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
FPS = 60
INTERVALS = 15
LEG_BONES = ("thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r", "ball_l", "ball_r")
STATIC_BONES = ("root", "pelvis", "spine_03", "head", "upperarm_l", "upperarm_r")


def import_sequence(source, skeleton):
    options = unreal.FbxImportUI()
    options.set_editor_properties({
        "automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION,
        "original_import_type": unreal.FBXImportType.FBXIT_ANIMATION,
        "import_as_skeletal": True,
        "import_mesh": False,
        "import_animations": True,
        "import_materials": False,
        "import_textures": False,
        "create_physics_asset": False,
        "skeleton": skeleton,
        "override_animation_name": NAME,
    })
    options.get_editor_property("anim_sequence_import_data").set_editor_properties({
        "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
        "use_default_sample_rate": False,
        "custom_sample_rate": FPS,
        "remove_redundant_keys": False,
        "convert_scene": True,
        "convert_scene_unit": True,
    })
    task = unreal.AssetImportTask()
    task.set_editor_properties({
        "filename": str(source),
        "destination_path": DIRECTORY,
        "destination_name": NAME,
        "automated": True,
        "replace_existing": False,
        "save": False,
        "options": options,
        "factory": unreal.FbxFactory(),
    })
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(DIRECTORY + "/" + NAME)


def angular_difference(first, other):
    a = (first.x, first.y, first.z, first.w)
    b = (other.x, other.y, other.z, other.w)
    dot = abs(sum(x * y for x, y in zip(a, b))) / (math.hypot(*a) * math.hypot(*b))
    return math.degrees(2 * math.acos(min(dot, 1.0)))


def verify_sequence(clip, skeleton):
    if not isinstance(clip, unreal.AnimSequence) or clip.get_editor_property("skeleton") != skeleton:
        raise RuntimeError("Cannot load the second jump on the existing Manny skeleton")
    data_model = clip.get_editor_property("data_model_interface")
    rate = data_model.get_frame_rate()
    length = clip.get_play_length()
    if ((rate.numerator, rate.denominator) != (FPS, 1)
            or data_model.get_number_of_frames() != INTERVALS or abs(length - INTERVALS / FPS) > .001):
        raise RuntimeError(f"Unexpected second jump timing: {rate}, {length}")
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
    options.set_editor_property("extract_root_motion", False)
    sequences = []
    for frame in range(INTERVALS + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, min(length, frame / FPS), options)
        sequences.append({
            bone: unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)
            for bone in LEG_BONES + STATIC_BONES
        })

    peak = {}
    for bone, initial in sequences[0].items():
        samples = [frame[bone] for frame in sequences]
        drift = max(math.dist(initial.translation.to_tuple(), point.translation.to_tuple()) for point in samples)
        changes = [angular_difference(initial.rotation, point.rotation) for point in samples]
        if (drift > .05 or changes[-1] > .05 or bone in STATIC_BONES and max(changes) > .05):
            raise RuntimeError(f"Unexpected root, non-leg or endpoint movement: {bone}, {drift}, {max(changes)}")
        if bone in LEG_BONES:
            peak[bone] = max(changes)
    if not 12 <= max(peak.values()) <= 45:
        raise RuntimeError(f"Invalid second jump leg pulse: {peak}")
    return {"asset": clip.get_path_name(), "duration_seconds": length, "sample_rate": FPS,
            "intervals": INTERVALS, "leg_peak_rotation_degrees": peak}


def main():
    source = Path(os.environ["ROGUE_SECOND_JUMP_FBX"]).resolve(strict=True)
    report = Path(os.environ["ROGUE_SECOND_JUMP_REPORT"])
    skeleton = unreal.load_asset(SKELETON)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError("Manny skeleton could not be loaded")
    if unreal.EditorAssetLibrary.does_asset_exist(DIRECTORY + "/" + NAME) or report.exists():
        raise RuntimeError("Second jump import requires a fresh destination and report")
    clip = import_sequence(source, skeleton)
    verification = verify_sequence(clip, skeleton)
    clip.set_editor_property("enable_root_motion", False)
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    unreal.EditorAssetLibrary.set_metadata_tag(clip, "Rogue10m.BlenderFBXSHA256", digest)
    if not unreal.EditorAssetLibrary.save_loaded_asset(clip):
        raise RuntimeError("Could not save the verified second jump")
    verification.update({"fbx_sha256": digest, "root_motion": False})
    report.write_text(json.dumps(verification, indent=2) + "\n", encoding="utf-8")
    unreal.log("RESULT=SECOND_JUMP_ANIMATION_IMPORTED " + clip.get_path_name())


main()
