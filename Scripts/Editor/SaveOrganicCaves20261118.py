"""Save only the reviewed four cave actor packages and four new assets."""
from pathlib import Path
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';OUT=BASE/'author-live'
assert not(OUT/'final-save-result.json').exists()
assert json.loads((BASE/'qa-final-live/result.json').read_text())['passed']
assert json.loads((BASE/'qa-final-live/render-geometry.json').read_text())['passed']
assert json.loads((OUT/'winding-repair.json').read_text())['passed']
target='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
assert editor.get_editor_world().get_path_name().split('.')[0]==target and not editor.get_game_world()
dirty=lambda:sorted({p.get_name()for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
before=dirty();expected=json.loads((OUT/'scene.json').read_text())['dirty_packages_after']
assert len(before)==8 and set(before)==set(expected)
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
assert n['success'] and n['composite_blake3']=='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
assert n['original_layer_blake3']=='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'
plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1','map':target,'confirm_save':True,
      'expected_composite_blake3':n['composite_blake3'],'expected_original_layer_blake3':n['original_layer_blake3'],'packages':before}
with(OUT/'final-save-plan.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(plan,f,indent=2)
r=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
assert r.get('dirty_before')==before, 'Preserve the native save report and exact pre-save package identities'
with(OUT/'final-save-result.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
assert r['success'] and not dirty()
print('ORGANIC_CAVES_EXACT8_SAVED_DIRTY0_RUNTIME_AND_COLD_PENDING')
