"""Plan-default Root-only commandlet owner. Never selects actors or creates approval.

Every executing phase is explicit. Timeout preserves the live owned process and
blocks further work. Existing E201 fixture and source inputs are never edited.
"""
from __future__ import annotations
import argparse
import base64
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import runpy
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ORIGIN = Path('D:/Project/Rogue10m')
PROJECT = Path('C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/Project')
ENGINE = Path('D:/Program Files/UE_5.8')
FIXTURE = ORIGIN / 'Feature/implementation/map-resume-20261113/world-map-maker-nongui-review/map_maker_api_fixture.py'
FIXTURE_SHA = 'E201B2F5A1E3816639D0809E5977F2F51247977D6CCE3570D2C2658BBF993D13'
PHYSICAL = PROJECT.parent / 'physical-clone-receipt.json'
PHYSICAL_SHA = '8AF525EA9D9A0132A3742D9D0E1D6EDEDAD24183B1366572DA53D4C3A063CA18'
AUDIT = ORIGIN / 'Feature/implementation/map-resume-20261113/world-map-maker-startup-audit/DELIVERABLES.sha256.json'
AUDIT_SHA = '1ADF625535F9DDEF28916AB4922C5298071ADF20987ACB028DB6832656609E49'
HOST = ORIGIN / 'Feature/doc/evidence/map-resume-20261113/InternalSmokeD/map-maker-host-startup-context.json'
HOST_SHA = '7D35ADAE8CCAE0CAA52ADABD5D2C9466503E5DCF7C12803AF73DEF4FE7E32A7E'
SOURCE_MAP = '/Game/Rogue10m/Maps/OpenRunMap'
HASH = re.compile(r'[0-9A-Fa-f]{64}')
NATIVE_PHASES = ('startup', 'duplicate', 'discover', 'move', 'cold_verify')


class Blocked(RuntimeError):
    pass


def need(value, message):
    if not value:
        raise Blocked(message)


def utc():
    return datetime.now(timezone.utc).isoformat()


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest().upper()


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode()).hexdigest().upper()


def physical(path):
    path = Path(os.path.abspath(path))
    for part in (path, *path.parents):
        if os.path.lexists(part):
            s = part.lstat()
            need(not part.is_symlink() and not getattr(s, 'st_file_attributes', 0) & 0x400, 'Reparse/link path: ' + str(part))
    return path


def pin(path, expected):
    path = physical(path)
    need(HASH.fullmatch(expected or '') and sha(path) == expected.upper(), 'Pin differs: ' + str(path))
    return path


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def pinned_json(ref):
    return load(pin(ref['path'], ref['sha256']))


def write_new(path, value, text=False):
    path = physical(path)
    need(path.parent.is_dir(), 'Create owned parent explicitly first: ' + str(path.parent))
    with path.open('x', encoding='utf-8', newline='\n') as f:
        if text:
            f.write(value)
        else:
            json.dump(value, f, ensure_ascii=False, indent=2, allow_nan=False)
            f.write('\n')


def fixture_api():
    pin(FIXTURE, FIXTURE_SHA)
    # E201 has no top-level engine, process or filesystem mutation. __main__ is not called.
    return runpy.run_path(str(FIXTURE), run_name='_pinned_owner_fixture_api')


def session_path(config):
    session = physical(config['session_root'])
    need(session.parent == PROJECT.parent and re.fullmatch(r'OwnerRun_[A-Za-z0-9]{1,40}', session.name), 'Exact QA sibling OwnerRun_<ASCII> required.')
    need(not re.search(r'[\s"\r\n]', str(session)), 'Owner wrapper path must contain no whitespace or quotes.')
    return session


def reservation(config):
    state = load(session_path(config) / 'reservation.json')
    need(state['schema'] == 'R10-mapmaker-owner-reservation-v1' and state['config_sha256'] == config['_sha'], 'Reservation/config differs.')
    fixture_api()['package_paths'](state['candidate_map'])
    return state


def candidate_absent(candidate):
    for relative in fixture_api()['package_paths'](candidate):
        need(not os.path.lexists(PROJECT / relative), 'Destination/partial target exists; do not retry: ' + relative)


