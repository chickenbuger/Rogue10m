"""Inert, owned NullRHI cold material observation; never authors or saves.

Only __main__ installs a bounded callback after exact command-line ownership
checks. Reports field-level differences without relaxing any validation gate.
"""
from __future__ import annotations
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import traceback

ROOT = Path('D:/Project/Rogue10m')
BASE = ROOT / 'Feature/doc/evidence/caves20261118'
OUT = BASE / 'cold-material-state'
TARGET = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
TRIPLANAR_SHA256 = 'cd3b1e386c2fba6f0d939b515be421bca5227ac0112cdf86c16fcd01f039d65e'
state = {'phase': 'registry', 'ticks': 0, 'busy': False, 'owned': False, 'started': None}


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def read(relative, expected_sha=None):
    file = BASE / relative
    if expected_sha:
        require(sha(file) == expected_sha, 'Frozen input changed: ' + relative)
    return json.loads(file.read_text(encoding='utf-8'))


def write(name, value):
    with (OUT / (name + '.json')).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write('\n')


def differences(expected, actual, field='$'):
    """Exact recursive field differences; no sorting, tolerance or exclusion."""
    rows = []
    if type(expected) is not type(actual):
        return [{'field': field, 'expected': expected, 'actual': actual, 'kind': 'type_or_value'}]
    if isinstance(expected, dict):
        for key in sorted(set(expected) | set(actual)):
            if key not in expected or key not in actual:
                rows.append({'field': field + '.' + str(key), 'expected': expected.get(key, '<missing>'),
                             'actual': actual.get(key, '<missing>'), 'kind': 'missing_field'})
            else:
                rows.extend(differences(expected[key], actual[key], field + '.' + str(key)))
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            rows.append({'field': field + '.length', 'expected': len(expected), 'actual': len(actual), 'kind': 'list_length'})
        for i in range(min(len(expected), len(actual))):
            rows.extend(differences(expected[i], actual[i], field + '[' + str(i) + ']'))
        for i in range(min(len(expected), len(actual)), max(len(expected), len(actual))):
            rows.append({'field': field + '[' + str(i) + ']', 'expected': expected[i] if i < len(expected) else '<missing>',
                         'actual': actual[i] if i < len(actual) else '<missing>', 'kind': 'missing_item'})
    elif expected != actual:
        rows.append({'field': field, 'expected': expected, 'actual': actual, 'kind': 'value'})
    return rows


def diagnose():
    inspector = module('cold_material_inspector', 'Scripts/Editor/InspectCaveSites20261118.py')
    reader = module('cold_material_graph_reader', 'Scripts/Editor/NaturalRoadMaterial20261117.py')
    validator = module('cold_material_validator_reader', 'Scripts/Editor/ValidateCaves20261118.py')
    baseline = read('baseline-live/baseline-materials.json')['materials']
    expected_new = read('author-live/triplanar.json', TRIPLANAR_SHA256)['expected_material_states_after']
    require(len(baseline) == 40 and len(expected_new) == 7, 'Exact forty source and seven owned material inputs required')
    dirty_before = inspector.dirty(u)
    sources, owned, parents = {}, {}, {}
    for name, expected in baseline.items():
        obj = u.load_asset(name)
        actual = inspector.material_state(u, obj, reader)
        sources[name] = {'expected': expected, 'actual': actual, 'differences': differences(expected, actual)}
    for name, expected in expected_new.items():
        obj = u.load_asset(name)
        actual = validator.material_live_state(u, obj, reader)
        owned[name] = {'expected': expected, 'actual': actual, 'differences': differences(expected, actual),
                       'disk': inspector.disk(obj)}
    for value in list(sources.values()) + list(owned.values()):
        base_path = value['actual']['base_material']
        if base_path not in parents:
            obj = u.load_asset(base_path)
            parents[base_path] = {'state': inspector.material_state(u, obj, reader),
                                 'automatically_set_usage_in_editor': inspector.optional_property(obj, 'automatically_set_usage_in_editor'),
                                 'used_with_instanced_static_meshes': bool(obj.get_editor_property('used_with_instanced_static_meshes'))}
    files = set()
    for value in sources.values():
        for key in ('disk', 'base_disk'):
            record = value['actual'][key]
            if 'file' in record:
                files.add(record['file'])
    for value in owned.values():
        if 'file' in value['disk']:
            files.add(value['disk']['file'])
    disk_files = {relative: {'sha256': sha(ROOT / relative), 'size': (ROOT / relative).stat().st_size}
                  for relative in sorted(files)}
    dirty_after = inspector.dirty(u)
    changed_sources = {p: r['differences'] for p, r in sources.items() if r['differences']}
    changed_owned = {p: r['differences'] for p, r in owned.items() if r['differences']}
    return {'schema': 'Rogue10m.CaveColdMaterialDiagnostic.20261118.v1', 'diagnostic_completed': True,
            'passed': dirty_before == dirty_after, 'cave_acceptance_claimed': False,
            'pid': os.getpid(), 'assets_saved': False, 'properties_set': False, 'validation_gates_changed': False,
            'source_count': 40, 'owned_count': 7, 'source_materials': sources, 'owned_materials': owned,
            'changed_source_count': len(changed_sources), 'changed_owned_count': len(changed_owned),
            'source_field_differences': changed_sources, 'owned_field_differences': changed_owned,
            'parent_materials': parents, 'observed_disk_files': disk_files,
            'dirty_before': dirty_before, 'dirty_after': dirty_after, 'dirty_set_preserved': dirty_before == dirty_after,
            'input_sha256': {'baseline-live/baseline-materials.json': sha(BASE / 'baseline-live/baseline-materials.json'),
                             'author-live/triplanar.json': TRIPLANAR_SHA256},
            'elapsed_seconds': time.monotonic() - state['started'],
            'scope': 'Exact read-only forty/seven material state comparison; no capsule QA, mutation, save, retry or threshold relaxation.'}


