"""Read-only Native51B numeric receipt validation, independent of Native43.

The caller seals policy/physical reads and supplies the complete current
Source/Config map. Historical receipts certify their actual project path only.
No approval, engine, GUI, process, package or save-routing action occurs here.
"""
import hashlib
import json
import ntpath
import re
from datetime import datetime, timezone
from pathlib import Path


NEW_SOURCE = "Source/Rogue10m/Tests/Rogue10mVillageQuestCombatCreditTests.cpp"
RED = re.compile(r"\bLog[^\s:]*:\s*(?:Error|Fatal)\s*:|\bFatal error\b|\bAssertion failed\b|\b(?:Handled\s+)?ensure\s+(?:condition|failed|failure)|LowLevelFatalError|Traceback \(most recent call last\)|AttributeError:", re.I)
START = re.compile(r"LogAutomationController:\s*Display:\s*Test Started\. Name=\{[^}]*\} Path=\{([^}]+)\}")
END = re.compile(r"LogAutomationController:\s*Display:\s*Test Completed\. Result=\{([^}]+)\} Name=\{[^}]*\} Path=\{([^}]+)\}")
WARNING = re.compile(r"(Log[^\s:]+):\s*Warning:\s*(.*)")
STATUS_EXIT = "FPlatformMisc::RequestExitWithStatus(1, 0, FEngineLoop::Tick.GScopedTestExit)"
PLAIN_EXIT = "FPlatformMisc::RequestExit(1, FEngineLoop::Tick.GScopedTestExit)"


def need(ok, reason):
    if not ok:
        raise ValueError(reason)


def sha(value):
    need(isinstance(value, str) and re.fullmatch(r"[0-9a-fA-F]{64}", value), "Invalid SHA256")
    return value.upper()


def path(value):
    value = str(value)
    need(ntpath.isabs(value) and bool(ntpath.splitdrive(value)[0]), "Require absolute Windows path")
    return ntpath.normcase(ntpath.normpath(value.replace("/", "\\")))


def utc(value):
    result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    need(result.tzinfo is not None and result.utcoffset().total_seconds() == 0, "Require UTC timestamp")
    return result


def parse(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            need(key not in result, "Duplicate JSON key")
            result[key] = value
        return result
    return json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique)


def sources(value):
    need(isinstance(value, dict) and value, "Missing full Source/Config map")
    result = {}
    for key, digest in value.items():
        identity = re.sub(r"[\\/]+", "/", key)
        need(not ntpath.isabs(identity) and identity.split("/")[0] in {"Source", "Config"} and all(part not in {"", ".", ".."} for part in identity.split("/")), "Unsafe Source/Config identity")
        identity = identity.casefold()
        need(identity not in result, "Colliding Source/Config identity")
        result[identity] = sha(digest)
    return result


def normalized(value):
    """Normalize only a reviewed paired world GUID, preserving every wrapper."""
    if isinstance(value, str):
        match = re.fullmatch(r"LogSpawn: UWorld::DestroyActor: World has no context! World: (VillageQuest(?:Lifecycle|CombatCredit))_([0-9A-Fa-f]{32}), Actor: /Engine/Transient\.\1_\2:PersistentLevel\.(Rogue10mVillageQuest(?:Objective|Monster))_([012])", value)
        if "VillageQuestLifecycle_" in value or "VillageQuestCombatCredit_" in value:
            need(match and (match[1], match[3]) in {("VillageQuestLifecycle", "Rogue10mVillageQuestObjective"), ("VillageQuestCombatCredit", "Rogue10mVillageQuestMonster")}, "Malformed raw dynamic world GUID warning")
            return value.replace(match[2], "<GUID>")
    if isinstance(value, list):
        return [normalized(item) for item in value]
    if isinstance(value, dict):
        return {key: normalized(item) for key, item in value.items()}
    return value


