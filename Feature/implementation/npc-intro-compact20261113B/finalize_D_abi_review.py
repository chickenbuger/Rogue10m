"""Record Root's actual offline D sample review, never startup permission/results."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import types

R = Path(__file__).resolve().parents[3]
E = R / 'Feature/doc/evidence/npc-intro-compact20261113B'
B = R / 'tmp/PackageAbiVerification20261113C'
H = B / 'verify_package_abi.py'
raw = H.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == '269A5ADA0B95750F985DEAE0210E1E7192ECBA8668922FFF95DF307CD687FB8F'
h = types.ModuleType('offline_review_pin'); h.__file__ = str(H)
exec(compile(raw, str(H), 'exec'), h.__dict__)


def pin(p):
    return h.file_pin(p, hashlib.sha256(p.read_bytes()).hexdigest())


abi = json.loads((B / 'D-actual-archive-01.json').read_text(encoding='utf-8'))
assert abi['status'] == 'ABI_48_PAIR_AND_PINNED_DEFINITIONS_VERIFIED'
collection = json.loads((B / 'D-disassembly-01/collection.json').read_text(encoding='utf-8'))
assert collection['abi_report']['sha256'] == '9E24D20E8551B00B40BF37D981C24422A3395A99E1805C1D3A6C7AD29B2459E8'
assert abi['producer']['size'] == 96 and abi['consumer_stride'] == 48
assert len(collection['producer_rows']) == 2
for row in collection['producer_rows']:
    assert row['info_symbol']['size'] == 32 and row['info_symbol']['displacement'] == 0
    assert row['info_section']['full_32_bytes_in_writable_section']
assert [row['version_info_size'] for row in collection['producer_rows']] == [48, 8]
assert [row['version_info_hash'] for row in collection['producer_rows']] == [3203274777, 1238498172]
owner = json.loads((E / 'internal-windows-D-owner-result.json').read_text(encoding='utf-8-sig'))
assert owner['exit_code'] == 0
area = Path(owner['output_root'])
post_path = area / 'evidence/postflight.json'
post = json.loads(post_path.read_text(encoding='utf-8-sig'))
assert post['game']['sha256'] == post['staged_game']['sha256'] == post['archive_game']['sha256'] == abi['exe']['sha256']
pdb_chain = [h.file_pin(p, abi['pdb']['sha256']) for p in [
    R / 'Binaries/Win64/Rogue10m.pdb',
    area / 'stage/Windows/Rogue10m/Binaries/Win64/Rogue10m.pdb',
    Path(abi['pdb']['path']),
]]
queries = []
for p in sorted((B / 'D-native-types-01').glob('*.process.json')):
    q = json.loads(p.read_text(encoding='utf-8'))
    assert q['exit_code'] == 0 and q['stderr']['size'] == 0
    assert q['pdb']['sha256'] == abi['pdb']['sha256']
    assert q['abi_report_sha256'] == collection['abi_report']['sha256']
    h.file_pin(q['stdout']['path'], q['stdout']['sha256'])
    queries.append(pin(p))
assert len(queries) == 14
report = {
    'status': 'ROOT_REVIEWED_ACTUAL_D_SAMPLED_ABI_CONSISTENT_NOT_STARTUP_OR_GLOBAL_CERTIFICATION',
    'recorded_utc': datetime.now(timezone.utc).isoformat(),
    'coverage': 'AttackSkillData two-row ScriptStructInfo and its two Info destinations only',
    'abi_report': collection['abi_report'], 'exe': abi['exe'], 'pdb_chain': pdb_chain,
    'PE_RSDS_and_PDB': abi['pdb_identity'],
    'row_bytes': 48, 'info_bytes': 32, 'version_info_bytes': 16,
    'field_offsets': {'row': [0, 8, 16, 24, 32], 'info': [0, 8, 16], 'version': [0, 8]},
    'manual_disassembly_review': {
        'initializer_R9_load_VA': '0x1413A9936', 'exact_producer_VA': '0x151173DA0',
        'count2_store_VA': '0x1413A9970', 'initializer_call_VA': '0x1413A9979',
        'consumer_VA': '0x1423265B0', 'loop_VA': '0x142326690',
        'registry_call_VA': '0x1423266E2', 'RSI_plus48_VA': '0x142326784',
        'count_decrement_VA': '0x142326788', 'backedge_VA': '0x14232678C',
        'finding': 'Same incoming R9 table moves to RSI; row+32 cursor loads register/name/info at -32/-16/-8; exact registry receives Info and VersionInfo at argument5/6; +48 and decremented count branch to same loop.',
        'independent_read_review': '/root/abi_final_read_review_d confirmed the same sample; no tools or raw hash recomputation by that reviewer.',
    },
    'native_query_process_receipts': queries,
    'disassembly_collection': pin(B / 'D-disassembly-01/collection.json'),
    'compile_link_provenance': pin(E / 'D-compile-provenance.json'),
    'completed_build_postflight': pin(post_path),
    'limits': ['Global/all generated TU ABI unverified.',
               'CRT pointer table proof incomplete: duplicate symbols and initial collector size cap.',
               'Registry primary unwind span does not cover its final write block.',
               'Compilation records do not certify every PCH/cache object.',
               'Actual package startup/UI/save routing not performed by this review.'],
    'startup': 'NOT_TESTED_BY_THIS_STEP', 'release_eligible': False,
    'UE_Game_executions_by_this_review': 0, 'denied_metadata_or_startup_permission_actions': 0,
}
with (E / 'D-abi-reviewed.json').open('x', encoding='utf-8', newline='\n') as stream:
    json.dump(report, stream, indent=2); stream.write('\n')
print(json.dumps({'status': report['status'], 'queries': len(queries), 'row_info_version': [48, 32, 16]}))
