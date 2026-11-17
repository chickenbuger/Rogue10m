"""Scoped recovery after the diagnosed BlueprintSetter Python export mismatch.

Actual live binding preflight must already exist. Replaces only the new H
group with its single zero-instance ISM; never repeats terrain/content moves.
"""
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
OUT=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/author-live'
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('natural_bindings_recovery',ROOT/'Scripts/Editor/AuthorNaturalWorld20261117.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.context()
m.require('-RogueNaturalStage=author' in u.SystemLibrary.get_command_line(),'Exact owned author session required')
m.require(json.loads((OUT/'startup.json').read_text())['pid']==os.getpid() and not (OUT/'recovery2-preflight.json').exists(),'Owned PID and one-shot diagnosed recovery required')
bindings=json.loads((OUT/'dressing-api-inspection.json').read_text())
m.require(bindings['pid']==os.getpid() and bindings['read_only'] and bindings['dirty_set_unchanged'] and
          {k for k,v in bindings['methods']['component'].items() if not v}=={'set_generate_overlap_events'} and
          bindings['properties']['generate_overlap_events']['readable'],'Actual live bindings do not match diagnosis')
failure=json.loads((OUT/'failure.json').read_text())
partial=failure['partial_content']
df=json.loads((OUT/'recovery-dressing-failure.json').read_text())
m.require('has no attribute \'set_generate_overlap_events\'' in df['traceback'] and len(df['groups'])==1 and df['instance_count']==0,'Exact diagnosed zero-instance boundary required')
before=json.loads((OUT/'native-before.json').read_text());applied=json.loads((OUT/'apply-height.json').read_text())
native=m.inspect()
m.require(native['composite_blake3']==applied['target_composite_blake3'] and native['original_layer_blake3']==before['original_layer_blake3'] and native['landscape_reports_up_to_date'],'Settled original-preserving target required')
plan=m.read('author-plan.json');actors=u.get_editor_subsystem(u.EditorActorSubsystem)
actual={a.get_path_name():a for a in actors.get_all_level_actors()}
m.require(len(actual)==1391 and len(df['created_actor_paths'])==1,'Only one partial new actor required')
a=actual[df['created_actor_paths'][0]];components=a.get_components_by_class(u.InstancedStaticMeshComponent)
m.require(a.get_actor_label()=='NW17_H_Enclosure' and a.get_class()==u.Actor.static_class() and
          'Rogue10m.NaturalBoundary20261117' in list(map(str,a.tags)) and len(components)==1 and components[0].get_instance_count()==0,
          'Partial actor/components changed; no deletion')
now=set(m.dirty());old=set(failure['dirty']);added=now-old;removed=old-now
baseline_packages={r['package'] for r in m.read('baseline-live/actors.json')}
ea='/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/'
m.require(added=={a.get_package().get_name()} and len(removed)==1 and all(p.startswith(ea) and p not in baseline_packages for p in removed),
          'Unexpected dirty-set delta since replacing the first empty group')
for row in plan['moves']:
    m.require(max(abs(v-w) for v,w in zip(m.xyz(actual[row['path']].get_actor_location()),row['after_location']))<.02,'Authored actor changed')
for row in partial['roads']:
    value=actual[row['path']];c=value.get_component_by_class(u.DecalComponent)
    m.require(m.path(c.get_decal_material())==row['material'] and max(abs(v-w) for v,w in zip(m.xyz(value.get_actor_location()),row['location']))<.02,'Road changed')
m.write('recovery2-preflight',{'pid':os.getpid(),'native_hash':native['composite_blake3'],
    'remove_only_empty_new_actor':m.path(a),'instances_before':0,'height_reapplied':False,
    'moves_repeated':0,'material_recreated':False,'dirty_added':sorted(added),'dirty_removed':sorted(removed)})
dressing=m.module('natural_bindings_fixed_dressing','Scripts/Editor/NaturalWorldDressing20261117.py')
m.write('recovery2-binding-preflight',dressing._binding_preflight(u))
m.require(actors.destroy_actor(a),'Could not remove the diagnosed empty new group')
try:partial['dressing']=dressing.build_dressing(plan['dressing'],allow_editor_mutation=True)
except dressing.DressingBuildError as e:
    m.write('recovery2-dressing-failure',e.report)
    raise RuntimeError('Diagnosed recovery stopped; preserve report, no automatic retry') from None
m.write('author-content',partial)
m.state.update(before=before,apply=applied,content=partial,partial_content=partial,phase='content_settle',
               ticks=0,started=time.monotonic(),failure_name='recovery2-callback-failure')
m.state['handle']=u.register_slate_post_tick_callback(m.tick)
m.write('recovery2-complete',{'pid':os.getpid(),'groups':len(partial['dressing']['groups']),
    'instances':partial['dressing']['instance_count'],'saved':False})
print('NATURAL_WORLD BINDINGS_RECOVERY_CONTENT_COMPLETE')