def validate_native51(refs, ctx):
    """Validate nine pinned Native51 artifacts; return evidence or ValueError.

    refs: process/result/reviewed/report/log/selection/build_result/launcher/
    preservation. ctx: root/project/engine/dll_sha256/source_config/
    editor_finished_utc/read_pin/policy. read_pin owns approved physical-path,
    reparse and stable-read checks. Policy is an owner-pinned input.
    """
    try:
        need("startup_review" not in refs, "Native51 forbids startup_review; whole log must have zero red rows")
        policy = ctx.get("policy") or parse(Path(__file__).with_name("native51-policy.json").read_bytes())
        need(policy["schema"] == "R10-native51-policy-v1", "Wrong Native51 policy")
        names = policy["tests"]
        need(len(names) == len(set(names)) == 51 and sum(name.startswith("Rogue10m.VillageQuest.") for name in names) == 26 and sum(name.startswith("Rogue10m.VillageQuest.CombatCredit.") for name in names) == 8, "Wrong exact Native51 policy")
        need(len(policy["warnings"]) == 11 and sum(row["count"] for row in policy["warnings"].values()) == 19, "Wrong warning policy totals")
        evidence = []
        def read(pin):
            data = ctx["read_pin"](pin)
            need(isinstance(data, bytes) and hashlib.sha256(data).hexdigest().upper() == sha(pin["sha256"]), "Pinned evidence bytes changed")
            evidence.append(dict(pin, size=len(data)))
            return data
        process, result, review, report, selection, build, preservation = [parse(read(refs[key])) for key in ("process", "result", "reviewed", "report", "selection", "build_result", "preservation")]
        read(refs["launcher"])
        log = read(refs["log"]).decode("utf-8-sig")
        lines = log.splitlines()
        need(not any(RED.search(line) for line in lines), "Native51 whole-log red error; no startup allowance exists")
        current = sources(ctx["source_config"])
        need(sha(refs["selection"]["sha256"]) == sha(policy["selection_sha256"]) and selection["tests"] == names, "Exact pinned selection differs")
        need(sha(selection["prior43_selection_sha256"]) == sha(policy["prior43_selection_sha256"]) and sha(selection["source_new_tests_sha256"]) == sha(policy["source_new_tests_sha256"]) == current[NEW_SOURCE.casefold()], "Selection is not bound to current new test source")
        for key, policy_key in (("build_result", "build_result_sha256"), ("launcher", "launcher_sha256")):
            need(sha(refs[key]["sha256"]) == sha(policy[policy_key]), "Different reviewed build/launcher")
        need(process["status"] == "running" and result["status"] == "headless_editor_exited_review_required", "Unknown/incomplete Native51 receipt")
        need({key: value for key, value in result.items() if key in process and key != "status"} == {key: value for key, value in process.items() if key != "status"}, "Process/result session differs")
        for record in (process, result):
            need(record["tests"] == names and type(record["count"]) is int and record["count"] == 51 and type(record["npc_cases"]) is int and record["npc_cases"] == 26, "Wrong receipt test set/count")
            need(sha(record["dll_sha256"]) == sha(ctx["dll_sha256"]) and sources(record["source_config_sha256"]) == current, "Different DLL/full Source/Config set")
            need(type(record["pid"]) is int and record["pid"] > 0 and type(record["gui_input_operations"]) is int and record["gui_input_operations"] == 0 and record["metadata_approval_or_clone_QA_retried"] is False and record["ordinary_cpp_fixture_UI_asset_loading_not_excluded"] is True, "Native51 execution boundary differs")
            need(sha(record["build_receipt_sha256"]) == sha(refs["build_result"]["sha256"]) and sha(record["launcher_sha256"]) == sha(refs["launcher"]["sha256"]), "Receipt build/launcher provenance differs")
        need(type(result["exit_code"]) is int and result["exit_code"] == 0 and result["same_dll"] is True and result["source_config_unchanged"] is True, "Native51 process/input failure")
        need(type(build["exit_code"]) is int and build["exit_code"] == 0 and sha(build["dll_sha256"]) == sha(ctx["dll_sha256"]) and build["gui_input_operations"] == 0 and build["metadata_approval_or_clone_QA_retried"] is False, "Wrong canonical Editor build")
        need(utc(build["started_utc"]) < utc(build["ended_utc"]) == utc(ctx["editor_finished_utc"]) < utc(result["started_utc"]) < utc(result["ended_utc"]), "Native51 predates canonical Editor build")
        project, root = path(ctx["project"]), path(ctx["root"])
        need(ntpath.dirname(project) == root, "Project differs from actual tested root")
        engine = str(ctx["engine"])
        executable = engine if engine.lower().endswith(".exe") else ntpath.join(engine, "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe")
        command = process["command"]
        need(isinstance(command, list) and len(command) == 18 and path(command[0]) == path(executable) and path(command[1]) == project, "Wrong canonical command project/engine")
        need(command[2:11] == ["/Engine/Maps/Entry", "-unattended", "-NullRHI", "-NoSound", "-NoSplash", "-NoLiveCoding", "-nop4", "-stdout", "-FullStdOutLogOutput"] and command[15:] == ["-culture=en", "-language=en", "-locale=en-US"], "Unreviewed Native51 flags/culture")
        selector = "Automation RunTests " + "+".join("^" + name + "$" for name in names)
        need(command[11] == "-ExecCmds=" + selector and command[12] == "-TestExit=Automation Test Queue Empty", "Non-exact Native51 selector/exit")
        need(command[13].startswith("-ReportExportPath=") and command[14].startswith("-abslog=") and path(command[13].split("=", 1)[1]) == ntpath.dirname(path(refs["report"]["path"])) and path(command[14].split("=", 1)[1]) == path(refs["log"]["path"]), "Different report/log session")
        expected_command = " ".join(command[2:11]) + ' -ExecCmds="' + selector + '" -TestExit="Automation Test Queue Empty" -ReportExportPath=' + command[13].split("=", 1)[1] + ' -abslog=' + command[14].split("=", 1)[1] + " " + " ".join(command[15:])
        need(re.findall(r"LogInit:\s*Command Line:\s*([^\r\n]+)", log) == [expected_command], "Actual engine command differs")
        for key, value in policy["report"].items():
            need(type(report[key]) is int and report[key] == value, "Raw report counters differ")
        tests = report["tests"]
        need(len(tests) == 51 and len({test["fullTestPath"] for test in tests}) == 51 and {test["fullTestPath"] for test in tests} == set(names), "Raw report missing/extra/duplicate tests")
        devices = report["devices"]
        need(len(devices) == 1 and devices[0]["platform"] == "WindowsEditor" and devices[0]["instanceName"].endswith("-" + str(result["pid"])), "Report device/PID differs")
        raw_warning_cases = []
        for test in tests:
            expected = policy["warnings"].get(test["fullTestPath"], {"count": 0, "entries": []})
            need(test["state"] == "Success" and type(test["errors"]) is int and test["errors"] == 0 and type(test["warnings"]) is int and test["warnings"] == expected["count"] and test["deviceInstance"] == [devices[0]["instance"]], "Raw per-test state/count/device differs")
            need(normalized(test["entries"]) == expected["entries"], "Raw full warning wrapper differs (GUID-only normalization)")
            if expected["count"]:
                raw_warning_cases.append({"test": test["fullTestPath"], "warnings": test["warnings"], "events": test["entries"]})
        for key, value in {"status": "reviewed_native51_cpp_numeric_regression", "count": 51, "npc_cases": 26, "new_combat_credit_cases": 8, "success": 51, "failed": [], "expected_names_exact": True, "completed_log_count": 51, "completed_log_names_exact": True, "queue51": True, "whole_log_red_count": 0, "red_rows": [], "report_succeeded": 40, "report_succeeded_with_warnings": 11, "warning_event_count": 19, "source_config_files": len(current), "source_config_unchanged": True, "game_execution": 0, "gui_input_operations": 0, "metadata_approval_or_clone_QA_retried": False}.items():
            need(type(review[key]) is type(value) and review[key] == value, "Reviewed Native51 summary differs: " + key)
        need(sha(review["dll_sha256"]) == sha(ctx["dll_sha256"]) and review["warning_cases"] == raw_warning_cases, "Reviewed full warning wrappers differ from raw report")
        starts = [(i, START.search(line)[1]) for i, line in enumerate(lines) if START.search(line)]
        ends = [(i, *END.search(line).groups()) for i, line in enumerate(lines) if END.search(line)]
        need(len(starts) == len(ends) == 51 and len({name for _, name in starts}) == 51 and {name for _, name in starts} == set(names), "Raw log started/completed set differs")
        need([name for _, name in starts] == [name for _, _, name in ends] and all(state == "Success" and begin < end for (begin, _), (end, state, _) in zip(starts, ends)) and all(ends[i][0] < starts[i + 1][0] for i in range(50)), "Raw log tests failed/overlap/out of order")
        startup_warnings = [m[1] + ": " + m[2] for line in lines[:starts[0][0]] if (m := WARNING.search(line))]
        need(startup_warnings == policy["startup_warning_messages"], "Startup warning inventory differs")
        for index, (begin, name) in enumerate(starts):
            stop = starts[index + 1][0] if index + 1 < len(starts) else len(lines)
            emitted, echoed = [], []
            for line in lines[begin:stop]:
                match = WARNING.search(line)
                if match:
                    if match[1] == "LogAutomationController":
                        echoed.append(normalized(re.sub(r"\s+\[log\]\s*$", "", match[2])))
                    else:
                        emitted.append(normalized(match[1] + ": " + match[2]))
            expected_messages = [entry["event"]["message"] for entry in policy["warnings"].get(name, {"entries": []})["entries"]]
            need(emitted == expected_messages and echoed == expected_messages, "Raw per-test emitted/echoed warning inventory differs")
        queues = [i for i, line in enumerate(lines) if re.search(r"LogAutomationCommandLine:\s*Display:\s*\.\.\.Automation Test Queue Empty 51 tests performed\.", line)]
        exits = [(i, match[0]) for i, line in enumerate(lines) for match in re.finditer(r"FPlatformMisc::RequestExit(?:WithStatus)?\([^)]*\)", line)]
        need(len(queues) == 1 and len(exits) == 2 and [value for _, value in exits] == [PLAIN_EXIT, STATUS_EXIT] and ends[-1][0] < queues[0] < exits[0][0] < exits[1][0], "Exact final queue51/plain-exit/status0 sequence differs")
        for key, value in policy["preservation"].items():
            need(type(preservation[key]) is type(value) and preservation[key] == value, "Historical preservation receipt differs: " + key)
        return {"status": "validated_native51_numeric_evidence", "tested_project": ctx["project"], "dll_sha256": sha(ctx["dll_sha256"]), "source_config_files": len(current), "native_tests": 51, "success": 51, "npc_cases": 26, "new_combat_credit_cases": 8, "warning_cases": 11, "warning_events": 19, "startup_warning_lines": 4, "whole_log_red_count": 0, "executed_here": False, "synthetic_fixture": ctx.get("synthetic_fixture") is True, "UI": "UNVERIFIED", "save_routing": "UNVERIFIED", "oldE5_contains_fix": False, "preservation_scope": "historical receipt boundary only; no current Content/save inventory claim", "evidence": evidence, "limits": ["No production Director/collision/loot/GUID/run-transition certification", "No actual UI/input/animation/visual or packaged-runtime certification", "No UserDir/Python/MCP/save-routing proof; no denied metadata/clone action"]}
    except ValueError:
        raise
    except (KeyError, TypeError, AttributeError, UnicodeError, OSError, IndexError) as error:
        raise ValueError("Incomplete Native51 evidence: " + str(error)) from error
