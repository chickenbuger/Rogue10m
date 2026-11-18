"""Retarget owned humanoid clips into a separate boss folder; never activate live content."""
from pathlib import Path
import json, math, hashlib
import unreal
PROJECT=Path(unreal.Paths.project_dir()).resolve()
ROOT='/Game/Rogue10m/Characters/Boss/AzathorHuman20261110'
OUT=PROJECT/'Feature/doc/evidence/humanoid-boss-npc-20261110'
TARGET='/Game/Polyart/ModularStylizedChars/Meshes/SkeletalMeshes/Characters/SK_Garret_FullBody'
GROUPS={
 'Combat':('/Game/CloseCombatAnimSet/Mannequin/Character/Mesh/SK_Mannequin',{
 'A_HumanBoss_Idle':'/Game/CloseCombatAnimSet/Animations/InPlace/Locomotion/Idle/NinjaIdle',
 'A_HumanBoss_Hook_Raw':'/Game/CloseCombatAnimSet/Animations/InPlace/Attacks/NinjaLeftHook',
 'A_HumanBoss_Hit':'/Game/CloseCombatAnimSet/Animations/InPlace/Blocks/NinjaBlockHit'}),
 'Magic':('/Game/CombatMagicAnims/Demo/Mannequins/Meshes/SKM_Manny_Simple',{
 'A_HumanBoss_Wave_Raw':'/Game/CombatMagicAnims/Animations/AS_DualEnergyBlas',
 'A_HumanBoss_Zone_Raw':'/Game/CombatMagicAnims/Animations/AS_GroundManaShot'}),
 'Manny':('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',{
 'A_HumanBoss_Walk':'/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd',
 'A_HumanBoss_Death':'/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01'})}
def require(p):
 v=unreal.load_asset(p)
 if not v: raise RuntimeError('Missing '+p)
 return v
def save(v):
 if not v.get_path_name().startswith(ROOT+'/'): raise RuntimeError('Refuse original mutation')
 if not unreal.EditorAssetLibrary.save_loaded_asset(v,False): raise RuntimeError('Cannot save '+v.get_path_name())
def new(name,cls,factory):
 p=ROOT+'/'+name
 return require(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT,cls,factory)
def rig(name,mesh):
 v=new(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
 c=unreal.IKRigController.get_controller(v)
 if not c.set_skeletal_mesh(mesh) or not c.apply_auto_generated_retarget_definition() or not c.apply_auto_fbik(): raise RuntimeError('Unsupported rig '+name)
 save(v)
 return v
def pose_report(clip,mesh):
 opt=unreal.AnimPoseEvaluationOptions(); opt.optional_skeletal_mesh=mesh
 names=['root','pelvis','head','upperarm_l','lowerarm_l','hand_l','upperarm_r','lowerarm_r','hand_r','thigh_l','calf_l','foot_l','thigh_r','calf_r','foot_r']
 rows=[]
 for i in range(round(clip.get_play_length()*30)+1):
  t=min(i/30,clip.get_play_length()); pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,t,opt)
  available=set(map(str,unreal.AnimPoseExtensions.get_bone_names(pose)))
  if not set(names)<=available: raise RuntimeError('Missing bones')
  row={'time':t,'bones':{}}
  for n in names:
   tr=unreal.AnimPoseExtensions.get_bone_pose(pose,n,unreal.AnimPoseSpaces.WORLD)
   values=[tr.translation.x,tr.translation.y,tr.translation.z]
   if not all(math.isfinite(x) for x in values): raise RuntimeError('Nonfinite pose')
   row['bones'][n]=values
  rows.append(row)
 spans=[r['bones']['head'][2]-min(r['bones']['foot_l'][2],r['bones']['foot_r'][2]) for r in rows]
 report={'path':clip.get_path_name(),'skeleton':clip.get_editor_property('skeleton').get_path_name(),'length':clip.get_play_length(),'head_foot_height_range_cm':[min(spans),max(spans)],'samples':rows}
 (OUT/(clip.get_name()+'-poses.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
 if max(spans)<100 or max(spans)>240: raise RuntimeError('Unexpected human scale '+str(report['head_foot_height_range_cm']))
 return {k:v for k,v in report.items() if k!='samples'}
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 target=require(TARGET); targetrig=rig('IK_HumanBoss',target)
 results=[]; protected={}
 for group,(meshpath,clips) in GROUPS.items():
  source=require(meshpath); sourcerig=rig('IK_'+group,source)
  rtg=new('RTG_'+group+'_HumanBoss',unreal.IKRetargeter,unreal.IKRetargetFactory()); c=unreal.IKRetargeterController.get_controller(rtg)
  s,t=unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
  c.set_ik_rig(s,sourcerig);c.set_ik_rig(t,targetrig);c.set_preview_mesh(s,source);c.set_preview_mesh(t,target)
  c.remove_all_ops();c.add_default_ops();c.auto_map_chains(unreal.AutoMapChainType.EXACT,True)
  c.reset_retarget_pose(c.get_current_retarget_pose_name(t),[],t);c.auto_align_all_bones(t);save(rtg)
  for name,path in clips.items():
   original=require(path)
   fp=PROJECT/'Content'/(path[6:]+'.uasset'); protected[path]=hashlib.sha256(fp.read_bytes()).hexdigest()
   destination=ROOT+'/'+name
   if not unreal.EditorAssetLibrary.does_asset_exist(destination):
    inp=unreal.IKRetargetBatchOperationInputs();inp.assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(path)]
    inp.source_mesh=source;inp.target_mesh=target;inp.ik_retarget_asset=rtg;inp.search=original.get_name();inp.replace=name
    inp.target_path=ROOT;inp.use_source_path=False;inp.include_referenced_assets=False;inp.overwrite_existing_files=False
    unreal.IKRetargetBatchOperation.run_batch_retarget(inp)
   clip=require(destination)
   if clip.get_editor_property('skeleton')!=target.get_editor_property('skeleton'): raise RuntimeError('Skeleton mismatch')
   clip.set_editor_property('enable_root_motion',False);clip.set_editor_property('force_root_lock',True)
   unreal.EditorAssetLibrary.set_metadata_tag(clip,'Rogue10m.Source',path)
   unreal.EditorAssetLibrary.set_metadata_tag(clip,'Rogue10m.Status','Isolated retarget candidate, live connection pending validation')
   save(clip); row=pose_report(clip,target);row['source']=path;results.append(row)
 for path,sha in protected.items():
  if hashlib.sha256((PROJECT/'Content'/(path[6:]+'.uasset')).read_bytes()).hexdigest()!=sha: raise RuntimeError('Original changed '+path)
 materials=[]
 for m in target.get_editor_property('materials'):
  mat=m.material_interface
  materials.append({'slot':str(m.material_slot_name),'material':mat.get_path_name() if mat else None})
 (OUT/'boss-retarget.json').write_text(json.dumps({'status':'retargeted_not_activated','mesh':TARGET,'materials':materials,'clips':results,'preserved_original_sha256':protected},indent=2),encoding='utf-8')
 print('RESULT=HUMAN_BOSS_RETARGET_PASSED')
if __name__=='__main__': main()

