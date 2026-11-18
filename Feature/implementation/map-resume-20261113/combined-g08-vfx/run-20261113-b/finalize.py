"""Build a no-commit, physical-copy combined handoff; never mutate the project baseline."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
from datetime import datetime, timezone

RUN = Path(__file__).resolve().parent
PROJECT = Path('D:/Project/Rogue10m')
assert RUN.is_relative_to(PROJECT / 'Feature/implementation/map-resume-20261113/combined-g08-vfx')
BASELINE = RUN / 'Baseline'
CANDIDATE = RUN / 'G08ThenVFX'
DELIVERY = RUN / 'Delivery'
REPLAY = RUN / 'CombinedReplay'
assert not DELIVERY.exists() and not REPLAY.exists(), 'Preserve existing outputs; use a new label.'
DELIVERY.mkdir()
REPLAY.mkdir()
G08_ROOT = PROJECT / 'tmp/SkillTiming20261113'
VFX_ROOT = Path('C:/Users/PC/Documents/Codex/2026-11-11/task-2/vfx-refresh-20261113-delivery-v2')
G08_PATCH = G08_ROOT / 'Delivery/g08-full-source-20261113.patch'
VFX_PATCH = VFX_ROOT / 'vfx-full-20261113.patch'
RECORDS = json.loads((RUN / 'baseline-manifest.json').read_text(encoding='utf-8-sig'))
COMMANDS = []

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None

def norm(path):
    return path.read_bytes().replace(b'\r\n', b'\n')

def write_json(name, data):
    (DELIVERY / name).write_text(json.dumps(data, ensure_ascii=True, indent=2) + '\n', encoding='utf-8')

def invoke(label, args, cwd, expect=0):
    result = subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (DELIVERY / (label + '.stdout.log')).write_bytes(result.stdout)
    (DELIVERY / (label + '.stderr.log')).write_bytes(result.stderr)
    COMMANDS.append({'label': label, 'args': list(map(str, args)), 'cwd': str(cwd), 'exit_code': result.returncode, 'expected_exit_code': expect})
    write_json('commands.json', COMMANDS)
    assert result.returncode == expect, (label, result.returncode, result.stderr.decode(errors='replace'))
    return result

new_paths = [row['path'] for row in RECORDS if row['baseline_sha256'] is None]
invoke('08-intent-to-add-new-files', ['git.exe', 'add', '--intent-to-add', '--', *new_paths], CANDIDATE)
invoke('09-combined-diff-check', ['git.exe', 'diff', '--check'], CANDIDATE)
payload = invoke('10-combined-diff', ['git.exe', 'diff', '--binary', '--no-ext-diff', '--no-color'], CANDIDATE).stdout
patch = DELIVERY / 'combined-g08-vfx-full-20261113.patch'
patch.write_bytes(payload)
assert payload.count(b'diff --git ') == 29
invoke('11-original-g08-reverse-check', ['git.exe', 'apply', '--reverse', '--check', str(G08_PATCH)], CANDIDATE)
invoke('12-original-vfx-reverse-check', ['git.exe', 'apply', '--reverse', '--check', str(VFX_PATCH)], CANDIDATE)

entries = []
for row in RECORDS:
    path = row['path']
    assert sha(PROJECT / path) == row['baseline_sha256'], ('Shared preimage drift', path)
    assert sha(BASELINE / path) == row['baseline_sha256'], ('Copied baseline drift', path)
    post = sha(CANDIDATE / path)
    assert post is not None
    entry = {'path': path, 'base_sha256': row['baseline_sha256'], 'candidate_sha256': post,
             'original_g08_candidate_sha256': row['g08_candidate_sha256'],
             'original_vfx_candidate_sha256': row['vfx_candidate_sha256'],
             'common_file': row['common'], 'preserved_helper': row['preserved_helper']}
    if row['g08_candidate_sha256']:
        entry['g08_raw_pin_matches'] = post == row['g08_candidate_sha256']
        if not row['common']:
            entry['g08_normalized_content_matches'] = norm(CANDIDATE / path) == norm(G08_ROOT / path)
            assert entry['g08_normalized_content_matches'], path
            entry['g08_raw_difference_reason'] = None if entry['g08_raw_pin_matches'] else 'LF-to-CRLF working-tree conversion under core.autocrlf=true; normalized bytes identical'
    if row['vfx_candidate_sha256']:
        entry['vfx_raw_pin_matches'] = post == row['vfx_candidate_sha256']
        assert row['common'] or entry['vfx_raw_pin_matches'], path
    if row['preserved_helper']:
        assert post == row['baseline_sha256'], path
    entries.append(entry)
manifest = {'schema_version': 1, 'packet': 'combined-g08-vfx-full-20261113',
            'baseline': 'exact current shared raw hashes copied physically; no original project writes',
            'entries': entries}
write_json('combined-manifest.json', manifest)

for source in BASELINE.rglob('*'):
    assert not source.is_symlink()
    if source.is_file():
        target = REPLAY / source.relative_to(BASELINE)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
invoke('13-replay-init', ['git.exe', 'init', '--initial-branch=Sprint#9-33-combined-g08-vfx-replay'], REPLAY)
invoke('14-replay-autocrlf', ['git.exe', 'config', '--local', 'core.autocrlf', 'true'], REPLAY)
invoke('15-replay-baseline-index', ['git.exe', 'add', '--all'], REPLAY)
invoke('16-combined-fresh-apply-check', ['git.exe', 'apply', '--check', str(patch)], REPLAY)
invoke('17-combined-fresh-apply', ['git.exe', 'apply', str(patch)], REPLAY)
for entry in entries:
    assert sha(REPLAY / entry['path']) == entry['candidate_sha256'], ('Fresh replay mismatch', entry['path'])
invoke('18-combined-reverse-check', ['git.exe', 'apply', '--reverse', '--check', str(patch)], REPLAY)

readiness = CANDIDATE / 'Scripts/VfxPacketReadiness20261113.py'
invoke('19-original-vfx-manifest-against-combined', [sys.executable, '-B', str(readiness), '--source-root', str(CANDIDATE), '--manifest', str(RUN / 'original-vfx-full-manifest.json'), '--report', str(DELIVERY / 'original-vfx-against-combined.json')], RUN, expect=2)
invoke('20-combined-manifest-against-baseline', [sys.executable, '-B', str(readiness), '--source-root', str(BASELINE), '--manifest', str(DELIVERY / 'combined-manifest.json'), '--report', str(DELIVERY / 'combined-against-baseline.json')], RUN)
invoke('21-combined-manifest-against-combined', [sys.executable, '-B', str(readiness), '--source-root', str(CANDIDATE), '--manifest', str(DELIVERY / 'combined-manifest.json'), '--report', str(DELIVERY / 'combined-against-combined.json')], RUN)

for name, original in [('g08-full-source-20261113.patch', G08_PATCH), ('vfx-full-20261113.patch', VFX_PATCH)]:
    shutil.copyfile(original, DELIVERY / name)
    assert sha(original) == sha(DELIVERY / name)

metadata = {'created_utc': datetime.now(timezone.utc).isoformat(), 'run': str(RUN),
            'combined_patch': patch.name, 'combined_patch_sha256': sha(patch), 'combined_patch_bytes': patch.stat().st_size,
            'g08_patch_sha256': sha(G08_PATCH), 'vfx_patch_sha256': sha(VFX_PATCH),
            'source_order': 'G08 full8 then VFX full23', 'changed_files': 29, 'manifest_entries': 33,
            'new_files': 24, 'existing_changed_files': 5, 'preserved_helpers': 4,
            'manual_conflict_resolution': False, 'reverse_order_needed': False,
            'fresh_combined_apply_and_raw_sha_replay': True,
            'original_g08_and_vfx_reverse_checks': True,
            'original_vfx_pins_modified': False, 'readiness_checker_modified': False,
            'original_vfx_manifest_against_combined': {'expected_exit': 2, 'status': 'blocked', 'already_integrated': 25, 'conflict': 2},
            'combined_manifest_against_baseline_expected': {'ready_add': 24, 'ready_update': 5, 'already_integrated': 4},
            'shared_preimages_rechecked_unchanged': True,
            'shared_source_config_content_git_changes': 0, 'ue_editor_ubt_cook_game_calls': 0,
            'commits_pushes': 0, 'junctions_symlinks': 0,
            'g08_new_files_line_endings': 'Four original LF candidates are CRLF after normal Windows Git application; normalized content verified identical; current raw pins are recorded, no check relaxed.',
            'engine_compiled_or_executed': False, 'visual_or_runtime_validation': False,
            'incomplete_previous_run': 'run-20261113-a retained; preparation helper name recursively shadowed git, interrupted before local Git init or patch application; fixed script uses git.exe in this new run.'}
write_json('combined-metadata.json', metadata)
print(json.dumps(metadata, indent=2))
