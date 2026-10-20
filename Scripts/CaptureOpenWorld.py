"""Capture actual Unreal editor viewpoints through the official MCP tool."""
import base64
import json
from pathlib import Path
import subprocess
import sys
from BuildOpenWorldPCG import call

ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=ROOT/'Feature/doc/evidence/open-world-20261005'
VIEWS={
    'overview':{'location':{'x':0,'y':-135000,'z':155000},'rotation':{'pitch':-50,'yaw':90,'roll':0}},
    'camp':{'location':{'x':-5800,'y':-73500,'z':6600},'rotation':{'pitch':-16,'yaw':62,'roll':0}},
    'river':{'location':{'x':-6000,'y':-7000,'z':8500},'rotation':{'pitch':-16,'yaw':63,'roll':0}},
    'ruins':{'location':{'x':1000,'y':48000,'z':14500},'rotation':{'pitch':-12,'yaw':65,'roll':0}},
}

def main():
    # PIE restores viewport show flags; prepare game view before every capture pass.
    command = (
        "import unreal; "
        "levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); "
        "assert not levels.is_in_play_in_editor(); "
        "assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()"
        ".get_path_name().split('.')[0]=='/Game/Rogue10m/Maps/OpenRunMap'; "
        "unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([]); "
        "levels.editor_set_game_view(True); assert levels.editor_get_game_view()"
    )
    preparation = subprocess.run(
        [sys.executable, str(ROOT / 'Scripts/RunEditorRemote.py'), '--command', command, '--timeout', '12'],
        capture_output=True, text=True, encoding='utf-8')
    if preparation.returncode:
        raise RuntimeError('Capture preparation failed: ' + preparation.stdout[-2000:] + preparation.stderr[-1000:])
    for name,transform in VIEWS.items():
        transform['scale']={'x':1,'y':1,'z':1}
        value=call('call_tool',{'toolset_name':'EditorToolset.EditorAppToolset','tool_name':'CaptureViewport',
            'arguments':{'captureTransform':transform,'annotations':None,'bShowUI':False}},'capture-'+name)
        image=value['image']
        if image['mimeType']!='image/png':
            raise RuntimeError('Unexpected capture image format')
        path=EVIDENCE/('view-'+name+'.png')
        path.write_bytes(base64.b64decode(image['data'],validate=True))
        print('Saved actual viewport:',path,flush=True)
    (EVIDENCE/'viewpoints.json').write_text(json.dumps(VIEWS,indent=2),encoding='utf-8')

if __name__=='__main__':
    main()
