"""Reuse the proven per-file optimizer for this independently reviewed selection."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
spec = importlib.util.spec_from_file_location('historical_optimizer',
    ROOT / 'Feature/doc/evidence/unused-file-optimization-20261020/optimize.py')
optimizer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(optimizer)
optimizer.HERE, optimizer.ROOT, optimizer.MANIFEST = HERE, ROOT, HERE / 'selection.json'

if __name__ == '__main__':
    import json
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['preview', 'apply', 'verify', 'restore'], default='preview')
    args = parser.parse_args()
    tracked = set(subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0'))
    for row in json.loads(optimizer.MANIFEST.read_text(encoding='utf-8'))['files']:
        if row['path'] in tracked or row['keep'] not in tracked:
            raise RuntimeError('Expected untracked output and committed retained copy: ' + row['path'])
    optimizer.run(args.mode)
