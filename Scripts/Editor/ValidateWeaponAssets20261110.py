"""Fresh-process, asset-read-only validation of weapon dependencies and grounded source poses."""
from pathlib import Path
import json,math,unreal
P=Path(unreal.Paths.project_dir()).resolve();R='/Game/Rogue10m/Animation/Combat/GreatSword20261110';OUT=P/'Feature/doc/evidence/sword-knuckle-20261110'
NAMES=['Idle','Walk','Run','Primary01','Primary02','Primary03','Special','Charged']
def need(path):
 x=unreal.load_asset(path)
 if not x:raise RuntimeError('Missing cold-loaded asset '+path)
 return x
def pose(clip,mesh,t):
 o=unreal.AnimPoseEvaluationOptions();o.optional_skeletal_mesh=mesh;o.extract_root_motion=False;o.incorporate_root_motion_into_pose=True
 return unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,t,o)
def bp(p,n):return unreal.AnimPoseExtensions.get_bone_pose(p,n,unreal.AnimPoseSpaces.WORLD)
def v(x):return [x.x,x.y,x.z]
def main():
 source_skeleton=need(R+'/Source/SK_MixamoXBot_Skeleton');source_mesh=need(R+'/Source/SK_MixamoXBot')
 if source_mesh.get_editor_property('skeleton')!=source_skeleton:raise RuntimeError('Cold source mesh skeleton mismatch')
 for name in NAMES:
  if need(R+'/Source/A_Source_'+name).get_editor_property('skeleton')!=source_skeleton:raise RuntimeError('Cold source animation skeleton mismatch '+name)
 manny=need('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple');human=need('/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody')
 reference=pose(need('/Game/Rogue10m/Animation/Combat/FighterStance/A_Fighter_CombatIdle'),manny,0)
 baseline=min(bp(reference,'foot_l').translation.z,bp(reference,'foot_r').translation.z)
 rows=[]
 for suffix,mesh in [('',manny),('_Human',human)]:
  for name in NAMES:
   clip=need(R+'/A_GreatSword_'+name+suffix)
   if clip.get_editor_property('skeleton')!=mesh.get_editor_property('skeleton'):raise RuntimeError('Cold retarget skeleton mismatch')
   frames=[]
   for t in [0,clip.get_play_length()*.5,clip.get_play_length()]:
    pp=pose(clip,mesh,t);bones={n:v(bp(pp,n).translation) for n in ['root','head','hand_l','hand_r','foot_l','foot_r']}
    if not all(math.isfinite(a) for xyz in bones.values() for a in xyz):raise RuntimeError('Cold nonfinite pose')
    frames.append({'t':t,'bones':bones})
   foot=min(frames[0]['bones']['foot_l'][2],frames[0]['bones']['foot_r'][2])
   if suffix=='' and name not in ['Walk','Run'] and not baseline-5<=foot<=baseline+15:raise RuntimeError('Manny ready foot plane differs from accepted guard '+str((name,foot,baseline)))
   rows.append({'clip':clip.get_path_name(),'duration':clip.get_play_length(),'first_min_foot_z':foot,'frames':frames})
 for name in ['GreatSword','Knuckle']:
  mesh=need('/Game/Weapons/Starter20261110/SM_Starter'+name)
  if len(mesh.get_editor_property('static_materials'))!=3:raise RuntimeError('Weapon materials missing')
 expected=unreal.SystemLibrary.get_command_line().find('-RogueWeaponExpectActive')>=0
 if expected:
  for name in NAMES[3:]:
   skill=need('/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_'+name);montage=skill.get_editor_property('attack_montage')
   if not montage or not montage.get_path_name().startswith(R+'/'):raise RuntimeError('Active sword montage not connected '+name)
   if abs(montage.get_editor_property('rate_scale')-1)>1e-6:raise RuntimeError('Montage rate scale not normalized')
 report={'status':'cold_read_passed','active_expected':expected,'source_skeleton':source_skeleton.get_path_name(),'manny_accepted_guard_min_foot_z':baseline,'clips':rows,'note':'Human and contact still require actual rendered gameplay review; no asset mutation performed.'}
 (OUT/('cold-reload-active.json' if expected else 'cold-reload-staged.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
 print('RESULT=WEAPON_COLD_RELOAD_PASSED')
if __name__=='__main__':
 try:main()
 finally:unreal.SystemLibrary.quit_editor()
