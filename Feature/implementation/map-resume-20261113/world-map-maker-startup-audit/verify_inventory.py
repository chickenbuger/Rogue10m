"""Offline checks of the static inventory and collector; never imports Unreal."""
from pathlib import Path
import ast
import hashlib
import json
import datetime


def require(value, message):
    if not value:
        raise ValueError(message)


def main():
    home = Path(__file__).absolute().parent
    inventory = json.loads((home / 'observed-final/inventory.json').read_text(encoding='utf-8'))
    unknown = json.loads((home / 'observed-final/runtime-inputs.UNRESOLVED.json').read_text(encoding='utf-8'))
    tree = ast.parse((home / 'collect_startup_inventory.py').read_text(encoding='utf-8'))
    imported = {alias.name.split('.')[0] for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names}
    imported.update(node.module.split('.')[0] for node in ast.walk(tree) if isinstance(node, ast.ImportFrom) and node.module)
    require(imported == {'pathlib', 'argparse', 'ast', 'datetime', 'hashlib', 'json', 'os', 're', 'stat'}, 'Collector imports changed')
    # Exercise only the trusted lexical helper, not scanned descriptors/modules.
    helper_nodes = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in ('need', 'normalize_descriptor_json')]
    namespace = {}
    exec(compile(ast.Module(body=helper_nodes, type_ignores=[]), '<collector lexical helpers>', 'exec'), namespace)
    normalize = namespace['normalize_descriptor_json']
    samples = [
        ('{"s":"https://x/a,]/*literal*/",//comment\n"a":[1,],}', {'s': 'https://x/a,]/*literal*/', 'a': [1]}),
        ('{/*block\n comment*/"s":"quote\\\"//literal", "a":[{"b":true,},],}', {'s': 'quote"//literal', 'a': [{'b': True}]}),
        ('{"s":"backslash\\\\", "a":0}', {'s': 'backslash\\', 'a': 0}),
    ]
    for raw, expected in samples:
        converted = normalize(raw)
        require(json.loads(converted) == expected and converted.count('\n') == raw.count('\n'), 'Lexical normalization changed strings/lines')
    try:
        normalize('{/* unterminated')
    except ValueError:
        pass
    else:
        raise ValueError('Unterminated descriptor comment accepted')
    require(inventory['status'] == 'BLOCKED_PENDING_RUNTIME_AND_STARTUP_REVIEW', 'Inventory approval drift')
    require(unknown['status'] == 'UNRESOLVED_NOT_APPROVED' and unknown['startup_safe'] is False, 'Unknown input auto-approval')
    require(all(row == {'observed': False, 'value': None} for row in unknown['environment'].values()), 'Host environment was filled')
    require(unknown['new_UserDir'] == {'path': None, 'exists': None, 'fresh': None, 'verified': False}, 'Host UserDir was filled')
    require(all(row['actual_mounted_for_final_commandlet'] is None for row in inventory['plugin_descriptors']), 'Actual mount claim')
    require(not inventory['descriptor_errors'] and all(row['parse'] == 'ast_only_not_executed' for row in inventory['python_files']), 'Unresolved static parse')
    rows = [inventory['uproject']] + inventory['config_pins'] + inventory['python_files'] + inventory['pth_files'] + inventory['source_pins']
    rows += [row['descriptor'] for row in inventory['plugin_descriptors']]
    rows += [row['pin'] for row in inventory['embedded_boot_candidates'] if row['pin']]
    unique = {}
    for row in rows:
        require(row['path'] not in unique or unique[row['path']]['sha256'] == row['sha256'], 'Inconsistent duplicate pin')
        unique[row['path']] = row
    for path, row in unique.items():
        raw = Path(path).read_bytes()
        require(len(raw) == row['size'] and hashlib.sha256(raw).hexdigest().upper() == row['sha256'], 'Pinned source drift: ' + path)
    for row in inventory['roots']:
        require(Path(row['path']).is_dir() == row['exists'], 'Python root presence drift')
    for row in inventory['embedded_boot_candidates']:
        require(Path(row['path']).is_file() == row['exists'], 'Embedded startup candidate presence drift')
    result = {'schema': 'R10-static-python-startup-offline-verification-v1',
              'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'offline_checks': 'PASS', 'startup_gate': 'BLOCKED_NOT_APPROVED',
              'lexical_valid_cases': len(samples), 'lexical_malformed_cases_rejected': 1,
              'collector_import_allowlist_verified': True, 'unresolved_fields_preserved': True,
              'unique_existing_input_pins_rechecked': len(unique),
              'source_project_root_and_embedded_startup_presence_rechecked': True,
              'scope': 'Existing recorded paths only; not a fresh runtime sys.path/mount/environment audit or native side-effect certification.',
              'Unreal_imported': False, 'UE_UBT_UAT_Cook_Game_MCP_GUI_calls': 0}
    with (home / 'verification.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, ensure_ascii=True, indent=2)
        stream.write('\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
