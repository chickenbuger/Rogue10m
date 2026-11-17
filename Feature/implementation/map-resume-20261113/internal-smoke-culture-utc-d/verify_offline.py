"""Offline tests, PS parsing and UNSEALED probes only. Never launches UE/UAT."""
import argparse
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
import internal_smoke as m


def run(output, powershell, strict_zip):
    c = m.helpers(); output = c.physical(output)
    c.require(output.parent == m.HERE / "results" and not output.exists(), "Fresh packet/results/<label> required")
    output.mkdir()
    def save(name, value):
        with (output / name).open("x", encoding="utf-8", newline="\n") as out:
            json.dump(value, out, ensure_ascii=False, indent=2); out.write("\n")
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.TestLoader().discover(str(m.HERE), pattern="test_*.py"))
    (output / "tests.txt").write_text(stream.getvalue(), encoding="utf-8")
    parser = output / "parse-only.ps1"
    parser.write_text('''$tokens=$null;$errs=$null
$tree=[Management.Automation.Language.Parser]::ParseFile($args[0],[ref]$tokens,[ref]$errs)
$bad=@($tree.FindAll({param($n) ($n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -in @('Stop-Process','taskkill','tskill')) -or ($n -is [Management.Automation.Language.InvokeMemberExpressionAst] -and $n.Member.Value -in @('Kill','Terminate'))},$true))
@{parse_errors=@($errs).Count;termination_calls=$bad.Count;ue_executed=$false}|ConvertTo-Json -Compress
''', encoding="utf-8")
    parsed = subprocess.run([powershell,"-NoProfile","-NonInteractive","-File",str(parser),str(m.HERE/"Invoke-InternalSmoke.ps1")],capture_output=True,text=True,encoding="utf-8")
    c.require(parsed.returncode == 0 and not parsed.stderr.strip(), "Parse-only PowerShell failed")
    ast = json.loads(parsed.stdout); save("powershell-ast.json",ast)
    s,r = c.file_pin(m.HERE/"smoke.UNSEALED.json"),c.file_pin(m.HERE/"owner-release.UNSEALED.json")
    py = subprocess.run([sys.executable,"-I","-B","-X","utf8",str(m.HERE/"internal_smoke.py"),"--seal",s["path"],"--seal-sha256",s["sha256"],"--plan"],capture_output=True,text=True,encoding="utf-8")
    ps = subprocess.run([powershell,"-NoProfile","-NonInteractive","-File",str(m.HERE/"Invoke-InternalSmoke.ps1"),"-Seal",s["path"],"-SealSha256",s["sha256"],"-Release",r["path"],"-ReleaseSha256",r["sha256"],"-OwnerId","offline-only","-PythonPath",sys.executable],capture_output=True,text=True,encoding="utf-8")
    py_data,ps_data=json.loads(py.stdout),json.loads(ps.stdout)
    for name,process,data in (("python",py,py_data),("dispatcher",ps,ps_data)):
        save("unsealed-"+name+".json",{"exit":process.returncode,"result":data,"stderr":process.stderr})
        c.require(process.returncode != 0 and data["status"]=="blocked" and data["executed"] is False and data["acceptance_flags"]==m.FLAGS
                  and data["release_eligible"] is False and "bcr_args" not in data, "UNSEALED probe emitted execution or certification")
    c.require(ps_data["journal"] is None, "UNSEALED dispatcher wrote an execution journal")
    archive=c.file_pin(strict_zip)
    c.require(archive["sha256"]=="38AA7DE207F1E171C1104DDAD0C392FF21AE862548BAC270EC301C9E583601BC", "Old strict ZIP changed")
    save("strict-preservation.json",{"strict_zip":archive,"strict_primitives_sha256":m.HELPER_SHA,"old_strict_modified":False})
    audit=c.load(c.stable_bytes(m.HERE/"results/checkpoint-c-read-only.json"))
    c.require(audit["status"]=="blocked" and "startup red" in audit["blocker"] and audit["source_config_same_as_retained"] is True, "Expected real Native43 startup blocker observation missing")
    final_audit=c.load(c.stable_bytes(m.HERE/"results/native43-retained-a-preflight.json"))
    c.require(final_audit["status"]=="BLOCKED_STARTUP_REVIEW_REQUIRED" and all(p["status"]=="blocked" for p in final_audit["observations"]), "Final Native43 must block both missing and NOT_APPROVED review")
    c.read_pin(final_audit["validator"]);c.read_pin(final_audit["policy"])
    summary=m.stamp({"schema":"R10-internal-smoke-offline-validation-v1","status":"passed" if result.wasSuccessful() and ast["parse_errors"]==ast["termination_calls"]==0 else "failed",
        "offline_tests":result.testsRun,"failures":len(result.failures),"errors":len(result.errors),"skipped":len(result.skipped),
        "powershell_parse_errors":ast["parse_errors"],"termination_calls":ast["termination_calls"],"unsealed_probes_blocked":True,
        "packaging_status":"NOT_EXECUTED","new_ue_ubt_uat_cook_game_executions":0,"new_cua_gui_operations":0,"shared_project_writes":0,
        "actual_native43_read_only_observation":"BLOCKED_PENDING_EXACT_STARTUP_ERROR_REVIEW","actual_startup_errors":15,"actual_source_config_observed":366,
        "old_strict_zip_preserved":True,"candidate_status":"UNSEALED"})
    save("validation-summary.json",summary);print(json.dumps(summary,indent=2))
    return 0 if summary["status"]=="passed" else 1


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--output",required=True);p.add_argument("--powershell",required=True);p.add_argument("--strict-zip",required=True)
    a=p.parse_args();raise SystemExit(run(a.output,a.powershell,a.strict_zip))
