"""Read-only duplicate audit. This tool never deletes or moves project files.

Only untracked tmp files with a byte-identical retained copy are candidates.
Path references are literal-path checks, not proof that a file is semantically unused.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess

KEEP_ROOTS = ('Feature/doc/evidence', 'ui-concepts', 'Scripts')
TEXT_SUFFIXES = {'.h', '.hpp', '.c', '.cpp', '.cs', '.py', '.ps1', '.psm1', '.bat', '.cmd',
                 '.js', '.ts', '.json', '.ini', '.toml', '.yaml', '.yml', '.xml', '.txt', '.md',
                 '.uproject', '.uplugin', '.sh'}
SCRIPT_SUFFIXES = {'.py', '.ps1', '.psm1', '.bat', '.cmd', '.js', '.ts', '.sh'}
TOOL_SUFFIXES = {'.exe', '.dll', '.pyd', '.so', '.dylib', '.whl', '.msi'}


def reparse(path: Path) -> bool:
    info = path.lstat()
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, 'st_file_attributes', 0)
                                            & getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400))


def safe_files(base: Path, root: Path, issues: list[dict]):
    """Never descend symlinks or Windows junction/reparse directories."""
    if not base.exists():
        return
    try:
        for parent in [base, *base.parents]:
            if parent == root:
                break
            if reparse(parent):
                issues.append({'path': str(base.relative_to(root)), 'reason': 'reparse ancestor'})
                return
        base.resolve().relative_to(root)
    except (OSError, ValueError) as exc:
        issues.append({'path': str(base), 'reason': str(exc)})
        return
    for folder, directories, filenames in os.walk(base, followlinks=False):
        allowed = []
        for name in directories:
            child = Path(folder) / name
            try:
                if not reparse(child):
                    allowed.append(name)
            except OSError as exc:
                issues.append({'path': str(child), 'reason': str(exc)})
        directories[:] = sorted(allowed)
        for name in sorted(filenames):
            child = Path(folder) / name
            try:
                if not reparse(child) and child.is_file():
                    child.resolve().relative_to(root)
                    yield child
            except (OSError, ValueError) as exc:
                issues.append({'path': str(child), 'reason': str(exc)})


def preserve_reason(relative: str, tracked: set[str]) -> str | None:
    if relative.casefold() in tracked:
        return 'git tracked'
    parts = Path(relative).parts
    if any('backup' in p.casefold() or p.casefold().startswith(('before', 'baseline')) for p in parts[1:]):
        return 'backup or tool/runtime original'
    for part in parts[1:-1]:
        name = part.casefold()
        if ('backup' in name or name.startswith('before') or name.startswith('baseline')
                or name in {'decoder', 'tools', 'tool', 'runtime', 'runtimes', 'dependencies',
                            'node_modules', '.venv', 'venv', 'third_party', 'third-party'}
                or name.endswith('-runtime') or name.endswith('-decoder')):
            return 'backup or tool/runtime original'
    if Path(relative).suffix.casefold() in TOOL_SUFFIXES:
        return 'tool/runtime binary'
    return None


def normalized(value: str) -> str:
    return re.sub(r'/+', '/', value.replace('\\', '/')).casefold()


def run(root: Path, output: Path) -> dict:
    root = root.resolve(strict=True)
    if not (root / 'Rogue10m.uproject').is_file():
        raise RuntimeError('Expected Rogue10m project root')
    if output.exists() and reparse(output):
        raise RuntimeError('Manifest output cannot be a reparse file')
    output = output.absolute()
    output.relative_to(root)
    output.resolve().relative_to(root)
    for parent in [output.parent, *output.parent.parents]:
        if parent == root:
            break
        if parent.exists() and reparse(parent):
            raise RuntimeError('Manifest output cannot use a reparse path')
    git = subprocess.run(['git', '-C', str(root), 'ls-files', '-z', '--', 'tmp'],
                         check=True, capture_output=True)
    tracked = {normalized(p) for p in git.stdout.decode('utf-8', errors='surrogateescape').split('\0') if p}
    issues: list[dict] = []
    preserved = Counter()
    temporary = []
    for path in safe_files(root / 'tmp', root, issues):
        relative = path.relative_to(root).as_posix()
        reason = preserve_reason(relative, tracked)
        if reason:
            preserved[reason] += 1
        else:
            temporary.append(path)
    by_size = defaultdict(list)
    for path in temporary:
        try:
            by_size[path.stat().st_size].append(path)
        except OSError as exc:
            issues.append({'path': str(path), 'reason': str(exc)})
    retained = defaultdict(list)
    for folder in KEEP_ROOTS:
        for path in safe_files(root / folder, root, issues):
            if path.absolute() == output:
                continue
            try:
                size = path.stat().st_size
                if size in by_size:
                    retained[size].append(path)
            except OSError as exc:
                issues.append({'path': str(path), 'reason': str(exc)})
    hashed = {}

    def digest(path: Path):
        if path in hashed:
            return hashed[path]
        try:
            if reparse(path):
                raise RuntimeError('Path became a reparse point')
            before = path.stat()
            sha = hashlib.sha256()
            with path.open('rb') as stream:
                for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
                    sha.update(block)
            after = path.stat()
            if (before.st_size, before.st_mtime_ns, before.st_ino) != (after.st_size, after.st_mtime_ns, after.st_ino):
                raise RuntimeError('File changed during hashing')
            hashed[path] = sha.hexdigest()
        except (OSError, RuntimeError) as exc:
            issues.append({'path': str(path.relative_to(root)), 'reason': str(exc)})
            hashed[path] = None
        return hashed[path]

    duplicates = []
    for size in sorted(retained):
        keep_hashes = defaultdict(list)
        for keep in sorted(retained[size]):
            key = digest(keep)
            if key:
                keep_hashes[key].append(keep)
        for original in sorted(by_size[size]):
            key = digest(original)
            if not key or key not in keep_hashes:
                continue
            keep = keep_hashes[key][0]
            duplicates.append({'original': original.relative_to(root).as_posix(),
                               'keep': keep.relative_to(root).as_posix(), 'sha256': key,
                               'bytes': size, 'references': []})
    tokens = defaultdict(set)
    for index, entry in enumerate(duplicates):
        for spelling in (entry['original'], str(root / entry['original'])):
            tokens[normalized(spelling)].add(index)
    pattern = re.compile('|'.join(re.escape(value) for value in sorted(tokens, key=len, reverse=True))) if tokens else None
    references = set()
    for folder in ('Source', 'Scripts', 'Config'):
        references.update(p for p in safe_files(root / folder, root, issues) if p.suffix.casefold() in TEXT_SUFFIXES)
    references.update(p for p in safe_files(root / 'tmp', root, issues) if p.suffix.casefold() in SCRIPT_SUFFIXES)
    references.update(root.glob('*.uproject'))
    scanned = 0
    if pattern:
        for path in sorted(references):
            if reparse(path):
                continue
            try:
                raw = path.read_bytes()
                if raw.startswith((b'\xff\xfe', b'\xfe\xff')):
                    content = raw.decode('utf-16')
                else:
                    content = raw.decode('utf-8-sig', errors='replace')
                content = normalized(content)
                scanned += 1
                matching = set()
                for match in pattern.finditer(content):
                    matching.update(tokens[match.group(0)])
                for index in matching:
                    duplicates[index]['references'].append(path.relative_to(root).as_posix())
            except OSError as exc:
                issues.append({'path': str(path.relative_to(root)), 'reason': str(exc)})
    candidates = [entry for entry in duplicates if not entry['references']]
    referenced = [entry for entry in duplicates if entry['references']]
    report = {'schema_version': 1, 'created_utc': datetime.now(timezone.utc).isoformat(),
              'project_root': str(root), 'mode': 'audit_only_no_deletions', 'candidate_root': 'tmp',
              'retained_roots': list(KEEP_ROOTS), 'reference_scope': ['Source', 'Scripts', 'Config', '*.uproject', 'tmp scripts'],
              'limitations': ['Literal normalized absolute/relative path references only; dynamically constructed paths are not resolved.',
                             'Candidate status is not deletion authorization; revalidate paths and both hashes before deleting.',
                             'SourceArt, Content, user design documents and nonidentical files are never candidates.'],
              'summary': {'eligible_tmp_files': len(temporary), 'hashed_files': len(hashed), 'reference_files_scanned': scanned,
                          'preserved': dict(preserved), 'candidates': len(candidates), 'candidate_bytes': sum(e['bytes'] for e in candidates),
                          'referenced_duplicates': len(referenced), 'issues': len(issues)},
              'candidates': candidates, 'referenced_duplicates': referenced, 'issues': issues}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    output = args.output or args.root / 'Feature/doc/evidence/unused-file-cleanup-20260928/candidates.json'
    report = run(args.root, output)
    print(json.dumps(report['summary'], ensure_ascii=False))
    print('Manifest: ' + str(output))


if __name__ == '__main__':
    main()
