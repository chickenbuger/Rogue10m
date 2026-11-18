"""Run the real PowerShell preflight against file fixtures; never launch Unreal or a build."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Scripts" / "VerifyCatalogConsumption.ps1"
TEST_ROOT = (ROOT / "Delivery" / "preflight-fixtures").resolve()
SHELL = shutil.which("pwsh") or shutil.which("powershell")
PACKAGE_FILE = Path("Content/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_ChargedShockwave.uasset")


class CatalogPreflightTests(unittest.TestCase):
    def setUp(self):
        if not SHELL:
            self.skipTest("PowerShell is required")
        TEST_ROOT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="case_", dir=TEST_ROOT)
        self.root = Path(self.temp.name).resolve()
        self.assertTrue(self.root.is_relative_to(TEST_ROOT))
        (self.root / "Scripts").mkdir()
        shutil.copy2(SCRIPT, self.root / "Scripts" / SCRIPT.name)
        (self.root / "Rogue10m.uproject").write_text("{}", encoding="utf-8")
        self.engine = self.root / "FixtureEngine"
        self.editor = self.engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
        self.editor.parent.mkdir(parents=True)
        # A non-executable sentinel tests file presence only. Every case is preflight-only or blocked.
        self.editor.write_bytes(b"preflight file fixture; not an executable")

    def tearDown(self):
        self.assertTrue(self.root.resolve().is_relative_to(TEST_ROOT))
        self.temp.cleanup()

    def run_script(self, *arguments):
        return subprocess.run(
            [SHELL, "-NoProfile", "-NonInteractive", "-File", str(self.root / "Scripts" / SCRIPT.name),
             "-EngineRoot", str(self.engine), "-Label", "TestA", *arguments],
            cwd=self.root, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20,
        )

    def report(self):
        path = self.root / "Feature/doc/evidence/catalog-consumption-20261110/TestA/preflight.json"
        return path, json.loads(path.read_text(encoding="utf-8-sig"))

    def assert_no_process(self, report):
        self.assertFalse(report["process_created"])
        stage_root = self.report()[0].parent
        self.assertFalse(any(path.is_dir() for path in stage_root.iterdir()))

    def test_core_preflight_accepts_no_content_and_reports_build_required(self):
        result = self.run_script("-PreflightOnly")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        _, report = self.report()
        self.assertEqual(report["expected_test_count"], 3)
        self.assertTrue(report["build_required"])
        self.assertEqual(report["missing_content_packages"], [])
        self.assert_no_process(report)

    def test_growth_missing_content_blocks_before_any_process(self):
        result = self.run_script("-PreflightOnly", "-IncludeGrowth")
        self.assertNotEqual(result.returncode, 0)
        _, report = self.report()
        self.assertEqual(report["expected_test_count"], 6)
        self.assertEqual(len(report["missing_content_packages"]), 1)
        self.assertIn("ChargedShockwave", report["missing_content_packages"][0])
        self.assertIn("before Editor launch", result.stderr)
        self.assert_no_process(report)

    def test_growth_file_presence_does_not_claim_catalog_opt_in(self):
        asset = self.root / PACKAGE_FILE
        asset.parent.mkdir(parents=True)
        asset.write_bytes(b"file-presence fixture; not an Unreal package")
        result = self.run_script("-PreflightOnly", "-IncludeGrowth")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        _, report = self.report()
        self.assertEqual(report["expected_test_count"], 6)
        self.assertEqual(report["missing_content_packages"], [])
        self.assertFalse(report["actual_catalog_opt_in_verified"])
        self.assert_no_process(report)

    def test_missing_binary_blocks_normal_mode_before_any_process(self):
        result = self.run_script()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("module missing", result.stderr)
        _, report = self.report()
        self.assertTrue(report["build_required"])
        self.assert_no_process(report)

    def test_missing_editor_is_precise_and_does_not_launch(self):
        self.editor.unlink()
        result = self.run_script("-PreflightOnly")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("executable missing", result.stderr)
        _, report = self.report()
        self.assertFalse(report["editor_exists"])
        self.assert_no_process(report)

    def test_missing_project_is_precise_and_does_not_launch(self):
        (self.root / "Rogue10m.uproject").unlink()
        result = self.run_script("-PreflightOnly")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Project file missing", result.stderr)
        _, report = self.report()
        self.assertFalse(report["project_exists"])
        self.assert_no_process(report)

    def test_existing_label_keeps_evidence_unchanged(self):
        self.assertEqual(self.run_script("-PreflightOnly").returncode, 0)
        path, _ = self.report()
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        result = self.run_script("-PreflightOnly")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("fresh label", result.stderr)
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)

    def test_metadata_failure_after_mock_launch_still_cleans_owned_process(self):
        module = self.root / "Binaries/Win64/UnrealEditor-Rogue10m.dll"
        module.parent.mkdir(parents=True)
        module.write_bytes(b"presence fixture only")
        wrapper = self.root / "simulate_metadata_failure.ps1"
        wrapper.write_text(r'''
$ErrorActionPreference='Stop'
$catalogFixtureRoot=$PSScriptRoot
function Start-Process {
    param($FilePath,$ArgumentList,$WindowStyle,[switch]$PassThru,$RedirectStandardOutput,$RedirectStandardError)
    $record=Join-Path $catalogFixtureRoot 'Feature/doc/evidence/catalog-consumption-20261110/TestA/preflight.json'
    Remove-Item -LiteralPath $record
    New-Item -ItemType Directory -Path $record | Out-Null
    Set-Content -LiteralPath (Join-Path $catalogFixtureRoot 'mock-launch.txt') -Value 'No real process created'
    $fake=[pscustomobject]@{Id=4242;HasExited=$false}
    $fake | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value {param($milliseconds) return $true}
    return $fake
}
function Stop-Process {
    param([int]$Id)
    Set-Content -LiteralPath (Join-Path $catalogFixtureRoot 'mock-cleanup.txt') -Value $Id
}
& (Join-Path $catalogFixtureRoot 'Scripts/VerifyCatalogConsumption.ps1') -EngineRoot (Join-Path $catalogFixtureRoot 'FixtureEngine') -Label TestA
''', encoding="utf-8")
        result = subprocess.run([SHELL, "-NoProfile", "-NonInteractive", "-File", str(wrapper)], cwd=self.root,
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue((self.root / "mock-launch.txt").is_file())
        self.assertEqual((self.root / "mock-cleanup.txt").read_text(encoding="utf-8-sig").strip(), "4242")


if __name__ == "__main__":
    unittest.main(verbosity=2)
