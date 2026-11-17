"""Pure SYNTHETIC Native51 fixtures. Never engine/build/game evidence.

All fake receipts/logs below disposable owned scratch, with no approval files,
process APIs, GUI actions or executable invocation. Full-smoke callers should
pin the returned ctx.policy, whose fake receipt/selection hashes intentionally
differ from the retained real policy. Supply NEW_SOURCE in source_config.
"""
import base64
import copy
import hashlib
import json
import ntpath
import re
import tempfile
import unittest
from pathlib import Path


PACKET = Path(__file__).resolve().parent
NEW_SOURCE = "Source/Rogue10m/Tests/Rogue10mVillageQuestCombatCreditTests.cpp"


def pin(path):
    path = Path(path).resolve()
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest().upper()}


def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=False, indent=2).encode("utf-8"))
    return pin(path)


def load_validator():
    module = PACKET / "native51_validation.py"
    scope = {"__file__": str(module), "__name__": "synthetic_native51_validation"}
    exec(compile(module.read_bytes(), str(module), "exec"), scope)
    return scope["validate_native51"]


def make_native51_fixture(base: Path, project: str, engine: str, source_config: dict, dll_sha: str):
    """Write fictional evidence only below base, return nine refs and fake ctx.

    The caller supplies the full fake source map, including NEW_SOURCE. The
    returned policy must be used by enclosing synthetic fixtures; it cannot
    validate any real Native51 build/log as synthetic execution evidence.
    """
    base = Path(base).resolve()
    base.mkdir(parents=True, exist_ok=True)
    policy = json.loads((PACKET / "native51-policy.json").read_text(encoding="utf-8"))
    names = policy["tests"]
    normalized_sources = {re.sub(r"[\\/]+", "/", key).casefold(): value for key, value in source_config.items()}
    if NEW_SOURCE.casefold() not in normalized_sources:
        raise ValueError("Synthetic full source map must include " + NEW_SOURCE)
    source_sha = normalized_sources[NEW_SOURCE.casefold()]
    selection = json.loads(base64.b64decode(policy["selection_document_utf8_base64"]).decode("utf-8-sig"))
    selection["source_new_tests_sha256"] = source_sha
    refs = {"selection": write(base / "selection.SYNTHETIC.json", selection)}
    policy.update(selection_sha256=refs["selection"]["sha256"], source_new_tests_sha256=source_sha, observed_dll_sha256=dll_sha, source_config_count_observed=len(source_config))
    policy["selection_document_utf8_base64"] = base64.b64encode(Path(refs["selection"]["path"]).read_bytes()).decode("ascii")
    build = {"started_utc": "2026-11-13T17:52:09+00:00", "ended_utc": "2026-11-13T17:52:51+00:00", "dll_sha256": dll_sha, "exit_code": 0, "gui_input_operations": 0, "metadata_approval_or_clone_QA_retried": False, "purpose": "SYNTHETIC fixture only; not a UE build"}
    refs["build_result"] = write(base / "build-result.SYNTHETIC.json", build)
    refs["launcher"] = write(base / "launcher.SYNTHETIC.py", b"# SYNTHETIC inert bytes, never executed\n")
    policy.update(build_result_sha256=refs["build_result"]["sha256"], launcher_sha256=refs["launcher"]["sha256"])
    exe = engine if engine.lower().endswith(".exe") else ntpath.join(engine, "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe")
    selector = "Automation RunTests " + "+".join("^" + name + "$" for name in names)
    command = [exe, project, "/Engine/Maps/Entry", "-unattended", "-NullRHI", "-NoSound", "-NoSplash", "-NoLiveCoding", "-nop4", "-stdout", "-FullStdOutLogOutput", "-ExecCmds=" + selector, "-TestExit=Automation Test Queue Empty", "-ReportExportPath=" + str(base / "report"), "-abslog=" + str(base / "editor.log"), "-culture=en", "-language=en", "-locale=en-US"]
    process = {"status": "running", "purpose": "SYNTHETIC fixture only; no process launched", "started_utc": "2026-11-13T17:54:09+00:00", "dll_sha256": dll_sha, "source_config_sha256": copy.deepcopy(source_config), "tests": names, "command": command, "count": 51, "npc_cases": 26, "gui_input_operations": 0, "metadata_approval_or_clone_QA_retried": False, "ordinary_cpp_fixture_UI_asset_loading_not_excluded": True, "build_receipt_sha256": refs["build_result"]["sha256"], "launcher_sha256": refs["launcher"]["sha256"], "pid": 17712, "synthetic_only": True}
    result = dict(process, status="headless_editor_exited_review_required", exit_code=0, elapsed_seconds=120, ended_utc="2026-11-13T17:56:10+00:00", same_dll=True, source_config_unchanged=True)
    report = dict(policy["report"], devices=[{"instance": "SYNTHETIC_DEVICE", "instanceName": "SYNTHETIC-17712", "platform": "WindowsEditor"}], reportCreatedOn="2026.11.13-17.56.09", tests=[])
    actual = " ".join(command[2:11]) + ' -ExecCmds="' + selector + '" -TestExit="Automation Test Queue Empty" -ReportExportPath=' + str(base / "report") + ' -abslog=' + str(base / "editor.log") + " " + " ".join(command[15:])
    log = ["SYNTHETIC offline fixture, not actual UE output", "LogInit: Command Line: " + actual]
    for message in policy["startup_warning_messages"]:
        log.append(message.replace(": ", ": Warning: ", 1))
    warning_cases = []
    # Intentionally differs from selection ordering, as actual report/log do.
    for name in sorted(names):
        expected = policy["warnings"].get(name, {"count": 0, "entries": []})
        entries = json.loads(json.dumps(expected["entries"]).replace("<GUID>", "A" * 32))
        report["tests"].append({"fullTestPath": name, "state": "Success", "deviceInstance": ["SYNTHETIC_DEVICE"], "warnings": expected["count"], "errors": 0, "entries": entries})
        if entries:
            warning_cases.append({"test": name, "warnings": expected["count"], "events": entries})
        log.append("LogAutomationController: Display: Test Started. Name={Synthetic} Path={" + name + "}")
        for entry in entries:
            log.append(entry["event"]["message"].replace(": ", ": Warning: ", 1))
        log.append("LogAutomationController: Display: Test Completed. Result={Success} Name={Synthetic} Path={" + name + "}")
        for entry in entries:
            log.append("LogAutomationController: Warning: " + entry["event"]["message"] + " [log] ")
    log += ["LogAutomationCommandLine: Display: ...Automation Test Queue Empty 51 tests performed.", "LogWindows: FPlatformMisc::RequestExit(1, FEngineLoop::Tick.GScopedTestExit)", "LogWindows: FPlatformMisc::RequestExitWithStatus(1, 0, FEngineLoop::Tick.GScopedTestExit)"]
    review = {"status": "reviewed_native51_cpp_numeric_regression", "count": 51, "npc_cases": 26, "new_combat_credit_cases": 8, "success": 51, "failed": [], "expected_names_exact": True, "completed_log_count": 51, "completed_log_names_exact": True, "queue51": True, "whole_log_red_count": 0, "red_rows": [], "report_succeeded": 40, "report_succeeded_with_warnings": 11, "warning_cases": warning_cases, "warning_event_count": 19, "source_config_files": len(source_config), "source_config_unchanged": True, "dll_sha256": dll_sha, "game_execution": 0, "gui_input_operations": 0, "metadata_approval_or_clone_QA_retried": False, "limits": ["SYNTHETIC fixture only"]}
    refs.update(process=write(base / "process.SYNTHETIC.json", process), result=write(base / "result.SYNTHETIC.json", result), reviewed=write(base / "reviewed.SYNTHETIC.json", review), report=write(base / "report/index.json", report), log=write(base / "editor.log", ("\n".join(log) + "\n").encode("utf-8")), preservation=write(base / "preservation.SYNTHETIC.json", dict(policy["preservation"], verified_utc="2026-11-13T18:02:39+00:00", synthetic_only=True)))
    ctx = {"root": Path(project).parent, "project": project, "engine": engine, "dll_sha256": dll_sha, "source_config": copy.deepcopy(source_config), "editor_finished_utc": build["ended_utc"], "read_pin": lambda item: Path(item["path"]).read_bytes(), "policy": policy, "synthetic_fixture": True}
    return refs, ctx


