"""Resume QA only after both one-shot content authors completed successfully."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import unreal as u

ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117';OUT=BASE/'refinement-live'
sys.dont_write_bytecode=True
assert os.getpid()==json.loads((OUT/'startup.json').read_text())['pid']
assert not (OUT/'qa-recovery-preflight.json').exists()
failure=json.loads((OUT/'failure.json').read_text())
assert failure['phase']=='await_content' and 'Intermediate checkpoint must stay clean' in failure['traceback']
assert json.loads((OUT/'boss-threshold.json').read_text())['success']
assert json.loads((OUT/'village-earth.json').read_text())['success']
spec=importlib.util.spec_from_file_location('natural_refinement_qa_recovery',ROOT/'Scripts/Editor/RefineNaturalWorld20261118.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.context();current=m.inspect();applied=m.read('refinement-live/apply-height.json')
assert current['composite_blake3']==applied['target_composite_blake3'] and current['original_layer_blake3']==m.read('author-live/native-before.json')['original_layer_blake3']
assert len(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())==1404
m.write('qa-recovery-preflight',{'pid':os.getpid(),'actors':1404,'authors_repeated':False,'height_edits':0,
    'diagnosis':'Nested Slate tick while material compiler pumped events; recursive clean guard stopped before mutation',
    'fixed':'Busy guard + early authoring phase; resume validation only'})
m.state.update(applied=applied,original_hash=current['original_layer_blake3'],started=time.monotonic(),
    phase='await_final_visual',failure_name='qa-recovery-final-failure',busy=False)
m.qa('complete')
m.write('ready-for-final-visual',{'target_composite_blake3':applied['target_composite_blake3'],'saved':False,'actors':1404})
m.state['handle']=u.register_slate_post_tick_callback(m.tick)
print('NATURAL_REFINEMENT COMPLETE_QA_RECOVERED_NO_AUTHOR_REPEAT')
