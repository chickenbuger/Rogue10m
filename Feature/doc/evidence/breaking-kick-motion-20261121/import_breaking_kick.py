"""Import the original Blender motion and rebind the existing Knuckle Special only."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal


root = Path(unreal.Paths.project_dir()).resolve()
if root.name != "BreakingKickMotionWorktree20261121":
    raise RuntimeError("Use the physical BreakingKick checkout.")
source = Path(os.environ["ROGUE_BREAKING_KICK_FBX"]).resolve()
report = Path(os.environ["ROGUE_BREAKING_KICK_REPORT"]).resolve()
destination = "/Game/Rogue10m/Animation/Combat/FighterSkills"
sequence_name = "A_Fighter_BreakingKick"
montage_name = "AM_Fighter_BreakingKick"
clip_path, montage_path = (f"{destination}/{name}" for name in (sequence_name, montage_name))
update_candidate = os.environ.get("ROGUE_BREAKING_KICK_REPLACE_CANDIDATE") == "1"
exists = [unreal.EditorAssetLibrary.does_asset_exist(p) for p in (clip_path, montage_path)]
if report.exists() or exists != [update_candidate, update_candidate]:
    raise RuntimeError("Use a fresh report and explicitly select whether to reimport the isolated candidate.")

skill = unreal.load_asset("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special")
current = skill.get_editor_property("attack_montage")
old = unreal.load_asset("/Game/Rogue10m/Animation/Combat/FistAuthored/AM_Knuckle_Uppercut")
skeleton = old.get_editor_property("skeleton")
if current.get_name() != (montage_name if update_candidate else old.get_name()):
    raise RuntimeError("Expected the deployed Knuckle Special and original Uppercut montage.")

fields = ("input_slot", "hit_start_delay_seconds", "animation_play_rate", "attack_range", "attack_shape",
          "arc_angle_degrees", "hit_count", "hit_mode", "hit_interval", "damage", "attack_cooldown",
          "effect_attach_socket", "enable_combo", "max_hits_per_target", "max_targets_per_hit")
preserved = {name: str(skill.get_editor_property(name)) for name in fields}

ui = unreal.FbxImportUI()
ui.set_editor_properties({
    "automated_import_should_detect_type": False, "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION,
    "original_import_type": unreal.FBXImportType.FBXIT_ANIMATION, "import_mesh": False,
    "import_as_skeletal": True, "import_animations": True, "import_materials": False, "import_textures": False,
    "create_physics_asset": False, "skeleton": skeleton, "override_animation_name": sequence_name,
})
ui.get_editor_property("anim_sequence_import_data").set_editor_properties({
    "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
    "use_default_sample_rate": False, "custom_sample_rate": 60, "remove_redundant_keys": False,
    "convert_scene": True, "convert_scene_unit": True,
})
task = unreal.AssetImportTask()
task.set_editor_properties({
    "filename": str(source), "destination_path": destination, "destination_name": sequence_name,
    "automated": True, "replace_existing": update_candidate, "replace_existing_settings": update_candidate,
    "save": False, "options": ui, "factory": unreal.FbxFactory(),
})
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
clips = [entry for entry in task.get_objects() if isinstance(entry, unreal.AnimSequence)]
if len(clips) != 1 or clips[0].get_path_name() != f"{clip_path}.{sequence_name}":
    raise RuntimeError(f"Unexpected candidate import: {task.imported_object_paths}")
clip = clips[0]
model = clip.get_editor_property("data_model_interface")
rate = model.get_frame_rate()
if (rate.numerator, rate.denominator, model.get_number_of_frames()) != (60, 1, 100) or clip.get_editor_property("skeleton") != skeleton:
    raise RuntimeError("Expected 100 x 60Hz time intervals and the same Manny skeleton.")
if abs(clip.get_play_length() - old.get_play_length()) > 1e-6:
    raise RuntimeError("The clip and original montage must share the same clock.")

options = unreal.AnimPoseEvaluationOptions()
options.set_editor_properties({"evaluation_type": unreal.AnimDataEvalType.RAW, "should_retarget": False})
targets = ("root", "pelvis", "head", "ball_l", "ball_r", "hand_l", "hand_r")
poses = []
for frame in range(101):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, frame / 60, options)
    poses.append({name: unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD).translation.to_tuple() for name in targets})
travel = {name: max(math.dist(row[name], poses[0][name]) for row in poses) for name in targets}
if travel["root"] > .01 or travel["ball_l"] > .1 or travel["ball_r"] < 100.0:
    raise RuntimeError(f"Manny import did not preserve stance or kick movement: {travel}")
clip.set_editor_property("enable_root_motion", False)
digest = hashlib.sha256(source.read_bytes()).hexdigest()
unreal.EditorAssetLibrary.set_metadata_tag(clip, "Rogue10m.BlenderFBXSHA256", digest)
unreal.EditorAssetLibrary.set_metadata_tag(clip, "Rogue10m.Source", "Original Blender motion on project-authored Manny FistAuthored ready pose")

montage = current if update_candidate else unreal.EditorAssetLibrary.duplicate_asset(old.get_path_name(), montage_path)
if not isinstance(montage, unreal.AnimMontage):
    raise RuntimeError("Cannot duplicate the preserved Uppercut montage.")
tracks = montage.get_editor_property("slot_anim_tracks")
if len(tracks) != 1 or str(tracks[0].get_editor_property("slot_name")) != "DefaultSlot":
    raise RuntimeError("Expected one compatible DefaultSlot.")
slot = tracks[0]
track = slot.get_editor_property("anim_track")
segments = track.get_editor_property("anim_segments")
if len(segments) != 1:
    raise RuntimeError("Expected one original segment with its existing time and play rate.")
segment_names = ("anim_start_time", "anim_end_time", "start_pos", "anim_play_rate", "looping_count")
segment = segments[0]
segment_contract = {name: segment.get_editor_property(name) for name in segment_names}
segment.set_editor_property("anim_reference", clip)
segments[0] = segment
track.set_editor_property("anim_segments", segments)
slot.set_editor_property("anim_track", track)
tracks[0] = slot
montage.set_editor_property("slot_anim_tracks", tracks)
reopened_segment = montage.get_editor_property("slot_anim_tracks")[0].get_editor_property("anim_track").get_editor_property("anim_segments")[0]
if (reopened_segment.get_editor_property("anim_reference") != clip
        or any(reopened_segment.get_editor_property(name) != value for name, value in segment_contract.items())
        or montage.get_play_length() != old.get_play_length()):
    raise RuntimeError("Original montage segment timing changed.")
skill.set_editor_property("attack_montage", montage)
if any(str(skill.get_editor_property(name)) != expected for name, expected in preserved.items()):
    raise RuntimeError("A protected skill field changed.")
for packaged in (clip, montage, skill):
    if not unreal.EditorAssetLibrary.save_loaded_asset(packaged, False):
        raise RuntimeError(f"Could not save {packaged.get_path_name()}")
details = {
    "old_montage": old.get_path_name(), "clip": clip.get_path_name(), "montage": montage.get_path_name(),
    "skill": skill.get_path_name(), "skeleton": skeleton.get_path_name(), "fps": 60, "intervals": 100,
    "duration_s": clip.get_play_length(), "segment": segment_contract, "preserved_skill_fields": preserved,
    "blender_fbx_sha256": digest, "raw_max_travel_cm": travel, "raw_contact_frame_13_cm": poses[13],
}
report.write_text(json.dumps(details, indent=2) + "\n", encoding="utf-8")
unreal.log("RESULT=BREAKING_KICK_AUTHORED_ASSETS_IMPORTED")
