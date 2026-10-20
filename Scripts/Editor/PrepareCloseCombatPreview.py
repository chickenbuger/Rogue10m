"""Owned Close Combat UE4 -> Manny preview. No gameplay packages are modified.

Default: read-only audit. Pass -RogueCloseCombatPrepare to create an EMPTY preview
folder once. -RogueCloseCombatLockPreview sets root-lock flags only on this task's
newly generated preview sequences. Run only in Unreal Editor.
"""
import json
import math
from pathlib import Path
import runpy
import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
EXPECTED_PROJECT = Path(r'D:\Project\Rogue10m').resolve()
EVIDENCE = PROJECT / 'Feature/doc/evidence/close-combat-preparation-20261002'
VENDOR = '/Game/CloseCombatAnimSet'
PREVIEW = '/Game/Rogue10m/Animation/Preview/CloseCombat'
SOURCE_MESH = VENDOR + '/Mannequin/Character/Mesh/SK_Mannequin'
MANNY = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
SPECS = [
    ('LeftHook', 'Attacks/NinjaLeftHook', 'hand_l'),
    ('RightUppercut', 'Attacks/NinjaRightUppercut', 'hand_r'),
    ('LowRightThaiKick', 'Attacks/NinjaLowRightThaiKick', 'foot_r'),
    ('StraightKick', 'Attacks/NinjaStraightKick', 'foot_r'),
    ('BlockIntro', 'Blocks/NinjaBlockIntro', None),
    ('BlockLoop', 'Blocks/NinjaBlockLoop', None),
    ('BlockHit', 'Blocks/NinjaBlockHit', None),
    ('FrontEvade', 'Evades/NinjaFrontEvade', None),
    ('BackEvade', 'Evades/NinjaBackEvade', None),
    ('LeftEvade', 'Evades/NinjaLeftEvade', None),
    ('RightEvade', 'Evades/NinjaRightEvade', None),
]
BONES = ['root', 'pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r',
         'upperarm_l', 'lowerarm_l', 'upperarm_r', 'lowerarm_r',
         'thigh_l', 'calf_l', 'thigh_r', 'calf_r']
LIMBS = [('upperarm_l', 'lowerarm_l'), ('lowerarm_l', 'hand_l'),
         ('upperarm_r', 'lowerarm_r'), ('lowerarm_r', 'hand_r'),
         ('thigh_l', 'calf_l'), ('calf_l', 'foot_l'),
         ('thigh_r', 'calf_r'), ('calf_r', 'foot_r')]


def require(path, cls=None):
    value = unreal.load_asset(path)
    if value is None or (cls and not isinstance(value, cls)):
        raise RuntimeError('Missing/unexpected asset ' + path)
    return value


def serial(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (list, tuple, unreal.Array)):
        return [serial(item) for item in value]
    return str(value)


def angle(a, b):
    norm = math.sqrt(sum(v*v for v in a) * sum(v*v for v in b))
    if norm < 1e-10:
        raise RuntimeError('Zero quaternion')
    return math.degrees(2 * math.acos(min(1.0, abs(sum(x*y for x, y in zip(a, b))) / norm)))


def transform(value):
    p, q, scale = value.translation, value.rotation, value.scale3d
    row = {'p': [p.x, p.y, p.z], 'q': [q.x, q.y, q.z, q.w],
           's': [scale.x, scale.y, scale.z]}
    if not all(math.isfinite(v) for vector in row.values() for v in vector):
        raise RuntimeError('Nonfinite transform')
    return row


