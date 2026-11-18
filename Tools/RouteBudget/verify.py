"""Run offline regressions and fresh static checks without overwriting evidence."""
import argparse
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import unittest

sys.dont_write_bytecode = True
import route_budget as rb

ROOT = Path(__file__).resolve().parent
SPEC_DIR = rb.PROJECT_ROOT / 'Feature' / 'doc' / 'evidence' / 'five-village-route-spec-20261110'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def write_json(path, value):
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=Path, default=rb.DEFAULT_PLAN)
    args = parser.parse_args()
    run_name = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    evidence = ROOT / 'evidence' / run_name
    evidence.mkdir(parents=True, exist_ok=False)
    source_files = {
        'validated-plan.json': args.plan.resolve(),
        'validated-static-verifier.ps1': SPEC_DIR / 'VerifyRoutePlan.ps1',
    }
    source_records = []
    for name, source in source_files.items():
        raw = source.read_bytes()
        with (evidence / name).open('xb') as stream:
            stream.write(raw)
        source_records.append({'source': str(source), 'snapshot': name, 'snapshot_sha256': sha(raw)})
    plan_timing = json.loads((evidence / 'validated-plan.json').read_text(encoding='utf-8-sig'))['timing']
    requested_endpoint_valid = (plan_timing['endpoint'] == 'boss_entrance_crossing' and plan_timing['content_entry_target_seconds'] == 900)

    output = io.StringIO()
    suite = unittest.defaultTestLoader.discover(str(ROOT), pattern='test_route_budget.py')
    tests = unittest.TextTestRunner(stream=output, verbosity=2).run(suite)
    with (evidence / 'unittest.txt').open('x', encoding='utf-8') as stream:
        stream.write(output.getvalue())
    report_commands = [subprocess.run(
        [sys.executable, '-B', str(ROOT / 'route_budget.py'), '--plan', str(evidence / 'validated-plan.json'), 'report'],
        cwd=ROOT, capture_output=True, text=True, encoding='utf-8') for _ in range(2)]
    reproducible = all(command.returncode == 0 for command in report_commands) and report_commands[0].stdout == report_commands[1].stdout
    if report_commands[0].returncode == 0:
        with (evidence / 'model-report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(report_commands[0].stdout)
    else:
        with (evidence / 'model-report-failure.txt').open('x', encoding='utf-8') as stream:
            stream.write('\n'.join(command.stderr for command in report_commands))

    powershell = shutil.which('pwsh') or shutil.which('powershell')
    static = None
    if powershell:
        static_command = subprocess.run(
            [powershell, '-NoProfile', '-File', str(evidence / 'validated-static-verifier.ps1'),
             '-PlanPath', str(evidence / 'validated-plan.json'),
             '-ReportPath', str(evidence / 'existing-static-validation.json')],
            cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
        static_log = static_command.stdout + static_command.stderr
        if static_command.returncode == 0:
            static = json.loads((evidence / 'existing-static-validation.json').read_text(encoding='utf-8-sig'))
    else:
        static_log = 'PowerShell was not found; fresh static verification did not run.'
    with (evidence / 'static-verification.txt').open('x', encoding='utf-8') as stream:
        stream.write(static_log)

    for item in source_records:
        source = Path(item['source'])
        item['source_sha256_at_finish'] = sha(source.read_bytes()) if source.is_file() else None
        item['source_matches_snapshot_at_finish'] = item['source_sha256_at_finish'] == item['snapshot_sha256']
    fixture = ROOT / 'inputs' / 'route-plan.json'
    manifest = {
        'recorded_utc': datetime.now(timezone.utc).isoformat(),
        'evidence_kind': 'design_static_only', 'runtime_measured': False,
        'python_executable': sys.executable, 'sources': source_records,
        'regression_fixture': {'path': str(fixture), 'sha256': sha(fixture.read_bytes())},
        'implementation_sha256': {name: sha((ROOT / name).read_bytes()) for name in ('route_budget.py', 'test_route_budget.py', 'verify.py')},
    }
    write_json(evidence / 'input-manifest.json', manifest)
    success = tests.wasSuccessful() and reproducible and static is not None and static['failure_count'] == 0 and requested_endpoint_valid
    summary = {
        'status': 'offline_checks_passed' if success else 'offline_checks_failed',
        'test_count': tests.testsRun, 'failures': len(tests.failures), 'errors': len(tests.errors), 'skipped': len(tests.skipped),
        'model_report_byte_reproducible': reproducible,
        'fresh_static_verification_ran': static is not None,
        'existing_static_check_count': static['check_count'] if static else None,
        'existing_static_failure_count': static['failure_count'] if static else None,
        'validated_plan_sha256': source_records[0]['snapshot_sha256'],
        'sources_unchanged_during_validation': all(item['source_matches_snapshot_at_finish'] for item in source_records),
        'timing_endpoint': plan_timing['endpoint'], 'content_entry_target_seconds': plan_timing['content_entry_target_seconds'],
        'matches_requested_15_minute_entrance_target': requested_endpoint_valid,
        'runtime_validation': 'not_run', 'runtime_measured': False,
        'evidence_directory': str(evidence),
    }
    write_json(evidence / 'verification-summary.json', summary)
    print(json.dumps(summary, indent=2))
    return 0 if success else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as exc:
        print(f'Offline verification could not finish: {exc}', file=sys.stderr)
        raise SystemExit(2)
