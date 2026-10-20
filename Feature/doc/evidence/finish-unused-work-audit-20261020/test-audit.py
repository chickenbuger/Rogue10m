"""Offline regression fixtures for the read-only tmp duplicate auditor."""
from __future__ import annotations

import sys
sys.dont_write_bytecode = True

from contextlib import redirect_stdout
import hashlib
import importlib.util
from io import StringIO
import json
import os
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory
from unittest.mock import patch


PROJECT = Path(__file__).resolve().parents[4]
spec = importlib.util.spec_from_file_location('unused_audit_under_test', PROJECT / 'Scripts/AuditUnusedWorkFiles.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def put(root, relative, content):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content.encode('utf-8') if isinstance(content, str) else content)
    return path


def hashes(root):
    return {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob('*') if path.is_file()}


def expect_error(callback, kind):
    try:
        callback()
    except kind:
        return
    raise AssertionError('Expected ' + kind.__name__)


def main():
    checks = []
    with TemporaryDirectory(prefix='rogue-unused-audit-') as directory:
        root = Path(directory).resolve()
        put(root, 'Rogue10m.uproject', '{}')
        values = {'free': b'unreferenced output', 'consumer-used': b'backup consumer input',
                  'nested-used': b'nested project input', 'registered-used': b'registered worktree input',
                  'utf16-used': b'utf16 absolute reference input', 'checkpoint-used': b'checkpoint consumer input'}
        for name, content in values.items():
            put(root, 'tmp/output/' + name + '.bin', content)
            # KEEP_ROOTS must not prune retained evidence backup/baseline folders.
            put(root, 'Feature/doc/evidence/baseline-backup/' + name + '.bin', content)
        put(root, 'tmp/tracked.bin', values['free'])
        put(root, 'tmp/before-checkpoint/duplicate.bin', values['free'])
        put(root, 'tmp/task-backup/consume.py', "INPUT = 'tmp/output/consumer-used.bin'")
        for checkpoint_name in ('checkpoint', 'authoring-checkpoint'):
            put(root, 'tmp/stable-head/' + checkpoint_name + '/snapshot.bin', values['free'])
            put(root, 'tmp/stable-head/' + checkpoint_name + '/Scripts/consume.py',
                "INPUT = 'tmp/output/checkpoint-used.bin'")
        windows_absolute = str(root / 'tmp/output/utf16-used.bin').replace('/', '\\')
        put(root, 'tmp/baseline-2026/consume.ps1', windows_absolute.encode('utf-16'))
        project = root / 'tmp/project'
        put(root, 'tmp/project/.git', 'gitdir: metadata-is-not-opened')
        put(root, 'tmp/project/Fixture.uproject', '{}')
        put(root, 'tmp/project/Source/consumer.cpp', 'const auto Input = "tmp/output/nested-used.bin";')
        put(root, 'tmp/project/Content/deep/trap.py', "INPUT = 'tmp/output/free.bin'")
        put(root, 'tmp/project/Source/Intermediate/deep/trap.py', "INPUT = 'tmp/output/free.bin'")
        registered = root / 'tmp/registered'
        put(root, 'tmp/registered/Config/consumer.ini', 'Input=tmp/output/registered-used.bin')
        put(root, 'tmp/registered/Content/deep/trap.py', "INPUT = 'tmp/output/free.bin'")
        # Marker-free registered worktrees must be detected from Git metadata.
        for relative in ('tmp/apollo-tools/deep/trap.py', 'tmp/node_modules/package/trap.py',
                         'tmp/Intermediate/deep/trap.py', 'tmp/python-env/deep/trap.py',
                         'tmp/vendor/deep/trap.py'):
            put(root, relative, "INPUT = 'tmp/output/free.bin'")
        put(root, 'tmp/python-env/pyvenv.cfg', 'home = portable-python')
        put(root, 'tmp/vendor/.git/config', '[core]')
        put(root, 'Scripts/keep-authoring.py', '# Primary authored reference scope')
        original_hashes = hashes(root)
        visited = []
        real_walk = os.walk

        def tracked_walk(*args, **kwargs):
            for row in real_walk(*args, **kwargs):
                visited.append(Path(row[0]))
                yield row

        def fake_git(command, **kwargs):
            if 'ls-files' in command:
                return subprocess.CompletedProcess(command, 0, stdout=b'tmp/tracked.bin\0')
            if 'worktree' in command:
                output = ('worktree ' + str(root) + '\0HEAD fixture\0\0worktree '
                          + str(registered) + '\0HEAD fixture\0\0')
                return subprocess.CompletedProcess(command, 0, stdout=output.encode('utf-8'))
            raise AssertionError('Unexpected external command: ' + repr(command))

        output = root / 'Feature/doc/evidence/new-audit/candidates.json'
        with patch.object(audit.subprocess, 'run', fake_git), patch.object(audit.os, 'walk', tracked_walk):
            report = audit.run(root, output, progress=False)
        assert [entry['original'] for entry in report['candidates']] == ['tmp/output/free.bin']
        assert report['candidates'][0]['keep'] == 'Feature/doc/evidence/baseline-backup/free.bin'
        checks.append('untracked_duplicate_found_and_retained_evidence_backup_not_pruned')
        referenced = {entry['original']: set(entry['references']) for entry in report['referenced_duplicates']}
        assert referenced == {
            'tmp/output/consumer-used.bin': {'tmp/task-backup/consume.py'},
            'tmp/output/nested-used.bin': {'tmp/project/Source/consumer.cpp'},
            'tmp/output/registered-used.bin': {'tmp/registered/Config/consumer.ini'},
            'tmp/output/utf16-used.bin': {'tmp/baseline-2026/consume.ps1'},
            'tmp/output/checkpoint-used.bin': {
                'tmp/stable-head/checkpoint/Scripts/consume.py',
                'tmp/stable-head/authoring-checkpoint/Scripts/consume.py'},
        }, referenced
        checks.append('backup_baseline_utf16_and_nested_authored_references_remove_candidates')
        assert set(report['nested_projects']) == {'tmp/project', 'tmp/registered'}
        checks.append('marker_and_registered_projects_detected')
        forbidden = [root / relative for relative in (
            'tmp/apollo-tools', 'tmp/node_modules', 'tmp/Intermediate',
            'tmp/python-env/deep', 'tmp/vendor/deep', 'tmp/vendor/.git',
            'tmp/project/Content', 'tmp/project/Source/Intermediate', 'tmp/registered/Content')]
        assert not any(folder == boundary or boundary in folder.parents
                       for folder in visited for boundary in forbidden), visited
        checks.append('protected_install_generated_vendor_and_content_subtrees_never_descended')
        excluded = {(entry['phase'], entry['path']) for entry in report['excluded_subtrees']}
        assert ('candidates', 'tmp/task-backup') in excluded
        assert ('tmp_references', 'tmp/task-backup') not in excluded
        assert ('tmp_references', 'tmp/apollo-tools') in excluded
        assert ('nested_project_references', 'tmp/project/Source/Intermediate') in excluded
        assert report['summary']['issues'] == 0
        checks.append('phase_specific_exclusions_recorded_without_claiming_descendant_counts')
        for checkpoint_name in ('checkpoint', 'authoring-checkpoint'):
            checkpoint_path = 'tmp/stable-head/' + checkpoint_name
            assert ('candidates', checkpoint_path) in excluded
            assert ('tmp_references', checkpoint_path) not in excluded
            assert audit.preserve_reason(checkpoint_path + '/snapshot.bin', set()) is not None
        checks.append('checkpoint_and_suffixed_checkpoint_preserved_but_authored_references_scanned')
        assert {name: value for name, value in hashes(root).items()
                if name != output.relative_to(root).as_posix()} == original_hashes
        assert json.loads(output.read_text(encoding='utf-8'))['mode'] == 'audit_only_no_deletions'
        checks.append('audit_changes_only_new_manifest')

        # Suffix filtering happens before per-file filesystem inspection.
        filter_root = root / 'tmp/filter'
        non_script = put(root, 'tmp/filter/large.bin', b'unneeded file metadata')
        script = put(root, 'tmp/filter/inspect.py', '# selected')
        original_reparse = audit.reparse

        def suffix_guard(path):
            assert path != non_script, 'Non-script reached per-file inspection'
            return original_reparse(path)

        with patch.object(audit, 'reparse', suffix_guard):
            assert list(audit.safe_files(filter_root, root, [], suffixes=audit.SCRIPT_SUFFIXES)) == [script]
        checks.append('reference_suffix_filter_precedes_per_file_inspection')

        # Simulate Windows reparse attributes without requiring symlink privilege.
        linked = root / 'tmp/reparse-boundary'
        put(root, 'tmp/reparse-boundary/inside.py', '# must not be visited')
        issues = []
        with patch.object(audit, 'reparse', lambda path: path == linked or original_reparse(path)):
            assert list(audit.safe_files(linked, root, issues)) == []
        assert issues and issues[0]['reason'] == 'reparse ancestor'
        checks.append('reparse_ancestor_blocks_traversal')

        expect_error(lambda: audit.run(root, root.parent / 'outside-audit.json', progress=False), ValueError)
        expect_error(lambda: audit.run(root, output, progress=False), FileExistsError)
        reparse_output = put(root, 'reparse-output.json', '{}')
        with patch.object(audit, 'reparse', lambda path: path == reparse_output or original_reparse(path)):
            expect_error(lambda: audit.run(root, reparse_output, progress=False), RuntimeError)
        reparse_parent = root / 'reparse-parent'
        reparse_parent.mkdir()
        with patch.object(audit, 'reparse', lambda path: path == reparse_parent or original_reparse(path)):
            expect_error(lambda: audit.run(root, reparse_parent / 'out.json', progress=False), RuntimeError)
        checks.append('output_outside_root_existing_file_and_reparse_paths_rejected')

        # A second writer may publish evidence after the final path check.
        race_output = root / 'Feature/doc/evidence/new-audit/race-exclusive.json'
        original_check = audit.checked_output
        output_checks = []

        def concurrent_writer(check_root, check_path):
            result = original_check(check_root, check_path)
            output_checks.append(check_path)
            if len(output_checks) == 3:
                check_path.write_bytes(b'concurrent evidence must survive')
            return result

        with patch.object(audit.subprocess, 'run', fake_git), \
                patch.object(audit, 'checked_output', concurrent_writer):
            expect_error(lambda: audit.run(root, race_output, progress=False), FileExistsError)
        assert race_output.read_bytes() == b'concurrent evidence must survive'
        checks.append('exclusive_create_preserves_evidence_created_after_final_path_check')

        # CLI defaults must use a fresh timestamped path and never the old audit.
        cli_outputs = []
        with patch.object(audit, 'run', lambda root, output: cli_outputs.append(output) or {'summary': {}}), \
                patch.object(audit.argparse.ArgumentParser, 'parse_args',
                             return_value=type('Args', (), {'root': root, 'output': None})()), \
                redirect_stdout(StringIO()):
            audit.main()
        assert len(cli_outputs) == 1
        assert cli_outputs[0].parent.name.startswith('unused-file-audit-')
        assert 'T' in cli_outputs[0].parent.name and cli_outputs[0].parent.name.endswith('Z')
        assert '20260928' not in str(cli_outputs[0])
        checks.append('cli_default_uses_new_timestamped_evidence_path')
    print(json.dumps({'result': 'PASS', 'checks': checks}, indent=2))


if __name__ == '__main__':
    main()
