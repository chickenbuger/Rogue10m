"""One fresh-S-spawn normal walk/double-jump attempt at the south perimeter.

Owner starts/stops PIE. Only public profile application, StartRun preparation,
AddMovementInput/Jump/StopJumping are invoked. No transform/speed/time setters.
"""
import importlib.util
import json
import math
import os
from pathlib import Path
import sys
import time
import traceback
import unreal as u

ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117'
OUT=BASE/'runtime-boundary-jump.json'
sys.dont_write_bytecode=True
assert os.getpid()==json.loads((BASE/'author-live/startup.json').read_text())['pid']
assert not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world() and not OUT.exists()
spec=importlib.util.spec_from_file_location('natural_boundary_walk_helpers',ROOT/'Scripts/Editor/PlaytestLandscapeCrop20261116.py')
w=importlib.util.module_from_spec(spec);spec.loader.exec_module(w)
spec=importlib.util.spec_from_file_location('natural_boundary_height_math',ROOT/'Scripts/NaturalLandscapePlan20261117.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
state={'phase':'waiting','wall':time.monotonic(),'seen':None,'samples':[],'jumps':[],'airborne':0,'max_signed':-1e9,'min_y':1e9}


def finish(error=None):
    u.unregister_slate_post_tick_callback(state['handle'])
    if state.get('pawn'):state['pawn'].get_movement_component().stop_movement_immediately()
    serial={k:v for k,v in state.items() if k not in ('handle','pawn','world','pc','gs')}
    serial.update(passed=error is None,error=error,physical_input=False,teleports=0,speed_overrides=0,
                  scope='One representative south approach using normal CharacterMovement and two public Jump requests; other segments use544staticcapsules')
    with OUT.open('x',encoding='utf-8') as f:json.dump(serial,f,ensure_ascii=False,indent=2)
    print('NATURAL_WORLD BOUNDARY_JUMP '+str(error is None))


def tick(_):
    try:
        assert time.monotonic()-state['wall']<180,'Boundary observer watchdog'
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if state['phase']=='waiting':
            if world is None:return
            if state['seen'] is None:state['seen']=time.monotonic();return
            if time.monotonic()-state['seen']<3:return
            _,world,pc=w._context({w.TARGET});pc.enter_selected_character();pc.close_all_blocking_panels()
            pawn=u.GameplayStatics.get_player_character(world,0);gs=u.GameplayStatics.get_game_state(world)
            w._selected_profile(u,require_runtime_name=True)
            p=pawn.get_actor_location();assert abs(p.x-15000)<100 and abs(p.y-32550)<100
            assert isinstance(pawn,u.Rogue10mCharacter) and not pawn.is_dead()
            assert pawn.get_editor_property('jump_max_count')==2 and pawn.get_editor_property('jump_max_hold_time')==0
            assert abs(gs.get_run_duration_seconds()-1200)<.01
            if not gs.is_run_active():assert gs.get_run_phase()==u.Rogue10mRunPhase.WAITING_TO_START;gs.start_run()
            assert gs.is_run_active()
            state.update(phase='settling',pawn=pawn,world=world,pc=pc,gs=gs,spawn=[p.x,p.y,p.z],settle=time.monotonic());return
        assert world==state['world'],'World changed'
        pawn=state['pawn'];movement=pawn.get_movement_component();p=pawn.get_actor_location()
        assert not pawn.is_dead() and state['gs'].is_run_active() and not pawn.is_sprinting() and not pawn.is_dodging()
        assert abs(movement.max_walk_speed-620)<.05 and abs(u.GameplayStatics.get_global_time_dilation(world)-1)<.001
        assert abs(pawn.get_editor_property('CustomTimeDilation')-1)<.001 and not u.GameplayStatics.is_game_paused(world)
        if state['phase']=='settling':
            if time.monotonic()-state['settle']<2:return
            assert movement.is_moving_on_ground()
            state.update(phase='attempting',start_sim=u.GameplayStatics.get_time_seconds(world),last_sample=-1,ground_start_z=p.z)
        elapsed=u.GameplayStatics.get_time_seconds(world)-state['start_sim']
        signed=h.signed_distance(p.x,p.y)
        state['max_signed']=max(state['max_signed'],signed);state['min_y']=min(state['min_y'],p.y)
        assert signed < -50,'Pawn crossed the intended perimeter'
        state['airborne']+=int(not movement.is_moving_on_ground())
        if not state['jumps'] and p.y<=27050:
            assert pawn.can_jump();pawn.jump();state['jumps'].append({'at':elapsed,'position':[p.x,p.y,p.z]})
        elif len(state['jumps'])==1:
            since=elapsed-state['jumps'][0]['at']
            if since>=.1 and not state.get('first_released'):pawn.stop_jumping();state['first_released']=True
            if since>=.22:
                assert not movement.is_moving_on_ground() and pawn.can_jump(),'Normal second jump unavailable'
                pawn.jump();state['jumps'].append({'at':elapsed,'position':[p.x,p.y,p.z]})
        elif len(state['jumps'])==2 and elapsed-state['jumps'][1]['at']>=.1 and not state.get('second_released'):
            pawn.stop_jumping();state['second_released']=True
        if elapsed-state['last_sample']>=.2:
            state['last_sample']=elapsed
            state['samples'].append({'seconds':elapsed,'position':[p.x,p.y,p.z],'signed_perimeter_cm':signed,
                'grounded':movement.is_moving_on_ground(),'jump_current_count':pawn.get_editor_property('jump_current_count')})
        if elapsed>=30:
            assert len(state['jumps'])==2 and state['airborne']>0 and state['min_y']<27000,'Did not exercise the perimeter approach'
            tail=[r for r in state['samples'] if r['seconds']>=25]
            assert len(tail)>=10 and max(r['position'][1] for r in tail)-min(r['position'][1] for r in tail)<50,'Pawn did not settle against the natural barrier'
            state['final_position']=[p.x,p.y,p.z];finish();return
        pawn.add_movement_input(u.Vector(0,-1,0),1.0,False)
    except Exception:finish(traceback.format_exc())


state['handle']=u.register_slate_post_tick_callback(tick)
print('NATURAL_WORLD BOUNDARY_JUMP_OBSERVER_ARMED')
