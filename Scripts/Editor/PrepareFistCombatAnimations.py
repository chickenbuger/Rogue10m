"""Prepare nine dedicated fist animations; validate before saving or reconnecting gameplay assets.
Run with UnrealEditor-Cmd -ExecutePythonScript=... after building the editor module.
"""
import hashlib
import json
import math
import re
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir())
EVIDENCE=ROOT/'Feature/doc/evidence/fist-combat-20260929'
FOLDER='/Game/Rogue10m/Animation/Combat/FistAuthored'
SOURCE='/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingLeftJab_Appearance'
BRAWLER='/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_'
KNUCKLE='/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_'
SPECS=[('Unarmed_LeftJab','left_jab',BRAWLER+'LeftJab',None),
       ('Unarmed_RightJab','right_jab',BRAWLER+'RightJab',None),
       ('Unarmed_Straight','straight',BRAWLER+'RightStraight',None),
       ('Unarmed_Hook','hook',BRAWLER+'RightHook',None),
       ('Knuckle_Jab','right_jab',KNUCKLE+'Primary01',0.15),
       ('Knuckle_BodyStraight','body',KNUCKLE+'Primary02',0.16),
       ('Knuckle_HookFlurry','flurry',KNUCKLE+'Primary03',0.18),
       ('Knuckle_Uppercut','uppercut',KNUCKLE+'Special',0.21),
       ('Knuckle_ShortBlow','heavy',KNUCKLE+'Charged',0.22)]
# Delay is deliberately the only timing exception; gameplay rates, damage and progression stay unchanged.
PROTECTED=['animation_play_rate','attack_cooldown','charge_seconds','hit_mode','hit_count','hit_interval',
 'max_hits_per_target','max_targets_per_hit','damage','min_damage_ratio_multiplier','max_damage_ratio_multiplier',
 'critical_chance_bonus','critical_damage_multiplier_bonus','resource_costs','attack_range','attack_shape',
 'attack_trace_radius','box_half_width','box_half_height','arc_angle_degrees','circle_forward_offset','enable_combo',
 'combo_window_open_seconds','combo_window_close_seconds','next_combo_skill','prerequisite_skills',
 'unlock_conditions','input_slot','effect_attach_socket','combo_input_slot','skill_tree_node_type','skill_tree_tier','skill_tree_position']
LIMBS=[('upperarm_l','lowerarm_l'),('lowerarm_l','hand_l'),('upperarm_r','lowerarm_r'),('lowerarm_r','hand_r'),
       ('thigh_l','calf_l'),('calf_l','foot_l'),('thigh_r','calf_r'),('calf_r','foot_r')]
STANCE=['root','pelvis','thigh_l','calf_l','foot_l','ball_l','thigh_r','calf_r','foot_r','ball_r']
IK=[('ik_hand_gun','hand_r'),('ik_hand_l','hand_l'),('ik_hand_r','hand_r'),('ik_foot_l','foot_l'),('ik_foot_r','foot_r')]


def require(path):
    value=unreal.load_asset(path)
    if value is None: raise RuntimeError('Missing asset '+path)
    return value


def duplicate(source,path):
    if unreal.EditorAssetLibrary.does_asset_exist(path): return require(path)
    result=unreal.EditorAssetLibrary.duplicate_asset(source,path)
    if result is None: raise RuntimeError('Duplicate failed '+path)
    return result


def serial(value):
    if value is None or isinstance(value,(str,int,float,bool)): return value
    if isinstance(value,unreal.Object): return value.get_path_name()
    if isinstance(value,(list,tuple,unreal.Array)): return [serial(x) for x in value]
    # Struct export text includes memory addresses; keep actual values for equality and reporting separately.
    return str(value)


def snapshot(skill):
    output={}
    for name in PROTECTED:
        try: output[name]=skill.get_editor_property(name)
        except Exception as exc: raise RuntimeError('Protected property unavailable: '+name) from exc
    return output


def dist(a,b): return math.sqrt(sum((getattr(a,k)-getattr(b,k))**2 for k in ['x','y','z']))
def angle(a,b):
    norm=math.sqrt(sum(getattr(a,k)**2 for k in ['x','y','z','w'])*sum(getattr(b,k)**2 for k in ['x','y','z','w']))
    dot=abs(sum(getattr(a,k)*getattr(b,k) for k in ['x','y','z','w']))/norm
    return math.degrees(2*math.acos(min(1.0,dot)))
