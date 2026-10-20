"""Three scoped fighter montage bindings. Default read-only; apply once explicitly.

Run in this project's UE Editor with -RogueBindFighterAnimations only after the
baseline evidence exists. Original animations, profiles and runtime code stay
untouched. Existing generated targets are never replaced.
"""
import json
import math
from pathlib import Path
import re
import runpy
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EXPECTED = Path(r'D:\Project\Rogue10m').resolve()
EVIDENCE = ROOT / 'Feature/doc/evidence/fighter-animation-binding-20261002'
FOLDER = '/Game/Rogue10m/Animation/Combat/FighterSkills'
MANNY = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
CC = '/Game/Rogue10m/Animation/Preview/CloseCombat/'
AUTHORED = '/Game/Rogue10m/Animation/Combat/FistAuthored/'
DA = '/Game/DataAsset/AttackSkill/'
SPECS = [
    {'name':'FlowingCloud_LeftCounter','skill':DA+'StoneFist/DA_Attack_StoneFist_Jab',
     'source':CC+'A_CC_LeftHook_Manny','frames':72,'start':25/60.,'source_rate':2.,
     'contact':6/120.,'source_contact':31/60.,'bone':'hand_l','blend_in':.04,
     'reason':'Returning left hook; contact before unchanged 0.1 s combo open. No parry gameplay added.'},
    {'name':'IronChain_TwoPunch','skill':DA+'StoneFist/DA_Attack_StoneFist_Straight',
     'source':AUTHORED+'A_Knuckle_HookFlurry','frames':84,'start':0.,'source_rate':1.,
     'contact':.273,'source_contact':.273,'bone':'hand_r','blend_in':.10,
     'reason':'Two alternating short hooks. Existing single damage pulse occurs at the finishing right fist; no hit-count increase.'},
    {'name':'Overlord_PowerPunch','skill':DA+'StoneFist/DA_Attack_StoneFist_ChargedShockwave',
     'source':'/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack',
     'frames':120,'start':.85,'source_rate':1.,'contact':34/120.,
     'source_contact':68/60.,'bone':'hand_r','blend_in':.10,
     'reason':'Existing charged forward punch, leading idle removed and derived root fixed. Camera, damage and charge requirements unchanged.'},
]


def require(path, cls=None):
    value=unreal.load_asset(path)
    if value is None or (cls and not isinstance(value,cls)):
        raise RuntimeError('Missing/unexpected '+path)
    return value


def serial(value):
    if value is None or isinstance(value,(str,int,float,bool)):return value
    if isinstance(value,unreal.Object):return value.get_path_name()
    if isinstance(value,(list,tuple,unreal.Array)):return [serial(v) for v in value]
    try:return {str(k):serial(v) for k,v in value.items()}
    except (AttributeError,TypeError):pass
    if hasattr(value,'export_text'):return value.export_text()
    return str(value)


def fields():
    text=(ROOT/'Source/Rogue10m/Data/Rogue10mAttackSkillData.h').read_text(encoding='utf-8')
    text=text[text.index('class ROGUE10M_API URogue10mAttackSkillData'):]
    names=re.findall(r'^\s*(?:\w+[\w:<>]*)\s+(\w+)\s*(?:=|;)',text,re.M)
    def snake(name):
        if name.startswith('b') and len(name)>1 and name[1].isupper():name=name[1:]
        return re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()
    return [snake(n) for n in names]


def tracks(montage):
    result=[]
    for slot in montage.get_editor_property('slot_anim_tracks'):
        segments=slot.get_editor_property('anim_track').get_editor_property('anim_segments')
        result.append({'slot':str(slot.get_editor_property('slot_name')),
            'segments':[{k:serial(segment.get_editor_property(k)) for k in
            ['anim_reference','start_pos','anim_start_time','anim_end_time','anim_play_rate','looping_count']}
            for segment in segments]})
    return result


def capture():
    profiles=[];skills={};keys=fields()
    for path in ['/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed','/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Knuckle']:
        profile=require(path)
        nodes=profile.get_editor_property('skill_tree_skills')
        profiles.append({'path':profile.get_path_name(),'nodes':[s.get_path_name() for s in nodes],
            'default_bindings':serial(profile.get_editor_property('default_skill_bindings')),
            'initial_unlocks':serial(profile.get_editor_property('initially_unlocked_skills'))})
        for skill in nodes:
            if skill.get_path_name() in skills:continue
            row={'path':skill.get_path_name(),'fields':{k:serial(skill.get_editor_property(k)) for k in keys}}
            montage=skill.get_editor_property('attack_montage')
            if montage:row['montage']={'path':montage.get_path_name(),'duration':montage.get_play_length(),
                'skeleton':serial(montage.get_editor_property('skeleton')),'rate_scale':montage.get_editor_property('rate_scale'),
                'tracks':tracks(montage),'blend_in':montage.get_editor_property('blend_in').get_editor_property('blend_time'),
                'blend_out':montage.get_editor_property('blend_out').get_editor_property('blend_time')}
            skills[skill.get_path_name()]=row
    return {'profiles':profiles,'keys':keys,'skills':list(skills.values())}


