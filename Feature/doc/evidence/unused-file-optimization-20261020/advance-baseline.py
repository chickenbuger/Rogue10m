"""Adopt already-present upstream docs without rewriting shared worktree files."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT).decode().strip()


branch = git('symbolic-ref', 'HEAD')
if branch != 'refs/heads/Sprint#6-2-unused-file-optimization':
    raise RuntimeError('Unexpected branch')
previous, target = git('rev-parse', 'HEAD'), git('rev-parse', 'main')
subprocess.run(['git', 'diff', '--cached', '--quiet'], cwd=ROOT, check=True)
subprocess.run(['git', 'merge-base', '--is-ancestor', previous, target], cwd=ROOT, check=True)
paths = subprocess.check_output(['git', 'diff', '--name-only', '-z', previous, target], cwd=ROOT).decode().split('\0')
checks = []
for path in filter(None, paths):
    current = git('hash-object', '--path=' + path, path)
    committed = git('rev-parse', target + ':' + path)
    if current != committed:
        raise RuntimeError('Upstream file differs from worktree: ' + path)
    checks.append({'path': path, 'blob': current})
subprocess.run(['git', 'update-ref', branch, target, previous], cwd=ROOT, check=True)
subprocess.run(['git', 'read-tree', target], cwd=ROOT, check=True)
report = {'previous': previous, 'adopted_main': target, 'verified_files': checks,
          'worktree_files_rewritten': False, 'result': 'PASS'}
destination = Path(__file__).with_name('baseline-adoption.json')
destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
