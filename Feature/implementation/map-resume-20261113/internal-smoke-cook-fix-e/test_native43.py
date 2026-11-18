"""Pure SYNTHETIC Native43 fixtures; no engine/test process is launched.

Fake approval applies only to the synthetic log/source files in owned scratch.
Never use these fixtures or approval as actual runtime/candidate evidence.
"""
import base64
import copy
import hashlib
import json
import ntpath
import tempfile
import unittest
from pathlib import Path


PACKET = Path(__file__).resolve().parent


def pin(path):
    path = Path(path).resolve()
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest().upper()}


def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=False, indent=2).encode("utf-8"))
    return pin(path)


def load_validator():
    path = PACKET / "native43_validation.py"
    scope = {"__file__": str(path), "__name__": "synthetic_native43_validation"}
    exec(compile(path.read_bytes(), str(path), "exec"), scope)
    return scope["validate_native43"]


def make_native_fixture(base: Path, project: str, engine: str, source_config: dict, dll_sha: str):
    """Return refs/context for fictional raw43 + exact synthetic startup review.

    Writes only new disposable files below base. Caller supplies fake canonical
    source/DLL identities and must label any enclosing fixture SYNTHETIC too.
    """
    base = Path(base).resolve()
    base.mkdir(parents=True, exist_ok=True)
    policy = json.loads((PACKET / "native43-policy.json").read_text(encoding="utf-8"))
    names = policy["tests"]
    exe = engine if engine.lower().endswith(".exe") else ntpath.join(engine, "Engine/Binaries/Win64/UnrealEditor-Cmd.exe")
    selector = "Automation RunTests " + "+".join("^" + name + "$" for name in names)
    command = [exe, project, "/Engine/Maps/Entry", "-unattended", "-NullRHI", "-NoSound", "-NoSplash", "-NoLiveCoding", "-nop4", "-stdout", "-FullStdOutLogOutput", "-ExecCmds=" + selector, "-TestExit=Automation Test Queue Empty", "-ReportExportPath=" + str(base / "report"), "-abslog=" + str(base / "editor.log")]
    record = {"status": "running", "started_utc": "2026-11-13T12:33:24+00:00", "dll_sha256": dll_sha, "selection_sha256": policy["selection_sha256"], "tests": names, "source_config_sha256": source_config, "command": command, "gui_input_operations": 0, "boundary": "SYNTHETIC offline fixture only", "pid": 41244}
    result = dict(record, status="headless_editor_exited_report_review_required", exit_code=0, ended_utc="2026-11-13T12:35:40+00:00", elapsed_seconds=136, same_dll=True, source_config_unchanged=True)
    report = dict(policy["report"], devices=[{"instance": "SYNTHETIC_DEVICE", "instanceName": "SYNTHETIC-41244", "platform": "WindowsEditor"}], tests=[])
    actual_command = " ".join(command[2:11]) + ' -ExecCmds="' + selector + '" -TestExit="Automation Test Queue Empty" -ReportExportPath=' + str(base / "report") + ' -abslog=' + str(base / "editor.log")
    startup_rows = [f"[2026.11.13-12.33.30:000][0]LogAutomationTest: Error: SYNTHETIC startup row {i}" for i in range(15)]
    log = ["LogInit: Command Line: " + actual_command, *startup_rows]
    reviewed_warnings = []
    for name in names:
        expected = policy["warnings"].get(name, {"count": 0, "events": []})
        events = [dict(e, message=e["message"].replace("<GUID>", "A" * 32)) for e in expected["events"]]
        report["tests"].append({"fullTestPath": name, "testDisplayName": name.rsplit(".", 1)[-1], "state": "Success", "warnings": expected["count"], "errors": 0, "deviceInstance": ["SYNTHETIC_DEVICE"], "entries": [{"event": event} for event in events], "artifacts": []})
        if events:
            reviewed_warnings.append({"name": name, "warnings": expected["count"], "events": events})
        log.append("LogAutomationController: Display: Test Started. Name={Synthetic} Path={" + name + "}")
        for event in events:
            log.append(event["message"].replace(": ", ": Warning: ", 1))
        log.append("LogAutomationController: Display: Test Completed. Result={Success} Name={Synthetic} Path={" + name + "}")
        for event in events:
            log.append("LogAutomationController: Warning: " + event["message"] + " [log] ")
    log += ["LogAutomationCommandLine: Display: ...Automation Test Queue Empty 43 tests performed.", "LogWindows: FPlatformMisc::RequestExitWithStatus(1, 0, FEngineLoop::Tick.GScopedTestExit)"]
    reviewed = {"completed": 43, "expected_count": 43, "expected_names_exact": True, "states": {"Success": 43}, "missing": [], "extra": [], "failed": [], "engine_queue_empty_43": True, "source_config_path_set_unchanged": True, "report_succeeded": 37, "report_succeeded_with_warnings": 6, "report_failed": 0, "report_not_run": 0, "warning_tests": reviewed_warnings}
    refs = {"process": write(base / "process.json", record), "result": write(base / "result.json", result), "reviewed": write(base / "reviewed.json", reviewed), "report": write(base / "report/index.json", report), "log": write(base / "editor.log", ("\n".join(log) + "\n").encode()), "selection": write(base / "selection.json", base64.b64decode(policy["selection_document_utf8_base64"]))}
    source = write(base / "SYNTHETIC-engine-proof.cpp", b"// SYNTHETIC fixture data, not installed-engine evidence\n")
    approval = {"schema": "R10-native-startup-review-v1", "status": "reviewed_exact_engine_startup", "log_sha256": refs["log"]["sha256"], "exact_red_lines": startup_rows, "reviewer": "SYNTHETIC_TEST_ONLY", "reason": "SYNTHETIC exact-row approval for offline counterexamples, NOT actual engine review", "source_evidence": [source], "synthetic_only": True}
    refs["startup_review"] = write(base / "SYNTHETIC-STARTUP-REVIEW.json", approval)
    ctx = {"root": Path(project).parent, "project": project, "engine": engine, "dll_sha256": dll_sha, "source_config": source_config, "editor_finished_utc": "2026-11-13T12:31:48+00:00", "read_pin": lambda item: Path(item["path"]).read_bytes(), "policy": policy, "synthetic_fixture": True}
    return refs, ctx