def audit_pins():
    manifest = load(pin(AUDIT, AUDIT_SHA))
    need(len(manifest['entries']) == 13, 'Frozen startup audit file set differs.')
    for item in manifest['entries']:
        relative = Path(item['path'])
        need(not relative.is_absolute() and '..' not in relative.parts, 'Audit relative path escaped.')
        pin(AUDIT.parent / relative, item['sha256'])
    host = load(pin(HOST, HOST_SHA))
    need(not os.environ.get('UE_PYTHONPATH') and host.get('UE_PYTHONPATH') is None, 'UE_PYTHONPATH requires a new owner audit.')
    need(not physical(host['user_python_root']).exists(), 'Observed host user Python root changed; re-audit required.')
    inventory = load(AUDIT.parent / manifest['primary_inventory'])
    rows = [inventory['uproject']] + inventory['config_pins'] + inventory['python_files'] + inventory['pth_files'] + inventory['source_pins']
    rows += [x['descriptor'] for x in inventory['plugin_descriptors']]
    rows += [x['pin'] for x in inventory['embedded_boot_candidates'] if x['pin']]
    inputs = {}
    for row in rows:
        pin(row['path'], row['sha256'])
        need(Path(row['path']).stat().st_size == row['size'], 'Recorded startup input size differs.')
        inputs[row['path']] = row['sha256']
    recorded_python = {str(physical(x['path'])) for x in inventory['python_files'] + inventory['pth_files']}
    observed_python = set()
    for row in inventory['roots']:
        root = physical(row['path'])
        need(root.is_dir() == row['exists'], 'Recorded startup root presence changed.')
        if root.is_dir():
            for directory, folders, names in os.walk(root, followlinks=False):
                folders[:] = [x for x in folders if x not in ('__pycache__', 'Lib', '.git')]
                physical(directory)
                for name in folders:
                    physical(Path(directory) / name)
                for name in names:
                    if Path(name).suffix == '.py':
                        observed_python.add(str(physical(Path(directory) / name)))
        for site in row['site_directory_candidates']:
            site_path = physical(site['path'])
            need(site_path.is_dir() == site['exists'], 'Recorded startup site directory presence changed.')
            if site_path.is_dir():
                observed_python.update(str(physical(p)) for p in site_path.glob('*.pth'))
    embedded_site = ENGINE / 'Engine/Binaries/ThirdParty/Python3/Win64/Lib/site-packages'
    if embedded_site.is_dir():
        observed_python.update(str(physical(p)) for p in embedded_site.glob('*.pth'))
    need(observed_python == recorded_python, 'Startup Python file namespace changed; new audit required.')
    for row in inventory['embedded_boot_candidates']:
        need(physical(row['path']).is_file() == row['exists'], 'Embedded startup input presence changed.')
    return digest(inputs)


def tree_snapshot(root):
    root = physical(root)
    files = {}
    if root.exists():
        need(root.is_dir(), 'Startup overlay must be a directory.')
        for directory, folders, names in os.walk(root, followlinks=False):
            physical(directory)
            for name in folders:
                physical(Path(directory) / name)
            for name in names:
                path = physical(Path(directory) / name)
                need(path.stat().st_nlink == 1, 'Shared hardlink in startup overlay.')
                files[path.relative_to(root).as_posix()] = sha(path)
    return {'exists': root.exists(), 'files': dict(sorted(files.items()))}


def startup_surface(config):
    return {'recorded_input_fingerprint': audit_pins(),
            'overlays': {str(path): tree_snapshot(path) for path in
                         (PROJECT / 'Saved/Config', PROJECT / 'Intermediate/PipInstall', physical(config['user_dir']))},
            'environment': {key: os.environ.get(key) for key in
                            ('UE_PYTHONPATH', 'PYTHONPATH', 'PYTHONHOME', 'USERPROFILE', 'LOCALAPPDATA', 'TEMP')}}


