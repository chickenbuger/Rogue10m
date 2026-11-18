"""Verify exact NPC delta, protected saves and unchanged pre-fix WindowsE5; no UE execution."""
from pathlib import Path
import hashlib,json,types,datetime,csv
R=Path(__file__).resolve().parents[3];E=R/'Feature/doc/evidence/npc-combat-credit-20261113A';P=R/'Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e'
b=(P/'strict_primitives.py').read_bytes();assert hashlib.sha256(b).hexdigest().upper()=='D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF'
c=types.ModuleType('npc_preservation');c.__file__=str(P/'strict_primitives.py');exec(compile(b,c.__file__,'exec'),c.__dict__)
seal=c.read_json_pin({'path':'C:/Users/PC/AppData/Local/Temp/R10InternalSmoke20261113D/smoke-e5.sealed.json','sha256':'536066AEE494178F4814D629F0C695BBD2D38B99B8E1F562B869D046301308B7'})
old=c.read_json_pin(seal['input_manifest']);now=c.input_inventory(R);x={p['path']:p for p in old['files']};y={p['path']:p for p in now['files']}
rows=list(csv.DictReader((E/'packet/combat-credit-file-hashes.tsv').read_text(encoding='utf-8-sig').splitlines(),delimiter='\t'))
expected_new={q['path'] for q in rows if not q['main_before_raw_sha256']};expected_changed={q['path'] for q in rows if q['main_before_raw_sha256']}
added=set(y)-set(x);missing=set(x)-set(y);changed={k for k in x.keys()&y.keys() if x[k]!=y[k]}
assert added==expected_new and not missing and changed==expected_changed,(added,missing,changed)
assert all(x[k]==y[k] for k in x if k not in expected_changed)
for row in rows:
 if row['kind']=='runtime' or row['path'].startswith('Scripts/'):
  raw=(R/row['path']).read_bytes().replace(b'\r\n',b'\n');assert hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()==row['post_normalized_blob']
assert c.file_pin(R/'Source/Rogue10m/Tests/Rogue10mVillageQuestCombatCreditTests.cpp')['sha256']=='73A8E6C4A9FC15BE92023E9DE5FF87DB1A673E528AAEC507BAEDAE0BBC8EAB98'
for item in seal['protected_saves']:c.compare_inventory(c.read_json_pin(item['manifest']),c.scan_tree(item['root'],True),'NPC regression protected saves')
top=R/'tmp/InternalWindows20261113E5_UNVERIFIED';assert c.file_pin(top/'DELIVERY.json')['sha256']=='C63906FC8BDD18B5ABCBE2DA0B1625EA271F0C89473634D508DB46C163902CB2'
delivery=c.load(c.stable_bytes(top/'DELIVERY.json'));assert c.payload_manifest(top/'Windows')==delivery['payload']
assert c.file_pin(R/'Binaries/Win64/Rogue10m.exe')['sha256']=='597E833F08AC7EF6D4124BE87A10768A2B3B62C699E9E9E2172AA94E44024FA6'
blocked=Path('C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/root-startup-approval-A.json');assert not blocked.exists()
v={'status':'NPC_DELTA_ONLY_ALL_OTHER_INPUTS_SAVES_OLD_WINDOWS_PRESERVED','verified_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'old_input_count':len(x),'current_input_count':len(y),'changed_existing':sorted(changed),'added_new':sorted(added),'other_inputs_unchanged':True,'content_files_unchanged':sum(p.startswith('Content/') for p in x),'protected_save_roots_unchanged':len(seal['protected_saves']),'old_Windows_E5_payload_unchanged':True,'old_Windows_E5_contains_this_NPC_fix':False,'old_Game_exe_unchanged':True,'blocked_metadata_approval_file_still_absent':True,'gui_input_operations':0,'metadata_qa_retried':False}
for name,value in [('post-native-inputs.json',now),('preservation-result.json',v)]:
 with (E/name).open('x',encoding='utf-8',newline='\n') as f:json.dump(value,f,indent=2)
print(json.dumps(v,indent=2))
