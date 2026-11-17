"""Bounded final cave checkpoint after integrated static QA; no SaveAll."""
from pathlib import Path
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
assert not (OUT/'final-save-result.json').exists()
qa=json.loads((BASE/'qa-collar-final-live/result.json').read_text());assert qa['passed'] is True
height=json.loads((OUT/'apply-height-collar.json').read_text());assert height['success']
structure=json.loads((OUT/'structures.json').read_text());dress=json.loads((OUT/'dressing.json').read_text())
assert structure['success'] and dress['success'] and json.loads((OUT/'triplanar.json').read_text())['success']
target='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==target and not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
dirty=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+
              list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
surface='/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/Caves20261118/M_CaveSurface'
expected=set(height['dirty_packages'])
assert set(dirty)==expected and len(dirty)==17, 'Exact17 collar terrain packages required after the pre-collar checkpoint'
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
assert n['success'] and n['composite_blake3']==height['target_composite_blake3']
assert n['original_layer_blake3']=='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'
plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1','map':target,'confirm_save':True,
      'expected_composite_blake3':n['composite_blake3'],'expected_original_layer_blake3':n['original_layer_blake3'],'packages':dirty}
with (OUT/'final-save-plan.json').open('x',encoding='utf-8') as f:json.dump(plan,f,indent=2)
result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
with (OUT/'final-save-result.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
assert result['success'],result.get('error')
after=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+
              list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
assert not after
with (OUT/'saved-checkpoint.json').open('x',encoding='utf-8') as f:
    json.dump({'saved':True,'packages':17,'dirty':after,'static_qa_passed':True,
               'actual_runtime_camera_pending':True,'cold_pending':True,'acceptance_complete':False},f,indent=2)
print('CAVES EXACT17_FINAL_SAVE_DIRTY0_RUNTIME_AND_COLD_PENDING')
