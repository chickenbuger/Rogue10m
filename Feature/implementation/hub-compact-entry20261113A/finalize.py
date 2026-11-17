"""Compare all pre-existing inputs after saved/cold-reloaded additive Hub content."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import types

R=Path(__file__).resolve().parents[3]
E=R/'Feature/doc/evidence/hub-compact-entry20261113A'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('hub_post_preservation');c.__file__=str(P)
exec(compile(raw,str(P),'exec'),c.__dict__)
assert not (E/'final-result.json').exists()
before=c.load(c.stable_bytes(E/'physical-checkpoint.json'))
initial=c.read_json_pin(before['input_manifest'])
author=c.load(c.stable_bytes(E/'author02-result.json'))
cold=c.load(c.stable_bytes(E/'cold03-result.json'))
assert author['status']=='ADDITIVE_HUB_COMPACT_ENTRY_SIX_NEW_ACTORS_SAVED_RUNTIME_UNVERIFIED'
assert cold['status']=='SAVED_HUB_ENTRY_AND_COMPONENT_DATA_IDENTICAL_AFTER_COLD_RELOAD'
for phase in ('inspect03','author02','collision01','cold03'):
    receipt=c.load(c.stable_bytes(E/(phase+'-process.json')))
    assert receipt['phase']==phase and receipt['exit_code']==0 and not receipt['red_lines'] and receipt['source_Editor_DLL_unchanged']
    assert receipt['result_exists'] and not receipt['error_exists']
assert cold['portals']==author['portals_after'] and len(cold['new_actors'])==6
assert sorted(cold['new_actors'],key=lambda x:x['path'])==sorted(author['new_actors'],key=lambda x:x['path'])
inspection=c.load(c.stable_bytes(E/'inspect03-result.json'))
new_portals=[p for p in cold['portals'] if p['target_level_name']=='/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A']
old_portals=[p for p in cold['portals'] if p not in new_portals]
assert old_portals==inspection['old_portals'] and len(old_portals)==3 and len(new_portals)==1
assert new_portals[0]['start_run_timer_on_travel'] and new_portals[0]['additional_travel_options']==''
assert new_portals[0]['class']=='/Script/Rogue10m.Rogue10mRunPortal' and not new_portals[0]['spatially_loaded']
assert not new_portals[0]['editor_only'] and len(cold['deep_readback'])==6
assert cold['actor_desc_count']==691 and not cold['cold_capsule_query']['blocking_hit']
current=c.input_inventory(R)
print('Full input rehash complete: '+str(len(current['files'])),flush=True)
with (E/'inputs-after.json').open('x',encoding='utf-8') as f:json.dump(current,f,indent=2)
old={p['path']:p for p in initial['files']};now={p['path']:p for p in current['files']}
changed=[{'path':p,'before':v,'after':now.get(p)} for p,v in old.items() if now.get(p)!=v]
added=[p for k,p in now.items() if k not in old]
allowed={'Content/'+p.removeprefix('/Game/')+'.uasset' for p in author['exact_new_packages']}
assert len(allowed)==6
assert not changed,changed
assert {p['path'] for p in added}==allowed,added
assert set(initial['directories']).issubset(current['directories'])
backup=Path(before['backup'])
for p in before['backed_up_files']:
    assert c.file_pin(backup/p['path'])['sha256']==p['sha256']
saves=[]
for s in before['protected_saves']:
    after=c.scan_tree(s['root'],True)
    c.compare_inventory(s['inventory'],after,'protected save after Hub authoring')
    for f in after['files']:
        assert c.file_pin(Path(s['physical_backup'])/f['path'])['sha256']==f['sha256']
    saves.append({'root':s['root'],'files':len(after['files']),'unchanged':True,'physical_backup_verified':True})
copies=[]
for d in before['existing_two_physical_D_copies']:
    payload=c.payload_manifest(d['root'])
    assert c.digest(c.canonical(payload))==d['payload_sha256']
    copies.append({**d,'unchanged':True,'contains_new_Hub_entry':False})
oldE=R/'Feature/doc/evidence/npc-intro-compact20261113B'
owner=c.load(c.stable_bytes(oldE/'internal-windows-D-owner-result.json'))
seal=c.read_json_pin(owner['seal'])
for f in seal['editor_products']:c.read_pin(f)
assert len(seal['editor_products'])==6
new_backup=c.load(c.stable_bytes(E/'new-actor-before-fix.json'))
for f in new_backup['files']:assert c.file_pin(f['backup'])['sha256']==f['sha256']
report={'status':'HUB_COMPACT_ADDITIVE_ENTRY_SAVED_AND_COLD_RELOADED_ALL_PRIOR_INPUTS_PRESERVED',
        'recorded_utc':datetime.now(timezone.utc).isoformat(),'prior_input_files_unchanged':len(old),
        'current_input_files':len(now),'added_content_files':added,'existing_changed_files':changed,
        'source_config_files_unchanged':sum(p.startswith(('Source/','Config/')) for p in old),
        'physical_backup_files_verified':len(before['backed_up_files']),'protected_saves':saves,
        'D_physical_copies_preserved':copies,'source_editor_products_unchanged':len(seal['editor_products']),
        'old_portals_unchanged':len(old_portals),'new_portals':len(new_portals),'new_actor_count':len(cold['new_actors']),
        'new_portal_target':'/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A',
        'travel_options_static_contract':'StartRun=1; no game-mode override; normal baseline/supply logic unchanged',
        'read_only_inspection':c.file_pin(E/'inspect03-result.json'),
        'save_result':c.file_pin(E/'author02-result.json'),'cold_result':c.file_pin(E/'cold03-result.json'),
        'collision_correction':c.file_pin(E/'collision01-result.json'),
        'new_six_package_pre_correction_backup_verified':True,
        'first_failures_preserved':['inspect01-process.json','inspect01-error.json','inspect02-process.json','inspect02-error.json','author01-process.json','author01-error.json','cold01-process.json','cold01-error.json'],
        'actual_runtime_portal_travel':'UNVERIFIED','player_input_or_save_IO':'UNVERIFIED',
        'new_content_visual_review':'UNVERIFIED: null RHI API authoring only',
        'D_package_contains_this_change':False,'landscape_extent_m':[1800,1800],'landscape_cropped':False,
        'CUA_calls_or_live_D_input':0,'new_games_or_shared_builds':0,'forced_process_termination':False,
        'denied_metadata_clone_QA_or_approval_actions':0,'commit_push':0}
with (E/'final-result.json').open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
print(json.dumps({k:report[k] for k in ('status','prior_input_files_unchanged','current_input_files','new_actor_count','old_portals_unchanged','new_portals')},indent=2))
