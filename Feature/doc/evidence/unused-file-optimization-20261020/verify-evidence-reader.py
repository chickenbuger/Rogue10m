"""Exercise the actual evidence reader without importing or starting Unreal.

Run again after compression to include all three archived pose reports. This
test creates fixtures only in an automatically removed temporary directory.
"""
from __future__ import annotations

import argparse
import ast
import gzip
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace


def load_functions(source: Path) -> dict:
    tree = ast.parse(source.read_text(encoding='utf-8'))
    names = {'evidence_exists', 'read', 'main'}
    functions = [node for node in tree.body
                 if isinstance(node, ast.FunctionDef) and node.name in names]
    if {node.name for node in functions} != names:
        raise AssertionError('The actual evidence reader/guard functions are missing')
    namespace = {'gzip': gzip, 'json': json}
    exec(compile(ast.Module(body=functions, type_ignores=[]), str(source), 'exec'), namespace)
    return namespace


def expect_error(callback, exception_type):
    try:
        callback()
    except exception_type as error:
        return error
    raise AssertionError('Expected ' + exception_type.__name__)


def verify(root: Path) -> dict:
    source = root / 'Scripts/Editor/EnhanceFighterArmSwing.py'
    namespace = load_functions(source)
    checks = []
    with TemporaryDirectory(prefix='rogue-evidence-reader-') as directory:
        evidence = Path(directory)
        namespace['EVIDENCE'] = evidence
        reader = namespace['read']
        exists = namespace['evidence_exists']
        plain = evidence / 'fixture.json'
        archive = evidence / 'fixture.json.gz'
        original = {'phase': 'plain', 'text': '기준선 보존', 'values': [1, 2.5, None]}
        plain.write_text(json.dumps(original, ensure_ascii=False), encoding='utf-8')
        assert exists('fixture.json') and reader('fixture.json') == original
        checks.append('plain_json_utf8')

        archived = {'phase': 'archived', 'text': '압축 검증'}
        archive.write_bytes(gzip.compress(json.dumps(archived, ensure_ascii=False).encode('utf-8'), mtime=0))
        assert reader('fixture.json') == original
        checks.append('plain_has_priority_when_both_exist')
        plain.unlink()
        assert exists('fixture.json') and reader('fixture.json') == archived
        checks.append('gzip_fallback_utf8')

        assert not exists('missing.json')
        expect_error(lambda: reader('missing.json'), FileNotFoundError)
        checks.append('missing_evidence_fails')
        archive.write_bytes(b'not a gzip stream')
        expect_error(lambda: reader('fixture.json'), gzip.BadGzipFile)
        checks.append('corrupt_gzip_fails')

        # A present but invalid plain report must fail, without using its archive.
        archive.write_bytes(gzip.compress(b'{"valid": true}', mtime=0))
        plain.write_text('{invalid', encoding='utf-8')
        expect_error(lambda: reader('fixture.json'), json.JSONDecodeError)
        checks.append('corrupt_plain_does_not_fall_back')

        baseline = evidence / 'baseline.json.gz'
        baseline_bytes = gzip.compress(b'{"historical": true}', mtime=0)
        baseline.write_bytes(baseline_bytes)
        assert exists('baseline.json')
        capture_calls = []
        namespace.update({
            'ROOT': root,
            'EXPECTED_ROOT': root,
            'MANNY': 'offline_mesh',
            'require': lambda path, cls: object(),
            'capture': lambda mesh: capture_calls.append(mesh),
            'unreal': SimpleNamespace(
                SkeletalMesh=object,
                SystemLibrary=SimpleNamespace(
                    get_command_line=lambda: '',
                    parse_command_line=lambda value: ([], [], {}))),
        })
        error = expect_error(namespace['main'], RuntimeError)
        assert 'Baseline already exists' in str(error)
        assert not capture_calls and baseline.read_bytes() == baseline_bytes
        assert not (evidence / 'baseline.json').exists()
        checks.append('archived_baseline_blocks_capture_without_overwrite')

    # Parse actual archives using the extracted reader. Keep peak memory bounded
    # by releasing each report before opening the next one.
    namespace['EVIDENCE'] = root / 'Feature/doc/evidence/fighter-arm-swing-20261005'
    reports = []
    for name in ('baseline.json', 'in-memory-poses.json', 'reload-poses.json'):
        archive = namespace['EVIDENCE'] / (name + '.gz')
        if not archive.exists():
            reports.append({'name': name, 'status': 'archive_not_present'})
            continue
        report = namespace['read'](name)
        assert isinstance(report, dict) and report, name
        clips = report['clips'] if name == 'baseline.json' else report
        assert len(clips) == 10, name
        reports.append({'name': name, 'status': 'PASS', 'clip_count': len(clips),
                        'reader_used_gzip': not (namespace['EVIDENCE'] / name).exists()})
        del report, clips
    return {'result': 'PASS', 'fixture_checks': checks, 'actual_archives': reports,
            'unreal_imported': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[4])
    args = parser.parse_args()
    print(json.dumps(verify(args.root.resolve(strict=True)), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