class Native51OfflineTests(unittest.TestCase):
    def setUp(self):
        scratch = PACKET / "scratch"
        scratch.mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=scratch, prefix="native51-SYNTHETIC-")
        self.addCleanup(self.tmp.cleanup)
        base = Path(self.tmp.name)
        sources = {NEW_SOURCE: "A" * 64, "Source/Test.cpp": "B" * 64, "Config/DefaultEngine.ini": "C" * 64}
        self.refs, self.ctx = make_native51_fixture(base / "qa", str(base / "candidate/Rogue10m.uproject"), str(base / "engine"), sources, "D" * 64)
        self.validate = load_validator()

    def change(self, role, mutate):
        old = self.refs[role]
        value = json.loads(Path(old["path"]).read_text(encoding="utf-8"))
        mutate(value)
        self.refs[role] = write(old["path"], value)

    def change_log(self, mutate):
        old = self.refs["log"]
        lines = Path(old["path"]).read_text(encoding="utf-8").splitlines()
        mutate(lines)
        self.refs["log"] = write(old["path"], ("\n".join(lines) + "\n").encode("utf-8"))

    def test_positive_is_only_synthetic_numeric_evidence(self):
        summary = self.validate(self.refs, self.ctx)
        self.assertEqual(summary["native_tests"], 51)
        self.assertEqual(summary["warning_events"], 19)
        self.assertEqual(summary["UI"], "UNVERIFIED")
        self.assertEqual(summary["save_routing"], "UNVERIFIED")
        self.assertFalse(summary["executed_here"])
        self.assertFalse(summary["oldE5_contains_fix"])

    def test_any_startup_review_key_rejected(self):
        self.refs["startup_review"] = None
        with self.assertRaisesRegex(ValueError, "forbids startup_review"):
            self.validate(self.refs, self.ctx)

    def test_startup_error_rejected_without_allowance(self):
        self.change_log(lambda rows: rows.insert(1, "LogAutomationTest: Error: SYNTHETIC startup failure"))
        with self.assertRaisesRegex(ValueError, "whole-log red"):
            self.validate(self.refs, self.ctx)

    def test_shutdown_error_rejected(self):
        self.change_log(lambda rows: rows.append("LogRogue10m: Error: SYNTHETIC late failure"))
        with self.assertRaisesRegex(ValueError, "whole-log red"):
            self.validate(self.refs, self.ctx)

    def test_dll_mismatch_rejected(self):
        self.ctx["dll_sha256"] = "E" * 64
        with self.assertRaisesRegex(ValueError, "DLL/full Source"):
            self.validate(self.refs, self.ctx)

    def test_source_identity_collision_rejected(self):
        self.ctx["source_config"]["Source\\\\Test.cpp"] = "B" * 64
        with self.assertRaisesRegex(ValueError, "Colliding"):
            self.validate(self.refs, self.ctx)

    def test_new_source_selection_binding_rejected(self):
        self.ctx["source_config"][NEW_SOURCE] = "F" * 64
        with self.assertRaisesRegex(ValueError, "current new test source"):
            self.validate(self.refs, self.ctx)

    def test_raw_tamper_rejected(self):
        report = Path(self.refs["report"]["path"])
        report.write_bytes(report.read_bytes() + b" ")
        with self.assertRaisesRegex(ValueError, "bytes changed"):
            self.validate(self.refs, self.ctx)

    def test_missing_name_rejected(self):
        self.change("report", lambda data: data["tests"][0].update(fullTestPath="Rogue10m.Wrong"))
        with self.assertRaisesRegex(ValueError, "missing/extra/duplicate"):
            self.validate(self.refs, self.ctx)

    def test_warning_wrapper_filename_rejected(self):
        self.change("report", lambda data: next(row for row in data["tests"] if row["warnings"])["entries"][0].update(filename="wrong.cpp"))
        with self.assertRaisesRegex(ValueError, "full warning wrapper"):
            self.validate(self.refs, self.ctx)

    def test_warning_wrapper_timestamp_rejected(self):
        self.change("report", lambda data: next(row for row in data["tests"] if row["warnings"])["entries"][0].update(timestamp="2099.01.01-00.00.00"))
        with self.assertRaisesRegex(ValueError, "full warning wrapper"):
            self.validate(self.refs, self.ctx)

    def test_raw_report_guid_placeholder_rejected(self):
        def placeholder(data):
            entry = next(row for row in data["tests"] if row["warnings"])["entries"][0]["event"]
            entry["message"] = entry["message"].replace("A" * 32, "<GUID>")
        self.change("report", placeholder)
        with self.assertRaisesRegex(ValueError, "Malformed raw dynamic world GUID"):
            self.validate(self.refs, self.ctx)

    def test_raw_log_guid_placeholder_rejected(self):
        self.change_log(lambda rows: rows.__setitem__(slice(None), [row.replace("A" * 32, "<GUID>") for row in rows]))
        with self.assertRaisesRegex(ValueError, "Malformed raw dynamic world GUID"):
            self.validate(self.refs, self.ctx)

    def test_warning_actor_order_rejected(self):
        def reorder(data):
            row = next(row for row in data["tests"] if "OwnerSwap" in row["fullTestPath"])
            row["entries"][1], row["entries"][2] = row["entries"][2], row["entries"][1]
        self.change("report", reorder)
        with self.assertRaisesRegex(ValueError, "full warning wrapper"):
            self.validate(self.refs, self.ctx)

    def test_build_receipt_binding_rejected(self):
        self.change("build_result", lambda data: data.update(exit_code=9))
        with self.assertRaisesRegex(ValueError, "reviewed build/launcher"):
            self.validate(self.refs, self.ctx)

    def test_metadata_retry_claim_rejected(self):
        for role in ("process", "result"):
            self.change(role, lambda data: data.update(metadata_approval_or_clone_QA_retried=True))
        with self.assertRaisesRegex(ValueError, "execution boundary"):
            self.validate(self.refs, self.ctx)

    def test_relocated_project_not_historical_execution(self):
        self.ctx["project"] = str(Path(self.ctx["root"]) / "Other.uproject")
        with self.assertRaisesRegex(ValueError, "command project"):
            self.validate(self.refs, self.ctx)

    def test_terminal_markers_before_tests_rejected(self):
        def reorder(rows):
            terminal = rows[-3:]
            del rows[-3:]
            rows[1:1] = terminal
        self.change_log(reorder)
        with self.assertRaisesRegex(ValueError, "final queue51"):
            self.validate(self.refs, self.ctx)

    def test_contradictory_exit_rejected(self):
        self.change_log(lambda rows: rows.append("LogWindows: FPlatformMisc::RequestExitWithStatus(1, 9, FEngineLoop::Tick.GScopedTestExit)"))
        with self.assertRaisesRegex(ValueError, "final queue51"):
            self.validate(self.refs, self.ctx)

    def test_overlapping_tests_rejected(self):
        def overlap(rows):
            end = next(i for i, row in enumerate(rows) if "Test Completed." in row)
            start = next(i for i in range(end + 1, len(rows)) if "Test Started." in rows[i])
            rows[end], rows[start] = rows[start], rows[end]
        self.change_log(overlap)
        with self.assertRaisesRegex(ValueError, "overlap/out of order"):
            self.validate(self.refs, self.ctx)

    def test_old_e5_fix_claim_rejected(self):
        self.change("preservation", lambda data: data.update(old_Windows_E5_contains_this_NPC_fix=True))
        with self.assertRaisesRegex(ValueError, "Historical preservation"):
            self.validate(self.refs, self.ctx)


if __name__ == "__main__":
    unittest.main(verbosity=2)
