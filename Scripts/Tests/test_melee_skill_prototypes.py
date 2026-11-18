"""Pure recipe checks and mocked Unreal authoring contracts. No Editor/UE build is used."""
import copy
from dataclasses import replace
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[2]
EDITOR = ROOT / "Scripts/Editor"
TEST_ROOT = (ROOT / "Delivery/melee-prototype-test-fixtures").resolve()


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


RECIPES = load_module("test_melee_recipes", EDITOR / "MeleeSkillPrototypeRecipes20261111.py")
STAGE = load_module("test_melee_stage", EDITOR / "StageMeleeSkillPrototypes20261111.py")


class FakeObject:
    def __init__(self, path):
        self.path = path

    def get_path_name(self):
        return self.path + "." + self.path.rsplit("/", 1)[-1]

    def __deepcopy__(self, memo):
        return self  # External UObject references stay shared when a DataAsset is duplicated.


class FakeAsset(FakeObject):
    def __init__(self, path, properties):
        super().__init__(path)
        self.properties = properties
        self.metadata = {}

    def get_editor_property(self, name):
        if name not in self.properties:
            raise AttributeError(name)
        return self.properties[name]

    def set_editor_property(self, name, value):
        if name not in self.properties:
            raise AttributeError(name)
        self.properties[name] = value


class FakeSkill(FakeAsset):
    pass


def stable(value):
    if isinstance(value, FakeObject):
        return value.get_path_name()
    if isinstance(value, dict):
        return {str(key): stable(item) for key, item in value.items()}
    if isinstance(value, list):
        return [stable(item) for item in value]
    return value


class FakeEntry:
    def __init__(self):
        self.properties = {}

    def set_editor_property(self, name, value):
        self.properties[name] = value

    def export_text(self):
        return json.dumps(stable(self.properties), sort_keys=True)


class FakeProfile(FakeAsset):
    def validate_active_technique_catalog(self):
        # A stand-in return contract only; real C++ catalog validation remains an integration check.
        entries = self.properties["active_technique_catalog"]
        return len(entries) == 10, "mock-count-check"


class FakeLibrary:
    def __init__(self):
        self.assets = {}
        self.duplicates = []
        self.saves = []
        self.directories = []
        self.fail_save = False
        self.damage_mutation_on_duplicate = False

    def load_asset(self, path):
        return self.assets.get(path.split(".", 1)[0])

    def does_asset_exist(self, path):
        return self.load_asset(path) is not None

    def make_directory(self, path):
        self.directories.append(path)
        return True

    def duplicate_asset(self, source, target):
        old = self.load_asset(source)
        if old is None or target in self.assets:
            return None
        new = type(old)(target, copy.deepcopy(old.properties))
        if self.damage_mutation_on_duplicate and isinstance(new, FakeSkill):
            new.properties["damage"] += 1
        self.assets[target] = new
        self.duplicates.append((source, target))
        return new

    def set_metadata_tag(self, asset, name, value):
        asset.metadata[name] = value

    def get_metadata_tag(self, asset, name):
        return asset.metadata.get(name, "")

    def save_loaded_asset(self, asset, only_if_dirty):
        if self.fail_save:
            return False
        self.saves.append(asset.path)
        return True