def validate_host(config):
    api = fixture_api()
    need(physical(config['project_root']) == PROJECT and physical(config['engine_root']) == ENGINE, 'Exact clone/engine roots required.')
    user = physical(config['user_dir'])
    need(user.parent == PROJECT.parent and re.fullmatch(r'User(?:_[A-Za-z0-9]{1,30})?', user.name)
         and (not user.exists() or user.is_dir()), 'Fresh separate physical User or User_<ASCII> sibling required.')
    receipt = load(pin(PHYSICAL, PHYSICAL_SHA))
    need(receipt['schema'] == 'R10-mapmaker-physical-snapshot-v1' and receipt['status'] == 'physical_snapshot_verified'
         and receipt['physical_snapshot_verified'] is True and receipt['source_before_after_unchanged'] is True
         and physical(receipt['project_root']) == PROJECT and physical(receipt['source_root']) == ORIGIN
         and receipt['buildId'] == '55116800' and receipt['selected_file_count'] == 9896, 'Physical clone receipt contract differs.')
    need(len(receipt['selected_source_and_target_file_pins']) == 9896, 'Physical receipt file count differs.')
    guard = pinned_json(config['origin_guard'])
    need(Path(guard['origin']) == ORIGIN and guard['roots'] == list(api['ROOTS']), 'Fresh original guard scope differs.')
    for entry in receipt['selected_source_and_target_file_pins']:
        relative = Path(entry['relative_path'])
        need(not relative.is_absolute() and '..' not in relative.parts, 'Physical receipt relative path escaped.')
        target = physical(PROJECT / relative)
        need(target.is_file() and target.stat().st_size == entry['size'] and target.stat().st_nlink == 1
             and not os.path.samefile(target, ORIGIN / relative), 'Clone has a missing/changed/shared physical file.')
        # Build file-open logs can change in a later owner cook. Fresh OriginGuard,
        # rather than a stale pre-cook log hash, protects the current entire source.
        if relative.parts[0] != 'Build':
            need(guard['snapshot']['files'].get(relative.as_posix(), '').upper() == entry['sha256'].upper(), 'Fresh source code/content/editor differs from the physical clone receipt.')
    exclude = api['package_paths'](reservation(config)['candidate_map']) if (session_path(config) / 'reservation.json').exists() else ()
    clone = api['protected_snapshot'](PROJECT, exclude, require_single_link=True)
    expected = {x['relative_path']: x['sha256'].upper() for x in receipt['selected_source_and_target_file_pins']}
    need(len(expected) == 9896 and {k: v.upper() for k, v in clone['files'].items()} == expected, 'Clone immutable namespace differs from its exact physical snapshot.')
    need(api['protected_snapshot'](ORIGIN) == guard['snapshot'] and api['local_save_snapshot']() == guard['local_saves'], 'Current original inputs/Game binaries or protected saves differ.')
    audit_pins()
    pin(config['executable']['path'], config['executable']['sha256'])
    need(physical(config['executable']['path']) == ENGINE / 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe', 'Exact commandlet exe required.')
    need(load(PROJECT / 'Binaries/Win64/UnrealEditor.modules')['BuildId'] == '55116800'
         and load(ENGINE / 'Engine/Binaries/Win64/UnrealEditor.modules')['BuildId'] == '55116800', 'BuildId differs.')
    return api, guard, receipt


def active_processes(powershell):
    """Executed only with --execute; query filtered UE/build/game rows, never stop one."""
    script = r'''$ErrorActionPreference='Stop'
$rows=@(Get-CimInstance Win32_Process | Where-Object {
 $_.Name -match '^(UnrealEditor.*|UnrealBuildTool|UnrealHeaderTool|AutomationTool|UnrealGame|Rogue10m.*|ShaderCompileWorker|UnrealPak|IoStore)\.exe$' -or
 ($_.Name -match '^(dotnet|cmd|powershell|pwsh)\.exe$' -and ($_.CommandLine -match '(?i)(UnrealBuildTool|AutomationTool|RunUAT\.bat|UnrealEditor(?:-Cmd)?\.exe|Rogue10m(?:-Win64-[^\\" ]+)?\.exe)' -or
 ($_.Name -match '^(dotnet|cmd)\.exe$' -and -not $_.CommandLine)))
} | Select-Object ProcessId,Name,ExecutablePath,CreationDate)
ConvertTo-Json -InputObject $rows -Compress
'''
    pin(powershell['path'], powershell['sha256'])
    query = subprocess.Popen([powershell['path'], '-NoProfile', '-NonInteractive', '-EncodedCommand',
                              base64.b64encode(script.encode('utf-16-le')).decode()], stdin=subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, creationflags=0x08000000, shell=False)
    try:
        stdout, stderr = query.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        raise Blocked('Process-query timeout; query PID ' + str(query.pid) + ' retained without killing; idle remains unverified.')
    need(query.returncode == 0, 'Cannot establish shared engine/build idle: ' + stderr[:600])
    rows = json.loads(stdout.lstrip('\ufeff'))
    need(isinstance(rows, list), 'Unexpected process inventory result.')
    need(not rows, 'Engine/build/game process active; no launch: ' + json.dumps(rows))
    return rows


def wrapper_bytes(script, args):
    return ('import runpy\nimport sys\nsys.argv = ' + repr([str(script), *map(str, args)]) +
            '\nrunpy.run_path(' + repr(str(script)) + ", run_name='__main__')\n# Fixed owner wrapper; no UI, input, selection or shell.\n")


def common_args(config, phase_dir):
    return [str(PROJECT / 'Rogue10m.uproject'), '-SCCProvider=None', '-NoSourceControl', '-Unattended', '-NullRHI',
            '-NoSound', '-NoSplash', '-NoLiveCoding', '-UserDir=' + str(physical(config['user_dir'])),
            '-EnablePlugins=PythonScriptPlugin',
            '-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:bRemoteExecution=False',
            '-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:bIsolateInterpreterEnvironment=True',
            '-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:bRunPipInstallOnStartup=False',
            '-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:bDeveloperMode=False',
            '-ini:EditorPerProjectUserSettings:[/Script/PythonScriptPlugin.PythonScriptPluginUserSettings]:EnablePythonOverride=None',
            '-ini:EditorPerProjectUserSettings:[/Script/PythonScriptPlugin.PythonScriptPluginUserSettings]:bDeveloperMode=False',
            '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
            '-abslog=' + str(phase_dir / 'engine.log')]


def make_plan(config, phase, selection_sha256=None):
    session = session_path(config)
    state = reservation(config)
    directory = session / phase
    args = common_args(config, directory)
    wrapper = None
    if phase == 'duplicate':
        args += [SOURCE_MAP, '-run=WorldPartitionBuilderCommandlet', '-Builder=WorldPartitionRenameDuplicateBuilder',
                 '-NewPackage=' + state['candidate_map']]
    else:
        args += ['-run=pythonscript', '-Script=' + str(directory / 'runner.py')]
        if phase == 'startup':
            wrapper = wrapper_bytes(HERE / 'startup_probe.py', ['--config', directory / 'probe-config.json', '--output', directory / 'metadata.json'])
        else:
            fixture_config = session / ('discover-config.json' if phase == 'discover' else 'selected-config.json')
            extra = ['--config', fixture_config, '--phase', phase, '--execute', '--owner-handoff', config['owner_lease']]
            if phase == 'cold_verify':
                extra += ['--move-exit-receipt', session / 'move/exit-receipt.json',
                          '--move-exit-sha256', sha(session / 'move/exit-receipt.json')]
            wrapper = wrapper_bytes(FIXTURE, extra)
    surface = startup_surface(config)
    return {'phase': phase, 'status': 'UNEXECUTED', 'candidate': state['candidate_map'], 'directory': str(directory),
            'executable': config['executable']['path'], 'argv': args,
            'native_command_line': subprocess.list2cmdline([config['executable']['path'], *args]),
            'argv_sha256': digest(args), 'wrapper': wrapper,
            'wrapper_sha256': hashlib.sha256(wrapper.encode()).hexdigest().upper() if wrapper else None,
            'driver_sha256': sha(__file__), 'probe_sha256': sha(HERE / 'startup_probe.py'),
            'fixture_sha256': FIXTURE_SHA, 'selection_sha256': selection_sha256,
            'startup_surface': surface, 'startup_surface_sha256': digest(surface),
            'approval_generated': False, 'actor_selection_generated': False}


def approval(args, config, plan):
    need(args.approval and args.approval_sha256, 'Root-authored pinned startup/phase approval JSON required.')
    permit = load(pin(args.approval, args.approval_sha256))
    need(permit.get('schema') == 'R10-mapmaker-owner-approval-v1' and permit.get('approved') is True
         and permit.get('owner') == 'Root' and permit.get('owner_lease') == config['owner_lease']
         and permit.get('phase') == plan['phase'] and permit.get('config_sha256') == config['_sha']
         and permit.get('argv_sha256') == plan['argv_sha256'] and permit.get('driver_sha256') == plan['driver_sha256']
         and permit.get('probe_sha256') == plan['probe_sha256'] and permit.get('fixture_sha256') == FIXTURE_SHA
         and permit.get('physical_receipt_sha256') == PHYSICAL_SHA and permit.get('audit_manifest_sha256') == AUDIT_SHA
         and permit.get('host_context_sha256') == HOST_SHA and permit.get('origin_guard_sha256') == config['origin_guard']['sha256']
         and permit.get('wrapper_sha256') == plan['wrapper_sha256']
         and permit.get('selection_sha256') == plan['selection_sha256']
         and permit.get('startup_surface_sha256') == plan['startup_surface_sha256']
         and all(permit.get(x) is True for x in ('final_argv_reviewed', 'plugin_plan_reviewed',
                'startup_before_probe_audit_accepted', 'no_ui_or_actor_startup_actions_approved')),
         'Approval is missing, stale, scoped differently or incomplete; never auto-approve.')
    if plan['phase'] != 'startup':
        observed = load(session_path(config) / 'startup/metadata.json')
        need(observed.get('status') == 'metadata_verified_against_owner_expectations', 'Startup metadata remains unverified.')
        need(permit.get('startup_metadata_sha256') == sha(session_path(config) / 'startup/metadata.json'), 'Root must review/pin actual startup metadata.')
    return permit


def child_identity(pid):
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    k.OpenProcess.restype = wintypes.HANDLE
    k.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    k.GetProcessTimes.restype = wintypes.BOOL
    k.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    k.QueryFullProcessImageNameW.restype = wintypes.BOOL
    k.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = k.OpenProcess(0x1000, False, pid)
    need(handle, 'Owned PID identity unavailable; outcome uncertain.')
    try:
        times = [wintypes.FILETIME() for _ in range(4)]
        need(k.GetProcessTimes(handle, *(ctypes.byref(x) for x in times)), 'Owned process creation time unavailable.')
        buf, length = ctypes.create_unicode_buffer(32768), wintypes.DWORD(32768)
        need(k.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(length)), 'Owned process image unavailable.')
        return {'pid': pid, 'creation_filetime_100ns': str((times[0].dwHighDateTime << 32) | times[0].dwLowDateTime),
                'executable': str(physical(buf.value))}
    finally:
        k.CloseHandle(handle)


