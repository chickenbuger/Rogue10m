"""Save exactly two corrected cave dressing packages after unchanged collision QA."""
from pathlib import Path
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
assert not (OUT/'exposure-save-result.json').exists()
assert json.loads((BASE/'qa-exposure-final-live/result.json').read_text())['passed'] is True
assert json.loads((OUT/'exposure-bounds.json').read_text())['success'] is True
assert json.loads((OUT/'final-save-result.json').read_text())['success'] is True
target='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
assert not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
assert u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]==target
dirty=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
expected=set(json.loads((OUT/'dressing.json').read_text())['packages'])
assert len(expected)==2 and set(dirty)==expected
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
assert n['success'] and n['composite_blake3']=='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
assert n['original_layer_blake3']=='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'
plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1','map':target,'confirm_save':True,
      'expected_composite_blake3':n['composite_blake3'],'expected_original_layer_blake3':n['original_layer_blake3'],'packages':dirty}
with (OUT/'exposure-save-plan.json').open('x',encoding='utf-8') as f:json.dump(plan,f,indent=2)
result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
with (OUT/'exposure-save-result.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
assert result['success'],result.get('error')
assert not list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())
print('CAVES EXACT2_LOCAL_EXPOSURE_SAVE_DIRTY0_RUNTIME_AND_COLD_PENDING')
