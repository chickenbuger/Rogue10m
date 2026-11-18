"""Read-only final primary Editor state after reviewed normal PIE traversal."""
from pathlib import Path
import json
import os
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118'
assert json.loads((BASE/'runtime-camera-verified/result.json').read_text())['passed'] is True
assert not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
dirty=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
assert not dirty
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
assert n['success'] and n['composite_blake3']=='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
assert n['original_layer_blake3']=='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'
assert n['component_count']==n['collision_count']==45
actors=list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
assert len(actors)==1408
with (BASE/'final-editor-state.json').open('x',encoding='utf-8') as f:
    json.dump({'passed':True,'pid':os.getpid(),'PIE':False,'dirty_packages':dirty,'actors':len(actors),
       'composite_blake3':n['composite_blake3'],'original_layer_blake3':n['original_layer_blake3'],
       'landscape_components':45,'collision_components':45,'mutations':False},f,indent=2)
print('CAVES FINAL_PRIMARY_EDITOR_CLEAN_PRESERVED')
