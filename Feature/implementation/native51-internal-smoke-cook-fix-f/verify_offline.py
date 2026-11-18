"""Synthetic tests + PS parse/blocked probes. No UE/UAT/game or approval call."""
import argparse
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
import internal_smoke as m


def run(output,powershell):
    c=m.helpers();output=c.physical(output)
    c.require(output.parent==m.HERE/'results' and not output.exists(),'Fresh packet/results/<label> required')
    output.mkdir()
    def save(name,data):
        with (output/name).open('x',encoding='utf-8',newline='\n') as f:json.dump(data,f,ensure_ascii=False,indent=2);f.write('\n')
    buffer=io.StringIO();tests=unittest.TextTestRunner(stream=buffer,verbosity=2).run(unittest.TestLoader().discover(str(m.HERE),pattern='test_*.py'))
    (output/'tests.txt').write_text(buffer.getvalue(),encoding='utf-8')
    parser=output/'parse-only.ps1'
    parser.write_text('''$tokens=$null;$errs=$null
$tree=[Management.Automation.Language.Parser]::ParseFile($args[0],[ref]$tokens,[ref]$errs)
$bad=@($tree.FindAll({param($n) ($n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -in @('Stop-Process','taskkill','tskill')) -or ($n -is [Management.Automation.Language.InvokeMemberExpressionAst] -and $n.Member.Value -in @('Kill','Terminate'))},$true))
@{parse_errors=@($errs).Count;termination_calls=$bad.Count;ue_executed=$false}|ConvertTo-Json -Compress
''',encoding='utf-8')
    p=subprocess.run([powershell,'-NoProfile','-NonInteractive','-File',str(parser),str(m.HERE/'Invoke-InternalSmoke.ps1')],capture_output=True,text=True,encoding='utf-8')
    c.require(p.returncode==0 and not p.stderr.strip(),'PS parse-only failed');ast=json.loads(p.stdout);save('powershell-ast.json',ast)
    seal=c.file_pin(m.HERE/'smoke.UNSEALED.json');release=c.file_pin(m.HERE/'owner-release.UNSEALED.json')
    calls=[('python',[sys.executable,'-I','-B','-X','utf8',str(m.HERE/'internal_smoke.py'),'--seal',seal['path'],'--seal-sha256',seal['sha256'],'--plan']),
           ('dispatcher',[powershell,'-NoProfile','-NonInteractive','-File',str(m.HERE/'Invoke-InternalSmoke.ps1'),'-Seal',seal['path'],'-SealSha256',seal['sha256'],'-Release',release['path'],'-ReleaseSha256',release['sha256'],'-OwnerId','offline-only','-PythonPath',sys.executable])]
    for label,argv in calls:
        p=subprocess.run(argv,capture_output=True,text=True,encoding='utf-8');data=json.loads(p.stdout)
        save('unsealed-'+label+'.json',{'exit':p.returncode,'result':data,'stderr':p.stderr})
        c.require(p.returncode!=0 and data['status']=='blocked' and data['executed'] is False and data['release_eligible'] is False
                  and data['acceptance_flags']==m.FLAGS and 'bcr_args' not in data,'UNSEALED must not emit execution/certification')
        c.require(label!='dispatcher' or data['journal'] is None,'Blocked dispatcher wrote execution journal')
    audit=c.load(c.stable_bytes(m.HERE/'results/retained-readonly-audit-final.json'))
    c.require(audit['native51']['whole_log_red_count']==0 and audit['current_source_config_files_observed']==368
              and audit['E5_contains_NPC_credit_fix'] is False and audit['final_content_captured_or_frozen'] is False,'Retained provenance observation differs')
    c.read_pin(audit['preservedE5_delivery_manifest'])
    old=[]
    for name,want in [('CandidateWindows20261113A.delta.zip','38AA7DE207F1E171C1104DDAD0C392FF21AE862548BAC270EC301C9E583601BC'),
                      ('NonGuiSmoke20261113A.delta.zip','BADA1140C01AD87C70509F9185C156569186586136071910D48AAAB03A65484F')]:
        pin=c.file_pin(m.HERE.parent/name);c.require(pin['sha256']==want,'Old packet ZIP changed');old.append(pin)
    save('preservation.json',{'old_zip_pins':old,'E5_delivery_manifest':audit['preservedE5_delivery_manifest'],'E5_payload_rehashed_this_work':False,'old_artifacts_modified_by_this_work':False})
    summary=m.stamp({'schema':'R10-native51-smoke-offline-validation-v1','status':'passed' if tests.wasSuccessful() and ast['parse_errors']==ast['termination_calls']==0 else 'failed',
        'offline_tests':tests.testsRun,'failures':len(tests.failures),'errors':len(tests.errors),'skipped':len(tests.skipped),
        'powershell_parse_errors':ast['parse_errors'],'termination_calls':ast['termination_calls'],'unsealed_probes_blocked':True,
        'packaging_status':'NOT_EXECUTED','candidate_status':'UNSEALED','final_content_capture':'NOT_PERFORMED_OWNER_EDITING',
        'retained_native51_only':True,'native51_warnings_preserved':19,'native51_whole_red':0,'old_NativeA_startup15_not_reused':True,
        'new_ue_ubt_uat_cook_game_executions':0,'new_gui_operations':0,'approval_or_metadata_clone_QA_operations':0,'shared_writes':0})
    save('validation-summary.json',summary);print(json.dumps(summary,indent=2));return 0 if summary['status']=='passed' else 1


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True);p.add_argument('--powershell',required=True)
    a=p.parse_args();raise SystemExit(run(a.output,a.powershell))