class AuthoringContractTests(unittest.TestCase):
    def setUp(self):
        TEST_ROOT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="case_", dir=TEST_ROOT)
        self.project = Path(self.temp.name).resolve()
        self.assertTrue(self.project.is_relative_to(TEST_ROOT))
        for name in ["Rogue10mAttackSkillData.h", "Rogue10mSkillLoadoutDataAsset.h"]:
            dest = self.project / "Source/Rogue10m/Data" / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / "Source/Rogue10m/Data" / name, dest)
        self.library = FakeLibrary()
        self.unreal = SimpleNamespace(
            Object=FakeObject, Text=str, Name=str, Rogue10mAttackSkillData=FakeSkill,
            Rogue10mWeaponSkillProfileDataAsset=FakeProfile, Rogue10mActiveTechniqueCatalogEntry=FakeEntry,
            Rogue10mSkillSlotPolicy=SimpleNamespace(COMMON="Common", MOUSE_CHORD_ONLY="MouseChordOnly", RIGHT_CHARGE_ONLY="RightChargeOnly"),
            Rogue10mAttackHitMode=SimpleNamespace(SINGLE="Single", CONTINUOUS="Continuous"),
            EditorAssetLibrary=self.library, Paths=SimpleNamespace(project_dir=lambda: str(self.project)))
        self.skill_fields = STAGE.reflected_fields(self.project, "Rogue10mAttackSkillData.h", "class ROGUE10M_API URogue10mAttackSkillData")
        for index, path in enumerate(sorted({row.source_package for row in RECIPES.recipes()})):
            properties = {name: None for name in self.skill_fields}
            properties.update(damage=10.0 + index, resource_costs=[{"type": "Stamina", "cost": 3.0 + index}],
                              attack_cooldown=0.7, charge_seconds=0.72, hit_start_delay_seconds=0.4,
                              attack_montage=FakeObject("/Game/ExistingMotion/AM_" + str(index)), animation_play_rate=0.62,
                              hit_mode="Single", hit_count=1, hit_interval=0.08, input_slot="Primary",
                              allowed_slot_policy="Legacy", enable_combo=True,
                              next_combo_skill=FakeObject("/Game/ExistingBasic/DA_Next"),
                              advance_before_hit=False, advance_distance=0.0, advance_duration=0.25,
                              cast_effect=FakeObject("/Game/ExistingFX/NS_Cast"), charge_effect=FakeObject("/Game/ExistingFX/NS_Charge"),
                              impact_effect=FakeObject("/Game/ExistingFX/NS_Impact"))
            self.library.assets[path] = FakeSkill(path, properties)
        profile_fields = STAGE.reflected_fields(self.project, "Rogue10mSkillLoadoutDataAsset.h", "class ROGUE10M_API URogue10mWeaponSkillProfileDataAsset")
        for weapon, path in RECIPES.PROFILE_SOURCES.items():
            source_assets = [self.library.assets[row.source_package] for row in RECIPES.recipes() if row.weapon == weapon]
            primary_path = (RECIPES.KNUCKLE if weapon == "Knuckle" else RECIPES.SWORD) + "Primary01"
            primary = self.library.assets[primary_path]
            properties = {name: None for name in profile_fields}
            properties.update(profile_id="Existing." + weapon, weapon_type=weapon,
                              default_skill_bindings={"Primary": primary}, initially_unlocked_skills=[primary],
                              skill_tree_skills=list(dict.fromkeys(source_assets)), active_technique_catalog=[],
                              max_jump_count=1)
            self.library.assets[path] = FakeProfile(path, properties)
        self.originals = {path: STAGE._fields(asset, self.skill_fields, self.unreal)
                          for path, asset in self.library.assets.items() if isinstance(asset, FakeSkill)}

    def tearDown(self):
        self.assertTrue(self.project.resolve().is_relative_to(TEST_ROOT))
        self.temp.cleanup()

    def run_stage(self, label="TestA", **options):
        return STAGE.stage_prototypes(label=label, unreal_module=self.unreal, **options)

    def report(self, label="TestA"):
        path = self.project / "Feature/doc/evidence/melee-skill-prototypes-20261111" / label / "report.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def assert_originals_unchanged(self):
        for path, before in self.originals.items():
            self.assertEqual(STAGE._fields(self.library.assets[path], self.skill_fields, self.unreal), before)

    def test_dry_run_never_duplicates_saves_or_changes_sources(self):
        report = self.run_stage()
        self.assertEqual(report["status"], "dry_run_only_no_assets_created")
        self.assertEqual(self.library.duplicates, [])
        self.assertEqual(self.library.saves, [])
        self.assertFalse(report["production_opt_in"])
        self.assert_originals_unchanged()

    def test_apply_creates_twenty_independent_assets_preserving_combat_fields(self):
        report = self.run_stage(apply=True)
        self.assertEqual(report["generated_skill_count"], 20)
        self.assertEqual(len(self.library.duplicates), 20)
        self.assertEqual(len(self.library.saves), 20)
        for row in RECIPES.recipes():
            clone = self.library.assets[row.destination_package]
            self.assertIsNot(clone, self.library.assets[row.source_package])
            self.assertEqual(clone.properties["damage"], self.library.assets[row.source_package].properties["damage"])
            self.assertFalse(clone.properties["enable_combo"])
            self.assertIsNone(clone.properties["next_combo_skill"])
            self.assertEqual(clone.metadata[STAGE.READY_TAG], "false")
        self.assert_originals_unchanged()

    def test_only_two_sword_prototypes_enable_advance(self):
        self.run_stage(apply=True)
        advanced = []
        for row in RECIPES.recipes():
            asset = self.library.assets[row.destination_package]
            if asset.properties["advance_before_hit"]:
                advanced.append(row.document_id)
        self.assertEqual(advanced, ["G04", "G10"])

    def test_matching_owned_prototypes_are_reused_without_resaving(self):
        self.run_stage(apply=True)
        before = (len(self.library.duplicates), len(self.library.saves))
        report = self.run_stage(label="RepeatA", apply=True)
        self.assertEqual((len(self.library.duplicates), len(self.library.saves)), before)
        self.assertEqual(len(report["reused_existing_prototypes"]), 20)

    def test_missing_source_rejects_before_any_asset_mutation(self):
        del self.library.assets[RECIPES.recipes()[-1].source_package]
        with self.assertRaisesRegex(RuntimeError, "Required existing asset"):
            self.run_stage(apply=True)
        self.assertEqual(self.library.duplicates, [])

    def test_stale_advance_reflection_rejects_before_any_asset_mutation(self):
        source = self.library.assets[RECIPES.recipes()[0].source_package]
        del source.properties["advance_before_hit"]
        with self.assertRaisesRegex(RuntimeError, "Stale reflection"):
            self.run_stage(apply=True)
        self.assertEqual(self.library.duplicates, [])

    def test_advance_multi_hit_source_rejected_without_retuning(self):
        row = next(row for row in RECIPES.recipes() if row.document_id == "G04")
        self.library.assets[row.source_package].properties["hit_count"] = 2
        with self.assertRaisesRegex(RuntimeError, "existing single-hit source"):
            self.run_stage(apply=True)
        self.assertEqual(self.library.duplicates, [])

    def test_unrelated_destination_rejected_before_any_duplication(self):
        row = RECIPES.recipes()[0]
        self.library.assets[row.destination_package] = FakeSkill(row.destination_package, {})
        with self.assertRaisesRegex(RuntimeError, "not an unchanged owned prototype"):
            self.run_stage(apply=True)
        self.assertEqual(self.library.duplicates, [])

    def test_manually_edited_prototype_is_not_overwritten(self):
        self.run_stage(apply=True)
        row = RECIPES.recipes()[0]
        clone = self.library.assets[row.destination_package]
        clone.properties["damage"] = 999.0
        with self.assertRaisesRegex(RuntimeError, "protected=.*damage"):
            self.run_stage(label="EditedA", apply=True)
        self.assertEqual(clone.properties["damage"], 999.0)
        self.assertEqual(len(self.library.duplicates), 20)

    def test_optional_profiles_keep_defaults_unlocks_and_production_profiles_unchanged(self):
        before = {weapon: copy.deepcopy(profile.properties) for weapon, path in RECIPES.PROFILE_SOURCES.items()
                  for profile in [self.library.assets[path]]}
        self.run_stage(apply=True, create_staged_profiles=True)
        self.assertEqual(len(self.library.saves), 22)
        for weapon, path in RECIPES.PROFILE_SOURCES.items():
            self.assertEqual(self.library.assets[path].properties, before[weapon])
            staged = self.library.assets[f"{RECIPES.DESTINATION_ROOT}/Profiles/DA_PrototypeProfile_{weapon}"]
            self.assertEqual(len(staged.properties["active_technique_catalog"]), 10)
            self.assertEqual(staged.properties["default_skill_bindings"], before[weapon]["default_skill_bindings"])
            self.assertEqual(staged.properties["initially_unlocked_skills"], before[weapon]["initially_unlocked_skills"])
        self.assert_originals_unchanged()

    def test_protected_clone_mutation_is_detected_before_explicit_save(self):
        self.library.damage_mutation_on_duplicate = True
        with self.assertRaisesRegex(RuntimeError, "protected=.*damage"):
            self.run_stage(apply=True)
        self.assertEqual(self.library.saves, [])
        self.assertEqual(self.report()["status"], "failed_candidates_preserved_no_production_opt_in")
        self.assertEqual(len(self.report()["created_assets"]), 1)
        self.assert_originals_unchanged()

    def test_save_failure_retains_partial_evidence_and_never_marks_ready(self):
        self.library.fail_save = True
        with self.assertRaisesRegex(RuntimeError, "Prototype save failed"):
            self.run_stage(apply=True)
        report = self.report()
        self.assertFalse(report["production_opt_in"])
        self.assertEqual(len(report["created_assets"]), 20)
        self.assertEqual(report["explicitly_saved_assets"], [])
        self.assert_originals_unchanged()


