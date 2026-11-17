"""Static Python-startup inventory only. Does not import any scanned Python module.

Run with isolated stdlib Python, preferably -I -B -S. A fresh output directory
must be beneath this collector's directory. Host/runtime values stay unknown.
Copy the frozen audit package elsewhere before a later, separately authorized run.
"""
from pathlib import Path
import argparse
import ast
import datetime
import hashlib
import json
import os
import re
import stat

SKIP_PLUGIN_DIRS = {'Binaries', 'Intermediate', 'Source', 'Content', 'Resources', 'Config', 'Shaders', 'Extras', 'ThirdParty', 'Documentation', 'Docs', 'Tests', '.git'}
CONFIG_KEYS = re.compile(r'StartupScripts|AdditionalPaths|bRemoteExecution|bIsolateInterpreterEnvironment|bRunPipInstallOnStartup|DisablePythonForCommandlet|EnablePythonOverride|PipInstall', re.I)
SIGNALS = re.compile(r'ToolMenus|refresh_all_widgets|extend_menu|add_menu_entry|EditorDialog|show_.*dialog|slate|selection|selected_|InputKey|process_key|route_pointer|register_.*callback|console_command|execute_console|open_level|load_map|save_(?:map|level|asset|package|all)|register_toolset|PythonTestRunner|shutil\.copy|subprocess|os\.system|sys\.path|agent_skill|\.register$|\.run$', re.I)


def need(value, reason):
    if not value:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def physical(path):
    path = Path(path)
    need(path.is_absolute() and '..' not in path.parts, 'Require absolute path')
    current = Path(path.anchor)
    for part in path.parts[1:]:
        current /= part
        if not current.exists():
            break
        info = current.lstat()
        need(not stat.S_ISLNK(info.st_mode) and not (getattr(info, 'st_file_attributes', 0) & 0x400), 'Reparse/link boundary: ' + str(current))
    return path


def read(path):
    path = physical(path)
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    fields = lambda x: (x.st_dev, x.st_ino, x.st_size, x.st_mtime_ns, x.st_ctime_ns)
    need(fields(before) == fields(after) and len(data) == after.st_size, 'File drift: ' + str(path))
    return data


def pin(path):
    data = read(path)
    return {'path': str(path), 'sha256': digest(data), 'size': len(data)}


def normalize_descriptor_json(text):
    # Lexical normalization only; preserve string contents and line numbering.
    chars = list(text)
    quoted = escaped = False
    index = 0
    while index < len(chars):
        char = chars[index]
        if quoted:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char == '/' and index + 1 < len(chars) and chars[index + 1] in ('/', '*'):
            block = chars[index + 1] == '*'
            end = index + 2
            if block:
                while end + 1 < len(chars) and chars[end:end + 2] != ['*', '/']:
                    end += 1
                need(end + 1 < len(chars), 'Unterminated descriptor comment')
                end += 2
            else:
                while end < len(chars) and chars[end] not in '\r\n':
                    end += 1
            for pos in range(index, end):
                if chars[pos] not in '\r\n':
                    chars[pos] = ' '
            index = end - 1
        index += 1
    quoted = escaped = False
    for index, char in enumerate(chars):
        if quoted:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char == ',':
            end = index + 1
            while end < len(chars) and chars[end].isspace():
                end += 1
            if end < len(chars) and chars[end] in ']}':
                chars[index] = ' '
    return ''.join(chars)


def parse_json(path, duplicate_keys=None, parse_notes=None):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                need(duplicate_keys is not None, 'Duplicate JSON key in ' + str(path))
                duplicate_keys.append({'key': key, 'earlier': result[key], 'later': value})
            result[key] = value
        return result
    text = read(path).decode('utf-8-sig')
    try:
        return json.loads(text, object_pairs_hook=unique)
    except json.JSONDecodeError:
        need(parse_notes is not None, 'Non-strict JSON requires explicit descriptor normalization')
        if duplicate_keys is not None:
            duplicate_keys.clear()
        parse_notes.append('comments_and_trailing_commas_lexically_normalized; original_raw_file_pinned; not_runtime_resolution')
        return json.loads(normalize_descriptor_json(text), object_pairs_hook=unique)


def walk(root, skip=()):
    if not root.is_dir():
        return
    physical(root)
    for parent, dirs, files in os.walk(root, followlinks=False):
        dirs[:] = sorted(d for d in dirs if d not in skip)
        for name in dirs:
            physical(Path(parent) / name)
        for name in sorted(files):
            yield physical(Path(parent) / name)