def launch(config, plan):
    directory = Path(plan['directory'])
    started = utc()
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    with (directory / 'stdout.log').open('xb') as out, (directory / 'stderr.log').open('xb') as err:
        process = subprocess.Popen([plan['executable'], *plan['argv']], cwd=str(PROJECT), stdin=subprocess.DEVNULL,
                                   stdout=out, stderr=err, startupinfo=startup, creationflags=0x08000000, shell=False)
        write_new(directory / 'pid-created.json', {'pid': process.pid, 'started_utc': started, 'executable': plan['executable'],
                  'executable_sha256': config['executable']['sha256'], 'project': str(PROJECT), 'argv': plan['argv']})
        try:
            identity = child_identity(process.pid)
            need(physical(identity['executable']) == physical(plan['executable']), 'Owned process image differs.')
            write_new(directory / 'owned-process.json', identity)
            code = process.wait(timeout=config['timeout_seconds'])
        except Exception as exc:
            write_new(directory / 'uncertain.json', {'status': 'uncertain_process_preserved_no_kill', 'pid': process.pid,
                      'error': str(exc), 'ended_utc': utc(), 'kill_attempted': False})
            raise Blocked('Owned process outcome uncertain; preserve it and all artifacts; no further phases.')
    receipt = {'schema_version': 1, 'status': 'owned_process_exited',
               'identity': {k: identity[k] for k in ('pid', 'creation_filetime_100ns')},
               'executable': identity['executable'], 'project_file': str(PROJECT / 'Rogue10m.uproject'),
               'exit_code': code, 'exit_confirmed': process.poll() is not None,
               'receipt_written_after_wait_for_exit': True, 'started_utc': started, 'ended_utc': utc()}
    write_new(directory / 'exit-receipt.json', receipt)
    return receipt


