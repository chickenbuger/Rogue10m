"""Offline checks for real severity parsing, interval checks, discovery and rotation."""
import contextlib
from datetime import datetime, timezone
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("output_log", Path(__file__).resolve().parents[1] / "CheckUnrealOutputLog.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


def row(message, time="12.00.00:000"):
    return f"[2026.11.16-{time}][  0]{message}\n"


class OutputLogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.log = self.root / "session.log"

    def write(self, contents):
        self.log.write_text(contents, encoding="utf-8")

    def test_severity_and_multiline_traceback(self):
        self.write(row("LogTemp: Error test: this is normal logging")
                   + row("LogWindows: Warning: Failed to load dll (GetLastError=126)")
                   + row("LogPython: Error: Traceback (most recent call last):")
                   + row("LogPython: Error: RuntimeError: 한국어 실패")
                   + row("LogOutputDevice: Error: Ensure condition failed: Ready")
                   + row("LoadErrors: Failed to load /Game/Missing")
                   + row("LogScript: Warning: Blueprint Runtime Error: Accessed None")
                   + "Fatal error: crash\n")
        result = checker.inspect(self.log)
        self.assertEqual(result["new_error_count"], 6)
        self.assertEqual(result["new_errors"][1]["message"], "RuntimeError: 한국어 실패")
        self.assertEqual(result["new_errors"][0]["line"], 3)
        self.assertEqual(result["new_errors"][-1]["severity"], "Fatal")

    def test_load_page_heading_warning_and_display_are_not_red_errors(self):
        self.write(row("LoadErrors: Warning: Missing optional resource")
                   + row("LoadErrors: Display: Validation summary")
                   + row("LoadErrors: New Page: Loading map: StartHub.umap")
                   + row("LoadErrors: Error: Failed required load"))
        result = checker.inspect(self.log)
        self.assertEqual(result["new_error_count"], 1)
        self.assertEqual(result["new_errors"][0]["line"], 4)

    def test_existing_errors_survive_clean_delta(self):
        self.write(row("LogPython: Error: prior failure") + row("LogInit: Ready"))
        result = checker.inspect(self.log, after_line=1)
        self.assertEqual((result["existing_error_count"], result["new_error_count"], result["total_error_count"]), (1, 0, 1))

    def test_line_boundary_is_exclusive(self):
        self.write(row("LogTest: Error: first") + row("LogTest: Error: second"))
        self.assertEqual(checker.inspect(self.log, after_line=1)["new_errors"][0]["line"], 2)

    def test_utc_boundary_is_inclusive_and_inherited(self):
        self.write(row("LogTest: Error: earlier", "11.59.59:999")
                   + row("LogTest: Error: at boundary") + "LogPython: Error: untimestamped continuation\n")
        result = checker.inspect(self.log, since=checker.utc("2026-11-16T12:00:00Z"))
        self.assertEqual((result["existing_error_count"], result["new_error_count"]), (1, 2))
        with self.assertRaises(ValueError):
            checker.utc("2026-11-16T12:00:00")

    def test_discovery_skips_newer_build_log(self):
        session = self.root / "Feature/doc/evidence/run/editor.log"
        build = self.root / "Saved/Logs/build.log"
        session.parent.mkdir(parents=True)
        build.parent.mkdir(parents=True)
        session.write_text(row("LogInit: Engine started"), encoding="utf-8")
        build.write_text("Build succeeded. 0 Error(s)\n", encoding="utf-8")
        os.utime(session, (100, 100))
        os.utime(build, (200, 200))
        self.assertEqual(checker.discover(self.root), session.resolve())

    def test_watch_preserves_existing_and_catches_append(self):
        self.write(row("LogTest: Error: existing"))
        def append(_):
            with self.log.open("a", encoding="utf-8") as stream:
                stream.write(row("LogTest: Error: new"))
        with mock.patch.object(checker.time, "monotonic", side_effect=[0, 0, 0, 2]), mock.patch.object(checker.time, "sleep", side_effect=append):
            result = checker.inspect(self.log, watch=True, duration=1)
        self.assertEqual((result["existing_error_count"], result["new_error_count"]), (1, 1))

    def test_watch_rescans_truncation(self):
        self.write(row("LogTest: Error: old message with extra length"))
        with mock.patch.object(checker.time, "monotonic", side_effect=[0, 0, 0, 2]), mock.patch.object(checker.time, "sleep", side_effect=lambda _: self.write(row("LogTest: Error: new"))):
            result = checker.inspect(self.log, watch=True, duration=1)
        self.assertEqual((result["existing_error_count"], result["new_error_count"]), (1, 1))
        self.assertEqual(len(result["events"]), 1)

    def test_watch_explicit_zero_includes_initial_errors(self):
        self.write(row("LogTest: Error: existing"))
        with mock.patch.object(checker.time, "monotonic", side_effect=[0, 2]):
            result = checker.inspect(self.log, after_line=0, watch=True, duration=1)
        self.assertEqual((result["existing_error_count"], result["new_error_count"]), (0, 1))

    def test_watch_incomplete_error_prefix_is_not_lost(self):
        self.write(row("LogInit: Ready") + "[2026.11.16-12.00.00:000][  0]LogPython: Err")
        def complete(_):
            with self.log.open("a", encoding="utf-8") as stream:
                stream.write("or: late failure\n")
        with mock.patch.object(checker.time, "monotonic", side_effect=[0, 0, 0, 2]), mock.patch.object(checker.time, "sleep", side_effect=complete):
            result = checker.inspect(self.log, watch=True, duration=1)
        self.assertEqual(result["new_error_count"], 1)
        self.assertEqual(result["new_errors"][0]["message"], "late failure")

    def test_appended_tail_updates_one_error_instead_of_duplicate(self):
        self.write("[2026.11.16-12.00.00:000][  0]LogPython: Error: partial")
        def complete(_):
            with self.log.open("a", encoding="utf-8") as stream:
                stream.write(" failure\n")
        with mock.patch.object(checker.time, "monotonic", side_effect=[0, 0, 0, 2]), mock.patch.object(checker.time, "sleep", side_effect=complete):
            result = checker.inspect(self.log, watch=True, duration=1)
        self.assertEqual(result["new_error_count"], 1)
        self.assertEqual(result["new_errors"][0]["message"], "partial failure")

    def test_nonfinite_watch_durations_are_rejected(self):
        self.write(row("LogInit: Ready"))
        for duration in (float("nan"), float("inf"), -1, 0):
            with self.assertRaises(ValueError):
                checker.inspect(self.log, watch=True, duration=duration)

    def test_cli_json_exit_codes_and_read_only_input(self):
        self.write(row("LogTest: Error: fail"))
        before = self.log.read_bytes()
        output = self.root / "result.json"
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(checker.main(["--log", str(self.log), "--json-output", str(output)]), 1)
            self.assertEqual(checker.main(["--log", str(self.log), "--after-line", "1"]), 0)
            self.assertEqual(checker.main(["--log", str(self.log), "--json-output", str(self.log)]), 2)
            self.assertEqual(checker.main(["--log", str(self.root / "missing.log")]), 2)
        self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["total_error_count"], 1)
        self.assertEqual(self.log.read_bytes(), before)

    def test_json_hardlink_cannot_overwrite_source_log(self):
        self.write(row("LogTest: Error: preserve me"))
        alias = self.root / "alias.json"
        try:
            os.link(self.log, alias)
        except OSError as exc:
            self.skipTest(str(exc))
        before = self.log.read_bytes()
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(checker.main(["--log", str(self.log), "--json-output", str(alias)]), 2)
        self.assertEqual(self.log.read_bytes(), before)

    def test_samefile_alias_branch_is_rejected_without_hardlink_permission(self):
        self.write(row("LogTest: Error: preserve me"))
        alias = self.root / "alias.json"
        alias.write_text("{}", encoding="utf-8")
        before = self.log.read_bytes()
        with mock.patch.object(Path, "samefile", return_value=True), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(checker.main(["--log", str(self.log), "--json-output", str(alias)]), 2)
        self.assertEqual(self.log.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
