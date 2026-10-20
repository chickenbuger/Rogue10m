"""Apply/verify/restore this reviewed manifest; never discover deletion candidates.

Run from the project root with --mode preview|apply|verify|restore.
Only individual regular files below evidence or tmp are eligible. The manifest
contains pre-optimization byte hashes; gzip archives are checked before unlink.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import stat

HERE = Path(__file__).absolute().parent
ROOT = HERE.parents[3]
MANIFEST = HERE / 'selection.json'


def safe(relative):
    parts = Path(relative).parts
    if (not parts or Path(relative).is_absolute() or '..' in parts
            or not (relative.startswith('Feature/doc/evidence/') or relative.startswith('tmp/'))):
        raise RuntimeError('Unapproved path: ' + relative)
    path = ROOT / relative
    path.resolve().relative_to(ROOT.resolve())
    for ancestor in [path, *path.parents]:
        if ancestor == ROOT:
            break
        if ancestor.exists() or ancestor.is_symlink():
            info = ancestor.lstat()
            if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
                raise RuntimeError('Reparse path refused: ' + str(ancestor))
    if path.exists() and not path.is_file():
        raise RuntimeError('Expected regular file: ' + relative)
    return path


def digest(stream):
    value = hashlib.sha256()
    size = 0
    for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
        value.update(block)
        size += len(block)
    return size, value.hexdigest()


def check(path, row, compressed=False):
    opener = gzip.open if compressed else open
    with opener(path, 'rb') as stream:
        actual = digest(stream)
    if actual != (row['bytes'], row['sha256']):
        raise RuntimeError('Byte/hash mismatch: ' + str(path))


def run(mode):
    if not (ROOT / 'Rogue10m.uproject').is_file():
        raise RuntimeError('Unexpected project root')
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    rows = manifest['files']
    paths = [row['path'].casefold() for row in rows]
    if len(paths) != len(set(paths)):
        raise RuntimeError('Duplicate manifest paths')
    # Validate every row before any mutation. A retained duplicate cannot be deleted.
    for row in rows:
        source = safe(row['path'])
        target = safe(row['keep'] if row['action'] == 'duplicate' else row['path'] + '.gz')
        if target.relative_to(ROOT).as_posix().casefold() in paths:
            raise RuntimeError('Retained file is also selected for deletion')
        if row['action'] == 'duplicate':
            check(target, row)
        elif row['action'] == 'gzip':
            if target.exists():
                check(target, row, compressed=True)
        else:
            raise RuntimeError('Unknown action')
        if source.exists():
            check(source, row)
        elif mode == 'preview' or (row['action'] == 'gzip' and not target.exists()):
            raise RuntimeError('Original missing: ' + row['path'])
        if mode == 'verify' and source.exists():
            raise RuntimeError('Selected original still exists: ' + row['path'])
    receipt = []
    for row in rows:
        source = safe(row['path'])
        target = safe(row['keep'] if row['action'] == 'duplicate' else row['path'] + '.gz')
        if mode == 'apply' and source.exists():
            if row['action'] == 'gzip' and not target.exists():
                # Exclusive create protects any independently created archive.
                try:
                    with target.open('xb') as raw:
                        with gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as archive:
                            with source.open('rb') as original:
                                for block in iter(lambda: original.read(4 * 1024 * 1024), b''):
                                    archive.write(block)
                except Exception:
                    # The source is still present if archive creation fails.
                    raise
            check(target, row, compressed=row['action'] == 'gzip')
            check(source, row)
            source.unlink()
        if mode == 'restore' and not source.exists():
            check(target, row, compressed=row['action'] == 'gzip')
            opener = gzip.open if row['action'] == 'gzip' else open
            with opener(target, 'rb') as original, source.open('xb') as restored:
                for block in iter(lambda: original.read(4 * 1024 * 1024), b''):
                    restored.write(block)
            check(source, row)
        retained_bytes, retained_sha256 = None, None
        if target.exists():
            with target.open('rb') as retained:
                retained_bytes, retained_sha256 = digest(retained)
        receipt.append({'path': row['path'], 'action': row['action'],
                        'original_bytes': row['bytes'], 'sha256': row['sha256'],
                        'retained_path': target.relative_to(ROOT).as_posix(),
                        'retained_bytes': retained_bytes, 'retained_sha256': retained_sha256,
                        'original_present': source.exists()})
    before = sum(row['bytes'] for row in rows)
    archive_bytes = sum(safe(row['path'] + '.gz').stat().st_size for row in rows
                        if row['action'] == 'gzip' and safe(row['path'] + '.gz').exists())
    removed = sum(row['original_bytes'] for row in receipt if not row['original_present'])
    report = {'mode': mode, 'files': receipt, 'original_bytes': before,
              'new_archive_bytes': archive_bytes, 'removed_bytes_net': removed - archive_bytes,
              'baseline_commit': manifest['baseline_commit'], 'result': 'PASS'}
    (HERE / (mode + '-receipt.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key != 'files'}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['preview', 'apply', 'verify', 'restore'], default='preview')
    run(parser.parse_args().mode)
