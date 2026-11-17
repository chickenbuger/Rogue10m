"""Post-PIE read-only return-to-editor and preserved-input check."""
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';OUT=BASE/'final-editor-state.json'
assert os.getpid()==49456 and not OUT.exists()
runtime=json.loads((BASE/'runtime-camera-verified/result.json').read_text());assert runtime['passed']
assert json.loads((BASE/'scope-final.json').read_text())['passed']
def module(n,p):
    s=importlib.util.spec_from_file_location(n,ROOT/p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
api=module('organic_final_api','Scripts/Editor/NaturalWorldDressing20261117.py')
inspector=module('organic_final_inspector','Scripts/Editor/InspectCaveSites20261118.py')
reader=module('organic_final_material_reader','Scripts/Editor/NaturalRoadMaterial20261117.py')
old=module('organic_final_previous_qa','Scripts/Editor/ValidateCaves20261118.py')
actor_reader=module('organic_final_actor_reader','Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem);world=editor.get_editor_world()
assert world.get_path_name().split('.')[0]=='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A' and not editor.get_game_world()
assert not api._dirty(u)
actors=list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors());assert len(actors)==1408
baseline=json.loads((ROOT/'Feature/doc/evidence/caves20261118/baseline-live/baseline-actors.json').read_text())
preserved=[a for a in actors if a.get_path_name()in baseline['actors']];assert len(preserved)==1404
assert old.normalized_addresses(actor_reader._snapshot_actors(u,preserved,api))==old.normalized_addresses(baseline['actors'])
assert old.normalized_addresses(inspector.persistent_components(u,preserved))==old.normalized_addresses(baseline['persistent_components'])
materials=json.loads((BASE/'baseline-live/materials.json').read_text())['materials']
assert len(materials)==47 and all(inspector.material_state(u,u.load_asset(p),reader)==v for p,v in materials.items())
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
assert n['success'] and n['composite_blake3']=='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
assert n['original_layer_blake3']=='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'
assert n['component_count']==n['collision_count']==45 and n['landscape_reports_up_to_date']
assert not api._dirty(u)
r={'passed':True,'pid':os.getpid(),'map':world.get_path_name(),'PIE':False,'game_world':False,'actors':1408,
   'original1404_actors_and_persistent_components_preserved':True,'original47_material_states_preserved':True,
   'terrain_render_components':45,'terrain_collision_components':45,'composite_blake3':n['composite_blake3'],
   'original_layer_blake3':n['original_layer_blake3'],'dirty_packages':[],'assets_saved':False,'read_only':True,
   'runtime_result_sha256':hashlib.sha256((BASE/'runtime-camera-verified/result.json').read_bytes()).hexdigest()}
with OUT.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
print('ORGANIC_FINAL_EDITOR_DIRTY0_PRESERVED1404_47_MATERIALS_TERRAIN45_PASS')
