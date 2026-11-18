"""Read-only public Python-binding preflight in the owned partial editor."""
import json
import os
from pathlib import Path
import re
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
OUT=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/author-live'
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]=='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
assert os.getpid()==json.loads((OUT/'startup.json').read_text())['pid']
assert not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
source=(ROOT/'Scripts/Editor/NaturalWorldDressing20261117.py').read_text()
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
owned=[a for a in actors if a.get_actor_label()=='NW17_H_Enclosure']
assert len(owned)==1
a=owned[0]
components=a.get_components_by_class(u.InstancedStaticMeshComponent)
assert len(components)==1 and components[0].get_instance_count()==0
c=components[0]
before=sorted(p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
report={'pid':os.getpid(),'read_only':True,'actors_created':0,'saved':False,'methods':{},'properties':{}}
for variable,value in (('component',c),('actor',a)):
    report['methods'][variable]={name:callable(getattr(value,name,None)) for name in sorted(set(re.findall(r'\b'+variable+r'\.(\w+)\(',source)))}
for name in ('generate_overlap_events','cast_shadow','visible','hidden_in_game','can_ever_affect_navigation'):
    try:report['properties'][name]={'readable':True,'value':str(c.get_editor_property(name))}
    except Exception as e:report['properties'][name]={'readable':False,'error':str(e)}
after=sorted(p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
report['dirty_set_unchanged']=before==after
assert before==after
with (OUT/'dressing-api-inspection.json').open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
print('NATURAL_DRESSING PUBLIC_BINDINGS_INSPECTED')
