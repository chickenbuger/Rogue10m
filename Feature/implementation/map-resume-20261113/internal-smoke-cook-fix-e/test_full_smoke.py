"""SYNTHETIC full smoke wiring only; never actual UE/build/runtime evidence.

All fabricated project, engine, process receipts and payload bytes live in owned
scratch. No executable, build tool, game, GUI or subprocess is launched here.
"""
import contextlib
from datetime import datetime, timedelta, timezone
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import internal_smoke as m
from test_native43 import make_native_fixture


class FullSmokeOfflineTests(unittest.TestCase):
    def setUp(self):
        self.c = m.helpers()
        scratch = m.HERE / "scratch"
        scratch.mkdir(exist_ok=True)
        scratch = self.c.physical(scratch, True)
        self.c.require(self.c.child(scratch, m.HERE), "Synthetic scratch must stay under this owned packet")
        self.tmp = tempfile.TemporaryDirectory(prefix="full-smoke-SYNTHETIC-", dir=scratch)
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.root = self.base / "project"
        self.engine = self.base / "engine"
        self.area = self.base / "owned"
        self.output = self.area / "package"
        self.output.mkdir(parents=True)
        self.original = self.base / "original"
        self.local = self.base / "local-app-data"
        self.local.mkdir()
        self.addCleanup(patch.stopall)
        patch.object(self.c, "ORIGINAL", str(self.original)).start()
        patch.dict(os.environ, {"LOCALAPPDATA": str(self.local)}).start()
        self.project = str(self.root / "Rogue10m.uproject")
        for scope in self.c.SCOPES:
            (self.root / scope).mkdir(parents=True)
        self.write(self.root / "Rogue10m.uproject", b'{"SYNTHETIC":true}')
        self.write(self.root / "Source/Test.cpp", b"// SYNTHETIC source, not game code\n")
        self.write(self.root / "Config/DefaultEngine.ini", b"; SYNTHETIC config\n")
        self.package = "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
        relative = self.package[6:]
        self.write(self.root / ("Content/" + relative + ".umap"), b"SYNTHETIC not a UE package")
        self.write(self.root / ("Content/__ExternalActors__/" + relative + "/A.uasset"), b"SYNTHETIC actor")
        self.write(self.root / ("Content/__ExternalObjects__/" + relative + "/B.uasset"), b"SYNTHETIC object")
        self.build_id = "SYNTHETIC_BUILD"
        for name in ("RunUAT.bat",):
            self.write(self.engine / "Engine/Build/BatchFiles" / name, b"REM SYNTHETIC - NEVER EXECUTE\n")
        for name in ("UnrealEditor-Cmd.exe", "UnrealPak.exe"):
            self.write(self.engine / "Engine/Binaries/Win64" / name, b"SYNTHETIC - NOT EXECUTABLE")
        self.write(self.engine / "Engine/Build/Build.version", {"MajorVersion": 5, "MinorVersion": 8})
        self.write(self.engine / "Engine/Binaries/Win64/UnrealEditor.modules", {"BuildId": self.build_id, "Modules": {}})
        for tool in ("AutomationTool", "UnrealBuildTool"):
            self.write(self.engine / ("Engine/Binaries/DotNET/" + tool + "/SYNTHETIC.txt"), b"not an installed tool")
        for name in self.c.PRODUCTS[:4]:
            self.write(self.root / "Binaries/Win64" / name, ("SYNTHETIC " + name).encode())
        self.write(self.root / "Binaries/Win64/UnrealEditor.modules", {"BuildId": self.build_id, "Modules": {
            "Rogue10m": self.c.PRODUCTS[0], "Rogue10mEditor": self.c.PRODUCTS[2]}})
        self.write(self.root / "Binaries/Win64/Rogue10mEditor.target", {
            "TargetName": "Rogue10mEditor", "TargetType": "Editor", "Platform": "Win64", "Configuration": "Development",
            "Version": {"BuildId": self.build_id}, "Project": "../../Rogue10m.uproject"})
        self.powershell = self.write(self.base / "pwsh-SYNTHETIC.exe", b"SYNTHETIC - NOT EXECUTABLE")
        self.products = [self.c.file_pin(self.root / "Binaries/Win64" / name) for name in self.c.PRODUCTS]
        self.dll = self.products[0]
        self.build = {"exit_code": 0, "dll_sha256": self.dll["sha256"],
                      "started_utc": "2026-11-13T12:30:00+00:00", "ended_utc": "2026-11-13T12:31:48+00:00"}
        build_result = self.write(self.base / "evidence/editor-result.json", self.build)
        build_log = self.write(self.base / "evidence/editor-build.log", (self.project + " Rogue10mEditor\nResult: Succeeded\n").encode())
        inputs = self.c.input_inventory(self.root)
        source_config = {p["path"]: p["sha256"] for p in inputs["files"] if p["path"].startswith(("Source/", "Config/"))}
        refs, _ = make_native_fixture(self.base / "evidence/native43", self.project, str(self.engine), source_config, self.dll["sha256"])
        externals = ["Content/__ExternalActors__/" + relative, "Content/__ExternalObjects__/" + relative]
        map_entry = {"package": self.package, "external_roots": externals,
                     "external_roots_present": {p: p in inputs["directories"] for p in externals},
                     "files": [p for p in inputs["files"] if p["path"] == "Content/" + relative + ".umap" or any(p["path"].startswith(e + "/") for e in externals)]}
        tools = {p: self.c.scan_tree(self.engine / p) for p in ("Engine/Binaries/DotNET/AutomationTool", "Engine/Binaries/DotNET/UnrealBuildTool")}
        self.seal = m.stamp({"schema": "R10-internal-smoke-seal-v1", "status": "sealed", "packaging_status": "NOT_EXECUTED",
            "root": str(self.root), "project": self.project, "engine": str(self.engine), "owner_area": str(self.area),
            "build_root_policy": "integration_owner_exact_root", "engine_build_id": self.build_id,
            "target": "Rogue10m", "platform": "Win64", "configuration": "Development", "run_tag": "SyntheticFull",
            "active_destination": self.package, "smoke_map_selection_final": True, "cook_maps": [self.package], "required_iostore_packages": [self.package],
            "input_manifest": self.write(self.base / "inputs.json", inputs),
            "map_manifest": self.write(self.base / "maps.json", {"schema": "R10-candidate-maps-v1", "root": str(self.root), "maps": [map_entry]}),
            "engine_tool_manifest": self.write(self.base / "engine-tools.json", tools), "editor_build": {"result": build_result, "log": build_log},
            "editor_products": self.products, "native43": refs, "protected_saves": [], "prior_game_products": [],
            "output_root": str(self.output), "outputs": {k: str(self.output / k) for k in self.c.OUTPUTS}, "zen_project": None,
            "runtime": {"python": self.c.file_pin(Path(sys.executable)), "powershell": self.powershell}})
        for key, name in (("helper", "strict_primitives.py"), ("validator", "internal_smoke.py"), ("native_validator", "native43_validation.py"),
                          ("native_policy", "native43-policy.json"), ("runner", "Invoke-InternalSmoke.ps1")):
            self.seal[key] = self.c.file_pin(m.HERE / name)
        self.seal["tool_pins"] = [self.c.file_pin(p) for p in m.required_tools(self.c, self.seal)]
        for i, saved in enumerate((self.original / "Saved/SaveGames", self.root / "Saved/SaveGames", self.local / "Rogue10m/Saved/SaveGames")):
            self.seal["protected_saves"].append({"root": str(saved), "manifest": self.write(self.base / ("saves-" + str(i) + ".json"), self.c.scan_tree(saved, True))})
        self.seal_pin = self.write(self.base / "seal.SYNTHETIC.json", self.seal)
        now = datetime.now(timezone.utc)
        self.release = {"schema": "R10-internal-smoke-release-v1", "status": "released_for_internal_smoke", "seal_sha256": self.seal_pin["sha256"],
            "project": self.project, "owner_id": "SYNTHETIC_NOT_AN_OWNER", "run_tag": self.seal["run_tag"], "active_destination": self.package,
            "scopes": ["Win64 Development internal smoke"], "project_build_outputs_authorized": True, "gui_forbidden": True, "release_eligible": False,
            "released_utc": (now-timedelta(minutes=1)).isoformat(), "expires_utc": (now+timedelta(minutes=30)).isoformat(),
            "owner_process": {"pid": 1, "start_utc_ticks": 1, "executable": self.powershell["path"], "sha256": self.powershell["sha256"]}}
        self.release_pin = self.write(self.base / "release.SYNTHETIC.json", self.release)

    def write(self, path, value):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value if isinstance(value, bytes) else json.dumps(value, indent=2).encode("utf-8"))
        return self.c.file_pin(path)

    def reseal(self):
        self.seal_pin = self.write(self.base / "seal.SYNTHETIC.json", self.seal)
        self.release["seal_sha256"] = self.seal_pin["sha256"]
        self.release_pin = self.write(self.base / "release.SYNTHETIC.json", self.release)

    def validate(self, fresh=True):
        return m.validate(self.c, self.seal_pin, self.release_pin, self.release["owner_id"], fresh)

    def argument_plan(self):
        seal, native = self.validate()
        return m.plan(self.c, seal, self.seal_pin["sha256"], self.release_pin["sha256"], native)

    def fake_postflight_outputs(self, plan):
        for path in self.seal["outputs"].values(): Path(path).mkdir()
        evidence = Path(self.seal["outputs"]["evidence"])
        started = (datetime.now(timezone.utc)-timedelta(seconds=5)).isoformat()
        self.write(evidence / "wrapper-journal.json", {"schema": "R10-internal-smoke-wrapper-v1", "plan_sha256": plan["plan_sha256"],
            "sdk_exit": 0, "buildcookrun_exit": 0, "buildcookrun_started_utc": started})
        self.write(self.root / "Binaries/Win64/Rogue10m.exe", b"SYNTHETIC fresh Game - NOT EXECUTABLE")
        self.write(self.root / "Binaries/Win64/Rogue10m.target", {"TargetName": "Rogue10m", "TargetType": "Game", "Platform": "Win64",
            "Configuration": "Development", "Version": {"BuildId": self.build_id}, "Project": "../../Rogue10m.uproject",
            "Launch": "$(ProjectDir)/Binaries/Win64/Rogue10m.exe"})
        windows = Path(self.seal["outputs"]["archive"]) / "Windows"
        self.write(windows / "Rogue10m/Binaries/Win64/Rogue10m.exe", b"SYNTHETIC fresh Game - NOT EXECUTABLE")
        self.write(windows / "Rogue10m/Content/Paks/SYNTHETIC.utoc", b"SYNTHETIC container")
        self.write(windows / "Rogue10m/Content/Paks/SYNTHETIC.ucas", b"SYNTHETIC container data")
        cook = 'Running: "' + str(self.engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe") + '" "' + self.project + '" -run=Cook '
        cook += '-UserDir="' + self.seal["outputs"]["cook_user"] + '" -DisablePython -NoLiveCoding '
        cook += '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False'
        self.write(evidence / "buildcookrun.log", (self.project + " Rogue10m Win64 Development\n" + cook + "\nZen oplog 'Rogue10m.AB12CD34/Windows' created\nBUILD SUCCESSFUL\n").encode())
        ubt = "-WaitMutex -NoXGE -NoSNDBS -NoFASTBuild -UBADisableRemote -MaxParallelActions=4\n"
        ubt += "Using Unreal Build Accelerator local executor\nExecuting up to 4 actions\nLink [x64] Rogue10m.exe\nWriteMetadata Rogue10m.target\n"
        self.write(evidence / "uat-bcr-logs/UBA-Rogue10m-Win64-Development-SYNTHETIC.txt", ubt.encode())
        return evidence

    def test_full_validate_plan_and_phase_hash_remain_nonrelease(self):
        plan = self.argument_plan()
        self.assertEqual(plan["native43"]["native_tests"], 43)
        self.assertEqual(plan["acceptance_flags"], m.FLAGS)
        self.assertFalse(plan["release_eligible"])
        self.assertTrue(plan["not_executed_by_this_validator"])
        for path in self.seal["outputs"].values(): Path(path).mkdir()
        seal, native = self.validate(False)
        again = m.plan(self.c, seal, self.seal_pin["sha256"], self.release_pin["sha256"], native)
        self.assertEqual(plan["plan_sha256"], again["plan_sha256"])
        with self.assertRaisesRegex(ValueError, "Fresh outputs"):
            self.validate(True)

    def test_full_gate_rejects_project_write_authority_absent(self):
        self.release["project_build_outputs_authorized"] = False
        self.release_pin = self.write(self.base / "release.SYNTHETIC.json", self.release)
        with self.assertRaisesRegex(ValueError, "write authority"):
            self.validate()

    def test_full_gate_rejects_source_changed_since_capture(self):
        self.write(self.root / "Source/Test.cpp", b"SYNTHETIC changed after freeze")
        with self.assertRaisesRegex(ValueError, "smoke inputs"):
            self.validate()

    def test_full_gate_rejects_native43_relocation(self):
        process = self.seal["native43"]["process"]
        value = self.c.read_json_pin(process)
        value["command"][1] = str(self.base / "different/Rogue10m.uproject")
        self.seal["native43"]["process"] = self.write(process["path"], value)
        self.reseal()
        with self.assertRaises(ValueError): self.validate()

    def test_full_gate_requires_per_log_startup_review(self):
        self.seal["native43"].pop("startup_review")
        self.reseal()
        with self.assertRaisesRegex(ValueError, "explicit pinned owner review"):
            self.validate()

    def test_full_gate_rejects_added_external_actor(self):
        self.write(self.root / ("Content/__ExternalActors__/" + self.package[6:] + "/ADDED.uasset"), b"SYNTHETIC added")
        with self.assertRaisesRegex(ValueError, "smoke inputs"):
            self.validate()

    def test_full_gate_rejects_protected_save_change(self):
        self.write(self.original / "Saved/SaveGames/User.sav", b"SYNTHETIC changed protected save")
        with self.assertRaisesRegex(ValueError, "protected SaveGames"):
            self.validate()

    def test_prior_game_snapshot_is_allowed_but_tamper_is_not(self):
        self.write(self.root / "Binaries/Win64/Rogue10m.exe", b"SYNTHETIC stale Game")
        self.seal["prior_game_products"] = [self.c.file_pin(self.root / "Binaries/Win64/Rogue10m.exe")]
        self.reseal()
        self.validate()
        self.write(self.root / "Binaries/Win64/Rogue10m.exe", b"SYNTHETIC changed prior Game")
        with self.assertRaisesRegex(ValueError, "Prior Game product snapshot"):
            self.validate()

    def test_main_postflight_derives_observed_zen_but_never_accepts_release(self):
        plan = self.argument_plan()
        evidence = self.fake_postflight_outputs(plan)
        argv = ["internal_smoke.py", "--seal", self.seal_pin["path"], "--seal-sha256", self.seal_pin["sha256"],
                "--release", self.release_pin["path"], "--release-sha256", self.release_pin["sha256"],
                "--owner-id", self.release["owner_id"], "--postflight", str(evidence)]
        out = io.StringIO()
        with patch.object(m, "helpers", return_value=self.c), patch.object(sys, "argv", argv), contextlib.redirect_stdout(out):
            code = m.main()
        result = json.loads(out.getvalue())
        self.assertEqual(code, 0, result)
        self.assertEqual(result["status"], "internal_smoke_artifact_built_UNVERIFIED")
        self.assertEqual(result["observed_zen_project"], "Rogue10m.AB12CD34")
        self.assertFalse(result["zen_presealed"])
        self.assertFalse(result["release_eligible"])
        self.assertEqual(result["acceptance_flags"], m.FLAGS)

    def test_full_postflight_rejects_stale_game_and_wrong_archive(self):
        plan = self.argument_plan()
        evidence = self.fake_postflight_outputs(plan)
        game = self.root / "Binaries/Win64/Rogue10m.exe"
        os.utime(game, (1, 1))
        with self.assertRaisesRegex(ValueError, "predate"):
            self.c.postflight({**self.seal, "zen_project": "Rogue10m.AB12CD34"}, plan, evidence)
        os.utime(game, None)
        self.write(Path(self.seal["outputs"]["archive"]) / "Windows/Rogue10m/Binaries/Win64/Rogue10m.exe", b"SYNTHETIC wrong archive")
        with self.assertRaisesRegex(ValueError, "Archive inner EXE"):
            self.c.postflight({**self.seal, "zen_project": "Rogue10m.AB12CD34"}, plan, evidence)


if __name__ == "__main__":
    unittest.main(verbosity=2)
