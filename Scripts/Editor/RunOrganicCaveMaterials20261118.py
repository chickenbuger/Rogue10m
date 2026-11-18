"""Explicit creation of two project-owned organic rock material derivatives."""
from pathlib import Path
import importlib.util
import json
ROOT=Path('D:/Project/Rogue10m');OUT=ROOT/'Feature/doc/evidence/cave-organic20261118/author-live'
OUT.mkdir(parents=True,exist_ok=True);assert not(OUT/'materials.json').exists()
backup=json.loads((ROOT/'Feature/doc/evidence/cave-organic20261118/baseline-files.json').read_text(encoding='utf-8'))
assert backup['status']=='EXACT1497_PHYSICAL_BACKUP_SHA256_VERIFIED_SCOPED_INPUTS_UNCHANGED'
s=importlib.util.spec_from_file_location('explicit_organic_materials',ROOT/'Scripts/Editor/OrganicCaves20261118Materials.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
try:
    r=m.create_organic_cave_materials(allow_editor_mutation=True,
        expected_spec_sha256='a2498735c25b9c878c2b2de522702e48119ebf66da7cad04e75e40f753804366',
        expected_baseline_materials_sha256='de2e85d4b23ae4ad014d88bcd0bf54d248722ee854e19f808744e5bf71a77afb',
        expected_editor_pid=49456,expected_dirty_packages=[])
except Exception as error:
    with(OUT/'materials-failure.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(getattr(error,'report',{'error':str(error)}),f,ensure_ascii=False,indent=2)
    raise
with(OUT/'materials.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,ensure_ascii=False,indent=2)
assert r['success']
print('ORGANIC_CAVE_TWO_MATERIALS_CREATED_UNSAVED')
