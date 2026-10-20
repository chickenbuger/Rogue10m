"""Inventory the current Content tree; allow only the moving-attack AnimBP change."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/moving-upper-body-attack-20261009'
ALLOWED = 'Content/Rogue10m/Animation/Common/ABP_Common_Unarmed.uasset'


def inventory():
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((ROOT / 'Content').rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    path = EVIDENCE / 'content-before.json'
    current = inventory()
    if args.baseline:
        if path.exists():
            raise RuntimeError('Baseline already exists; refusing to overwrite')
        EVIDENCE.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(current, indent=2), encoding='utf-8')
        print('MOVING_ATTACK_CONTENT_BASELINE files=' + str(len(current)))
        return
    before = json.loads(path.read_text(encoding='utf-8'))
    added, removed = sorted(current.keys() - before.keys()), sorted(before.keys() - current.keys())
    changed = sorted(p for p in before.keys() & current.keys() if before[p] != current[p])
    result = {'before_files': len(before), 'after_files': len(current), 'added': added,
              'removed': removed, 'changed': changed,
              'passed': not added and not removed and changed == [ALLOWED]}
    (EVIDENCE / 'content-after.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
    if not result['passed']:
        raise RuntimeError('Unexpected Content changes')


if __name__ == '__main__':
    main()
