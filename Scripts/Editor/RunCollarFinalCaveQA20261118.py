"""Run one read-only integrated cave QA after the owner explicitly invokes it."""
from pathlib import Path
import importlib.util
import json
import os
import time
import traceback
import unreal as u

ROOT=Path('D:/Project/Rogue10m');OUT=ROOT/'Feature/doc/evidence/caves20261118/qa-collar-final-live'
OUT.mkdir(exist_ok=False)
state={'started':time.monotonic(),'busy':False,'ticks':0}
def tick(_):
    if state['busy']:return
    state['busy']=True
    try:
        state['ticks']+=1
        if state['ticks']<10:return
        u.unregister_slate_post_tick_callback(state['handle'])
        spec=importlib.util.spec_from_file_location('cave_collar_final_integrated_read_only_qa',ROOT/'Scripts/Editor/ValidateCaves20261118.py')
        v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
        result=v.validate_caves(require_saved_clean=False)
        with (OUT/'validation.json').open('x',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2)
        with (OUT/'result.json').open('x',encoding='utf-8') as f:
            json.dump({'passed':result['passed'],'pid':os.getpid(),'elapsed_wall_seconds':time.monotonic()-state['started'],
                       'assets_saved':False,'diagnostics':result.get('diagnostics'),'traceback':result.get('traceback')},f,indent=2)
        if result['passed']:print('CAVES INTEGRATED_STATIC_QA_PASS')
        else:u.log_error('CAVES INTEGRATED_STATIC_QA_FAILED; original report retained')
    except Exception:
        with (OUT/'failure.json').open('x',encoding='utf-8') as f:json.dump({'traceback':traceback.format_exc()},f,indent=2)
        u.log_error('CAVES INTEGRATED_STATIC_QA '+traceback.format_exc())
    finally:state['busy']=False

state['handle']=u.register_slate_post_tick_callback(tick)
print('CAVES INTEGRATED_STATIC_QA_ARMED')
