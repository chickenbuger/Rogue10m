"""Read-only idle-editor collision QA, before image-transmission approval."""
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
assert json.loads((OUT/'validation-rotation-fix.json').read_text())['passed']
spec=importlib.util.spec_from_file_location('natural_capsule_qa',ROOT/'Scripts/Editor/ValidateNaturalCapsules20261117.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
report=m.validate_capsules()
with (OUT/'capsule-validation-name-order-fix.json').open('x',encoding='utf-8') as f:json.dump(report,f,ensure_ascii=False,indent=2)
print('NATURAL_WORLD CAPSULE_QA '+str(report.get('passed'))+' '+str(len(report.get('results',[]))))