def bone(p,n,space=unreal.AnimPoseSpaces.WORLD): return unreal.AnimPoseExtensions.get_bone_pose(p,n,space)
def pose(s,t,options): return unreal.AnimPoseExtensions.get_anim_pose_at_time(s,t,options)


def validate(row,options,guard):
    sequence=row['sequence']; length=sequence.get_play_length()
    blend_in=row['montage'].get_editor_property('blend_in').get_editor_property('blend_time')
    if blend_in>row['contacts'][0]/row['rate']+.001:
        raise RuntimeError(row['name']+' montage blend would hide its first contact')
    contacts=row['contacts']; style=row['style']
    ref_lengths={b:dist(bone(guard,a).translation,bone(guard,b).translation) for a,b in LIMBS}
    names=unreal.AnimPoseExtensions.get_bone_names(guard)
    errors={'bone_length_cm':0.,'render_local_translation_cm':0.,'render_local_scale':0.,'stance_position_cm':0.,
            'stance_rotation_deg':0.,'ik_position_cm':0.,'ik_rotation_deg':0.,'head_travel_cm':0.,'head_rotation_deg':0.}
    samples=[]
    previous_arm=None
    motion={'max_elbow_step_cm':0.,'max_arm_rotation_step_deg':0.,'boundary_elbow_step_cm':0.,'boundary_arm_rotation_step_deg':0.}
    arm_names=['upperarm_l','lowerarm_l','hand_l','upperarm_r','lowerarm_r','hand_r']
    for frame in range(round(length*120)+1):
        t=frame/120.; p=pose(sequence,t,options)
        for name in names:
            if str(name).startswith('ik_'): continue
            a,b=bone(p,name,unreal.AnimPoseSpaces.LOCAL),bone(guard,name,unreal.AnimPoseSpaces.LOCAL)
            errors['render_local_translation_cm']=max(errors['render_local_translation_cm'],dist(a.translation,b.translation))
            errors['render_local_scale']=max(errors['render_local_scale'],dist(a.scale3d,b.scale3d))
        for a,b in LIMBS:
            errors['bone_length_cm']=max(errors['bone_length_cm'],abs(dist(bone(p,a).translation,bone(p,b).translation)-ref_lengths[b]))
        for name in STANCE:
            a,b=bone(p,name),bone(guard,name)
            errors['stance_position_cm']=max(errors['stance_position_cm'],dist(a.translation,b.translation))
            errors['stance_rotation_deg']=max(errors['stance_rotation_deg'],angle(a.rotation,b.rotation))
        for a,b in IK:
            x,y=bone(p,a),bone(p,b)
            errors['ik_position_cm']=max(errors['ik_position_cm'],dist(x.translation,y.translation))
            errors['ik_rotation_deg']=max(errors['ik_rotation_deg'],angle(x.rotation,y.rotation))
        a,b=bone(p,'head'),bone(guard,'head')
        errors['head_travel_cm']=max(errors['head_travel_cm'],dist(a.translation,b.translation))
        errors['head_rotation_deg']=max(errors['head_rotation_deg'],angle(a.rotation,b.rotation))
        current_arm={n:bone(p,n) for n in arm_names}
        if previous_arm:
            for n in arm_names:
                motion['max_arm_rotation_step_deg']=max(motion['max_arm_rotation_step_deg'],angle(current_arm[n].rotation,previous_arm[n].rotation))
                if n.startswith('lowerarm'):
                    motion['max_elbow_step_cm']=max(motion['max_elbow_step_cm'],dist(current_arm[n].translation,previous_arm[n].translation))
        previous_arm=current_arm
        samples.append({'time':t,'left':serial_vector(bone(p,'hand_l').translation),'right':serial_vector(bone(p,'hand_r').translation)})
    for key,value in errors.items():
        limit=5. if key=='head_travel_cm' else .02
        if not math.isfinite(value) or value>limit: raise RuntimeError(row['name']+' invalid '+key+'='+str(value))
    # Both gates are required: the native solver checks A -> 0 for every actual
    # chest pose (including opposite-arm overlap); this checks the baked boundary keys.
    boundary_checks=[]
    for index,contact in enumerate(contacts):
        left=style=='left_jab' or style=='flurry' and index%2==0
        side='l' if left else 'r'
        attack=row['spacing']*.82 if style=='flurry' else min(contacts[0],.32 if style=='straight' else .28)
        recover=row['spacing']*.90 if style=='flurry' else min(length-contact,.46 if style=='straight' else .35)
        for label,boundary in [('start',contact-attack),('end',contact+recover)]:
            lo=max(0.,math.floor((boundary-1e-7)*120)/120.)
            hi=min(length,math.ceil((boundary+1e-7)*120)/120.)
            before,after=pose(sequence,lo,options),pose(sequence,hi,options)
            elbow=dist(bone(before,'lowerarm_'+side).translation,bone(after,'lowerarm_'+side).translation)
            rotation=max(angle(bone(before,n+'_'+side).rotation,bone(after,n+'_'+side).rotation) for n in ['upperarm','lowerarm','hand'])
            motion['boundary_elbow_step_cm']=max(motion['boundary_elbow_step_cm'],elbow)
            motion['boundary_arm_rotation_step_deg']=max(motion['boundary_arm_rotation_step_deg'],rotation)
            boundary_checks.append({'contact_index':index,'boundary':label,'times':[lo,hi],'elbow_step_cm':elbow,'arm_step_deg':rotation})
    if motion['boundary_elbow_step_cm']>.6 or motion['boundary_arm_rotation_step_deg']>2.5:
        raise RuntimeError(row['name']+' activation boundary discontinuity '+str(motion))
    if motion['max_elbow_step_cm']>8. or motion['max_arm_rotation_step_deg']>30.:
        raise RuntimeError(row['name']+' excessive 120Hz arm step '+str(motion))
    checks=[]
    for index,contact in enumerate(contacts):
        left=style=='left_jab' or style=='flurry' and index%2==0
        hand='hand_l' if left else 'hand_r'; shoulder='upperarm_l' if left else 'upperarm_r'
        p=pose(sequence,contact,options); h,s=bone(p,hand).translation,bone(p,shoulder).translation
        # Manny mesh forward is +Y. Evaluate the rendered contact, not merely the planned IK target.
        forward=h.y-s.y; before=pose(sequence,max(0.,contact-(row['spacing']*.8 if style=='flurry' else .12)),options)
        before_h=bone(before,hand).translation
        excursion=dist(h,before_h)
        if forward<25. or excursion<5.: raise RuntimeError(row['name']+' contact is not a visible forward strike '+str((forward,excursion)))
        if style=='uppercut' and h.z-before_h.z<8.: raise RuntimeError('Uppercut does not rise at contact')
        if style=='body' and h.z-s.z>-15.: raise RuntimeError('Body straight does not target torso height')
        if style in ['right_jab','straight','hook','flurry'] and h.z-s.z<3.:
            raise RuntimeError(row['name']+' intended forward strike is below shoulder height')
        if style=='heavy' and h.z-s.z<0.:
            raise RuntimeError('Short heavy blow is unintentionally downward')
        checks.append({'index':index,'hand':hand,'montage_seconds':contact,'real_seconds_at_speed1':contact/row['rate'],
                       'forward_from_shoulder_cm':forward,'height_from_shoulder_cm':h.z-s.z,
                       'forward_from_head_cm':h.y-bone(p,'head').translation.y,'height_from_head_cm':h.z-bone(p,'head').translation.z,
                       'approach_travel_cm':excursion,'hand_position':serial_vector(h)})
    return {'name':row['name'],'style':style,'duration':length,'blend_in_seconds':blend_in,'frames':len(samples),'errors':errors,'contacts':checks,'motion_continuity':motion,'activation_boundaries':boundary_checks,'samples':samples}


