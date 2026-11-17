"""Launch the current Editor build as a normal game in a fresh QA profile/save namespace."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,os,re,subprocess,types
R=Path(__file__).resolve().parents[3];E=R/'Feature/doc/evidence/npc-intro-compact20261113B'
assert (E/'cold-reload-result.json').is_file() and not (E/'game-D1-launch.json').exists()
pin=R/'Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e/strict_primitives.py'
raw=pin.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF'
c=types.ModuleType('game_save_guard');c.__file__=str(pin);exec(compile(raw,str(pin),'exec'),c.__dict__)
before=json.loads((E/'physical-checkpoint.json').read_text(encoding='utf-8'))
for item in before['protected_saves']:
 c.compare_inventory(c.load(c.stable_bytes(E/item['manifest'])),c.scan_tree(item['root'],True),'protected saves before game D1')
assert c.file_pin(R/'Binaries/Win64/UnrealEditor-Rogue10m.dll')['sha256']=='CA647B1E2264CB5DBC5025F81D2C28E10E12CDE5BF889F552033F8B1D2CB9175'
pwsh=r'C:\Users\PC\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
ps=r'''$p=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -match "^(UnrealEditor|UnrealBuildTool|AutomationTool|UnrealPak|Rogue10m)([.\-]|$)" -or ($_.Name -eq "dotnet.exe" -and $_.CommandLine -match "UnrealBuildTool|AutomationTool") } | Select-Object ProcessId,Name,CommandLine); ConvertTo-Json -InputObject $p -Depth 4 -Compress'''
obs=subprocess.run([pwsh,'-NoProfile','-Command',ps],capture_output=True,text=True,encoding='utf-8',check=True)
active=json.loads(obs.stdout or '[]');assert active==[],active
tag='CI20261113D01'
assert re.fullmatch('[A-Za-z0-9_]+',tag)
slots={'RogueProfileSaveSlot':'Rogue10m_Town_QA_'+tag,
       'RogueProgressionSavePrefix':'Rogue10m_Progression_QA_'+tag,
       'RogueSkillKeySaveSlot':'Rogue10m_Fighter_QA_'+tag}
assert len(slots['RogueProgressionSavePrefix'])<=80 and all(len(v)<=100 for v in slots.values())
qa=Path(os.environ['TEMP'])/'Rogue10mNpcIntro20261113B/GameD1'
assert not qa.exists();qa=c.physical(qa)
for forbidden in [R,Path(r'D:\Program Files\UE_5.8'),*[Path(x['root']) for x in before['protected_saves']]]:
 assert qa!=forbidden and forbidden not in qa.parents and qa not in forbidden.parents
qa.mkdir(parents=True); user=qa/'User';user.mkdir()
args=[r'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',str(R/'Rogue10m.uproject'),
 '/Game/Rogue10m/Maps/L_Menu','-game','-windowed','-ResX=1280','-ResY=720','-NoLiveCoding',
 '-NoRemoteShaderCompile','-NoSplash','-NoSound','-DisablePython','-culture=en','-language=en','-locale=en-US',
 '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
 '-UserDir='+str(user),*[f'-{k}={v}' for k,v in slots.items()],'-abslog='+str(E/'game-D1.log')]
with (E/'game-D1-stdout.log').open('xb') as stream:
 process=subprocess.Popen(args,cwd=R,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,
                          creationflags=subprocess.CREATE_NEW_PROCESS_GROUP|subprocess.CREATE_NO_WINDOW)
record={'status':'NORMAL_EDITOR_GAME_STARTED_UI_AND_ROUTING_UNVERIFIED','started_utc':datetime.now(timezone.utc).isoformat(),
 'owner_pid':process.pid,'command':args,'QA_root':str(qa),'QA_user_dir':str(user),
 'expected_save_root':str(user/'Saved/SaveGames'),'save_slots':slots,'fresh_tag':tag,
 'protected_saves_matched_before':2,'source_editor_DLL':'CA647B1E2264CB5DBC5025F81D2C28E10E12CDE5BF889F552033F8B1D2CB9175',
 'normal_hub_candidate_portal_exists':False,'candidate_entry_scope':'normal menu/profile/Hub baseline first; any later development travel to candidate must be recorded separately',
 'existing_Hub_modified':False,'packaged_Windows_EXE':False,'metadata_approval_or_clone_QA_retried':False}
(E/'game-D1-launch.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record,indent=2))
