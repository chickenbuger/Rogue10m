"""Repair one empty native-import candidate through the complete FbxFactory options route.
Preserves the empty candidate and all original source/retargeted clips for evidence.
"""
from pathlib import Path
import json,math,runpy,hashlib,unreal
P=Path(unreal.Paths.project_dir()).resolve();R='/Game/Rogue10m/Animation/Combat/GreatSword20261110';O=P/'Feature/doc/evidence/sword-knuckle-20261110/ResumeSword1'
def main():
 h=runpy.run_path(str(P/'Scripts/Editor/PrepareGreatSwordAnimations20261110.py'));need=h['require'];save=h['save'];source=need(R+'/Source/SK_MixamoXBot');skeleton=source.get_editor_property('skeleton')
 filename=Path('C:/Users/PC/Downloads/Rogue10m_Mixamo_GreatSword_20261110/great sword slash (4).fbx');dest=R+'/Source/A_Source_AltSlash04Repair1'
 if unreal.EditorAssetLibrary.does_asset_exist(dest):raise RuntimeError('Repair destination exists; read result before retry')
 ui=unreal.FbxImportUI();ui.set_editor_properties({'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_ANIMATION,'original_import_type':unreal.FBXImportType.FBXIT_ANIMATION,'import_as_skeletal':True,'import_mesh':False,'import_animations':True,'skeleton':skeleton,'import_materials':False,'import_textures':False})
 ui.anim_sequence_import_data.set_editor_properties({'use_default_sample_rate':True,'import_bone_tracks':True,'remove_redundant_keys':False})
 task=unreal.AssetImportTask();task.set_editor_properties({'filename':str(filename),'destination_path':R+'/Source','destination_name':'A_Source_AltSlash04Repair1','automated':True,'replace_existing':False,'save':True,'options':ui,'factory':unreal.FbxFactory()})
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);clip=need(dest);model=clip.get_editor_property('data_model_interface');tracks=list(map(str,model.get_bone_track_names()))
 if len(tracks)<20:raise RuntimeError('Factory bone tracks still empty '+str(len(tracks)))
 def motion(a,mesh):
  opts=unreal.AnimPoseEvaluationOptions();opts.optional_skeletal_mesh=mesh;opts.extract_root_motion=False;opts.incorporate_root_motion_into_pose=True
  poses=[unreal.AnimPoseExtensions.get_anim_pose_at_time(a,a.get_play_length()*t,opts) for t in [0,.2,.4,.6,.8,1]]
  names=list(map(str,unreal.AnimPoseExtensions.get_bone_names(poses[0])));hand=next(n for n in names if n.lower().endswith('righthand') or n=='hand_r')
  points=[unreal.AnimPoseExtensions.get_bone_pose(p,hand,unreal.AnimPoseSpaces.WORLD).translation for p in poses];travel=max((p-points[0]).length() for p in points)
  if travel<10:raise RuntimeError('No real hand motion '+a.get_path_name()+' '+str(travel))
  return {'track_count':len(list(a.get_editor_property('data_model_interface').get_bone_track_names())),'hand':hand,'hand_travel_cm':travel,'duration':a.get_play_length()}
 reports=[{'asset':clip.get_path_name(),**motion(clip,source)}];save(clip)
 manny=h['retarget']('A_GreatSword_AltSlash04Repair1',clip,source,need(h['MANNY']),need(R+'/RTG_Mixamo_Manny'));reports.append({'asset':manny.get_path_name(),**motion(manny,need(h['MANNY']))})
 human=h['retarget']('A_GreatSword_AltSlash04Repair1_Human',manny,need(h['MANNY']),need(h['HUMAN']),need(R+'/RTG_Manny_Human'));reports.append({'asset':human.get_path_name(),**motion(human,need(h['HUMAN']))})
 for a,mesh in [(manny,need(h['MANNY'])),(human,need(h['HUMAN']))]:unreal.EditorAssetLibrary.set_metadata_tag(a,'Rogue10m.Source',clip.get_path_name());save(a);h['audit'](a,mesh)
 (O/'alternative-repair1.json').write_text(json.dumps({'route':'FbxFactory with full FbxImportUI; MCP import API unavailable','source_file':str(filename),'sha256':hashlib.sha256(filename.read_bytes()).hexdigest(),'activated':False,'stages':reports},indent=2),encoding='utf-8')
 print('RESULT=GREATSWORD_ALTERNATIVE_REPAIR1_POSES_VALID '+json.dumps(reports))
if __name__=='__main__':main()