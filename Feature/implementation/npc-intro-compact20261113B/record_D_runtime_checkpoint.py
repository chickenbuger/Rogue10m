"""Read-only preservation and startup evidence while owned D remains at menu.

Does not send input, close processes, create profiles or retry CUA approval.
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import types

R = Path(__file__).resolve().parents[3]
E = R / 'Feature/doc/evidence/npc-intro-compact20261113B'
P = R / 'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw = P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == '0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c = types.ModuleType('runtime_checkpoint_inventory'); c.__file__ = str(P)
exec(compile(raw, str(P), 'exec'), c.__dict__)
output = E / 'D-runtime01-checkpoint.json'
assert not output.exists()
owner = c.load(c.stable_bytes(E / 'internal-windows-D-owner-result.json'))
seal = c.read_json_pin(owner['seal'])
expected = c.read_json_pin(seal['input_manifest'])
actual = c.input_inventory(R)
print('Completed full current input inventory: ' + str(len(actual['files'])), flush=True)
with (E / 'D-runtime01-inputs.json').open('x', encoding='utf-8', newline='\n') as stream:
    json.dump(actual, stream, indent=2); stream.write('\n')
old = {f['path']: f for f in expected['files']}
new = {f['path']: f for f in actual['files']}
changes = [{'path': p, 'expected': old.get(p), 'current': new.get(p)}
           for p in sorted(old.keys() | new.keys()) if old.get(p) != new.get(p)]
saves = []
for protected in seal['protected_saves']:
    before = c.read_json_pin(protected['manifest'])
    after = c.scan_tree(protected['root'], True)
    saves.append({'root': protected['root'], 'unchanged': before == after, 'files': len(after['files'])})
editor = []
for f in seal['editor_products']:
    current = c.file_pin(f['path'])
    editor.append({'expected': f, 'current': current, 'unchanged': f == current})
payloads = []
for dirname, sha in [
    ('InternalWindows20261113E5_UNVERIFIED', '431968EB98AA22D819C458AABCB347E55A4C74172417A78926C16D7E13A1079C'),
    ('InternalWindows20261113Native51IntroB_UNVERIFIED', '2454269EB43A7A7FFFFC8F1F7B2A0D2016BDF90A9C0E49345E76CC2106B9F1A4'),
    ('InternalWindows20261113Native51IntroD_UNVERIFIED', 'CF46302DB0FE392ED7BCC3F57E426F468D461FF9C1906463C45778FF9DC137FA'),
]:
    folder = R / 'tmp' / dirname / 'Windows'
    inventory = c.payload_manifest(folder)
    current = c.digest(c.canonical(inventory))
    payloads.append({'root': str(folder), 'files': len(inventory), 'expected_sha256': sha,
                     'current_sha256': current, 'unchanged': current == sha})
launch = c.load(c.stable_bytes(E / 'package-D-runtime01-launch.json'))
user = Path(launch['user_dir'])
screenshots = sorted((user / 'Saved/Screenshots/Windows').glob('*.png'))
assert len(screenshots) == 1 and screenshots[0].name == 'ScreenShot00000.png'
native = c.file_pin(screenshots[0])
visual = E / 'PackagedDVisual'
visual.mkdir(exist_ok=True)
copy = visual / '01-native-menu-startup.png'
assert not copy.exists()
shutil.copy2(screenshots[0], copy)
assert c.file_pin(copy)['sha256'] == native['sha256']
log = c.stable_bytes(E / 'package-D-runtime01.log')
snapshot = E / 'D-runtime01-startup-log-snapshot.log'
with snapshot.open('xb') as stream:
    stream.write(log)
lines = log.decode('utf-8-sig', 'replace').splitlines()
keys = ('GameScreenshotSaveDirectory =', 'Final Value(s)', '[PlatformFeatures]',
        'LoadMap:', 'Bringing World', 'Game class is', 'LogExit: Exiting.', 'QUIT_MENU')
selected = [{'line': i + 1, 'text': line} for i, line in enumerate(lines) if any(k in line for k in keys)]
qa = c.scan_tree(user / 'Saved/SaveGames', True)
preserved = actual == expected and all(s['unchanged'] for s in saves + editor + payloads)
processes = c.load(c.stable_bytes(E / 'D-runtime01-process-observation.json'))
result = {
    'status': 'ACTUAL_PACKAGE_MENU_STARTUP_OBSERVED_UI_ACCESS_TIMED_OUT',
    'recorded_utc': datetime.now(timezone.utc).isoformat(), 'build': 'Native51IntroD01',
    'game': launch['expected_child_game'], 'process_observation': processes,
    'still_running_at_process_observation': any(p['ProcessId'] == 54244 for p in processes['processes']),
    'normal_exit_in_log_snapshot': any('LogExit: Exiting.' in line for line in lines),
    'full_input_preservation': preserved, 'input_files': len(actual['files']),
    'input_manifest_exact': actual == expected, 'changed_input_files': changes,
    'editor_products': editor, 'protected_saves': saves, 'preserved_payloads': payloads,
    'routing_probe': {
        'expected_UserDir': str(user), 'expected_saved_root': str(user / 'Saved'),
        'actual_native_screenshot': native, 'raw_evidence_copy': c.file_pin(copy),
        'menu_visual': 'Root inspected actual engine-generated PNG: title ROGUE10M and start/quit buttons visible.',
        'backend': 'Native GetIni final PlatformFeatures section has no SaveGameSystemModule value; installed Windows/generic source fallback supports ProjectSavedDir/SaveGames.',
        'actual_save_IO': 'UNVERIFIED: no profile creation or save writes performed',
        'qa_save_files': len(qa['files']), 'qa_save_inventory': qa,
    },
    'startup_log_snapshot': c.file_pin(snapshot), 'selected_native_log_lines': selected,
    'snapshot_red_lines': [x for x in lines if c.RED.search(x)],
    'CUA': {'window_inventory_observed': True, 'game_window_id': 9964506,
            'bind_action': 'cua.getApp({windowId:9964506})',
            'outcome': 'Computer Use app approval timed out after 301 seconds',
            'GUI_inputs_sent': 0, 'retry_or_GUI_automation_workaround': False,
            'history_check': 'read_messages was attempted once after the timeout; aborted by user, no result/approval evidence returned'},
    'unverified': ['profile creation and persistence', 'actual save IO/routing',
                   'packaged NPC collection/reward/new run', 'quit cancel/confirm/normal exit/restart',
                   'UI7/save failure', 'physical routes/growth/full run', 'global ABI'],
    'release_eligible': False, 'forced_process_termination': False,
    'denied_root_startup_or_metadata_clone_actions': 0,
}
with output.open('x', encoding='utf-8', newline='\n') as stream:
    json.dump(result, stream, indent=2); stream.write('\n')
print(json.dumps({'status': result['status'], 'preserved': preserved,
                  'inputs': len(actual['files']), 'changed': len(changes),
                  'QA_save_files': len(qa['files']), 'native_menu_png': str(copy)}, indent=2))
