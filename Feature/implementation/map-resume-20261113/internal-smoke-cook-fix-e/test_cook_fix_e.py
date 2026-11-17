"""SYNTHETIC offline Cook-Fix-E contracts; no UE/build/game/GUI/process launch.

Every fake project, log, executable-shaped byte string and receipt stays under
this packet's owned scratch. These tests are never actual packaging evidence.
"""
import contextlib
import copy
from datetime import datetime, timedelta, timezone
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

import internal_smoke as m
import test_full_smoke as previous


class CookFixEOfflineTests(unittest.TestCase):
    write = previous.FullSmokeOfflineTests.write
    reseal = previous.FullSmokeOfflineTests.reseal
    validate = previous.FullSmokeOfflineTests.validate
    argument_plan = previous.FullSmokeOfflineTests.argument_plan

    def setUp(self):
        original_fixture = previous.make_native_fixture

        def clean_native_fixture(*args, **kwargs):
            refs, context = original_fixture(*args, **kwargs)
            tail = ["-culture=en", "-language=en", "-locale=en-US"]
            for key in ("process", "result"):
                value = json.loads(Path(refs[key]["path"]).read_bytes())
                value["command"] += tail
                refs[key] = self.write(refs[key]["path"], value)
            log = Path(refs["log"]["path"]).read_text(encoding="utf-8").splitlines()
            log = [line for line in log if "SYNTHETIC startup row" not in line]
            log[0] += " " + " ".join(tail)
            refs["log"] = self.write(refs["log"]["path"], ("\n".join(log) + "\n").encode())
            refs.pop("startup_review")
            return refs, context

        with patch.object(previous, "make_native_fixture", side_effect=clean_native_fixture):
            previous.FullSmokeOfflineTests.setUp(self)
        self.assertTrue(self.c.child(self.base, m.HERE / "scratch"))
        for name in ("Popen", "run", "call", "check_call", "check_output"):
            guard = patch.object(subprocess, name, side_effect=AssertionError("External process forbidden in offline tests"))
            guard.start(); self.addCleanup(guard.stop)
        guard = patch.object(os, "system", side_effect=AssertionError("Shell forbidden in offline tests"))
        guard.start(); self.addCleanup(guard.stop)
        if hasattr(os, "startfile"):
            guard = patch.object(os, "startfile", side_effect=AssertionError("GUI forbidden in offline tests"))
            guard.start(); self.addCleanup(guard.stop)

    def fake_postflight_outputs(self, plan):
        evidence = previous.FullSmokeOfflineTests.fake_postflight_outputs(self, plan)
        ubt_args = next(a.split("=", 1)[1] for a in plan["bcr_args"] if a.startswith("-UbtArgs="))
        ubt_command = '"' + str(self.engine / "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll") + '" Rogue10m Win64 Development -Project=' + self.project + " " + ubt_args
        ubt = "Command line: " + ubt_command + "\nUsing Unreal Build Accelerator local executor\nExecuting up to 4 actions\nLink [x64] Rogue10m.exe\nWriteMetadata Rogue10m.target\n"
        self.ubt_path = evidence / "uat-bcr-logs/UBA-Rogue10m-Win64-Development-SYNTHETIC.txt"
        self.write(self.ubt_path, ubt.encode())
        self.cook_dir = Path(self.seal["outputs"]["cook"]) / "Windows"
        self.open_log = Path(self.seal["outputs"]["cook_user"]) / "Build/Windows/FileOpenOrder/EditorOpenOrder.log"
        cooker_args = '-run=Cook -TargetPlatform=Windows -outputdir="' + str(self.cook_dir) + '" '
        cooker_args += next(a.split("=", 1)[1] for a in plan["bcr_args"] if a.startswith("-AdditionalCookerOptions=")) + " -fileopenlog"
        self.cook_command = '"' + self.project + '" ' + cooker_args
        running = 'Running: "' + str(self.engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe") + '" ' + self.cook_command
        main_log = "Parsing command line: " + " ".join(plan["bcr_args"]) + "\n" + self.project + " Rogue10m Win64 Development\n"
        main_log += running + "\nZen oplog 'Rogue10m.AB12CD34/Windows' created\nCOOK COMMAND COMPLETED\nBUILD SUCCESSFUL\n"
        self.main_log = evidence / "buildcookrun.log"
        self.write(self.main_log, main_log.encode())
        self.cook_log_path = evidence / "uat-bcr-logs/Cook-2026.11.13-SYNTHETIC.txt"
        self.write(self.cook_log_path, ("LogFileOpenOrder: Initialized file open order log : " + str(self.open_log) + "\nLogInit: Command Line: " + self.cook_command + "\n").encode())
        self.write(self.open_log, b'"SYNTHETIC-NEVER-A-REAL-ASSET" 0\n')
        self.write(self.cook_dir / "ue.projectstore", {"zenserver": {"projectid": "Rogue10m.AB12CD34", "oplogid": "Windows"}})
        self.write(self.cook_dir / "Rogue10m/Metadata/zenfs.manifest", b"SYNTHETIC manifest - not a real Zen store")
        return evidence

    def postflight(self, evidence, plan):
        return self.c.postflight({**self.seal, "zen_project": "Rogue10m.AB12CD34"}, plan, evidence)

    def main_postflight(self, evidence):
        argv = ["internal_smoke.py", "--seal", self.seal_pin["path"], "--seal-sha256", self.seal_pin["sha256"],
                "--release", self.release_pin["path"], "--release-sha256", self.release_pin["sha256"],
                "--owner-id", self.release["owner_id"], "--postflight", str(evidence)]
        out = io.StringIO()
        # Reuse the same pinned helper instance whose ORIGINAL points to the
        # fictional original-save root. Production helpers()/guards stay intact.
        with patch.object(m, "helpers", return_value=self.c), patch.object(sys, "argv", argv), contextlib.redirect_stdout(out):
            code = m.main()
        return code, json.loads(out.getvalue())

    def test_positive_full_native43_plan_postflight_stays_unverified(self):
        plan = self.argument_plan()
        self.assertEqual(plan["native43"]["native_tests"], 43)
        self.assertEqual(plan["native43"]["startup_red_rows"], 0)
        args = plan["bcr_args"]
        self.assertEqual(args.count("-platform=Win64"), 1)
        self.assertEqual(args.count("-nocompileeditor"), 1)
        self.assertFalse(any(a.lower().split("=")[0] == "-clean" for a in args))
        self.assertEqual(sum(a.startswith("-CookOutputDir=") for a in args), 1)
        self.assertIn("-CookOutputDir=" + str(Path(self.seal["outputs"]["cook"]) / "Windows"), args)
        ubt = next(a for a in args if a.startswith("-UbtArgs="))
        self.assertEqual(ubt.split().count("-Rebuild"), 1)
        cooker = next(a for a in args if a.startswith("-AdditionalCookerOptions="))
        self.assertEqual(cooker.count("-FOOBASEDIR="), 1)
        self.assertIn('-FOOBASEDIR="' + self.seal["outputs"]["cook_user"] + '"', cooker)
        evidence = self.fake_postflight_outputs(plan)
        code, result = self.main_postflight(evidence)
        self.assertEqual(code, 0, result)
        self.assertEqual(result["status"], "internal_smoke_artifact_built_UNVERIFIED")
        self.assertEqual(result["acceptance_flags"], m.FLAGS)
        self.assertEqual(result["known_limitations"], m.LIMITS)
        self.assertFalse(result["release_eligible"])
        self.assertEqual(result["cook_output_dir"], str(self.cook_dir))
        self.assertEqual(result["isolated_fileopenlog"], self.c.file_pin(self.open_log))

    def test_actual_cooker_rejects_old_missing_duplicate_or_outside_paths(self):
        plan = self.argument_plan(); self.fake_postflight_outputs(plan)
        good = self.cook_command
        cooker = self.seal["outputs"]["cook_user"]
        cookdir = str(self.cook_dir)
        bad = {
            "old_A_cook_root": good.replace(cookdir, self.seal["outputs"]["cook"]),
            "placeholder_is_not_stage_override": good.replace(cookdir, str(Path(self.seal["outputs"]["cook"]) / "[Platform]")),
            "missing_outputdir": good.replace('-outputdir="' + cookdir + '" ', ""),
            "duplicate_outputdir": good + ' -outputdir="' + cookdir + '"',
            "outside_outputdir": good.replace(cookdir, str(self.root / "Saved/Cooked/Windows")),
            "missing_foo": good.replace('-FOOBASEDIR="' + cooker + '" ', ""),
            "duplicate_foo": good + ' -FOOBASEDIR="' + cooker + '"',
            "project_foo": good.replace('-FOOBASEDIR="' + cooker + '"', '-FOOBASEDIR="' + str(self.root) + '"'),
            "other_output_foo": good.replace('-FOOBASEDIR="' + cooker + '"', '-FOOBASEDIR="' + self.seal["outputs"]["qa_user"] + '"'),
            "alternate_saved_base": good + " -FOOUSESAVEDDIR",
            "windows_client": good.replace("-TargetPlatform=Windows", "-TargetPlatform=WindowsClient"),
            "multiple_platforms": good.replace("-TargetPlatform=Windows", "-TargetPlatform=Windows+Linux"),
            "duplicate_platform": good + " -TargetPlatform=Windows",
            "missing_fileopenlog": good.replace(" -fileopenlog", ""),
            "duplicate_fileopenlog": good + " -fileopenlog",
            "duplicate_userdir": good + ' -UserDir="' + cooker + '"',
            "duplicate_project": good + ' "' + self.project + '"',
            "clean_false_is_still_forbidden": good + " -Clean=false",
        }
        for label, command in bad.items():
            with self.subTest(case=label), self.assertRaises(ValueError):
                self.c.exact_cooker_command(command, self.project, cooker, cookdir)
        self.c.exact_cooker_command(good, self.project, cooker, cookdir)

    def test_actual_bcr_rejects_clean_multiplatform_wrong_or_duplicate_target(self):
        plan = self.argument_plan(); evidence = self.fake_postflight_outputs(plan)
        good = self.main_log.read_bytes()
        variants = {
            "top_clean": good.replace(b"BuildCookRun ", b"BuildCookRun -clean "),
            "top_clean_false": good.replace(b"BuildCookRun ", b"BuildCookRun -clean=false "),
            "multi_platform": good.replace(b"-platform=Win64", b"-platform=Win64+Linux"),
            "duplicate_platform": good.replace(b"-platform=Win64", b"-platform=Win64 -platform=Win64"),
            "wrong_target": good.replace(b"-target=Rogue10m", b"-target=Rogue10mEditor"),
            "wrong_config": good.replace(b"-clientconfig=Development", b"-clientconfig=Shipping"),
            "editor_compile": good.replace(b"-nocompileeditor ", b""),
            "old_cook_root": good.replace(("-CookOutputDir=" + str(self.cook_dir)).encode(), ("-CookOutputDir=" + self.seal["outputs"]["cook"]).encode()),
            "duplicate_bcr_command": good + good.splitlines(keepends=True)[0],
        }
        for label, raw in variants.items():
            with self.subTest(case=label):
                self.write(self.main_log, raw)
                with self.assertRaises(ValueError): self.postflight(evidence, plan)
        self.write(self.main_log, good); self.postflight(evidence, plan)

    def test_actual_ubt_requires_single_rebuild_and_exact_game(self):
        plan = self.argument_plan(); evidence = self.fake_postflight_outputs(plan)
        good = self.ubt_path.read_bytes()
        variants = {
            "no_rebuild": good.replace(b" -Rebuild", b""),
            "duplicate_rebuild": good.replace(b" -Rebuild", b" -Rebuild -Rebuild"),
            "clean_instead": good.replace(b" -Rebuild", b" -Clean"),
            "extra_target_descriptor": good.replace(b" -Rebuild", b" -Rebuild -Target=Rogue10mEditor"),
            "wrong_game": good.replace(b"Rogue10m Win64 Development", b"Rogue10mEditor Win64 Development"),
            "wrong_project": good.replace(("-Project=" + self.project).encode(), ("-Project=" + str(self.base / "wrong.uproject")).encode()),
            "no_link": good.replace(b"Link [x64] Rogue10m.exe", b"Target is up to date"),
            "no_metadata": good.replace(b"WriteMetadata Rogue10m.target", b"up-to-date metadata"),
        }
        for label, raw in variants.items():
            with self.subTest(case=label):
                self.write(self.ubt_path, raw)
                with self.assertRaises(ValueError): self.postflight(evidence, plan)
        self.write(self.ubt_path, good)
        self.write(self.ubt_path.with_name("UBA-Rogue10m-Win64-Development-SYNTHETIC_2.txt"), good)
        with self.assertRaisesRegex(ValueError, "Exactly one dedicated"): self.postflight(evidence, plan)

    def test_actual_cook_log_requires_matching_loginit_and_isolated_init(self):
        plan = self.argument_plan(); evidence = self.fake_postflight_outputs(plan)
        good = self.cook_log_path.read_bytes()
        variants = {
            "loginit_old_output": good.replace(str(self.cook_dir).encode(), self.seal["outputs"]["cook"].encode()),
            "init_project_build": good.replace(str(self.open_log).encode(), str(self.root / "Build/Windows/FileOpenOrder/EditorOpenOrder.log").encode()),
            "duplicate_init": good + good.splitlines(keepends=True)[0],
            "missing_init": b"\n".join(good.splitlines()[1:]),
            "duplicate_loginit": good + good.splitlines(keepends=True)[1],
            "cook_error": good + b"LogCook: Error: SYNTHETIC counterexample\n",
        }
        for label, raw in variants.items():
            with self.subTest(case=label):
                self.write(self.cook_log_path, raw)
                with self.assertRaises(ValueError): self.postflight(evidence, plan)
        self.write(self.cook_log_path, good)
        self.write(self.cook_log_path.with_name("Cook-2026.11.13-SYNTHETIC_2.txt"), good)
        with self.assertRaisesRegex(ValueError, "Exactly one retained"): self.postflight(evidence, plan)

    def test_isolated_fileopen_log_requires_fresh_real_nonempty_file(self):
        plan = self.argument_plan(); evidence = self.fake_postflight_outputs(plan)
        self.open_log.unlink()
        with self.assertRaises(ValueError): self.postflight(evidence, plan)
        self.write(self.open_log, b"")
        with self.assertRaisesRegex(ValueError, "empty/stale"): self.postflight(evidence, plan)
        self.write(self.open_log, b"SYNTHETIC")
        os.utime(self.open_log, (1, 1))
        with self.assertRaisesRegex(ValueError, "empty/stale"): self.postflight(evidence, plan)

    def test_zen_metadata_requires_correct_platform_directory_and_identity(self):
        plan = self.argument_plan(); evidence = self.fake_postflight_outputs(plan)
        store = self.cook_dir / "ue.projectstore"
        good = store.read_bytes()
        variants = {
            "foreign_zen_project": {"zenserver": {"projectid": "Rogue10m.DEADBEEF", "oplogid": "Windows"}},
            "foreign_zen_oplog": {"zenserver": {"projectid": "Rogue10m.AB12CD34", "oplogid": "Linux"}},
        }
        for label, value in variants.items():
            with self.subTest(case=label):
                self.write(store, value)
                with self.assertRaisesRegex(ValueError, "metadata/output"): self.postflight(evidence, plan)
        self.write(store, good)
        os.utime(store, (1, 1))
        with self.assertRaisesRegex(ValueError, "metadata/output"): self.postflight(evidence, plan)
        self.write(store, good)
        zenfs = self.cook_dir / "Rogue10m/Metadata/zenfs.manifest"
        zenfs.unlink()
        with self.assertRaises(ValueError): self.postflight(evidence, plan)

    def test_fresh_game_timestamp_archive_and_journal_gates_remain(self):
        plan = self.argument_plan(); evidence = self.fake_postflight_outputs(plan)
        for name in ("Rogue10m.exe", "Rogue10m.target"):
            with self.subTest(case="stale_" + name):
                p = self.root / "Binaries/Win64" / name
                os.utime(p, (1, 1))
                with self.assertRaisesRegex(ValueError, "predate"): self.postflight(evidence, plan)
                os.utime(p, None)
        archived = Path(self.seal["outputs"]["archive"]) / "Windows/Rogue10m/Binaries/Win64/Rogue10m.exe"
        good = archived.read_bytes(); self.write(archived, b"SYNTHETIC foreign archive")
        with self.assertRaisesRegex(ValueError, "Archive inner EXE"): self.postflight(evidence, plan)
        self.write(archived, good)
        journal = evidence / "wrapper-journal.json"
        j = json.loads(journal.read_bytes()); j["buildcookrun_exit"] = 1; self.write(journal, j)
        with self.assertRaisesRegex(ValueError, "execution journal"): self.postflight(evidence, plan)

    def test_prior_game_is_exactly_captured_and_rebuild_can_be_byte_identical(self):
        self.write(self.root / "Binaries/Win64/Rogue10m.exe", b"SYNTHETIC fresh Game - NOT EXECUTABLE")
        self.seal["prior_game_products"] = [self.c.file_pin(self.root / "Binaries/Win64/Rogue10m.exe")]
        self.reseal(); plan = self.argument_plan()
        evidence = self.fake_postflight_outputs(plan)
        code, result = self.main_postflight(evidence)
        self.assertEqual(code, 0, result)
        self.assertEqual(result["game"]["sha256"], self.seal["prior_game_products"][0]["sha256"])

    def test_prior_game_tamper_is_not_silently_reused_or_deleted(self):
        self.write(self.root / "Binaries/Win64/Rogue10m.exe", b"SYNTHETIC prior Game")
        self.seal["prior_game_products"] = [self.c.file_pin(self.root / "Binaries/Win64/Rogue10m.exe")]
        self.reseal(); self.validate()
        self.write(self.root / "Binaries/Win64/Rogue10m.exe", b"SYNTHETIC changed Game")
        with self.assertRaisesRegex(ValueError, "Prior Game product snapshot"): self.validate()
        (self.root / "Binaries/Win64/Rogue10m.exe").unlink()
        with self.assertRaisesRegex(ValueError, "Prior Game product snapshot"): self.validate()

    def test_all_six_input_scopes_detect_added_files_without_ignore(self):
        for scope in self.c.SCOPES:
            with self.subTest(scope=scope):
                p = self.root / scope / "SYNTHETIC-after-freeze.bin"
                self.write(p, b"SYNTHETIC changed input")
                with self.assertRaisesRegex(ValueError, "smoke inputs"): self.validate()
                p.unlink()
        self.validate()

    def test_editor6_all_raw_pins_remain_required(self):
        for pin in self.seal["editor_products"]:
            with self.subTest(product=Path(pin["path"]).name):
                p = Path(pin["path"]); good = p.read_bytes(); self.write(p, good + b"SYNTHETIC changed Editor6")
                with self.assertRaises(ValueError): self.validate()
                self.write(p, good)
        self.validate()

    def test_all_three_protected_save_roots_remain_exact(self):
        for saved in self.seal["protected_saves"]:
            with self.subTest(root=saved["root"]):
                p = Path(saved["root"]) / "SYNTHETIC.sav"
                self.write(p, b"SYNTHETIC protected save mutation")
                with self.assertRaisesRegex(ValueError, "protected SaveGames"): self.validate()
                p.unlink(); p.parent.rmdir()
        self.validate()

    def test_output6_freshness_and_common_owner_root_are_enforced(self):
        good = copy.deepcopy(self.seal["outputs"])
        for key in self.c.OUTPUTS:
            with self.subTest(case="existing_" + key):
                Path(good[key]).mkdir()
                with self.assertRaisesRegex(ValueError, "Fresh outputs"): self.validate()
                Path(good[key]).rmdir()
        for label, path in (("project", self.root / "cook_user"), ("other_output", Path(good["qa_user"])),
                            ("outside_parent", self.area / "different/cook_user")):
            with self.subTest(case=label):
                self.seal["outputs"]["cook_user"] = str(path); self.reseal()
                with self.assertRaises(ValueError): self.validate()
        self.seal["outputs"] = good; self.reseal(); self.validate()

    def test_old_A_startup_red_and_old15_args_still_fail(self):
        refs = self.seal["native43"]
        logpath = Path(refs["log"]["path"]); good = logpath.read_bytes()
        self.seal["native43"]["log"] = self.write(logpath, b"LogAutomationTest: Error: SYNTHETIC old A startup row\n" + good)
        self.reseal()
        with self.assertRaisesRegex(ValueError, "startup red"): self.validate()
        self.seal["native43"]["log"] = self.write(logpath, good)
        for key in ("process", "result"):
            value = self.c.read_json_pin(refs[key]); value["command"] = value["command"][:15]
            self.seal["native43"][key] = self.write(refs[key]["path"], value)
        self.reseal()
        with self.assertRaisesRegex(ValueError, "18-argument"): self.validate()

    def test_ui7_map_extent_and_release_are_never_upgraded(self):
        for label, mutate in (
            ("UI7", lambda s: s["acceptance_flags"].update(UI7="PASSED")),
            ("visual", lambda s: s["acceptance_flags"].update(visual_review="PASSED")),
            ("release", lambda s: s.update(release_eligible=True)),
            ("crop", lambda s: s["known_limitations"].update(map_crop="PERFORMED")),
            ("npc", lambda s: s["known_limitations"].update(npc_placement="PERFORMED")),
            ("extent", lambda s: s["known_limitations"].update(map_extent_m=[900, 900])),
        ):
            with self.subTest(case=label):
                good = copy.deepcopy(self.seal); mutate(self.seal); self.reseal()
                with self.assertRaises(ValueError): self.validate()
                self.seal = good; self.reseal()
        self.validate()

    def test_offline_evidence_writer_refuses_parent_traversal_and_existing_roots(self):
        import run_offline_e as runner
        for value in ("evidence/../outside", "evidence/new/../../outside", str(self.root / "Source/out"),
                      str(m.HERE / "evidence/offline-run01"), str(m.HERE)):
            with self.subTest(path=value), self.assertRaises(ValueError):
                runner.fresh_output(value)
        self.assertEqual(runner.fresh_output(str(m.HERE / "evidence/SYNTHETIC-new-output")),
                         m.HERE / "evidence/SYNTHETIC-new-output")


if __name__ == "__main__":
    unittest.main(verbosity=2)