def sample(sequence, mesh, effector=None, respect_root_lock=False):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', mesh)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', not respect_root_lock)
    length = sequence.get_play_length()
    first = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, 0.0, options)
    available = {str(n) for n in unreal.AnimPoseExtensions.get_bone_names(first)}
    if any(b not in available for b in BONES):
        raise RuntimeError('Required bones absent ' + sequence.get_path_name())
    reference = {b: transform(unreal.AnimPoseExtensions.get_ref_bone_pose(
        first, b, unreal.AnimPoseSpaces.WORLD)) for b in BONES}
    frames = []
    for index in range(math.ceil(length*60)+1):
        time = min(index/60.0, length)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
        frames.append({'t': time, 'bones': {b: transform(unreal.AnimPoseExtensions.get_bone_pose(
            pose, b, unreal.AnimPoseSpaces.WORLD)) for b in BONES}})
    summary = {}
    for b in BONES:
        values = [frame['bones'][b] for frame in frames]
        summary[b] = {
            'max_rotation_from_start_deg': max(angle(v['q'], values[0]['q']) for v in values),
            'max_rotation_from_ref_deg': max(angle(v['q'], reference[b]['q']) for v in values),
            'max_step_rotation_deg_60hz': max(angle(a['q'], z['q']) for a, z in zip(values, values[1:])),
            'max_displacement_from_start_cm': max(math.dist(v['p'], values[0]['p']) for v in values),
            'position_range_cm': [max(v['p'][i] for v in values)-min(v['p'][i] for v in values) for i in range(3)],
            'end_delta_cm': [values[-1]['p'][i]-values[0]['p'][i] for i in range(3)],
        }
    lengths = {}
    for parent, child in LIMBS:
        base = math.dist(reference[parent]['p'], reference[child]['p'])
        values = [math.dist(f['bones'][parent]['p'], f['bones'][child]['p'])/base for f in frames]
        lengths[parent+'->'+child] = [min(values), max(values)]
    contact = None
    if effector:
        # Component-space +Y is Manny's forward. This is a review cue, not proven contact.
        peak = max(frames, key=lambda f: f['bones'][effector]['p'][1]-f['bones']['pelvis']['p'][1])
        contact = {'method': 'maximum +Y effector reach relative to pelvis; manual review required',
                   'bone': effector, 'candidate_sequence_seconds': peak['t'],
                   'candidate_frame_60hz': round(peak['t']*60),
                   'forward_from_pelvis_cm': peak['bones'][effector]['p'][1]-peak['bones']['pelvis']['p'][1],
                   'effector_height_cm': peak['bones'][effector]['p'][2],
                   'head_rotation_from_ref_deg': angle(peak['bones']['head']['q'], reference['head']['q'])}
    return {'asset': sequence.get_path_name(), 'skeleton': serial(sequence.get_editor_property('skeleton')),
            'duration': length, 'sample_hz': 60, 'root_motion_enabled': sequence.get_editor_property('enable_root_motion'),
            'force_root_lock': sequence.get_editor_property('force_root_lock'),
            'root_motion_root_lock': str(sequence.get_editor_property('root_motion_root_lock')),
            'pose_mode': 'package root lock respected' if respect_root_lock else 'raw root trajectory incorporated',
            'summary': summary, 'limb_length_ref_ratios': lengths,
            'contact_review_cue': contact, 'frames': frames}


def gameplay_contract():
    fields = ['attack_montage', 'animation_play_rate', 'hit_start_delay_seconds', 'hit_count',
              'hit_interval', 'hit_mode', 'damage', 'attack_range', 'attack_shape',
              'attack_trace_radius', 'box_half_width', 'box_half_height']
    skills = []
    for folder in ['/Game/DataAsset/AttackSkill/BasicBrawler', '/Game/DataAsset/AttackSkill/ClassCombat/Knuckle']:
        for path in sorted(unreal.EditorAssetLibrary.list_assets(folder, recursive=True)):
            asset = unreal.load_asset(path)
            if not isinstance(asset, unreal.Rogue10mAttackSkillData):
                continue
            row = {'path': asset.get_path_name()}
            for field in fields:
                row[field] = serial(asset.get_editor_property(field))
            montage = asset.get_editor_property('attack_montage')
            if montage:
                row['montage_seconds'] = montage.get_play_length()
                row['montage_tracks'] = []
                for track in montage.get_editor_property('slot_anim_tracks'):
                    segments = track.get_editor_property('anim_track').get_editor_property('anim_segments')
                    row['montage_tracks'].append({
                        'slot': str(track.get_editor_property('slot_name')),
                        'segments': [{key: serial(segment.get_editor_property(key))
                                      for key in ['anim_reference', 'start_pos', 'anim_start_time',
                                                  'anim_end_time', 'anim_play_rate', 'looping_count']}
                                     for segment in segments]})
            skills.append(row)
    return skills


def create(name, cls, factory):
    path = PREVIEW + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError('Refusing to replace existing preview ' + path)
    result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PREVIEW, cls, factory)
    if result is None:
        raise RuntimeError('Creation failed ' + path)
    return result


