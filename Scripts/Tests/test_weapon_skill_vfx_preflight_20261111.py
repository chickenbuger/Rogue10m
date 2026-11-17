"""Read-only synthetic fixtures; no Unreal, network, build or production assets."""
from pathlib import Path
import copy
import hashlib
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import WeaponSkillVfxPreflight20261111 as preflight


class WeaponVfxPreflightTests(unittest.TestCase):
    def setUp(self):
        # Keep Unreal-style paths short and avoid writing beside another user's
        # project when this test is integrated into the shared checkout.
        temp_parent = Path(os.environ.get("ROGUE_VFX_TEST_TEMP_ROOT", str(Path(__file__).resolve().parents[2])))
        if len(str(temp_parent.resolve())) + 180 > 248:
            raise RuntimeError("Set ROGUE_VFX_TEST_TEMP_ROOT to an existing short writable test directory")
        self.temp = tempfile.TemporaryDirectory(prefix="vfxp_", dir=temp_parent)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "project"
        self.root.mkdir()
        sources = {preflight._source(w, p) for w in preflight.WEAPONS for p in preflight.PHASES}
        source_pins = {p: self.write_package(p, b"source") for p in sources}
        rows, self.pins = [], {}
        snapshots = {}
        for w in preflight.WEAPONS:
            for v in preflight.VARIANTS:
                skill = preflight.skill_path(w, v)
                skill_hash = self.write_package(skill, skill.encode())
                props = dict.fromkeys(preflight.REQUIRED)
                props.update(enable_attack_effects=True, use_first_person_effect_overrides=True,
                    cast_effect=None, impact_effect=None, charge_effect=None, attack_montage="/Game/Test/Montage.Montage",
                    cast_effect_scale=.3, impact_effect_scale=.4, charge_effect_scale=.2,
                    cast_effect_emission_duration=.35, impact_effect_emission_duration=.35,
                    attack_effect_time_dilation=1., first_person_emission_duration_multiplier=1.,
                    first_person_cast_scale_multiplier=.5, first_person_impact_scale_multiplier=.5,
                    first_person_charge_scale_multiplier=.5, damage=10., hit_count=1,
                    hit_start_delay_seconds=0., hit_interval=.1, animation_play_rate=1., resource_costs=[])
                snapshots[skill] = {"properties": props, "reflected_fields": sorted(props), "asset_sha256": skill_hash}
                for p in preflight.PHASES:
                    asset = preflight.candidate_path(w, v, p)
                    self.pins[asset] = self.write_package(asset, asset.encode())
                    rows.append({"name": asset.rsplit("/", 1)[1], "asset": asset, "weapon": w, "skill": v,
                        "phase": p, "existing_skill_asset": skill, "source": preflight._source(w, p),
                        "source_sha256": source_pins[preflight._source(w, p)], "saved": True,
                        "activated": False, "presentation_only": True, "local_space": p != "Impact",
                        "lifetime_sim_seconds": .2, "compile": {"aggregateStatus": "UpToDate",
                        "bIsCompiling": False, "bIsStale": False, "bHasErrors": False}, "issues": {"numErrors": 0}})
        self.manifest = {"status": "30_candidates_saved_compile_checked_rendered_preview_pending",
            "activated": False, "gameplay_modified": False, "continuous_ribbon_implemented": False,
            "candidates": rows, "created_paths": [r["asset"] for r in rows], "sources_preserved": True,
            "source_hashes_before": source_pins, "source_hashes_after": copy.deepcopy(source_pins)}
        self.snapshots = {"schema": preflight.SNAPSHOT_SCHEMA, "skills": snapshots}

    def write_package(self, package, contents):
        target = preflight.package_file(self.root, package)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(contents)
        return hashlib.sha256(contents).hexdigest()

    def audit(self):
        return preflight.audit(self.manifest, self.pins, self.root, set(preflight.REQUIRED), self.snapshots)

    def test_exact_eight_inspection_intents_protect_active_two_and_all_trajectories(self):
        before = copy.deepcopy((self.manifest, self.snapshots))
        result = self.audit()
        self.assertEqual(result["candidate_hash_matches"], 30)
        self.assertEqual(sum("inspection_binding_intent" in r for r in result["skills"]), 8)
        for row in result["skills"]:
            self.assertEqual(row["proposed_property_updates"], {})
            self.assertFalse(row["activation_ready"])
            self.assertFalse(row["trajectory"]["binding_allowed"])
            if row["role"] == "protected_existing_active_pilot":
                self.assertNotIn("inspection_binding_intent", row)
                self.assertFalse(row["eligible_for_scoped_live_inspection"])
        self.assertEqual(before, (self.manifest, self.snapshots))
        self.assertFalse(result["new_render_performed"])

    def test_editor_tuned_lifetime_and_space_use_pinned_data_without_arbitrary_caps(self):
        first = preflight.skill_path("GreatSword", "Primary01")
        self.snapshots["skills"][first]["properties"].update(
            cast_effect_emission_duration=1., impact_effect_emission_duration=1.)
        for row in self.manifest["candidates"]:
            if row["weapon"] == "GreatSword" and row["skill"] == "Primary01":
                row["lifetime_sim_seconds"] = .75
                row["local_space"] = row["phase"] == "Impact"
        row = next(r for r in self.audit()["skills"] if r["skill"] == first)
        self.assertEqual(row["timing"]["cast"]["candidate_particle_sim_seconds"], .75)
        self.assertFalse(row["timing"]["cast"]["candidate_local_space"])
        self.assertTrue(row["timing"]["impact"]["candidate_local_space"])
        self.assertFalse(row["timing"]["cast"]["third_person_particle_truncated_by_destroy_timer"])
        bad = copy.deepcopy(self.manifest)
        bad["candidates"][0]["local_space"] = 1
        with self.assertRaises(ValueError):
            preflight.validate_manifest(bad)

    def test_missing_snapshot_schema_and_export_hash_never_pass(self):
        first = preflight.skill_path("GreatSword", "Primary01")
        props = self.snapshots["skills"][first]
        props["properties"].pop("damage")
        props["reflected_fields"].remove("damage")
        props["asset_sha256"] = None
        row = next(r for r in self.audit()["skills"] if r["skill"] == first)
        self.assertFalse(row["eligible_for_scoped_live_inspection"])
        self.assertIn("damage", row["snapshot_schema"]["missing_source_declared_fields"])
        self.assertIn("snapshot_asset_hash_missing_or_stale", row["blockers"])
        empty = preflight.audit(self.manifest, self.pins, self.root, set(preflight.REQUIRED))
        self.assertTrue(all("missing_source_snapshot" in r["blockers"] for r in empty["skills"]))

    def test_particle_world_time_and_fp_truncation_are_separate(self):
        first = preflight.skill_path("GreatSword", "Primary01")
        props = self.snapshots["skills"][first]["properties"]
        props.update(attack_effect_time_dilation=2., cast_effect_emission_duration=.12,
                     first_person_emission_duration_multiplier=.5)
        row = next(r for r in self.audit()["skills"] if r["skill"] == first)
        self.assertEqual(row["timing"]["cast"]["particle_world_seconds_estimate"], .1)
        self.assertFalse(row["timing"]["cast"]["third_person_particle_truncated_by_destroy_timer"])
        self.assertTrue(row["timing"]["cast"]["first_person_particle_truncated_by_destroy_timer"])
        self.assertIn("cast_first_person_timer_truncates_candidate", row["blockers"])

    def test_shared_enable_and_charge_risk_never_produces_updates(self):
        first = preflight.skill_path("GreatSword", "Primary01")
        self.snapshots["skills"][first]["properties"].update(enable_attack_effects=False,
                                                          charge_effect="/Game/Test/Charge.Charge")
        row = next(r for r in self.audit()["skills"] if r["skill"] == first)
        self.assertIn("shared_enable_may_affect_charge_feedback", row["blockers"])
        self.assertEqual(row["proposed_property_updates"], {})

    def test_minimum_destroy_timer_prevents_false_truncation_below_ten_milliseconds(self):
        first = preflight.skill_path("GreatSword", "Primary01")
        props = self.snapshots["skills"][first]["properties"]
        props.update(cast_effect_emission_duration=.005, first_person_emission_duration_multiplier=.5)
        next(r for r in self.manifest["candidates"] if r["asset"] ==
             preflight.candidate_path("GreatSword", "Primary01", "Cast"))["lifetime_sim_seconds"] = .0075
        for override, expected_fp_raw in ((True, .0025), (False, .005)):
            with self.subTest(use_first_person_overrides=override):
                props["use_first_person_effect_overrides"] = override
                row = next(r for r in self.audit()["skills"] if r["skill"] == first)
                timing = row["timing"]["cast"]
                self.assertEqual(timing["destroy_timer_raw_world_seconds"], .005)
                self.assertEqual(timing["first_person_destroy_timer_raw_world_seconds"], expected_fp_raw)
                self.assertEqual(timing["destroy_timer_world_seconds"], .01)
                self.assertEqual(timing["first_person_destroy_timer_world_seconds"], .01)
                self.assertFalse(timing["third_person_particle_truncated_by_destroy_timer"])
                self.assertFalse(timing["first_person_particle_truncated_by_destroy_timer"])

    def test_native_nullable_charge_envelopes_retain_explicit_enable_review(self):
        first = preflight.skill_path("GreatSword", "Primary01")
        props = self.snapshots["skills"][first]["properties"]
        props["enable_attack_effects"] = False
        for value in (None, "", "None", " null ", {"refPath": None},
                      {"refPath": ""}, {"refPath": "None"}, {"refPath": " null "}):
            with self.subTest(reference=value):
                props["charge_effect"] = copy.deepcopy(value)
                before = copy.deepcopy(self.snapshots)
                row = next(r for r in self.audit()["skills"] if r["skill"] == first)
                self.assertIn("effects_enable_requires_explicit_review", row["blockers"])
                self.assertNotIn("shared_enable_may_affect_charge_feedback", row["blockers"])
                self.assertFalse(row["eligible_for_scoped_live_inspection"])
                self.assertEqual(self.snapshots, before)
        props["charge_effect"] = {"refPath": "/Game/Test/Charge.Charge"}
        row = next(r for r in self.audit()["skills"] if r["skill"] == first)
        self.assertIn("shared_enable_may_affect_charge_feedback", row["blockers"])

    def test_reference_envelopes_reject_extra_fields_nested_values_and_foreign_paths(self):
        for value in ({}, {"refPath": None, "dirty": False}, {"path": None},
                      {"refPath": {"refPath": None}}, {"refPath": True},
                      {"refPath": "/Engine/Test/Charge.Charge"},
                      {"refPath": "/Game/Test/Charge.Other"}):
            with self.subTest(reference=value):
                with self.assertRaises(ValueError):
                    preflight._ref(value)

    def test_duplicate_foreign_manifest_paths_and_source_drift_are_rejected(self):
        for field, value in (("asset", "/Game/Other/NS_Bad"), ("source", "/Game/Other/Source"),
                             ("existing_skill_asset", preflight.skill_path("Knuckle", "Charged"))):
            bad = copy.deepcopy(self.manifest)
            bad["candidates"][0][field] = value
            with self.assertRaises(ValueError):
                preflight.validate_manifest(bad)
        self.manifest["candidates"][-1] = copy.deepcopy(self.manifest["candidates"][0])
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            self.audit()

    def test_hash_mismatch_or_missing_candidate_blocks_affected_skill(self):
        cast = preflight.candidate_path("GreatSword", "Primary01", "Cast")
        preflight.package_file(self.root, cast).write_bytes(b"changed")
        impact = preflight.candidate_path("Knuckle", "Special", "Impact")
        preflight.package_file(self.root, impact).unlink()
        result = self.audit()
        self.assertEqual(result["candidate_hash_matches"], 28)
        self.assertEqual(result["candidate_disk_hashes"][cast]["status"], "mismatch")
        self.assertEqual(result["candidate_disk_hashes"][impact]["status"], "missing")
        for w, v in (("GreatSword", "Primary01"), ("Knuckle", "Special")):
            row = next(r for r in result["skills"] if r["skill"] == preflight.skill_path(w, v))
            self.assertFalse(row["eligible_for_scoped_live_inspection"])

    def test_nonfinite_bool_numeric_and_traversal_are_rejected(self):
        for number in (float("nan"), float("inf"), True):
            bad = copy.deepcopy(self.manifest)
            bad["candidates"][0]["lifetime_sim_seconds"] = number
            with self.assertRaises(ValueError):
                preflight.validate_manifest(bad)
        for bad_path in ("/Game/../Escape", "/Game/Good/Asset.SubObject", "C:/Outside/Asset"):
            with self.assertRaises(ValueError):
                preflight.package_file(self.root, bad_path)
        first = preflight.skill_path("GreatSword", "Primary01")
        self.snapshots["skills"][first]["properties"]["attack_effect_time_dilation"] = True
        with self.assertRaisesRegex(ValueError, "finite positive"):
            self.audit()

    def test_capture_lineage_uses_raw_manifest_hash_and_scopes_only_candidate_pins(self):
        raw_hash = "a" * 64
        hashes = {str(preflight.package_file(self.root, a)): h for a, h in self.pins.items()}
        hashes[str(self.root / "Content/ProtectedOldAsset.uasset")] = "b" * 64
        captures = {"manifest_sha256": raw_hash, "protected_sha256_before": hashes,
                    "protected_sha256_after": copy.deepcopy(hashes), "protected_assets_unchanged": True,
                    "manifest_candidate_count": 30, "selected_assets": list(self.pins)}
        pins = preflight.candidate_pins(self.manifest, captures, self.root, raw_hash)
        self.assertEqual(pins, self.pins)
        with self.assertRaises(ValueError):
            preflight.candidate_pins(self.manifest, captures, self.root, "c" * 64)
        captures["protected_sha256_before"][str(self.root.parent / "escape.uasset")] = "b" * 64
        captures["protected_sha256_after"] = copy.deepcopy(captures["protected_sha256_before"])
        with self.assertRaisesRegex(ValueError, "escapes"):
            preflight.candidate_pins(self.manifest, captures, self.root, raw_hash)

    def test_source_schema_parser_preserves_boolean_snake_names_and_future_fields(self):
        lines = ["class ROGUE10M_API URogue10mAttackSkillData {"]
        def native(s):
            return "".join(x.capitalize() for x in s.split("_"))
        for field in preflight.REQUIRED:
            name = native(field)
            if field in ("enable_attack_effects", "use_first_person_effect_overrides"):
                name = "b" + name
            lines += ["UPROPERTY(EditAnywhere)", "float " + name + " = 0;"]
        lines += ["UPROPERTY(EditAnywhere)", "float FutureGameplayContract = 1;", "};"]
        fields = preflight.schema_fields("\n".join(lines))
        self.assertEqual(fields, set(preflight.REQUIRED) | {"future_gameplay_contract"})


if __name__ == "__main__":
    unittest.main(verbosity=2)