def write(name, value):
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    (EVIDENCE/name).write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')


def output_path(spec,kind):return FOLDER+'/'+kind+'_Fighter_'+spec['name']


def source_time(spec, time):
    if spec['name']=='IronChain_TwoPunch' and time>.273:
        return time+.372  # same right-fist phase, then final recovery instead of four extra strikes
    return spec['start']+time*spec['source_rate']


def options(mesh):
    value=unreal.AnimPoseEvaluationOptions()
    value.set_editor_property('optional_skeletal_mesh',mesh)
    value.set_editor_property('incorporate_root_motion_into_pose',True)
    value.set_editor_property('extract_root_motion',False)
    return value


def local_pose(source, time, settings, names):
    pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(source,time,settings)
    return {str(n):unreal.AnimPoseExtensions.get_bone_pose(pose,n,unreal.AnimPoseSpaces.LOCAL) for n in names}


def bake(spec, source, mesh, existing_target=None):
    target_path=output_path(spec,'A')
    if existing_target is None:
        if unreal.EditorAssetLibrary.does_asset_exist(target_path):raise RuntimeError('Refusing replacement '+target_path)
    elif spec['name']!='FlowingCloud_LeftCounter' or existing_target.get_path_name().split('.')[0]!=target_path:
        raise RuntimeError('Refusing repair outside this task FlowingCloud derivative')
    settings=options(mesh)
    initial=unreal.AnimPoseExtensions.get_anim_pose_at_time(source,source_time(spec,0.),settings)
    names=unreal.AnimPoseExtensions.get_bone_names(initial)
    keys={str(n):{'p':[],'q':[],'s':[]} for n in names}
    for index in range(spec['frames']+1):
        rows=local_pose(source,min(source.get_play_length(),source_time(spec,index/120.)),settings,names)
        for name, transform in rows.items():
            # In-place derivative; local limb transforms and rotations come from the evaluated source.
            keys[name]['p'].append(unreal.Vector(0.,0.,0.) if name=='root' else transform.translation)
            keys[name]['q'].append(transform.rotation)
            keys[name]['s'].append(transform.scale3d)
    target=existing_target or unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(),target_path)
    if not target:raise RuntimeError('Duplicate failed '+target_path)
    controller=target.get_editor_property('controller')
    if not controller:raise RuntimeError('Missing AnimDataController '+target_path)
    controller.open_bracket('Derived fighter animation, originals preserved',False)
    try:
        controller.remove_all_bone_tracks(False)
        for kind in [unreal.RawCurveTrackTypes.RCT_FLOAT,unreal.RawCurveTrackTypes.RCT_TRANSFORM]:
            controller.remove_all_curves_of_type(kind,False)
        controller.set_frame_rate(unreal.FrameRate(120,1),False)
        controller.set_number_of_frames(unreal.FrameNumber(spec['frames']),False)
        for name,row in keys.items():
            if not controller.add_bone_curve(name,False):raise RuntimeError('Add bone failed '+name)
            if not controller.set_bone_track_keys(name,row['p'],row['q'],row['s'],False):
                raise RuntimeError('Set bone keys failed '+name)
    finally:controller.close_bracket(False)
    # Verified UE 5.8 public API; clears duplicated events and sync markers.
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(target)
    if unreal.AnimationLibrary.get_animation_notify_events(target):raise RuntimeError('Stale notify events')
    target.set_editor_property('rate_scale',1.)
    target.set_editor_property('enable_root_motion',False)
    target.set_editor_property('force_root_lock',True)
    target.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    if abs(target.get_play_length()-spec['frames']/120.)>1e-5:raise RuntimeError('Wrong baked duration')
    save(target)
    return target


def save(asset):
    path=asset.get_path_name()
    allowed={s['skill']+'.'+s['skill'].rsplit('/',1)[1] for s in SPECS}
    if not path.startswith(FOLDER+'/') and path not in allowed:
        raise RuntimeError('Refusing save outside binding scope '+path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False):
        raise RuntimeError('Save failed '+path)


