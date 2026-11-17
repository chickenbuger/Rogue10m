"""Physical recovery checkpoint of the latest real content before map production."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import subprocess
import types

R=Path(__file__).resolve().parents[3]
E=R/'Feature/doc/evidence/native-landscape-crop20261114A'
B=R/'tmp/NativeLandscapeCrop20261114A/before'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('crop_physical_checkpoint');c.__file__=str(P);exec(compile(raw,str(P),'exec'),c.__dict__)
assert not E.exists() and not B.exists()
E.mkdir(parents=True);B.mkdir(parents=True)

def write(path,value):
    with path.open('x',encoding='utf-8') as f:json.dump(value,f,indent=2)
    return c.file_pin(path)

W=R/'Feature/doc/evidence/hub-entry-windows20261114A'
final=c.load(c.stable_bytes(W/'final-result.json'))
assert final['status']=='HUB_ENTRY_WINDOWS_INTERNAL_DEVELOPMENT_BUILT_NOT_EXECUTED_UNVERIFIED'
owner=c.load(c.stable_bytes(W/'owner-result.json'));seal=c.read_json_pin(owner['seal'])
ps=r'''$rows=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')} | Select-Object ProcessId,ParentProcessId,Name,ExecutablePath,CommandLine);ConvertTo-Json -InputObject $rows -Depth 4 -Compress'''
q=subprocess.run([seal['runtime']['powershell']['path'],'-NoProfile','-Command',ps],capture_output=True,text=True,encoding='utf-8',timeout=30,check=True)
processes=json.loads(q.stdout)
write(E/'processes-before.json',{'utc':datetime.now(timezone.utc).isoformat(),'processes':processes})
assert not processes,'Preserve another engine/build/game; coordinate ownership before map changes.'
current=c.input_inventory(R)
c.compare_inventory(c.read_json_pin(seal['input_manifest']),current,'latest inputs before crop checkpoint')
assert len(current['files'])==10472
input_pin=write(E/'inputs-before.json',current)
selected=['Source','Config','Content/Rogue10m/Maps/StartHub.umap','Content/__ExternalActors__/Rogue10m/Maps/StartHub','Content/__ExternalObjects__/Rogue10m/Maps/StartHub','Content/Rogue10m/Maps/Production/WorldLayoutCompact20261113A.umap','Content/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A','Content/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A','Content/Rogue10m/World/Production/WorldLayoutCompact20261113A']
files=[]
for row in current['files']:
    relative=row['path']
    if not any(relative==key or relative.startswith(key+'/') for key in selected):continue
    old=c.physical(R/relative,True);new=c.physical(B/relative)
    assert c.child(new,B) and not new.exists()
    new.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(old,new)
    assert c.file_pin(new)['sha256']==row['sha256']
    files.append(row)
latest_hub=c.load(c.stable_bytes(R/'Feature/doc/evidence/hub-compact-entry20261113A/final-result.json'))
assert all(row in files for row in latest_hub['added_content_files'])
assert any('DA_IntroSupply_Compact20261113B.uasset' in row['path'] for row in files)
saves=[]
for i,s in enumerate(seal['protected_saves']):
    actual=c.scan_tree(s['root'],True)
    c.compare_inventory(c.read_json_pin(s['manifest']),actual,'protected saves before actual map authoring')
    for row in actual['files']:
        new=c.physical(B/'ProtectedSaves'/str(i)/row['path']);assert c.child(new,B) and not new.exists()
        new.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(Path(s['root'])/row['path'],new)
        assert c.file_pin(new)['sha256']==row['sha256']
    saves.append({'root':s['root'],'inventory':actual,'backup':str(B/'ProtectedSaves'/str(i))})
for pin in seal['editor_products']:c.read_pin(pin)
packages=[]
for root in (final['archive'],final['source_archive']):
    data=c.payload_manifest(root);assert c.digest(c.canonical(data))==final['payload_sha256']
    packages.append({'root':root,'files':len(data),'payload_sha256':final['payload_sha256']})
checkpoint={
    'status':'LATEST_COMPACT_NPC_DA_HUB_SOURCE_CONFIG_PHYSICAL_BACKUP_VERIFIED_NO_ASSETS_CHANGED',
    'recorded_utc':datetime.now(timezone.utc).isoformat(),'root':str(R),'backup':str(B),'input_manifest':input_pin,
    'input_files':len(current['files']),'backed_up_files':files,'backup_files':len(files),
    'latest_six_Hub_actor_packages_included':True,'latest_Intro_DA_included':True,
    'protected_saves':saves,'editor_products':seal['editor_products'],'new_Hub_Windows_copies':packages,
    'old_B_C_D_E5_preservation_receipt':c.file_pin(W/'final-result.json'),
    'source_map':'/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A',
    'new_production_map_reserved_not_created':'/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A',
    'proposed_retention':{'component_x_inclusive':[5,13],'component_y_inclusive':[10,14],'section_roi':[315,630,882,945],'grid':[9,5],'extent_m':[1012.5,562.5],'approved_final_size':False},
    'engine_or_GUI_actions_by_prepare':0,'Content_Source_Config_changed':False,'denied_metadata_clone_QA_or_approval_actions':0,
}
write(E/'physical-checkpoint.json',checkpoint)
print(json.dumps({k:checkpoint[k] for k in ('status','backup_files','input_files','latest_six_Hub_actor_packages_included','latest_Intro_DA_included')},indent=2),flush=True)
