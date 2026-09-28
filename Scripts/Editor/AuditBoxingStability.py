"""Read-only audit of native imported Boxing, Manny retarget, idle and appearance bakes.
No import, retarget, asset property writes, package saves or pose edits are performed.
"""
from pathlib import Path
import hashlib
import json
import math
import unreal

PROJECT = Path(unreal.Paths.project_dir())
OUTPUT = PROJECT / 'Feature/doc/evidence/animation-stability-20260928/boxing-stability.json'
PREVIEW = '/Game/Rogue10m/Animation/Preview/Boxing'
COMBAT = '/Game/Rogue10m/Animation/Combat/Boxing'
MANNY = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
SOURCE = PREVIEW + '/Source/Persistent/SK_BoxingSource'
CLIPS = {
    'native_import': (SOURCE + '_Anim_mixamo_com', SOURCE),
    'manny_preview': (PREVIEW + '/A_Boxing_Manny', MANNY),
    'manny_gameplay_copy': (COMBAT + '/A_BoxingJab_Manny', MANNY),
    'appearance_left': (COMBAT + '/A_BoxingLeftJab_Appearance', MANNY),
    'appearance_right': (COMBAT + '/A_BoxingRightJab_Appearance', MANNY),
    'idle': ('/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle', MANNY),
}
BONES = {
    'root': 'Hips', 'pelvis': 'Hips', 'spine_01': 'Spine', 'spine_02': 'Spine1', 'spine_03': 'Spine2',
    'neck_01': 'Neck', 'head': 'Head', 'clavicle_l': 'LeftShoulder', 'upperarm_l': 'LeftArm',
    'lowerarm_l': 'LeftForeArm', 'hand_l': 'LeftHand', 'clavicle_r': 'RightShoulder',
    'upperarm_r': 'RightArm', 'lowerarm_r': 'RightForeArm', 'hand_r': 'RightHand',
    'thigh_l': 'LeftUpLeg', 'calf_l': 'LeftLeg', 'foot_l': 'LeftFoot', 'ball_l': 'LeftToeBase',
    'thigh_r': 'RightUpLeg', 'calf_r': 'RightLeg', 'foot_r': 'RightFoot', 'ball_r': 'RightToeBase',
    'index_01_l': 'LeftHandIndex1', 'index_03_l': 'LeftHandIndex3',
    'index_01_r': 'RightHandIndex1', 'index_03_r': 'RightHandIndex3',
}
LIMBS = [('upperarm_l','lowerarm_l'),('lowerarm_l','hand_l'),('upperarm_r','lowerarm_r'),
         ('lowerarm_r','hand_r'),('thigh_l','calf_l'),('calf_l','foot_l'),('thigh_r','calf_r'),('calf_r','foot_r')]


def require(path):
    value = unreal.load_asset(path)
    if not value: raise RuntimeError('Missing asset: ' + path)
    return value


def transform(value):
    p, q, s = value.translation, value.rotation, value.scale3d
    result = {'position':[p.x,p.y,p.z], 'rotation':[q.x,q.y,q.z,q.w], 'scale':[s.x,s.y,s.z]}
    if not all(math.isfinite(v) for values in result.values() for v in values):
        raise RuntimeError('Nonfinite bone transform')
    return result


def angle(a, b):
    aa=math.sqrt(sum(x*x for x in a));bb=math.sqrt(sum(x*x for x in b))
    if min(aa,bb)<1e-10: return None
    return math.degrees(2*math.acos(min(1.0,abs(sum(x*y for x,y in zip(a,b))/(aa*bb)))))


