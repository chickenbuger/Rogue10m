"""Audit all original Content bytes and the scope of new open-world assets."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/open-world-20261005'
ALLOWED_CHANGES = {
    'Content/Rogue10m/Maps/StartHub.umap',
    'Content/__ExternalActors__/Rogue10m/Maps/StartHub/4/6L/9ETBXYOM6DWLFQ0NWB8O0H.uasset',
}
NEW_PREFIXES = (
    'Content/Rogue10m/World/OpenWorld/',
    'Content/__ExternalActors__/Rogue10m/Maps/OpenRunMap/',
    'Content/__ExternalObjects__/Rogue10m/Maps/OpenRunMap/',
)


def fingerprint(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return {'size': path.stat().st_size, 'sha256': digest.hexdigest()}


def main():
    baseline = json.loads((EVIDENCE / 'baseline-content.json').read_text(encoding='utf-8'))

    def inspect(item):
        relative, expected = item
        path = ROOT / relative
        if not path.is_file():
            return relative, 'missing'
        return relative, 'unchanged' if fingerprint(path) == expected else 'changed'

    with ThreadPoolExecutor(max_workers=4) as workers:
        results = list(workers.map(inspect, baseline.items()))
    changed = sorted(path for path, state in results if state == 'changed')
    missing = sorted(path for path, state in results if state == 'missing')
    new = sorted(path.relative_to(ROOT).as_posix() for path in (ROOT / 'Content').rglob('*')
                 if path.is_file() and path.relative_to(ROOT).as_posix() not in baseline)
    unexpected_new = [path for path in new
                      if path != 'Content/Rogue10m/Maps/OpenRunMap.umap'
                      and not path.startswith(NEW_PREFIXES)]
    unexpected_changes = sorted(set(changed) - ALLOWED_CHANGES)
    passed = not (missing or unexpected_changes or unexpected_new)
    report = {'status': 'passed' if passed else 'failed',
              'baseline_content_files': len(baseline),
              'sha256_unchanged_files': sum(state == 'unchanged' for _, state in results),
              'intended_hub_portal_changes': changed,
              'missing_files': missing,
              'unexpected_changes': unexpected_changes,
              'new_content_files': len(new),
              'new_content_paths': new,
              'unexpected_new_paths': unexpected_new,
              'note': 'Full size+SHA256 audit after the final scoped MCP save, including river polish.'}
    (EVIDENCE / 'preservation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key != 'new_content_paths'}, indent=2))
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
