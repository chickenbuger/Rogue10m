import unreal,json
from pathlib import Path
root='/Game/Rogue10m/Characters/Arin/'
mesh=unreal.load_asset(root+'RefinedV3/SK_Arin')
opts=unreal.AnimPoseEvaluationOptions();opts.optional_skeletal_mesh=mesh
out={}
for path in [root+'RefinedV3/ArinIdle',root+'Standing20261110/A_Breathing_Arin_Raw']:
 a=unreal.load_asset(path);p=unreal.AnimPoseExtensions.get_anim_pose_at_time(a,0,opts)
 out[path]={}
 for n in ['root','pelvis','spine_01','head','foot_l']:
  out[path][n]={}
  for label,s in [('local',unreal.AnimPoseSpaces.LOCAL),('world',unreal.AnimPoseSpaces.WORLD)]:
   for pref,tr in [('pose',unreal.AnimPoseExtensions.get_bone_pose(p,n,s)),('ref',unreal.AnimPoseExtensions.get_ref_bone_pose(p,n,s))]:
    out[path][n][pref+label]={'p':list(tr.translation.to_tuple()),'q':[tr.rotation.x,tr.rotation.y,tr.rotation.z,tr.rotation.w],'s':list(tr.scale3d.to_tuple())}
Path(unreal.Paths.project_dir()+'/DevLog/20261110_HumanoidBossNpc/NPC/scale-diagnostic.json').write_text(json.dumps(out,indent=2))
