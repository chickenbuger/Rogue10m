"""Freeze the current saved Hub entry for a new, non-executed Windows artifact."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import subprocess
import types

R = Path(__file__).resolve().parents[3]
E = R / 'Feature/doc/evidence/hub-entry-windows20261114A'
T = R / 'Feature/implementation/native51-internal-smoke-cook-fix-f'
H = R / 'Feature/doc/evidence/hub-compact-entry20261113A'
N = R / 'Feature/doc/evidence/npc-intro-compact20261113B'
A = Path('C:/Users/PC/AppData/Local/Temp/R10HubEntryWindows20261114A')
C = A / 'capture01'
raw = (T / 'internal_smoke.py').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == '5ACCE0EB5443ED9C5DB9ECDF8B76BB227E1DE4E66771114DAB3A1DD3102EA192'
m = types.ModuleType('hub_windows_smoke'); m.__file__ = str(T / 'internal_smoke.py')
exec(compile(raw, m.__file__, 'exec'), m.__dict__)
c = m.helpers()
assert not E.exists() and not A.exists(), 'Preserve every prior output; choose a genuinely new run if needed.'
assert c.physical(R / 'Content', True) == R / 'Content'
E.mkdir(parents=True); A.mkdir()

def write(path, data):
    with path.open('x', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, ensure_ascii=False, indent=2); f.write('\n')
    return c.file_pin(path)

d_owner = c.load(c.stable_bytes(N / 'internal-windows-D-owner-result.json'))
d_seal = c.read_json_pin(d_owner['seal'])
for pin in d_seal['runtime'].values(): c.read_pin(pin)
ps = r'''$rows=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')} | Select-Object ProcessId,ParentProcessId,Name,ExecutablePath,CommandLine);ConvertTo-Json -InputObject $rows -Depth 4 -Compress'''
q = subprocess.run([d_seal['runtime']['powershell']['path'], '-NoProfile', '-Command', ps], capture_output=True, text=True, encoding='utf-8', timeout=30, check=True)
processes = json.loads(q.stdout)
write(E / 'processes-before.json', {'observed_utc': datetime.now(timezone.utc).isoformat(), 'processes': processes, 'input_or_termination': 0})
assert processes == [], 'Another engine/build/game exists; preserve it and stop.'
expected = c.load(c.stable_bytes(H / 'inputs-after.json'))
current = c.input_inventory(R)
write(E / 'inputs-reconnected.json', current)
c.compare_inventory(expected, current, 'reconnected Hub input identity')
assert len(current['files']) == 10472
assert sum(p['path'].startswith(('Source/', 'Config/')) for p in current['files']) == 368
for pin in d_seal['editor_products']: c.read_pin(pin)
assert len(d_seal['editor_products']) == 6
print('Reconnected full inputs10472 and Editor6 match.', flush=True)
preserved = []
for name, receipt_path in (
    ('E5', R / 'Feature/doc/evidence/map-resume-20261113/InternalSmokeD/persistent-delivery05.json'),
    ('B', N / 'internal-windows-delivery.json'),
    ('D', N / 'internal-windows-D-delivery.json'),
):
    receipt = c.load(c.stable_bytes(receipt_path))
    roots = []
    for key in ('archive', 'source_archive'):
        root = c.physical(receipt[key], True)
        assert not c.child(A, root) and not c.child(root, A) and root != A
        payload = c.payload_manifest(root)
        assert c.digest(c.canonical(payload)) == receipt['payload_sha256'], (name, key)
        roots.append({'root': str(root), 'payload_sha256': receipt['payload_sha256'], 'files': len(payload)})
    preserved.append({'name': name, 'receipt': c.file_pin(receipt_path), 'copies': roots})
    print('Preserved both physical '+name+' payloads.', flush=True)
cp = c.load(c.stable_bytes(N / 'D-C-preservation.json'))
c_pairs = []
for row in cp['unchanged_original_copy_pairs']:
    for key in ('source', 'copy'):
        assert c.file_pin(row[key])['sha256'] == row['sha256']
    c_pairs.append(row)
prior_records = []
for area in (
    Path('C:/Users/PC/AppData/Local/Temp/R10Native51IntroSmoke20261113B'),
    Path('C:/Users/PC/AppData/Local/Temp/R10Native51IntroSmoke20261113C'),
    Path('C:/Users/PC/AppData/Local/Temp/R10Native51IntroSmoke20261113D'),
    Path('C:/Users/PC/AppData/Local/Temp/R10InternalSmoke20261113D/run05'),
):
    for pattern in ('*.lease', 'wrapper-journal.json', 'native-owner-release.json', 'smoke.sealed.json'):
        for path in sorted(area.rglob(pattern)):
            prior_records.append(c.file_pin(path))
assert len({p['path'] for p in prior_records}) == len(prior_records)
old_game = [c.file_pin(R / 'Binaries/Win64' / name) for name in ('Rogue10m.exe', 'Rogue10m.pdb', 'Rogue10m.target')]
d_delivery = c.load(c.stable_bytes(N / 'internal-windows-D-delivery.json'))
assert old_game[0]['sha256'] == d_delivery['game']['sha256']
assert old_game[1]['sha256'] == '919BDF0745E2A3E23C8AC9CECD98CAE584BFA8E06CA746C515F3D1E88D919BC5'
maps = list(d_seal['cook_maps'])
assert len(maps) == 6
m.capture(c, str(R), d_seal['engine'], str(A), str(C), maps, d_seal['runtime']['powershell']['path'])
s = c.load(c.stable_bytes(C / 'smoke.UNSEALED.json'))
captured = c.read_json_pin(s['input_manifest'])
c.compare_inventory(current, captured, 'new capture inputs')
assert s['native51'] == d_seal['native51'] and s['editor_build'] == d_seal['editor_build']
assert s['editor_products'] == d_seal['editor_products']
old_hub = c.load(c.stable_bytes(H / 'physical-checkpoint.json'))
save_proofs = []
for save in s['protected_saves']:
    actual = c.read_json_pin(save['manifest'])
    old = next(v for v in old_hub['protected_saves'] if v['root'] == save['root'])
    c.compare_inventory(old['inventory'], actual, 'protected saves since Hub authoring')
    save_proofs.append({'root': save['root'], 'files': len(actual['files']), 'manifest': save['manifest']})
out = A / 'run01'; assert not out.exists(); out.mkdir()
s.update(status='sealed', run_tag='HubEntry20261114A01', output_root=str(out), outputs={k: str(out / k) for k in c.OUTPUTS}, smoke_map_selection_final=True)
s['required_iostore_packages'] = maps + ['/Game/Rogue10m/World/Production/WorldLayoutCompact20261113A/Quests/DA_IntroSupply_Compact20261113B']
s['content_state'] = {
    'status': 'OWNER_RECORDED_FINAL', 'map_extent_m': [1800, 1800],
    'npc_placement': 'Saved/cold-reloaded Serin IntroSupply DA+Director; prior Editor-game accept/collect2/reward40 once/duplicate/new-Run observed. No packaged NPC play certified.',
    'default_map': 'L_Menu/StartHub unchanged. Additive native Compact Hub entry1 plus visual actors5 at (-1440,4270,180)cm; old3 portals unchanged; StartRun=1 with no GameMode override. Both production maps explicitly cooked.',
    'build_ABI_correction': 'Reuse reviewed FixF: compile-only UBT -NoLiveCoding override absent; runtime/cooker -NoLiveCoding retained. New binary identity/ABI must be checked; no new Game execution authorized in this packet.',
    'hub_entry_new_external_actor_files': c.load(c.stable_bytes(H / 'final-result.json'))['added_content_files'],
    'remaining': ['Landscape1800m not cropped', 'Actual new Hub travel/RunId/timer/F/NPC/saveIO/quit/restart UNVERIFIED', 'New content rendering UNVERIFIED', 'Korean TextRender tofu and existing gray player/vendor dependency remain', 'Physical direct route15min growth20min full run UNVERIFIED', 'Separate denied startup approval and metadata/clone QA not executed'],
    'release_complete': False,
}
frozen = {
    'schema': 'R10-native51-content-freeze-v1', 'status': 'frozen_for_internal_smoke', 'project': s['project'],
    'active_destination': s['active_destination'], 'input_manifest_sha256': s['input_manifest']['sha256'],
    'map_manifest_sha256': s['map_manifest']['sha256'], 'editor_dll_sha256': s['editor_products'][0]['sha256'],
    'recorded_utc': datetime.now(timezone.utc).isoformat(), 'recorder': 'Actual Root integration owner; saved Source Hub content identity only',
    'content_state': s['content_state'], 'evidence': [c.file_pin(H / name) for name in ('final-result.json', 'author02-result.json', 'collision01-result.json', 'cold03-result.json', 'supplemental-review.json')] + [c.file_pin(N / name) for name in ('runtime-final-result.json', 'post-game-preservation.json', 'D-abi-reviewed.json')],
    'note': 'Current artifact-only input freeze, not root-startup approval, metadata/clone QA permission, raw test execution, Game access approval or release certification.',
}
s['content_freeze'] = write(C / 'content-freeze.recorded.json', frozen)
seal_pin = write(C / 'smoke.sealed.json', s)
summary = {
    'status': 'ACTUAL_HUB_INPUTS_SEALED_INTERNAL_DEVELOPMENT_NOT_EXECUTED', 'recorded_utc': datetime.now(timezone.utc).isoformat(),
    'seal': seal_pin, 'input_manifest': s['input_manifest'], 'map_manifest': s['map_manifest'], 'content_freeze': s['content_freeze'],
    'output_root': str(out), 'prior_artifacts': preserved, 'prior_records': prior_records, 'C_original_copy_pairs': c_pairs,
    'prior_source_game_products': old_game, 'editor_products': s['editor_products'], 'protected_saves': save_proofs,
    'native51': s['native51'], 'input_files': len(captured['files']), 'source_config_files': 368,
    'game_processes_observed': processes, 'new_game_or_GUI_operations': 0, 'release_eligible': False,
}
write(E / 'content-checkpoint.json', summary)
print(json.dumps({k: summary[k] for k in ('status', 'seal', 'input_files', 'source_config_files', 'output_root')}, indent=2), flush=True)