def fixture_config(config, candidate, selection):
    session = session_path(config)
    copy = session / 'duplicate/copy-receipt.json'
    return {'root_exclusive_qa_execution_authorized': True, 'execution_project_root': str(PROJECT), 'engine_root': str(ENGINE),
            'candidate_map': candidate, 'copy_receipt': str(copy), 'copy_receipt_sha256': sha(copy),
            'origin_guard': config['origin_guard']['path'], 'origin_guard_sha256': config['origin_guard']['sha256'],
            'editor_runtime_sha256': sha(PROJECT / 'Binaries/Win64/UnrealEditor-Rogue10m.dll'), 'user_dir': config['user_dir'],
            'actor_paths': selection.get('actor_paths', []), 'witness_actor_path': selection.get('witness_actor_path'),
            'destination_cell': selection.get('destination_cell', [1, 0]),
            'author_owner_reference': selection.get('author_owner_reference', False),
            'evidence_relative': 'Feature/doc/evidence/world-map-maker-nongui/' + session.name}


def execute(args, config):
    need(os.name == 'nt' and config.get('root_exclusive_qa_execution_authorized') is True
         and re.fullmatch(r'[A-Za-z0-9_-]{8,80}', config.get('owner_lease', '')), 'Explicit Root isolated-QA execution lease required.')
    need(type(config['timeout_seconds']) is int and 30 <= config['timeout_seconds'] <= 7200, 'Bounded timeout 30..7200s required.')
    api, guard, receipt = validate_host(config)
    session = session_path(config)
    if args.phase == 'reserve':
        need(not session.exists(), 'Existing owner session preserved.')
        candidate = '/Game/Rogue10m/Maps/QA/MapMakerMoveReload_' + uuid.uuid4().hex
        candidate_absent(candidate)
        session.mkdir(exist_ok=False)
        write_new(session / 'reservation.json', {'schema': 'R10-mapmaker-owner-reservation-v1', 'candidate_map': candidate,
                  'config_sha256': config['_sha'], 'created_utc': utc(), 'physical_receipt_sha256': PHYSICAL_SHA})
        return {'status': 'reserved_no_engine_execution', 'candidate': candidate, 'session': str(session)}
    need(args.phase in NATIVE_PHASES, 'Choose an explicit executing phase.')
    for directory in session.iterdir():
        need(not (directory.is_dir() and ((directory / 'started.json').exists() and not (directory / 'completed.json').exists())),
             'Uncompleted/uncertain phase exists; owner must inspect without retrying: ' + str(directory))
    state = reservation(config)
    plan = make_plan(config, args.phase, args.selection_sha256)
    permit = approval(args, config, plan)
    directory = Path(plan['directory'])
    need(not directory.exists(), 'Phase directory exists; do not retry partial work.')
    if args.phase == 'duplicate':
        candidate_absent(state['candidate_map'])
    elif args.phase == 'startup':
        need(not physical(config['user_dir']).exists(), 'First startup requires an absent fresh UserDir; retain old UserDirs and use a fresh session/User sibling.')
    elif args.phase != 'startup':
        need((session / 'duplicate/completed.json').is_file(), 'Successful duplicate is required first.')
    selection = {}
    if args.phase in ('move', 'cold_verify'):
        need(args.selection and args.selection_sha256, 'Root must provide pinned explicit Actor/witness selection.')
        selection = load(pin(args.selection, args.selection_sha256))
        need(selection.get('candidate_map') == state['candidate_map'], 'Selection belongs to another candidate.')
        expected_config = fixture_config(config, state['candidate_map'], selection)
        if args.phase == 'cold_verify':
            need(load(session / 'selected-config.json') == expected_config, 'Move/cold selected config differs.')
        else:
            need(not (session / 'selected-config.json').exists(), 'Selected fixture config exists; never overwrite.')
    before = api['protected_snapshot'](PROJECT, api['package_paths'](state['candidate_map']), require_single_link=True)
    directory.mkdir(exist_ok=False)
    write_new(directory / 'started.json', {'status': 'started_do_not_retry', 'plan': plan, 'approval_sha256': args.approval_sha256})
    try:
        if plan['wrapper'] is not None:
            write_new(directory / 'runner.py', plan['wrapper'], text=True)
            need(len(plan['wrapper'].splitlines()) == 5, 'Fixed wrapper must contain exactly five lines.')
        if args.phase == 'startup':
            probe = {'project_root': str(PROJECT), 'engine_root': str(ENGINE), 'user_dir': config['user_dir'],
                     'expected_metadata': permit.get('expected_metadata'), 'owner_approval_sha256': args.approval_sha256}
            write_new(directory / 'probe-config.json', probe)
        elif args.phase == 'discover':
            write_new(session / 'discover-config.json', fixture_config(config, state['candidate_map'], {}))
        elif args.phase == 'move':
            candidate_config = fixture_config(config, state['candidate_map'], selection)
            api['preflight'](candidate_config, config['owner_lease'], True)
            write_new(session / 'selected-config.json', candidate_config)
        # Recheck immediately before native launch, after all local preparation.
        validate_host(config)
        active_processes(config['powershell'])
        need(digest(startup_surface(config)) == plan['startup_surface_sha256'], 'Startup overlays/environment/external input fingerprint changed after owner approval.')
        if args.phase == 'duplicate':
            candidate_absent(state['candidate_map'])
        result = launch(config, plan)
        preservation = {'origin_inputs': api['protected_snapshot'](ORIGIN) == guard['snapshot'],
                        'default_saves': api['local_save_snapshot']() == guard['local_saves'],
                        'clone_outside_candidate': api['protected_snapshot'](PROJECT, api['package_paths'](state['candidate_map']), require_single_link=True) == before}
        write_new(directory / 'preservation.json', preservation)
        write_new(directory / 'startup-surface-after.json', startup_surface(config))
        need(all(preservation.values()), 'Source/save/clone protection failed after native operation.')
        need(result['exit_code'] == 0 and result['exit_confirmed'], 'Native commandlet failed; partial state preserved.')
        if args.phase == 'startup':
            need(load(directory / 'metadata.json').get('status') == 'metadata_verified_against_owner_expectations', 'Startup metadata missing/unverified; no subsequent operation approved.')
        if args.phase == 'duplicate':
            paths = api['package_paths'](state['candidate_map'])
            need((PROJECT / paths[0]).is_file() and (PROJECT / paths[2]).is_dir(), 'Duplicate map/external actor output missing.')
            copy = {'schema_version': 1, 'status': 'independent_wp_copy_verified', 'project_root': str(PROJECT),
                    'source_map': SOURCE_MAP, 'candidate_map': state['candidate_map'], 'physical_snapshot_verified': True,
                    'destination_absent_before': True, 'rename_switch': False, 'builder_exit_code': result['exit_code'],
                    'source_hash_changes': [], 'python_startup_audited_no_project_writes_or_ui': True,
                    'startup_audit_evidence': str(session / 'startup/metadata.json'),
                    'physical_receipt_sha256': PHYSICAL_SHA,
                    'editor6_sha256': {Path(x['path']).name: x['sha256'] for x in receipt['editor6']}}
            write_new(directory / 'copy-receipt.json', copy)
        if args.phase in ('discover', 'move', 'cold_verify'):
            fp = PROJECT / fixture_config(config, state['candidate_map'], selection)['evidence_relative'] / (args.phase + '-result.json')
            wanted = {'discover': 'discovery_only_movement_unexecuted', 'move': 'moved_saved_cold_verify_pending',
                      'cold_verify': 'underlying_actor_api_cold_readback_passed'}[args.phase]
            need(load(fp).get('status') == wanted, 'E201 fixture did not certify this exact phase.')
        write_new(directory / 'completed.json', {'status': 'completed', 'phase': args.phase, 'ended_utc': utc(),
                  'exit_receipt_sha256': sha(directory / 'exit-receipt.json'), 'source_and_saves_preserved': True,
                  'clone_outside_candidate_preserved': True, 'panel_tested': False, 'actual_map_maker_model_tested': False})
        return {'status': 'completed', 'phase': args.phase, 'directory': str(directory), 'candidate': state['candidate_map']}
    except Exception as exc:
        write_new(directory / 'failed.json', {'status': 'failed_or_uncertain_preserved_do_not_retry', 'error': str(exc),
                  'ended_utc': utc(), 'cleanup_or_kill_attempted': False})
        raise


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', required=True)
    parser.add_argument('--phase', choices=('plan', 'reserve', *NATIVE_PHASES), default='plan')
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--approval')
    parser.add_argument('--approval-sha256')
    parser.add_argument('--selection')
    parser.add_argument('--selection-sha256')
    args = parser.parse_args(argv)
    config = load(args.config)
    config['_sha'] = sha(args.config)
    try:
        if not args.execute:
            result = make_plan(config, args.phase, args.selection_sha256) if args.phase in NATIVE_PHASES else {
                'status': 'UNEXECUTED', 'phase': args.phase, 'engine_calls': 0, 'process_queries': 0,
                'writes': 0, 'approval_generated': False, 'actor_selection_generated': False,
                'required': ['fresh post-run05 OriginGuard/code/exe/runtime pins', 'Root exclusive owner lease',
                             'reserve fresh session', 'Root-authored phase/argv/startup approval', 'explicit Actor/witness selection']}
        else:
            result = execute(args, config)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except Exception as exc:
        print(json.dumps({'status': 'BLOCKED', 'error': str(exc), 'cleanup_or_kill_attempted': False}, ensure_ascii=False))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
