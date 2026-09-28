"""Offline A/B retarget into diagnostic assets only. Production assets are never saved or edited.
The comparison does not include the runtime Common ABP's Control Rig or montage blending.
"""
from pathlib import Path
import hashlib
import json
import math
import unreal

PROJECT=Path(unreal.Paths.project_dir())
DEST='/Game/Developers/Codex/AppearanceRetargetAudit'
RIG='/Game/Character/Customization/Retargeting/IKR_Manny_To_Hu_M'
SOURCE_MESH='/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
TARGET_MESH='/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody'
CLIPS=('/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingJab_Manny',
       '/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingLeftJab_Appearance')
VARIANTS=('current','root_bones','no_root_op','no_ik','root_and_unmap_accessories','only_align','root_and_align')
BONES=('root','pelvis','spine_01','spine_02','spine_03','neck_01','head','upperarm_l','lowerarm_l','hand_l',
       'upperarm_r','lowerarm_r','hand_r','thigh_l','calf_l','foot_l','ball_l','thigh_r','calf_r','foot_r','ball_r')
PAIRS=(('upperarm_l','lowerarm_l'),('lowerarm_l','hand_l'),('upperarm_r','lowerarm_r'),('lowerarm_r','hand_r'),
       ('thigh_l','calf_l'),('calf_l','foot_l'),('thigh_r','calf_r'),('calf_r','foot_r'))

def require(path):
    asset=unreal.load_asset(path)
    if not asset: raise RuntimeError('Missing '+path)
    return asset

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def file_for(asset): return PROJECT/'Content'/Path(asset.removeprefix('/Game/')+'.uasset')
def angle(a,b):
    dot=abs(sum(x*y for x,y in zip(a,b)))
    denom=math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1,max(0,dot/denom))))
def tr(t):
    p,q=t.translation,t.rotation
    return {'p':[p.x,p.y,p.z],'q':[q.x,q.y,q.z,q.w]}

def sample(sequence,mesh):
    options=unreal.AnimPoseEvaluationOptions(); options.optional_skeletal_mesh=mesh
    first=unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence,0,options)
    available={str(n).lower():str(n) for n in unreal.AnimPoseExtensions.get_bone_names(first)}
    names={key:available[key] for key in BONES if key in available}
    reference={bone:tr(unreal.AnimPoseExtensions.get_ref_bone_pose(first,name,unreal.AnimPoseSpaces.WORLD)) for bone,name in names.items()}
    duration=sequence.get_play_length(); fps=120
    frames=[]
    for index in range(round(duration*fps)+1):
        time=min(index/fps,duration)
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence,time,options)
        frames.append({'time':time,'bones':{bone:tr(unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.WORLD)) for bone,name in names.items()}})
    summary={bone:{'max_ref_angle':max(angle(f['bones'][bone]['q'],reference[bone]['q']) for f in frames),
        'max_first_angle':max(angle(f['bones'][bone]['q'],frames[0]['bones'][bone]['q']) for f in frames),
        'max_step_angle':max([angle(a['bones'][bone]['q'],b['bones'][bone]['q']) for a,b in zip(frames,frames[1:])] or [0]),
        'first_position':frames[0]['bones'][bone]['p'],
        'max_first_distance':max(math.dist(f['bones'][bone]['p'],frames[0]['bones'][bone]['p']) for f in frames)} for bone in names}
    limbs={}
    for a,b in PAIRS:
        if a not in names or b not in names: continue
        base=math.dist(reference[a]['p'],reference[b]['p'])
        lengths=[math.dist(f['bones'][a]['p'],f['bones'][b]['p']) for f in frames]
        limbs[a+'->'+b]={'min_ratio':min(lengths)/base,'max_ratio':max(lengths)/base}
    return {'asset':sequence.get_path_name(),'summary':summary,'limbs':limbs,'frames':frames,'reference':reference}

def main():
    production=[RIG,SOURCE_MESH,TARGET_MESH,*CLIPS]
    before={path:sha(file_for(path)) for path in production}
    backup=PROJECT/'tmp/appearance-retarget/before-ab'
    backup.mkdir(parents=True,exist_ok=True)
    for path in production:
        source=file_for(path)
        target=backup/(source.name+'.backup')
        if not target.exists(): target.write_bytes(source.read_bytes())
    rig=require(RIG); source=require(SOURCE_MESH); target=require(TARGET_MESH)
    report={'production_before_hashes':before,'variants':{},'source':{},'diagnostic_folder':DEST,
            'limitations':['Offline retarget only; source Common ABP Control Rig, montage blend and camera are excluded.']}
    for clip in CLIPS: report['source'][clip]=sample(require(clip),source)
    for variant in VARIANTS:
        # AssetTools duplicates into a separate unsaved diagnostic package. No mutation of the original retargeter.
        name='IKR_Audit_'+variant
        duplicate=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name,DEST,rig)
        if not duplicate: raise RuntimeError('Could not duplicate diagnostic retargeter '+variant)
        controller=unreal.IKRetargeterController.get_controller(duplicate)
        root_index=next(i for i in range(controller.get_num_retarget_ops()) if 'Root Motion' in str(controller.get_op_name(i)))
        if variant in ('root_bones','root_and_unmap_accessories','root_and_align'):
            root=controller.get_op_controller(root_index)
            root.set_source_root_bone('root'); root.set_target_root_bone('Root')
        elif variant=='no_root_op': controller.set_retarget_op_enabled(root_index,False)
        elif variant=='no_ik':
            for i in range(controller.get_num_retarget_ops()):
                if 'Run IK Rig' in str(controller.get_op_name(i)): controller.set_retarget_op_enabled(i,False)
        if variant=='root_and_unmap_accessories':
            for name in ('Tail','Cape','Tabard_Back','Tabard_Front'): controller.set_source_chain('None',name)
        if variant in ('only_align','root_and_align'):
            controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
        report['variants'][variant]={}
        for clip in CLIPS:
            sequence=require(clip)
            output_name=sequence.get_name()+'_'+variant
            inputs=unreal.IKRetargetBatchOperationInputs()
            inputs.assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(clip)]
            inputs.source_mesh=source; inputs.target_mesh=target; inputs.ik_retarget_asset=duplicate
            inputs.search=sequence.get_name(); inputs.replace=output_name
            inputs.target_path=DEST; inputs.use_source_path=False
            inputs.include_referenced_assets=False; inputs.overwrite_existing_files=False
            results=unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
            if len(results)!=1: raise RuntimeError('Expected exactly one diagnostic clip')
            output=results[0].get_asset()
            report['variants'][variant][clip]=sample(output,target)
            unreal.log(f'APPEARANCE_RETARGET_AB variant={variant} clip={clip} head='+str(report['variants'][variant][clip]['summary']['head']))
    report['production_after_hashes']={path:sha(file_for(path)) for path in production}
    if before!=report['production_after_hashes']: raise RuntimeError('Production asset bytes changed during diagnostic run')
    output=PROJECT/'tmp/appearance-retarget/retarget-ab.json'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log(f'RESULT=APPEARANCE_RETARGET_AB_COMPLETE output={output}')

if __name__=='__main__': main()
