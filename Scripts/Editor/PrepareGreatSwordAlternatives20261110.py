"""Try the two licensed animation-only alternatives after the repaired source skeleton cold reload.
Only new task-owned assets are imported/retargeted; existing eight clips and rigs are untouched.
"""
from pathlib import Path
import json,runpy,unreal
P=Path(unreal.Paths.project_dir()).resolve();R='/Game/Rogue10m/Animation/Combat/GreatSword20261110';OUT=P/'Feature/doc/evidence/sword-knuckle-20261110/ResumeSword1'
def main():
 h=runpy.run_path(str(P/'Scripts/Editor/PrepareGreatSwordAnimations20261110.py'));need=h['require'];save=h['save'];retarget=h['retarget'];audit=h['audit']
 source=need(R+'/Source/SK_MixamoXBot');sk=source.get_editor_property('skeleton')
 if not sk or not unreal.EditorAssetLibrary.does_asset_exist(sk.get_path_name()):raise RuntimeError('Correct saved source skeleton required')
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();unreal.SystemLibrary.execute_console_command(world,'Rogue10m.ImportGreatSwordAlternatives')
 rows=[]
 for n in ['AltSlash04','AltSlash05']:
  path=R+'/Source/A_Source_'+n
  if not unreal.EditorAssetLibrary.does_asset_exist(path):rows.append({'name':n,'status':'import_failed_not_retried'});continue
  clip=need(path)
  if clip.get_editor_property('skeleton')!=sk:raise RuntimeError('Alternative skeleton mismatch '+n)
  save(clip)
  a=retarget('A_GreatSword_'+n,clip,source,need(h['MANNY']),need(R+'/RTG_Mixamo_Manny'))
  b=retarget('A_GreatSword_'+n+'_Human',a,need(h['MANNY']),need(h['HUMAN']),need(R+'/RTG_Manny_Human'))
  for obj,mesh in [(a,need(h['MANNY'])),(b,need(h['HUMAN']))]:
   unreal.EditorAssetLibrary.set_metadata_tag(obj,'Rogue10m.Source',clip.get_path_name());save(obj);audit(obj,mesh)
  rows.append({'name':n,'status':'new_candidate_saved','source':clip.get_path_name(),'manny':a.get_path_name(),'human':b.get_path_name()})
 (OUT/'alternative-preparation.json').write_text(json.dumps({'source_skeleton':sk.get_path_name(),'activated':False,'candidates':rows},indent=2),encoding='utf-8')
 print('RESULT=GREATSWORD_ALTERNATIVES_CHECKED '+json.dumps(rows))
if __name__=='__main__':
 try:main()
 finally:unreal.SystemLibrary.quit_editor()