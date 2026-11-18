"""Offline contract tests; no Unreal, network, editor or build invocation."""
import copy
import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import MagicSkillVfxBinding20261111 as binding


def snapshot():
    return {
        "enable_attack_effects": True,
        "cast_effect": "/Game/Existing/NS_OldCast",
        "impact_effect": "/Game/Existing/NS_OldImpact.NS_OldImpact",
        "charge_effect": None,
        "cast_effect_scale": .22,
        "impact_effect_scale": .30,
        "charge_effect_scale": .24,
        "attack_effect_time_dilation": 3.0,
        "first_person_emission_duration_multiplier": .50,
        "cast_effect_emission_duration": .105,
        "impact_effect_emission_duration": .120,
        "first_person_cast_scale_multiplier": .24,
        "first_person_impact_scale_multiplier": .52,
        "first_person_charge_scale_multiplier": .10,
        "attack_montage": "/Game/Rogue10m/Animation/Combat/Staff/AM_Staff_Primary01",
        "animation_play_rate": 1.08,
        "hit_start_delay_seconds": .25,
        "resource_costs": [{"resource_type": "Mana", "cost": 5.0}],
        "effect_attach_socket": "hand_r",
        "off_hand_effect_attach_socket": "hand_l",
        "spawn_effect_on_off_hand": False,
        "attack_cooldown": .55,
        "attack_range": 1150.0,
        "damage": 17.0,
        "future_unknown_field": {"nested": [1, True, {"keep": "unchanged"}]},
    }


