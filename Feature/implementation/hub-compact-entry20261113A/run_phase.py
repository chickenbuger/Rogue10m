"""Own one headless normal-project content commandlet; never control live D."""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import os
import subprocess
import types

R=Path(__file__).resolve().parents[3]
H=Path(__file__).resolve().parent
E=R/'Feature/doc/evidence/hub-compact-entry20261113A'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('hub_commandlet_guard');c.__file__=str(P)
exec(compile(raw,str(P),'exec'),c.__dict__)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--phase',required=True,choices=['inspect01','inspect02','inspect03','author01','author02','cold01','diagnose_cold02','collision01','cold03'])
args=parser.parse_args();phase=args.phase
before=c.load(c.stable_bytes(E/'physical-checkpoint.json'))
assert before['status']=='PHYSICAL_BACKUPS_AND_EXACT_NORMAL_PROJECT_INPUTS_READY'
assert not (E/(phase+'-launch.json')).exists() and not (E/(phase+'-process.json')).exists()
assert not (E/(phase+'-result.json')).exists() and not (E/(phase+'-error.json')).exists()
if phase=='inspect02':
    first=c.load(c.stable_bytes(E/'inspect01-process.json'))
    assert first['exit_code']==4294967295 and first['error_exists'] and not first['result_exists']
    assert any("blocking_hit" in r['text'] for r in first['red_lines'])
if phase=='inspect03':
    second=c.load(c.stable_bytes(E/'inspect02-process.json'))
    assert second['exit_code']==4294967295 and second['error_exists'] and not second['result_exists']
    assert any("Failed to find property 'blocking_hit'" in r['text'] for r in second['red_lines'])
if phase in ('author01','author02','cold01'):
    prior='author02' if phase=='cold01' else 'inspect03'
    receipt=c.load(c.stable_bytes(E/(prior+'-process.json')))
    assert receipt['exit_code']==0 and not receipt['red_lines']
    assert (E/(prior+'-result.json')).exists()
if phase=='diagnose_cold02':
    failed=c.load(c.stable_bytes(E/'cold01-process.json'))
    assert failed['exit_code']==4294967295 and failed['error_exists'] and not failed['result_exists']
    saved=c.load(c.stable_bytes(E/'author02-process.json'))
    assert saved['exit_code']==0 and not saved['red_lines']
if phase in ('collision01','cold03'):
    prior='diagnose_cold02' if phase=='collision01' else 'collision01'
    previous=c.load(c.stable_bytes(E/(prior+'-process.json')))
    assert previous['exit_code']==0 and not previous['red_lines'] and previous['result_exists'] and not previous['error_exists']
if phase=='collision01':
    backup=c.load(c.stable_bytes(E/'new-actor-before-fix.json'))
    assert len(backup['files'])==6
    for f in backup['files']:
        assert c.file_pin(R/f['path'])['sha256']==f['sha256']
        assert c.file_pin(f['backup'])['sha256']==f['sha256']
baseline=c.read_json_pin(before['input_manifest'])
if phase=='author02':
    failure=c.load(c.stable_bytes(E/'author01-process.json'))
    assert failure['exit_code']==4294967295 and failure['error_exists'] and not failure['result_exists']
    assert not (E/'author01-save-intent.json').exists()
    current=c.input_inventory(R)
    with (E/'author02-inputs-before.json').open('x',encoding='utf-8') as f:json.dump(current,f,indent=2)
    assert current==baseline,'Previous failed author or concurrent work changed inputs; preserve and stop.'
    print('Full input comparison before author02: unchanged '+str(len(current['files'])),flush=True)
if phase in ('inspect01','inspect02','inspect03','author01','author02'):
    for row in before['backed_up_files']:
        assert c.file_pin(R/row['path'])['sha256']==row['sha256'],'Protected source/Hub/candidate file changed: '+row['path']
    for scope in ('Source','Config'):
        tree=c.scan_tree(R/scope)
        actual=[{**r,'path':scope+'/'+r['path']} for r in tree['files']]
        assert actual==[r for r in baseline['files'] if r['path'].startswith(scope+'/')]
