"""Verify retained live QA readbacks and original map integrity after Editor exit."""
import importlib.util
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('map_maker_tests', root / 'Scripts/RunMapMakerTests.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def read(name):
    return json.loads((evidence / name).read_text(encoding='utf-8-sig'))

before = read('original-map-before.json')
after = module.snapshot(root)
changed = sorted(k for k in before.keys() | after.keys() if before.get(k) != after.get(k))
assert len(before) == len(after) == 1260 and not changed
baseline = read('live-baseline-final.json')
final = read('live-final.json')
assert baseline['world'] == final['world'] == '/Game/Rogue10m/Maps/OpenRunMap.OpenRunMap'
assert baseline['actor_count'] == final['actor_count'] == 1243
assert not final['maker_actors'] and not final['dirty_maps'] and not final['dirty_content']
placed = read('live-placed.json')['maker_actors']
undone = read('live-undo.json')['maker_actors']
redone = read('live-redo.json')['maker_actors']
moved = read('live-moved.json')['maker_actors']
assert len(placed) == len(redone) == len(moved) == 1 and not undone
prop, restored, destination = placed[0], redone[0], moved[0]
assert prop == restored
assert prop['mesh'] == '/Engine/BasicShapes/Cube.Cube'
assert prop['location']['x'] == 12800 and prop['location']['y'] == -64000
assert abs(prop['rotation']['yaw'] - 45) < 0.001
assert prop['scale'] == {'x': 20.0, 'y': 20.0, 'z': 20.0}
assert destination['path'] == prop['path']
assert destination['location'] == dict(prop['location'], x=38400.0)
assert destination['rotation'] == prop['rotation'] and destination['scale'] == prop['scale']
for camera in ('edge', 'camp'):
    rpc = read(f'focus-camera-{camera}-final.json')
    payload = json.loads(rpc['result']['content'][0]['text'])['returnValue']
    assert abs(payload['rotation']['pitch'] + 80) < 0.001
regression = read('automation-final/result.json')
assert regression['success'] and regression['exit_code'] == 0
assert len(regression['tests']) == 3 and set(regression['tests'].values()) == {'Success'}
status = subprocess.check_output(['git', 'status', '--porcelain', '--', 'Content'], cwd=root, text=True)
assert not status.strip(), status
result = {
    'success': True,
    'original_map_file_count': len(after),
    'changed_original_map_files': changed,
    'content_git_status': status,
    'final_actor_count': final['actor_count'],
    'final_map_maker_actor_count': len(final['maker_actors']),
    'final_dirty_map_count': len(final['dirty_maps']),
    'final_dirty_content_count': len(final['dirty_content']),
    'live_readback_assertions': 'placement, keyboard undo/redo, move, edge/camp focus, cleanup passed',
    'regression': regression['tests'],
    'screen_checks': 'See feature document: screenshots were inspected directly during GUI actions.'
}
(evidence / 'original-map-after.json').write_text(json.dumps(after, indent=2), encoding='utf-8')
(evidence / 'final-checks.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
