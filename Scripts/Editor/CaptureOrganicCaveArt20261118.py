"""Three explicit 3D-only editor art views; no scene/save/camera-default edits."""
from pathlib import Path
import json
import time
import unreal as u
BASE=Path('D:/Project/Rogue10m/Feature/doc/evidence/cave-organic20261118');OUT=BASE/'visual'
assert json.loads((BASE/'author-live/scene.json').read_text())['passed']
rows=[('after-Q-entry',[-9750,63000,8820],[8,90,0]),
      ('after-Q-vault',[-7150,66500,8820],[12,45,0]),
      ('after-Z-forge',[53000,49950,8820],[12,30,0])]
assert all(not(OUT/(r[0]+'.png')).exists()for r in rows)
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem);level=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert not editor.get_game_world()
level.editor_set_game_view(True)
state={'index':0,'phase':'pose','busy':False,'receipts':[],'started':time.monotonic()}
def tick(_):
    if state['busy']:return
    state['busy']=True
    try:
        assert time.monotonic()-state['started']<180
        name,location,rotation=rows[state['index']]
        if state['phase']=='pose':
            editor.set_level_viewport_camera_info(u.Vector(*location),u.Rotator(*rotation))
            state.update(phase='settle',since=time.monotonic(),ticks=0);return
        if state['phase']=='settle':
            state['ticks']+=1
            if state['ticks']<12 or time.monotonic()-state['since']<2:return
            task=u.AutomationLibrary.take_high_res_screenshot(1280,720,str(OUT/(name+'.png')),force_game_view=True)
            assert task and task.is_valid_task();state.update(phase='capture',task=task);return
        if not state['task'].is_task_done():return
        assert(OUT/(name+'.png')).exists()
        state['receipts'].append({'image':name+'.png','editor_view_location_cm':location,'rotation_pitch_yaw_roll':rotation,
           'actual_game_camera':False,'capture':'AutomationLibrary.TakeHighResScreenshot Camera=None force_game_view=True',
           'scene_changes':False,'gameplay_camera_defaults_changed':False})
        state['index']+=1
        if state['index']==len(rows):
            u.unregister_slate_post_tick_callback(state['handle'])
            with(OUT/'editor-capture-receipt.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(state['receipts'],f,indent=2)
            print('ORGANIC_CAVE_ART_THREE_VIEWS_READY');return
        state['phase']='pose'
    except Exception:
        u.unregister_slate_post_tick_callback(state['handle']);raise
    finally:state['busy']=False
state['handle']=u.register_slate_post_tick_callback(tick)
print('ORGANIC_CAVE_ART_CAPTURE_ARMED')
