"""Repair only the original eight task-local animation skeleton bindings, then exit for a real cold reload."""
from pathlib import Path
import json,unreal
P=Path(unreal.Paths.project_dir()).resolve();R='/Game/Rogue10m/Animation/Combat/GreatSword20261110/Source'
NAMES=['Idle','Walk','Run','Primary01','Primary02','Primary03','Special','Charged']
try:
 mesh=unreal.load_asset(R+'/SK_MixamoXBot');skeleton=unreal.load_asset(R+'/SK_MixamoXBot_Skeleton')
 if not mesh or not skeleton:raise RuntimeError('Persisted source mesh/skeleton missing before binding repair')
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 prior_load_diagnostics=False
 try:unreal.SystemLibrary.execute_console_command(world,'Rogue10m.ImportGreatSwordAlternatives')
 except RuntimeError as error:
  # The native command repairs the objects, then Python receives pre-existing load
  # diagnostics. Do not infer success from the exception: verify every binding below.
  text=str(error)
  if '/Source/A_Source_' not in text or 'USkeleton' not in text:raise
  prior_load_diagnostics=True
  print('Existing invalid-source load diagnostics retained in this repair log; verifying native outcome before save.')
 rows=[]
 for name in NAMES:
  clip=unreal.load_asset(R+'/A_Source_'+name)
  if not clip or clip.get_editor_property('skeleton')!=skeleton:raise RuntimeError('Source binding was not repaired '+name)
  if not unreal.EditorAssetLibrary.save_loaded_asset(clip,False):raise RuntimeError('Cannot save repaired source '+name)
  rows.append({'clip':clip.get_path_name(),'skeleton':skeleton.get_path_name(),'duration':clip.get_play_length()})
 for obj in [mesh,skeleton]:
  if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False):raise RuntimeError('Cannot save source dependency')
 out=P/'Feature/doc/evidence/sword-knuckle-20261110/source-binding-repair.json';out.write_text(json.dumps({'prior_invalid_source_load_diagnostics':prior_load_diagnostics,'clips':rows,'requires_separate_cold_reload':True},indent=2),encoding='utf-8')
 print('RESULT=WEAPON_SOURCE_BINDINGS_REPAIRED')
finally:unreal.SystemLibrary.quit_editor()
