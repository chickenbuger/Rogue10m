"""Import licensed local Mixamo greatsword clips, retarget via Manny and stage weapon-specific gameplay assets.
No downloaded FBX, vendor originals or existing fist clips are overwritten. Run with UnrealEditor-Cmd.
"""
from pathlib import Path
import json,math,hashlib,runpy
import unreal
PROJECT=Path(unreal.Paths.project_dir()).resolve()
ROOT='/Game/Rogue10m/Animation/Combat/GreatSword20261110'
OUT=PROJECT/'Feature/doc/evidence/sword-knuckle-20261110'
SOURCE=Path('C:/Users/PC/Downloads/Rogue10m_Mixamo_GreatSword_20261110')
MANNY='/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
HUMAN='/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody'
SKILLS='/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_'
CLIPS={'Idle':'great sword idle.fbx','Walk':'great sword walk.fbx','Run':'great sword run.fbx',
'Primary01':'great sword attack.fbx','Primary02':'great sword slash.fbx','Primary03':'great sword slash (3).fbx',
'Special':'great sword high spin attack.fbx','Charged':'great sword slash (2).fbx'}
PROTECTED=['animation_play_rate','attack_cooldown','charge_seconds','hit_mode','hit_count','hit_interval','max_hits_per_target','max_targets_per_hit','damage','resource_costs','attack_range','attack_shape','attack_trace_radius','box_half_width','box_half_height','arc_angle_degrees','enable_combo','combo_window_open_seconds','combo_window_close_seconds','next_combo_skill','input_slot']
def require(p):
 x=unreal.load_asset(p)
 if not x:raise RuntimeError('Missing '+p)
 return x
def save(x):
 if not unreal.EditorAssetLibrary.save_loaded_asset(x,False):raise RuntimeError('Save failed '+x.get_path_name())
def create(name,cls,factory):
 p=ROOT+'/'+name
 return require(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT,cls,factory)
def rig(name,mesh):
 x=create(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory());c=unreal.IKRigController.get_controller(x)
 if not c.set_skeletal_mesh(mesh) or not c.apply_auto_generated_retarget_definition() or not c.apply_auto_fbik():raise RuntimeError('Auto rig failed '+name)
 save(x);return x
def retargeter(name,sm,tm,sr,tr):
 x=create(name,unreal.IKRetargeter,unreal.IKRetargetFactory());c=unreal.IKRetargeterController.get_controller(x)
 s,t=unreal.RetargetSourceOrTarget.SOURCE,unreal.RetargetSourceOrTarget.TARGET
 c.set_ik_rig(s,sr);c.set_ik_rig(t,tr);c.set_preview_mesh(s,sm);c.set_preview_mesh(t,tm)
 c.remove_all_ops();c.add_default_ops();c.auto_map_chains(unreal.AutoMapChainType.EXACT,True)
 c.reset_retarget_pose(c.get_current_retarget_pose_name(t),[],t);c.auto_align_all_bones(t)
 # Preserve the target proportions without FBIK adding foot or wrist spikes.
 for i in range(c.get_num_retarget_ops()):
  op=c.get_op_controller(i)
  if isinstance(op,unreal.IKRetargetRunIKRigController):c.set_retarget_op_enabled(i,False)
  if isinstance(op,unreal.IKRetargetRootMotionController) and sm.get_path_name().split('.')[0]==MANNY:
   op.set_source_root_bone('root');op.set_target_root_bone('root')
 save(x);return x
