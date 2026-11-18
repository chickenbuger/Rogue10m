"""Exact C++ NPC combat-credit regression plus retained NativeD cases; no metadata QA or GUI input."""
from pathlib import Path
import argparse,collections,datetime,hashlib,json,re,subprocess,time
ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/'Feature/implementation/map-resume-20261113/native-no-gui-tests-20261113.json'
CPP=ROOT/'Source/Rogue10m/Tests/Rogue10mVillageQuestCombatCreditTests.cpp'
OUT=ROOT/'Feature/doc/evidence/npc-combat-credit-20261113A/Native51B'
ENGINE=Path('D:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
EXTRA_SUFFIXES=('AttributedLethalOwnerReceivesOneFact','InventoryReentryCannotRedirectLethalContext','UnattributedDeathKeepsRecoveryOpportunity','OwnerSwapDiscardsCreditAndRecoveryCannotFarmRewards','QueuedCreditSurvivesCorpseDestruction','RunningEventInvalidatesQueuedDeathCredit','ReentrantLowerSlotDeathDrainsThroughScheduledTimer','GuardedNestedRewardsScheduleSkippedLowerSlotDeath')
RED=re.compile(r'\bLog[^\s:]*:\s*(?:Error|Fatal)\s*:|\bFatal error\b|\bAssertion failed\b|\b(?:Handled\s+)?ensure\s+(?:condition|failed|failure)|LowLevelFatalError|Traceback \(most recent call last\)|AttributeError:',re.I)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def write(name,value):
 with (OUT/name).open('x',encoding='utf-8',newline='\n') as f:json.dump(value,f,indent=2)
def inputs():return {p.relative_to(ROOT).as_posix():sha(p) for folder in ('Source','Config') for p in (ROOT/folder).rglob('*') if p.is_file()}
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--execute',action='store_true');args=a.parse_args()
 assert sha(BASE)=='389C520AFF9C1F3B3358270C67480D427116DAC1B4D368AD429AEA45DDED99C8'
 j=json.loads(BASE.read_text(encoding='utf-8-sig'));old=[x if isinstance(x,str) else x['name'] for x in j['tests']]
 extra=['Rogue10m.VillageQuest.CombatCredit.'+s for s in EXTRA_SUFFIXES]
 observed=re.findall(r'"(Rogue10m\.VillageQuest\.CombatCredit\.[A-Za-z0-9]+)"',CPP.read_text(encoding='utf-8-sig'))
 assert len(old)==43 and len(observed)==8 and set(observed)==set(extra)
 names=old+extra;assert len(names)==len(set(names))==51
 assert sum(n.startswith('Rogue10m.VillageQuest.') for n in names)==26
 selection='+'.join('^'+n+'$' for n in names)
 command=[str(ENGINE),str(ROOT/'Rogue10m.uproject'),'/Engine/Maps/Entry','-unattended','-NullRHI','-NoSound','-NoSplash','-NoLiveCoding','-nop4','-stdout','-FullStdOutLogOutput','-ExecCmds=Automation RunTests '+selection,'-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(OUT/'report'),'-abslog='+str(OUT/'editor.log'),'-culture=en','-language=en','-locale=en-US']
 if not args.execute:
  print(json.dumps({'status':'plan_only','tests':names,'count':51,'npc_cases':26,'command':command,'metadata_approval_or_clone_QA':False},indent=2));return
 build=json.loads((OUT.parent/'editor-build-fixture-result.json').read_text(encoding='utf-8-sig'));assert build['exit_code']==0
 dll=ROOT/'Binaries/Win64/UnrealEditor-Rogue10m.dll';dll_sha=sha(dll);assert dll_sha==build['dll_sha256'].upper()
 OUT.mkdir(exist_ok=False);before=inputs();started=datetime.datetime.now(datetime.timezone.utc).isoformat()
 assert datetime.datetime.fromisoformat(build['ended_utc'].replace('Z','+00:00'))<datetime.datetime.fromisoformat(started)
 record={'status':'running','purpose':'Independent NPC combat-credit C++ regression; no denied metadata/clone action','started_utc':started,'dll_sha256':dll_sha,'source_config_sha256':before,'tests':names,'command':command,'count':51,'npc_cases':26,'gui_input_operations':0,'metadata_approval_or_clone_QA_retried':False,'ordinary_cpp_fixture_UI_asset_loading_not_excluded':True,'build_receipt_sha256':sha(OUT.parent/'editor-build-fixture-result.json'),'launcher_sha256':sha(Path(__file__))}
 write('selection.json',{'tests':names,'source_new_tests_sha256':sha(CPP),'prior43_selection_sha256':sha(BASE)})
 elapsed=time.monotonic()
 with (OUT/'stdout.log').open('w',encoding='utf-8') as so,(OUT/'stderr.log').open('w',encoding='utf-8') as se:
  proc=subprocess.Popen(command,cwd=ROOT,stdout=so,stderr=se,stdin=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
  record['pid']=proc.pid;write('process.json',record)
  try:code=proc.wait(timeout=900)
  except subprocess.TimeoutExpired:
   record.update(status='timeout_process_retained_outcome_uncertain',timeout_seconds=900);write('result.json',record);raise
 record.update(status='headless_editor_exited_review_required',exit_code=code,elapsed_seconds=time.monotonic()-elapsed,ended_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),same_dll=sha(dll)==dll_sha,source_config_unchanged=inputs()==before)
 write('result.json',record)
 assert code==0 and record['same_dll'] and record['source_config_unchanged'],'Process/input failure; preserve all evidence'
 report=json.loads((OUT/'report/index.json').read_text(encoding='utf-8-sig'));tests=report['tests'];actual=[t['fullTestPath'] for t in tests]
 assert len(actual)==51 and set(actual)==set(names),'Exact report selection mismatch'
 failed=[t['fullTestPath'] for t in tests if t['state']!='Success']
 text=(OUT/'editor.log').read_text(encoding='utf-8-sig');lines=text.splitlines();red=[line for line in lines if RED.search(line)]
 ends=re.findall(r'LogAutomationController:\s*Display:\s*Test Completed\. Result=\{([^}]+)\} Name=\{[^}]+\} Path=\{([^}]+)\}',text)
 warnings=[{'test':t['fullTestPath'],'warnings':t.get('warnings',0),'events':[e for e in t.get('entries',[]) if e.get('event',{}).get('type')=='Warning']} for t in tests if t.get('warnings',0)]
 review={'status':'reviewed_native51_cpp_numeric_regression','count':51,'npc_cases':26,'new_combat_credit_cases':8,'success':sum(t['state']=='Success' for t in tests),'failed':failed,'expected_names_exact':len(actual)==51 and set(actual)==set(names),'completed_log_count':len(ends),'completed_log_names_exact':len(ends)==51 and {n for _,n in ends}==set(names),'queue51':bool(re.search(r'Automation Test Queue Empty 51 tests performed\.',text)),'whole_log_red_count':len(red),'red_rows':red,'report_succeeded':report.get('succeeded'),'report_succeeded_with_warnings':report.get('succeededWithWarnings'),'warning_cases':warnings,'warning_event_count':sum(t.get('warnings',0) for t in tests),'source_config_files':len(before),'source_config_unchanged':record['source_config_unchanged'],'dll_sha256':dll_sha,'game_execution':0,'gui_input_operations':0,'metadata_approval_or_clone_QA_retried':False,'limits':['C++ isolated tests; not production Director/collision/loot/production GUID or actual UI/physical input certification','Source integration postdates NativeD43 and old WindowsE5; neither old evidence is updated']}
 write('reviewed-result.json',review)
 assert not failed and not red and review['queue51'] and review['completed_log_names_exact'],'Native51 report/log failure; no errors filtered'
 print(json.dumps({k:v for k,v in review.items() if k not in ('warning_cases','red_rows','limits')},indent=2))
if __name__=='__main__':main()
