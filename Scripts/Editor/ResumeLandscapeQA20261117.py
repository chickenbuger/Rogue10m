"""Exact-candidate startup baseline and observer for three owner-started PIE runs."""
import importlib.util
import json
from pathlib import Path
import runpy
import sys
import time
import traceback
import unreal

ROOT = Path('D:/Project/Rogue10m')
OUT = ROOT / 'Feature/doc/evidence/landscape-resume-20261117'
TARGET = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert Path(unreal.Paths.project_dir()).resolve() == ROOT
assert not EDITOR.get_game_world()
assert EDITOR.get_editor_world().get_path_name().split('.')[0] == TARGET
OUT.mkdir(parents=True, exist_ok=True)

def write(name, value):
    with (OUT / (name + '.json')).open('x', encoding='utf-8') as f:
        json.dump(value, f, ensure_ascii=False, indent=2)
        f.write('\n')

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    result = importlib.util.module_from_spec(spec)
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(result)
    finally:
        sys.dont_write_bytecode = previous
    return result

runpy.run_path(str(ROOT / 'Scripts/Editor/RestoreEditorCultureAfterSmoke.py'), run_name='__main__')
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
loaded = json.loads(unreal.Rogue10mCrop20261114WorldAuthoring.load_candidate_region())
write('region-load', loaded)
assert loaded.get('success') and loaded.get('dirty_package_names_unchanged')

WALK = module('landscape_walk_20261117', 'Scripts/Editor/PlaytestLandscapeCrop20261116.py')
WALK.EVIDENCE = OUT / 'runtime-walk'
VERIFY = module('landscape_verify_20261117', 'Scripts/Editor/VerifyLandscapeProduction20261116.py')
VERIFY.EVIDENCE = OUT
STATE = {'phase':'settling_baseline', 'ticks':0, 'round':0, 'last_world':None, 'first_seen':None, 'handle':None}

def baseline():
    previous = sys.argv
    sys.argv = ['VerifyLandscapeProduction20261116.py', '--stage', 'cold-reload']
    try:
        VERIFY.main()
    finally:
        sys.argv = previous
    report = json.loads((OUT/'cold-reload/comparison.json').read_text(encoding='utf-8'))
    assert report['passed'] and report['loadedReflectionWalkComplete'] is True
    assert not report['dirtyAfter']['maps'] and not report['dirtyAfter']['content']
    write('ready', {'ready':True,'owner_starts_PIE':True,'rounds':3,
                    'QA_profile':'existing QA_Landscape20261116A; no creation',
                    'baseline':str(OUT/'cold-reload/comparison.json')})
    STATE['phase'] = 'waiting_PIE'
    print('LANDSCAPE_RESUME_20261117 READY')

def preflight(index):
    u, world, pc = WALK._context({TARGET})
    pawn = u.GameplayStatics.get_player_character(world, 0)
    identity = WALK._selected_profile(u, require_runtime_name=True)
    movement = pawn.get_movement_component()
    gs = u.GameplayStatics.get_game_state(world)
    position = pawn.get_actor_location()
    checks = {'name':str(pawn.get_character_display_name()) == WALK.CHARACTER_NAME,
              'production_character':isinstance(pawn, u.Rogue10mCharacter),
              'third_person':pawn.is_third_person_gameplay_active(),
              'alive':not pawn.is_dead(),'not_sprinting':not pawn.is_sprinting(),
              'not_dodging':not pawn.is_dodging(),
              'walk620':abs(movement.max_walk_speed-620)<0.05,
              'stat620':abs(pawn.get_character_stat_snapshot().get_editor_property('MoveSpeed')-620)<0.05,
              'global_time1':abs(u.GameplayStatics.get_global_time_dilation(world)-1)<0.001,
              'pawn_time1':abs(pawn.get_editor_property('CustomTimeDilation')-1)<0.001,
              'S_spawn':abs(position.x-15000)<100 and abs(position.y-32550)<100,
              'duration1200':abs(gs.get_run_duration_seconds()-1200)<0.01}
    record = {'round':index,'identity':identity,'checks':checks,'passed':all(checks.values()),
              'location':[position.x,position.y,position.z],
              'run_phase':str(gs.get_run_phase()),'physical_input':False}
    write('preflight-'+str(index), record)
    assert record['passed'], record
    # Default StartRun is explicit preparation; no duration/position/speed override.
    WALK.begin_round(index, fresh_pie_confirmed=True, allow_default_run_start=True)

def tick(delta):
    try:
        if STATE['phase'] == 'settling_baseline':
            STATE['ticks'] += 1
            if STATE['ticks'] >= 15:
                baseline()
            return
        world = EDITOR.get_game_world()
        if STATE['phase'] == 'waiting_PIE':
            if world is None:
                STATE['last_world'] = None
                STATE['first_seen'] = None
                return
            if STATE['last_world'] is not None:
                return
            if STATE['first_seen'] is None:
                STATE['first_seen'] = time.monotonic()
                return
            if time.monotonic()-STATE['first_seen'] < 3:
                return
            index = STATE['round']+1
            preflight(index)
            STATE['round'] = index
            STATE['last_world'] = world
            STATE['phase'] = 'walking'
        elif STATE['phase'] == 'walking' and WALK._runner.handle is None:
            report = json.loads((WALK.EVIDENCE/('round-%d.json'%STATE['round'])).read_text(encoding='utf-8'))
            assert report['passed'], report.get('error')
            if STATE['round'] == 3:
                WALK.summarize()
                STATE['phase'] = 'complete'
                unreal.unregister_slate_post_tick_callback(STATE['handle'])
                STATE['handle'] = None
                print('LANDSCAPE_RESUME_20261117 THREE_ROUNDS_COMPLETE')
            else:
                STATE['phase'] = 'waiting_PIE'
                STATE['ticks'] = 0
                print('LANDSCAPE_RESUME_20261117 ROUND_COMPLETE; owner stops/starts fresh PIE')
    except Exception:
        error = traceback.format_exc()
        STATE['phase'] = 'blocked'
        if STATE['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(STATE['handle'])
            STATE['handle'] = None
        write('observer-failure', {'passed':False,'round':STATE['round'],'traceback':error})
        unreal.log_error('LANDSCAPE_RESUME_20261117 '+error)

STATE['handle'] = unreal.register_slate_post_tick_callback(tick)