def serial_vector(v): return [v.x,v.y,v.z]


def source_hashes():
    paths=[ROOT/'Content/Rogue10m/Animation/Combat/Boxing',ROOT/'Content/Characters/Mannequins/Anims/Unarmed',
           ROOT/'Content/Rogue10m/Animation/Common',ROOT/'Content/Rogue10m/Animation/Preview/Boxing']
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in paths for p in folder.rglob('*.uasset')}


def main():
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    originals=source_hashes(); rows=[]; outputs=[]; lines=[]
    for name,style,path,new_delay in SPECS:
        skill=require(path); before=snapshot(skill); original=skill.get_editor_property('attack_montage')
        length=original.get_play_length(); rate=skill.get_editor_property('animation_play_rate')
        delay=skill.get_editor_property('hit_start_delay_seconds') if new_delay is None else new_delay
        count=skill.get_editor_property('hit_count'); spacing=skill.get_editor_property('hit_interval')*rate
        if count!=(6 if style=='flurry' else 1): raise RuntimeError('Unreviewed hit count '+path)
        sequence=duplicate(SOURCE,FOLDER+'/A_'+name)
        montage=duplicate(original.get_path_name().split('.')[0],FOLDER+'/AM_'+name)
        rows.append({'name':name,'style':style,'skill':skill,'before':before,'sequence':sequence,'montage':montage,
                     'old_delay':skill.get_editor_property('hit_start_delay_seconds'),'delay':delay,'rate':rate,'spacing':spacing,
                     'contacts':[delay+i*spacing for i in range(count)]})
        outputs.extend([sequence,montage]); lines.append('|'.join(map(str,[name,style,length,delay,spacing,count])))
    (EVIDENCE/'authoring-requests.txt').write_text('\n'.join(lines),encoding='utf-8')
    result=EVIDENCE/'native-authoring.txt'; result.write_text('PENDING',encoding='utf-8')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'Rogue10m.AuthorFistCombat')
    native_report=result.read_text(encoding='utf-8-sig')
    if 'RESULT=FIST_COMBAT_AUTHORING_PASSED' not in native_report: raise RuntimeError(native_report)
    guard_limits=[{'elbow_cm':float(a),'rotation_deg':float(b)} for a,b in re.findall(r'continuity_elbow_cm=([0-9.]+) continuity_rotation_deg=([0-9.]+)',native_report)]
    if len(guard_limits)!=9 or any(x['elbow_cm']>=.01 or x['rotation_deg']>=.01 for x in guard_limits):
        raise RuntimeError('Native Activity=1e-6 guard-limit checks missing or failed')
    options=unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh',require('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'))
    guard=pose(require(SOURCE),0.,options)
    checks=[validate(row,options,guard) for row in rows]
    jab=next(c for c in checks if c['name']=='Unarmed_RightJab')
    straight=next(c for c in checks if c['name']=='Unarmed_Straight')
    distances=[math.dist(a['right'],b['right']) for a,b in zip(jab['samples'],straight['samples'])]
    distinction={'jab_straight_hand_max_distance_cm':max(distances),'jab_straight_hand_rms_distance_cm':math.sqrt(sum(d*d for d in distances)/len(distances))}
    if distinction['jab_straight_hand_max_distance_cm']<3. or distinction['jab_straight_hand_rms_distance_cm']<1.:
        raise RuntimeError('Right jab and straight are not distinct motions: '+str(distinction))
    if originals!=source_hashes(): raise RuntimeError('Original animation packages changed')
    for row in rows:
        skill=row['skill']; skill.set_editor_property('attack_montage',row['montage']); skill.set_editor_property('hit_start_delay_seconds',row['delay'])
        if row['style']=='uppercut':
            skill.set_editor_property('skill_name','승룡권')
            skill.set_editor_property('skill_description','몸 가까이에서 오른 주먹을 올려치는 짧은 어퍼컷입니다.')
        if row['style']=='heavy':
            skill.set_editor_property('skill_name','축경권')
            skill.set_editor_property('skill_description','힘을 모은 뒤 짧고 묵직한 오른 주먹을 내지릅니다.')
        if row['style']=='flurry':
            skill.set_editor_property('skill_description','근거리에서 좌우 짧은 훅을 여섯 번 연속으로 적중시킵니다.')
        after=snapshot(skill)
        for key,value in row['before'].items():
            if after[key]!=value: raise RuntimeError('Gameplay contract changed '+row['name']+' '+key)
    for asset in outputs+[r['skill'] for r in rows]:
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False): raise RuntimeError('Cannot save '+asset.get_path_name())
    report={'result':'PASS','source':SOURCE,'original_hashes_unchanged':originals==source_hashes(),'original_package_sha256':originals,
            'scope':'Four active BasicBrawler attacks and five actual progression Knuckle skill assets. Other StoneFist skills are unchanged.',
            'timing':'first delay / play rate; subsequent hit interval unaffected by play rate; contacts baked accordingly',
            'skills':[{'name':r['name'],'path':r['skill'].get_path_name(),'montage':r['montage'].get_path_name(),
                       'old_delay':r['old_delay'],'new_delay':r['delay'],'protected_fields':{k:serial(v) for k,v in r['before'].items()}} for r in rows],
            'motion_distinction':distinction,'guard_limit_activity':1e-6,'guard_limits':guard_limits,'checks':checks}
    (EVIDENCE/'authored-animation-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('RESULT=FIST_COMBAT_PREPARE_PASSED')


if __name__=='__main__':
    try: main()
    finally: unreal.SystemLibrary.quit_editor()