def retarget(name,clip,sm,tm,rt):
 p=ROOT+'/'+name
 if True:
  i=unreal.IKRetargetBatchOperationInputs();i.assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(clip.get_path_name())]
  i.source_mesh=sm;i.target_mesh=tm;i.ik_retarget_asset=rt;i.search=clip.get_name();i.replace=name;i.target_path=ROOT
  i.use_source_path=False;i.include_referenced_assets=False;i.overwrite_existing_files=True
  unreal.IKRetargetBatchOperation.run_batch_retarget(i)
 x=require(p)
 if x.get_editor_property('skeleton')!=tm.get_editor_property('skeleton'):raise RuntimeError('Retarget skeleton mismatch')
 x.set_editor_properties({'enable_root_motion':False,'force_root_lock':False})
 # Keep authored vertical body motion and pose turns for third-person combat, removing
 # only planar displacement. ForceRootLock RefPose would erase the imported height/turn.
 opt=unreal.AnimPoseEvaluationOptions();opt.optional_skeletal_mesh=tm;opt.extract_root_motion=False;opt.incorporate_root_motion_into_pose=True
 model=x.get_editor_property('data_model_interface');frames=model.get_number_of_frames();rate=model.get_frame_rate();fps=rate.numerator/rate.denominator
 translations=[];rotations=[];scales=[];first=None
 for frame in range(frames+1):
  pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(x,min(frame/fps,x.get_play_length()),opt)
  root=unreal.AnimPoseExtensions.get_bone_pose(pose,'root',unreal.AnimPoseSpaces.LOCAL)
  if first is None:first=root.translation
  translations.append(unreal.Vector(first.x,first.y,root.translation.z));rotations.append(root.rotation);scales.append(root.scale3d)
 ctl=x.get_editor_property('controller');ctl.open_bracket('Preserve authored height and turn; remove planar root displacement',False)
 try:ctl.set_bone_track_keys('root',translations,rotations,scales,False)
 finally:ctl.close_bracket(False)
 save(x);return x
