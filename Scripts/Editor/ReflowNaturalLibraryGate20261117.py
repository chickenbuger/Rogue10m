"""One planned 2m rigid library move to clear the measured T/E17 approach."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
BASE=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117'
OUT=BASE/'author-live'
GROUP='WorldLayout20261112/Architecture/WL26_T_OpenLibrary'
sys.dont_write_bytecode=True
assert os.getpid()==json.loads((OUT/'startup.json').read_text())['pid']
assert not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]=='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
assert not (OUT/'library-gate-reflow-preflight.json').exists()
old=json.loads((BASE/'author-plan.json').read_text())
new=json.loads((BASE/'author-plan-gate-reflow.json').read_text())
oldrows={r['path']:r for r in old['moves'] if r['group']==GROUP}
newrows={r['path']:r for r in new['moves'] if r['group']==GROUP}
assert len(oldrows)==len(newrows)==8 and oldrows.keys()==newrows.keys()
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
actual={a.get_path_name():a for a in actors.get_all_level_actors()}
for name,row in oldrows.items():
    a=actual[name];p=a.get_actor_location();target=newrows[name]['after_location']
    assert str(a.get_folder_path())==GROUP and max(abs(x-y) for x,y in zip([p.x,p.y,p.z],row['after_location']))<.02
    assert target==[row['after_location'][0]-200,row['after_location'][1],row['after_location'][2]]
with (OUT/'library-gate-reflow-preflight.json').open('x') as f:json.dump({'pid':os.getpid(),'group':GROUP,'parts':8,'delta_cm':[-200,0,0],'reason':new['gate_reflow_reason']},f,indent=2)
result=[]
with u.ScopedEditorTransaction('Clear T E17 approach by rigid library reflow'):
    for name,row in newrows.items():
        a=actual[name];a.modify();a.root_component.modify()
        a.set_actor_location(u.Vector(*row['after_location']),False,False)
        p=a.get_actor_location()
        assert max(abs(x-y) for x,y in zip([p.x,p.y,p.z],row['after_location']))<.02
        result.append({'actor':name,'location':[p.x,p.y,p.z]})
with (OUT/'library-gate-reflow-result.json').open('x') as f:json.dump({'parts':result,'saved':False},f,indent=2)
spec=importlib.util.spec_from_file_location('natural_reflow_validator',ROOT/'Scripts/Editor/ValidateNaturalWorld20261117.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
validation=m.validate_editor()
with (OUT/'validation-gate-reflow.json').open('x',encoding='utf-8') as f:json.dump(validation,f,ensure_ascii=False,indent=2)
assert validation.get('passed'),validation.get('diagnostics')
spec=importlib.util.spec_from_file_location('natural_reflow_capsules',ROOT/'Scripts/Editor/ValidateNaturalCapsules20261117.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
capsules=c.validate_capsules()
with (OUT/'capsule-validation-gate-reflow.json').open('x',encoding='utf-8') as f:json.dump(capsules,f,ensure_ascii=False,indent=2)
assert capsules.get('passed'),capsules.get('failures')
print('NATURAL_WORLD LIBRARY_REFLOW_STRUCTURAL_AND_CAPSULES_PASS')
