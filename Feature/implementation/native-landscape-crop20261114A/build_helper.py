"""Build the additive production-only observer after preserving the previous Editor."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,shutil,subprocess,types
R=Path(__file__).resolve().parents[3];E=R/'Feature/doc/evidence/native-landscape-crop20261114A'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('production_editor_builder');c.__file__=str(P);exec(compile(raw,str(P),'exec'),c.__dict__)
checkpoint=c.load(c.stable_bytes(E/'physical-checkpoint.json'));derivation=c.load(c.stable_bytes(E/'helper-derivation.json'))
old=[v for v in checkpoint['backed_up_files'] if v['path'].startswith(('Source/','Config/'))];assert len(old)==368
for v in old:assert c.file_pin(R/v['path'])['sha256']==v['sha256']
for v in derivation['new_sources']:assert c.file_pin(R/v['path'])['sha256']==v['sha256']
old_owner=c.load(c.stable_bytes(R/'Feature/doc/evidence/hub-entry-windows20261114A/owner-result.json'));seal=c.read_json_pin(old_owner['seal'])
ps=r'''$rows=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')} | Select-Object ProcessId,Name,CommandLine);ConvertTo-Json -InputObject $rows -Depth 3 -Compress'''
q=subprocess.run([seal['runtime']['powershell']['path'],'-NoProfile','-Command',ps],capture_output=True,text=True,encoding='utf-8',timeout=30,check=True)
assert json.loads(q.stdout)==[],'Another owner is active; preserve and stop.'
backup=Path(checkpoint['backup'])/'EditorProducts';assert not backup.exists();backup.mkdir()
for v in checkpoint['editor_products']:
    c.read_pin(v);dest=backup/Path(v['path']).name;assert not dest.exists();shutil.copy2(v['path'],dest);assert c.file_pin(dest)['sha256']==v['sha256']
log=E/'editor-build.log';result=E/'editor-build-result.json';assert not log.exists() and not result.exists()
args=[seal['runtime']['powershell']['path'],'-NoProfile','-File',str(R/'Scripts/BuildEditor.ps1'),'-NoHotReload']
start=datetime.now(timezone.utc)
with log.open('xb') as f:
    process=subprocess.Popen(args,cwd=R,stdin=subprocess.DEVNULL,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    process.wait()
end=datetime.now(timezone.utc)
record={'started_utc':start.isoformat(),'ended_utc':end.isoformat(),'elapsed_seconds':(end-start).total_seconds(),'args':args,'pid':process.pid,'exit_code':process.returncode,'log':c.file_pin(log),'old_Editor_physical_backup':str(backup),'original_SourceConfig_368_unchanged':True,'new_Helper_Source_files':derivation['new_sources'],'new_Editor_products':[c.file_pin(v['path']) for v in checkpoint['editor_products']],'Game_or_GUI_executions':0,'denied_metadata_clone_QA_actions':0}
for v in old:assert c.file_pin(R/v['path'])['sha256']==v['sha256']
with result.open('x',encoding='utf-8') as f:json.dump(record,f,indent=2)
print(json.dumps({k:record[k] for k in ('exit_code','elapsed_seconds','log','original_SourceConfig_368_unchanged')},indent=2),flush=True)
raise SystemExit(process.returncode)
