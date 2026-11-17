"""Read-only Native43 artifact validation, not a release/UI/save-isolation seal.

The actual tested project must equal ctx.project. To retain origin evidence for
a relocated snapshot, the owner must separately seal the origin->target byte
mapping; calling this API with a different project is intentionally rejected.
Startup Error/Fatal/ensure rows require an explicit per-log reviewed envelope.
The shipped NOT_APPROVED sample is never an approval. read_pin owns physical
path/immutable-read checks; bytes are independently SHA256 checked here.
"""
import hashlib
import json
import ntpath
import re
from datetime import datetime
from pathlib import Path


RED = re.compile(r"\bLog[^\s:]*:\s*(?:Error|Fatal)\s*:|\bFatal error\b|\bAssertion failed\b|\b(?:Handled\s+)?ensure\s+(?:condition|failed|failure)|LowLevelFatalError|Traceback \(most recent call last\)|AttributeError:", re.I)
START = re.compile(r"LogAutomationController:\s*Display:\s*Test Started\. Name=\{[^}]*\} Path=\{([^}]+)\}")
END = re.compile(r"LogAutomationController:\s*Display:\s*Test Completed\. Result=\{([^}]+)\} Name=\{[^}]*\} Path=\{([^}]+)\}")


def need(ok, why):
    if not ok:
        raise ValueError(why)


def sha(value):
    need(isinstance(value, str) and re.fullmatch(r"[0-9a-fA-F]{64}", value), "Invalid SHA256")
    return value.upper()


def path(value):
    value = str(value)
    need(ntpath.isabs(value) and bool(ntpath.splitdrive(value)[0]), "Require absolute Windows path")
    return ntpath.normcase(ntpath.normpath(value.replace("/", "\\")))


def timestamp(value):
    result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    need(result.tzinfo is not None and result.utcoffset().total_seconds() == 0, "Timestamp must be UTC")
    return result


def parse(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            need(key not in result, "Duplicate JSON key")
            result[key] = value
        return result
    return json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique)


def source_map(value):
    need(isinstance(value, dict), "Missing source/config map")
    result = {}
    for key, digest in value.items():
        identity = re.sub(r"[\\/]+", "/", key)
        need(not ntpath.isabs(identity) and identity.split("/")[0] in {"Source", "Config"} and all(p not in {"", ".", ".."} for p in identity.split("/")), "Unsafe source/config identity")
        identity = identity.casefold()
        need(identity not in result, "Colliding source/config identity")
        result[identity] = sha(digest)
    need(bool(result), "Empty source/config set")
    return result


def message(value):
    # Only this exact reviewed dynamic-world shape is normalized; categories,
    # actor suffixes and the complete warning text remain significant.
    if "VillageQuestLifecycle_" in value:
        match = re.fullmatch(r"LogSpawn: UWorld::DestroyActor: World has no context! World: VillageQuestLifecycle_([0-9A-Fa-f]{32}), Actor: /Engine/Transient\.VillageQuestLifecycle_\1:PersistentLevel.Rogue10mVillageQuestObjective_([01])", value)
        if match:
            value = value.replace(match[1], "<GUID>")
    return value