def python_review(path):
    raw = read(path)
    try:
        text = raw.decode('utf-8-sig')
        tree = ast.parse(text, filename=str(path))
    except (UnicodeError, SyntaxError) as error:
        return dict(pin(path), parse='unresolved', error=str(error), calls=[], imports=[])
    lines = text.splitlines()
    imports = []
    calls = []
    class Visitor(ast.NodeVisitor):
        depth = 0
        def visit_Import(self, node):
            imports.extend({'line': node.lineno, 'module': alias.name, 'deferred': bool(self.depth)} for alias in node.names)
        def visit_ImportFrom(self, node):
            imports.append({'line': node.lineno, 'module': node.module, 'names': [a.name for a in node.names], 'level': node.level, 'deferred': bool(self.depth)})
        def visit_Call(self, node):
            name = ast.unparse(node.func)
            if SIGNALS.search(name) or ('register' in name and not self.depth):
                calls.append({'line': node.lineno, 'call': name, 'scope': 'deferred_function_body' if self.depth else 'module_or_class_body', 'source': lines[node.lineno - 1].strip()})
            self.generic_visit(node)
        def visit_FunctionDef(self, node):
            for decorator in node.decorator_list:
                self.visit(decorator)
            for value in node.args.defaults + [v for v in node.args.kw_defaults if v is not None]:
                self.visit(value)
            self.depth += 1
            for child in node.body:
                self.visit(child)
            self.depth -= 1
        visit_AsyncFunctionDef = visit_FunctionDef
    Visitor().visit(tree)
    return dict(pin(path), parse='ast_only_not_executed', imports=imports, calls=calls,
                commandlet_guard_mentions=[i + 1 for i, line in enumerate(lines) if 'is_running_commandlet' in line])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root', default='D:/Project/Rogue10m')
    parser.add_argument('--engine-root', default='D:/Program Files/UE_5.8')
    parser.add_argument('--out-dir', required=True)
    args = parser.parse_args()
    home = physical(Path(__file__).absolute().parent)
    output = physical(Path(args.out_dir).absolute())
    need(output.is_relative_to(home) and output != home and not output.exists(), 'Output must be a fresh directory beneath this collector')
    project = physical(Path(args.project_root).absolute())
    engine = physical(Path(args.engine_root).absolute())
    uproject = project / 'Rogue10m.uproject'
    descriptor = parse_json(uproject)
    project_refs = {row['Name']: row for row in descriptor.get('Plugins', [])}
    descriptors = []
    descriptor_errors = []
    plugin_search_roots = [('engine', engine / 'Engine/Plugins'), ('project', project / 'Plugins')]
    engine_platforms = engine / 'Engine/Platforms'
    project_platforms = project / 'Platforms'
    for base in (engine_platforms, project_platforms):
        if base.is_dir():
            plugin_search_roots.extend(('platform_extension_candidate', child / 'Plugins') for child in sorted(base.iterdir()) if child.is_dir())
    for kind, search in plugin_search_roots:
        for file in walk(search, SKIP_PLUGIN_DIRS):
            if file.suffix.lower() == '.uplugin':
                duplicates = []
                notes = []
                try:
                    data = parse_json(file, duplicates, notes)
                except (ValueError, UnicodeError) as error:
                    descriptor_errors.append(dict(pin(file), error=str(error)))
                    data = {}
                descriptors.append({'name': file.stem, 'kind': kind, 'descriptor': pin(file), 'root': str(file.parent),
                                    'enabled_by_default': data.get('EnabledByDefault'), 'can_contain_content': data.get('CanContainContent', False),
                                    'dependencies': data.get('Plugins', []), 'modules': data.get('Modules', []),
                                    'project_reference': project_refs.get(file.stem), 'duplicate_keys': duplicates,
                                    'parse_notes': notes,
                                    'descriptor_parse_is_runtime_resolution': False, 'descriptor_data': data})
    by_name = {}
    for row in descriptors:
        by_name.setdefault(row['name'], []).append(row)
    static_reasons = {}
    allow_defaults = not descriptor.get('DisableEnginePluginsByDefault', False)
    for row in descriptors:
        ref = project_refs.get(row['name'])
        if ref is not None:
            if ref.get('Enabled') is True:
                static_reasons.setdefault(row['name'], []).append('project_explicit_enabled_candidate')
        elif row['enabled_by_default'] is True and (row['kind'] == 'project' or allow_defaults):
            static_reasons.setdefault(row['name'], []).append('descriptor_default_enabled_candidate')
        elif row['enabled_by_default'] is None and row['kind'] == 'project':
            static_reasons.setdefault(row['name'], []).append('project_plugin_unspecified_default_candidate')
    pending = list(static_reasons)
    while pending:
        name = pending.pop()
        for row in by_name.get(name, []):
            for dep in row['dependencies']:
                target = dep['Name']
                if dep.get('Enabled') is True and project_refs.get(target, {}).get('Enabled') is not False and target not in static_reasons:
                    static_reasons[target] = ['enabled_dependency_candidate_of:' + name]
                    pending.append(target)
    roots = [{'role': 'project', 'path': str(project / 'Content/Python')}, {'role': 'engine', 'path': str(engine / 'Engine/Content/Python')}]
    for row in descriptors:
        row['static_candidate_reasons'] = static_reasons.get(row['name'], [])
        row['actual_mounted_for_final_commandlet'] = None
        row.pop('descriptor_data')
        folder = Path(row['root']) / 'Content/Python'
        row['python_directory_exists'] = folder.is_dir()
        row['init_unreal_exists'] = (folder / 'init_unreal.py').is_file()
        if folder.is_dir():
            roots.append({'role': 'plugin', 'plugin': row['name'], 'path': str(folder), 'static_candidate_reasons': row['static_candidate_reasons'], 'actual_mounted': None})
    python_files = []
    pth_files = []
    seen_python = set()
    for row in roots:
        folder = Path(row['path'])
        row['exists'] = folder.is_dir()
        row['init_unreal'] = pin(folder / 'init_unreal.py') if (folder / 'init_unreal.py').is_file() else None
        site_dirs = [folder / 'Lib/Win64/x64/site-packages', folder / 'Lib/Win64/site-packages', folder / 'Lib/site-packages']
        row['site_directory_candidates'] = [{'path': str(p), 'exists': p.is_dir()} for p in site_dirs]
        # Inventory local startup/import sources without importing them. Third-party
        # site-packages and native extension behavior remain a separate boundary.
        for file in walk(folder, {'__pycache__', 'Lib', '.git'}):
            if file.suffix == '.py' and str(file) not in seen_python:
                seen_python.add(str(file))
                python_files.append(python_review(file))
        for site_dir in site_dirs:
            if site_dir.is_dir():
                for file in sorted(site_dir.glob('*.pth')):
                    pth_files.append(dict(pin(file), lines=[{'line': i + 1, 'text': line, 'exec_import_line': line.startswith(('import ', 'import\t'))} for i, line in enumerate(read(file).decode('utf-8-sig').splitlines()) if line.strip() and not line.startswith('#')]))
    embedded_site = engine / 'Engine/Binaries/ThirdParty/Python3/Win64/Lib/site-packages'
    for file in sorted(embedded_site.glob('*.pth')) if embedded_site.is_dir() else []:
        pth_files.append(dict(pin(file), lines=[{'line': i + 1, 'text': line, 'exec_import_line': line.startswith(('import ', 'import\t'))} for i, line in enumerate(read(file).decode('utf-8-sig').splitlines()) if line.strip() and not line.startswith('#')]))
    embedded_home = embedded_site.parent.parent
    boot_candidates = [embedded_home / 'Lib/sitecustomize.py', embedded_site / 'sitecustomize.py',
                       embedded_site / 'usercustomize.py'] + sorted(embedded_home.glob('*._pth'))
    embedded_boot_files = [{'path': str(file), 'exists': file.is_file(),
                            'pin': pin(file) if file.is_file() else None} for file in boot_candidates]
    config_pins, config_rows = [], []
    config_roots = [project / 'Config', engine / 'Engine/Config'] + [Path(row['root']) / 'Config' for row in descriptors]
    for base in (engine_platforms, project_platforms):
        if base.is_dir():
            config_roots.extend(child / 'Config' for child in sorted(base.iterdir()) if child.is_dir())
    seen_config = set()
    for folder in config_roots:
        for file in walk(folder):
            if file.suffix.lower() != '.ini' or str(file) in seen_config:
                continue
            seen_config.add(str(file))
            config_pins.append(pin(file))
            section = ''
            for no, line in enumerate(read(file).decode('utf-8-sig', errors='replace').splitlines(), 1):
                if line.strip().startswith('['):
                    section = line.strip()
                if not line.lstrip().startswith((';', '#')) and (CONFIG_KEYS.search(line) or ('PythonScriptPlugin' in section and '=' in line)):
                    config_rows.append({'path': str(file), 'line': no, 'section': section, 'text': line})
    source_relative = [
        'Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PythonScriptPlugin.cpp',
        'Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PythonScriptCommandlet.cpp',
        'Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PythonScriptPluginSettings.h',
        'Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PythonScriptPluginSettings.cpp',
        'Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Public/PythonScriptTypes.h',
        'Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PyUtil.cpp',
        'Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PipInstall.cpp',
        'Engine/Source/Runtime/Projects/Private/PluginManager.cpp',
        'Engine/Source/Runtime/Projects/Private/PluginDescriptor.cpp',
        'Engine/Source/Runtime/Core/Private/Windows/WindowsPlatformProcess.cpp',
        'Engine/Source/Runtime/Core/Private/Misc/App.cpp',
        'Engine/Source/Runtime/Core/Private/Misc/Paths.cpp',
        'Engine/Source/Runtime/Launch/Resources/Version.h',
        'Engine/Source/Runtime/Launch/Private/LaunchEngineLoop.cpp',
        'Engine/Binaries/ThirdParty/Python3/Win64/Lib/site.py']
    sources = [pin(engine / rel) for rel in source_relative if (engine / rel).is_file()]
    unknown = {'status': 'UNRESOLVED_NOT_APPROVED', 'startup_safe': False,
               'clone_root': None, 'clone_frozen_input_manifest_pin': None, 'exact_command_line': None,
               'new_UserDir': {'path': None, 'exists': None, 'fresh': None, 'verified': False},
               'Windows_Documents_UnrealEngine_Python': {'resolved_host_path': None, 'exists': None, 'inventory_pin': None, 'reviewed': False},
               'environment': {key: {'observed': False, 'value': None} for key in ('UE_PYTHONPATH', 'UE_PIPINSTALL_PATH', 'PYTHONPATH', 'PYTHONHOME', 'PYTHONUSERBASE', 'UE_SHOTGRID_ENABLED', 'UE_SHOTGRID_BOOTSTRAP', 'UE_SHOTGUN_ENABLED', 'UE_SHOTGUN_BOOTSTRAP', 'SETUPTOOLS_USE_DISTUTILS')},
               'effective_merged_python_config': None, 'ordered_mounted_content_roots': None, 'initial_CPython_sys_path': None,
               'commandlet_platform_arch_CWD_base_directory': None,
               'AdditionalPaths_resolved_inventory': None, 'configured_StartupScripts_review': None,
               'DeferredCommands_and_OnPythonInitialized_callbacks': None, 'pip_site_pth_inventory': None,
               'new_clone_Save_SourceConfig_Content_Editor6_pin_equality': None,
               'native_modules_side_effects_certified': False,
               'note': 'No empty array, absent current project Python folder, or unfilled field is approval. Root must collect exact final-host/clone inputs separately; do not edit the frozen audit package.'}
    report = {'schema': 'R10-static-python-startup-audit-v1', 'status': 'BLOCKED_PENDING_RUNTIME_AND_STARTUP_REVIEW',
              'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'project': str(project), 'engine': str(engine),
              'uproject': pin(uproject), 'roots': roots, 'plugin_descriptors': descriptors, 'descriptor_errors': descriptor_errors, 'config_pins': config_pins,
              'python_config_rows': config_rows, 'python_files': python_files, 'pth_files': pth_files, 'source_pins': sources,
              'embedded_boot_candidates': embedded_boot_files,
              'host_environment_read': False, 'Unreal_imported': False, 'UE_UBT_UAT_Cook_Game_MCP_GUI_calls': 0,
              'actual_mounted_plugin_set_proven': False, 'static_candidate_resolution_is_not_runtime_plugin_resolution': True,
              'additional_plugin_directories': descriptor.get('AdditionalPluginDirectories', []),
              'scope_limit': 'Python startup/config/path inventory and AST evidence only; no execution or whole-native-module side-effect certification.'}
    output.mkdir()
    for name, value in [('inventory.json', report), ('runtime-inputs.UNRESOLVED.json', unknown)]:
        with (output / name).open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(value, stream, ensure_ascii=True, indent=2)
            stream.write('\n')
    print(json.dumps({'status': report['status'], 'output': str(output), 'descriptors': len(descriptors),
                      'plugin_init_files': sum(row['init_unreal_exists'] for row in descriptors),
                      'descriptor_errors': len(descriptor_errors),
                      'descriptors_with_duplicate_keys': sum(bool(row['duplicate_keys']) for row in descriptors),
                      'candidate_plugin_inits': [row['name'] for row in descriptors if row['init_unreal_exists'] and row['static_candidate_reasons']],
                      'python_sources': len(python_files), 'pth_files': len(pth_files), 'config_files': len(config_pins),
                      'config_rows': len(config_rows), 'host_environment_read': False, 'Unreal_imported': False}, indent=2))


if __name__ == '__main__':
    main()
