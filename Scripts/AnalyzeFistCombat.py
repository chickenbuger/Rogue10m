"""Summarize real Rogue10m.TestFistCombat CSV evidence without changing assets.

Bone-length gates and finite values are structural checks, not a naturalness score.
Review footage and high angular steps separately, especially relaxed-idle transitions.
"""
import argparse
import csv
import json
import math
from pathlib import Path


def rows(path):
    with path.open(encoding='utf-8-sig', newline='') as stream:
        yield from csv.DictReader(stream)


def angular(a, b):
    norm=math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1.,abs(sum(x*y for x,y in zip(a,b)))/norm)))


def analyse(folder):
    frames={int(r['frame']):r for r in rows(folder/'frames.csv')}
    camera=list(rows(folder/'camera.csv'))
    fixed={'spine_01','thigh_l','thigh_r','calf_l','calf_r','foot_l','foot_r','lowerarm_l','lowerarm_r','hand_l','hand_r'}
    stats={}; bad=0; count=0
    for r in rows(folder/'poses.csv'):
        count+=1
        numeric=[float(v) for k,v in r.items() if k not in {'stage','bone','parent'}]
        bad+=sum(not math.isfinite(v) for v in numeric)
        bone=r['bone'].lower(); key=r['stage']+'/'+bone
        if bone not in fixed|{'head'}: continue
        s=stats.setdefault(key,{'min_length_ratio':1.,'max_length_ratio':1.,'max_scale_error':0.,'max_rotation_step_deg':0.,'max_step_frame':0})
        ratio=float(r['length_ratio']); s['min_length_ratio']=min(s['min_length_ratio'],ratio);s['max_length_ratio']=max(s['max_length_ratio'],ratio)
        s['max_scale_error']=max(s['max_scale_error'],*(abs(float(r['local_s'+a])-1) for a in 'xyz'))
        step=float(r['step_deg'])
        if step>s['max_rotation_step_deg']:
            s['max_rotation_step_deg']=step;s['max_step_frame']=int(r['frame'])
    first_camera=[r for r in camera if r['third']=='0']
    maxima={k:max(float(r[k]) for r in camera) for k in camera[0] if k not in {'frame','third'}}
    attacks={}
    previous=None
    for frame,r in frames.items():
        q=tuple(float(r['view_q'+a]) for a in 'xyzw')
        if previous and r['third']=='0' and previous['third']=='0' and r['montage']:
            key=r['montage'].split('/')[-1].split('.')[0]
            s=attacks.setdefault(key,{'samples':0,'max_camera_rotation_step_deg':0.,'max_camera_translation_step_cm':0.})
            s['samples']+=1
            s['max_camera_rotation_step_deg']=max(s['max_camera_rotation_step_deg'],angular(q,tuple(float(previous['view_q'+a]) for a in 'xyzw')))
            s['max_camera_translation_step_cm']=max(s['max_camera_translation_step_cm'],math.sqrt(sum((float(r['view_t'+a])-float(previous['view_t'+a]))**2 for a in 'xyz')))
        previous=r
    structural=bad==0 and all(s['max_scale_error']<.01 and (key.split('/')[1]=='head' or .99<=s['min_length_ratio']<=s['max_length_ratio']<=1.01) for key,s in stats.items())
    return {'capture':folder.name,'frames':len(frames),'pose_rows':count,'nonfinite_values':bad,'structural_pass':structural,
            'first_person_samples':len(first_camera),'third_person_samples':len(camera)-len(first_camera),
            'camera_maxima':maxima,'bones':stats,'first_person_attack_camera_steps':attacks,'skills':list(rows(folder/'skills.csv'))}


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('captures',nargs='+',type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
    reports=[analyse(folder) for folder in a.captures]
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps({'note':__doc__,'captures':reports},ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps([{'capture':r['capture'],'structural_pass':r['structural_pass'],'frames':r['frames']} for r in reports]))
    if not all(r['structural_pass'] for r in reports): raise SystemExit(1)
