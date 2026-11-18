"""SYNTHETIC Native51 wiring; no UE, UAT, game, GUI or approval operation.

Every project, engine, freeze, release-contract and payload below is fictional
data in disposable owned scratch. Bootstrap copies keep fake policy pins away
from the shipped files. Nothing in these tests is actual execution evidence.
"""
import contextlib
import copy
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
from test_native51 import NEW_SOURCE, make_native51_fixture


class FullNative51OfflineTests(unittest.TestCase):
    def setUp(self):
        self.c = m.helpers()
        original_packet = m.HERE
        scratch = original_packet / "scratch"
        scratch.mkdir(exist_ok=True)
        scratch = self.c.physical(scratch, True)
        self.c.require(self.c.child(scratch, original_packet), "Synthetic scratch must be in this owned packet")
        self.tmp = tempfile.TemporaryDirectory(prefix="full-native51-SYNTHETIC-", dir=scratch)
        self.addCleanup(self.tmp.cleanup)
        self.base = self.c.physical(Path(self.tmp.name), True)
        self.packet = self.base / "bootstrap"
        self.packet.mkdir()
        for name in ("internal_smoke.py", "strict_primitives.py", "native51_validation.py", "native51-policy.json", "Invoke-InternalSmoke.ps1"):
            self.write(self.packet / name, (original_packet / name).read_bytes())
        self.here_patch = patch.object(m, "HERE", self.packet)
        self.here_patch.start()
        self.addCleanup(self.here_patch.stop)
        self.root, self.engine = self.base / "project", self.base / "engine"
        self.area = self.base / "owned"
        self.output = self.area / "new-run"
        self.output.mkdir(parents=True)
        self.original, self.local = self.base / "original", self.base / "local-app-data"
        self.local.mkdir()
        self.c.ORIGINAL = str(self.original)
        env_patch = patch.dict(os.environ, {"LOCALAPPDATA": str(self.local)})
        env_patch.start()
        self.addCleanup(env_patch.stop)
        self.project = str(self.root / "Rogue10m.uproject")
        for scope in self.c.SCOPES:
            (self.root / scope).mkdir(parents=True)
        self.write(self.root / "Rogue10m.uproject", b'{"SYNTHETIC":true}')
        self.write(self.root / "Source/Test.cpp", b"// SYNTHETIC source\n")
        self.write(self.root / NEW_SOURCE, b"// SYNTHETIC new CombatCredit source\n")
        self.write(self.root / "Config/DefaultEngine.ini", b"; SYNTHETIC config\n")
        self.package = "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
        relative = self.package[6:]
        self.write(self.root / ("Content/" + relative + ".umap"), b"SYNTHETIC not a UE package")
        self.write(self.root / ("Content/__ExternalActors__/" + relative + "/A.uasset"), b"SYNTHETIC actor")
        self.write(self.root / ("Content/__ExternalObjects__/" + relative + "/B.uasset"), b"SYNTHETIC object")
        self.build_id = "SYNTHETIC_BUILD"
        self.write(self.engine / "Engine/Build/BatchFiles/RunUAT.bat", b"REM SYNTHETIC - NEVER EXECUTE\n")
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
        inputs = self.c.input_inventory(self.root)
        source_config = {p["path"]: p["sha256"] for p in inputs["files"] if p["path"].startswith(("Source/", "Config/"))}
        refs, native_ctx = make_native51_fixture(self.base / "evidence/native51", self.project, str(self.engine), source_config, self.dll["sha256"])
        self.write(self.packet / "native51-policy.json", native_ctx["policy"])
        build_log = self.write(self.base / "evidence/editor-build.log", (self.project + " Rogue10mEditor\nResult: Succeeded\n").encode())
        externals = ["Content/__ExternalActors__/" + relative, "Content/__ExternalObjects__/" + relative]
        map_entry = {"package": self.package, "external_roots": externals,
                     "external_roots_present": {p: p in inputs["directories"] for p in externals},
                     "files": [p for p in inputs["files"] if p["path"] == "Content/" + relative + ".umap" or any(p["path"].startswith(e + "/") for e in externals)]}
        tools = {p: self.c.scan_tree(self.engine / p) for p in ("Engine/Binaries/DotNET/AutomationTool", "Engine/Binaries/DotNET/UnrealBuildTool")}
        self.seal = m.stamp({"schema": "R10-native51-smoke-seal-v1", "status": "sealed", "packaging_status": "NOT_EXECUTED",
            "root": str(self.root), "project": self.project, "engine": str(self.engine), "owner_area": str(self.area),
            "build_root_policy": "integration_owner_exact_root", "engine_build_id": self.build_id,
            "target": "Rogue10m", "platform": "Win64", "configuration": "Development", "run_tag": "SyntheticNative51",
            "active_destination": self.package, "smoke_map_selection_final": True, "cook_maps": [self.package], "required_iostore_packages": [self.package],
            "input_manifest": self.write(self.base / "inputs.json", inputs),
            "map_manifest": self.write(self.base / "maps.json", {"schema": "R10-candidate-maps-v1", "root": str(self.root), "maps": [map_entry]}),
            "engine_tool_manifest": self.write(self.base / "engine-tools.json", tools),
            "editor_build": {"result": refs["build_result"], "log": build_log}, "editor_products": self.products,
            "native51": refs, "protected_saves": [], "prior_game_products": [],
            "output_root": str(self.output), "outputs": {k: str(self.output / k) for k in self.c.OUTPUTS}, "zen_project": None,
            "runtime": {"python": self.c.file_pin(Path(sys.executable)), "powershell": self.powershell},
            "content_state": {"status": "OWNER_RECORDED_FINAL", "map_extent_m": [1800, 1800],
                "npc_placement": "SYNTHETIC fictional additive NPC state; no asset edited", "default_map": "SYNTHETIC fictional default-map state"}})
        self.freeze = {"schema": "R10-native51-content-freeze-v1", "status": "frozen_for_internal_smoke", "project": self.project,
            "active_destination": self.package, "input_manifest_sha256": self.seal["input_manifest"]["sha256"],
            "map_manifest_sha256": self.seal["map_manifest"]["sha256"], "editor_dll_sha256": self.dll["sha256"],
            "content_state": copy.deepcopy(self.seal["content_state"]), "recorded_utc": (datetime.now(timezone.utc)-timedelta(minutes=1)).isoformat(),
            "recorder": "SYNTHETIC_ONLY_NOT_AN_OWNER", "evidence": [self.write(self.base / "evidence/content-SYNTHETIC.txt", b"fictional content state; not actual map/NPC evidence")]}
        self.seal["content_freeze"] = self.write(self.base / "freeze.SYNTHETIC.json", self.freeze)
        self.old_e5 = self.area / "old-E5-preserved"
        self.old_archive = self.old_e5 / "Windows"
        self.old_source_archive = self.area / "old-E5-source" / "Windows"
        self.write(self.old_archive / "SYNTHETIC.txt", b"fictional old package; never a real E5 payload")
        self.write(self.old_source_archive / "SYNTHETIC.txt", b"fictional old source archive; not actual E5 data")
        self.old_manifest = self.write(self.old_e5 / "DELIVERY.SYNTHETIC.json", {
            "archive": str(self.old_archive), "source_archive": str(self.old_source_archive), "synthetic_only": True})
        self.preserved = {"status": "UNVERIFIED_INTERNAL_DEVELOPMENT_PACKAGE_PRESERVED", "release_eligible": False, "game_execution": 0,
            "archive": str(self.old_archive), "source_archive": str(self.old_source_archive), "delivery_manifest": self.old_manifest, "synthetic_only": True}
        self.seal["preserved_artifacts"] = [self.write(self.base / "evidence/preserved-E5.SYNTHETIC.json", self.preserved)]
        for key, name in (("helper", "strict_primitives.py"), ("validator", "internal_smoke.py"), ("native_validator", "native51_validation.py"),
                          ("native_policy", "native51-policy.json"), ("runner", "Invoke-InternalSmoke.ps1")):
            self.seal[key] = self.c.file_pin(self.packet / name)
        self.seal["tool_pins"] = [self.c.file_pin(p) for p in m.required_tools(self.c, self.seal)]
        for i, saved in enumerate((self.original / "Saved/SaveGames", self.root / "Saved/SaveGames", self.local / "Rogue10m/Saved/SaveGames")):
            self.seal["protected_saves"].append({"root": str(saved), "manifest": self.write(self.base / ("saves-" + str(i) + ".json"), self.c.scan_tree(saved, True))})
        self.seal_pin = self.write(self.base / "seal.SYNTHETIC.json", self.seal)
        now = datetime.now(timezone.utc)
        self.release = {"schema": "R10-native51-smoke-release-v1", "status": "released_for_native51_smoke", "seal_sha256": self.seal_pin["sha256"],
            "project": self.project, "owner_id": "SYNTHETIC_NOT_A_REAL_OWNER", "run_tag": self.seal["run_tag"], "active_destination": self.package,
            "scopes": ["Win64 Development Native51 internal smoke"], "project_build_outputs_authorized": True, "gui_forbidden": True, "release_eligible": False,
            "released_utc": (now-timedelta(minutes=1)).isoformat(), "expires_utc": (now+timedelta(minutes=30)).isoformat(),
            "owner_process": {"pid": 1, "start_utc_ticks": 1, "executable": self.powershell["path"], "sha256": self.powershell["sha256"]}, "synthetic_only": True}
        self.release_pin = self.write(self.base / "release-contract.SYNTHETIC.json", self.release)

    def write(self, path, value):
        path = self.c.physical(Path(path))
        self.c.require(self.c.child(path, self.base), "Fixture writes must stay in disposable owned scratch")
        path.parent.mkdir(parents=True, exist_ok=True)
        self.c.physical(path)
        path.write_bytes(value if isinstance(value, bytes) else json.dumps(value, indent=2).encode("utf-8"))
        return self.c.file_pin(path)

    def reseal(self):
        self.seal_pin = self.write(self.base / "seal.SYNTHETIC.json", self.seal)
        self.release["seal_sha256"] = self.seal_pin["sha256"]
        self.release_pin = self.write(self.base / "release-contract.SYNTHETIC.json", self.release)

    def validate(self, fresh=True):
        return m.validate(self.c, self.seal_pin, self.release_pin, self.release["owner_id"], fresh)

    def argument_plan(self):
        seal, native = self.validate()
        return m.plan(self.c, seal, self.seal_pin["sha256"], self.release_pin["sha256"], native)

    def fake_postflight_outputs(self, plan):
        for path in self.seal["outputs"].values(): Path(path).mkdir()
        evidence = Path(self.seal["outputs"]["evidence"])
        started = (datetime.now(timezone.utc)-timedelta(seconds=5)).isoformat()
        self.write(evidence / "wrapper-journal.json", {"schema": "R10-native51-smoke-wrapper-v1", "plan_sha256": plan["plan_sha256"],
            "sdk_exit": 0, "buildcookrun_exit": 0, "buildcookrun_started_utc": started})
        game_bytes = b"SYNTHETIC fresh Game - NOT EXECUTABLE"
        self.write(self.root / "Binaries/Win64/Rogue10m.exe", game_bytes)
        self.write(self.root / "Binaries/Win64/Rogue10m.target", {"TargetName": "Rogue10m", "TargetType": "Game", "Platform": "Win64",
            "Configuration": "Development", "Version": {"BuildId": self.build_id}, "Project": "../../Rogue10m.uproject",
            "Launch": "$(ProjectDir)/Binaries/Win64/Rogue10m.exe"})
        windows = Path(self.seal["outputs"]["archive"]) / "Windows"
        self.write(windows / "Rogue10m/Binaries/Win64/Rogue10m.exe", game_bytes)
        self.write(Path(self.seal["outputs"]["stage"]) / "Windows/Rogue10m/Binaries/Win64/Rogue10m.exe", game_bytes)
        self.write(windows / "Rogue10m/Content/Paks/SYNTHETIC.utoc", b"SYNTHETIC container")
        self.write(windows / "Rogue10m/Content/Paks/SYNTHETIC.ucas", b"SYNTHETIC container data")
        cooker_dir = Path(self.seal["outputs"]["cook"]) / "Windows"
        self.write(cooker_dir / "ue.projectstore", {"zenserver": {"projectid": "Rogue10m.AB12CD34", "oplogid": "Windows"}})
        self.write(cooker_dir / "Rogue10m/Metadata/zenfs.manifest", b"SYNTHETIC Zen metadata")
        open_log = Path(self.seal["outputs"]["cook_user"]) / "Build/Windows/FileOpenOrder/EditorOpenOrder.log"
        self.write(open_log, b"SYNTHETIC isolated file-open log")
        bcr = ("BuildCookRun -platform=Win64 -target=Rogue10m -clientconfig=Development -project=" + self.project
               + " -nocompileeditor -CookOutputDir=" + str(cooker_dir) + " -stagingdirectory=" + self.seal["outputs"]["stage"]
               + " -archivedirectory=" + self.seal["outputs"]["archive"])
        cook = ('"' + self.project + '" -run=Cook -TargetPlatform=Windows -outputdir="' + str(cooker_dir)
                + '" -UserDir="' + self.seal["outputs"]["cook_user"] + '" -FOOBASEDIR="' + self.seal["outputs"]["cook_user"]
                + '" -DisablePython -NoLiveCoding -fileopenlog '
                + '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False')
        log = ("Parsing command line: " + bcr + "\n" + self.project + " Rogue10m Win64 Development\nRunning: "
               + str(self.engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe") + " " + cook
               + "\nZen oplog 'Rogue10m.AB12CD34/Windows' created\nCOOK COMMAND COMPLETED\nBUILD SUCCESSFUL\n")
        self.write(evidence / "buildcookrun.log", log.encode())
        self.write(evidence / "uat-bcr-logs/Cook-SYNTHETIC.txt", ("LogFileOpenOrder: Initialized file open order log : " + str(open_log)
                   + "\nLogInit: Command Line: " + cook + "\n").encode())
        ubt = ("Command line: UnrealBuildTool.dll Rogue10m Win64 Development -Project=" + self.project
               + " -WaitMutex -NoXGE -NoSNDBS -NoFASTBuild -UBADisableRemote -MaxParallelActions=4 -Rebuild\n"
               + "Using Unreal Build Accelerator local executor\nExecuting up to 4 actions\nLink [x64] Rogue10m.exe\nWriteMetadata Rogue10m.target\n")
        self.write(evidence / "uat-bcr-logs/UBA-Rogue10m-Win64-Development-SYNTHETIC.txt", ubt.encode())
        return evidence

    def cli_postflight(self, evidence):
        argv = ["internal_smoke.py", "--seal", self.seal_pin["path"], "--seal-sha256", self.seal_pin["sha256"],
                "--release", self.release_pin["path"], "--release-sha256", self.release_pin["sha256"],
                "--owner-id", self.release["owner_id"], "--postflight", str(evidence)]
        out = io.StringIO()
        # Reuse the pinned helper instance with synthetic ORIGINAL; all actual
        # inventory, native, command, RED and postflight functions stay intact.
        with patch.object(m, "helpers", return_value=self.c), patch.object(sys, "argv", argv), contextlib.redirect_stdout(out):
            code = m.main()
        return code, json.loads(out.getvalue())

    def test_positive_full_plan_and_phase_hash_permanently_unverified(self):
        plan = self.argument_plan()
        self.assertEqual(plan["native51"]["native_tests"], 51)
        self.assertEqual(plan["native51"]["new_combat_credit_cases"], 8)
        self.assertEqual(plan["acceptance_flags"], m.FLAGS)
        self.assertFalse(plan["release_eligible"])
        self.assertTrue(plan["not_executed_by_this_validator"])
        for path in self.seal["outputs"].values(): Path(path).mkdir()
        seal, native = self.validate(False)
        again = m.plan(self.c, seal, self.seal_pin["sha256"], self.release_pin["sha256"], native)
        self.assertEqual(plan["plan_sha256"], again["plan_sha256"])
        with self.assertRaisesRegex(ValueError, "Fresh outputs"):
            self.validate(True)

    def test_current_source_drift_blocks_full_gate(self):
        self.write(self.root / NEW_SOURCE, b"SYNTHETIC source changed after freeze")
        with self.assertRaisesRegex(ValueError, "smoke inputs"): self.validate()

    def test_each_content_freeze_binding_is_required(self):
        for key in ("project", "active_destination", "input_manifest_sha256", "map_manifest_sha256", "editor_dll_sha256", "content_state"):
            value = copy.deepcopy(self.freeze)
            value[key] = {"status": "wrong"} if key == "content_state" else "SYNTHETIC_WRONG"
            self.seal["content_freeze"] = self.write(self.base / "freeze.SYNTHETIC.json", value)
            self.reseal()
            with self.subTest(field=key), self.assertRaisesRegex(ValueError, "Final content freeze"):
                self.validate()

    def test_content_freeze_requires_evidence_and_recorded_final_state(self):
        for key, value in (("evidence", []), ("recorded_utc", "2099-01-01T00:00:00+00:00"), ("recorder", "")):
            bad = {**self.freeze, key: value}
            self.seal["content_freeze"] = self.write(self.base / "freeze.SYNTHETIC.json", bad)
            self.reseal()
            with self.subTest(field=key), self.assertRaises(ValueError): self.validate()

    def test_stale_native43_receipt_cannot_satisfy_native51(self):
        pin = self.seal["native51"]["result"]
        value = self.c.read_json_pin(pin)
        value.update(count=43, status="headless_editor_exited_report_review_required")
        self.seal["native51"]["result"] = self.write(pin["path"], value)
        self.reseal()
        with self.assertRaisesRegex(ValueError, "Native51 receipt|session|test set"): self.validate()

    def test_native51_build_receipt_must_equal_canonical_editor_pin(self):
        self.seal["editor_build"]["result"] = self.write(self.base / "another-editor-result.json", self.c.read_json_pin(self.seal["native51"]["build_result"]))
        self.reseal()
        with self.assertRaisesRegex(ValueError, "receipt pins differ"): self.validate()

    def test_existing_e5_output_directory_is_never_reused(self):
        self.seal["output_root"] = str(self.old_e5)
        self.seal["outputs"] = {k: str(self.old_e5 / ("Windows" if k == "archive" else k)) for k in self.c.OUTPUTS}
        self.reseal()
        with self.assertRaises(ValueError): self.validate()

    def test_new_outputs_cannot_be_nested_inside_preserved_e5_payload(self):
        self.seal["output_root"] = str(self.old_archive)
        self.seal["outputs"] = {k: str(self.old_archive / k) for k in self.c.OUTPUTS}
        self.reseal()
        with self.assertRaises(ValueError): self.validate()

    def test_protected_original_build_root_and_default_saves_are_checked(self):
        for index, record in enumerate(self.seal["protected_saves"]):
            changed = Path(record["root"]) / "User.SYNTHETIC.sav"
            self.write(changed, b"SYNTHETIC changed protected data")
            with self.subTest(save_root=index), self.assertRaisesRegex(ValueError, "protected SaveGames"): self.validate()
            changed.unlink()

    def test_tampered_preserved_delivery_manifest_blocks(self):
        self.write(self.old_manifest["path"], b"SYNTHETIC altered old delivery record")
        with self.assertRaisesRegex(ValueError, "SHA mismatch"): self.validate()

    def test_positive_full_postflight_keeps_native51_and_content_limits(self):
        evidence = self.fake_postflight_outputs(self.argument_plan())
        code, result = self.cli_postflight(evidence)
        self.assertEqual(code, 0, result)
        self.assertEqual(result["schema"], "R10-native51-smoke-result-v1")
        self.assertEqual(result["status"], "internal_smoke_artifact_built_UNVERIFIED")
        self.assertEqual(result["stage_archive_chain"], "EXACT_NEW_OUTPUTS")
        self.assertEqual(result["native51"]["native_tests"], 51)
        self.assertEqual(result["acceptance_flags"], m.FLAGS)
        self.assertFalse(result["release_eligible"])
        self.assertFalse(result["zen_presealed"])
        self.assertEqual(result["content_state"], self.seal["content_state"])

    def test_game_compile_override_absent_and_cooker_iostore_flags_remain(self):
        plan = self.argument_plan()
        ubt = next(x for x in plan["bcr_args"] if x.startswith("-UbtArgs="))
        self.assertNotIn("-nolivecoding", ubt.lower())
        self.assertEqual(ubt.split().count("-Rebuild"), 1)
        self.assertIn("-NoHotReload", ubt.split())
        cooker = next(x for x in plan["bcr_args"] if x.startswith("-AdditionalCookerOptions=")).split("=", 1)[1]
        self.assertEqual(cooker.split().count("-NoLiveCoding"), 1)
        self.assertEqual(cooker.split().count("-DisablePython"), 1)
        evidence = self.fake_postflight_outputs(plan)
        code, result = self.cli_postflight(evidence)
        self.assertEqual(code, 0, result)
        self.assertIn("-NoLiveCoding", result["next_iostore"]["args"])
        self.assertFalse(result["release_eligible"])

    def test_actual_game_ubt_compile_no_live_coding_injection_is_blocked(self):
        evidence = self.fake_postflight_outputs(self.argument_plan())
        path = evidence / "uat-bcr-logs/UBA-Rogue10m-Win64-Development-SYNTHETIC.txt"
        original = path.read_text(encoding="utf-8")
        for override in ("-NoLiveCoding", "-nOlIvEcOdInG", '"-NoLiveCoding"', "-NoLiveCoding=false",
                         '-No"Live"Coding', '"-NoLiveCoding"=false'):
            with self.subTest(override=override):
                path.write_text(original.replace(" -Rebuild\n", " " + override + " -Rebuild\n"), encoding="utf-8")
                code, result = self.cli_postflight(evidence)
                self.assertEqual(code, 2, result)
                self.assertIn("Actual Game UBT compile -NoLiveCoding is forbidden", result["blocker"])
                self.assertFalse(result["release_eligible"])
        path.write_text(original, encoding="utf-8")

    def test_wrong_staged_game_is_blocked_after_helper_postflight(self):
        evidence = self.fake_postflight_outputs(self.argument_plan())
        self.write(Path(self.seal["outputs"]["stage"]) / "Windows/Rogue10m/Binaries/Win64/Rogue10m.exe", b"SYNTHETIC wrong staged Game")
        code, result = self.cli_postflight(evidence)
        self.assertEqual(code, 2)
        self.assertIn("Staged/Game/archive", result["blocker"])
        self.assertFalse(result["release_eligible"])

    def test_wrong_archive_game_is_blocked(self):
        evidence = self.fake_postflight_outputs(self.argument_plan())
        self.write(Path(self.seal["outputs"]["archive"]) / "Windows/Rogue10m/Binaries/Win64/Rogue10m.exe", b"SYNTHETIC wrong archived Game")
        code, result = self.cli_postflight(evidence)
        self.assertEqual(code, 2)
        self.assertIn("Archive inner EXE", result["blocker"])

    def test_wrong_or_duplicate_actual_archive_path_is_blocked(self):
        evidence = self.fake_postflight_outputs(self.argument_plan())
        path = evidence / "buildcookrun.log"
        original = path.read_text(encoding="utf-8")
        actual = "-archivedirectory=" + self.seal["outputs"]["archive"]
        for replacement in ("-archivedirectory=" + str(self.old_archive), actual + " " + actual):
            self.write(path, original.replace(actual, replacement).encode())
            code, result = self.cli_postflight(evidence)
            with self.subTest(replacement=replacement):
                self.assertEqual(code, 2)
                self.assertIn("archive path", result["blocker"])

    def test_stale_game_timestamp_is_not_repaired_by_matching_archive(self):
        evidence = self.fake_postflight_outputs(self.argument_plan())
        os.utime(self.root / "Binaries/Win64/Rogue10m.exe", (1, 1))
        code, result = self.cli_postflight(evidence)
        self.assertEqual(code, 2)
        self.assertIn("predate", result["blocker"])

    def test_actual_cook_without_foo_override_is_blocked(self):
        evidence = self.fake_postflight_outputs(self.argument_plan())
        path = evidence / "buildcookrun.log"
        value = path.read_text(encoding="utf-8").replace('-FOOBASEDIR="' + self.seal["outputs"]["cook_user"] + '"', "")
        self.write(path, value.encode())
        code, result = self.cli_postflight(evidence)
        self.assertEqual(code, 2)
        self.assertIn("FOOBASEDIR", result["blocker"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
