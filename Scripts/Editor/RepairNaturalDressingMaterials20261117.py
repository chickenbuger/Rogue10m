"""Run the reviewed five-derivative fix in the same owned, idle editor."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
OUT=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/author-live'
sys.dont_write_bytecode=True
assert os.getpid()==json.loads((OUT/'startup.json').read_text())['pid']
assert not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert not (OUT/'ism-material-repair.json').exists()
spec=importlib.util.spec_from_file_location('natural_owned_material_repair',ROOT/'Scripts/Editor/NaturalDressingMaterialDerivatives20261117.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
bindings=m.inspect_api_bindings()
with (OUT/'ism-material-bindings.json').open('x') as f:json.dump(bindings,f,indent=2)
try:
    report=m.repair_material_bindings(allow_editor_mutation=True)
except m.MaterialDerivativeError as e:
    with (OUT/'ism-material-repair-failure.json').open('x',encoding='utf-8') as f:json.dump(e.report,f,ensure_ascii=False,indent=2)
    raise RuntimeError('Material repair stopped; inspect exact partial report') from None
with (OUT/'ism-material-repair.json').open('x',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
assert report['success']
spec=importlib.util.spec_from_file_location('natural_owned_material_validator',ROOT/'Scripts/Editor/ValidateNaturalWorld20261117.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
validation=v.validate_editor()
with (OUT/'validation-owned-materials.json').open('x',encoding='utf-8') as f:json.dump(validation,f,ensure_ascii=False,indent=2)
assert validation['passed'],validation.get('diagnostics')
print('NATURAL_WORLD OWNED_MATERIALS_SOURCE_PRESERVATION_PASS')
