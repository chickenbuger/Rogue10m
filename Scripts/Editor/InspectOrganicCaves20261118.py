"""Explicit read-only saved baseline for organic cave refinement."""
from pathlib import Path
import importlib.util
import hashlib
import json
import os
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';OUT=BASE/'baseline-live'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
def module(name,file):
    s=importlib.util.spec_from_file_location(name,ROOT/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def write(name,value):
    with (OUT/(name+'.json')).open('x',encoding='utf-8',newline='\n')as f:json.dump(value,f,ensure_ascii=False,indent=2)
assert not OUT.exists()
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
assert editor.get_editor_world().get_path_name().split('.')[0]==TARGET and not editor.get_game_world()
inspector=module('organic_baseline_inspector','Scripts/Editor/InspectCaveSites20261118.py')
qa=module('organic_prior_verified_qa','Scripts/Editor/ValidateCaves20261118.py')
reader=module('organic_material_reader','Scripts/Editor/NaturalRoadMaterial20261117.py')
actor_reader=module('organic_actor_reader','Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
api=module('organic_scene_api','Scripts/Editor/NaturalWorldDressing20261117.py')
assert not api._dirty(u)
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
assert n['success'] and n['component_count']==n['collision_count']==45
assert n['composite_blake3']=='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
actors=list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors());assert len(actors)==1408
caves=[a for a in actors if a.get_actor_label().startswith('CV18_')];assert len(caves)==4
old=[a for a in actors if a not in caves];old_ref=json.loads((ROOT/'Feature/doc/evidence/caves20261118/baseline-live/baseline-actors.json').read_text(encoding='utf-8'))
assert qa.normalized_addresses(actor_reader._snapshot_actors(u,old,api))==qa.normalized_addresses(old_ref['actors'])
assert qa.normalized_addresses(inspector.persistent_components(u,old))==qa.normalized_addresses(old_ref['persistent_components'])
OUT.mkdir(parents=True)
write('terrain',n)
write('cave-actors',{'actors':actor_reader._snapshot_actors(u,caves,api),'components':inspector.persistent_components(u,caves)})
paths=set(json.loads((ROOT/'Feature/doc/evidence/caves20261118/baseline-live/baseline-materials.json').read_text(encoding='utf-8'))['materials'])
paths.update(json.loads((ROOT/'Feature/doc/evidence/caves20261118/author-live/triplanar.json').read_text(encoding='utf-8'))['expected_material_states_after'])
assert len(paths)==47
write('materials',{'materials':{p:inspector.material_state(u,u.load_asset(p),reader)for p in sorted(paths)}})
components={}
for a in caves:
    rows=[]
    for c in a.get_components_by_class(u.PrimitiveComponent):
        d={'path':c.get_path_name(),'name':c.get_name(),'class':c.get_class().get_path_name(),
           'visible':c.get_editor_property('visible'),'hidden_in_game':c.get_editor_property('hidden_in_game'),
           'collision':str(c.get_collision_enabled()),'materials':[inspector.path(c.get_material(i))for i in range(c.get_num_materials())]}
        if isinstance(c,u.InstancedStaticMeshComponent):d.update(instances=c.get_instance_count(),mesh=inspector.path(c.get_editor_property('static_mesh')))
        rows.append(d)
    components[a.get_actor_label()]=rows
write('component-inventory',components)
assert not api._dirty(u)
write('result',{'passed':True,'pid':os.getpid(),'map':TARGET,'actors':1408,'cave_actor_paths':[a.get_path_name()for a in caves],
    'original1404_preserved':True,'material_count':47,'dirty_packages':[],'assets_saved':False,'mutations':False,
    'command_line':u.SystemLibrary.get_command_line(),'engine_version':u.SystemLibrary.get_engine_version()})
print('ORGANIC_CAVES_READ_ONLY_BASELINE_PASS')
