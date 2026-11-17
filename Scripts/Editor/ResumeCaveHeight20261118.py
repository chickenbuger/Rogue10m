"""Resume after the native clean-package guard rejected before height mutation."""
from pathlib import Path
import importlib.util
import json
import os
import time
import traceback
import unreal as u

ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
spec=importlib.util.spec_from_file_location('cave_height_order_recovery',ROOT/'Scripts/Editor/BuildCaves20261118.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
c.context()
assert not (OUT/'structure-checkpoint-save-result.json').exists()
failed=c.read('author-live/apply-height.json')
assert failed['success'] is False and failed['mutation_started'] is False and failed['packages_saved'] is False
assert 'clean packages' in failed['error']
assert c.read('author-live/structures.json')['success'] and c.read('author-live/materials.json')['success']
c.state.update(materials=c.read('author-live/materials.json'),structures=c.read('author-live/structures.json'),started=time.monotonic())
original_write=c.write
def recovery_write(name,obj):original_write('height-recovery-failure' if name=='failure' else name,obj)
c.write=recovery_write
try:
    c.save_structure_checkpoint()
    c.apply_height(suffix='-confirmed')
    c.write('height-order-recovery',{'original_failed_attempt_mutation_started':False,'original_failure_preserved':True,
        'reason':'Native height revision requires clean packages. Save only the validated twelve new structure/material/folder packages first.',
        'save_all_used':False,'actors_or_materials_repeated':False,'automatic_retry':False})
    c.state['handle']=u.register_slate_post_tick_callback(c.tick)
    print('CAVES HEIGHT_ORDER_FIXED_REVISION_CONFIRMED_SETTLE_ARMED')
except Exception:
    c.write('height-recovery-failure',{'traceback':traceback.format_exc(),'dirty':c.dirty()})
    u.log_error('CAVES HEIGHT_RECOVERY '+traceback.format_exc())
    raise
