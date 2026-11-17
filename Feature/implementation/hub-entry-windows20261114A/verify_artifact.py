"""Inspect a built artifact using official offline IoStore tools; never run Game."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re
import subprocess
import sys
import types

R = Path(__file__).resolve().parents[3]
E = R / 'Feature/doc/evidence/hub-entry-windows20261114A'
N = R / 'Feature/doc/evidence/npc-intro-compact20261113B'
H = R / 'Feature/doc/evidence/hub-compact-entry20261113A'
T = R / 'Feature/implementation/native51-internal-smoke-cook-fix-f'
RESUME = sys.argv[1:] == ['--resume-iostore-identity']
assert RESUME or not sys.argv[1:]
raw = (T / 'strict_primitives.py').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == '0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c = types.ModuleType('hub_artifact'); c.__file__ = str(T / 'strict_primitives.py')
exec(compile(raw, c.__file__, 'exec'), c.__dict__)

def write(path, value):
    with path.open('x', encoding='utf-8') as f: json.dump(value, f, indent=2)
    return c.file_pin(path)

def run(name, argv):
    if RESUME and name in ('iostore-listing', 'iostore-identity'):
        saved = c.load(c.stable_bytes(E / (name+'-process.json')))
        assert saved['args'] == argv and saved['exit_code'] == 0
        for pin in c.load(c.stable_bytes(E / 'identity-reuse.json'))['files']: c.read_pin(pin)
        return c.stable_bytes(E / (name+'.stdout.log')), c.stable_bytes(E / (name+'.stderr.log'))
    assert not (E / (name+'-process.json')).exists()
    started = datetime.now(timezone.utc)
    p = subprocess.Popen(argv, cwd=R, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
    stdout, stderr = p.communicate()
    (E / (name+'.stdout.log')).write_bytes(stdout)
    (E / (name+'.stderr.log')).write_bytes(stderr)
    write(E / (name+'-process.json'), {'started_utc': started.isoformat(), 'ended_utc': datetime.now(timezone.utc).isoformat(), 'pid': p.pid, 'args': argv, 'exit_code': p.returncode, 'new_Game_or_GUI': 0})
    assert p.returncode == 0, (name, p.returncode)
    assert not c.RED.search((stdout+b'\n'+stderr).decode('utf-8', errors='replace')), name
    return stdout, stderr

owner = c.load(c.stable_bytes(E / 'owner-result.json'))
assert owner['exit_code'] == 0 and owner['execute_requested'] is True
s = c.read_json_pin(owner['seal'])
assert s['run_tag'] == 'HubEntry20261114A01'
ev = Path(s['outputs']['evidence'])
post = c.load(c.stable_bytes(ev / 'postflight.json'))
assert post['status'] == 'internal_smoke_artifact_built_UNVERIFIED'
assert post['seal_sha256'] == owner['seal']['sha256'] and post['release_eligible'] is False
listing = post['next_iostore']; assert len(listing['required_packages']) == 7
tool = next(pin for pin in s['tool_pins'] if pin['path'] == listing['exe']); c.read_pin(tool)
csv = ev / 'iostore.csv'
assert csv.is_file() if RESUME else not csv.exists()
run('iostore-listing', [listing['exe'], *listing['args']])
assert csv.exists()
argv = [sys.executable, '-I', '-B', '-S', '-X', 'utf8', str(T / 'internal_smoke.py'), '--seal', owner['seal']['path'], '--seal-sha256', owner['seal']['sha256'], '--release', owner['release']['path'], '--release-sha256', owner['release']['sha256'], '--owner-id', 'RootHubEntryA', '--verify-iostore', str(csv)]
stdout, stderr = run('iostore-identity', argv)
identity = json.loads(stdout.decode('utf-8-sig'))
assert not stderr.strip() and identity['status'] == 'iostore_csv_identity_only_UNVERIFIED'
windows = Path(s['outputs']['archive']) / 'Windows'
pak = windows / 'Rogue10m/Content/Paks'
global_utoc = pak / 'global.utoc'; assert global_utoc.exists()
dump = E / ('hub-cooked-describe-B.txt' if RESUME else 'hub-cooked-describe.txt'); assert not dump.exists()
describe_args = [listing['exe'], 'IoStore', '-Describe='+str(global_utoc), '-PackageFilter=/Game/Rogue10m/Maps/StartHub*', '-DumpToFile='+str(dump), '-IncludeExportHashes', '-unattended', '-utf8output', '-DisablePython', '-NoLiveCoding', '-UserDir='+s['outputs']['qa_user'], '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False']
run('hub-describe-B' if RESUME else 'hub-describe', describe_args)
text = dump.read_text(encoding='utf-8-sig')
packages = re.split(r"Package '([^']+)' Summary", text)
assert len(packages) > 1, 'Official Describe package summaries missing'
exports = []
for i in range(1, len(packages), 2):
    package, body = packages[i:i+2]
    if not package.startswith('/Game/Rogue10m/Maps/StartHub'): continue
    for match in re.finditer(r"\bExport (\d+): '([^']+)'(.*?)(?=\bExport \d+: '|Export Bundle|\Z)", body, re.S):
        index, name, block = match.groups()
        fields = {}
        for key in ('OuterIndex', 'ClassIndex', 'TemplateIndex', 'ExportHash', 'Offset', 'Size'):
            values = re.findall(r'(?m)^\s*'+key+r':\s*(.*?)\s*$', block)
            assert len(values) == 1, (package, name, key)
            fields[key] = values[0]
        assert re.fullmatch(r'[A-Fa-f0-9]{40}', fields['ExportHash'])
        assert int(fields['Size']) >= 0 and int(fields['Offset']) >= 0
        commands = re.findall(r"\b(Create|Serialize):\s*"+index+r" '"+re.escape(name)+r"'", body)
        exports.append({'package': package, 'index': int(index), 'name': name, 'bundle_commands': commands, **fields})
by_index = {(row['package'], row['index']): row for row in exports}
assert len(by_index) == len(exports), 'Duplicate local export indices'

def ancestors(row):
    result = []; seen = {row['index']}; value = row['OuterIndex']
    while re.fullmatch(r'\d+', value):
        index = int(value)
        assert index not in seen, 'Cyclic local export OuterIndex'
        seen.add(index)
        parent = by_index[(row['package'], index)]
        result.append(parent)
        value = parent['OuterIndex']
    return result

def native_class(row):
    found = re.fullmatch(r"0x[A-Fa-f0-9]+ '([^']+)'", row['ClassIndex'])
    assert found, ('Unresolved exact script class', row)
    return found[1].replace('.', '/').casefold()

def required_serialized(row):
    assert int(row['Size']) > 0 and row['ExportHash'] != '0'*40
    assert row['bundle_commands'] == ['Create', 'Serialize']
cold = c.load(c.stable_bytes(H / 'cold03-result.json'))
matched = []
for actor in cold['new_actors']:
    name = actor['path'].rsplit('.', 1)[-1]
    found = [row for row in exports if row['name'] == name]
    assert len(found) == 1, (actor['label'], len(found))
    row = found[0]
    assert native_class(row) == actor['class'].replace('.', '/').casefold()
    required_serialized(row)
    assert any(value['name'] == 'PersistentLevel' for value in ancestors(row))
    components = [value for value in exports if value['package'] == row['package'] and any(parent['index'] == row['index'] for parent in ancestors(value))]
    assert components, ('No cooked actor components', actor['label'])
    expected_component = {'Rogue10mRunPortal': '/script/engine/boxcomponent', 'StaticMeshActor': '/script/engine/staticmeshcomponent', 'TextRenderActor': '/script/engine/textrendercomponent'}[actor['class'].rsplit('.', 1)[-1]]
    essential = [value for value in components if native_class(value) == expected_component]
    assert len(essential) == 1, (actor['label'], expected_component, essential)
    required_serialized(essential[0])
    matched.append({'source_actor': actor, 'cooked_export': row, 'component_exports': components})
old_portals = c.load(c.stable_bytes(H / 'inspect03-result.json'))['old_portals']
old_matched = []
for actor in old_portals:
    name = actor['path'].rsplit('.', 1)[-1]
    found = [row for row in exports if row['name'] == name]
    assert len(found) == 1, (actor['label'], len(found))
    assert native_class(found[0]) == actor['class'].replace('.', '/').casefold()
    required_serialized(found[0])
    old_matched.append({'source_portal': actor, 'cooked_export': found[0]})
io_result = {'status': 'OFFICIAL_IOSTORE_MAP_DA_AND_HUB_ACTOR_IDENTITIES_CONFIRMED_PROPERTIES_NOT_DECODED', 'identity': identity, 'source_cold': c.file_pin(H / 'cold03-result.json'), 'input_manifest': s['input_manifest'], 'postflight': c.file_pin(ev / 'postflight.json'), 'describe': c.file_pin(dump), 'new_actor_exports': matched, 'prior_portal_exports': old_matched, 'typed_properties': 'Source cold03 only: target/StartRun contract/NoCollision/text/material are not independently decoded by Describe; exact input-cook lineage is recorded.', 'new_game_or_GUI': 0, 'release_eligible': False}
write(E / 'iostore-hub-result.json', io_result)
chain = {}
for name in ('Rogue10m.exe', 'Rogue10m.pdb'):
    pins = [c.file_pin(root / name) for root in (R / 'Binaries/Win64', Path(s['outputs']['stage']) / 'Windows/Rogue10m/Binaries/Win64', windows / 'Rogue10m/Binaries/Win64')]
    assert len({p['sha256'] for p in pins}) == 1
    chain[name] = pins
frozen = c.read_json_pin(s['content_freeze'])
d_abi_pin = next(pin for pin in frozen['evidence'] if pin['path'] == str(N / 'D-abi-reviewed.json'))
d_abi = c.read_json_pin(d_abi_pin)
same = chain['Rogue10m.exe'][0]['sha256'] == d_abi['exe']['sha256'] and chain['Rogue10m.pdb'][0]['sha256'] == d_abi['pdb_chain'][0]['sha256']
if same:
    assert d_abi['status'] == 'ROOT_REVIEWED_ACTUAL_D_SAMPLED_ABI_CONSISTENT_NOT_STARTUP_OR_GLOBAL_CERTIFICATION'
    assert d_abi['row_bytes'] == 48 and d_abi['info_bytes'] == 32 and d_abi['version_info_bytes'] == 16
    assert d_abi['manual_disassembly_review']['RSI_plus48_VA']
    abi_status = 'EXACT_EXE_AND_PDB_BYTES_MATCH_D_REVIEWED_SAMPLE_NO_NEW_ABI_EXECUTION'
else:
    abi_status = 'NEW_LINK_DIFFERENT_BYTES_REQUIRES_NEW_SAMPLED_ABI_REVIEW'
write(E / 'binary-chain.json', {'status': abi_status, 'chain': chain, 'matches_D_exe_and_pdb': same, 'D_review': c.file_pin(N / 'D-abi-reviewed.json'), 'new_game_or_GUI': 0})
print(json.dumps({'status': io_result['status'], 'new_actor_exports': len(matched), 'prior_portals': len(old_matched), 'export_rows': identity['export_rows'], 'world_partition_cells': len(identity['world_partition_cells']), 'binary_status': abi_status}, indent=2))
