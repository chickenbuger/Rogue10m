"""One reviewed recovery in the existing owned, unsaved editor.

Never reapplies heights, repeats moves/material creation, or saves. Removes only
the first empty unsaved group made by the diagnosed factory-root mismatch.
Run explicitly from the Editor's ordinary Python console through Slate UI.
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
spec=importlib.util.spec_from_file_location('natural_reviewed_recovery',ROOT/'Scripts/Editor/AuthorNaturalWorld20261117.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.context()
m.require('-RogueNaturalStage=author' in u.SystemLibrary.get_command_line(),'Exact owned author session required')
startup=json.loads((OUT/'startup.json').read_text())
m.require(startup['pid']==os.getpid() and not (OUT/'recovery-preflight.json').exists(),'Owned PID and one-shot recovery required')
failure=json.loads((OUT/'failure.json').read_text())
partial=failure['partial_content']
m.require(partial is not None and len(partial['moves'])==672 and len(partial['roads'])==29 and partial['quest'] and partial['dressing'] is None,'Exact partial-content boundary required')
m.require(m.dirty()==failure['dirty'],'Editor dirty set changed since the failure; preserve it')
before=json.loads((OUT/'native-before.json').read_text())
applied=json.loads((OUT/'apply-height.json').read_text())
native=m.inspect(True)
m.require(native['composite_blake3']==applied['target_composite_blake3'] and native['original_layer_blake3']==before['original_layer_blake3'] and native['landscape_reports_up_to_date'],'Settled original-preserving target required')
plan=m.read('author-plan.json')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
actual={a.get_path_name():a for a in actors.get_all_level_actors()}
for row in plan['moves']:
    m.require(max(abs(v-w) for v,w in zip(m.xyz(actual[row['path']].get_actor_location()),row['after_location']))<.02,'An authored actor moved after failure')
for row in partial['roads']:
    a=actual[row['path']];c=a.get_component_by_class(u.DecalComponent)
    m.require(m.path(c.get_decal_material())==row['material'] and max(abs(v-w) for v,w in zip(m.xyz(a.get_actor_location()),row['location']))<.02,'Road state changed after failure')
dressing_failure=json.loads((OUT/'dressing-failure.json').read_text())
created=dressing_failure['created_actor_paths']
m.require(len(created)==1 and len(actual)==1391 and dressing_failure['instance_count']==0,'Exactly one empty failed group required')
a=actual[created[0]]
m.require(a.get_actor_label()=='NW17_H_Enclosure' and a.get_class()==u.Actor.static_class() and
          'Rogue10m.NaturalBoundary20261117' in list(map(str,a.tags)) and not a.get_components_by_class(u.InstancedStaticMeshComponent),
          'Failed actor identity/components differ; no deletion')
for asset in (m.QUEST,m.OWN+'/M_RoadFeather'):
    m.require(not (ROOT/'Content'/(asset[6:]+'.uasset')).exists(),'Partial content was saved unexpectedly')
m.write('recovery-preflight',{'pid':os.getpid(),'target_hash':native['composite_blake3'],
    'original_preserved':True,'remove_only_empty_new_actor':m.path(a),'moves_repeated':0,'height_edits_repeated':0,'material_creation_repeated':0})
m.require(actors.destroy_actor(a),'Failed empty new actor could not be removed')
dressing=m.module('natural_fixed_dressing','Scripts/Editor/NaturalWorldDressing20261117.py')
try:
    partial['dressing']=dressing.build_dressing(plan['dressing'],allow_editor_mutation=True)
except dressing.DressingBuildError as e:
    m.write('recovery-dressing-failure',e.report)
    raise RuntimeError('Recovery dressing failed; inspect partial report, no retry') from None
m.write('author-content',partial)
m.state.update(before=before,apply=applied,content=partial,partial_content=partial,
               phase='content_settle',ticks=0,started=time.monotonic(),failure_name='recovery-callback-failure')
m.state['handle']=u.register_slate_post_tick_callback(m.tick)
m.write('recovery-complete',{'pid':os.getpid(),'new_groups':len(partial['dressing']['groups']),
                          'instances':partial['dressing']['instance_count'],'saved':False})
print('NATURAL_WORLD RECOVERY_CONTENT_COMPLETE; waiting for independent structural QA')
