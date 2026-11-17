"""Import licensed Mixamo idle, retarget through Manny to existing Arin. No originals overwritten."""
from pathlib import Path
import hashlib, json, math, sys
import unreal
PROJECT=Path(unreal.Paths.project_dir()).resolve()
ROOT='/Game/Rogue10m/Characters/Arin/Standing20261110'
OUT=PROJECT/'DevLog/20261110_HumanoidBossNpc/NPC'
FBX=Path('C:/Users/PC/Downloads/Breathing Idle.fbx')
MANNY='/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
ARIN='/Game/Rogue10m/Characters/Arin/RefinedV3/SK_Arin'
DATA='/Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide'

def require(path):
    obj=unreal.load_asset(path)
    if not obj: raise RuntimeError('Missing '+path)
    return obj

def save(obj):
    if not obj.get_path_name().startswith(ROOT+'/'): raise RuntimeError('Outside owned scope')
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False): raise RuntimeError('Save failed')

def asset(name,cls,factory):
    path=ROOT+'/'+name
    return require(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT,cls,factory)

def rig(name,mesh):
    obj=asset(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
    ctl=unreal.IKRigController.get_controller(obj)
    if not ctl.set_skeletal_mesh(mesh): raise RuntimeError('Rig mesh failed')
    if not ctl.apply_auto_generated_retarget_definition(): raise RuntimeError('Auto rig template unavailable: '+name)
    if not ctl.apply_auto_fbik(): raise RuntimeError('Auto FBIK unavailable: '+name)
    save(obj)
    return obj

def retarget(name,source_mesh,target_mesh,source_rig,target_rig,clip,result_name):
    rtg=asset(name,unreal.IKRetargeter,unreal.IKRetargetFactory())
    ctl=unreal.IKRetargeterController.get_controller(rtg)
    s,t=unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
    ctl.set_ik_rig(s,source_rig); ctl.set_ik_rig(t,target_rig)
    ctl.set_preview_mesh(s,source_mesh); ctl.set_preview_mesh(t,target_mesh)
    ctl.remove_all_ops(); ctl.add_default_ops(); ctl.auto_map_chains(unreal.AutoMapChainType.EXACT,True)
    ctl.reset_retarget_pose(ctl.get_current_retarget_pose_name(t),[],t)
    ctl.auto_align_all_bones(t)
    # Real root, never pelvis, owns root-motion output for custom skeleton stage.
    if target_mesh.get_path_name().split('.')[0]==ARIN:
        for i in range(ctl.get_num_retarget_ops()):
            op=ctl.get_op_controller(i)
            if isinstance(op,unreal.IKRetargetRootMotionController):
                op.set_source_root_bone('root'); op.set_target_root_bone('root')
    save(rtg)
    target_path=ROOT+'/'+result_name
    if not unreal.EditorAssetLibrary.does_asset_exist(target_path):
        inputs=unreal.IKRetargetBatchOperationInputs()
        inputs.assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(clip.get_path_name())]
        inputs.source_mesh=source_mesh; inputs.target_mesh=target_mesh; inputs.ik_retarget_asset=rtg
        inputs.search=clip.get_name(); inputs.replace=result_name; inputs.target_path=ROOT
        inputs.use_source_path=False; inputs.include_referenced_assets=False; inputs.overwrite_existing_files=False
        unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    output=require(target_path)
    if output.get_editor_property('skeleton')!=target_mesh.get_editor_property('skeleton'): raise RuntimeError('Wrong skeleton')
    output.set_editor_property('enable_root_motion',False); save(output)
    return output

