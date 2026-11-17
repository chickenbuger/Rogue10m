"""Build a reviewed, deterministic actor/instance plan from the read-only baseline."""
import importlib.util
import json
import math
from pathlib import Path
import sys

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"Feature/doc/evidence/landscape-natural-boundary20261117"


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result)
    return result


def build():
    terrain=module('natural_height','Scripts/NaturalLandscapePlan20261117.py')
    old=module('old_scene_pure','Scripts/Editor/AuthorWorldLayoutScene20261112.py')
    dressing=module('natural_dressing','Scripts/NaturalWorldSetDressing20261117.py')
    original=old.build_placement_plan()
    old_terrain=old._load_terrain()
    spec=json.loads((ROOT/'Docs/References/WorldMap/world-layout-20261112.json').read_text(encoding='utf-8-sig'))
    decor=dressing.build_plan(spec,original)
    actors=json.loads((OUT/'baseline-live/actors.json').read_text(encoding='utf-8'))
    source={p['label']:p for p in original['placements']}
    groups={}
    for a in actors:
        if '/Architecture/' in a['folder'] and a['label'] in source:
            groups.setdefault(a['folder'],[]).append(source[a['label']])
    anchors={}
    for name,rows in groups.items():
        floor=next((r for r in rows if r['label'].endswith('_Foundation')),None)
        if floor:
            anchors[name]=[floor['center'][0],floor['center'][1],floor['center'][2]-4]
        else:
            x=sum(r['center'][0] for r in rows)/len(rows);y=sum(r['center'][1] for r in rows)/len(rows)
            site=min(original['sites'],key=lambda s:math.hypot(s['center'][0]-x,s['center'][1]-y))
            anchors[name]=list(site['center'])
    moves=[];retained=[];roads=[]
    for a in actors:
        label=a['label']
        if a['class']=='/Script/Engine.DecalActor' and label.startswith('WL26_RoadSurface20261117_'):
            roads.append(a);continue
        if label.startswith('WL26_Road_') or label=='WL26_PersistentBossGround':
            retained.append(a['path']);continue
        if not label.startswith('WL26_') or 'Rogue10m.WorldLayout20261112.Scene' not in a['tags']:
            retained.append(a['path']);continue
        require_source=(label in source or a['class']=='/Script/Engine.PlayerStart')
        if not require_source:raise RuntimeError('Unrecognized owned actor: '+label)
        x,y,z=a['location']
        if a['folder'] in anchors:
            base=anchors[a['folder']];old_z=base[2];new_z=terrain.height_at(base[0],base[1])
        else:
            row=source.get(label)
            sx,sy=(row['center'][:2] if row else (x,y))
            old_z=old_terrain.height_at(sx,sy);new_z=terrain.height_at(sx,sy)
        after=[x,y,z+new_z-old_z]
        moves.append({'path':a['path'],'label':label,'before_location':a['location'],'after_location':after,
                      'old_ground_reference':old_z,'new_ground_reference':new_z,'group':a['folder'] if a['folder'] in anchors else None})
    if len(roads)!=29 or len(anchors)!=20:raise RuntimeError('Unexpected road/building group count')
    data={'schema':'Rogue10m.NaturalWorldAuthorPlan.20261117.v1','map':terrain.TARGET,'status':'planned_not_authored',
          'base_actor_count':len(actors),'moves':moves,'road_decals':roads,'unchanged_actor_paths':retained,
          'building_group_anchors':anchors,'dressing':decor,'plateau_z_cm':terrain.PLATEAU_Z,
          'director_baseline':json.loads((OUT/'baseline-live/directors.json').read_text()),
          'height_plan':'target-height-plan.json','preserve_original_quest_dataasset':True,
          'road_width_cm':800,'road_projection_half_depth_cm':500,
          'source_original_scene':'Scripts/Editor/AuthorWorldLayoutScene20261112.py'}
    return data


if __name__=='__main__':
    data=build();p=OUT/'author-plan.json'
    with p.open('x',encoding='utf-8') as f:json.dump(data,f,ensure_ascii=False,indent=2,allow_nan=False)
    print(json.dumps({'moves':len(data['moves']),'road_decals':len(data['road_decals']),
                      'unchanged_actors':len(data['unchanged_actor_paths']),'building_groups':len(data['building_group_anchors']),
                      'new_groups':data['dressing']['group_count'],'instances':data['dressing']['instance_count']}))
