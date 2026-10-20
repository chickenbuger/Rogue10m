"""Separate Unreal process: reload saved two clips/guide, without asset writes."""
import json
import os
from pathlib import Path
import sys
import traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import ApplyArinWalkTalk as application

report={'process_id':os.getpid(),'runtime':'fresh UnrealEditor-Cmd NullRHI persistence inspection',
        'asset_mutation_requested':False,'rendering_test':False}
try:
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(application.model.MAP_PATH):
        raise RuntimeError('Saved StartHub load failed')
    result=application.main(validate_only=True)
    report.update({'status':'passed','saved_assets':result})
except Exception:
    report.update({'status':'failed','exception':traceback.format_exc()})
application.write('ue-fresh-process.json',report)
if report['status']!='passed':
    raise RuntimeError(report['exception'])
unreal.log('ARIN_WALK_TALK_FRESH_PROCESS_PASS pid='+str(os.getpid()))
unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'QUIT_EDITOR')