class Native43OfflineTests(unittest.TestCase):
    def setUp(self):
        scratch = PACKET / "scratch"
        scratch.mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=scratch, prefix="native43-synthetic-")
        self.addCleanup(self.tmp.cleanup)
        base = Path(self.tmp.name)
        self.refs, self.ctx = make_native_fixture(base / "qa", str(base / "candidate/Rogue10m.uproject"), str(base / "engine"), {"Source/Test.cpp": "A" * 64, "Config/DefaultEngine.ini": "B" * 64}, "C" * 64)
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
        self.change("startup_review", lambda d: d.update(log_sha256=self.refs["log"]["sha256"]))

    def test_positive_is_only_synthetic_numeric_evidence(self):
        result = self.validate(self.refs, self.ctx)
        self.assertEqual(result["native_tests"], 43)
        self.assertEqual(result["UI7"], "UNVERIFIED")
        self.assertFalse(result["executed_here"])

    def test_missing_startup_review_blocks(self):
        self.refs.pop("startup_review")
        with self.assertRaisesRegex(ValueError, "explicit pinned owner review"):
            self.validate(self.refs, self.ctx)

    def test_not_approved_sample_cannot_pass(self):
        self.change("startup_review", lambda d: d.update(status="NOT_APPROVED"))
        with self.assertRaises(ValueError): self.validate(self.refs, self.ctx)

    def test_test_phase_error_cannot_be_startup_approved(self):
        old = self.refs["log"]
        self.refs["log"] = write(old["path"], Path(old["path"]).read_bytes() + b"LogRogue10m: Error: injected after tests\n")
        with self.assertRaisesRegex(ValueError, "Test/shutdown"):
            self.validate(self.refs, self.ctx)

    def test_dll_mismatch_blocks(self):
        self.change("result", lambda d: d.update(dll_sha256="D" * 64))
        with self.assertRaises(ValueError): self.validate(self.refs, self.ctx)

    def test_wrong_test_id_blocks(self):
        self.change("report", lambda d: d["tests"][0].update(fullTestPath="Rogue10m.Wrong"))
        with self.assertRaises(ValueError): self.validate(self.refs, self.ctx)

    def test_warning_count_drift_blocks(self):
        self.change("report", lambda d: d["tests"][0].update(warnings=1))
        with self.assertRaises(ValueError): self.validate(self.refs, self.ctx)

    def test_source_identity_collision_blocks(self):
        self.change("result", lambda d: d["source_config_sha256"].update({"Source\\\\Test.cpp": "A" * 64}))
        with self.assertRaises(ValueError): self.validate(self.refs, self.ctx)

    def test_new_project_is_not_historical_execution(self):
        self.ctx["project"] = str(Path(self.ctx["root"]) / "Different.uproject")
        with self.assertRaises(ValueError): self.validate(self.refs, self.ctx)

    def test_raw_tamper_blocks(self):
        p = Path(self.refs["report"]["path"])
        p.write_bytes(p.read_bytes() + b" ")
        with self.assertRaises(ValueError): self.validate(self.refs, self.ctx)

    def test_queue_and_exit_before_final_completion_blocks(self):
        def reorder(lines):
            terminal = lines[-2:]
            del lines[-2:]
            lines[1:1] = terminal
        self.change_log(reorder)
        with self.assertRaisesRegex(ValueError, "ordered queue43"):
            self.validate(self.refs, self.ctx)

    def test_contradictory_exit_blocks(self):
        self.change_log(lambda lines: lines.append("LogWindows: FPlatformMisc::RequestExitWithStatus(1, 9, FEngineLoop::Tick.GScopedTestExit)"))
        with self.assertRaisesRegex(ValueError, "contradictory"):
            self.validate(self.refs, self.ctx)

    def test_overlapping_test_sequence_blocks(self):
        def overlap(lines):
            end = next(i for i, line in enumerate(lines) if "Test Completed." in line)
            start = next(i for i in range(end + 1, len(lines)) if "Test Started." in lines[i])
            lines[end], lines[start] = lines[start], lines[end]
        self.change_log(overlap)
        with self.assertRaisesRegex(ValueError, "overlap"):
            self.validate(self.refs, self.ctx)


if __name__ == "__main__":
    unittest.main()