class BindingContractTests(unittest.TestCase):
    def setUp(self):
        self.plan = binding.build_binding_plan(Path("D:/Project/Rogue10m"))
        self.updates = binding.planned_visual_updates(binding.PILOT_ASSET)

    def test_manifest_exact_scope_and_orchestrator_shape(self):
        self.assertTrue(binding.validate_plan(self.plan))
        self.assertEqual(self.plan["status"], "candidate_unconnected")
        self.assertEqual(self.plan["candidate_root"], binding.CANDIDATE_ROOT)
        self.assertEqual(self.plan["profile_asset"], binding.PROFILE_ASSET)
        self.assertEqual(self.plan["character_asset"], binding.CHARACTER_ASSET)
        self.assertEqual(len(self.plan["targets"]), 5)
        self.assertEqual([r["role"] for r in self.plan["targets"]], ["pilot"] + ["protected"] * 4)
        self.assertEqual(len(self.plan["candidate_assets"]), 4)
        self.assertEqual(len(self.updates), 8)
        self.assertEqual(self.plan["targets"][0]["updates"], self.updates)
        self.assertTrue(all(row["updates"] == {} for row in self.plan["targets"][1:]))

    def test_root_parameter_is_filesystem_metadata_not_asset_root(self):
        self.assertIsNone(binding.build_binding_plan()["project_root"])
        for root in (binding.CANDIDATE_ROOT, "relative/project", "D:/Project/../Other", 3):
            with self.subTest(root=root), self.assertRaises(ValueError):
                binding.build_binding_plan(root)

    def test_extra_duplicate_reordered_targets_and_foreign_paths_rejected(self):
        for mutation in (
            lambda p: p["targets"].append(copy.deepcopy(p["targets"][0])),
            lambda p: p["targets"].__setitem__(1, copy.deepcopy(p["targets"][0])),
            lambda p: p["targets"].reverse(),
            lambda p: p["targets"][0].__setitem__("asset", binding.SKILL_ROOT + "/DA_Attack_Staff_Charged"),
            lambda p: p["candidate_assets"].append("/Game/Other/NS_Extra"),
            lambda p: p.__setitem__("candidate_root", "/Game/Other"),
            lambda p: p["targets"][1]["updates"].__setitem__("damage", 5),
        ):
            bad = copy.deepcopy(self.plan)
            mutation(bad)
            with self.assertRaises(ValueError):
                binding.validate_plan(bad)

    def test_nonpilot_planned_updates_rejected_and_all_charge_fields_absent(self):
        for row in self.plan["targets"][1:]:
            with self.subTest(row=row["asset"]):
                with self.assertRaises(ValueError):
                    binding.planned_visual_updates(row)
                with self.assertRaises(ValueError):
                    binding.planned_visual_updates(row["asset"])
        self.assertFalse(any("charge" in key for key in self.updates))
        self.assertEqual(binding.planned_visual_updates(self.plan["targets"][0]), self.updates)

    def test_ref_normalization_and_null_forms(self):
        package = binding.CANDIDATES["telegraph"]
        object_path = package + "." + package.rsplit("/", 1)[-1]
        self.assertEqual(binding.normalize_object_ref(package), object_path)
        self.assertEqual(binding.normalize_object_ref({"refPath": object_path}), object_path)
        for value in (None, "", "None", "null", {"refPath": None}, {"refPath": "None"}):
            self.assertIsNone(binding.normalize_object_ref(value))
        for value in (42, True, {"path": package}, package + ".Other", package + ":Emitter", "/Game/../Escape", "NiagaraSystem'" + package + "'"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                binding.normalize_object_ref(value)

    def test_plan_and_returned_updates_are_detached(self):
        first = binding.build_binding_plan()
        first["targets"][0]["updates"]["cast_effect_scale"] = 9
        first["event_contracts"]["release_event_added"] = True
        self.updates["impact_effect_scale"] = 8
        second = binding.build_binding_plan()
        self.assertEqual(second["targets"][0]["updates"]["cast_effect_scale"], 1)
        self.assertEqual(binding.planned_visual_updates(binding.PILOT_ASSET)["impact_effect_scale"], 1)
        self.assertFalse(second["event_contracts"]["release_event_added"])

    def test_visual_update_audit_and_idempotent_noop(self):
        before = snapshot()
        after = copy.deepcopy(before)
        after.update(self.updates)
        report = binding.audit_transition(before, after, self.updates)
        self.assertEqual(set(report["changed_fields"]), set(self.updates) - {"enable_attack_effects"})
        self.assertFalse(report["idempotent_noop"])
        self.assertTrue(binding.audit_transition(after, copy.deepcopy(after), self.updates)["idempotent_noop"])
        self.assertEqual(before, snapshot())

    def test_protected_nonpilot_all_fields_unchanged_and_null_refs_equivalent(self):
        for suffix in binding.SUFFIXES[1:]:
            before = snapshot()
            before["charge_effect"] = "/Game/Existing/NS_Held"
            after = copy.deepcopy(before)
            after["charge_effect"] = {"refPath": "/Game/Existing/NS_Held.NS_Held"}
            self.assertTrue(binding.audit_transition(before, after, {})["idempotent_noop"], suffix)
            after["future_unknown_field"]["nested"][2]["keep"] = "clobbered"
            with self.assertRaises(ValueError):
                binding.audit_transition(before, after, {})

    def test_nonwhitelist_clobbers_and_missing_properties_rejected(self):
        for field, value in (("charge_effect", binding.CANDIDATES["telegraph"]), ("charge_effect_scale", 1.0),
                             ("first_person_cast_scale_multiplier", 1.0), ("effect_attach_socket", "hand_l"),
                             ("attack_montage", "/Game/Other/AM_New"), ("hit_start_delay_seconds", 0.0),
                             ("resource_costs", []), ("damage", 99.0), ("attack_cooldown", .01),
                             ("spawn_effect_on_off_hand", 0)):
            before = snapshot()
            after = copy.deepcopy(before)
            after.update(self.updates)
            after[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                binding.audit_transition(before, after, self.updates)
        for alter in (lambda a: a.pop("charge_effect"), lambda a: a.__setitem__("extra", 1)):
            before = snapshot()
            after = copy.deepcopy(before)
            alter(after)
            with self.assertRaises(ValueError):
                binding.audit_transition(before, after, {})

    def test_approved_dtype_finite_positive_and_expected_value_checks(self):
        for field in binding.FLOAT_UPDATES:
            for value in (True, "1", float("nan"), float("inf"), -.1, 0, 100):
                row = copy.deepcopy(self.plan["targets"][0])
                row["updates"][field] = value
                with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                    binding.planned_visual_updates(row)
        for value in (1, "true", False):
            row = copy.deepcopy(self.plan["targets"][0])
            row["updates"]["enable_attack_effects"] = value
            with self.assertRaises(ValueError):
                binding.planned_visual_updates(row)
        for field in ("cast_effect", "impact_effect"):
            row = copy.deepcopy(self.plan["targets"][0])
            row["updates"][field] = binding.CANDIDATES["release"]
            with self.assertRaises(ValueError):
                binding.planned_visual_updates(row)

    def test_snapshot_nonfinite_or_invalid_dtype_rejected(self):
        for field, value in (("cast_effect_scale", True), ("charge_effect_scale", -.2),
                             ("first_person_emission_duration_multiplier", float("nan")),
                             ("damage", float("inf")), ("enable_attack_effects", 1)):
            before = snapshot()
            before[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                binding.audit_transition(before, copy.deepcopy(before), {})

    def test_actual_float_storage_roundoff_and_reference_serialization(self):
        before = snapshot()
        after = copy.deepcopy(before)
        after.update(self.updates)
        after["cast_effect"] = binding.CANDIDATES["telegraph"]
        after["impact_effect"] = {"refPath": binding.CANDIDATES["impact"]}
        after["cast_effect_emission_duration"] = .3499999940395355
        after["impact_effect_emission_duration"] = .27000001072883606
        self.assertEqual(binding.audit_transition(before, after, self.updates)["status"], "pilot_visual_snapshot_verified")

    def test_world_destroy_budgets_and_preserved_first_person_lifetime(self):
        lifetime = self.plan["lifetime_contract"]
        for role in ("cast", "impact"):
            effective = self.updates[role + "_effect_emission_duration"]
            self.assertTrue(math.isclose(effective, lifetime[role]["system_seconds"]))
            self.assertGreaterEqual(effective + 1.e-9, lifetime[role]["particle_seconds"] + .05)
        after = snapshot()
        old_scale = after["first_person_cast_scale_multiplier"]
        after.update(self.updates)
        self.assertEqual(after["cast_effect_scale"] * after["first_person_cast_scale_multiplier"], old_scale)
        self.assertNotIn("first_person_emission_duration_multiplier", self.updates)
        self.assertEqual(after["first_person_emission_duration_multiplier"], .5)
        binding.audit_transition(snapshot(), after, self.updates)
        self.assertLess(after["cast_effect_emission_duration"] * .5, lifetime["cast"]["particle_seconds"])
        self.assertIn("paused", lifetime["first_person_duration_multiplier"])
        drifted = copy.deepcopy(after)
        drifted["first_person_emission_duration_multiplier"] = 1.
        with self.assertRaisesRegex(ValueError, "Unplanned property changed"):
            binding.audit_transition(snapshot(), drifted, self.updates)

    def test_first_person_multiplier_is_required_protected_and_not_an_update(self):
        forbidden=copy.deepcopy(self.updates)
        forbidden["first_person_emission_duration_multiplier"]=1.
        with self.assertRaisesRegex(ValueError,"eight-field"):
            binding.audit_transition(snapshot(), snapshot(), forbidden)
        missing=snapshot()
        del missing["first_person_emission_duration_multiplier"]
        with self.assertRaisesRegex(ValueError,"Complete snapshots"):
            binding.audit_transition(missing, missing, {})

    def test_no_fake_release_hold_or_gameplay_contract(self):
        self.assertEqual({row["asset"] for row in self.plan["deferred"]}, {binding.CANDIDATES["circle"], binding.CANDIDATES["release"]})
        self.assertTrue(all(row["status"] == "unbound_requires_real_release_event" for row in self.plan["deferred"]))
        self.assertIn("cost-accepted", self.plan["event_contracts"]["cast"])
        self.assertIn("not connected to chargeeffect", self.plan["lifetime_contract"]["charge_candidate"].lower())
        self.assertIn("world-space", self.plan["worldspace_warning"])
        self.assertFalse(self.plan["event_contracts"]["release_event_added"])
        self.assertFalse(self.plan["event_contracts"]["gameplay_or_input_changes"])

    def test_existing_charge_visual_blocks_shared_flag_or_dilation_change(self):
        before = snapshot()
        before["charge_effect"] = "/Game/Existing/NS_Charge"
        after = copy.deepcopy(before)
        after.update(self.updates)
        with self.assertRaisesRegex(ValueError, "charge visual"):
            binding.audit_transition(before, after, self.updates)


if __name__ == "__main__":
    unittest.main(verbosity=2)
