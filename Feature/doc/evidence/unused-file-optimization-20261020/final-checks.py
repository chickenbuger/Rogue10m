"""Bounded final syntax, scope, append-only log and document-link checks."""
import ast
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def output(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT).decode('utf-8')


assert not output('diff', '--name-only', 'HEAD', '--', 'Source', 'Config', 'Content', 'Rogue10m.uproject').strip()
syntax = [ROOT / 'Scripts/Editor/EnhanceFighterArmSwing.py', *HERE.glob('*.py')]
for path in syntax:
    ast.parse(path.read_text(encoding='utf-8-sig'), filename=str(path))
json_files = list(HERE.glob('*.json'))
for path in json_files:
    json.loads(path.read_text(encoding='utf-8-sig'))
for relative in ['DevLog/20261020.txt', 'Docs/SprintChangeLog.md']:
    previous = output('show', 'HEAD:' + relative).replace('\r\n', '\n')
    assert (ROOT / relative).read_text(encoding='utf-8-sig').startswith(previous), relative
documents = [ROOT / 'Feature/architect/2026-10-20_unused-file-optimization.md',
             ROOT / 'Feature/doc/2026-10-20_unused-file-optimization.md', *HERE.glob('*.md')]
links = []
for path in documents:
    for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)', path.read_text(encoding='utf-8-sig')):
        if target.startswith(('http:', 'https:', '#')):
            continue
        assert (path.parent / target.split('#')[0]).exists(), (str(path), target)
        links.append(target)
changed = {path for path in output('diff', '--name-only', '-z', 'HEAD').split('\0') if path}
changed.update(path for path in output('ls-files', '--others', '--exclude-standard', '-z').split('\0') if path)
for name in ['commit-files.txt', 'final-checks.json']:
    changed.add((HERE / name).relative_to(ROOT).as_posix())
selected = json.loads((HERE / 'selection.json').read_text(encoding='utf-8'))['files']
allowed = {row['path'] for row in selected}
allowed.update(row['path'] + '.gz' for row in selected if row['action'] == 'gzip')
allowed.update(['DevLog/20261020.txt', 'Docs/SprintChangeLog.md',
                'Scripts/Editor/EnhanceFighterArmSwing.py',
                'Feature/architect/2026-10-20_unused-file-optimization.md',
                'Feature/doc/2026-10-20_unused-file-optimization.md'])
assert all(path in allowed or path.startswith('Feature/doc/evidence/unused-file-optimization-20261020/') for path in changed), changed - allowed
rows = []
for path in sorted(changed):
    if path in {row['path'] for row in selected}:
        reason = '검증된 중복/무손실 보관 원본 정리'
    elif path.endswith('.json.gz'):
        reason = '원본 바이트 동일 무손실 검증 자료'
    elif path.startswith('Scripts/'):
        reason = '압축 자료 읽기와 기준선 덮어쓰기 보호'
    else:
        reason = '최적화 계획·결과·검증·복구·개발 기록'
    rows.append(path + ' | ' + reason)
(HERE / 'commit-files.txt').write_text('\n'.join(rows) + '\n', encoding='utf-8')
report = {'result': 'PASS', 'changed_files': len(changed), 'python_syntax_files': len(syntax),
          'parsed_json_reports': len(json_files), 'verified_doc_links': len(links),
          'append_only_logs': True, 'source_config_content_changes': 0,
          'baseline': output('rev-parse', 'HEAD').strip()}
(HERE / 'final-checks.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
