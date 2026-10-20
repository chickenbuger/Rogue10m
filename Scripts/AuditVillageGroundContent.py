"""Checkpoint and verify Content bytes for the material-only village repair."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/village-ground-20261005'


def fingerprint(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return {'size': path.stat().st_size, 'sha256': digest.hexdigest()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    files = sorted(p for p in (ROOT / 'Content').rglob('*') if p.is_file())
    with ThreadPoolExecutor(max_workers=4) as workers:
        current = dict(zip((p.relative_to(ROOT).as_posix() for p in files), workers.map(fingerprint, files)))
    baseline_path = EVIDENCE / 'baseline-content.json'
    if args.baseline:
        if baseline_path.exists():
            raise RuntimeError('Preserve the existing checkpoint')
        baseline_path.write_text(json.dumps(current, indent=2), encoding='utf-8')
        print('Content baseline:', len(current), 'files')
        return
    baseline = json.loads(baseline_path.read_text(encoding='utf-8'))
    application = json.loads((EVIDENCE / 'application.json').read_text(encoding='utf-8'))
    allowed = {'Content/Rogue10m/Maps/StartHub.umap',
               'Content/Rogue10m/World/StartVillage/Materials/MI_StartVillage_Grass.uasset',
               'Content/Rogue10m/World/StartVillage/Materials/MI_StartVillage_EarthPath.uasset'}
    allowed.update(package.replace('/Game/', 'Content/', 1) + '.uasset'
                   for package in application['actor_packages'])
    changed = sorted(p for p in baseline if p in current and baseline[p] != current[p])
    missing = sorted(set(baseline) - set(current))
    new = sorted(set(current) - set(baseline))
    unexpected_changes = sorted(set(changed) - allowed)
    unexpected_new = [p for p in new if not p.startswith('Content/Rogue10m/World/StartVillage/Materials/')]
    passed = not (missing or unexpected_changes or unexpected_new)
    result = {'status': 'passed' if passed else 'failed', 'baseline_files': len(baseline),
              'unchanged_sha256': len(baseline) - len(changed) - len(missing),
              'changed_files': changed, 'new_files': new, 'missing': missing,
              'unexpected_changes': unexpected_changes, 'unexpected_new': unexpected_new}
    (EVIDENCE / 'preservation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2))
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
