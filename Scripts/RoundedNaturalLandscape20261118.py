"""Rounded perimeter revision: unchanged inner/outer feet and flat playfield."""
import importlib.util
import math
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('natural_height_original_profile',ROOT/'Scripts/NaturalLandscapePlan20261117.py')
BASE=importlib.util.module_from_spec(spec);spec.loader.exec_module(BASE)
PROFILE='20261118-rounded-sine-profile'


def height_at(x,y):
    d=BASE.signed_distance(x,y)
    jitter=150*math.sin(x/1900+y/2700)+100*math.sin(y/1500-x/3800)
    s=d+jitter
    if s<=-1500 or s>=2200:return 9600.0
    amplitude=2600+450*math.sin(x/3300+y/5100)+250*math.cos(y/2600-x/4500)
    t=(s+1500)/3700
    return 9600+amplitude*math.sin(math.pi*t)**2


def build_height_plan():
    data=BASE.build_height_plan()
    data['target_heights']=[BASE.encode(height_at(BASE.ORIGIN[0]+ix*BASE.SPACING,BASE.ORIGIN[1]+iy*BASE.SPACING))
        for iy in range(BASE.BOUNDS[1],BASE.BOUNDS[3]+1) for ix in range(BASE.BOUNDS[0],BASE.BOUNDS[2]+1)]
    data['profile_revision']=PROFILE
    return data


if __name__=='__main__':
    import json
    out=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/target-height-plan-rounded-ridge.json'
    plan=build_height_plan()
    with out.open('x') as f:json.dump(plan,f,separators=(',',':'))
    author=json.loads((out.parent/'author-plan-gate-reflow.json').read_text())
    differences=[]
    for row in author['moves']:
        anchor=author['building_group_anchors'][row['group']] if row['group'] else row['after_location']
        actual=height_at(*anchor[:2])
        if abs(actual-row['new_ground_reference'])>1.01:differences.append({'label':row['label'],'old_ground':row['new_ground_reference'],'new_ground':actual})
    report={'profile':PROFILE,'target_samples':len(plan['target_heights']),
        'target_min_cm':2*(min(plan['target_heights'])-32768),'target_max_cm':2*(max(plan['target_heights'])-32768),
        'actor_ground_reference_differences':differences,'flat_playfield_preserved':not differences,
        'previous_flat_crest_width_cm':850,'new_flat_crest_width_cm':0}
    with (out.parent/'rounded-plan-preflight.json').open('x') as f:json.dump(report,f,indent=2)
    print(json.dumps(report))
