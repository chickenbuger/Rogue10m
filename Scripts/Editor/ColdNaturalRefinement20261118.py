"""Owned unattended cold read-only verification; never saves or authors assets."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import traceback
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
OUT=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/refinement-cold'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
state={'phase':'registry','ticks':0,'started':time.monotonic()}


def write(name,value):
    with (OUT/(name+'.json')).open('x',encoding='utf-8') as f:json.dump(value,f,ensure_ascii=False,indent=2)


def tick(_):
    try:
        if time.monotonic()-state['started']>300:raise RuntimeError('Cold watchdog')
        if u.AssetRegistryHelpers.get_asset_registry().is_loading_assets():return
        if state['phase']=='registry':
            editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
            if editor.get_game_world() or editor.get_editor_world().get_path_name().split('.')[0]!=TARGET:raise RuntimeError('Exact idle candidate required')
            d=json.loads(u.Rogue10mCrop20261114WorldAuthoring.load_candidate_region())
            write('region-load',d)
            if not d.get('success'):raise RuntimeError('Full region not loaded')
            state['phase']='settling';return
        state['ticks']+=1
        if state['ticks']<30:return
        spec=importlib.util.spec_from_file_location('natural_cold_validator',ROOT/'Scripts/Editor/ValidateNaturalWorld20261117.py')
        m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
        packages={}
        prefix='/Game/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/'
        for obj in u.ObjectIterator(u.Object):
            name=obj.get_package().get_name()
            if name.startswith(prefix):
                row=packages.setdefault(name,{'classes':set(),'object_paths':[]})
                row['classes'].add(obj.get_class().get_path_name())
                row['object_paths'].append(obj.get_path_name())
        write('package-inventory',{'source':'Loaded public UObject class/package metadata; no property mutation',
                                  'packages':[{'package':k,'classes':sorted(v['classes']),
                                               'object_paths':sorted(v['object_paths'])} for k,v in sorted(packages.items())]})
        report=m.validate_editor(require_saved=True,refinement_stage='complete')
        write('validation',report)
        write('result',{'passed':report.get('passed') is True,'pid':os.getpid(),'assets_saved':False,
                        'checks':len(report.get('checks',{})),'diagnostics':report.get('diagnostics'),
                        'traceback':report.get('traceback')})
        if not report.get('passed'):u.log_error('Natural world cold validation failed; preserve diagnostics')
        u.unregister_slate_post_tick_callback(state['handle']);u.SystemLibrary.quit_editor()
    except Exception:
        write('failure',{'traceback':traceback.format_exc(),'phase':state['phase']})
        u.log_error(traceback.format_exc())
        u.unregister_slate_post_tick_callback(state['handle']);u.SystemLibrary.quit_editor()


if '-RogueNaturalStage=refinement-cold' not in u.SystemLibrary.get_command_line() or '-unattended' not in u.SystemLibrary.get_command_line():raise RuntimeError('Explicit owned cold launch required')
sys.dont_write_bytecode=True
OUT.mkdir(parents=True,exist_ok=False)
state['handle']=u.register_slate_post_tick_callback(tick)
