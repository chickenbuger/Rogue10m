"""Review completed NativeTestsD artifacts only; never starts Unreal or a process.

Usage (after NativeTestsD result.json confirms completion):
  python -I -B -X utf8 ReviewNativeD.py --execute-after-completion
Outputs are new files in NativeTestsD; existing outputs and NativeTestsC are never overwritten.
"""
from pathlib import Path
import argparse
from collections import Counter
import hashlib
import json
import os
import re
import stat
import sys
import types

ROOT = Path('D:/Project/Rogue10m')
ENGINE = Path('D:/Program Files/UE_5.8')
BASE = ROOT / 'Feature/doc/evidence/map-resume-20261113'
NATIVE = BASE / 'NativeTestsD'
PREVIOUS = BASE / 'NativeTestsC'
BUILD = BASE / 'GameGuardFixD/result.json'
TOOL = ROOT / 'Feature/implementation/map-resume-20261113/internal-smoke-culture-utc-d'
EXPECTED_DLL = '0AB7748B6C89334185031CD285A2160F66B94146BF4D0EA14B1D5DD78BD8E90C'
VALIDATOR_SHA = '985500591447274B367B97A89257D1CA992BA7891890D220608D7B13B383CF98'
POLICY_SHA = 'BE6FA5AD2D8722A486B0619570A058A219007F4775A40FF0DDEB3F0159FCBDC3'
OBSERVED = {}


def need(condition, reason):
    if not condition:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def identity(value):
    return os.path.normcase(os.path.abspath(value))


def physical(path, exists=True):
    path = Path(path)
    need(path.is_absolute() and '..' not in path.parts, 'Require absolute physical path')
    need(path.is_relative_to(ROOT) or path.is_relative_to(ENGINE), 'Read outside project/engine boundary')
    cursor = Path(path.anchor)
    for part in path.parts[1:]:
        cursor /= part
        if not cursor.exists():
            need(not exists and cursor == path, 'Missing physical path: ' + str(cursor))
            break
        info = cursor.lstat()
        need(not stat.S_ISLNK(info.st_mode) and not (getattr(info, 'st_file_attributes', 0) & 0x400), 'Linked/reparse path: ' + str(cursor))
    return path


