"""Save an explicitly numeric checkpoint; visual acceptance remains pending.

This is not the visual-review signal and never claims image review. It checks
the completed strict collision receipt and reruns structural QA before the
native exact-loaded-package save. No broad SaveAll is used.
"""
import importlib.util
import json
import os
from pathlib import Path
import sys
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
OUT=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/author-live'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
sys.dont_write_bytecode=True
assert os.getpid()==json.loads((OUT/'startup.json').read_text())['pid']
assert not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert not (OUT/'save-result.json').exists()
caps=json.loads((OUT/'capsule-validation-gate-reflow.json').read_text())
assert caps['passed'] and len(caps['results'])==544 and not caps['failures']
spec=importlib.util.spec_from_file_location('natural_checkpoint_validator',ROOT/'Scripts/Editor/ValidateNaturalWorld20261117.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
validation=m.validate_editor()
with (OUT/'validation-before-numeric-save.json').open('x',encoding='utf-8') as f:json.dump(validation,f,ensure_ascii=False,indent=2)
assert validation['passed'],validation.get('diagnostics')
native=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate())
before=json.loads((OUT/'native-before.json').read_text());applied=json.loads((OUT/'apply-height.json').read_text())
assert native['success'] and native['landscape_reports_up_to_date']
assert native['original_layer_blake3']==before['original_layer_blake3'] and native['composite_blake3']==applied['target_composite_blake3']
packages=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1','map':TARGET,'confirm_save':True,
      'expected_composite_blake3':native['composite_blake3'],'expected_original_layer_blake3':before['original_layer_blake3'],
      'packages':packages}
with (OUT/'save-plan.json').open('x',encoding='utf-8') as f:json.dump(plan,f,indent=2)
result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
with (OUT/'save-result.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
assert result['success'],result.get('error')
with (OUT/'numeric-checkpoint.json').open('x') as f:json.dump({'saved':True,'structural_checks':len(validation['checks']),
    'capsule_cases_passed':544,'visual_reviewed':False,'image_transmission_approval':'pending user response',
    'acceptance_complete':False,'save_all_dirty_called':False},f,indent=2)
print('NATURAL_WORLD NUMERIC_CHECKPOINT_SAVED; VISUAL_ACCEPTANCE_PENDING')
