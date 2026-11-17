"""Create one fresh real production map using Epic's WP duplicate builder."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,types
R=Path(__file__).resolve().parents[3];E=R/'Feature/doc/evidence/native-landscape-crop20261114A'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('normal_production_duplicate');c.__file__=str(P);exec(compile(raw,str(P),'exec'),c.__dict__)
checkpoint=c.load(c.stable_bytes(E/'physical-checkpoint.json'))
build=c.load(c.stable_bytes(E/'editor-build-result.json'));assert build['exit_code']==0
source=checkpoint['source_map'];target=checkpoint['new_production_map_reserved_not_created']
assert source=='/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'
assert target=='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
relative=target.removeprefix('/Game/')
destinations=[R/'Content'/(relative+'.umap'),R/'Content'/(relative+'_BuiltData.uasset'),R/'Content/__ExternalActors__'/relative,R/'Content/__ExternalObjects__'/relative]
for path in destinations:assert not c.physical(path).exists(),('Entire destination namespace must be absent',path)
for row in checkpoint['backed_up_files']:assert c.file_pin(R/row['path'])['sha256']==row['sha256']
for row in c.load(c.stable_bytes(E/'helper-derivation.json'))['new_sources']:assert c.file_pin(R/row['path'])['sha256']==row['sha256']
owner=c.load(c.stable_bytes(R/'Feature/doc/evidence/hub-entry-windows20261114A/owner-result.json'));seal=c.read_json_pin(owner['seal'])
ps=r'''$rows=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')} | Select-Object ProcessId,Name,CommandLine);ConvertTo-Json -InputObject $rows -Depth 3 -Compress'''
q=subprocess.run([seal['runtime']['powershell']['path'],'-NoProfile','-Command',ps],capture_output=True,text=True,encoding='utf-8',timeout=30,check=True)
assert json.loads(q.stdout)==[],'Another engine/build/game exists; preserve and stop.'
user=Path('C:/Users/PC/AppData/Local/Temp/R10NativeLandscapeCrop20261114A/DuplicateUser');assert not user.exists();user.mkdir(parents=True)
log=E/'duplicate-builder-A.log';stdout=E/'duplicate-builder-A.stdout.log';result=E/'duplicate-builder-A-result.json'
assert not any(path.exists() for path in (log,stdout,result))
exe=Path('D:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
args=[str(exe),str(R/'Rogue10m.uproject'),source,'-run=WorldPartitionBuilderCommandlet','-Builder=WorldPartitionRenameDuplicateBuilder','-NewPackage='+target,'-SCCProvider=None','-unattended','-NoSplash','-NoSound','-NoLiveCoding','-DisablePython','-NullRHI','-culture=en','-language=en','-locale=en-US','-UserDir='+str(user),'-AbsLog='+str(log),'-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False']
assert not any(arg.lower()=='-rename' for arg in args)
start=datetime.now(timezone.utc)
with stdout.open('xb') as stream:
    process=subprocess.Popen(args,cwd=R,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    process.wait()
end=datetime.now(timezone.utc)
unchanged=[]
for row in checkpoint['backed_up_files']:
    if c.file_pin(R/row['path'])['sha256']!=row['sha256']:unchanged.append(row['path'])
new_files=[]
for path in destinations:
    if path.is_file():new_files.append(c.file_pin(path))
    elif path.is_dir():new_files.extend(c.file_pin(f) for f in sorted(path.rglob('*')) if f.is_file())
text=c.stable_bytes(log).decode('utf-8-sig',errors='replace') if log.exists() else ''
record={'status':'OFFICIAL_PRODUCTION_WP_DUPLICATE_RETURNED_REQUIRES_IN_EDITOR_OWNERSHIP_AND_DATA_VALIDATION','started_utc':start.isoformat(),'ended_utc':end.isoformat(),'elapsed_seconds':(end-start).total_seconds(),'pid':process.pid,'args':args,'exit_code':process.returncode,'source_map':source,'new_production_map':target,'fresh_destination_guard':True,'original_checkpoint_files_changed':unchanged,'new_package_files':new_files,'log':c.file_pin(log) if log.exists() else None,'red_lines':[line for line in text.splitlines() if c.RED.search(line)],'actual_crop_performed':False,'GUI_or_Game_executions':0,'denied_metadata_clone_QA_or_approval_actions':0}
with result.open('x',encoding='utf-8') as f:json.dump(record,f,indent=2)
print(json.dumps({k:record[k] for k in ('status','exit_code','elapsed_seconds','original_checkpoint_files_changed','new_production_map')},indent=2),flush=True)
print('New physical packages: '+str(len(new_files)),flush=True)
assert process.returncode==0 and not unchanged and not record['red_lines'] and destinations[0].is_file()