def audit(clip,mesh):
    opts=unreal.AnimPoseEvaluationOptions(); opts.optional_skeletal_mesh=mesh
    names=['root','pelvis','head','upperarm_l','lowerarm_l','hand_l','upperarm_r','lowerarm_r','hand_r','thigh_l','calf_l','foot_l','thigh_r','calf_r','foot_r']
    rows=[]
    for i in range(round(clip.get_play_length()*30)+1):
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,min(i/30,clip.get_play_length()),opts)
        row={}
        for n in names:
            tr=unreal.AnimPoseExtensions.get_bone_pose(pose,n,unreal.AnimPoseSpaces.WORLD)
            row[n]=[tr.translation.x,tr.translation.y,tr.translation.z]
            if not all(math.isfinite(v) for v in row[n]): raise RuntimeError('Nonfinite pose')
        rows.append(row)
    ranges={n:[max(r[n][k] for r in rows)-min(r[n][k] for r in rows) for k in range(3)] for n in names}
    report={'asset':clip.get_path_name(),'mesh':mesh.get_path_name(),'duration':clip.get_play_length(),'ranges_cm':ranges,'samples':rows}
    (OUT/(clip.get_name()+'-poses.json')).write_text(json.dumps(report,indent=2))
    return {k:v for k,v in report.items() if k!='samples'}

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    source_root=ROOT+'/Source'
    digest=hashlib.sha256(FBX.read_bytes()).hexdigest()
    if not unreal.EditorAssetLibrary.does_directory_exist(source_root):
        opt=unreal.FbxImportUI()
        opt.set_editor_properties({'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_SKELETAL_MESH,'original_import_type':unreal.FBXImportType.FBXIT_SKELETAL_MESH,'import_as_skeletal':True,'import_mesh':True,'import_animations':True,'import_materials':False,'import_textures':False,'create_physics_asset':False})
        opt.skeletal_mesh_import_data.set_editor_properties({'use_t0_as_ref_pose':False,'update_skeleton_reference_pose':False,'import_morph_targets':False})
        opt.anim_sequence_import_data.set_editor_properties({'use_default_sample_rate':True,'remove_redundant_keys':False})
        task=unreal.AssetImportTask(); task.set_editor_properties({'filename':str(FBX),'destination_path':source_root,'destination_name':'SK_MixamoBreathing','automated':True,'replace_existing':False,'save':True,'options':opt,'factory':unreal.FbxFactory()})
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        for p in unreal.EditorAssetLibrary.list_assets(source_root,recursive=True):
            obj=require(p); unreal.EditorAssetLibrary.set_metadata_tag(obj,'Rogue10m.SourceSHA256',digest); save(obj)
    imported=[require(p) for p in unreal.EditorAssetLibrary.list_assets(source_root,recursive=True)]
    if any(unreal.EditorAssetLibrary.get_metadata_tag(o,'Rogue10m.SourceSHA256')!=digest for o in imported): raise RuntimeError('Source provenance changed')
    source_mesh=next(o for o in imported if isinstance(o,unreal.SkeletalMesh))
    clip=next(o for o in imported if isinstance(o,unreal.AnimSequence))
    manny,arin=require(MANNY),require(ARIN)
    sr=rig('IK_MixamoBreathing',source_mesh); mr=rig('IK_Manny',manny)
    mclip=retarget('RTG_Mixamo_Manny',source_mesh,manny,sr,mr,clip,'A_Breathing_Manny')
    first=audit(mclip,manny)
    ar=rig('IK_Arin',arin)
    aclip=retarget('RTG_Manny_Arin',manny,arin,mr,ar,mclip,'A_Breathing_Arin_Raw')
    second=audit(aclip,arin)
    report={'status':'retargeted_not_yet_activated_requires_visual_review','source_fbx':str(FBX),'sha256':digest,'source_animation':clip.get_path_name(),'stages':[first,second],'guide_idle_unchanged':require(DATA).get_editor_property('guide_idle_animation').get_path_name()}
    (OUT/'retarget-report.json').write_text(json.dumps(report,indent=2))
    unreal.log('ARIN_STANDING_RETARGET_DONE '+json.dumps(report))

if __name__=='__main__':
    main()
