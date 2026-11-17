"""Explicit read-only integrated geometry/terrain/capsule/camera-arc gate."""
from pathlib import Path
import importlib.util
import json
import os
import time
import traceback
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';OUT=BASE/'qa-final-live'
assert os.getpid()==49456;OUT.mkdir(exist_ok=False)
s=importlib.util.spec_from_file_location('organic_integrated_qa',ROOT/'Scripts/Editor/OrganicCaves20261118QA.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
start=time.monotonic()
r=m.validate_organic_caves(json.loads((BASE/'validation-config.json').read_text(encoding='utf-8')),require_saved_clean=False)
with(OUT/'validation.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,ensure_ascii=False,indent=2)
if r['passed']:
    arc=m.audit_original_camera_arc()
    with(OUT/'camera-arc.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(arc,f,indent=2)
else:arc={'passed':False,'not_run':'integrated QA failed'}
with(OUT/'result.json').open('x',encoding='utf-8',newline='\n')as f:json.dump({'passed':r['passed'] and arc['passed'],'pid':os.getpid(),'static_passed':r['passed'],'camera_arc_passed':arc['passed'],'elapsed_seconds':time.monotonic()-start,'diagnostics':r.get('diagnostics'),'traceback':r.get('traceback'),'saved':False},f,indent=2)
if r['passed'] and arc['passed']:print('ORGANIC_CAVES_INTEGRATED_STATIC_AND_ARC_PASS')
else:u.log_error('ORGANIC_CAVES_INTEGRATED_FAILED; preserved diagnostics in '+str(OUT))
