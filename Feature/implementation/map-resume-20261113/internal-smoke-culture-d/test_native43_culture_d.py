"""Culture-D SYNTHETIC counterexamples; no subprocess, Unreal, GUI or build.

Run this file explicitly. Preserved upstream tests describe the old policy.
All generated fixtures are retained under one fresh staging/evidence directory.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest
from zipfile import ZipFile


PACKET = Path(__file__).resolve().parent
UPSTREAM_ZIP = Path(r"C:\Users\PC\Documents\Codex\2026-11-12\task\NonGuiSmoke20261113A.delta.zip")
UPSTREAM_SHA = "BADA1140C01AD87C70509F9185C156569186586136071910D48AAAB03A65484F"
CULTURE = ["-culture=en", "-language=en", "-locale=en-US"]
EVIDENCE = None
SUBCASES = 0


def module_scope(data, filename):
    scope = {"__file__": str(filename), "__name__": "culture_d_synthetic_fixture"}
    exec(compile(data, str(filename), "exec"), scope)
    return scope


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def evidence_path(value):
    requested = Path(value)
    if not requested.is_absolute() or any(part in (".", "..") for part in str(value).replace("\\", "/").split("/")):
        raise ValueError("Canonical absolute evidence path without traversal required")
    target = requested.absolute()
    allowed = (PACKET / "evidence").absolute()
    if (not target.is_relative_to(allowed) or target == allowed or target.exists()
            or not target.resolve(strict=False).is_relative_to(allowed.resolve(strict=False))):
        raise ValueError("A fresh child of staging/evidence is required")
    for parent in target.parents:
        if parent.exists():
            info = parent.lstat()
            if parent.is_symlink() or getattr(info, "st_file_attributes", 0) & 0x400 or parent.resolve() != parent:
                raise ValueError("Physical staging evidence ancestors required")
    return target


class CultureDTests(unittest.TestCase):
    def setUp(self):
        self.case = EVIDENCE / self._testMethodName
        self.case.mkdir(exist_ok=False)
        scope = module_scope((PACKET / "test_native43.py").read_bytes(), PACKET / "test_native43.py")
        self.write = scope["write"]
        self.refs, self.ctx = scope["make_native_fixture"](
            self.case / "qa", str(self.case / "candidate/Rogue10m.uproject"),
            str(self.case / "engine"), {"Source/Test.cpp": "A" * 64, "Config/DefaultEngine.ini": "B" * 64}, "C" * 64)
        self.validate = module_scope((PACKET / "native43_validation.py").read_bytes(),
                                     PACKET / "native43_validation.py")["validate_native43"]
        self.original_lines = self.lines()
        self.original_command = self.json("process")["command"]

    def json(self, role):
        return json.loads(Path(self.refs[role]["path"]).read_text(encoding="utf-8"))

    def change(self, role, mutate):
        value = self.json(role)
        mutate(value)
        self.refs[role] = self.write(self.refs[role]["path"], value)

    def lines(self):
        return Path(self.refs["log"]["path"]).read_text(encoding="utf-8").splitlines()

    def set_lines(self, lines):
        self.refs["log"] = self.write(self.refs["log"]["path"], ("\n".join(lines) + "\n").encode("utf-8"))
        if "startup_review" in self.refs:
            self.change("startup_review", lambda value: value.update(
                log_sha256=self.refs["log"]["sha256"],
                exact_red_lines=[line for line in lines if "SYNTHETIC startup row" in line]))

    @staticmethod
    def actual_command(command):
        selector = command[11].removeprefix("-ExecCmds=")
        return (" ".join(command[2:11]) + ' -ExecCmds="' + selector +
                '" -TestExit="Automation Test Queue Empty" -ReportExportPath=' +
                command[13].split("=", 1)[1] + " -abslog=" + command[14].split("=", 1)[1] +
                (" " + " ".join(command[15:]) if command[15:] else ""))

    def command(self, tail=CULTURE, clean=True):
        command = self.original_command[:15] + list(tail)
        for role in ("process", "result"):
            self.change(role, lambda value: value.update(command=copy.deepcopy(command)))
        lines = [line for line in self.original_lines if not clean or "SYNTHETIC startup row" not in line]
        lines[0] = "LogInit: Command Line: " + self.actual_command(command)
        self.set_lines(lines)
        return command

    def test_clean_exact18_is_numeric_only(self):
        self.command()
        result = self.validate(self.refs, self.ctx)
        self.assertEqual(result["native_tests"], 43)
        self.assertEqual(result["startup_red_rows"], 0)
        self.assertFalse(result["startup_reviewed"])
        self.assertEqual(result["warning_cases"], 6)
        self.assertEqual(result["warning_events"], 12)
        self.assertEqual(result["UI7"], "UNVERIFIED")
        self.assertFalse(result["executed_here"])
        self.assertTrue(result["synthetic_fixture"])

    def test_clean_exact18_needs_no_startup_review(self):
        self.command()
        self.refs.pop("startup_review")
        self.assertEqual(self.validate(self.refs, self.ctx)["startup_red_rows"], 0)

    def test_old15_positive_upstream_is_rejected_by_derivative(self):
        self.assertEqual(digest(UPSTREAM_ZIP.read_bytes()), UPSTREAM_SHA)
        with ZipFile(UPSTREAM_ZIP) as archive:
            old = archive.read("NonGuiSmoke20261113A/native43_validation.py")
        upstream = module_scope(old, PACKET / "UPSTREAM-native43-not-executed.py")["validate_native43"]
        self.assertEqual(upstream(self.refs, self.ctx)["native_tests"], 43)
        with self.assertRaisesRegex(ValueError, "18-argument"):
            self.validate(self.refs, self.ctx)

    def test_18args_and_valid_synthetic_approval_never_allow_startup15(self):
        self.command(clean=False)
        self.assertEqual(self.json("startup_review")["status"], "reviewed_exact_engine_startup")
        with self.assertRaisesRegex(ValueError, "startup red rows are forbidden"):
            self.validate(self.refs, self.ctx)

    def test_not_approved_sample_does_not_allow_startup15(self):
        self.command(clean=False)
        self.change("startup_review", lambda value: value.update(status="NOT_APPROVED"))
        with self.assertRaisesRegex(ValueError, "startup red rows are forbidden"):
            self.validate(self.refs, self.ctx)

    def test_one_or_many_startup_red_rows_are_forbidden(self):
        global SUBCASES
        self.command()
        clean = self.lines()
        for red in ("LogAutomationTest: Error: Condition failed", "LogExample: Fatal: synthetic",
                    "Handled ensure condition failed: synthetic", "Traceback (most recent call last):"):
            with self.subTest(red=red):
                SUBCASES += 1
                lines = clean[:1] + [red] + clean[1:]
                self.set_lines(lines)
                with self.assertRaisesRegex(ValueError, "startup red rows are forbidden"):
                    self.validate(self.refs, self.ctx)

    def test_missing_extra_duplicate_order_case_and_value_tails_fail(self):
        global SUBCASES
        tails = [[], CULTURE[:2], CULTURE + ["-culture=en"], CULTURE[::-1],
                 ["-culture=ko", CULTURE[1], CULTURE[2]],
                 ["-culture=EN", CULTURE[1], CULTURE[2]],
                 [CULTURE[0], "-language=EN", CULTURE[2]],
                 [CULTURE[0], CULTURE[1], "-locale=en-us"],
                 ["-Culture=en", CULTURE[1], CULTURE[2]],
                 [CULTURE[0], CULTURE[1], "-locale=en-US "],
                 [CULTURE[0], CULTURE[1], "-locale=en-US -NoSmokeTests"]]
        for tail in tails:
            with self.subTest(tail=tail):
                SUBCASES += 1
                self.command(tail)
                with self.assertRaisesRegex(ValueError, "18-argument|culture tail"):
                    self.validate(self.refs, self.ctx)

    def test_actual_log_tail_drift_is_not_hidden_by_correct_receipts(self):
        global SUBCASES
        for suffix in ("", "-culture=en -locale=en-US -language=en", "-culture=en -language=en -locale=ko-KR",
                       "-culture=en -language=en -locale=en-US -culture=en"):
            with self.subTest(suffix=suffix):
                SUBCASES += 1
                command = self.command()
                lines = self.lines()
                lines[0] = "LogInit: Command Line: " + self.actual_command(command[:15]) + (" " + suffix if suffix else "")
                self.set_lines(lines)
                with self.assertRaisesRegex(ValueError, "Actual engine command differs"):
                    self.validate(self.refs, self.ctx)

    def test_receipt_record_culture_cannot_be_added_without_log_tail(self):
        self.command()
        lines = self.lines()
        lines[0] = "LogInit: Command Line: " + self.actual_command(self.original_command)
        self.set_lines(lines)
        with self.assertRaisesRegex(ValueError, "Actual engine command differs"):
            self.validate(self.refs, self.ctx)

    def test_existing_flags_and_selector_remain_protected(self):
        self.command()
        for role in ("process", "result"):
            self.change(role, lambda value: value["command"].__setitem__(7, "-NoSmokeTests"))
        with self.assertRaisesRegex(ValueError, "command flags"):
            self.validate(self.refs, self.ctx)
        self.command()
        for role in ("process", "result"):
            self.change(role, lambda value: value["command"].__setitem__(11, "-ExecCmds=Automation RunTests Rogue10m.Wrong"))
        with self.assertRaisesRegex(ValueError, "Non-exact native selector"):
            self.validate(self.refs, self.ctx)

    def test_dll_and_source_snapshot_remain_protected(self):
        self.command()
        self.change("result", lambda value: value.update(dll_sha256="D" * 64))
        with self.assertRaisesRegex(ValueError, "DLL/source/config"):
            self.validate(self.refs, self.ctx)
        self.change("result", lambda value: value.update(dll_sha256="C" * 64))
        self.change("result", lambda value: value["source_config_sha256"].update({"Source/Test.cpp": "D" * 64}))
        with self.assertRaisesRegex(ValueError, "DLL/source/config"):
            self.validate(self.refs, self.ctx)

    def test_exact43_ids_remain_protected(self):
        self.command()
        self.change("report", lambda value: value["tests"][0].update(fullTestPath="Rogue10m.Wrong"))
        with self.assertRaises(ValueError):
            self.validate(self.refs, self.ctx)

    def test_warning_inventory_remains_protected(self):
        self.command()
        self.change("report", lambda value: value["tests"][0].update(warnings=1))
        with self.assertRaises(ValueError):
            self.validate(self.refs, self.ctx)

    def test_test_and_shutdown_red_remain_forbidden(self):
        self.command()
        self.set_lines(self.lines() + ["LogRogue10m: Error: SYNTHETIC shutdown red"])
        with self.assertRaisesRegex(ValueError, "Test/shutdown"):
            self.validate(self.refs, self.ctx)

    def test_queue_and_exit_order_remain_protected(self):
        self.command()
        lines = self.lines()
        terminal = lines[-2:]
        del lines[-2:]
        lines[1:1] = terminal
        self.set_lines(lines)
        with self.assertRaisesRegex(ValueError, "ordered queue43"):
            self.validate(self.refs, self.ctx)

    def test_nonzero_and_duplicate_exit_remain_forbidden(self):
        self.command()
        self.set_lines(self.lines() + ["LogWindows: FPlatformMisc::RequestExitWithStatus(1, 9, FEngineLoop::Tick.GScopedTestExit)"])
        with self.assertRaisesRegex(ValueError, "contradictory"):
            self.validate(self.refs, self.ctx)

    def test_evidence_root_traversal_is_rejected_before_any_write(self):
        with self.assertRaisesRegex(ValueError, "without traversal"):
            evidence_path(str(PACKET / "evidence") + "/nonexistent/../../outside")
        with self.assertRaisesRegex(ValueError, "Canonical absolute"):
            evidence_path("relative/evidence")
        with self.assertRaisesRegex(ValueError, "fresh child"):
            evidence_path(str(PACKET.parent / "outside"))


def main():
    global EVIDENCE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-root", required=True)
    arguments = parser.parse_args()
    EVIDENCE = evidence_path(arguments.evidence_root)
    EVIDENCE.mkdir(parents=True, exist_ok=False)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(CultureDTests))
    report = {"schema": "R10-culture-d-offline-counterexamples-v1", "synthetic_only": True,
              "tests": result.testsRun, "additional_parameterized_subcases": SUBCASES,
              "failures": len(result.failures), "errors": len(result.errors), "passed": result.wasSuccessful(),
              "validator_sha256": digest((PACKET / "native43_validation.py").read_bytes()),
              "upstream_zip_sha256": digest(UPSTREAM_ZIP.read_bytes()), "fixtures_retained": True,
              "ue_ubt_uat_cook_game_gui_mcp_subprocess_calls": 0, "actual_native43B_validated": False,
              "UI7": "UNVERIFIED", "visual_review": "UNVERIFIED", "release_eligible": False}
    with (EVIDENCE / "validation.json").open("x", encoding="utf-8", newline="\n") as output:
        json.dump(report, output, indent=2); output.write("\n")
    print(json.dumps(report, indent=2))
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
