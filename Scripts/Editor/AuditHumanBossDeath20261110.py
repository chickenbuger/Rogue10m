"""Read-only source animation audit for a complete humanoid fall. Never changes assets."""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
base='/Game/CombatMagicAnims/Demo/Mannequins/Anims/Death/'
rows=[]
for name in ['MM_Death_Back_01','MM_Death_Front_01','MM_Death_Front_02','MM_Death_Front_03','MM_Death_Left_01','MM_Death_Right_01']:
    clip=unreal.load_asset(base+name)
    if not clip: continue
    opt=unreal.AnimPoseEvaluationOptions()
    values=[]
    for t in [0.,clip.get_play_length()*.5,max(0.,clip.get_play_length()-1/30)]:
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,t,opt)
        b={n:unreal.AnimPoseExtensions.get_bone_pose(pose,n,unreal.AnimPoseSpaces.WORLD).translation.to_tuple() for n in ['head','pelvis','foot_l','foot_r']}
        values.append({'t':t,'bones':b,'head_above_low_foot_cm':b['head'][2]-min(b['foot_l'][2],b['foot_r'][2])})
    rows.append({'source':clip.get_path_name(),'length':clip.get_play_length(),'samples':values})
out=ROOT/'Feature/doc/evidence/humanoid-boss-npc-20261110/death-source-audit.json'
out.write_text(json.dumps(rows,indent=2),encoding='utf-8')
for r in rows: print(r['source'],r['length'],r['samples'][-1]['head_above_low_foot_cm'])
print('RESULT=DEATH_SOURCE_AUDIT_PASSED')
