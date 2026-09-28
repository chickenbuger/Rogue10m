"""Read-only validation of the baked Manny Martelo preview animation."""
import json
import math
from pathlib import Path
import unreal

ASSET='/Game/Rogue10m/Animation/Preview/Martelo/A_Martelo2_Manny'
MESH='/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'

def main():
    sequence=unreal.load_asset(ASSET)
    mesh=unreal.load_asset(MESH)
    if not sequence or not mesh:
        raise RuntimeError('Missing preview sequence or Manny mesh')
    if sequence.get_editor_property('skeleton') != mesh.get_editor_property('skeleton'):
        raise RuntimeError('Martelo preview does not use the Manny skeleton')
    duration=sequence.get_play_length()
    if abs(duration-1.3)>0.05:
        raise RuntimeError(f'Unexpected clip duration: {duration}')
    options=unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh',mesh)
    rows=[]
    bone_names=['root','pelvis','head','upperarm_l','lowerarm_l','hand_l','upperarm_r','lowerarm_r','hand_r','thigh_l','calf_l','foot_l','thigh_r','calf_r','foot_r']
    for frame in range(40):
        time=min(frame/30,duration)
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence,time,options)
        available=[str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
        if any(n not in available for n in bone_names):
            raise RuntimeError('Required target bone absent from evaluated pose')
        row={'frame':frame,'time':time,'bones':{}}
        for name in bone_names:
            transform=unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.WORLD)
            p=transform.translation
            q=transform.rotation
            scale=transform.scale3d
            if not all(math.isfinite(v) for v in [p.x,p.y,p.z,q.x,q.y,q.z,q.w,scale.x,scale.y,scale.z]):
                raise RuntimeError(f'Nonfinite retarget transform: {name}/{frame}')
            row['bones'][name]=[p.x,p.y,p.z]
        rows.append(row)
    spans={name:max(r['bones'][name][2] for r in rows)-min(r['bones'][name][2] for r in rows) for name in ['foot_l','foot_r','hand_l','hand_r']}
    if spans['foot_l']<60 or spans['foot_l']<spans['foot_r']*2:
        raise RuntimeError(f'Left kick lost during retarget: {spans}')
    result={'asset':ASSET,'duration':duration,'vertical_spans_cm':spans,'samples':rows}
    output=Path(unreal.Paths.project_dir())/'Feature/doc/evidence/martelo-first-person-20260922/retarget-poses.json'
    output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.log('RESULT=MARTELO_RETARGET_POSES_PASSED '+str(spans))

if __name__=='__main__':
    main()
