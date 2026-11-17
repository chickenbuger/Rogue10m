"""Read binding docs for 3D-only visual captures."""
from pathlib import Path
import json
import unreal as u
p=Path('D:/Project/Rogue10m/Feature/doc/evidence/cave-organic20261118/viewport-api.json');assert not p.exists()
r={}
for cls,names in [(u.UnrealEditorSubsystem,['set_level_viewport_camera_info']),(u.LevelEditorSubsystem,['editor_set_game_view']),(u.AutomationLibrary,['take_high_res_screenshot'])]:
    for n in names:r[cls.__name__+'.'+n]=str(getattr(cls,n).__doc__)
with p.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
print('ORGANIC_VIEWPORT_BINDINGS_READ_ONLY')
