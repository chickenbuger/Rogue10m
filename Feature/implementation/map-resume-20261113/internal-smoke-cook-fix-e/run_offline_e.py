"""Run only synthetic Python contracts; no subprocess, UE/build/game or GUI."""
import argparse
import ast
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import stat
import sys
import unittest

HERE = Path(__file__).resolve().parent


def pin(path):
    path = Path(path)
    data = path.read_bytes()
    return {"path": str(path), "size": len(data), "sha256": hashlib.sha256(data).hexdigest().upper()}


class Result(unittest.TextTestResult):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.subcase_passes = []
        self.subcase_failures = []

    def addSubTest(self, test, subtest, error):
        (self.subcase_passes if error is None else self.subcase_failures).append(subtest.id())
        super().addSubTest(test, subtest, error)


def fresh_output(value):
    requested = Path(value)
    if ".." in requested.parts:
        raise ValueError("Parent traversal refused")
    output = requested.absolute()
    for ancestor in [output, *output.parents]:
        try:
            facts = ancestor.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(facts.st_mode) or getattr(facts, "st_file_attributes", 0) & 1024:
            raise ValueError("Reparse/symlink output refused")
    if output != output.resolve() or not output.is_relative_to(HERE / "evidence") or output.exists():
        raise ValueError("Fresh evidence child of this packet required")
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = fresh_output(args.output)
    for name in ("strict_primitives.py", "internal_smoke.py", "test_cook_fix_e.py", "run_offline_e.py"):
        ast.parse((HERE / name).read_bytes(), filename=name)
    started = datetime.now(timezone.utc).isoformat()
    suite = unittest.defaultTestLoader.loadTestsFromName("test_cook_fix_e")
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2, resultclass=Result).run(suite)
    output.mkdir(parents=True, exist_ok=False)
    raw = stream.getvalue().encode("utf-8")
    (output / "tests.txt").write_bytes(raw)
    report = {"schema": "R10-cook-fix-e-synthetic-offline-v1", "status": "PASS" if result.wasSuccessful() else "FAIL",
        "synthetic_only": True, "actual_packaging_evidence": False, "started_utc": started,
        "ended_utc": datetime.now(timezone.utc).isoformat(), "python": {**pin(sys.executable), "version": sys.version},
        "tests_run": result.testsRun, "failures": len(result.failures), "errors": len(result.errors),
        "subcase_pass_count": len(result.subcase_passes), "subcase_failure_count": len(result.subcase_failures),
        "subcase_passes": result.subcase_passes, "subcase_failures": result.subcase_failures,
        "external_processes_started": 0, "ue_ubt_uat_cook_game_mcp_gui_executions": 0,
        "syntax_parse": "PASS", "inputs": [pin(HERE / n) for n in ("strict_primitives.py", "internal_smoke.py", "native43_validation.py", "Invoke-InternalSmoke.ps1", "test_cook_fix_e.py")],
        "test_log": pin(output / "tests.txt")}
    (output / "validation.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("status", "tests_run", "failures", "errors", "subcase_pass_count", "subcase_failure_count")}))
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