def validate_native43(refs, ctx):
    """Return reviewed numeric evidence summary; raise ValueError on any gap.

    refs: pinned process/result/reviewed/report/log; optional startup_review.
    ctx: root,project,engine,dll_sha256,source_config,editor_finished_utc,read_pin.
    read_pin must enforce stable physical reads under owner-approved roots.
    """
    try:
        policy = ctx.get("policy") or parse(Path(__file__).with_name("native43-policy.json").read_bytes())
        names = policy["tests"]
        need(len(names) == len(set(names)) == 43, "Invalid Native43 policy")
        evidence = []
        def read(pin):
            data = ctx["read_pin"](pin)
            need(isinstance(data, bytes) and hashlib.sha256(data).hexdigest().upper() == sha(pin["sha256"]), "Pinned evidence bytes changed")
            evidence.append(dict(pin, size=len(data)))
            return data
        process, result, reviewed, report = [parse(read(refs[k])) for k in ("process", "result", "reviewed", "report")]
        selection = parse(read(refs["selection"]))
        selected_names = [row if isinstance(row, str) else row["name"] for row in selection["tests"]]
        need(selected_names == names and sha(refs["selection"]["sha256"]) == sha(policy["selection_sha256"]), "Pinned selection JSON differs from exact policy")
        log_data = read(refs["log"])
        text = log_data.decode("utf-8-sig")
        lines = text.splitlines()
        project, root = path(ctx["project"]), path(ctx["root"])
        need(ntpath.dirname(project) == root, "Project is not at candidate root")
        engine = str(ctx["engine"])
        exe = engine if engine.lower().endswith(".exe") else ntpath.join(engine, "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe")
        current = source_map(ctx["source_config"])
        need(process["status"] == "running" and result["status"] in {"commandlet_exited_report_review_required", "headless_editor_exited_report_review_required"}, "Uncompleted/unknown Native43 receipt")
        need(type(result["exit_code"]) is int and result["exit_code"] == 0 and result["same_dll"] is True and result["source_config_unchanged"] is True, "Native43 process/input failure")
        for record in (process, result):
            need(sha(record["dll_sha256"]) == sha(ctx["dll_sha256"]) and source_map(record["source_config_sha256"]) == current, "Different DLL/source/config set")
            need(record["tests"] == names and sha(record["selection_sha256"]) == sha(policy["selection_sha256"]), "Different exact Native43 selection")
            need(type(record["pid"]) is int and record["pid"] > 0 and type(record["gui_input_operations"]) is int and record["gui_input_operations"] == 0, "Invalid PID/GUI boundary")
        need(process["pid"] == result["pid"] and process["command"] == result["command"] and process["started_utc"] == result["started_utc"], "Process/result session mismatch")
        need(timestamp(ctx["editor_finished_utc"]) <= timestamp(result["started_utc"]) < timestamp(result["ended_utc"]), "Tests predate supplied canonical Editor build")
        command = process["command"]
        need(isinstance(command, list) and len(command) == 18 and path(command[0]) == path(exe) and path(command[1]) == project, "Wrong canonical command project/engine or exact 18-argument count")
        need(command[2:11] == ["/Engine/Maps/Entry", "-unattended", "-NullRHI", "-NoSound", "-NoSplash", "-NoLiveCoding", "-nop4", "-stdout", "-FullStdOutLogOutput"], "Unreviewed Native43 command flags")
        culture_args = ["-culture=en", "-language=en", "-locale=en-US"]
        need(command[15:] == culture_args, "Exact ordered Native43 culture tail required")
        selector = "Automation RunTests " + "+".join("^" + name + "$" for name in names)
        need(command[11] == "-ExecCmds=" + selector and command[12] == "-TestExit=Automation Test Queue Empty", "Non-exact native selector/exit command")
        need(path(command[13].removeprefix("-ReportExportPath=")) == ntpath.dirname(path(refs["report"]["path"])) and path(command[14].removeprefix("-abslog=")) == path(refs["log"]["path"]), "Wrong raw report/log output session")
        actual = re.findall(r"LogInit:\s*Command Line:\s*([^\r\n]+)", text)
        need(len(actual) == 1, "Actual engine command line missing/duplicate")
        # Engine removes executable/project from its own LogInit command line.
        expected_command = " ".join(command[2:11]) + ' -ExecCmds="' + selector + '" -TestExit="Automation Test Queue Empty" -ReportExportPath=' + command[13].split("=", 1)[1] + ' -abslog=' + command[14].split("=", 1)[1] + " " + " ".join(culture_args)
        need(actual[0] == expected_command, "Actual engine command differs from recorded session")
        for key, value in policy["report"].items():
            need(type(report[key]) is int and report[key] == value, "Raw report counters differ")
        tests = report["tests"]
        need(len(tests) == 43 and len({t["fullTestPath"] for t in tests}) == 43 and {t["fullTestPath"] for t in tests} == set(names), "Raw report has missing/extra/duplicate test")
        devices = report["devices"]
        need(len(devices) == 1 and devices[0]["platform"] == "WindowsEditor" and devices[0]["instanceName"].endswith("-" + str(result["pid"])), "Report device/PID differs")
        warnings = {}
        for test in tests:
            expected = policy["warnings"].get(test["fullTestPath"], {"count": 0, "events": []})
            need(test["state"] == "Success" and type(test["errors"]) is int and test["errors"] == 0 and type(test["warnings"]) is int and test["warnings"] == expected["count"] and test["deviceInstance"] == [devices[0]["instance"]], "Raw per-test state/count/device failure")
            events = [e["event"] for e in test["entries"] if e["event"]["type"] == "Warning"]
            need(not any(e["event"]["type"] == "Error" for e in test["entries"]), "Report contains an Error event")
            normalized = [dict(e, message=message(e["message"])) for e in events]
            need(normalized == expected["events"], "Warning event text/context/artifact differs")
            if events:
                warnings[test["fullTestPath"]] = test["warnings"]
        need(reviewed["completed"] == reviewed["expected_count"] == 43 and reviewed["expected_names_exact"] is True and reviewed["states"] == {"Success": 43} and reviewed["engine_queue_empty_43"] is True and reviewed["source_config_path_set_unchanged"] is True, "Reviewed summary incomplete")
        for key in ("missing", "extra", "failed"):
            need(reviewed[key] == [], "Reviewed summary has missing/extra/failed tests")
        need([reviewed[k] for k in ("report_succeeded", "report_succeeded_with_warnings", "report_failed", "report_not_run")] == [37, 6, 0, 0], "Reviewed/raw report counters differ")
        need({w["name"]: w["warnings"] for w in reviewed["warning_tests"]} == warnings and len(reviewed["warning_tests"]) == 6, "Reviewed warning cases differ from raw report")
        for warning in reviewed["warning_tests"]:
            need([dict(e, message=message(e["message"])) for e in warning["events"]] == policy["warnings"][warning["name"]]["events"], "Reviewed warning event differs")
        starts = [(i, START.search(line)[1]) for i, line in enumerate(lines) if START.search(line)]
        ends = [(i, *END.search(line).groups()) for i, line in enumerate(lines) if END.search(line)]
        need(len(starts) == len(ends) == 43 and len({name for _, name in starts}) == 43 and {name for _, name in starts} == set(names), "Raw started/completed test set differs")
        need([name for _, name in starts] == [name for _, state, name in ends] and all(state == "Success" and begin < end for (begin, _), (end, state, _) in zip(starts, ends)), "Raw completions failed/out of order")
        need(all(ends[i][0] < starts[i + 1][0] for i in range(42)), "Raw sequential tests overlap")
        for index, (begin, name) in enumerate(starts):
            stop = starts[index + 1][0] if index + 1 < len(starts) else len(lines)
            emitted, echoed = [], []
            for line in lines[begin:stop]:
                warning = re.search(r"(Log[^\s:]+):\s*Warning:\s*(.*)", line)
                if warning:
                    if warning[1] == "LogAutomationController":
                        echoed.append(message(re.sub(r"\s+\[log\]\s*$", "", warning[2])))
                    else:
                        emitted.append(message(warning[1] + ": " + warning[2]))
            expected_messages = [e["message"] for e in policy["warnings"].get(name, {"events": []})["events"]]
            need(emitted == expected_messages and echoed == expected_messages, "Raw per-test warning/echo inventory differs")
        queues = [i for i, line in enumerate(lines) if re.search(r"LogAutomationCommandLine:\s*Display:\s*\.\.\.Automation Test Queue Empty 43 tests performed\.", line)]
        exits = [(i, match[0]) for i, line in enumerate(lines) for match in re.finditer(r"FPlatformMisc::RequestExit(?:WithStatus)?\([^)]*\)", line)]
        status_exit = "FPlatformMisc::RequestExitWithStatus(1, 0, FEngineLoop::Tick.GScopedTestExit)"
        plain_exit = "FPlatformMisc::RequestExit(1, FEngineLoop::Tick.GScopedTestExit)"
        status_rows = [i for i, value in exits if value == status_exit]
        plain_rows = [i for i, value in exits if value == plain_exit]
        need(len(queues) == len(status_rows) == 1 and len(plain_rows) <= 1 and len(exits) == 1 + len(plain_rows) and ends[-1][0] < queues[0] < status_rows[0] and all(queues[0] < i < status_rows[0] for i in plain_rows), "Exact ordered queue43/zero-status exit absent or contradictory")
        first = starts[0][0]
        startup_red = [line for line in lines[:first] if RED.search(line)]
        need(not any(RED.search(line) for line in lines[first:]), "Test/shutdown red error cannot be approved as startup")
        need(not startup_red, "Native43 startup red rows are forbidden; no startup review allowance")
        return {"status": "validated_native43_numeric_evidence", "tested_project": ctx["project"], "dll_sha256": sha(ctx["dll_sha256"]), "source_config_files": len(current), "native_tests": 43, "success": 43, "warning_cases": 6, "warning_events": 12, "startup_red_rows": len(startup_red), "startup_reviewed": bool(startup_red), "startup_warning_lines": sum(bool(re.search(r"Log[^\s:]+:\s*Warning:", line)) for line in lines[:first]), "evidence": evidence, "UI7": "UNVERIFIED", "executed_here": False, "synthetic_fixture": ctx.get("synthetic_fixture") is True, "gui_input_operations": 0, "limits": ["No actual input/animation/UI/visual certification", "No UserDir/Python/MCP or save-isolation proof", "No map crop/NPC/default-map-change or packaged validation proof"]}
    except ValueError:
        raise
    except (KeyError, TypeError, AttributeError, UnicodeError, OSError, IndexError) as error:
        raise ValueError("Incomplete Native43 evidence: " + str(error)) from error
