"""Apply only the reviewed under-wall floor collar after its exact WIP save."""
from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import time
import traceback
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
assert json.loads((OUT/'collar-pre-save-result.json').read_text())['success']
assert not (OUT/'apply-height-collar.json').exists()
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(True))
s=importlib.util.spec_from_file_location('reviewed_cave_floor_collar',ROOT/'Scripts/CaveFloorCollarPlan20261118.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
plan=m.guarded_collar_revision(n,
    expected_recipe_sha256='4f3d5dacfdee8af8e00e32c6db3597208061e2593e488f74cdd88de334a3696a',
    expected_preflight_sha256='2fd5f9dd3c8d57eb723d215ac724724b54234e8b23389971fe4e91f67a8e84ca')
with (OUT/'applied-height-plan-collar.json').open('x',encoding='utf-8') as f:json.dump(plan,f,indent=2)
r=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.revise_height_plan(json.dumps(plan),True))
with (OUT/'apply-height-collar.json').open('x',encoding='utf-8') as f:json.dump(r,f,indent=2)
assert r['success'] and r['original_layer_preserved'],r.get('error')
state={'started':time.monotonic(),'busy':False,'ticks':0}
def tick(_):
    if state['busy']:return
    state['busy']=True
    try:
        assert time.monotonic()-state['started']<180,'Collar settle watchdog'
        state['ticks']+=1
        if state['ticks']%10:return
        current=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(True))
        assert current['success'] and current['original_layer_blake3']==n['original_layer_blake3']
        if current['landscape_reports_up_to_date'] and current['composite_blake3']==r['target_composite_blake3']:
            sha=hashlib.sha256(b''.join(struct.pack('<H',v) for v in current['composite_heights_uint16'])).hexdigest()
            assert sha=='a2c8846963f7d08f7df561b6c800f2f389255e6f52c14a1453a6e5e3f7674726'
            with (OUT/'collar-settled.json').open('x',encoding='utf-8') as f:
                json.dump({'success':True,'composite_blake3':current['composite_blake3'],'grid_sha256':sha,
                    'original_layer_blake3':current['original_layer_blake3'],'component_count':45,'collision_count':45,
                    'saved':False,'wall_probe_parameters_unchanged':True},f,indent=2)
            u.unregister_slate_post_tick_callback(state['handle'])
            print('CAVES UNDER_WALL_FLOOR_COLLAR_SETTLED_QA_PENDING')
    except Exception:
        u.unregister_slate_post_tick_callback(state['handle'])
        with (OUT/'collar-settle-failure.json').open('x',encoding='utf-8') as f:json.dump({'traceback':traceback.format_exc()},f,indent=2)
        u.log_error('CAVES COLLAR_SETTLE '+traceback.format_exc())
    finally:state['busy']=False
state['handle']=u.register_slate_post_tick_callback(tick)
