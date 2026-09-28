"""Promote the validated Boxing retarget to the gameplay asset folder using Unreal APIs."""
from pathlib import Path
import json, math
import unreal
ROOT=Path(unreal.Paths.project_dir())
SOURCE='/Game/Rogue10m/Animation/Preview/Boxing/A_Boxing_Manny'
DEST='/Game/Rogue10m/Animation/Combat/Boxing/A_BoxingJab_Manny'
def main():
 src=unreal.EditorAssetLibrary.load_asset(SOURCE)
 if not isinstance(src,unreal.AnimSequence): raise RuntimeError('Validated Boxing retarget missing')
 dst=unreal.EditorAssetLibrary.load_asset(DEST) if unreal.EditorAssetLibrary.does_asset_exist(DEST) else unreal.EditorAssetLibrary.duplicate_asset(SOURCE,DEST)
 if not isinstance(dst,unreal.AnimSequence): raise RuntimeError('Gameplay sequence creation failed')
 if dst.get_editor_property('skeleton')!=src.get_editor_property('skeleton'): raise RuntimeError('Skeleton mismatch')
 if abs(dst.get_play_length()-src.get_play_length())>1e-6: raise RuntimeError('Duration mismatch')
 mesh=unreal.EditorAssetLibrary.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
 opt=unreal.AnimPoseEvaluationOptions();opt.set_editor_property('optional_skeletal_mesh',mesh)
 max_pos=0.;max_rot=0.;count=0
 for frame in range(53):
  t=min(frame/30.,src.get_play_length())
  a=unreal.AnimPoseExtensions.get_anim_pose_at_time(src,t,opt);b=unreal.AnimPoseExtensions.get_anim_pose_at_time(dst,t,opt)
  for bone in ['root','pelvis','head','hand_l','hand_r','foot_l','foot_r']:
   x=unreal.AnimPoseExtensions.get_bone_pose(a,bone,unreal.AnimPoseSpaces.WORLD);y=unreal.AnimPoseExtensions.get_bone_pose(b,bone,unreal.AnimPoseSpaces.WORLD)
   max_pos=max(max_pos,math.sqrt(sum((getattr(x.translation,k)-getattr(y.translation,k))**2 for k in ['x','y','z'])))
   max_rot=max(max_rot,max(abs(getattr(x.rotation,k)-getattr(y.rotation,k)) for k in ['x','y','z','w']));count+=1
 if max_pos>1e-5 or max_rot>1e-5: raise RuntimeError('Gameplay copy changed source poses')
 unreal.EditorAssetLibrary.set_metadata_tag(dst,'Rogue10m.SourcePreview',SOURCE)
 unreal.EditorAssetLibrary.set_metadata_tag(dst,'Rogue10m.Usage','Basic unarmed left jab; runtime mirrors right jab; source peak 13/30 seconds')
 if not unreal.EditorAssetLibrary.save_loaded_asset(dst,False): raise RuntimeError('Gameplay sequence save failed')
 evidence=ROOT/'Feature/doc/evidence/boxing-basic-20260923';evidence.mkdir(parents=True,exist_ok=True)
 (evidence/'asset-validation.json').write_text(json.dumps(dict(source=SOURCE,destination=DEST,samples=count,duration=dst.get_play_length(),max_position_error_cm=max_pos,max_quaternion_component_error=max_rot),indent=2))
 unreal.log('RESULT=BOXING_GAMEPLAY_ASSET_PASSED')
if __name__=='__main__':
 try:main()
 finally:unreal.SystemLibrary.quit_editor()
