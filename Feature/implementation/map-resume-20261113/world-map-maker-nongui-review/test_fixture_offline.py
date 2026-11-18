"""Pure guard/contract checks; never imports Unreal or queries/starts processes."""
import ast
import importlib.util
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

HERE = Path(__file__).parent
SPEC = importlib.util.spec_from_file_location("map_maker_api_fixture", HERE / "map_maker_api_fixture.py")
FIXTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURE)
CANDIDATE = "/Game/Rogue10m/Maps/QA/MapMakerMoveReload_" + "a" * 32
ROOT = Path("C:/Users/PC/AppData/Local/Temp/R10MMQA20261113A/Project")
ENGINE = Path("D:/Program Files/UE_5.8")
EXE = ENGINE / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"


class OfflineGuards(unittest.TestCase):
    def test_default_plan_keeps_unreal_unimported_and_qa_authorization_false(self):
        self.assertNotIn("unreal", sys.modules)
        config = json.loads((HERE / "config.UNEXECUTED.json").read_text(encoding="utf-8"))
        self.assertIs(config["root_exclusive_qa_execution_authorized"], False)
        result = FIXTURE.plan(config)
        self.assertEqual(result["status"], "UNEXECUTED")
        self.assertIs(result["panel_tested"], False)
        self.assertIs(result["actual_map_maker_model_tested"], False)
        self.assertNotIn("unreal", sys.modules)

    def test_unauthorized_preflight_stops_before_filesystem_snapshot(self):
        with patch.object(FIXTURE, "protected_snapshot", side_effect=AssertionError("unexpected read")):
            with self.assertRaisesRegex(FIXTURE.FixtureError, "Root must explicitly authorize"):
                FIXTURE.preflight({"root_exclusive_qa_execution_authorized": False}, "")

    def test_only_fresh_qa_namespace_is_accepted(self):
        paths = FIXTURE.package_paths(CANDIDATE)
        self.assertEqual(len(paths), 4)
        for package in ("/Game/Rogue10m/Maps/OpenRunMap", "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A",
                        CANDIDATE + "/../Other", CANDIDATE + "extra"):
            with self.subTest(package=package), self.assertRaises(FIXTURE.FixtureError):
                FIXTURE.package_paths(package)
        tail = CANDIDATE[len("/Game/"):]
        self.assertTrue(FIXTURE.belongs(CANDIDATE + "_BuiltData", CANDIDATE))
        self.assertTrue(FIXTURE.belongs("/Game/__ExternalActors__/" + tail + "/A/BC/FILE", CANDIDATE))
        self.assertTrue(FIXTURE.belongs("/Game/__ExternalObjects__/" + tail + "/FILE", CANDIDATE))
        self.assertFalse(FIXTURE.belongs("/Game/__ExternalActors__/" + tail + "suffix/A", CANDIDATE))
        self.assertFalse(FIXTURE.belongs("/Game/__ExternalActors__/Rogue10m/Maps/OpenRunMap/A", CANDIDATE))

    def test_clipped_grid_and_noninteger_rejection(self):
        self.assertEqual(FIXTURE.cell_center([-4, 3]), [-83400, 83400])
        self.assertEqual(FIXTURE.cell_center([1, 0]), [38400, 12800])
        for cell in ([4, 0], [True, 0], [1.0, 0], [1]):
            with self.subTest(cell=cell), self.assertRaises(FIXTURE.FixtureError):
                FIXTURE.cell_center(cell)

    def test_commandlet_only_no_regular_editor_or_missing_flags(self):
        valid = "-run=pythonscript -Unattended -NullRHI"
        FIXTURE.require_commandlet(valid, EXE, ENGINE)
        for command, executable in ((valid, ENGINE / "Engine/Binaries/Win64/UnrealEditor.exe"),
                                    ("-run=pythonscript -NullRHI", EXE),
                                    ("-run=pythonscript -Unattended", EXE),
                                    (valid + " -run=Other", EXE),
                                    ("-run=Other -Unattended -NullRHI", EXE)):
            with self.subTest(command=command, executable=executable), self.assertRaises(FIXTURE.FixtureError):
                FIXTURE.require_commandlet(command, executable, ENGINE)

    def test_descriptors_are_not_loaded_or_boolean_success(self):
        class Library:
            result = ["descriptor"]

            @classmethod
            def get_actor_descs(cls):
                return cls.result

        class FakeUnreal:
            WorldPartitionBlueprintLibrary = Library

        self.assertEqual(FIXTURE.get_descs(FakeUnreal), ["descriptor"])
        Library.result = (True, ["descriptor"])
        self.assertEqual(FIXTURE.get_descs(FakeUnreal), ["descriptor"])
        for value in (None, False, (False, [])):
            Library.result = value
            with self.subTest(value=value), self.assertRaises(FIXTURE.FixtureError):
                FIXTURE.get_descs(FakeUnreal)

    def test_cold_boundary_requires_exact_confirmed_exited_identity(self):
        previous = {"process_identity": {"pid": 123, "creation_filetime_100ns": "123456"},
                    "finished_utc": "2026-11-13T12:00:00+00:00"}
        receipt = {"schema_version": 1, "status": "owned_process_exited",
                   "identity": dict(previous["process_identity"]), "exit_code": 0,
                   "exit_confirmed": True, "receipt_written_after_wait_for_exit": True,
                   "project_file": str(ROOT / "Rogue10m.uproject"), "executable": str(EXE),
                   "ended_utc": "2026-11-13T12:00:01+00:00"}
        FIXTURE.validate_move_exit(receipt, previous, ROOT, ENGINE)
        for replacement in ({"exit_confirmed": False}, {"exit_code": 1},
                            {"identity": {"pid": 123, "creation_filetime_100ns": "different"}},
                            {"project_file": "D:/Project/Rogue10m/Rogue10m.uproject"},
                            {"receipt_written_after_wait_for_exit": False},
                            {"ended_utc": "2026-11-13T11:59:59+00:00"}):
            with self.subTest(replacement=replacement), self.assertRaises(FIXTURE.FixtureError):
                FIXTURE.validate_move_exit({**receipt, **replacement}, previous, ROOT, ENGINE)

    def test_no_forbidden_gui_or_process_launch_calls_in_fixture(self):
        tree = ast.parse((HERE / "map_maker_api_fixture.py").read_text(encoding="utf-8"))
        calls = {x.func.attr for x in ast.walk(tree) if isinstance(x, ast.Call) and isinstance(x.func, ast.Attribute)}
        self.assertFalse(calls & {"set_selected_level_actors", "select_actor", "select_nothing", "open_editor_for_assets",
                                 "spawn_and_register_tab", "load_asset", "Popen", "system", "kill", "terminate"})
        self.assertIn("set_actor_transform", calls)
        self.assertIn("save_packages", calls)
        self.assertIn("load_actors", calls)


if __name__ == "__main__":
    unittest.main(verbosity=2)