ps=r'''$rows=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor.*|UnrealBuildTool|AutomationTool|Rogue10m.*)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')} | Select-Object ProcessId,ParentProcessId,Name,ExecutablePath,CommandLine);ConvertTo-Json -InputObject $rows -Depth 4 -Compress'''
pwsh=r'C:\Users\PC\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
q=subprocess.run([pwsh,'-NoProfile','-Command',ps],capture_output=True,text=True,encoding='utf-8',check=True,timeout=30)
processes=json.loads(q.stdout)
oldE=R/'Feature/doc/evidence/npc-intro-compact20261113B'
delivery=c.load(c.stable_bytes(oldE/'internal-windows-D-delivery.json'))
allowed={14384:delivery['bootstrap'],54244:delivery['game']}
for p in processes:
    assert p['ProcessId'] in allowed and p['ExecutablePath']==allowed[p['ProcessId']]['path'],p
    c.read_pin(allowed[p['ProcessId']])
assert {p['ProcessId'] for p in processes}==set(allowed),'Owner state changed; stop instead of guessing.'
editor_dll=c.file_pin(R/'Binaries/Win64/UnrealEditor-Rogue10m.dll')
assert editor_dll['sha256']=='CA647B1E2264CB5DBC5025F81D2C28E10E12CDE5BF889F552033F8B1D2CB9175'
engine=Path('D:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
area=c.physical(Path('C:/Users/PC/AppData/Local/Temp/R10HubCompactEntry20261113A')/phase)
assert not area.exists()
area.mkdir(parents=True);user=area/'User';user.mkdir()
script_copy=E/(phase+'-script.py')
assert not script_copy.exists()
script_bytes=c.stable_bytes(H/'hub_entry.py')
with script_copy.open('xb') as f:f.write(script_bytes)
assert c.file_pin(script_copy)['sha256']==c.file_pin(H/'hub_entry.py')['sha256']
cmd=[str(engine),str(R/'Rogue10m.uproject'),'-run=pythonscript','-script='+str(script_copy),
     '-unattended','-nullrhi','-nosound','-nosplash','-NoLiveCoding','-NoRemoteShaderCompile',
     '-SCCProvider=None','-utf8output','-culture=en','-language=en','-locale=en-US',
     '-UserDir='+str(user),'-AbsLog='+str(E/(phase+'-editor.log')),
     '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False']
environment=os.environ.copy();environment['R10_HUB_PACKET_PHASE']=phase
environment['PYTHONDONTWRITEBYTECODE']='1'
started=datetime.now(timezone.utc).isoformat()
with (E/(phase+'-stdout.log')).open('xb') as stream:
    child=subprocess.Popen(cmd,cwd=R,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,
                           env=environment,creationflags=subprocess.CREATE_NO_WINDOW|subprocess.CREATE_NEW_PROCESS_GROUP)
    launch={'purpose':'Normal source-project Hub content API operation, unrelated to denied metadata/clone QA or live D UI',
            'phase':phase,'pid':child.pid,'started_utc':started,'command':cmd,'user_dir':str(user),
            'engine':c.file_pin(engine),'script':c.file_pin(script_copy),'source_editor_dll':editor_dll,
            'preserved_existing_D_processes':processes,'GUI_or_live_D_input':False}
    with (E/(phase+'-launch.json')).open('x',encoding='utf-8') as f:json.dump(launch,f,indent=2)
    try:
        code=child.wait(timeout=600)
    except subprocess.TimeoutExpired:
        with (E/(phase+'-timeout.json')).open('x',encoding='utf-8') as f:
            json.dump({'pid':child.pid,'status':'OWNED_COMMANDLET_TIMEOUT_PRESERVED_NO_TERMINATION','phase':phase},f,indent=2)
        raise
log=E/(phase+'-editor.log')
lines=log.read_text(encoding='utf-8-sig',errors='replace').splitlines() if log.exists() else []
red=[{'line':i+1,'text':v} for i,v in enumerate(lines) if c.RED.search(v)]
result={'phase':phase,'pid':child.pid,'started_utc':started,'ended_utc':datetime.now(timezone.utc).isoformat(),
        'exit_code':code,'red_lines':red,'result_exists':(E/(phase+'-result.json')).exists(),
        'error_exists':(E/(phase+'-error.json')).exists(),'engine_log':c.file_pin(log) if log.exists() else None,
        'source_Editor_DLL_unchanged':c.file_pin(R/'Binaries/Win64/UnrealEditor-Rogue10m.dll')==editor_dll,
        'GUI_or_live_D_input':False,'forced_process_termination':False,'denied_metadata_clone_QA':False}
with (E/(phase+'-process.json')).open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
print(json.dumps(result,indent=2))
assert code==0 and not red and result['result_exists'] and not result['error_exists'] and result['source_Editor_DLL_unchanged']
