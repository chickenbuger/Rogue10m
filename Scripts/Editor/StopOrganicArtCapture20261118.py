"""End only this task's editor-art callback before actual gameplay QA."""
from pathlib import Path
import json
import sys
import unreal as u
p=Path('D:/Project/Rogue10m/Feature/doc/evidence/cave-organic20261118/visual/editor-capture-stopped.json');assert not p.exists()
m=sys.modules.get('rogue_organic_cave_art_capture_20261118');r={'module_present':m is not None}
candidate=getattr(m,'state',None) if m else None
if candidate is None:
    candidate=globals().get('state')
    r['shared_console_state_fallback']=True
if isinstance(candidate,dict) and {'receipts','index','phase','started','handle'}<=set(candidate):
    r.update({k:candidate.get(k) for k in ['index','phase','busy','ticks']})
    r['task_valid']=candidate.get('task')is not None and candidate['task'].is_valid_task()
    r['task_done']=candidate.get('task')is not None and candidate['task'].is_task_done()
    u.unregister_slate_post_tick_callback(candidate['handle']);r['owned_callback_unregistered']=True
with p.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
print('ORGANIC_EDITOR_ART_CALLBACK_ENDED')
