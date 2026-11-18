"""Check additive saved packages without editing or running Unreal."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib,json
R=Path(__file__).resolve().parents[3]; E=R/'Feature/doc/evidence/npc-intro-compact20261113B'
old=json.loads((E/'physical-checkpoint.json').read_text(encoding='utf-8'))
auth=json.loads((E/'authoring-result.json').read_text(encoding='utf-8'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
for item in old['files']: assert sha(R/item['path'])==item['sha256'],item['path']
native=json.loads((R/'Feature/doc/evidence/npc-combat-credit-20261113A/Native51B/result.json').read_text(encoding='utf-8'))
for path,digest in native['source_config_sha256'].items():assert sha(R/path)==digest,path
assert sha(R/'Binaries/Win64/UnrealEditor-Rogue10m.dll')==native['dll_sha256']
added=['Content/'+p.removeprefix('/Game/')+'.uasset' for p in auth['saved_packages']]
added.append('Content/'+auth['data_asset'].split('.')[0].removeprefix('/Game/')+'.uasset')
assert len(added)==5 and len(set(added))==5
for path in added:assert (R/path).is_file(),path
out={'status':'EXISTING_CANDIDATE1419_AND_SOURCE368_UNCHANGED_FIVE_NEW_CONTENT_FILES',
 'verified_utc':datetime.now(timezone.utc).isoformat(),'existing_candidate_files_unchanged':len(old['files']),
 'source_config_files_unchanged':len(native['source_config_sha256']),
 'editor_DLL_unchanged':True,'added_files':[{'path':p,'sha256':sha(R/p),'bytes':(R/p).stat().st_size} for p in added],
 'landscape_and_preexisting_actor_packages_changed':False,'full_other_content_verification':'pending final inventory'}
with (E/'authoring-preservation.json').open('x',encoding='utf-8') as f:json.dump(out,f,indent=2)
print(json.dumps(out,indent=2))