def stamp(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def stable_bytes(path):
    path = physical(path)
    before = path.lstat()
    need(stat.S_ISREG(before.st_mode) and before.st_nlink == 1, 'Require single-link regular file: ' + str(path))
    with path.open('rb') as stream:
        opened = os.fstat(stream.fileno())
        need(stamp(before) == stamp(opened), 'File changed before read: ' + str(path))
        data = stream.read()
        finished = os.fstat(stream.fileno())
    physical(path)
    after = path.lstat()
    need(stamp(before) == stamp(finished) == stamp(after) and len(data) == before.st_size, 'File changed during read: ' + str(path))
    pin = {'path': str(path), 'sha256': digest(data), 'size': len(data)}
    prior = OBSERVED.get(identity(path))
    need(prior is None or prior == pin, 'Previously read evidence changed: ' + str(path))
    OBSERVED[identity(path)] = pin
    return data


def pin(path):
    data = stable_bytes(path)
    return {'path': str(path), 'sha256': digest(data), 'size': len(data)}


def parse(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            need(key not in result, 'Duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(data.decode('utf-8-sig'), object_pairs_hook=unique)


def encode(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def source_config():
    result = {}
    folded = set()
    for scope in ('Source', 'Config'):
        directory = physical(ROOT / scope)
        for parent, dirs, files in os.walk(directory, followlinks=False):
            for name in dirs:
                physical(Path(parent) / name)
            for name in sorted(files):
                path = Path(parent) / name
                relative = str(path.relative_to(ROOT)).replace('\\', '/')
                need(relative.casefold() not in folded, 'Source/Config case collision')
                folded.add(relative.casefold())
                result[relative] = digest(stable_bytes(path))
    return result


def exclusive_write(path, data):
    physical(path, exists=False)
    need(path.parent == NATIVE and not path.exists(), 'Output already exists or wrong NativeD output path')
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, 'O_BINARY', 0)
    with os.fdopen(os.open(path, flags, 0o600), 'wb') as stream:
        stream.write(data)
    need(stable_bytes(path) == data, 'Published output differs')


def review():
    output_paths = {name: NATIVE / name for name in ('reviewed-result.json', 'startup-review-clean-en.json', 'internal-smoke-native-preflight.json')}
    for path in output_paths.values():
        physical(path, exists=False)
        need(not path.exists(), 'Preserve existing NativeD output: ' + str(path))
    # Completion is checked before any output is created.
    process_path, result_path = NATIVE / 'process.json', NATIVE / 'result.json'
    process, result = parse(stable_bytes(process_path)), parse(stable_bytes(result_path))
    need(result.get('status') in ('commandlet_exited_report_review_required', 'headless_editor_exited_report_review_required') and result.get('exit_code') == 0 and result.get('same_dll') is True and result.get('source_config_unchanged') is True, 'NativeD has not completed successfully')
    need(str(process.get('dll_sha256', '')).upper() == str(result.get('dll_sha256', '')).upper() == EXPECTED_DLL, 'Wrong NativeD DLL')
    build = parse(stable_bytes(BUILD))
    need(build.get('exit_code') == 0 and build.get('gui_operations') == 0 and build.get('dll_sha256') == EXPECTED_DLL, 'Wrong GameGuardFixD Editor build receipt')
    dll_path = ROOT / 'Binaries/Win64/UnrealEditor-Rogue10m.dll'
    need(pin(dll_path)['sha256'] == EXPECTED_DLL, 'Current DLL differs from GameGuardFixD')
    validator_path, policy_path = TOOL / 'native43_validation.py', TOOL / 'native43-policy.json'
    validator_bytes = stable_bytes(validator_path)
    policy_bytes = stable_bytes(policy_path)
    need(digest(validator_bytes) == VALIDATOR_SHA and digest(policy_bytes) == POLICY_SHA, 'Production validator/policy pin changed')
    validator = types.ModuleType('production_native43_culture_utc_d')
    validator.__file__ = str(validator_path)
    exec(compile(validator_bytes, str(validator_path), 'exec'), validator.__dict__)
    policy = parse(policy_bytes)
    names = policy['tests']
    current = source_config()
    need(validator.source_map(process['source_config_sha256']) == validator.source_map(result['source_config_sha256']) == validator.source_map(current), 'Current Source/Config map differs from NativeD')
    report_path, log_path = NATIVE / 'report/index.json', NATIVE / 'editor.log'
    report = parse(stable_bytes(report_path))
    log_bytes = stable_bytes(log_path)
    lines = log_bytes.decode('utf-8-sig').splitlines()
    tests = report['tests']
    actual = [test['fullTestPath'] for test in tests]
    need(len(actual) == len(set(actual)) == 43 and set(actual) == set(names), 'Raw report does not contain the exact 43 unique tests')
    completed = [(index, *validator.END.search(line).groups()) for index, line in enumerate(lines) if validator.END.search(line)]
    need(len(completed) == 43 and all(state == 'Success' for _, state, _ in completed), 'Raw completion count/state differs')
    need(not any(validator.RED.search(line) for line in lines), 'Global startup/test/shutdown RED must be zero')
    queues = [line for line in lines if re.search(r'LogAutomationCommandLine:\s*Display:\s*\.\.\.Automation Test Queue Empty 43 tests performed\.', line)]
    need(len(queues) == 1, 'Missing/duplicate queue43 marker')
    warning_tests = []
    for test in tests:
        if test['warnings']:
            events = [entry['event'] for entry in test['entries'] if entry['event']['type'] == 'Warning']
            need(test['warnings'] == len(events), 'Warning count/events mismatch')
            warning_tests.append({'name': test['fullTestPath'], 'warnings': test['warnings'], 'events': events})
    need(len(warning_tests) == 6 and sum(test['warnings'] for test in warning_tests) == 12, 'Expected six cases/twelve warning events')
    reviewed = {'completed': len(completed), 'expected_count': 43, 'expected_names_exact': set(actual) == set(names),
                'states': dict(Counter(test['state'] for test in tests)), 'missing': sorted(set(names) - set(actual)),
                'extra': sorted(set(actual) - set(names)), 'failed': [test['fullTestPath'] for test in tests if test['state'] != 'Success'],
                'engine_queue_empty_43': True, 'source_config_path_set_unchanged': True,
                'report_succeeded': report['succeeded'], 'report_succeeded_with_warnings': report['succeededWithWarnings'],
                'report_failed': report['failed'], 'report_not_run': report['notRun'], 'warning_tests': warning_tests}
    previous = parse(stable_bytes(PREVIOUS / 'reviewed-result.json'))
    need(set(reviewed) == set(previous), 'NativeC reviewed-result schema keys differ')
    previous_startup = parse(stable_bytes(PREVIOUS / 'startup-review-clean-en.json'))
    startup = {'schema': 'R10-native-startup-review-v1', 'status': 'reviewed_exact_engine_startup',
               'log_sha256': digest(log_bytes), 'exact_red_lines': [],
               'reviewer': 'ReviewNativeD.py; artifact-only strict production-validator review',
               'reason': 'Fresh GameGuardFixD NativeTestsD uses exact en/en-US flags; complete raw log RED0 and selected43 succeed with6 retained warning cases. Source evidence is freshly pinned. Original NativeTestsA15 remain NOT_APPROVED; no allowance, mask, UI/visual/runtime release certification or new causal attribution.',
               'source_evidence': [pin(Path(item['path'])) for item in previous_startup['source_evidence']]}
    need(set(startup) == set(previous_startup), 'NativeC startup envelope schema keys differ')
    reviewed_bytes = encode(reviewed)
    virtual = {identity(output_paths['reviewed-result.json']): reviewed_bytes}

    def read_pin(value):
        path = Path(value['path'])
        data = virtual.get(identity(path))
        if data is None:
            data = stable_bytes(path)
        need(digest(data) == value['sha256'].upper(), 'Pinned bytes differ: ' + str(path))
        if 'size' in value:
            need(len(data) == value['size'], 'Pinned byte count differs: ' + str(path))
        return data

    refs = {'process': pin(process_path), 'result': pin(result_path),
            'reviewed': {'path': str(output_paths['reviewed-result.json']), 'sha256': digest(reviewed_bytes), 'size': len(reviewed_bytes)},
            'report': pin(report_path), 'selection': pin(ROOT / 'Feature/implementation/map-resume-20261113/native-no-gui-tests-20261113.json'), 'log': pin(log_path)}
    ctx = {'root': str(ROOT), 'project': str(ROOT / 'Rogue10m.uproject'), 'engine': str(ENGINE),
           'dll_sha256': EXPECTED_DLL, 'source_config': current, 'editor_finished_utc': build['ended_utc'],
           'read_pin': read_pin, 'policy': policy, 'synthetic_fixture': False}
    preflight = validator.validate_native43(refs, ctx)
    need(preflight['status'] == 'validated_native43_numeric_evidence' and preflight['startup_red_rows'] == 0, 'Production preflight failed')
    # Recheck every raw input before publishing. The only virtual input is the
    # new derived reviewed JSON, which is also revalidated from disk below.
    need(validator.source_map(source_config()) == validator.source_map(current), 'Source/Config changed during review')
    for value in list(OBSERVED.values()):
        read_pin(value)
    exclusive_write(output_paths['reviewed-result.json'], reviewed_bytes)
    virtual.clear()
    preflight = validator.validate_native43(refs, ctx)
    exclusive_write(output_paths['startup-review-clean-en.json'], encode(startup))
    preflight['review_context'] = {'editor_build_receipt': pin(BUILD), 'editor_finished_utc': build['ended_utc'],
                                   'current_dll': pin(dll_path), 'production_validator': pin(validator_path),
                                   'policy': pin(policy_path), 'review_script': pin(Path(__file__).absolute()),
                                   'startup_envelope': pin(output_paths['startup-review-clean-en.json']),
                                   'source_config_rechecked': True, 'UE_processes_started': 0}
    exclusive_write(output_paths['internal-smoke-native-preflight.json'], encode(preflight))
    print(json.dumps({'status': preflight['status'], 'dll_sha256': EXPECTED_DLL, 'native_tests': 43, 'warning_cases': 6,
                      'warning_events': 12, 'global_red_rows': 0, 'editor_finished_utc': build['ended_utc'],
                      'outputs': [pin(path) for path in output_paths.values()], 'UI7': 'UNVERIFIED', 'UE_processes_started': 0}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute-after-completion', action='store_true', help='Review artifacts only after NativeD has completed; never starts the engine')
    args = parser.parse_args()
    need(args.execute_after_completion, 'Explicit --execute-after-completion required')
    review()


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError, UnicodeError) as error:
        print(json.dumps({'status': 'blocked', 'reason': str(error), 'UE_processes_started': 0}, ensure_ascii=False), file=sys.stderr)
        sys.exit(2)