def sample(label, path, mesh_path):
    sequence, mesh = require(path), require(mesh_path)
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', mesh)
    fps = 120 if label.startswith('appearance_') else 30
    duration = sequence.get_play_length()
    first = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, 0.0, options)
    available = {str(n).split(':')[-1]:str(n) for n in unreal.AnimPoseExtensions.get_bone_names(first)}
    mapped = {}
    for target, source in BONES.items():
        expected = source if label == 'native_import' else target
        if expected in available: mapped[target] = available[expected]
    if 'head' not in mapped or 'pelvis' not in mapped: raise RuntimeError('Required head/pelvis bone missing')
    reference = {bone: {space:transform(unreal.AnimPoseExtensions.get_ref_bone_pose(first, name, enum))
        for space, enum in [('local',unreal.AnimPoseSpaces.LOCAL),('world',unreal.AnimPoseSpaces.WORLD)]}
        for bone,name in mapped.items()}
    frames=[]
    for index in range(round(duration*fps)+1):
        time=min(index/fps,duration)
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence,time,options)
        frames.append({'time':time,'bones':{bone:{space:transform(unreal.AnimPoseExtensions.get_bone_pose(pose,name,enum))
            for space,enum in [('local',unreal.AnimPoseSpaces.LOCAL),('world',unreal.AnimPoseSpaces.WORLD)]}
            for bone,name in mapped.items()}})
    summaries={}
    for bone in mapped:
        values=[row['bones'][bone] for row in frames];ref=reference[bone]
        summaries[bone]={
            'start_world_position':values[0]['world']['position'],
            'start_world_rotation':values[0]['world']['rotation'],
            'max_world_displacement_from_first_cm':max(math.dist(v['world']['position'],values[0]['world']['position']) for v in values),
            'max_world_rotation_from_first_deg':max(angle(v['world']['rotation'],values[0]['world']['rotation']) for v in values),
            'max_world_rotation_from_reference_deg':max(angle(v['world']['rotation'],ref['world']['rotation']) for v in values),
            'start_world_rotation_from_reference_deg':angle(values[0]['world']['rotation'],ref['world']['rotation']),
            'max_world_step_rotation_deg':max([angle(a['world']['rotation'],b['world']['rotation']) for a,b in zip(values,values[1:])] or [0]),
            'max_local_rotation_from_reference_deg':max(angle(v['local']['rotation'],ref['local']['rotation']) for v in values),
            'max_local_translation_delta_reference_cm':max(math.dist(v['local']['position'],ref['local']['position']) for v in values),
            'local_scale_min':[min(v['local']['scale'][axis] for v in values) for axis in range(3)],
            'local_scale_max':[max(v['local']['scale'][axis] for v in values) for axis in range(3)],
        }
    lengths={}
    for parent,child in LIMBS:
        if parent not in mapped or child not in mapped: continue
        vals=[math.dist(r['bones'][parent]['world']['position'],r['bones'][child]['world']['position']) for r in frames]
        base=math.dist(reference[parent]['world']['position'],reference[child]['world']['position'])
        lengths[parent+'->'+child]={'min_cm':min(vals),'max_cm':max(vals),'reference_cm':base,
                                   'min_ref_ratio':min(vals)/base if base else None,'max_ref_ratio':max(vals)/base if base else None}
    return {'asset':path,'mesh':mesh_path,'duration':duration,'fps':fps,'bone_map':mapped,
            'reference':reference,'summary':summaries,'limb_lengths':lengths,'frames':frames}


def comparison(a,b):
    result={}
    for bone in sorted(set(a['bones']) & set(b['bones'])):
        av,bv=a['bones'][bone],b['bones'][bone]
        result[bone]={space:{'position_gap_cm':math.dist(av[space]['position'],bv[space]['position']),
                            'rotation_gap_deg':angle(av[space]['rotation'],bv[space]['rotation'])}
                      for space in ['local','world']}
    return result


def main():
    clips={label:sample(label,*values) for label,values in CLIPS.items()}
    original=Path('C:/Users/PC/Downloads/Boxing.fbx')
    sha=hashlib.sha256(original.read_bytes()).hexdigest()
    imported=require(CLIPS['native_import'][0])
    recorded=unreal.EditorAssetLibrary.get_metadata_tag(imported,'Boxing.OriginalSHA256').lower()
    if recorded!=sha: raise RuntimeError('Source FBX provenance mismatch')
    transitions={}
    idle=clips['idle']['frames'][0]
    for label in ['manny_preview','appearance_left','appearance_right']:
        transitions['idle_to_'+label]=comparison(idle,clips[label]['frames'][0])
    transitions['left_end_to_right_start']=comparison(clips['appearance_left']['frames'][-1],clips['appearance_right']['frames'][0])
    copy_errors=[]
    for a,b in zip(clips['manny_preview']['frames'],clips['manny_gameplay_copy']['frames']):
        copy_errors.extend(v['world']['position_gap_cm'] for v in comparison(a,b).values())
    report={'source_fbx':str(original),'source_sha256':sha,'import_metadata_sha256':recorded,
            'method':'Same-skeleton idle/attack rotation compared directly. Native/target local axes differ; only their relative motion angles and limb ratios are comparable.',
            'limitations':['Native source rotations come from the provenance-checked UE FBX import, not a second independent FBX SDK extraction.',
                           'World means animation component space, without actor transform or runtime montage blending.',
                           'This script never saves or changes animation assets.'],
            'preview_to_gameplay_copy_max_position_error_cm':max(copy_errors),'transitions':transitions,'clips':clips}
    OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    OUTPUT.write_text(json.dumps(report,indent=2),encoding='utf-8')
    compact={'head':{label:clip['summary']['head'] for label,clip in clips.items()},
             'idle_to_attack_head':{label:value['head'] for label,value in transitions.items()}}
    (OUTPUT.parent/'boxing-stability-summary.json').write_text(json.dumps(compact,indent=2),encoding='utf-8')
    unreal.log('RESULT=BOXING_STABILITY_AUDIT_PASSED '+str(OUTPUT))


if __name__=='__main__':
    try: main()
    finally: unreal.SystemLibrary.quit_editor()