def validate_sequence(spec,sequence,mesh,api):
    data=api['sample'](sequence,mesh,spec['bone'],respect_root_lock=True)
    ratios=[x for r in data['limb_length_ref_ratios'].values() for x in r]
    if min(ratios)<.995 or max(ratios)>1.005:raise RuntimeError('Limb length failure '+spec['name'])
    if data['summary']['root']['max_displacement_from_start_cm']>.001:raise RuntimeError('Root travel '+spec['name'])
    if sequence.get_editor_property('enable_root_motion'):raise RuntimeError('Unexpected root motion')
    if abs(sequence.get_play_length()-spec['frames']/120.)>1e-5:raise RuntimeError('Wrong duration')
    if abs(source_time(spec,spec['contact'])-spec['source_contact'])>1e-5:
        raise RuntimeError('Contact mapping failure '+spec['name'])
    data['chosen_contact']={'montage_seconds':spec['contact'],'source_seconds':spec['source_contact'],
        'bone':spec['bone'],'method':'Reviewed source pose/reach cue, converted through the explicit time map; target collision is checked separately.'}
    return data


def create_montage(spec,sequence):
    path=output_path(spec,'AM')
    if unreal.EditorAssetLibrary.does_asset_exist(path):raise RuntimeError('Refusing replacement '+path)
    factory=unreal.AnimMontageFactory()
    factory.set_editor_property('source_animation',sequence)
    factory.set_editor_property('target_skeleton',sequence.get_editor_property('skeleton'))
    montage=unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],FOLDER,unreal.AnimMontage,factory)
    if not montage:raise RuntimeError('Montage creation failed '+path)
    for field,value in [('blend_in',spec['blend_in']),('blend_out',.12)]:
        blend=montage.get_editor_property(field);blend.set_editor_property('blend_time',value);montage.set_editor_property(field,blend)
    montage.set_editor_property('rate_scale',1.)
    save(montage)
    return montage


def apply(baseline,mesh,api):
    if capture()!=baseline:raise RuntimeError('DA baseline changed; re-inspect before applying')
    sources=[require(s['source'],unreal.AnimSequence) for s in SPECS]
    skeleton=mesh.get_editor_property('skeleton')
    for spec,source in zip(SPECS,sources):
        if source.get_editor_property('skeleton')!=skeleton:raise RuntimeError('Wrong source skeleton')
        rate=source.get_editor_property('platform_target_frame_rate').get_editor_property('default')
        compressed_frames=spec['frames']/120.*rate.numerator/rate.denominator
        if abs(compressed_frames-round(compressed_frames))>1e-5:
            raise RuntimeError('Length incompatible with inherited compression frame rate')
        require(spec['skill'],unreal.Rogue10mAttackSkillData)
        for kind in ['A','AM']:
            if unreal.EditorAssetLibrary.does_asset_exist(output_path(spec,kind)):
                raise RuntimeError('Output already exists; inspect actual state before resuming')
    settings=options(mesh)
    sequence=sources[1]
    pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence,.273,settings)
    names=unreal.AnimPoseExtensions.get_bone_names(pose)
    a=local_pose(sequence,.273,settings,names);b=local_pose(sequence,.645,settings,names)
    positions=[];angles=[]
    for name in a:
        x,y=api['transform'](a[name]),api['transform'](b[name])
        positions.append(math.dist(x['p'],y['p']));angles.append(api['angle'](x['q'],y['q']))
    seam={'position_cm':max(positions),'rotation_deg':max(angles),'bone_count':len(names)}
    if seam['position_cm']>.1 or seam['rotation_deg']>.05:raise RuntimeError('Two-punch join failed '+str(seam))
    write('two-punch-join.json',seam)
    created=[]
    for spec,source in zip(SPECS,sources):
        sequence=bake(spec,source,mesh)
        validate_sequence(spec,sequence,mesh,api)
        created.append(create_montage(spec,sequence))
    # Only bind after all three new sequence/montage pairs are created and validated.
    for spec,montage in zip(SPECS,created):
        skill=require(spec['skill'])
        skill.set_editor_property('attack_montage',montage)
        skill.set_editor_property('hit_start_delay_seconds',spec['contact'])
        save(skill)


