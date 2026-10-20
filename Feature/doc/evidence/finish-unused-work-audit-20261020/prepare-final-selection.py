"""Resolve both independent reviews conservatively; never delete files."""
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
reviews = defaultdict(list)
for name in ['candidate-review-art.json', 'candidate-review-other.json']:
    for row in json.loads((HERE / name).read_text(encoding='utf-8-sig'))['files']:
        reviews[row['original']].append({'review': name, **row})
initial = json.loads((HERE / 'tmp-audit.json').read_text(encoding='utf-8'))
final = json.loads((HERE / 'tmp-audit-final.json').read_text(encoding='utf-8'))
assert not final['issues'] and len(reviews) == len(initial['candidates']) == 471
old = json.loads((ROOT / 'Feature/doc/evidence/unused-file-cleanup-20260928/reviewed_candidates.json').read_text(encoding='utf-8-sig'))
protected = {row['original'] for row in old['files'] if row['decision'] == 'preserve'}
tracked = set(subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0'))
selected, decisions = [], []
for row in final['candidates']:
    observations = reviews[row['original']]
    # Require both reviewers to recommend; any preserve decision wins.
    approved = (len(observations) == 2 and row['original'] not in protected
                and all(item['decision'].startswith('recommend') for item in observations))
    decisions.append({'original': row['original'], 'decision': 'delete_duplicate' if approved else 'preserve',
                      'reviews': [{'file': item['review'], 'decision': item['decision'],
                                   'reason': item.get('reason', item.get('reasons'))} for item in observations]})
    if not approved:
        continue
    assert row['original'] not in tracked and row['keep'] in tracked
    for path in [row['original'], row['keep']]:
        content = (ROOT / path).read_bytes()
        assert len(content) == row['bytes'] and hashlib.sha256(content).hexdigest() == row['sha256']
    selected.append({'action': 'duplicate', 'path': row['original'], 'keep': row['keep'],
                     'bytes': row['bytes'], 'sha256': row['sha256'],
                     'reason': 'Both independent reviewers confirmed completed diagnostic/build output; canonical committed evidence is used by current verifiers.'})
assert len(selected) == 4 and sum(row['bytes'] for row in selected) == 11947
selection = {'schema_version': 1, 'baseline_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
             'files': selected, 'original_audit_candidates': 471, 'final_audit_candidates': len(final['candidates']),
             'policy': 'Both independent recommendations required; any preserve decision wins; prior87 protection unchanged.'}
report = {'result': 'PASS', 'initial_review_coverage': len(reviews),
          'checkpoint_candidates_moved_to_protected_subtree': 242,
          'final_candidates': len(decisions), 'selected': len(selected), 'selected_bytes': 11947,
          'preserved_final_candidates': len(decisions) - len(selected), 'referenced_duplicates_preserved': 15,
          'decisions': decisions}
for name, value in [('selection.json', selection), ('candidate-review-final.json', report)]:
    with (HERE / name).open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({key: value for key, value in report.items() if key != 'decisions'}))
