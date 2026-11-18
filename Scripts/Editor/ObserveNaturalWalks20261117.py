"""Owner-controlled rendered PIE, three ordinary central walks plus optional route.

No editor save, travel, pawn transform/speed setter, or PIE lifecycle control.
The source and candidate remain unchanged; existing QA profile is applied by
the same public controller path as the original prepared-walk helpers.
"""
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import traceback
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
BASE=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117'
OUT=BASE/'runtime-walk'
sys.dont_write_bytecode=True
assert os.getpid()==json.loads((BASE/'author-live/startup.json').read_text())['pid']
assert not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert json.loads((BASE/'author-live/capsule-validation-gate-reflow.json').read_text())['passed']
OUT.mkdir(exist_ok=False)


def module(name,relative):
    s=importlib.util.spec_from_file_location(name,ROOT/relative);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m


def write(name,data):
    with (OUT/(name+'.json')).open('x',encoding='utf-8') as f:json.dump(data,f,ensure_ascii=False,indent=2)


walk=module('natural_central_walk','Scripts/Editor/PlaytestLandscapeCrop20261116.py')
walk.EVIDENCE=OUT
optional=module('natural_optional_walk','Scripts/Editor/WalkLandscapeOptionalRoads20261117.py')
optional.OUT=BASE/'runtime-optional-walk.json'
state={'phase':'waiting','round':0,'last':None,'seen':None,'started':time.monotonic()}


def tick(_):
    try:
        if time.monotonic()-state['started']>1800:raise RuntimeError('Walk observation watchdog')
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if state['phase']=='waiting':
            if world is None:state.update(last=None,seen=None);return
            if state['last'] is not None:return
            if state['seen'] is None:state['seen']=time.monotonic();return
            if time.monotonic()-state['seen']<3:return
            index=state['round']+1
            if index<=3:
                setup=walk.begin_round(index,fresh_pie_confirmed=True,allow_default_run_start=True)
                pawn=walk._runner.pawn
            else:
                _,world,pc=walk._context({walk.TARGET})
                pc.enter_selected_character();pc.close_all_blocking_panels()
                setup=optional.begin();pawn=optional._runner.pawn
            capsule=pawn.get_editor_property('capsule_component');movement=pawn.get_movement_component()
            write('kinematics-'+str(index),{'PIE_world':world.get_path_name(),'profile':str(pawn.get_character_display_name()),
                'radius_cm':capsule.get_scaled_capsule_radius(),'half_height_cm':capsule.get_scaled_capsule_half_height(),
                'jump_z_velocity':movement.get_editor_property('jump_z_velocity'),
                'gravity_scale':movement.get_editor_property('gravity_scale'),
                'world_gravity_z':movement.get_gravity_z() if callable(getattr(movement,'get_gravity_z',None)) else 'public binding unavailable',
                'jump_max_count':pawn.get_editor_property('jump_max_count'),'jump_max_hold_time':pawn.get_editor_property('jump_max_hold_time'),
                'setup':setup,'physical_input':False,'editor_content_saved_before_walk':(BASE/'author-live/numeric-checkpoint.json').exists()})
            state.update(phase='walking',round=index,last=world);return
        if state['phase']=='walking':
            runner=walk._runner if state['round']<=3 else optional._runner
            if runner.handle is not None:return
            file=OUT/('round-%d.json'%state['round']) if state['round']<=3 else optional.OUT
            report=json.loads(file.read_text(encoding='utf-8'))
            if not report.get('passed'):raise RuntimeError('Walk failed; no retry: '+str(report.get('error')))
            if state['round']==3:walk.summarize()
            if state['round']==4:
                state['phase']='complete';u.unregister_slate_post_tick_callback(state['handle'])
                write('all-walks-complete',{'passed':True,'central_repetitions':3,'optional_edges':8,'physical_input':False})
                print('NATURAL_WORLD FOUR_WALKS_COMPLETE')
            else:state['phase']='waiting';print('NATURAL_WORLD WALK_COMPLETE '+str(state['round'])+'; owner stops/starts fresh PIE')
    except Exception:
        phase=state['phase'];state['phase']='failed'
        u.unregister_slate_post_tick_callback(state['handle'])
        write('observer-failure',{'phase':phase,'round':state['round'],'traceback':traceback.format_exc()})
        u.log_error(traceback.format_exc())


state['handle']=u.register_slate_post_tick_callback(tick)
write('armed',{'owner_controls_PIE':True,'central_walks':3,'optional_route':list(optional.SEQUENCE),'teleports':0,'speed_overrides':0})
print('NATURAL_WORLD WALK_OBSERVER_ARMED')
