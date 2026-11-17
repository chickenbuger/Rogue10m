"""Launch the owned normal production editor, never the blocked clone-QA runner."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,types
R=Path(__file__).resolve().parents[3];E=R/'Feature/doc/evidence/native-landscape-crop20261114A'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('production_editor_owner');c.__file__=str(P);exec(compile(raw,str(P),'exec'),c.__dict__)
build=c.load(c.stable_bytes(E/'editor-build-result.json'));dup=c.load(c.stable_bytes(E/'duplicate-builder-A-result.json'))
assert build['exit_code']==0 and dup['exit_code']==0 and not dup['original_checkpoint_files_changed'] and not dup['red_lines']
for pin in build['new_Editor_products']:c.read_pin(pin)
for pin in dup['new_package_files']:c.read_pin(pin)
checkpoint=c.load(c.stable_bytes(E/'physical-checkpoint.json'))
for row in checkpoint['backed_up_files']:assert c.file_pin(R/row['path'])['sha256']==row['sha256']
owner=c.load(c.stable_bytes(R/'Feature/doc/evidence/hub-entry-windows20261114A/owner-result.json'));seal=c.read_json_pin(owner['seal'])
ps=r'''$rows=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')} | Select-Object ProcessId,Name,CommandLine);ConvertTo-Json -InputObject $rows -Depth 3 -Compress'''
q=subprocess.run([seal['runtime']['powershell']['path'],'-NoProfile','-Command',ps],capture_output=True,text=True,encoding='utf-8',timeout=30,check=True)
assert json.loads(q.stdout)==[],'Another editor/build/game owns the workspace; preserve and stop.'
backup=Path(checkpoint['backup']).parent/'new-candidate-before-edit';assert not backup.exists();backup.mkdir()
import shutil
for pin in dup['new_package_files']:
    old=Path(pin['path']);relative=old.relative_to(R);dest=c.physical(backup/relative)
    assert c.child(dest,backup) and not dest.exists();dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(old,dest)
    assert c.file_pin(dest)['sha256']==pin['sha256']
user=Path('C:/Users/PC/AppData/Local/Temp/R10NativeLandscapeCrop20261114A/EditorUser');assert not user.exists();user.mkdir()
log=E/'production-editor-A.log';out=E/'production-editor-A.stdout.log';receipt=E/'editor-launch.json'
assert not any(path.exists() for path in (log,out,receipt))
exe=Path('D:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe')
argv=[str(exe),str(R/'Rogue10m.uproject'),dup['new_production_map'],'-NoSplash','-NoSound','-NoLiveCoding','-culture=en','-language=en','-locale=en-US','-UserDir='+str(user),'-AbsLog='+str(log),'-ExecCmds=ModelContextProtocol.StartServer 8001']
env=dict(__import__('os').environ);env['PYTHONDONTWRITEBYTECODE']='1'
with out.open('xb') as stream:
    process=subprocess.Popen(argv,cwd=R,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW,env=env)
record={'status':'NORMAL_PRODUCTION_EDITOR_LAUNCHED_REQUIRE_LIVE_IDENTITY_AND_UI_ACCESS','started_utc':datetime.now(timezone.utc).isoformat(),'pid':process.pid,'args':argv,'map':dup['new_production_map'],'new_candidate_before_edit_backup':str(backup),'new_candidate_backup_files':len(dup['new_package_files']),'editor_products':build['new_Editor_products'],'MCP_endpoint':'http://127.0.0.1:8001/mcp','new_Game_executions':0,'GUI_input_by_launcher':0,'metadata_clone_QA_or_denied_startup_approval_actions':0}
with receipt.open('x',encoding='utf-8') as f:json.dump(record,f,indent=2)
print(json.dumps(record,indent=2),flush=True)
