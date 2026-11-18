"""Offline stdlib contract tests. Never import Unreal or run an Unreal process.

Run with the engine's standalone Python, using -B to suppress bytecode caches.
"""
from __future__ import annotations

import ast
import builtins
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock

sys.dont_write_bytecode = True
SCRIPT = Path(__file__).resolve().parents[1] / "Editor/CreateIntroVillageQuestDefinition20261113.py"
SOURCE = SCRIPT.read_text(encoding="utf-8")
DESTINATION = "/Game/Rogue10m/Data/Quests/DA_IntroSupply_OfflineTest"
WORLD_PACKAGE = "/Game/Rogue10m/Maps/OwnerChosenMap"
WORLD_OBJECT = WORLD_PACKAGE + ".OwnerChosenMap"


def load_helper():
    spec = importlib.util.spec_from_file_location("intro_authoring_offline", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def vector(x=0, y=0, z=0):
    return types.SimpleNamespace(x=x, y=y, z=z)


class Properties:
    def __init__(self, **properties):
        self.properties = properties

    def get_editor_property(self, name):
        return self.properties[name]

    def set_editor_property(self, name, value):
        self.properties[name] = value


class MockWorld:
    def __init__(self, path=WORLD_OBJECT):
        self.path = path

    def get_path_name(self):
        return self.path


class Definition(Properties):
    def __init__(self, environment, *, default=False):
        super().__init__(last_validation_passed=False)
        self.environment = environment
        self.default = default
        self.path = DESTINATION + ".DA_IntroSupply_OfflineTest"

    def get_class(self):
        return self.environment.returned_class

    def get_path_name(self):
        return self.path

    def reset_to_intro_supply_example(self):
        if self.default:
            raise AssertionError("The CDO must never be reset.")
        self.environment.events.append("reset")
        rows = [Properties(enabled=index == 0, supply_clue_objective_index=-1,
                           offers_clue_analysis=False, accepts_clue_report=False,
                           combat_objective_index=-1, combat_monster_data=None,
                           reward_item=None, reward_gold=0) for index in range(5)]
        rows[0].properties.update(quest_id="Q_Intro_supply", npc_name="서린", reward_gold=40,
                                  objective_labels=["First supply box", "Second supply box"],
                                  objective_locations=[vector(-500, 400, 100), vector(500, 400, 100)],
                                  npc_location=vector(0, 0, 20))
        self.properties.update(quests=rows, supported_world=None, layout_origin_cm=vector(),
                               layout_rotation=types.SimpleNamespace(pitch=0, yaw=0, roll=0))
        if self.environment.reset_raises:
            raise RuntimeError("Mock reset failure")

    def set_editor_property(self, name, value):
        if self.default:
            raise AssertionError("The CDO must never be edited.")
        self.environment.events.append("set:" + name)
        super().set_editor_property(name, value)

    def validate_configuration(self):
        if self.default:
            raise AssertionError("The CDO must never be validated by this tool.")
        self.environment.events.append("validate")
        self.properties["last_validation_passed"] = self.environment.validation_passed
        # A deliberately misleading localized diagnostic must not decide success.
        self.properties["last_validation_result"] = self.environment.validation_text
        if self.environment.after_validation:
            self.environment.after_validation(self)

    def get_enabled_quest_count(self):
        self.environment.events.append("count")
        if self.environment.count_override is not None:
            return self.environment.count_override
        return sum(row.properties["enabled"] for row in self.properties["quests"])


class OfflineEditor:
    """A narrow Unreal substitute; map/actor/save-all APIs deliberately do not exist."""
    def __init__(self, root):
        self.root = root
        self.project = root / "Rogue10m.uproject"
        self.project.write_text('{"Modules":[{"Name":"Rogue10m","Type":"Runtime"}]}', encoding="utf-8")
        self.events = []
        self.saved = []
        self.created = None
        self.factory = None
        self.definition_class = object()
        self.returned_class = self.definition_class
        self.class_available = True
        self.pie = False
        self.registered_existing = False
        self.loaded_object = None
        self.loaded_package = None
        self.world = MockWorld()
        self.after_world_load = None
        self.after_validation = None
        self.validation_passed = True
        self.validation_text = "not a success marker"
        self.count_override = None
        self.reset_raises = False
        self.creation_returns_none = False
        self.creation_raises = False
        self.return_cdo = False
        self.returned_path = None
        self.save_result = True
        self.save_raises = False
        self.default = Definition(self, default=True)
        module = types.ModuleType("unreal")
        module.Paths = types.SimpleNamespace(get_project_file_path=lambda: str(self.project),
                                            convert_relative_path_to_full=lambda value: value)
        module.LevelEditorSubsystem = object()
        module.get_editor_subsystem = self.get_subsystem
        module.load_class = lambda outer, path: self.definition_class if self.class_available else None
        module.get_default_object = lambda asset_class: self.default
        module.find_object = self.find_object
        module.find_package = self.find_package
        module.load_object = self.load_world
        module.World = MockWorld
        module.DataAssetFactory = self.make_factory
        module.AssetToolsHelpers = types.SimpleNamespace(get_asset_tools=lambda: self)
        module.EditorAssetLibrary = types.SimpleNamespace(does_asset_exist=self.asset_exists,
                                                         save_loaded_asset=self.save_loaded_asset)
        self.module = module

    def get_subsystem(self, subsystem):
        self.events.append("idle_check")
        return types.SimpleNamespace(is_in_play_in_editor=lambda: self.pie)

    def asset_exists(self, package):
        self.events.append("registry_check")
        return self.registered_existing

    def find_object(self, outer, object_path, *, follow_redirectors):
        self.events.append("object_check")
        return self.loaded_object

    def find_package(self, package):
        self.events.append("package_check")
        return self.loaded_package

    def load_world(self, outer, object_path, *, type, follow_redirectors):
        self.events.append("load_world")
        if type is not MockWorld or follow_redirectors is not False or object_path != WORLD_OBJECT:
            raise AssertionError("World must be explicitly class-checked and redirects must not change selection.")
        if self.after_world_load:
            self.after_world_load()
        return self.world

    def make_factory(self):
        self.events.append("factory")
        self.factory = Properties()
        return self.factory

    def create_asset(self, name, path, asset_class, factory, *, overwrite_existing):
        self.events.append("create")
        if overwrite_existing is not False:
            raise AssertionError("Creation must never request overwriting an existing asset.")
        if factory.get_editor_property("data_asset_class") is not self.definition_class:
            raise AssertionError("Factory must use the compiled quest definition class.")
        if self.creation_raises:
            raise RuntimeError("Mock factory creation exception")
        if self.creation_returns_none:
            return None
        if self.return_cdo:
            return self.default
        self.created = Definition(self)
        self.created.path = self.returned_path or path + "/" + name + "." + name
        return self.created

    def save_loaded_asset(self, asset, *, only_if_is_dirty):
        self.events.append("save")
        self.saved.append(asset)
        if only_if_is_dirty is not False or asset is not self.created:
            raise AssertionError("Only the exact newly created definition can be saved.")
        if self.save_raises:
            raise OSError("Mock save exception")
        return self.save_result


class IntroAuthoringTests(unittest.TestCase):
    def setUp(self):
        self.helper = load_helper()
        self.temporary = tempfile.TemporaryDirectory(prefix="RogueIntroAuthoringOffline_")
        self.root = Path(self.temporary.name).resolve()
        if self.root.parent != Path(tempfile.gettempdir()).resolve() or not self.root.name.startswith("RogueIntroAuthoringOffline_"):
            raise AssertionError("Temporary cleanup boundary escaped the exact offline test namespace.")
        self.addCleanup(self.temporary.cleanup)
        self.helper.ROOT = self.root
        self.editor = OfflineEditor(self.root)
        self.unreal_patch = mock.patch.dict(sys.modules, {"unreal": self.editor.module})
        self.unreal_patch.start()
        self.addCleanup(self.unreal_patch.stop)

    def execute(self, **changes):
        arguments = dict(destination=DESTINATION, supported_world=WORLD_PACKAGE,
                         execute=True, owner_handoff="offline mock owner", build_verified=True)
        arguments.update(changes)
        return self.helper.create_intro_definition(**arguments)

    def assert_rejected_without_save(self, pattern, **changes):
        with self.assertRaisesRegex(self.helper.AuthoringError, pattern):
            self.execute(**changes)
        self.assertEqual(self.editor.saved, [])

    def test_import_and_main_have_no_unreal_or_mutation_effects(self):
        original_import = builtins.__import__

        def guarded_import(name, *arguments, **keywords):
            if name == "unreal":
                raise AssertionError("Import/default main must not import Unreal.")
            return original_import(name, *arguments, **keywords)

        for module_name in ("offline_import", "__main__"):
            with self.subTest(module_name=module_name), mock.patch("builtins.__import__", guarded_import), \
                    mock.patch.object(Path, "mkdir", side_effect=AssertionError("No directory creation")), \
                    mock.patch.object(Path, "write_text", side_effect=AssertionError("No text writes")), \
                    mock.patch.object(Path, "write_bytes", side_effect=AssertionError("No binary writes")):
                output = io.StringIO()
                namespace = {"__name__": module_name, "__file__": str(SCRIPT)}
                with contextlib.redirect_stdout(output):
                    exec(compile(SOURCE, str(SCRIPT), "exec"), namespace)
                if module_name == "__main__":
                    self.assertEqual(json.loads(output.getvalue())["status"], "plan_only_not_executed")
                else:
                    self.assertEqual(output.getvalue(), "")

    def test_default_call_and_plan_are_pure(self):
        before = sorted(path.relative_to(self.root) for path in self.root.rglob("*"))
        with mock.patch.dict(sys.modules, {"unreal": None}):
            report = self.helper.create_intro_definition()
            planned = self.helper.plan(destination=DESTINATION, supported_world=WORLD_OBJECT)
        self.assertEqual(report["status"], "plan_only_not_executed")
        self.assertIsNone(report["destination"])
        self.assertEqual(planned["supported_world"], WORLD_PACKAGE)
        self.assertEqual(planned["expected"]["enabled_quests"], 1)
        self.assertFalse(planned["runtime_verified"])
        self.assertFalse(planned["map_modified"])
        self.assertEqual(before, sorted(path.relative_to(self.root) for path in self.root.rglob("*")))
        self.assertEqual(self.editor.events, [])

    def test_missing_handoff_and_build_are_rejected_before_unreal_import(self):
        for changes in ({"owner_handoff": ""}, {"owner_handoff": " \t"}, {"owner_handoff": True},
                        {"build_verified": False}, {"build_verified": "true"}, {"build_verified": 1}):
            with self.subTest(changes=changes), mock.patch.dict(sys.modules, {"unreal": None}):
                self.assert_rejected_without_save("owner_handoff", **changes)
        self.assertEqual(self.editor.events, [])

    def test_missing_world_or_destination_is_rejected_before_unreal_import(self):
        for changes in ({"destination": None}, {"supported_world": None}):
            with self.subTest(changes=changes), mock.patch.dict(sys.modules, {"unreal": None}):
                self.assert_rejected_without_save("explicit NEW destination", **changes)
        self.assertEqual(self.editor.events, [])

    def test_nonboolean_execute_is_rejected(self):
        for value in ("true", 1, None):
            with self.subTest(value=value):
                self.assert_rejected_without_save("explicit boolean", execute=value)

    def test_bad_destination_paths_and_vendor_namespace_are_rejected(self):
        paths = ("", " /Game/Rogue10m/DA_X", "/Game/Other/DA_X", "/Engine/DA_X",
                 "/Game/Rogue10m/../DA_X", "/Game/Rogue10m//DA_X", "/Game/Rogue10m/DA_X.Other",
                 "/Game/Rogue10m/DA_X:Subobject", "/Game/Rogue10m/Vendor/DA_X",
                 "/Game/Rogue10m/Marketplace/DA_X", "/Game/Rogue10m/Data/Quest",
                 "/Game/Rogue10m/Data/DA_X;save", "/Game/Rogue10m/Data/DA_X.umap")
        for value in paths:
            with self.subTest(value=value):
                self.assert_rejected_without_save(".", destination=value)
        self.assertEqual(self.editor.events, [])

    def test_bad_world_path_or_same_package_is_rejected(self):
        for value in ("", "/Game/Maps/World.Other", "/Game/Maps/../World", DESTINATION):
            with self.subTest(value=value):
                self.assert_rejected_without_save(".", supported_world=value)
        self.assertEqual(self.editor.events, [])

    def test_wrong_project_and_pie_are_rejected(self):
        self.editor.module.Paths.get_project_file_path = lambda: str(self.root / "Other.uproject")
        self.assert_rejected_without_save("Wrong project")
        self.editor.module.Paths.get_project_file_path = lambda: str(self.editor.project)
        self.editor.pie = True
        self.assert_rejected_without_save("idle editor")
        self.assertNotIn("factory", self.editor.events)

    def test_missing_compiled_class_is_rejected(self):
        self.editor.class_available = False
        self.assert_rejected_without_save("class unavailable")
        self.assertNotIn("factory", self.editor.events)

    def test_each_missing_compiled_method_is_rejected_before_factory(self):
        for name in self.helper._METHODS:
            with self.subTest(name=name):
                original = getattr(self.editor.default, name)
                setattr(self.editor.default, name, None)
                try:
                    self.assert_rejected_without_save("methods unavailable")
                finally:
                    setattr(self.editor.default, name, original)
        self.assertNotIn("factory", self.editor.events)

    def test_missing_machine_validation_property_is_rejected(self):
        self.editor.default.properties.clear()
        self.assert_rejected_without_save("last_validation_passed")
        self.assertNotIn("factory", self.editor.events)

    def test_registered_and_loaded_collisions_are_rejected(self):
        for name in ("registered_existing", "loaded_object", "loaded_package"):
            with self.subTest(name=name):
                setattr(self.editor, name, True)
                try:
                    self.assert_rejected_without_save("already exists or is loaded")
                finally:
                    setattr(self.editor, name, False if name == "registered_existing" else None)
        self.assertNotIn("factory", self.editor.events)

    def test_on_disk_package_collisions_are_rejected(self):
        target = self.root / "Content" / DESTINATION.removeprefix("/Game/")
        target.parent.mkdir(parents=True)
        for suffix in (".uasset", ".umap", ".uexp", ".ubulk"):
            with self.subTest(suffix=suffix):
                file = target.with_suffix(suffix)
                file.write_bytes(b"existing package must remain unchanged")
                try:
                    self.assert_rejected_without_save("already has package files")
                    self.assertEqual(file.read_bytes(), b"existing package must remain unchanged")
                finally:
                    file.unlink()
        self.assertNotIn("factory", self.editor.events)

    def test_collision_loaded_as_world_dependency_is_rechecked(self):
        self.editor.after_world_load = lambda: setattr(self.editor, "loaded_package", object())
        self.assert_rejected_without_save("already exists or is loaded")
        self.assertIn("load_world", self.editor.events)
        self.assertNotIn("factory", self.editor.events)

    def test_collision_during_factory_setup_is_rechecked_before_creation(self):
        original = self.editor.make_factory

        def introduce_collision():
            self.editor.loaded_package = object()
            return original()

        self.editor.module.DataAssetFactory = introduce_collision
        self.assert_rejected_without_save("already exists or is loaded")
        self.assertIn("factory", self.editor.events)
        self.assertNotIn("create", self.editor.events)

    def test_redirected_project_owned_content_root_is_rejected(self):
        original = Path.resolve

        def redirected(path, *arguments, **keywords):
            if path == self.root / "Content/Rogue10m":
                return self.root / "VendorOutsideContent"
            return original(path, *arguments, **keywords)

        with mock.patch.object(Path, "resolve", redirected):
            self.assert_rejected_without_save("outside project-owned")
        self.assertNotIn("factory", self.editor.events)

    def test_missing_wrong_class_or_redirected_world_is_rejected(self):
        for value in (None, Properties(), MockWorld("/Game/Other.Other")):
            with self.subTest(world=value):
                self.editor.world = value
                self.assert_rejected_without_save("exact World asset")
        self.assertNotIn("factory", self.editor.events)

    def test_creation_failure_does_not_save(self):
        self.editor.creation_returns_none = True
        self.assert_rejected_without_save("creation failed")

    def test_creation_exception_reports_target_without_save_or_cleanup(self):
        self.editor.creation_raises = True
        self.assert_rejected_without_save("Asset creation raised.*no save.*Target:")

    def test_factory_returned_wrong_path_class_or_cdo_is_never_edited(self):
        cases = (("returned_path", "/Game/Existing.Existing"), ("returned_class", object()), ("return_cdo", True))
        for name, value in cases:
            with self.subTest(name=name):
                original = getattr(self.editor, name)
                setattr(self.editor, name, value)
                try:
                    self.assert_rejected_without_save("exact requested path/class")
                    self.assertNotIn("reset", self.editor.events)
                finally:
                    setattr(self.editor, name, original)

    def test_reset_failure_leaves_inspectable_unsaved_draft(self):
        self.editor.reset_raises = True
        self.assert_rejected_without_save("No save was attempted.*Target:")
        self.assertIsNotNone(self.editor.created)

    def test_false_validation_bool_blocks_save_even_with_success_text(self):
        self.editor.validation_passed = False
        self.editor.validation_text = "DefinitionValid: success"
        self.assert_rejected_without_save("Native configuration validation failed")

    def test_structural_count_and_first_enabled_slot_are_checked(self):
        mutations = (lambda asset: setattr(self.editor, "count_override", 2),
                     lambda asset: asset.properties["quests"].pop(),
                     lambda asset: asset.properties["quests"][1].properties.update(enabled=True),
                     lambda asset: (asset.properties["quests"][0].properties.update(enabled=False),
                                    asset.properties["quests"][1].properties.update(enabled=True)))
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                self.editor.count_override = None
                self.editor.after_validation = mutate
                self.assert_rejected_without_save("five stable slots|first intro quest")

    def test_story_combat_and_item_roles_are_checked_in_disabled_slots_too(self):
        cases = (("supply_clue_objective_index", 1), ("offers_clue_analysis", True),
                 ("accepts_clue_report", True), ("combat_objective_index", 0),
                 ("combat_monster_data", object()), ("reward_item", object()))
        for field, value in cases:
            with self.subTest(field=field):
                self.editor.after_validation = lambda asset, f=field, v=value: asset.properties["quests"][4].properties.update({f: v})
                self.assert_rejected_without_save("story roles|monster|item or book")

    def test_enabled_quest_reward_identity_and_two_pickups_are_checked(self):
        cases = (("reward_gold", 0), ("reward_gold", -5), ("reward_gold", True),
                 ("quest_id", "Q_Legacy"), ("npc_name", ""), ("objective_labels", ["one"]),
                 ("objective_labels", ["one", " "]), ("objective_locations", [vector()]),
                 ("objective_locations", [vector(), vector()]),
                 ("objective_locations", [vector(float("nan")), vector(10)]),
                 ("objective_locations", [vector(14000), vector(-14000)]))
        for field, value in cases:
            with self.subTest(field=field, value=value):
                self.editor.after_validation = lambda asset, f=field, v=value: asset.properties["quests"][0].properties.update({f: v})
                self.assert_rejected_without_save("gold-only|stable quest ID|pickups|finite")

    def test_anchor_and_selected_world_are_checked_after_native_validation(self):
        cases = (("layout_origin_cm", vector(100, 0, 0)),
                 ("layout_rotation", types.SimpleNamespace(pitch=0, yaw=90, roll=0)),
                 ("supported_world", None), ("supported_world", MockWorld("/Game/Other.Other")))
        for field, value in cases:
            with self.subTest(field=field):
                self.editor.after_validation = lambda asset, f=field, v=value: asset.properties.update({f: v})
                self.assert_rejected_without_save("zero layout|selected supported World")

    def test_localized_npc_display_text_does_not_duplicate_native_content_gate(self):
        self.editor.after_validation = lambda asset: asset.properties["quests"][0].properties.update(npc_name="Serin")
        self.assertEqual(self.execute()["status"], "definition_saved")

    def test_success_saves_exactly_one_new_asset_after_all_checks(self):
        existing_map = self.root / "Content/Rogue10m/Maps/OwnerChosenMap.umap"
        existing_map.parent.mkdir(parents=True)
        existing_map.write_bytes(b"original map")
        before = sorted(path.relative_to(self.root) for path in self.root.rglob("*"))
        report = self.execute()
        self.assertEqual(report["status"], "definition_saved")
        self.assertTrue(report["native_validation_passed"])
        self.assertEqual(self.editor.saved, [self.editor.created])
        self.assertEqual(report["saved_assets"], [DESTINATION + ".DA_IntroSupply_OfflineTest"])
        self.assertIs(self.editor.created.properties["supported_world"], self.editor.world)
        self.assertFalse(report["map_modified"])
        self.assertFalse(report["runtime_verified"])
        self.assertLess(self.editor.events.index("reset"), self.editor.events.index("set:supported_world"))
        self.assertLess(self.editor.events.index("set:supported_world"), self.editor.events.index("validate"))
        self.assertLess(self.editor.events.index("validate"), self.editor.events.index("count"))
        self.assertLess(self.editor.events.index("count"), self.editor.events.index("save"))
        self.assertEqual(self.editor.events[-1], "save")
        self.assertEqual(existing_map.read_bytes(), b"original map")
        self.assertEqual(before, sorted(path.relative_to(self.root) for path in self.root.rglob("*")))
        self.assertEqual(self.editor.default.properties, {"last_validation_passed": False})

    def test_save_false_and_exception_report_uncertain_outcome(self):
        for fail_with_exception in (False, True):
            with self.subTest(exception=fail_with_exception):
                self.editor.saved.clear()
                self.editor.save_result = False
                self.editor.save_raises = fail_with_exception
                with self.assertRaisesRegex(self.helper.AuthoringError, "Save was attempted; its outcome may be uncertain"):
                    self.execute()
                self.assertEqual(self.editor.saved, [self.editor.created])

    def test_ast_has_no_map_actor_savegame_or_process_mutation_calls(self):
        tree = ast.parse(SOURCE)
        attribute_calls = {node.func.attr for node in ast.walk(tree)
                           if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)}
        prohibited = {"open_level", "load_level", "save_current_level", "save_all_dirty_levels",
                      "save_dirty_packages", "save_directory", "spawn_actor_from_class",
                      "destroy_actor", "save_game_to_slot", "delete_asset", "rename_asset",
                      "write_text", "write_bytes", "mkdir", "unlink", "rmdir", "Popen", "run", "system"}
        self.assertFalse(attribute_calls & prohibited)
        save_calls = [node for node in ast.walk(tree) if isinstance(node, ast.Call)
                      and isinstance(node.func, ast.Attribute) and node.func.attr.startswith("save_")]
        self.assertEqual([node.func.attr for node in save_calls], ["save_loaded_asset"])
        imported = {node.names[0].name for node in ast.walk(tree) if isinstance(node, ast.Import)}
        self.assertFalse(imported & {"subprocess", "os", "unreal_engine"})


if __name__ == "__main__":
    unittest.main(verbosity=2)
