"""Save only the Q dressing actor after unchanged Pawn QA and actual Camera14 arc."""
from pathlib import Path
import json
import unreal as u
BASE=Path('D:/Project/Rogue10m/Feature/doc/evidence/caves20261118');OUT=BASE/'author-live'
assert not (OUT/'camera-relief-save-result.json').exists()
assert json.loads((BASE/'qa-camera-final-live/result.json').read_text())['passed'] is True
assert json.loads((BASE/'camera-arc-after.json').read_text())['passed'] is True
assert json.loads((OUT/'camera-relief.json').read_text())['success'] is True
assert not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
dirty=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
assert dirty==['/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/7/0E/NL2M3149WONPRWLKFKVP6Y']
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
assert n['success'] and n['composite_blake3']=='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
assert n['original_layer_blake3']=='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'
plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1',
    'map':'/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A','confirm_save':True,
    'expected_composite_blake3':n['composite_blake3'],'expected_original_layer_blake3':n['original_layer_blake3'],'packages':dirty}
with (OUT/'camera-relief-save-plan.json').open('x',encoding='utf-8') as f:json.dump(plan,f,indent=2)
result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
with (OUT/'camera-relief-save-result.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
assert result['success'] and not result['dirty_packages']
print('CAVES EXACT1_Q_CAMERA_RELIEF_SAVED_DIRTY0')
