"""Serialize official Unreal MCP calls and capture matched StartHub ground views."""
import argparse
import base64
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/village-ground-20261005'
EVIDENCE.mkdir(parents=True, exist_ok=True)
VIEWS = {
    'grass': {'location': {'x': 950, 'y': -1900, 'z': 175}, 'rotation': {'pitch': -47, 'yaw': 100, 'roll': 0}},
    'path': {'location': {'x': 0, 'y': -2200, 'z': 175}, 'rotation': {'pitch': -37, 'yaw': 90, 'roll': 0}},
    'plaza': {'location': {'x': 720, 'y': -920, 'z': 175}, 'rotation': {'pitch': -35, 'yaw': 95, 'roll': 0}},
}


def call(tool, arguments, name):
    argument_file = EVIDENCE / (name + '-args.json')
    output = EVIDENCE / (name + '.json')
    argument_file.write_text(json.dumps(arguments, indent=2), encoding='utf-8')
    result = subprocess.run([sys.executable, str(ROOT / 'Scripts/InvokeUnrealMCP.py'),
                             '--tool', tool, '--arguments-file', str(argument_file),
                             '--output', str(output), '--timeout', '60'],
                            capture_output=True, text=True, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(name + ': ' + result.stdout[-2000:] + result.stderr[-1000:])
    response = json.loads(output.read_text(encoding='utf-8'))
    if response.get('error') or response['result'].get('isError'):
        raise RuntimeError('MCP rejected ' + name)
    bodies = [json.loads(c['text']) for c in response['result'].get('content', []) if c['type'] == 'text']
    if len(bodies) != 1:
        raise RuntimeError('Unexpected MCP response ' + name)
    print('MCP', name, 'completed', flush=True)
    return bodies[0].get('returnValue', bodies[0])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--phase', choices=['before', 'after'], required=True)
    phase = parser.parse_args().phase
    command = (
        "import unreal; levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); "
        "assert not levels.is_in_play_in_editor(); "
        "assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()"
        ".get_path_name().split('.')[0]=='/Game/Rogue10m/Maps/StartHub'; "
        "unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([]); "
        "levels.editor_set_game_view(True)"
    )
    preparation = subprocess.run([sys.executable, str(ROOT / 'Scripts/RunEditorRemote.py'),
                                  '--command', command, '--timeout', '12'],
                                 capture_output=True, text=True, encoding='utf-8')
    if preparation.returncode:
        raise RuntimeError('Capture preparation: ' + preparation.stdout[-2000:])
    for name, view in VIEWS.items():
        transform = {**view, 'scale': {'x': 1, 'y': 1, 'z': 1}}
        value = call('call_tool', {'toolset_name': 'EditorToolset.EditorAppToolset',
                                  'tool_name': 'CaptureViewport',
                                  'arguments': {'captureTransform': transform, 'annotations': None, 'bShowUI': False}},
                     phase + '-capture-' + name)
        picture = value['image']
        if picture['mimeType'] != 'image/png':
            raise RuntimeError('Unexpected screenshot format')
        (EVIDENCE / (phase + '-' + name + '.png')).write_bytes(base64.b64decode(picture['data'], validate=True))
    (EVIDENCE / 'viewpoints.json').write_text(json.dumps(VIEWS, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
