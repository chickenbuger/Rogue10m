"""Safety-contract tests with mocked child processes; no UE/process-query calls."""
import argparse
import ast
from contextlib import redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).parent
SPEC = importlib.util.spec_from_file_location('owner_driver', HERE / 'owner_driver.py')
D = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(D)
H = 'A' * 64


class OwnerContracts(unittest.TestCase):
    def test_default_plan_has_no_mutations_queries_or_engine_calls(self):
        with patch.object(D, 'validate_host', side_effect=AssertionError('host scan')), patch.object(D, 'active_processes', side_effect=AssertionError('process scan')), patch.object(D.subprocess, 'Popen', side_effect=AssertionError('launch')):
            out = io.StringIO()
            with redirect_stdout(out):
                code = D.main(['--config', str(HERE / 'config.UNEXECUTED.json')])
        self.assertEqual(code, 0)
        result = json.loads(out.getvalue())
        self.assertEqual(result['status'], 'UNEXECUTED')
        self.assertEqual([result[x] for x in ('engine_calls', 'process_queries', 'writes')], [0, 0, 0])

    def test_execute_without_root_lease_stops_before_host_or_process_access(self):
        with patch.object(D, 'validate_host', side_effect=AssertionError('host scan')), patch.object(D.subprocess, 'Popen', side_effect=AssertionError('launch')):
            out = io.StringIO()
            with redirect_stdout(out):
                code = D.main(['--config', str(HERE / 'config.UNEXECUTED.json'), '--phase', 'startup', '--execute'])
        self.assertEqual(code, 2)
        self.assertIn('Explicit Root', json.loads(out.getvalue())['error'])

    def test_five_line_runner_binds_exact_sys_argv_without_inner_commandline(self):
        argv = ['--config', 'C:/owner/config.json', '--phase', 'move', '--execute']
        script = D.wrapper_bytes(D.FIXTURE, argv)
        self.assertEqual(len(script.splitlines()), 5)
        tree = ast.parse(script)
        assignment = next(x for x in tree.body if isinstance(x, ast.Assign))
        self.assertEqual(ast.literal_eval(assignment.value), [str(D.FIXTURE), *argv])
        self.assertIn("run_name='__main__'", script)

    def test_native_phase_argv_never_renames_and_script_is_single_plain_path(self):
        config = json.loads((HERE / 'config.UNEXECUTED.json').read_text())
        config['owner_lease'] = 'ROOT_EXCLUSIVE_TEST'
        config['_sha'] = H
        with patch.object(D, 'reservation', return_value={'candidate_map': '/Game/Rogue10m/Maps/QA/MapMakerMoveReload_' + 'a' * 32}), patch.object(D, 'startup_surface', return_value={}), patch.object(D, 'sha', return_value=H):
            duplicate = D.make_plan(config, 'duplicate')
            startup = D.make_plan(config, 'startup')
        self.assertFalse(any(x.lower() == '-rename' for x in duplicate['argv']))
        self.assertIn('-Builder=WorldPartitionRenameDuplicateBuilder', duplicate['argv'])
        script = next(x for x in startup['argv'] if x.startswith('-Script='))
        self.assertNotIn(' ', script)
        self.assertNotIn('"', script)
        for flag in ('-Unattended', '-NullRHI', '-NoLiveCoding'):
            self.assertIn(flag, startup['argv'])

    def test_absent_approval_cannot_launch_probe(self):
        args = argparse.Namespace(approval=None, approval_sha256=None)
        with self.assertRaisesRegex(D.Blocked, 'Root-authored pinned'):
            D.approval(args, {}, {})

    def test_overlay_change_invalidates_exact_owner_approval(self):
        config = {'owner_lease': 'ROOT_ONLY', '_sha': H, 'origin_guard': {'sha256': H}}
        plan = {'phase': 'startup', 'argv_sha256': H, 'driver_sha256': H, 'probe_sha256': H,
                'wrapper_sha256': H, 'selection_sha256': None, 'startup_surface_sha256': H}
        permit = {'schema': 'R10-mapmaker-owner-approval-v1', 'approved': True, 'owner': 'Root',
                  'owner_lease': 'ROOT_ONLY', 'phase': 'startup', 'config_sha256': H, 'argv_sha256': H,
                  'driver_sha256': H, 'probe_sha256': H, 'fixture_sha256': D.FIXTURE_SHA,
                  'physical_receipt_sha256': D.PHYSICAL_SHA, 'audit_manifest_sha256': D.AUDIT_SHA,
                  'host_context_sha256': D.HOST_SHA, 'origin_guard_sha256': H, 'wrapper_sha256': H,
                  'selection_sha256': None, 'startup_surface_sha256': H,
                  **{x: True for x in ('final_argv_reviewed', 'plugin_plan_reviewed', 'startup_before_probe_audit_accepted', 'no_ui_or_actor_startup_actions_approved')}}
        args = argparse.Namespace(approval='unused.json', approval_sha256=H)
        with patch.object(D, 'pin', return_value=Path('unused.json')), patch.object(D, 'load', return_value=permit):
            D.approval(args, config, plan)
            with self.assertRaises(D.Blocked):
                D.approval(args, config, {**plan, 'startup_surface_sha256': 'B' * 64})

    def test_native_timeout_preserves_owned_pid_and_never_kills(self):
        class FakeProcess:
            pid = 123
            def wait(self, timeout):
                raise subprocess.TimeoutExpired('mock-only', timeout)
            def kill(self):
                raise AssertionError('kill called')
            def terminate(self):
                raise AssertionError('terminate called')
        with tempfile.TemporaryDirectory(prefix='offline_owned_', dir=HERE) as owned:
            plan = {'directory': owned, 'executable': str(D.ENGINE / 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'), 'argv': []}
            config = {'timeout_seconds': 30, 'executable': {'sha256': H}}
            identity = {'pid': 123, 'creation_filetime_100ns': '456', 'executable': plan['executable']}
            with patch.object(D.subprocess, 'Popen', return_value=FakeProcess()), patch.object(D, 'child_identity', return_value=identity):
                with self.assertRaisesRegex(D.Blocked, 'outcome uncertain'):
                    D.launch(config, plan)
            result = json.loads((Path(owned) / 'uncertain.json').read_text())
            self.assertFalse(result['kill_attempted'])
            self.assertFalse((Path(owned) / 'exit-receipt.json').exists())

    def test_process_query_timeout_never_uses_run_auto_kill(self):
        class FakeQuery:
            pid = 789
            def communicate(self, timeout):
                raise subprocess.TimeoutExpired('mock-query-only', timeout)
            def kill(self):
                raise AssertionError('kill called')
        with patch.object(D, 'pin'), patch.object(D.subprocess, 'Popen', return_value=FakeQuery()):
            with self.assertRaisesRegex(D.Blocked, 'retained without killing'):
                D.active_processes({'path': 'unused-powershell', 'sha256': H})

    def test_probe_is_getter_only_and_existing_fixture_stays_pinned(self):
        tree = ast.parse((HERE / 'startup_probe.py').read_text())
        calls = {x.func.attr for x in ast.walk(tree) if isinstance(x, ast.Call) and isinstance(x.func, ast.Attribute)}
        self.assertFalse(calls & {'load_asset', 'load_object', 'load_class', 'set_editor_property', 'get_all_level_actors',
                                 'set_actor_transform', 'save_packages', 'select_actor', 'set_selected_level_actors'})
        self.assertIn('find_object', calls)
        self.assertEqual(D.sha(D.FIXTURE), D.FIXTURE_SHA)


if __name__ == '__main__':
    unittest.main(verbosity=2)
