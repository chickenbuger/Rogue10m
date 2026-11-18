"""Finish only the diagnosed two unsaved derivatives, then resume later phases."""
from pathlib import Path
import importlib.util
import json
import os
import time
import traceback
import unreal as u

ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
def load(name,rel):
    s=importlib.util.spec_from_file_location(name,ROOT/rel);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
assert os.getpid()==json.loads((OUT/'startup.json').read_text())['pid']
assert not (OUT/'materials.json').exists() and not (OUT/'material-resume-failure.json').exists()
assert json.loads((OUT/'failure.json').read_text())['phase']=='authoring_materials'
mat=load('cave_material_diagnosed_completion','Scripts/Editor/CaveMaterials20261118.py')
try:
    result=mat.create_cave_materials(allow_editor_mutation=True,resume_diagnosed_partial=True)
except mat.CaveMaterialError as e:
    with (OUT/'material-resume-failure.json').open('x',encoding='utf-8') as f:json.dump(e.report,f,indent=2)
    u.log_error('CAVES MATERIAL_COMPLETION '+e.report['traceback'])
    raise RuntimeError('Material completion stopped; preserve partial work') from None
assert result['success'] and len(result['created_assets'])==7
c=load('cave_author_after_diagnosed_materials','Scripts/Editor/BuildCaves20261118.py')
c.context();c.write('materials',result)
c.write('material-recovery',{'source_files_unchanged':True,'existing_two_assets_reused':True,
    'new_duplicate_of_existing_assets':False,'original_failure_preserved':True,
    'binding_bug':'MaterialEditingLibrary.cpp setters never change their false result variable; actual getter values are verified.',
    'automatic_retry':False})
c.state.update(materials=result,phase='structures',started=time.monotonic(),busy=False,
               failure_name='post-material-recovery-failure')
# Keep the initial controller failure immutable if a later independent phase fails.
original_write=c.write
def recovery_write(name,obj):original_write('post-material-recovery-failure' if name=='failure' else name,obj)
c.write=recovery_write
c.state['handle']=u.register_slate_post_tick_callback(c.tick)
print('CAVES MATERIALS_COMPLETED_SOURCE_PRESERVED_NEXT_PHASES_ARMED')
