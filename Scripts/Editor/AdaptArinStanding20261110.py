"""Adapt retargeted Mixamo upper body to Arin's preserved metric rig and grounded idle."""
import unreal,sys,json,math
from pathlib import Path
PROJECT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(PROJECT/'Scripts/Editor'))
import PrepareArinStanding20261110 as job
from PrepareFighterCombatStance import transform,native
BASE='/Game/Rogue10m/Characters/Arin/RefinedV3/ArinIdle'
TARGET=job.ROOT+'/A_Arin_BreathingIdle'

def slerp(a,b,t):
    dot=sum(x*y for x,y in zip(a,b))
    if dot<0: b=[-x for x in b];dot=-dot
    dot=min(1.,dot)
    if dot>.9995: q=[x+(y-x)*t for x,y in zip(a,b)]
    else:
        angle=math.acos(dot); den=math.sin(angle)
        q=[(math.sin((1-t)*angle)*x+math.sin(t*angle)*y)/den for x,y in zip(a,b)]
    norm=math.sqrt(sum(x*x for x in q));return [x/norm for x in q]

def main():
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET): raise RuntimeError('Preserve existing final candidate; use new name to revise')
    base=job.require(BASE);raw=job.require(job.ROOT+'/A_Breathing_Arin_Raw');mesh=job.require(job.ARIN)
    opts=unreal.AnimPoseEvaluationOptions();opts.optional_skeletal_mesh=mesh
    bp=unreal.AnimPoseExtensions.get_anim_pose_at_time(base,0,opts)
    names=[str(n) for n in unreal.AnimPoseExtensions.get_bone_names(bp)]
    baseline={n:transform(unreal.AnimPoseExtensions.get_bone_pose(bp,n,unreal.AnimPoseSpaces.LOCAL)) for n in names}
    frames=round(raw.get_play_length()*30)
    keys={n:{'p':[],'q':[],'s':[]} for n in names};first={}
    weights={'spine_01':.35,'spine_02':.4,'spine_03':.4,'neck_01':.35,'head':.35,'clavicle_l':.7,'clavicle_r':.7,'upperarm_l':.85,'upperarm_r':.85,'lowerarm_l':.85,'lowerarm_r':.85,'hand_l':.7,'hand_r':.7}
    for f in range(frames+1):
        t=f/30.;pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(raw,min(t,raw.get_play_length()),opts)
        for n in names:
            tr=baseline[n]; q=tr['q']
            if n in weights:
                source=transform(unreal.AnimPoseExtensions.get_bone_pose(pose,n,unreal.AnimPoseSpaces.LOCAL))
                q=slerp(q,source['q'],weights[n])
                if f==0:first[n]=q
                # Smoothly return to first pose over final 0.8s; exact seam.
                u=max(0.,min(1.,(t-(raw.get_play_length()-.8))/.8));u=u*u*(3-2*u)
                q=slerp(q,first[n],u)
            keys[n]['p'].append(unreal.Vector(*tr['p']));keys[n]['q'].append(unreal.Quat(*q));keys[n]['s'].append(unreal.Vector(*tr['s']))
    target=unreal.EditorAssetLibrary.duplicate_asset(BASE,TARGET)
    ctl=target.get_editor_property('controller');ctl.open_bracket('Arin planted Mixamo breathing idle',False)
    try:
        ctl.remove_all_bone_tracks(False);ctl.set_frame_rate(unreal.FrameRate(30,1),False);ctl.set_number_of_frames(unreal.FrameNumber(frames),False)
        for n,v in keys.items():
            if not ctl.add_bone_curve(n,False) or not ctl.set_bone_track_keys(n,v['p'],v['q'],v['s'],False):raise RuntimeError('Track failure '+n)
    finally:ctl.close_bracket(False)
    target.set_editor_property('enable_root_motion',False)
    unreal.EditorAssetLibrary.set_metadata_tag(target,'Rogue10m.Provenance','Mixamo Breathing Idle -> Manny -> Arin; original Arin lower body/scale, restrained upper body, 0.8s loop seam')
    job.save(target)
    result=job.audit(target,mesh)
    for n in ['root','pelvis','foot_l','foot_r']:
        if max(result['ranges_cm'][n])>.01:raise RuntimeError('Grounded body moved '+n)
    for n in ['head','hand_l','hand_r']:
        if max(result['ranges_cm'][n])<.1:raise RuntimeError('Breathing motion lost '+n)
    report={'status':'numeric_pass_requires_rendered_review','source':raw.get_path_name(),'baseline':BASE,'target':TARGET,'lower_body':'existing accepted Arin idle pose, zero drift','preserved_bones':list(set(names)-set(weights)),'upper_body_source_weights':weights,'duration':target.get_play_length(),'validation':result}
    (job.OUT/'adapted-report.json').write_text(json.dumps(report,indent=2))
    unreal.log('ARIN_ADAPTED_NUMERIC_PASS')

if __name__=='__main__':main()
