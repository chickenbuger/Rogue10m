"""Correct only the diagnosed UE Python positional-Rotator axis mapping.

Prevalidates all708 current transforms against the preserved author receipt
before the first update. No spawns, terrain, moves, material or quest edits.
"""
import importlib.util
import json
import math
import os
from pathlib import Path
import sys
import time
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
OUT=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/author-live'
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('natural_rotation_repair',ROOT/'Scripts/Editor/AuthorNaturalWorld20261117.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.context()
m.require(os.getpid()==json.loads((OUT/'startup.json').read_text())['pid'] and not (OUT/'rotation-repair-preflight.json').exists(),'Exact owned one-shot repair')
before=json.loads((OUT/'native-before.json').read_text());applied=json.loads((OUT/'apply-height.json').read_text())
report=json.loads((OUT/'author-content.json').read_text())
old_validation=json.loads((OUT/'validation.json').read_text())
m.require({k for k,v in old_validation['checks'].items() if not v}=={'exact708_instance_transforms_and_settings','thirteen_actual_gate_inner_bounds_14m'},'Unexpected structural failure; preserve state')
native=m.inspect()
m.require(native['composite_blake3']==applied['target_composite_blake3'] and native['original_layer_blake3']==before['original_layer_blake3'],'Terrain changed')
plan=m.read('author-plan.json')
rows={r['label']:r for g in plan['dressing']['groups'] for r in g['instances']}
actors={a.get_path_name():a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()}
ops=[]
for group in report['dressing']['groups']:
    a=actors[group['actor_path']]
    actual={c.get_path_name():c for c in a.get_components_by_class(u.InstancedStaticMeshComponent)}
    for component_report in group['components']:
        c=actual[component_report['path']]
        m.require(callable(getattr(c,'update_instance_transform',None)),'Public ISM update binding required')
        for instance in component_report['instances']:
            index=instance['index'];row=rows[instance['label']]
            tr=c.get_instance_transform(index,world_space=True)
            old=instance['observed_world_transform']
            q=[getattr(tr.rotation,axis) for axis in 'xyzw']
            m.require(max(abs(v-w) for v,w in zip(m.xyz(tr.translation),old['p']))<.02 and
                      max(abs(v-w) for v,w in zip(m.xyz(tr.scale3d),old['s']))<.0001 and
                      abs(abs(sum(v*w for v,w in zip(q,old['q'])))-1)<.0001,'An instance changed after authoring')
            fixed=u.Transform()
            fixed.translation=tr.translation;fixed.scale3d=tr.scale3d
            p,y,r=row['rotation_pitch_yaw_roll']
            fixed.rotation=u.Rotator(pitch=p,yaw=y,roll=r).quaternion()
            m.require(all(math.isfinite(getattr(fixed.rotation,axis)) for axis in 'xyzw'),'Nonfinite target quaternion')
            ops.append((c,index,fixed,instance['label']))
m.require(len(ops)==708 and len({label for _,_,_,label in ops})==708,'Exact708 before any update')
m.write('rotation-repair-preflight',{'pid':os.getpid(),'instances':708,'terrain_unchanged':True,
    'allowed_changes':'Instance quaternion only; explicit Rotator keyword fields','spawns':0,'height_edits':0})
changes=[]
with u.ScopedEditorTransaction('Correct natural-world instance rotation fields'):
    for c,index,fixed,label in ops:
        c.modify()
        m.require(c.update_instance_transform(index,fixed,True,True,True),'ISM transform update failed; no retry')
        tr=c.get_instance_transform(index,world_space=True)
        q=[getattr(tr.rotation,axis) for axis in 'xyzw']
        expected=[getattr(fixed.rotation,axis) for axis in 'xyzw']
        m.require(abs(abs(sum(v*w for v,w in zip(q,expected)))-1)<.0001 and
                  max(abs(v-w) for v,w in zip(m.xyz(tr.translation),m.xyz(fixed.translation)))<.02,
                  'Updated instance readback failed')
        changes.append({'label':label,'component':c.get_path_name(),'index':index,'rotation_xyzw':q})
m.write('rotation-repair-result',{'pid':os.getpid(),'updated_instances':len(changes),'changes':changes,'saved':False})
m.state.update(before=before,apply=applied,content=report,partial_content=report,phase='content_settle',
    ticks=0,started=time.monotonic(),failure_name='rotation-fix-callback-failure',validation_name='validation-rotation-fix')
m.state['handle']=u.register_slate_post_tick_callback(m.tick)
print('NATURAL_WORLD ROTATION_FIELDS_CORRECTED; independent QA pending')