def quit_owned():
    require(state['owned'], 'Only the exact owned unattended process may quit')
    handle = state.pop('handle', None)
    if handle is not None:
        u.unregister_slate_post_tick_callback(handle)
    state['phase'] = 'exiting'
    u.SystemLibrary.quit_editor()


def tick(_):
    if state['busy']:
        return
    state['busy'] = True
    try:
        require(time.monotonic() - state['started'] < 240, 'Owned material diagnostic watchdog exceeded')
        if u.AssetRegistryHelpers.get_asset_registry().is_loading_assets():
            return
        editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
        world = editor.get_editor_world()
        require(world and not editor.get_game_world() and world.get_path_name().split('.')[0] == TARGET,
                'Exact idle candidate diagnostic world required')
        if state['phase'] == 'registry':
            loaded = json.loads(u.Rogue10mCrop20261114WorldAuthoring.load_candidate_region())
            require(loaded.get('success') is True, 'Independent diagnostic region load failed')
            state.update(phase='settling', ticks=0)
            return
        state['ticks'] += 1
        if state['ticks'] < 20:
            return
        state['phase'] = 'diagnosing'
        write('result', diagnose())
        quit_owned()
    except Exception:
        failure = {'diagnostic_completed': False, 'passed': False, 'pid': os.getpid(), 'assets_saved': False,
                   'phase': state['phase'], 'traceback': traceback.format_exc()}
        if not (OUT / 'result.json').exists():
            write('result', failure)
        u.log_error('CAVE COLD MATERIAL DIAGNOSTIC ' + failure['traceback'])
        quit_owned()
    finally:
        state['busy'] = False


def bootstrap():
    global u
    import unreal as u
    command = u.SystemLibrary.get_command_line().lower()
    require('-roguecavestage=materialdiag' in command and '-unattended' in command and '-nullrhi' in command,
            'Explicit owned unattended nullrhi material diagnostic launch required')
    require('bautosaveenable=false' in command and 'bautostartserver=false' in command,
            'Launch must disable autosave and MCP auto-start')
    require(Path(u.Paths.project_dir()).resolve() == ROOT, 'Exact physical project required')
    require(str(u.SystemLibrary.get_engine_version()).startswith('5.8.'), 'Exact UE5.8 required')
    sys.dont_write_bytecode = True
    OUT.mkdir(parents=True, exist_ok=False)
    state.update(owned=True, started=time.monotonic())
    write('startup', {'pid': os.getpid(), 'stage': 'RogueCaveStage=materialdiag', 'target': TARGET,
                      'nullrhi': True, 'unattended': True, 'autosave_and_MCP_autostart_disabled': True,
                      'assets_saved': False, 'script_sha256': sha(Path(__file__)),
                      'reader_script_sha256': {name: sha(ROOT / name) for name in
                          ('Scripts/Editor/ValidateCaves20261118.py', 'Scripts/Editor/InspectCaveSites20261118.py',
                           'Scripts/Editor/NaturalRoadMaterial20261117.py')}})
    state['handle'] = u.register_slate_post_tick_callback(tick)


if __name__ == '__main__':
    bootstrap()