def audit(baseline,mesh,api):
    actual=capture();errors=[];changes=[]
    if actual['profiles']!=baseline['profiles']:errors.append('Fighter profile contract changed')
    before={r['path']:r for r in baseline['skills']}
    paths={require(s['skill']).get_path_name():s for s in SPECS}
    for row in actual['skills']:
        old=before[row['path']];spec=paths.get(row['path'])
        changed=[k for k in actual['keys'] if row['fields'][k]!=old['fields'][k]]
        if spec:
            if sorted(changed)!=['attack_montage','hit_start_delay_seconds']:
                errors.append('Unauthorized DA change '+row['path']+str(changed))
            expected=require(output_path(spec,'AM'),unreal.AnimMontage)
            if row['fields']['attack_montage']!=expected.get_path_name():errors.append('Wrong montage '+spec['name'])
            if abs(row['fields']['hit_start_delay_seconds']-spec['contact'])>1e-6:errors.append('Wrong contact delay')
            if spec['name']=='FlowingCloud_LeftCounter' and spec['contact']>=row['fields']['combo_window_open_seconds']:
                errors.append('Contact after combo open')
            changes.append({'skill':row['fields']['skill_name'],'path':row['path'],'changed_fields':changed,
                'before_montage':old['fields']['attack_montage'],'after_montage':row['fields']['attack_montage'],
                'before_contact':old['fields']['hit_start_delay_seconds'],'after_contact':row['fields']['hit_start_delay_seconds']})
        elif row!=old:errors.append('Unchanged node modified '+row['path'])
        montage=require(row['fields']['attack_montage']) if row['fields']['attack_montage'] else None
        if montage:
            if montage.get_editor_property('skeleton')!=mesh.get_editor_property('skeleton'):errors.append('Montage skeleton mismatch')
            ts=tracks(montage)
            if len(ts)!=1 or ts[0]['slot']!='DefaultSlot' or len(ts[0]['segments'])!=1:errors.append('Wrong montage track')
            segment=ts[0]['segments'][0]
            sequence=require(segment['anim_reference'],unreal.AnimSequence)
            if sequence.get_editor_property('skeleton')!=mesh.get_editor_property('skeleton'):errors.append('Clip skeleton mismatch')
            if montage.get_play_length()<=0:errors.append('Empty montage')
            if spec and (abs(montage.get_play_length()-spec['frames']/120.)>1e-5 or segment['anim_play_rate']!=1. or segment['looping_count']!=1):
                errors.append('Wrong new montage duration/rate')
    poses={s['name']:validate_sequence(s,require(output_path(s,'A')),mesh,api) for s in SPECS}
    result={'result':'FAIL' if errors else 'PASS','errors':errors,'nodes':len(actual['skills']),
        'profile_contract_unchanged':actual['profiles']==baseline['profiles'],'changed_skills':changes,
        'protected_fields':'Every exposed DA field except the three AttackMontage/HitStartDelaySeconds pairs; all other nodes and profile fields identical.',
        'limits':'Structural/timing audit. Render, camera and actual target damage require separate runtime evidence. JumpSlam source remains unavailable; passive GoldenBody has no attack montage.'}
    write('bound-data.json',actual);write('bound-poses.json',poses);write('binding-validation.json',result)
    if errors:raise RuntimeError('Binding audit failed '+str(errors))
    unreal.log('RESULT=FIGHTER_ANIMATION_BINDING_PASSED nodes='+str(len(actual['skills']))+' changed='+str(len(changes)))


def adjust_flow(mesh,api):
    # One explicitly scoped repair of our own generated output, after inspecting
    # the first runtime attempt. Does not regenerate any preserved animation.
    prior=json.loads((EVIDENCE/'bound-data.json').read_text(encoding='utf-8'))
    current=capture()
    if current!=prior:raise RuntimeError('Stored binding state changed; inspect before adjustment')
    if (EVIDENCE/'flow-counter-adjustment.json').exists():raise RuntimeError('Already adjusted; run read-only audit')
    spec=SPECS[0];skill=require(spec['skill']);old=skill.get_editor_property('hit_start_delay_seconds')
    if abs(old-10/120.)>1e-6:raise RuntimeError('Unexpected previous contact time')
    write('initial-bound-data.json',prior)
    sequence=bake(spec,require(spec['source']),mesh,existing_target=require(output_path(spec,'A')))
    validate_sequence(spec,sequence,mesh,api)
    skill.set_editor_property('hit_start_delay_seconds',spec['contact']);save(skill)
    write('flow-counter-adjustment.json',{'before':old,'after':spec['contact'],
        'reason':'50 ms margin before unchanged 100 ms combo open, including 30 Hz timer quantization.',
        'preserved':'All prior nine animations and every balance/combination field remain identical.'})


def main():
    if ROOT!=EXPECTED:raise RuntimeError('Unexpected project')
    baseline=json.loads((EVIDENCE/'baseline-data.json').read_text(encoding='utf-8'))
    api=runpy.run_path(str(ROOT/'Scripts/Editor/PrepareCloseCombatPreview.py'),run_name='readonly_pose_helpers')
    mesh=require(MANNY)
    switches=unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())[1]
    if switches.count('RogueBindFighterAnimations'):
        apply(baseline,mesh,api)
    elif switches.count('RogueAdjustFlowCounter'):
        adjust_flow(mesh,api)
    audit(baseline,mesh,api)


if __name__=='__main__':
    try:main()
    finally:unreal.SystemLibrary.quit_editor()
