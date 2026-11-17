"""Record Root's bounded review of the actual new native evidence, not approval."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import types

R=Path(__file__).resolve().parents[3]
E=R/'Feature/doc/evidence/hub-entry-windows20261114A'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('actual_new_review');c.__file__=str(P);exec(compile(raw,str(P),'exec'),c.__dict__)
target=E/'new-binary-abi-review.json';assert not target.exists()
collection=c.load(c.stable_bytes(E/'new-binary-abi-collection.json'))
assert collection['status']=='NEW_NATIVE_TYPE_LINKS_AND_BOUNDED_DISASSEMBLY_COLLECTED_MANUAL_REVIEW_REQUIRED'
for key in ('exe','pdb','definitions','llvm'):c.read_pin(collection[key])
abi=c.read_json_pin(collection['abi_report']);bounds=c.read_json_pin(collection['disassembly_collection'])
assert abi['status']=='ABI_48_PAIR_AND_PINNED_DEFINITIONS_VERIFIED'
assert abi['exe']['sha256']==collection['exe']['sha256'] and abi['pdb']['sha256']==collection['pdb']['sha256']
assert bounds['exe']['sha256']==collection['exe']['sha256'] and bounds['pdb']['sha256']==collection['pdb']['sha256']
assert abi['producer']['size']==96 and abi['producer']['stride']==48 and abi['consumer_stride']==48
assert [row['name'] for row in bounds['producer_rows']]==['Rogue10mSkillUnlockCondition','Rogue10mAttackResourceCost']
assert [row['version_info_size'] for row in bounds['producer_rows']]==[48,8]
for row in bounds['producer_rows']:
    assert row['info_symbol']['size']==32 and row['info_symbol']['displacement']==0
    assert row['info_section']['writable'] and row['info_section']['full_32_bytes_in_writable_section']
    assert row['version_info_padding_hex']=='00000000'
disassembly={record['label']:c.read_pin(record['stdout']).decode('utf-8-sig') for record in bounds['disassembly']}
initial=disassembly['initializer-0'];consumer=disassembly['consumer'];registry=disassembly['registry']
assert 'lea         r9,[0000000151173DA0h]' in initial
assert 'mov         qword ptr [rsp+20h],2' in initial and 'call        00000001423265B0' in initial
assert abi['producer']['rva']==0x11173DA0
assert 'mov         rsi,r9' in consumer and 'add         rsi,20h' in consumer
for line in ('mov         rdi,qword ptr [rsi-20h]','mov         r15,qword ptr [rsi-8]','mov         rbx,qword ptr [rsi-10h]','mov         qword ptr [rsp+28h],rsi','mov         qword ptr [rsp+20h],r15','call        000000014230CE60','add         rsi,30h','sub         r14,1','jne         0000000142326690'):
    assert line in consumer,line
assert bounds['disassembly'][1]['symbol']['name']=='TDeferredRegistry<TRegistrationInfo<UScriptStruct,FStructReloadVersionInfo> >::AddRegistration'
assert registry.strip()
for pin in collection['query_receipts']:
    receipt=c.read_json_pin(pin);assert receipt['exit_code']==0
    c.read_pin(receipt['stdout']);assert not c.read_pin(receipt['stderr']).strip()
compile_proof=c.load(c.stable_bytes(E/'compile-provenance.json'))
assert compile_proof['status']=='ACTUAL_NEW_COMPILE_LINK_CHAIN_PINNED_REQUIRES_FINAL_BINARY_REVIEW_NO_RUNTIME'
for pin in compile_proof['records'].values():c.read_pin(pin)
assert '#define WITH_LIVE_CODING 1' in compile_proof['observed_macro_directives']['shared_definitions']
result={
    'status':'ROOT_REVIEWED_NEW_SAMPLED_ABI_CONSISTENT_NOT_STARTUP_OR_GLOBAL_CERTIFICATION',
    'recorded_utc':datetime.now(timezone.utc).isoformat(),'exe':collection['exe'],'pdb':collection['pdb'],
    'PE_RSDS_and_PDB':bounds['pe_rsds'],'row':48,'info':32,'version_info':16,'consumer':48,
    'field_offsets':{'row':[0,8,16,24,32],'info':[0,8,16],'version':[0,8]},
    'reviewed_flow':{'initializer_R9_load_VA':'0x1413A9936','producer_VA':'0x151173DA0','count2_store_VA':'0x1413A9970','initializer_call_VA':'0x1413A9979','consumer_VA':'0x1423265B0','R9_to_RSI_VA':'0x1423265C9','loop_VA':'0x142326690','registry_call_VA':'0x1423266E2','plus48_VA':'0x142326784','decrement_VA':'0x142326788','backedge_VA':'0x14232678C','finding':'Exact incoming R9 table and count2; RSI row+32 cursor passes Info and full VersionInfo to typed UScriptStruct registry and advances48 with decremented count/backedge.'},
    'coverage':'AttackSkillData two-row ScriptStructInfo and two writable32B Info destinations only. Fresh LLVM global/type/field links and actual new bounded PE/PDB instructions were read by Root.',
    'evidence':{'collection':c.file_pin(E/'new-binary-abi-collection.json'),'bounded':collection['disassembly_collection'],'actual_offline_abi':collection['abi_report'],'compile_provenance':c.file_pin(E/'compile-provenance.json')},
    'initial_parser_failure_preserved':c.file_pin(E/'abi-parser-failure-A.json'),
    'limitations':['Not every generated record or PCH/CRT ABI certified','CRT initializer table remains UNVERIFIED; registry dump is bounded primary unwind region','No new Game startup/UI/profile/save routing/travel verification'],
    'Game_or_UE_executions_by_this_review':0,'startup_approval_or_metadata_clone_QA_actions':0,
}
with target.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
print(json.dumps({k:result[k] for k in ('status','row','info','version_info','consumer')},indent=2))
