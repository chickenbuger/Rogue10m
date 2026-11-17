"""Owner-approved commandlet metadata only; no world/actor/load/save/UI operations.

Startup scripts run BEFORE this file. This probe cannot authorize those scripts.
Missing public metadata or owner expectations remains unverified, never defaulted.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
from datetime import datetime, timezone


def need(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest().upper()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text(encoding='utf-8-sig'))
    output = Path(args.output)
    report = {'schema': 'R10-mapmaker-startup-metadata-v1', 'status': 'unverified',
              'startup_precedes_probe': True, 'startup_execution_authorized_by_probe': False,
              'actor_or_world_operations': 0, 'gui_selection_or_input_operations': 0,
              'native_commandlet_global_getter': 'unverified_not_publicly_confirmed',
              'owner_approval_sha256': config['owner_approval_sha256'], 'errors': []}
    try:
        import unreal
        command = unreal.SystemLibrary.get_command_line()
        runs = re.findall(r'(?:^|\s)-run=(?:"([^"]+)"|([^\s]+))', command, re.I)
        need(len(runs) == 1 and (runs[0][0] or runs[0][1]).lower() == 'pythonscript', 'Wrong commandlet CLI.')
        need(unreal.SystemLibrary.is_unattended(), 'Actual unattended getter is false.')
        need(re.search(r'(?:^|\s)-NullRHI(?:\s|$)', command, re.I), 'NullRHI CLI missing.')
        actual = {
            'project_file': str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path()))),
            'project_dir': str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))),
            'project_content_dir': str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))),
            'project_user_dir': str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_user_dir()))),
        }
        need(Path(actual['project_file']) == Path(config['project_root']) / 'Rogue10m.uproject'
             and Path(actual['project_dir']) == Path(config['project_root'])
             and Path(actual['project_content_dir']) == Path(config['project_root']) / 'Content'
             and Path(actual['project_user_dir']) == Path(config['user_dir']), 'Actual project/content/UserDir differ.')
        report['paths'] = actual
        report['command_line'] = command
        report['unattended_getter'] = True
        report['enabled_plugins'] = sorted(str(x) for x in unreal.PluginBlueprintLibrary.get_enabled_plugin_names())
        report['enabled_plugins_prove_module_loaded'] = False
        # StaticFindObject only. Do not load classes/CDOs, create settings objects,
        # set properties, call GUI handlers or silently replace missing values.
        settings = unreal.find_object(None, '/Script/PythonScriptPlugin.Default__PythonScriptPluginSettings', follow_redirectors=False)
        need(settings is not None, 'Already-loaded Python settings CDO unavailable.')
        report['python_settings'] = {
            'startup_scripts': [str(x) for x in settings.get_editor_property('StartupScripts')],
            'additional_paths': [str(x.get_editor_property('Path')) for x in settings.get_editor_property('AdditionalPaths')],
            'isolate_interpreter_environment': bool(settings.get_editor_property('bIsolateInterpreterEnvironment')),
            'remote_execution': bool(settings.get_editor_property('bRemoteExecution')),
            'run_pip_install_on_startup': bool(settings.get_editor_property('bRunPipInstallOnStartup')),
            'developer_mode': bool(settings.get_editor_property('bDeveloperMode')),
        }
        python = report['python_settings']
        need(python['isolate_interpreter_environment'] and not python['remote_execution']
             and not python['run_pip_install_on_startup'] and not python['developer_mode'], 'Merged Python settings differ from required safe CLI.')
        user_settings = unreal.find_object(None, '/Script/PythonScriptPlugin.Default__PythonScriptPluginUserSettings', follow_redirectors=False)
        need(user_settings is not None, 'Already-loaded per-user Python settings CDO unavailable.')
        report['python_user_settings'] = {
            'enable_python_override': str(user_settings.get_editor_property('EnablePythonOverride')),
            'developer_mode': bool(user_settings.get_editor_property('bDeveloperMode')),
        }
        need(not report['python_user_settings']['developer_mode'], 'Per-user developer mode is active.')
        mcp = unreal.find_object(None, '/Script/ModelContextProtocolEngine.Default__ModelContextProtocolSettings', follow_redirectors=False)
        need(mcp is not None, 'Already-loaded MCP settings CDO unavailable.')
        report['mcp_auto_start_server'] = bool(mcp.get_editor_property('bAutoStartServer'))
        need(not report['mcp_auto_start_server'], 'Merged MCP auto-start is enabled.')
        report['sys_path'] = [str(x) for x in sys.path]
        report['UE_PYTHONPATH'] = os.environ.get('UE_PYTHONPATH')
        inits = []
        for folder in sys.path:
            path = Path(folder) / 'init_unreal.py'
            if path.is_file():
                inits.append({'path': str(path.resolve()), 'sha256': sha(path)})
        report['init_unreal_files_on_observed_sys_path'] = sorted(inits, key=lambda x: x['path'].casefold())
        comparable = {key: report[key] for key in ('enabled_plugins', 'python_settings', 'python_user_settings', 'mcp_auto_start_server',
                       'UE_PYTHONPATH', 'init_unreal_files_on_observed_sys_path')}
        expected = config.get('expected_metadata')
        need(isinstance(expected, dict) and set(expected) == set(comparable), 'Explicit complete owner metadata expectations missing; retain observed values as unverified.')
        need(expected == comparable, 'Observed metadata differs from owner-reviewed expectations.')
        report['status'] = 'metadata_verified_against_owner_expectations'
    except Exception as exc:
        report['errors'].append(str(exc))
    report['finished_utc'] = datetime.now(timezone.utc).isoformat()
    with output.open('x', encoding='utf-8', newline='\n') as f:
        json.dump(report, f, ensure_ascii=False, indent=2, allow_nan=False)
        f.write('\n')
    need(report['status'] == 'metadata_verified_against_owner_expectations', 'Metadata probe remains unverified: ' + str(output))


if __name__ == '__main__':
    main()
