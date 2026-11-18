"""Explicit read-only actual RenderData validation after winding repair."""
from pathlib import Path
import importlib.util
import hashlib
import json
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';p=BASE/'qa-final-live/render-geometry.json';assert not p.exists()
s=importlib.util.spec_from_file_location('organic_actual_render_qa',ROOT/'Scripts/Editor/OrganicCaves20261118RenderQA.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
r=m.validate_render_geometry(json.loads((BASE/'validation-config.json').read_text()),expected_qa_sha256=hashlib.sha256((ROOT/'Scripts/Editor/OrganicCaves20261118QA.py').read_bytes()).hexdigest())
with p.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
print('ORGANIC_ACTUAL_RENDER_GEOMETRY '+str(r['passed']))
