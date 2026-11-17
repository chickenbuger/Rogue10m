"""Back up normal Hub/candidate/source/save inputs; do not touch live D or run UE."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import subprocess
import types

R=Path(__file__).resolve().parents[3]
E=R/'Feature/doc/evidence/hub-compact-entry20261113A'
B=R/'tmp/HubCompactEntry20261113A/before'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('hub_preservation');c.__file__=str(P)
exec(compile(raw,str(P),'exec'),c.__dict__)
assert not E.exists() and not B.exists()
E.mkdir(parents=True);B.mkdir(parents=True)
assert c.physical(R/'Content',True)==R/'Content'
oldE=R/'Feature/doc/evidence/npc-intro-compact20261113B'
owner=c.load(c.stable_bytes(oldE/'internal-windows-D-owner-result.json'))
seal=c.read_json_pin(owner['seal'])
assert c.file_pin(R/'Binaries/Win64/UnrealEditor-Rogue10m.dll')['sha256']=='CA647B1E2264CB5DBC5025F81D2C28E10E12CDE5BF889F552033F8B1D2CB9175'
ps=r'''$rows=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')} | Select-Object ProcessId,ParentProcessId,Name,ExecutablePath,CommandLine);ConvertTo-Json -InputObject $rows -Depth 4 -Compress'''
q=subprocess.run([seal['runtime']['powershell']['path'],'-NoProfile','-Command',ps],capture_output=True,text=True,encoding='utf-8',check=True,timeout=30)
processes=json.loads(q.stdout)
delivery=c.load(c.stable_bytes(oldE/'internal-windows-D-delivery.json'))
allowed={14384:delivery['bootstrap'],54244:delivery['game']}
for p in processes:
    assert p['ProcessId'] in allowed,p
    assert p['ExecutablePath']==allowed[p['ProcessId']]['path'],p
    c.read_pin(allowed[p['ProcessId']])
    assert '-UserDir=C:\\Users\\PC\\AppData\\Local\\Temp\\R10Native51IntroSmoke20261113D\\PackageRuntime01\\User' in p['CommandLine']
assert {p['ProcessId'] for p in processes}==set(allowed),'Preserve changed owner state; re-evaluate independently.'
before=c.input_inventory(R)
with (E/'inputs-before.json').open('x',encoding='utf-8') as f:json.dump(before,f,indent=2)
assert before==c.read_json_pin(seal['input_manifest']),'Current source/content changed; preserve and stop.'
selected=[
    'Source','Config','Content/Rogue10m/Maps/StartHub.umap',
    'Content/__ExternalActors__/Rogue10m/Maps/StartHub',
    'Content/__ExternalObjects__/Rogue10m/Maps/StartHub',
    'Content/Rogue10m/Maps/Production/WorldLayoutCompact20261113A.umap',
    'Content/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A',
    'Content/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A',
    'Content/Rogue10m/World/Production/WorldLayoutCompact20261113A',
]
files=[]
for row in before['files']:
    relative=row['path']
    if not any(relative==s or relative.startswith(s+'/') for s in selected):continue
    source=c.physical(R/relative,True);target=c.physical(B/relative)
    assert c.child(target,B) and not target.exists()
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    assert c.file_pin(target)['sha256']==row['sha256']
    files.append(row)
saves=[]
for i,s in enumerate(seal['protected_saves']):
    inventory=c.scan_tree(s['root'],True)
    c.compare_inventory(c.read_json_pin(s['manifest']),inventory,'protected saves before Hub work')
    for row in inventory['files']:
        target=c.physical(B/'ProtectedSaves'/str(i)/row['path'])
        assert c.child(target,B) and not target.exists()
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(Path(s['root'])/row['path'],target)
        assert c.file_pin(target)['sha256']==row['sha256']
    saves.append({'root':s['root'],'inventory':inventory,'physical_backup':str(B/'ProtectedSaves'/str(i))})
copies=[]
for folder in [Path(delivery['archive']),Path(delivery['source_archive'])]:
    folder=c.physical(folder,True)
    assert not c.child(folder,R/'Content') and not c.child(R/'Content',folder)
    payload=c.payload_manifest(folder)
    assert c.digest(c.canonical(payload))==delivery['payload_sha256']
    copies.append({'root':str(folder),'payload_sha256':delivery['payload_sha256'],'files':len(payload)})
result={'status':'PHYSICAL_BACKUPS_AND_EXACT_NORMAL_PROJECT_INPUTS_READY','recorded_utc':datetime.now(timezone.utc).isoformat(),
        'root':str(R),'backup':str(B),'backed_up_files':files,'backup_files':len(files),
        'input_manifest':c.file_pin(E/'inputs-before.json'),'input_files':len(before['files']),
        'protected_saves':saves,'existing_two_physical_D_copies':copies,'processes':processes,
        'game_input_or_termination':0,'Source_Config_Content_changed_by_prepare':False,
        'purpose':'Normal source Hub additive candidate entry authoring; not denied metadata/clone QA or live game input'}
with (E/'physical-checkpoint.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
print(json.dumps({'status':result['status'],'backup_files':len(files),'inputs':len(before['files']),'D_copies':len(copies)}),flush=True)
