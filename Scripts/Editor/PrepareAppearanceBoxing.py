"""Connect the approved Boxing source to the ordinary Appearance Mesh montage path.
Run after building the editor. Only two dedicated jab DAs and four dedicated animation assets are saved.
"""
from pathlib import Path
import json
import math
import unreal

ROOT = Path(unreal.Paths.project_dir())
FOLDER = '/Game/Rogue10m/Animation/Combat/Boxing'
SOURCE = FOLDER + '/A_BoxingJab_Manny'
COMMON = '/Game/Rogue10m/Animation/Common'
DATA = '/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_'
EVIDENCE = ROOT / 'Feature/doc/evidence/animation-stability-20260928'
RESULT = ROOT / 'tmp/appearance-head/boxing-authoring.txt'
# Gameplay timing and balance must remain identical when changing the animation reference.
FIELDS = ['animation_play_rate', 'hit_start_delay_seconds', 'attack_cooldown', 'damage',
          'attack_range', 'enable_combo', 'combo_window_open_seconds', 'combo_window_close_seconds',
          'next_combo_skill', 'hit_count', 'hit_interval', 'max_hits_per_target', 'charge_seconds']


def require(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError('Missing asset: ' + path)
    return asset


def duplicate(source, destination):
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        return require(destination)
    asset = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    if asset is None:
        raise RuntimeError('Cannot duplicate: ' + destination)
    return asset


def validate_right_stance(options, hit_time):
    left = require(FOLDER + '/A_BoxingLeftJab_Appearance')
    right = require(FOLDER + '/A_BoxingRightJab_Appearance')
    stance = ['root','pelvis','thigh_l','calf_l','foot_l','ball_l','thigh_r','calf_r','foot_r','ball_r','ik_foot_root','ik_foot_l','ik_foot_r','ik_hand_root']
    max_pos = max_rot = max_length = max_hand_ik_pos = max_hand_ik_rot = 0.0
    samples = []
    def dist(a,b): return math.sqrt(sum((getattr(a,k)-getattr(b,k))**2 for k in ['x','y','z']))
    def angle(a,b):
        dot=abs(sum(getattr(a,k)*getattr(b,k) for k in ['x','y','z','w']))
        norm=math.sqrt(sum(getattr(a,k)**2 for k in ['x','y','z','w'])*sum(getattr(b,k)**2 for k in ['x','y','z','w']))
        return math.degrees(2*math.acos(min(1.0,dot/norm)))
    def world(p,b): return unreal.AnimPoseExtensions.get_bone_pose(p,b,unreal.AnimPoseSpaces.WORLD)
    if abs(left.get_play_length()-right.get_play_length())>0.0001: raise RuntimeError('Jab duration mismatch')
    for frame in range(round(left.get_play_length()*120)+1):
        t=frame/120.0
        a,b=[unreal.AnimPoseExtensions.get_anim_pose_at_time(s,t,options) for s in [left,right]]
        for bone in stance:
            for space in [unreal.AnimPoseSpaces.LOCAL,unreal.AnimPoseSpaces.WORLD]:
                x,y=[unreal.AnimPoseExtensions.get_bone_pose(p,bone,space) for p in [a,b]]
                max_pos=max(max_pos,dist(x.translation,y.translation));max_rot=max(max_rot,angle(x.rotation,y.rotation))
        for parent,child in [('upperarm_l','lowerarm_l'),('lowerarm_l','hand_l'),('upperarm_r','lowerarm_r'),('lowerarm_r','hand_r')]:
            refs=[unreal.AnimPoseExtensions.get_ref_bone_pose(b,n,unreal.AnimPoseSpaces.WORLD) for n in [parent,child]]
            max_length=max(max_length,abs(dist(world(b,parent).translation,world(b,child).translation)-dist(refs[0].translation,refs[1].translation)))
        for ik,hand in [('ik_hand_gun','hand_r'),('ik_hand_l','hand_l'),('ik_hand_r','hand_r')]:
            x,y=[world(b,name) for name in [ik,hand]]
            max_hand_ik_pos=max(max_hand_ik_pos,dist(x.translation,y.translation))
            max_hand_ik_rot=max(max_hand_ik_rot,angle(x.rotation,y.rotation))
        if frame==0:
            heads=[world(p,'head') for p in [a,b]]
            head_pos=dist(heads[0].translation,heads[1].translation);head_rot=angle(heads[0].rotation,heads[1].rotation)
            l,r=[world(b,n).translation for n in ['upperarm_l','upperarm_r']]
            dx,dy=l.x-r.x,l.y-r.y;n=math.hypot(dx,dy);forward=[-dy/n,dx/n,0.0]
        hand,shoulder=[world(b,n).translation for n in ['hand_r','upperarm_r']]
        delta=[hand.x-shoulder.x,hand.y-shoulder.y,hand.z-shoulder.z]
        samples.append({'time':t,'forward_cm':sum(x*y for x,y in zip(delta,forward))})
    peak=max(samples,key=lambda x:x['forward_cm']);at_hit=min(samples,key=lambda x:abs(x['time']-hit_time))
    excursion=peak['forward_cm']-min(x['forward_cm'] for x in samples)
    if max_pos>0.01 or max_rot>0.01: raise RuntimeError('Right jab stance changed: '+str((max_pos,max_rot)))
    if head_pos>0.01 or head_rot>0.01 or max_length>0.01: raise RuntimeError('Right central starting pose or arm length failed')
    if max_hand_ik_pos>0.01 or max_hand_ik_rot>0.01: raise RuntimeError('Hand IK disagrees with final hand pose: '+str((max_hand_ik_pos,max_hand_ik_rot)))
    if excursion<15 or at_hit['forward_cm']<10 or at_hit['forward_cm']<peak['forward_cm']*0.70:
        raise RuntimeError('Right jab forward hit failed: '+str((excursion,peak,at_hit)))
    return {'frames':len(samples),'stance_max_position_error_cm':max_pos,'stance_max_rotation_error_deg':max_rot,
            'start_head_position_error_cm':head_pos,'start_head_rotation_error_deg':head_rot,
            'hand_ik_position_error_cm':max_hand_ik_pos,'hand_ik_rotation_error_deg':max_hand_ik_rot,
            'max_arm_segment_length_error_cm':max_length,'right_forward_excursion_cm':excursion,
            'right_forward_peak':peak,'right_forward_at_hit':at_hit,'samples':samples}


def main():
    source = require(SOURCE)
    skills = {}
    previous = {}
    outputs = []
    for side, index in [('Left', '01'), ('Right', '02')]:
        skill = require(DATA + side + 'Jab')
        skills[side] = skill
        previous[side] = {field: skill.get_editor_property(field) for field in FIELDS}
        sequence = duplicate(SOURCE, FOLDER + '/A_Boxing' + side + 'Jab_Appearance')
        montage = duplicate(COMMON + '/AM_Punch_' + index, FOLDER + '/AM_Boxing' + side + 'Jab_Appearance')
        if sequence.get_editor_property('skeleton') != source.get_editor_property('skeleton'):
            raise RuntimeError('Wrong output skeleton')
        outputs.extend([sequence, montage])
    RESULT.parent.mkdir(parents=True, exist_ok=True)
    RESULT.write_text('PENDING', encoding='utf-8')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'Rogue10m.AuthorAppearanceBoxing')
    result = RESULT.read_text(encoding='utf-8-sig')
    if 'RESULT=APPEARANCE_BOXING_AUTHORING_PASSED' not in result:
        raise RuntimeError('Native pose bake failed: ' + result)
    checks = []
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh', require('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'))
    left = require(FOLDER + '/A_BoxingLeftJab_Appearance')
    length = left.get_play_length()
    hit = previous['Left']['hit_start_delay_seconds']
    source_hit = 13.0 / 30.0
    max_error = 0.0
    # Check authored bake keys before saving any generated asset or changing skill references.
    for frame in range(round(length * 120) + 1):
        t = frame / 120.0
        st = source_hit * t / hit if t <= hit else source_hit + (source.get_play_length() - source_hit) * (t - hit) / (length - hit)
        actual = unreal.AnimPoseExtensions.get_anim_pose_at_time(left, t, options)
        expected = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, min(st, source.get_play_length()), options)
        for bone in ['root', 'pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r']:
            a = unreal.AnimPoseExtensions.get_bone_pose(actual, bone, unreal.AnimPoseSpaces.WORLD)
            b = unreal.AnimPoseExtensions.get_bone_pose(expected, bone, unreal.AnimPoseSpaces.WORLD)
            max_error = max(max_error, math.sqrt(sum((getattr(a.translation, k)-getattr(b.translation, k))**2 for k in ['x','y','z'])))
    if max_error > 0.15:
        raise RuntimeError('Baked left jab source-pose discrepancy: ' + str(max_error))
    stance_validation = validate_right_stance(options, previous['Right']['hit_start_delay_seconds'])
    for asset in outputs:
        unreal.EditorAssetLibrary.set_metadata_tag(asset, 'Rogue10m.Source', SOURCE)
        unreal.EditorAssetLibrary.set_metadata_tag(asset, 'Rogue10m.Usage',
            'Appearance Mesh Boxing; left original; right mirrored arms and rebased torso; source lower-body stance; final-pose hand IK; hit-aligned timing')
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            raise RuntimeError('Cannot save: ' + asset.get_path_name())
    for side in ['Left', 'Right']:
        skill = skills[side]
        montage = require(FOLDER + '/AM_Boxing' + side + 'Jab_Appearance')
        skill.set_editor_property('attack_montage', montage)
        for field, value in previous[side].items():
            if skill.get_editor_property(field) != value:
                raise RuntimeError('Gameplay field changed: ' + side + '/' + field)
        if not unreal.EditorAssetLibrary.save_loaded_asset(skill, False):
            raise RuntimeError('Cannot save jab skill')
        checks.append({'side': side, 'montage': montage.get_path_name(), 'duration': montage.get_play_length(),
                       'play_rate': previous[side]['animation_play_rate'], 'hit_delay': previous[side]['hit_start_delay_seconds']})
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / 'boxing-assets.json').write_text(json.dumps({'source': SOURCE, 'jabs': checks,
        'left_max_source_position_error_cm': max_error, 'right_stance_validation': stance_validation, 'authoring': result}, indent=2), encoding='utf-8')
    unreal.log('RESULT=APPEARANCE_BOXING_ASSETS_PASSED')


if __name__ == '__main__':
    try:
        main()
    finally:
        unreal.SystemLibrary.quit_editor()
