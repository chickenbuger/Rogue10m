"""Explicit owner phases; no map travel, broad save, or automatic retries."""
import argparse
import importlib.util
from pathlib import Path
import sys
import time
import traceback
import unreal

ROOT = Path('D:/Project/Rogue10m')
parser = argparse.ArgumentParser()
parser.add_argument('--phase', required=True, choices=('armoptional','road-preflight','road-prototype'))
phase = parser.parse_args().phase

def module(key, relative):
    if key not in sys.modules:
        spec = importlib.util.spec_from_file_location(key, ROOT/relative)
        result = importlib.util.module_from_spec(spec)
        previous = sys.dont_write_bytecode
        sys.dont_write_bytecode = True
        try:
            spec.loader.exec_module(result)
        finally:
            sys.dont_write_bytecode = previous
        sys.modules[key] = result
    return sys.modules[key]

if phase == 'armoptional':
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not editor.get_game_world()
    assert editor.get_editor_world().get_path_name().split('.')[0] == '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
    out = ROOT/'Feature/doc/evidence/landscape-resume-20261117'
    import json
    summary = json.loads((out/'runtime-walk/summary.json').read_text(encoding='utf-8'))
    assert summary['passed'] and summary['repetitions'] == 3
    task = module('landscape_optional_20261117', 'Scripts/Editor/WalkLandscapeOptionalRoads20261117.py')
    pending = {'handle':None,'first_seen':None}
    def tick(delta):
        if editor.get_game_world() is None:
            return
        if pending['first_seen'] is None:
            pending['first_seen'] = time.monotonic()
            return
        if time.monotonic()-pending['first_seen'] < 3:
            return
        unreal.unregister_slate_post_tick_callback(pending['handle'])
        pending['handle'] = None
        try:
            task.begin()
        except Exception:
            error = traceback.format_exc()
            with (out/'optional-preflight-failure.json').open('x',encoding='utf-8') as f:
                json.dump({'passed':False,'traceback':error},f,indent=2)
            unreal.log_error('LANDSCAPE_OPTIONAL_PREFLIGHT '+error)
    pending['handle'] = unreal.register_slate_post_tick_callback(tick)
    print('LANDSCAPE_OPTIONAL_ARMED; owner starts fourth fresh PIE')
else:
    task = module('landscape_road_surface_20261117', 'Scripts/Editor/RepairLandscapeRoadSurface20261117.py')
    result = task.preflight() if phase == 'road-preflight' else task.prototype(allow_editor_mutation=True)
    print('LANDSCAPE_ROAD_SURFACE_PHASE '+phase+' '+result['status'])
