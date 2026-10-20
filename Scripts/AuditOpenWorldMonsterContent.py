"""Verify task-owned binary changes against the full pre-authoring SHA checkpoint."""
import json
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from AuditVillageGroundContent import fingerprint

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/open-world-monsters-20261005'


def main():
    baseline = json.loads((EVIDENCE / 'baseline-content.json').read_text(encoding='utf-8'))
    files = sorted(p for p in (ROOT / 'Content').rglob('*') if p.is_file())
    with ThreadPoolExecutor(max_workers=4) as workers:
        current = dict(zip((p.relative_to(ROOT).as_posix() for p in files), workers.map(fingerprint, files)))
    application = json.loads((EVIDENCE / 'application.json').read_text(encoding='utf-8'))
    npc = json.loads((EVIDENCE / 'npc-application.json').read_text(encoding='utf-8'))
    allowed = {'Content/Rogue10m/Maps/OpenRunMap.umap', 'Content/Rogue10m/Maps/StartHub.umap',
               'Content/AI/Monster/BB_Monster.uasset'}
    for package in application['saved_assets'] + npc['saved_assets'] + application['actor_packages'] + npc['actor_packages']:
        allowed.add(package.replace('/Game/', 'Content/', 1) + '.uasset')
    changed = sorted(p for p in baseline if p in current and baseline[p] != current[p])
    missing = sorted(set(baseline) - set(current))
    new = sorted(set(current) - set(baseline))
    unexpected = sorted((set(changed) | set(new)) - allowed)
    report = {'status': 'passed' if not missing and not unexpected else 'failed',
              'baseline_files': len(baseline), 'unchanged_sha256': len(baseline)-len(changed)-len(missing),
              'changed_files': changed, 'new_files': new, 'missing': missing, 'unexpected': unexpected,
              'blackboard_checkpoint_note': 'BB_Monster was dirty after native AI asset loading and individually saved before Editor restart.'}
    (EVIDENCE / 'preservation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    if report['status'] != 'passed':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
