"""Create the explicitly reviewed selection, no deletions or compression."""
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
PAIRS = [
    ('arin-reference-refinement-20261009/blender-save-backups/Arin.blend1',
     'arin-reference-refinement-20261009/third-build/Arin.blend'),
    ('arin-reference-refinement-20261009/preview/preview.blend1',
     'arin-reference-refinement-20261009/preview-attempt2/preview.blend'),
    ('arin-reference-refinement-20261009/preview-attempt2/preview.blend1',
     'arin-reference-refinement-20261009/preview-attempt1/preview.blend'),
    ('arin-quality-20261007/blender-save-backups/Arin_EditableParts.blend1',
     'arin-quality-20261007/export-candidate-1/Arin_EditableParts.blend'),
]


def facts(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            value.update(block)
    return {'bytes': path.stat().st_size, 'sha256': value.hexdigest()}


rows = []
for original, keep in PAIRS:
    original, keep = 'Feature/doc/evidence/' + original, 'Feature/doc/evidence/' + keep
    before = facts(ROOT / original)
    if before != facts(ROOT / keep):
        raise RuntimeError('Duplicate proof failed: ' + original)
    rows.append({'action': 'duplicate', 'path': original, 'keep': keep, **before,
                 'reason': '과거 Blender 저장 백업; 별도 후보 원본과 바이트 동일. 실행 입력/직접 링크 없음.'})
for name in ['baseline.json', 'in-memory-poses.json', 'reload-poses.json']:
    relative = 'Feature/doc/evidence/fighter-arm-swing-20261005/' + name
    rows.append({'action': 'gzip', 'path': relative, **facts(ROOT / relative),
                 'reason': '과거 120Hz 원시 검증 자료를 무손실 보관. 기준선 reader는 gzip fallback과 덮어쓰기 가드 지원.'})
report = {'schema_version': 1,
          'baseline_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
          'files': rows,
          'preserved': ['Content/ and vendor assets', 'Source/ and Config/',
                        'all current Arin sources including unique .blend1 versions',
                        'FBX .fbm texture dependencies', 'failure logs and summary reports',
                        'registered Git worktrees, source captures and local tool runtimes']}
destination = HERE / 'selection.json'
with destination.open('x', encoding='utf-8') as stream:
    stream.write(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({'files': len(rows), 'original_bytes': sum(row['bytes'] for row in rows)}))