def import_source():
 folder=ROOT+'/Source';meshpath=folder+'/SK_MixamoXBot'
 if not unreal.EditorAssetLibrary.does_asset_exist(meshpath):
  o=unreal.FbxImportUI();o.set_editor_properties({'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_SKELETAL_MESH,'original_import_type':unreal.FBXImportType.FBXIT_SKELETAL_MESH,'import_as_skeletal':True,'import_mesh':True,'import_animations':False,'import_materials':False,'import_textures':False,'create_physics_asset':False})
  o.skeletal_mesh_import_data.set_editor_properties({'use_t0_as_ref_pose':False,'update_skeleton_reference_pose':False,'import_morph_targets':False})
  task=unreal.AssetImportTask();task.set_editor_properties({'filename':str(SOURCE/'X Bot.fbx'),'destination_path':folder,'destination_name':'SK_MixamoXBot','automated':True,'replace_existing':False,'save':True,'options':o,'factory':unreal.FbxFactory()})
  unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 mesh=require(meshpath);clips={}
 if mesh.get_editor_property('skeleton'):save(mesh.get_editor_property('skeleton'))
 missing_alternatives=[]
 if missing_alternatives or not mesh.get_editor_property('skeleton'):
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
  unreal.SystemLibrary.execute_console_command(world,'Rogue10m.ImportGreatSwordAlternatives')
  skeleton=mesh.get_editor_property('skeleton')
  if not skeleton:raise RuntimeError('Source skeleton dependency was not repaired')
  save(skeleton);save(mesh)
  for name in missing_alternatives:
   candidate=unreal.load_asset(folder+'/A_Source_'+name)
   if candidate:save(candidate)
 skipped=[]
 for name,filename in CLIPS.items():
  p=folder+'/A_Source_'+name
  if name.startswith('Alt') and not unreal.EditorAssetLibrary.does_asset_exist(p):
   skipped.append({'candidate':name,'file':filename,'status':'optional import failed; retained outside base activation'});continue
  if not unreal.EditorAssetLibrary.does_asset_exist(p):
   o=unreal.FbxImportUI();o.set_editor_properties({'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_ANIMATION,'original_import_type':unreal.FBXImportType.FBXIT_ANIMATION,'import_as_skeletal':True,'import_mesh':False,'import_animations':True,'skeleton':mesh.get_editor_property('skeleton'),'import_materials':False,'import_textures':False})
   o.anim_sequence_import_data.set_editor_properties({'use_default_sample_rate':True,'remove_redundant_keys':False})
   factory=unreal.FbxFactory()
   task=unreal.AssetImportTask();task.set_editor_properties({'filename':str(SOURCE/filename),'destination_path':folder,'destination_name':'A_Source_'+name,'automated':True,'replace_existing':False,'save':True,'options':o,'factory':factory})
   unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
  clips[name]=require(p)
  if clips[name].get_editor_property('skeleton')!=mesh.get_editor_property('skeleton'):raise RuntimeError('Refuse to save or retarget a source with an invalid skeleton '+name)
  unreal.EditorAssetLibrary.set_metadata_tag(clips[name],'Rogue10m.Source',filename)
  unreal.EditorAssetLibrary.set_metadata_tag(clips[name],'Rogue10m.SourceSHA256',hashlib.sha256((SOURCE/filename).read_bytes()).hexdigest());save(clips[name])
 (OUT/'source-import-skips.json').write_text(json.dumps(skipped,indent=2),encoding='utf-8')
 return mesh,clips
def vec(v):return [v.x,v.y,v.z]
def audit(clip,mesh):
 o=unreal.AnimPoseEvaluationOptions();o.optional_skeletal_mesh=mesh;o.extract_root_motion=False;o.incorporate_root_motion_into_pose=True
 rows=[]
 for i in range(math.ceil(clip.get_play_length()*60)+1):
  t=min(i/60,clip.get_play_length());p=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,t,o);bones={}
  for n in ['root','pelvis','head','hand_l','hand_r','middle_01_l','middle_01_r','foot_l','foot_r']:
   tr=unreal.AnimPoseExtensions.get_bone_pose(p,n,unreal.AnimPoseSpaces.WORLD)
   bones[n]={'p':vec(tr.translation),'q':[tr.rotation.x,tr.rotation.y,tr.rotation.z,tr.rotation.w]}
   if not all(math.isfinite(v) for v in bones[n]['p']+bones[n]['q']):raise RuntimeError('Nonfinite pose')
  rows.append({'t':t,'bones':bones})
 heights=[row['bones']['head']['p'][2]-min(row['bones']['foot_l']['p'][2],row['bones']['foot_r']['p'][2]) for row in rows]
 root0=rows[0]['bones']['root']['p'];root_travel=max(math.dist(row['bones']['root']['p'][:2],root0[:2]) for row in rows)
 if not 100<max(heights)<240 or root_travel>.01:raise RuntimeError('Retarget stature/root drift check failed '+clip.get_path_name())
 hand_travel_cm=max(math.dist(row['bones'][hand]['p'],rows[0]['bones'][hand]['p']) for row in rows for hand in ['hand_l','hand_r'])
 bone_track_count=len(clip.get_editor_property('data_model_interface').get_bone_track_names())
 if any(n in clip.get_name() for n in ['Primary','Special','Charged','AltSlash']) and (bone_track_count<20 or hand_travel_cm<2.):raise RuntimeError('Empty/static attack candidate '+clip.get_path_name())
 result={'bone_track_count':bone_track_count,'hand_travel_cm':hand_travel_cm,'asset':clip.get_path_name(),'mesh':mesh.get_path_name(),'duration':clip.get_play_length(),'root_first':root0,'root_xy_travel_cm':root_travel,'root_z_range_cm':[min(row['bones']['root']['p'][2] for row in rows),max(row['bones']['root']['p'][2] for row in rows)],'foot_z_range_cm':[min(row['bones'][foot]['p'][2] for row in rows for foot in ['foot_l','foot_r']),max(row['bones'][foot]['p'][2] for row in rows for foot in ['foot_l','foot_r'])],'head_foot_height_range_cm':[min(heights),max(heights)],'samples':rows}
 (OUT/(clip.get_name()+'-poses.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
 return result
def armed_directional_stride(manny):
 """Keep the proven directional leg strides, replace only arm subtrees with the licensed sword guard."""
 bs=require(ROOT+'/BS_GreatSword_Ground');source_bs=require('/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference')
 o=unreal.AnimPoseEvaluationOptions();o.optional_skeletal_mesh=manny;o.extract_root_motion=False;o.incorporate_root_motion_into_pose=True
 idle=require(ROOT+'/A_GreatSword_Idle');guard=unreal.AnimPoseExtensions.get_anim_pose_at_time(idle,0,o)
 names=[str(n) for n in unreal.AnimPoseExtensions.get_bone_names(guard)]
 arms=[]
 for n in names:
  chain=[str(x) for x in unreal.AnimationLibrary.find_bone_path_to_root(idle,n)]
  if any(x in chain for x in ['clavicle_l','clavicle_r']):arms.append(n)
 requests=[];done={}
 for sample in source_bs.get_editor_property('sample_data'):
  original=sample.get_editor_property('animation')
  if original.get_path_name() in done:continue
  name='A_Armed_'+original.get_name();path=ROOT+'/'+name
  if True:
   x=require(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(original.get_path_name(),path)
   model=x.get_editor_property('data_model_interface');rate=model.get_frame_rate();frames=model.get_number_of_frames()
   controller=x.get_editor_property('controller');controller.open_bracket('Sword guard over preserved directional stride',False)
   try:
    for bone in arms:
     tr=unreal.AnimPoseExtensions.get_bone_pose(guard,bone,unreal.AnimPoseSpaces.LOCAL)
     controller.add_bone_curve(bone,False)
     controller.set_bone_track_keys(bone,[tr.translation]*(frames+1),[tr.rotation]*(frames+1),[tr.scale3d]*(frames+1),False)
   finally:controller.close_bracket(False)
   unreal.EditorAssetLibrary.set_metadata_tag(x,'Rogue10m.Source',original.get_path_name())
   unreal.EditorAssetLibrary.set_metadata_tag(x,'Rogue10m.ArmedPose',idle.get_path_name());save(x)
  done[original.get_path_name()]=path;requests.append(original.get_path_name()+'|'+path)
 # Idempotent rerun accepts current mapped clip names as well.
 for path in done.values():requests.append(path+'.'+path.rsplit('/',1)[1]+'|'+path)
 (OUT/'stride-requests.txt').write_text('\n'.join(requests),encoding='utf-8')
 return done

def prepare_sword_body_blueprint(rt):
 path=ROOT+'/ABP_Human_GreatSword'
 bp=require(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset('/Game/Character/Customization/Retargeting/ABP_Retarget_Hu_M',path)
 graphs=unreal.BlueprintEditorLibrary.list_graphs(bp)
 nodes=[n for g in graphs for n in unreal.BlueprintGraphEditor.get_graph_editor(g).list_all_nodes() if isinstance(n,unreal.AnimGraphNode_RetargetPoseFromMesh)]
 if len(nodes)!=1:raise RuntimeError('Expected one sword body retarget node, got '+str(len(nodes)))
 node=nodes[0];runtime=node.get_editor_property('node');runtime.set_editor_property('ik_retargeter_asset',rt);node.set_editor_property('node',runtime)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 for g in graphs:
  errors=unreal.BlueprintGraphEditor.get_graph_editor(g).list_nodes_with_errors()
  if errors:raise RuntimeError('Sword body graph compile error '+str(errors))
 save(bp)
 return bp

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 sm,clips=import_source();m=require(MANNY);h=require(HUMAN)
 sr,mr,hr=rig('IK_MixamoGreatSword',sm),rig('IK_MannyGreatSword',m),rig('IK_HumanGreatSword',h)
 rt=retargeter('RTG_Mixamo_Manny',sm,m,sr,mr)
 # Weapon-owned aligned retargeter; the normal unarmed/knuckle retargeter stays unchanged.
 rh=retargeter('RTG_Manny_Human',m,h,mr,hr)
 prepare_sword_body_blueprint(rh)
 results=[]
 for name,clip in clips.items():
  a=retarget('A_GreatSword_'+name,clip,sm,m,rt);b=retarget('A_GreatSword_'+name+'_Human',a,m,h,rh)
  for x,mesh in [(a,m),(b,h)]:
   unreal.EditorAssetLibrary.set_metadata_tag(x,'Rogue10m.Source',clip.get_path_name());save(x)
   report=audit(x,mesh);results.append({k:v for k,v in report.items() if k!='samples'})
 # Duplicate existing directional stride grid; native authoring replaces samples with weapon-specific clips.
 bs=ROOT+'/BS_GreatSword_Ground'
 if not unreal.EditorAssetLibrary.does_asset_exist(bs):unreal.EditorAssetLibrary.duplicate_asset('/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference',bs)
 stride=armed_directional_stride(m)
 requests=[]
 for name in ['Primary01','Primary02','Primary03','Special','Charged']:
  skill=require(SKILLS+name);dest=ROOT+'/AM_GreatSword_'+name
  if not unreal.EditorAssetLibrary.does_asset_exist(dest):unreal.EditorAssetLibrary.duplicate_asset(skill.get_editor_property('attack_montage').get_path_name(),dest)
  requests.append(dest+'|'+ROOT+'/A_GreatSword_'+name)
 (OUT/'montage-requests.txt').write_text('\n'.join(requests),encoding='utf-8')
 (OUT/'greatsword-retarget.json').write_text(json.dumps({'status':'staged_not_activated','source_directory':str(SOURCE),'clips':results,'armed_stride_derivatives':stride},indent=2),encoding='utf-8')
 print('RESULT=GREATSWORD_RETARGET_STAGED')
if __name__=='__main__':main()