def save(asset):
    if not asset.get_path_name().startswith(PREVIEW + '/'):
        raise RuntimeError('Refusing save outside preview ' + asset.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Save failed ' + asset.get_path_name())


def prepare(source_mesh, target_mesh):
    if unreal.EditorAssetLibrary.does_directory_exist(PREVIEW):
        if unreal.EditorAssetLibrary.list_assets(PREVIEW, recursive=True):
            raise RuntimeError('Preview already contains assets; use default audit mode')
    # Validate every source before the first mutation.
    sources = [require(VENDOR+'/Animations/InPlace/'+path, unreal.AnimSequence) for _, path, _ in SPECS]
    if any(s.get_editor_property('skeleton') != source_mesh.get_editor_property('skeleton') for s in sources):
        raise RuntimeError('Source skeleton mismatch')
    rigs = []
    for name, mesh in [('IK_CloseCombat_UE4', source_mesh), ('IK_CloseCombat_Manny', target_mesh)]:
        rig = create(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        controller = unreal.IKRigController.get_controller(rig)
        if not controller.set_skeletal_mesh(mesh) or not controller.apply_auto_generated_retarget_definition():
            raise RuntimeError('Auto humanoid rig setup failed ' + name)
        chains = {str(chain.get_editor_property('chain_name')).lower()
                  for chain in controller.get_retarget_chains()}
        if 'root' not in chains:
            controller.add_retarget_chain('Root', 'root', 'root', 'None')
        if not controller.apply_auto_fbik():
            raise RuntimeError('Auto IK setup failed ' + name)
        rigs.append(rig)
    retargeter = create('RTG_CloseCombat_UE4_Manny', unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctl = unreal.IKRetargeterController.get_controller(retargeter)
    src, dst = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    for side, rig, mesh in [(src, rigs[0], source_mesh), (dst, rigs[1], target_mesh)]:
        ctl.set_ik_rig(side, rig)
        ctl.set_preview_mesh(side, mesh)
    ctl.remove_all_ops()
    ctl.add_default_ops()
    ctl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    ctl.reset_retarget_pose(ctl.get_current_retarget_pose_name(dst), [], dst)
    ctl.auto_align_all_bones(dst)
    helper = runpy.run_path(str(PROJECT/'Scripts/Editor/CreateInheritedCharacterAssets.py'),
                           run_name='close_combat_retarget_helpers')
    policy = helper['configure_pose_preserving_retargeter'](ctl)
    if policy['source_root'].lower() != 'root' or policy['target_root'].lower() != 'root' or policy['run_ik_enabled']:
        raise RuntimeError('Pose-preserving policy failed: '+json.dumps(policy))
    for asset in rigs+[retargeter]:
        save(asset)
    for (label, _, _), sequence in zip(SPECS, sources):
        destination = PREVIEW + '/A_CC_' + label + '_Manny'
        if unreal.EditorAssetLibrary.does_asset_exist(destination):
            raise RuntimeError('Output collision ' + destination)
        inputs = unreal.IKRetargetBatchOperationInputs()
        inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(sequence.get_path_name())]
        inputs.source_mesh = source_mesh
        inputs.target_mesh = target_mesh
        inputs.ik_retarget_asset = retargeter
        inputs.search = sequence.get_name()
        inputs.replace = 'A_CC_' + label + '_Manny'
        inputs.target_path = PREVIEW
        inputs.use_source_path = False
        inputs.include_referenced_assets = False
        inputs.overwrite_existing_files = False
        result = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        output = require(destination, unreal.AnimSequence)
        if len(result) != 1 or output.get_editor_property('skeleton') != target_mesh.get_editor_property('skeleton'):
            raise RuntimeError('Unexpected retarget result ' + destination)
        if abs(output.get_play_length()-sequence.get_play_length()) > 1/60:
            raise RuntimeError('Retarget duration changed ' + destination)
        output.set_editor_property('enable_root_motion', False)
        output.set_editor_property('force_root_lock', True)
        output.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        save(output)
    return policy


def lock_new_preview_only():
    baseline = json.loads((EVIDENCE/'baseline-files.json').read_text(encoding='utf-8'))
    assets = []
    for label, _, _ in SPECS:
        path = PREVIEW+'/A_CC_'+label+'_Manny'
        relative = 'Content/'+path.removeprefix('/Game/')+'.uasset'
        if relative in baseline['files']:
            raise RuntimeError('Refusing to modify a pre-task package '+relative)
        assets.append(require(path, unreal.AnimSequence))
    for asset in assets:
        asset.set_editor_property('enable_root_motion', False)
        asset.set_editor_property('force_root_lock', True)
        asset.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        save(asset)


def main():
    if PROJECT != EXPECTED_PROJECT:
        raise RuntimeError('Run only in the verified existing Rogue10m checkout')
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    source_mesh = require(SOURCE_MESH, unreal.SkeletalMesh)
    target_mesh = require(MANNY, unreal.SkeletalMesh)
    before = gameplay_contract()
    create_outputs = '-RogueCloseCombatPrepare' in unreal.SystemLibrary.get_command_line()
    policy = prepare(source_mesh, target_mesh) if create_outputs else None
    lock_outputs = '-RogueCloseCombatLockPreview' in unreal.SystemLibrary.get_command_line()
    if lock_outputs:
        lock_new_preview_only()
    retargeter_path = PREVIEW+'/RTG_CloseCombat_UE4_Manny'
    if policy is None and unreal.EditorAssetLibrary.does_asset_exist(retargeter_path):
        ctl = unreal.IKRetargeterController.get_controller(require(retargeter_path))
        policy = {'saved_op_settings': [
            {'name': str(ctl.get_op_name(i)), 'enabled': ctl.get_retarget_op_enabled(i),
             'settings': ctl.get_op_controller(i).get_settings().export_text()}
            for i in range(ctl.get_num_retarget_ops())]}
    report = {'created_preview': create_outputs, 'updated_new_preview_root_lock':lock_outputs, 'gameplay_contract': before,
              'source_mesh': source_mesh.get_path_name(), 'target_mesh': target_mesh.get_path_name(),
              'policy': policy, 'clips': {}, 'rm_evades': {},
              'limitations': ['Editor pose samples; no rendered preview or fixed-aim target damage test.',
                              'Reach peak is only a contact review cue, not an approved hit delay.',
                              'Guard hit clips are not verified parries. Evade labels do not prove directional rolls.',
                              'No Data Asset, gameplay montage, AnimBP, camera, or C++ was changed.']}
    for label, path, effector in SPECS:
        source = require(VENDOR+'/Animations/InPlace/'+path, unreal.AnimSequence)
        row = {'source': sample(source, source_mesh, effector)}
        output_path = PREVIEW+'/A_CC_'+label+'_Manny'
        if unreal.EditorAssetLibrary.does_asset_exist(output_path):
            target = require(output_path, unreal.AnimSequence)
            if target.get_editor_property('skeleton') != target_mesh.get_editor_property('skeleton'):
                raise RuntimeError('Preview is not Manny ' + output_path)
            row['manny'] = sample(target, target_mesh, effector)
            row['manny_with_package_root_lock'] = sample(target, target_mesh, effector, respect_root_lock=True)
        report['clips'][label] = row
    for direction in ['Front', 'Back', 'Left', 'Right']:
        sequence = require(VENDOR+'/Animations/RootMotion/Evades/Ninja'+direction+'Evade', unreal.AnimSequence)
        report['rm_evades'][direction] = sample(sequence, source_mesh)
    if gameplay_contract() != before:
        raise RuntimeError('Gameplay contract changed during preview preparation')
    report['gameplay_contract_unchanged'] = True
    (EVIDENCE/'editor-audit.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    compact = dict(report)
    compact['clips'] = {key: {side: {k: v for k, v in data.items() if k != 'frames'}
                             for side, data in row.items()} for key, row in report['clips'].items()}
    compact['rm_evades'] = {key: {k: v for k, v in row.items() if k != 'frames'}
                           for key, row in report['rm_evades'].items()}
    (EVIDENCE/'editor-summary.json').write_text(json.dumps(compact, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('RESULT=CLOSE_COMBAT_PREVIEW_AUDIT_PASSED clips='+str(len(report['clips'])))


if __name__ == '__main__':
    try:
        main()
    finally:
        unreal.SystemLibrary.quit_editor()
