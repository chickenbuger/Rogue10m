"""Record the run04 frozen-seal failure without granting a packaging exception."""
import datetime,hashlib,json,types
from pathlib import Path
R=Path(__file__).resolve().parents[3]
P=R/'Feature/implementation/map-resume-20261113/internal-smoke-culture-utc-d'
A=Path('C:/Users/PC/AppData/Local/Temp/R10InternalSmoke20261113D')
raw=(P/'strict_primitives.py').read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='02DF5B26A566EBF528280B3DEAB1E97081E9CD7B3AAE6B47A8277C8D6A029980'
c=types.ModuleType('run04_diagnostic');c.__file__=str(P/'strict_primitives.py');exec(compile(raw,c.__file__,'exec'),c.__dict__)
s=c.read_json_pin({'path':str(A/'smoke-d4.sealed.json'),'sha256':'1634A6756D4BE05EFC397441F93695CE192B3D9343DDAB94C60BCC215FE85BF4'})
x=c.read_json_pin(s['input_manifest']);y=c.input_inventory(R)
xm={p['path']:p for p in x['files']};ym={p['path']:p for p in y['files']}
added=sorted(ym.keys()-xm.keys());missing=sorted(xm.keys()-ym.keys());changed=[{'before':xm[k],'after':ym[k]} for k in sorted(xm.keys()&ym.keys()) if xm[k]!=ym[k]]
assert not added and not missing and x['directories']==y['directories']
assert len(changed)==1 and changed[0]['after']['path']=='Build/Windows/FileOpenOrder/EditorOpenOrder.log',changed
assert changed[0]['after']['sha256']=='3CF6F190C933E635EB4177DF9BAA298C431BA2F1242E1DDE2EDFD81154CCC5D1'
for pin in s['editor_products']:c.read_pin(pin)
for q in s['protected_saves']:c.compare_inventory(c.read_json_pin(q['manifest']),c.scan_tree(q['root'],True),'run04 protected SaveGames')
log=c.stable_bytes(Path(s['outputs']['evidence'])/'buildcookrun.log').decode('utf-8-sig')
provenance=[line for line in log.splitlines() if 'Initialized file open order log' in line or 'COOK COMMAND COMPLETED' in line or 'Expected to find cooked directory' in line]
assert any('COOK COMMAND COMPLETED' in row for row in provenance)
e=R/'Feature/doc/evidence/map-resume-20261113/InternalSmokeD'
for name,value in [('run04-actual-input-manifest.json',y),('run04-input-diagnostic.json',{'status':'OLD_SEAL_FAILED_GENERATED_BUILD_LOG_CHANGED_PROTECTED_AREAS_UNCHANGED','time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'full_input_manifest_unchanged':False,'added':added,'missing':missing,'changed':changed,'source_config_content_scripts_uproject_unchanged':True,'input_files':len(ym),'editor6_unchanged':True,'protected_save_roots_unchanged':len(s['protected_saves']),'uat_exit':1,'cook_completed':True,'staging_completed':False,'release_eligible':False,'no_paths_ignored_or_exception_granted':True,'log_provenance':provenance,'followup':'Preserve current generated Build log. Use official FOOBASEDIR override and a new complete capture/seal. Never reuse old seal or claim all old inputs unchanged.','gui_operations':0})]:
 with (e/name).open('x',encoding='utf-8',newline='\n') as f:json.dump(value,f,indent=2)
b=c.stable_bytes(R/'Build/Windows/FileOpenOrder/EditorOpenOrder.log')
with (e/'run04-generated-EditorOpenOrder.log').open('xb') as f:f.write(b)
print(json.dumps({'changed':changed,'input_files':len(ym),'protected_areas_editor6_save_roots_unchanged':True,'old_seal_valid':False,'release_eligible':False},indent=2))
