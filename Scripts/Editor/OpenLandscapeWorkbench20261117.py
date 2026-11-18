"""Visible workbench startup: preserve baseline, inspect roads, arm optional walk."""
import importlib.util
import json
from pathlib import Path
import runpy
import sys
import time
import traceback
import unreal

ROOT = Path('D:/Project/Rogue10m')
OUT = ROOT/'Feature/doc/evidence/landscape-resume-20261117/workbench'
OUT.mkdir(parents=True,exist_ok=True)
EDITOR = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert Path(unreal.Paths.project_dir()).resolve() == ROOT
assert not EDITOR.get_game_world()
assert EDITOR.get_editor_world().get_path_name().split('.')[0] == '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
summary=json.loads((OUT.parent/'runtime-walk/summary.json').read_text(encoding='utf-8'))
assert summary['passed'] and summary['repetitions'] == 3

def load(key,relative):
    spec=importlib.util.spec_from_file_location(key,ROOT/relative)
    value=importlib.util.module_from_spec(spec)
    previous=sys.dont_write_bytecode
    sys.dont_write_bytecode=True
    try:spec.loader.exec_module(value)
    finally:sys.dont_write_bytecode=previous
    sys.modules[key]=value
    return value

runpy.run_path(str(ROOT/'Scripts/Editor/RestoreEditorCultureAfterSmoke.py'),run_name='__main__')
loaded=json.loads(unreal.Rogue10mCrop20261114WorldAuthoring.load_candidate_region())
with (OUT/'region-load.json').open('x',encoding='utf-8') as f:json.dump(loaded,f,indent=2)
assert loaded['success'] and loaded['dirty_package_names_unchanged']
VERIFY=load('landscape_workbench_verify_20261117','Scripts/Editor/VerifyLandscapeProduction20261116.py')
VERIFY.EVIDENCE=OUT
ROAD=load('landscape_road_surface_20261117','Scripts/Editor/RepairLandscapeRoadSurface20261117.py')
OPTIONAL=load('landscape_optional_20261117','Scripts/Editor/WalkLandscapeOptionalRoads20261117.py')
STATE={'ticks':0,'phase':'baseline','handle':None,'first_seen':None}

def tick(delta):
    try:
        if STATE['phase']=='baseline':
            STATE['ticks']+=1
            if STATE['ticks']<15:return
            previous=sys.argv
            sys.argv=['VerifyLandscapeProduction20261116.py','--stage','cold-reload']
            try:VERIFY.main()
            finally:sys.argv=previous
            plan=ROAD.preflight()
            assert plan['status']=='read_only_preflight_passed_prototype_not_authored'
            with (OUT/'ready.json').open('x',encoding='utf-8') as f:
                json.dump({'baseline_passed':True,'road_preflight_passed':True,'optional_armed':True},f)
            STATE['phase']='optional_armed'
            print('LANDSCAPE_WORKBENCH_20261117 READY; optional observer armed')
        elif STATE['phase']=='optional_armed':
            if EDITOR.get_game_world() is None:return
            if STATE['first_seen'] is None:
                STATE['first_seen']=time.monotonic()
                return
            if time.monotonic()-STATE['first_seen']<3:return
            unreal.unregister_slate_post_tick_callback(STATE['handle'])
            STATE['handle']=None
            OPTIONAL.begin()
            STATE['phase']='optional_walking'
    except Exception:
        error=traceback.format_exc()
        if STATE['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(STATE['handle'])
            STATE['handle']=None
        STATE['phase']='blocked'
        with (OUT/'failure.json').open('x',encoding='utf-8') as f:json.dump({'traceback':error},f,indent=2)
        unreal.log_error('LANDSCAPE_WORKBENCH_20261117 '+error)

STATE['handle']=unreal.register_slate_post_tick_callback(tick)
