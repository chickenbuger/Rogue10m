"""Cold-assert the repaired base sources, prepare candidates, then leave one serial editor session for scoped VFX inspection."""
from pathlib import Path
import json,runpy,unreal
P=Path(unreal.Paths.project_dir()).resolve();R='/Game/Rogue10m/Animation/Combat/GreatSword20261110/Source'
try:
 skeleton=unreal.load_asset(R+'/SK_MixamoXBot_Skeleton');mesh=unreal.load_asset(R+'/SK_MixamoXBot')
 if not skeleton or not mesh or mesh.get_editor_property('skeleton')!=skeleton:raise RuntimeError('Cold source dependency mismatch')
 rows=[]
 for name in ['Idle','Walk','Run','Primary01','Primary02','Primary03','Special','Charged']:
  clip=unreal.load_asset(R+'/A_Source_'+name)
  if not clip or clip.get_editor_property('skeleton')!=skeleton:raise RuntimeError('Cold source binding mismatch '+name)
  o=unreal.AnimPoseEvaluationOptions();o.optional_skeletal_mesh=mesh;o.extract_root_motion=False;o.incorporate_root_motion_into_pose=True
  pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,0,o)
  if not unreal.AnimPoseExtensions.get_bone_names(pose):raise RuntimeError('Cold source pose empty '+name)
  rows.append({'name':name,'duration':clip.get_play_length(),'skeleton':skeleton.get_path_name()})
 (P/'Feature/doc/evidence/sword-knuckle-20261110/source-cold-bindings.json').write_text(json.dumps({'result':'PASS','clips':rows,'repair_was_not_run_in_this_process':True},indent=2),encoding='utf-8')
 print('RESULT=WEAPON_SOURCE_COLD_PASSED')
 runpy.run_path(str(P/'Scripts/Editor/PrepareGreatSwordAnimations20261110.py'),run_name='__main__')
 print('RESULT=WEAPON_BASE_EDITOR_READY')
except Exception:
 unreal.SystemLibrary.quit_editor();raise