class RecipeTests(unittest.TestCase):
    def test_canonical_twenty_unique_ids_and_explicit_policies(self):
        rows = RECIPES.recipes()
        RECIPES.validate_recipes(rows)
        for weapon in ["Knuckle", "GreatSword"]:
            group = [row for row in rows if row.weapon == weapon]
            self.assertEqual(len(group), 10)
            self.assertEqual([row.slot_policy for row in group].count("COMMON"), 6)
            self.assertEqual([row.slot_policy for row in group].count("MOUSE_CHORD_ONLY"), 2)
            self.assertEqual([row.slot_policy for row in group].count("RIGHT_CHARGE_ONLY"), 2)
        self.assertNotIn("Staff", {row.weapon for row in rows})

    def test_duplicate_or_missing_document_id_is_rejected(self):
        rows = list(RECIPES.recipes())
        rows[-1] = replace(rows[-1], document_id="G09")
        with self.assertRaises(ValueError):
            RECIPES.validate_recipes(tuple(rows))

    def test_invalid_advance_cannot_sneak_into_a_recipe(self):
        for distance, duration in [(float("nan"), 0.2), (0, 0.2), (601, 0.2), (150, 0.01), (150, float("inf"))]:
            rows = list(RECIPES.recipes())
            index = next(index for index, row in enumerate(rows) if row.document_id == "G04")
            rows[index] = replace(rows[index], advance=RECIPES.AdvancePrototype(distance, duration))
            with self.assertRaises(ValueError):
                RECIPES.validate_recipes(tuple(rows))

    def test_fallbacks_do_not_claim_unimplemented_mechanics(self):
        rows = {row.document_id: row for row in RECIPES.recipes()}
        for document_id in ["G05", "G06", "G08"]:
            self.assertTrue(any("미구현" in text for text in rows[document_id].pending))
        self.assertTrue(any("관통 피해 미구현" in text for text in rows["G10"].pending))
        self.assertEqual(rows["K10"].source_package, RECIPES.KNUCKLE + "Charged")


if __name__ == "__main__":
    unittest.main(verbosity=2)
