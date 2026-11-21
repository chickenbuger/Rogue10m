"""Install the isolated project-authored shoulder strike on the one live skill."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal


root = Path(unreal.Paths.project_dir()).resolve()
if root.name != 'DirectionalRollAudit20261121':
    raise RuntimeError('Use the physical animation audit checkout.')
source = Path(os.environ['ROGUE_SHOULDER_FBX']).resolve()
report = Path(os.environ['ROGUE_SHOULDER_REPORT']).resolve()
folder = '/Game/Rogue10m/Animation/Combat/FighterSkills'
clip_name, montage_name = 'A_Fighter_ShoulderBash', 'AM_Fighter_ShoulderBash'
clip_path, montage_path = (folder + '/' + name for name in (clip_name, montage_name))
update = os.environ.get('ROGUE_SHOULDER_REIMPORT') == '1'
exists = [unreal.EditorAssetLibrary.does_asset_exist(path) for path in (clip_path, montage_path)]
if report.exists() or exists != [update, update]:
    raise RuntimeError('Use a fresh receipt and explicitly select whether to reimport.')
skill = unreal.load_asset('/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged')
current = skill.get_editor_property('attack_montage')
old = unreal.load_asset('/Game/Rogue10m/Animation/Combat/FistAuthored/AM_Knuckle_ShortBlow')
assert current.get_name() == (montage_name if update else old.get_name())
skeleton = old.get_editor_property('skeleton')
properties = ('input_slot', 'hit_start_delay_seconds', 'animation_play_rate', 'attack_range', 'attack_shape',
              'charge_seconds', 'hit_count', 'hit_mode', 'hit_interval', 'max_hits_per_target', 'max_targets_per_hit',
              'attack_cooldown', 'damage', 'enable_combo', 'effect_attach_socket', 'box_half_width', 'box_half_height')
protected = {name: str(skill.get_editor_property(name)) for name in properties}

ui = unreal.FbxImportUI()
ui.set_editor_properties({'automated_import_should_detect_type': False, 'mesh_type_to_import': unreal.FBXImportType.FBXIT_ANIMATION,
                          'original_import_type': unreal.FBXImportType.FBXIT_ANIMATION, 'import_mesh': False,
                          'import_as_skeletal': True, 'import_animations': True, 'import_materials': False,
                          'import_textures': False, 'create_physics_asset': False, 'skeleton': skeleton,
                          'override_animation_name': clip_name})
ui.get_editor_property('anim_sequence_import_data').set_editor_properties({
    'animation_length': unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME, 'use_default_sample_rate': False,
    'custom_sample_rate': 60, 'remove_redundant_keys': False, 'convert_scene': True, 'convert_scene_unit': True})
task = unreal.AssetImportTask()
task.set_editor_properties({'filename': str(source), 'destination_path': folder, 'destination_name': clip_name,
                            'automated': True, 'replace_existing': update, 'replace_existing_settings': update, 'save': False,
                            'options': ui, 'factory': unreal.FbxFactory()})
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
clips = [asset for asset in task.get_objects() if isinstance(asset, unreal.AnimSequence)]
if len(clips) != 1 or clips[0].get_path_name() != clip_path + '.' + clip_name:
    raise RuntimeError(f'Unexpected import: {task.imported_object_paths}')
clip = clips[0]
model = clip.get_editor_property('data_model_interface')
rate = model.get_frame_rate()
if (rate.numerator, rate.denominator, model.get_number_of_frames()) != (60, 1, 110) or clip.get_editor_property('skeleton') != skeleton:
    raise RuntimeError('Expected 110 source intervals at 60Hz and original Manny skeleton.')
if abs(clip.get_play_length() - old.get_play_length()) > 1e-6:
    raise RuntimeError('Original attack clock changed.')

options = unreal.AnimPoseEvaluationOptions()
options.set_editor_properties({'evaluation_type': unreal.AnimDataEvalType.RAW, 'should_retarget': False})
names = ('root','pelvis','upperarm_r','hand_r','hand_l','ball_r','ball_l')
poses = []
for index in range(111):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, index / 60, options)
    poses.append({name: unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD).translation.to_tuple()
                  for name in names})
travel = {name: max(math.dist(row[name], poses[0][name]) for row in poses) for name in names}
if travel['root'] > .08 or travel['ball_l'] > .08 or travel['upperarm_r'] < 30 or travel['ball_r'] < 15:
    raise RuntimeError(f'Invalid in-place shoulder/body movement: {travel}')
clip.set_editor_property('enable_root_motion', False)
digest = hashlib.sha256(source.read_bytes()).hexdigest()
unreal.EditorAssetLibrary.set_metadata_tag(clip, 'Rogue10m.BlenderFBXSHA256', digest)
unreal.EditorAssetLibrary.set_metadata_tag(clip, 'Rogue10m.Source', 'Original Blender shoulder pose on project-authored Manny ShortBlow guard')

montage = current if update else unreal.EditorAssetLibrary.duplicate_asset(old.get_path_name(), montage_path)
if not isinstance(montage, unreal.AnimMontage):
    raise RuntimeError('Cannot duplicate the original montage.')
tracks = montage.get_editor_property('slot_anim_tracks')
slot = tracks[0]
track = slot.get_editor_property('anim_track')
segments = track.get_editor_property('anim_segments')
if len(tracks) != 1 or len(segments) != 1 or str(slot.get_editor_property('slot_name')) != 'DefaultSlot':
    raise RuntimeError('Unexpected original slot or segment.')
segment = segments[0]
segment_options = ('anim_start_time','anim_end_time','start_pos','anim_play_rate','looping_count')
contract = {name: segment.get_editor_property(name) for name in segment_options}
segment.set_editor_property('anim_reference', clip)
segments[0] = segment
track.set_editor_property('anim_segments', segments)
slot.set_editor_property('anim_track', track)
tracks[0] = slot
montage.set_editor_property('slot_anim_tracks', tracks)
readback = montage.get_editor_property('slot_anim_tracks')[0].get_editor_property('anim_track').get_editor_property('anim_segments')[0]
if (readback.get_editor_property('anim_reference') != clip or montage.get_play_length() != old.get_play_length()
        or any(readback.get_editor_property(name) != expected for name, expected in contract.items())):
    raise RuntimeError('Animation or protected segment properties failed to read back.')
skill.set_editor_property('attack_montage', montage)
if any(str(skill.get_editor_property(name)) != expected for name, expected in protected.items()):
    raise RuntimeError('The original gameplay contract changed.')
for asset in (clip, montage, skill):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError(f'Cannot save {asset.get_path_name()}')
receipt = {'original_montage': old.get_path_name(), 'clip': clip.get_path_name(), 'montage': montage.get_path_name(),
           'skill': skill.get_path_name(), 'original_skeleton': skeleton.get_path_name(), 'duration_s': clip.get_play_length(),
           'fps': 60, 'intervals': 110, 'segment': contract, 'preserved_skill_fields': protected, 'digest': digest,
           'raw_max_travel_cm': travel, 'raw_frame_13_cm': poses[13]}
report.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
unreal.log('RESULT=SHOULDER_ASSETS_IMPORTED')
