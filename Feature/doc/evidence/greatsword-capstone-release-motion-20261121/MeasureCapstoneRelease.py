"""Read the true held-RMB capstone fixture's world/capsule and displayed pose samples."""
import csv
import json
import math
from pathlib import Path
import sys

PROJECT=next(folder for folder in Path(__file__).resolve().parents if (folder/'Rogue10m.uproject').is_file())


def vec(row, prefix):
    return tuple(float(row[f'{prefix}_{axis}']) for axis in 'xyz')


def offset(a,b):
    return tuple(x-y for x,y in zip(a,b))


def blade(row):
    return offset(vec(row,'blade_tip'),vec(row,'blade_base'))


def angle(a,b):
    dot=sum(x*y for x,y in zip(a,b))/(math.hypot(*a)*math.hypot(*b))
    return math.degrees(math.acos(max(-1.,min(1.,dot))))


def max_step(seq):
    return max(math.dist(a,b) for a,b in zip(seq,seq[1:]))


def examine(path):
    with path.open(newline='',encoding='utf-8-sig') as source:
        data=list(csv.DictReader(source))
    results={}
    for case in sorted({row['case'] for row in data}):
        rows=[r for r in data if r['case']==case]
        released=[r for r in rows if r['phase']=='release' and float(r['montage_s'])>=0.]
        if not released: continue
        held=[r for r in rows if r['phase']=='hold']
        ready=next((r for r in rows if r['phase']=='ready'),held[0])
        recovery=next(r for r in rows if r['phase']=='recovery')
        last=held[-1]
        first=released[0]
        actor=vec(ready,'actor')
        nearest=min(released,key=lambda r:float(r['blade_capsule_gap_cm']))
        mature=[r for r in released if float(r['montage_weight'])>=.95 and .25<=float(r['montage_s'])<=1.64]
        if not mature: mature=released
        wr=[offset(vec(r,'hand_r'),vec(r,'pelvis')) for r in released]
        wl=[offset(vec(r,'hand_l'),vec(r,'pelvis')) for r in released]
        results[case]={
            'held_samples':len(held), 'first_advanced_montage_s':float(first['montage_s']),
            'first_damage_montage_s':float(first['damage_montage_s']),
            'damage_amount':float(last['health'])-float(first['health']),
            'additional_health_drops':sum(float(a['health'])-float(b['health'])>.01 for a,b in zip(released,released[1:])),
            'advance_wrist_l_cm':math.dist(vec(last,'hand_l'),vec(first,'hand_l')),
            'advance_wrist_r_cm':math.dist(vec(last,'hand_r'),vec(first,'hand_r')),
            'advance_blade_deg':angle(blade(last),blade(first)),
            'release_samples':len(released),
            'full_span_wrist_peak_cm':max(float(r['hand_span_cm']) for r in released),
            'full_span_palm_peak_cm':max(float(r['palm_span_cm']) for r in released),
            'mature_palm_peak_cm':max(float(r['palm_span_cm']) for r in mature),
            'mature_samples':len(mature),
            'mature_samples_with_palm_over_33cm':sum(float(r['palm_span_cm'])>33 for r in mature),
            'largest_step_blade_deg':max(angle(blade(a),blade(b)) for a,b in zip(released,released[1:])),
            'largest_pelvis_relative_wrist_r_step_cm':max_step(wr), 'largest_pelvis_relative_wrist_l_step_cm':max_step(wl),
            'closest_capsule_gap_cm':float(nearest['blade_capsule_gap_cm']),
            'closest_capsule_montage_s':float(nearest['montage_s']),
            'closest_capsule_elapsed_since_first_display_s':float(nearest['world_s'])-float(first['world_s']),
            'min_blade_above_capsule_floor_cm':min(float(r['blade_floor_cm']) for r in released),
            'max_actor_travel_cm':max(math.dist(actor,vec(r,'actor')) for r in rows),
            'recovered_wrist_l_vs_ready_cm':math.dist(vec(recovery,'hand_l'),vec(ready,'hand_l')),
            'recovered_wrist_r_vs_ready_cm':math.dist(vec(recovery,'hand_r'),vec(ready,'hand_r')),
            'recovered_blade_vs_ready_deg':angle(blade(recovery),blade(ready)),
        }
    return results


if __name__=='__main__':
    for label in sys.argv[1:]:
        folder=PROJECT/'tmp/MeleeVfxQARuns'/label
        result=examine(folder/'capstone-brace.csv')
        text=json.dumps(result,indent=2)+'\n'
        (folder/'release-metrics.json').write_text(text,encoding='utf-8')
        print(text)
