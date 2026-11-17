"""Verify preservation and copy the new archive to an independent local delivery."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import types

R = Path(__file__).resolve().parents[3]
E = R / 'Feature/doc/evidence/hub-entry-windows20261114A'
T = R / 'Feature/implementation/native51-internal-smoke-cook-fix-f'
raw = (T / 'strict_primitives.py').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == '0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c = types.ModuleType('hub_delivery'); c.__file__ = str(T / 'strict_primitives.py')
exec(compile(raw, c.__file__, 'exec'), c.__dict__)

def write(path, value):
    with path.open('x', encoding='utf-8') as f: json.dump(value, f, ensure_ascii=False, indent=2)
    return c.file_pin(path)

assert not (E / 'final-result.json').exists()
initial = c.load(c.stable_bytes(E / 'content-checkpoint.json'))
owner = c.load(c.stable_bytes(E / 'owner-result.json'))
assert owner['exit_code'] == 0 and owner['execute_requested']
s = c.read_json_pin(owner['seal'])
for pin in c.read_json_pin(s['content_freeze'])['evidence']: c.read_pin(pin)
compile_proof = c.load(c.stable_bytes(E / 'compile-provenance.json'))
assert compile_proof['status'] == 'ACTUAL_NEW_COMPILE_LINK_CHAIN_PINNED_REQUIRES_FINAL_BINARY_REVIEW_NO_RUNTIME'
assert compile_proof['generated_files_modified'] == 0
response_proof = c.load(c.stable_bytes(E / 'compile-response-supplement.json'))
assert response_proof['status'] == 'ACTUAL_NESTED_RESPONSE_CHAIN_PINNED_NO_RELOAD_MACRO_OVERRIDES_IN_RECORDED_RSP_FILES'
for pin in response_proof['response_files']: c.read_pin(pin)
post = c.load(c.stable_bytes(Path(s['outputs']['evidence']) / 'postflight.json'))
assert post['status'] == 'internal_smoke_artifact_built_UNVERIFIED'
io = c.load(c.stable_bytes(E / 'iostore-hub-result.json'))
assert io['status'] == 'OFFICIAL_IOSTORE_MAP_DA_AND_HUB_ACTOR_IDENTITIES_CONFIRMED_PROPERTIES_NOT_DECODED'
assert len(io['new_actor_exports']) == 6 and len(io['prior_portal_exports']) == 3
binary = c.load(c.stable_bytes(E / 'binary-chain.json'))
if binary['matches_D_exe_and_pdb']:
    abi = {'basis': 'Whole EXE and PDB byte identity to actual D sampled ABI review', 'review': binary['D_review'], 'row': 48, 'info': 32, 'version_info': 16, 'consumer': 48}
else:
    new_abi = c.load(c.stable_bytes(E / 'new-binary-abi-review.json'))
    assert new_abi['status'] == 'ROOT_REVIEWED_NEW_SAMPLED_ABI_CONSISTENT_NOT_STARTUP_OR_GLOBAL_CERTIFICATION'
    assert new_abi['exe']['sha256'] == binary['chain']['Rogue10m.exe'][0]['sha256']
    assert new_abi['pdb']['sha256'] == binary['chain']['Rogue10m.pdb'][0]['sha256']
    assert [new_abi[k] for k in ('row', 'info', 'version_info', 'consumer')] == [48,32,16,48]
    abi = {'basis': 'Fresh actual EXE/PDB sampled review', 'review': c.file_pin(E / 'new-binary-abi-review.json'), 'row': 48, 'info': 32, 'version_info': 16, 'consumer': 48}
after = c.input_inventory(R)
c.compare_inventory(c.read_json_pin(s['input_manifest']), after, 'all inputs after new artifact')
assert len(after['files']) == 10472
write(E / 'inputs-after-package.json', after)
for pin in s['editor_products']: c.read_pin(pin)
for pin in initial['prior_records']: c.read_pin(pin)
for row in initial['C_original_copy_pairs']:
    for key in ('source', 'copy'): assert c.file_pin(row[key])['sha256'] == row['sha256']
saves = []
for entry in s['protected_saves']:
    actual = c.scan_tree(entry['root'], True)
    c.compare_inventory(c.read_json_pin(entry['manifest']), actual, 'protected saves after new artifact')
    saves.append({'root': entry['root'], 'files': len(actual['files']), 'unchanged': True})
for artifact in initial['prior_artifacts']:
    c.read_pin(artifact['receipt'])
    for folder in artifact['copies']:
        assert c.digest(c.canonical(c.payload_manifest(folder['root']))) == folder['payload_sha256']
print('All10472 inputs, Editor6, saves and B/E5/D both copies preserved.', flush=True)
source = c.physical(Path(s['outputs']['archive']) / 'Windows', True)
payload = c.payload_manifest(source)
assert payload == post['payload'] and c.digest(c.canonical(payload)) == post['payload_sha256']
target = c.physical(R / 'tmp/InternalWindows20261114HubEntryA_UNVERIFIED')
assert c.child(target, R / 'tmp') and not target.exists()
for artifact in initial['prior_artifacts']:
    for folder in artifact['copies']:
        old = Path(folder['root'])
        assert old != target and not c.child(target, old) and not c.child(old, target)
windows = target / 'Windows'; windows.mkdir(parents=True)
for row in payload:
    old = c.physical(source / row['path'], True); new = c.physical(windows / row['path'])
    assert c.child(old, source) and c.child(new, windows) and not new.exists()
    new.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(old, new)
    assert c.file_pin(new)['sha256'] == row['sha256']
assert c.payload_manifest(windows) == payload
delivery = {
    'status': 'HUB_ENTRY_WINDOWS_INTERNAL_DEVELOPMENT_BUILT_NOT_EXECUTED_UNVERIFIED', 'created_utc': datetime.now(timezone.utc).isoformat(),
    'source_archive': str(source), 'archive': str(windows), 'files': len(payload), 'payload_bytes': sum(v['size'] for v in payload),
    'payload_sha256': post['payload_sha256'], 'payload': payload, 'content_state': s['content_state'],
    'seal': owner['seal'], 'postflight': c.file_pin(Path(s['outputs']['evidence']) / 'postflight.json'),
    'iostore_hub': c.file_pin(E / 'iostore-hub-result.json'), 'binary_chain': c.file_pin(E / 'binary-chain.json'), 'compile_provenance': c.file_pin(E / 'compile-provenance.json'), 'compile_response_supplement': c.file_pin(E / 'compile-response-supplement.json'), 'sampled_ABI': abi,
    'actual_new_game_executions': 0, 'GUI_or_game_input': 0, 'UI7': 'UNVERIFIED', 'visual_review': 'UNVERIFIED', 'release_eligible': False,
    'runtime_travel_RunId_timer_NPC_F_profile_saveIO_quit_restart': 'UNVERIFIED', 'landscape_extent_m': [1800,1800], 'landscape_cropped': False,
    'old_B_C_D_E5_or_strict_B_B2_modified': False, 'denied_startup_metadata_clone_QA_operations': 0,
}
delivery_pin = write(target / 'DELIVERY.json', delivery)
(target / 'README.ko.md').write_text('# Hub 후보 입구 포함 내부 Windows 빌드\n\n정규 Win64 Development Build/Cook/Stage/Archive를 수행한 내부 미검수 아티팩트다. 이번 패키지는 실행하지 않았다.\n\n새 Source Hub 입구1과 표시Actor5, 기존포털3, Compact 목적지와 초급 의뢰DA를 포함하는 입력으로 cook했다. 공식 IoStore 목록과 Describe로 맵6+DA, 신규Actor6/기존포털3의 cooked export identity를 확인했다. target/timer/장식충돌/텍스트의 typed값은 Source cold reload와 동결된 입력 계보 근거이며 cooked FProperty를 직접 해석한 결과가 아니다.\n\n맵은1800×1800m이며 미축소다. 실제 여행·RunId/20분시계·프로필·NPC/F·보상·saveIO·종료/재시작·연출·물리경로/15분성장/완주·UI7 검수는 남았다. 기존B/C/D/E5는 보존했고 이전D의 시작메뉴 관측을 이번 버전의 실행 성공으로 세지 않는다. 릴리스 완료품이 아니다. 자세한 해시와 근거는 DELIVERY.json을 따른다.\n', encoding='utf-8')
final = {k:v for k,v in delivery.items() if k != 'payload'}
final.update(delivery_manifest=delivery_pin, all_input_files_unchanged=10472, source_config_files_unchanged=368, editor_products_unchanged=6, protected_saves=saves, preserved_artifacts=initial['prior_artifacts'], prior_record_pins_verified=len(initial['prior_records']), C_pairs_preserved=len(initial['C_original_copy_pairs']), new_content_files_written_by_this_packet=0, commit_push=0)
write(E / 'final-result.json', final)
print(json.dumps({k: final[k] for k in ('status', 'archive', 'files', 'payload_bytes', 'payload_sha256', 'all_input_files_unchanged', 'actual_new_game_executions')}, indent=2), flush=True)
