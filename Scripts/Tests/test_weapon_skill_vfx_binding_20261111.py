"""Detached binding-contract regression fixtures; no engine/files/assets/network."""
from pathlib import Path
import copy
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import WeaponSkillVfxBinding20261111 as binding
import WeaponSkillVfxPreflight20261111 as preflight


def valid_fixture(weapon="GreatSword", variant="Primary01", enabled=True):
    """Return (plan, before_snake, synthetic_preflight_report), not live proof."""
    plan = binding.build_plan(weapon, variant, "FixtureA")
    before = dict.fromkeys(binding.REQUIRED)
    before.update(
        enable_attack_effects=enabled, cast_effect=None, impact_effect=None,
        charge_effect=None, attack_effect=None,
        attack_montage="/Game/Test/Montage.Montage", animation_play_rate=1.,
        damage=10., hit_count=1, hit_start_delay_seconds=.2, hit_interval=.1,
        cast_effect_scale=.3, impact_effect_scale=.4, charge_effect_scale=.2,
        cast_effect_emission_duration=.4, impact_effect_emission_duration=.4,
        attack_effect_time_dilation=2., use_first_person_effect_overrides=True,
        first_person_emission_duration_multiplier=.5,
        first_person_cast_scale_multiplier=.5, first_person_impact_scale_multiplier=.5,
        first_person_charge_scale_multiplier=.5,
        first_person_effect_offset={"x": 0., "y": 10., "z": -8.},
        first_person_effect_rotation={"pitch": 0., "yaw": 0., "roll": 0.},
        effect_attach_socket="hand_r", resource_costs=[{"resource": "stamina", "amount": 5.}],
        input_slot="Primary", attack_shape="Sphere", hit_mode="Single",
        allowed_slot_policy="TechniqueOnly", skill_tree_node_type="ActiveTechnique",
        advance_before_hit=True, advance_distance=80., advance_duration=.12,
        future_nested={"flags": [True, 1, 1., {"key": "preserved"}]})
    timing = {}
    for phase in ("cast", "impact"):
        raw = before[phase + "_effect_emission_duration"]
        fp = raw * before["first_person_emission_duration_multiplier"]
        timing[phase] = {
            "basis": "provided_snapshot_only_game_world_seconds",
            "candidate_particle_sim_seconds": .1, "component_dilation": 2.,
            "particle_world_seconds_estimate": .05,
            "destroy_timer_raw_world_seconds": raw,
            "first_person_destroy_timer_raw_world_seconds": fp,
            "destroy_timer_minimum_world_seconds": .01,
            "destroy_timer_world_seconds": max(.01, raw),
            "first_person_destroy_timer_world_seconds": max(.01, fp),
            "third_person_particle_truncated_by_destroy_timer": False,
            "first_person_particle_truncated_by_destroy_timer": False,
            "system_tail_lifetime_certified": False,
        }
    rows = []
    for w in preflight.WEAPONS:
        for v in preflight.VARIANTS:
            protected = (w, v) in preflight.ACTIVE
            row = {
                "skill": preflight.skill_path(w, v),
                "role": "protected_existing_active_pilot" if protected else "unbound_candidate_inspection",
                "activation_ready": False, "proposed_property_updates": {},
                "trajectory": {"asset": preflight.candidate_path(w, v, "Trajectory"),
                               "binding_allowed": False, "reason": "No ribbon/contact authority"},
                "snapshot_schema": {"missing_source_declared_fields": [], "extra_fields": [],
                                    "live_reflection_verified": False},
                "snapshot_asset_hash_matches_current_disk": True,
                "blockers": [] if enabled else ["effects_enable_requires_explicit_review"],
                "timing": copy.deepcopy(timing),
                "eligible_for_scoped_live_inspection": not protected and enabled,
                "status": "protected_no_updates" if protected else "offline_conditions_pass_live_schema_render_pending",
            }
            if not protected:
                row["inspection_binding_intent"] = {
                    "cast_effect": preflight.candidate_path(w, v, "Cast"),
                    "impact_effect": preflight.candidate_path(w, v, "Impact")}
            rows.append(row)
    evidence = {"expected_sha256": "a" * 64, "actual_sha256": "a" * 64, "status": "match"}
    report = {
        "schema": preflight.SCHEMA, "status": "read_only_offline_preflight_no_activation",
        "candidate_disk_hashes": {preflight.candidate_path(w, v, p): copy.deepcopy(evidence)
                                  for w in preflight.WEAPONS for v in preflight.VARIANTS
                                  for p in preflight.PHASES},
        "source_system_disk_hashes": {preflight._source(w, p): copy.deepcopy(evidence)
                                      for w in preflight.WEAPONS for p in preflight.PHASES},
        "candidate_hash_matches": 30, "source_declared_field_count": len(before),
        "protected_active_skills": 2, "unbound_candidate_skill_count": 8,
        "skills": rows, "unreal_or_mcp_executed": False,
        "asset_or_gameplay_modified": False, "new_render_performed": False,
        "visual_quality_approved": False,
    }
    return plan, before, report


class WeaponVfxBindingTests(unittest.TestCase):
    def setUp(self):
        self.plan, self.before, self.report = valid_fixture()

    def row(self):
        return binding.checked_preflight_row(self.report, self.plan["source_asset"])

    def updates(self):
        return binding.planned_updates(self.plan, self.before, self.row())

    def test_single_target_plans_protect_the_other_nine_and_both_profiles(self):
        count = 0
        for weapon in preflight.WEAPONS:
            for variant in preflight.VARIANTS:
                if (weapon, variant) in preflight.ACTIVE:
                    with self.assertRaisesRegex(ValueError, "active pilot is protected"):
                        binding.build_plan(weapon, variant, "SafeA")
                    continue
                count += 1
                plan = binding.build_plan(weapon, variant, "SafeA")
                binding.validate_plan(plan)
                self.assertEqual(len(plan["protected_assets"]), 9)
                self.assertNotIn(plan["source_asset"], plan["protected_assets"])
                self.assertEqual(len(plan["profile_assets"]), 2)
                self.assertFalse(plan["event_contract"]["trajectory_bound"])
                self.assertFalse(plan["event_contract"]["immunity_added"])
                self.assertFalse(plan["event_contract"]["selectable_catalog_modified"])
                self.assertNotEqual(plan["staged_asset"], plan["source_asset"])
        self.assertEqual(count, 8)

    def test_plan_scope_injection_and_bad_labels_are_rejected(self):
        for label in ("", "../escape", "x/y", "a" * 25):
            with self.assertRaises(ValueError):
                binding.build_plan("GreatSword", "Primary01", label)
        for field, value in (("source_asset", preflight.skill_path("GreatSword", "Charged")),
                             ("staged_asset", self.plan["source_asset"]),
                             ("updates", {"damage": 999.})):
            plan = copy.deepcopy(self.plan)
            plan[field] = value
            with self.assertRaisesRegex(ValueError, "Plan changed"):
                binding.validate_plan(plan)

    def test_native_snapshot_conversion_is_detached_and_rejects_collisions(self):
        native = {"bEnableAttackEffects": True, "attackEffectTimeDilation": 2.,
                  "firstPersonEffectOffset": {"x": 1., "y": 2.}, "advance_before_hit": True}
        props, reverse = binding.snake_snapshot(native)
        self.assertEqual(reverse["enable_attack_effects"], "bEnableAttackEffects")
        self.assertEqual(reverse["attack_effect_time_dilation"], "attackEffectTimeDilation")
        props["first_person_effect_offset"]["x"] = 7.
        self.assertEqual(native["firstPersonEffectOffset"]["x"], 1.)
        for bad in ({"castEffect": None, "cast_effect": None},
                    {"bEnableAttackEffects": 1}, {"futureValue": float("nan")},
                    {"opaque": object()}, {"not a field": None}):
            with self.assertRaises(ValueError):
                binding.snake_snapshot(bad)

    def test_hash_receipt_requires_the_entire_current_thirty_and_four_scope(self):
        row = self.row()
        self.assertEqual(len(row["binding_hash_scope"]["candidate_disk_hashes"]), 30)
        self.assertEqual(len(row["binding_hash_scope"]["source_system_disk_hashes"]), 4)
        for kind in ("missing", "mismatch", "malformed", "source_drift", "fake_visual_claim"):
            report = copy.deepcopy(self.report)
            candidate = next(iter(report["candidate_disk_hashes"]))
            if kind == "missing":
                report["candidate_disk_hashes"].pop(candidate)
            elif kind == "mismatch":
                report["candidate_disk_hashes"][candidate]["actual_sha256"] = "b" * 64
            elif kind == "malformed":
                report["candidate_disk_hashes"][candidate]["expected_sha256"] = "sha"
            elif kind == "source_drift":
                report["source_system_disk_hashes"][next(iter(report["source_system_disk_hashes"]))]["status"] = "mismatch"
            else:
                report["visual_quality_approved"] = True
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                binding.checked_preflight_row(report, self.plan["source_asset"])

    def test_only_three_binding_fields_are_produced_and_full_audit_is_idempotent(self):
        updates = self.updates()
        self.assertEqual(set(updates), binding.ALLOWED_FIELDS)
        after = copy.deepcopy(self.before)
        after.update(updates)
        result = binding.audit_transition(self.before, after, updates)
        self.assertEqual(result["changed_fields"], ["cast_effect", "impact_effect"])
        self.assertIn("no live/disk/gameplay/render approval", result["proof_scope"])
        self.assertTrue(binding.audit_transition(after, after, updates)["idempotent_noop"])
        self.assertTrue(binding.audit_transition(after, after, {})["idempotent_noop"])

    def test_enable_review_is_explicit_and_null_charge_and_legacy_are_mandatory(self):
        self.plan, self.before, self.report = valid_fixture(enabled=False)
        updates = self.updates()
        self.assertIs(updates["enable_attack_effects"], True)
        for field in ("charge_effect", "attack_effect"):
            before = copy.deepcopy(self.before)
            before[field] = "/Game/Test/ExistingFx.ExistingFx"
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "Charge|legacy"):
                binding.planned_updates(self.plan, before, self.row())
            after = copy.deepcopy(before)
            after.update(updates)
            with self.assertRaisesRegex(ValueError, "Shared enable"):
                binding.audit_transition(before, after, updates)
        missing = copy.deepcopy(self.before)
        missing.pop("attack_effect")
        with self.assertRaisesRegex(ValueError, "legacy AttackEffect required"):
            binding.planned_updates(self.plan, missing, self.row())

    def test_already_enabled_charge_and_legacy_references_stay_strictly_preserved(self):
        self.before.update(charge_effect="/Game/Test/Charge.Charge", attack_effect="/Game/Test/Legacy.Legacy")
        updates = self.updates()
        after = copy.deepcopy(self.before)
        after.update(updates)
        binding.audit_transition(self.before, after, updates)
        after["charge_effect"] = {"refPath": "/Game/Test/Charge.Charge"}
        with self.assertRaisesRegex(ValueError, "Protected reflected field changed: charge_effect"):
            binding.audit_transition(self.before, after, updates)

    def test_missing_schema_stale_snapshot_and_unresolved_blockers_never_pass(self):
        for key, value in (("blockers", ["missing_source_snapshot"]),
                           ("blockers", ["cast_first_person_timer_truncates_candidate"]),
                           ("snapshot_asset_hash_matches_current_disk", False),
                           ("snapshot_schema", {"missing_source_declared_fields": ["advance_distance"],
                                                "extra_fields": [], "live_reflection_verified": False}),
                           ("activation_ready", True)):
            row = self.row()
            row[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                binding.planned_updates(self.plan, self.before, row)
        raw = next(r for r in self.report["skills"] if r["skill"] == self.plan["source_asset"])
        with self.assertRaisesRegex(ValueError, "checked_preflight_row"):
            binding.planned_updates(self.plan, self.before, raw)
        truncated = copy.deepcopy(self.before)
        truncated.pop("future_nested")
        with self.assertRaisesRegex(ValueError, "complete source field count"):
            binding.planned_updates(self.plan, truncated, self.row())

    def test_before_timing_must_match_receipt_and_truncation_is_recomputed(self):
        for field, value in (("attack_effect_time_dilation", 3.),
                             ("cast_effect_emission_duration", .5),
                             ("first_person_emission_duration_multiplier", .1)):
            before = copy.deepcopy(self.before)
            before[field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "stale or inconsistent"):
                binding.planned_updates(self.plan, before, self.row())
        row = self.row()
        # A forged false truncation flag cannot override consistent numeric evidence.
        row["timing"]["cast"].update(candidate_particle_sim_seconds=.5, particle_world_seconds_estimate=.25)
        with self.assertRaisesRegex(ValueError, "truncated"):
            binding.planned_updates(self.plan, self.before, row)

    def test_editor_tuned_finite_lifetime_uses_current_asset_budgets(self):
        before = copy.deepcopy(self.before)
        row = self.row()
        for phase in ("cast", "impact"):
            before[phase + "_effect_emission_duration"] = 1.
            row["timing"][phase].update(
                candidate_particle_sim_seconds=.75, particle_world_seconds_estimate=.375,
                destroy_timer_raw_world_seconds=1., first_person_destroy_timer_raw_world_seconds=.5,
                destroy_timer_world_seconds=1., first_person_destroy_timer_world_seconds=.5)
        self.assertEqual(binding.planned_updates(self.plan, before, row), self.plan["updates"])

    def test_gameplay_catalog_interruption_first_person_and_unknown_fields_are_frozen(self):
        updates = self.updates()
        for field, value in (("damage", 999.), ("hit_start_delay_seconds", .7),
                             ("animation_play_rate", 2.), ("advance_before_hit", False),
                             ("advance_distance", 999.), ("allowed_slot_policy", "Any"),
                             ("skill_tree_node_type", "Passive"),
                             ("first_person_emission_duration_multiplier", 1.),
                             ("attack_effect_time_dilation", 3.),
                             ("future_nested", {"flags": [1, 1, 1., {"key": "preserved"}]})):
            after = copy.deepcopy(self.before)
            after.update(updates)
            after[field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "Protected reflected field changed"):
                binding.audit_transition(self.before, after, updates)

    def test_illegal_updates_cross_skill_refs_and_active_pilots_are_rejected(self):
        after = copy.deepcopy(self.before)
        after.update(self.updates())
        bad = copy.deepcopy(self.updates())
        bad["damage"] = 99.
        with self.assertRaisesRegex(ValueError, "exact three binding fields"):
            binding.audit_transition(self.before, after, bad)
        for weapon, variant in (("Knuckle", "Primary03"), ("GreatSword", "Primary02")):
            bad = copy.deepcopy(self.updates())
            bad["impact_effect"] = preflight.candidate_path(weapon, variant, "Impact")
            with self.subTest(weapon=weapon, variant=variant), self.assertRaisesRegex(ValueError, "cross-skill"):
                binding.audit_transition(self.before, after, bad)
        active = preflight.skill_path("GreatSword", "Charged")
        with self.assertRaisesRegex(ValueError, "non-active pilot"):
            binding.checked_preflight_row(self.report, active)

    def test_snapshot_and_receipt_values_are_detached_and_nonfinite_data_fail(self):
        updates = self.updates()
        updates["cast_effect"] = "changed"
        self.assertNotEqual(self.plan["updates"]["cast_effect"], "changed")
        row = self.row()
        row["binding_hash_scope"]["candidate_disk_hashes"].clear()
        self.assertEqual(len(self.report["candidate_disk_hashes"]), 30)
        for bad in (float("nan"), float("inf"), object()):
            after = copy.deepcopy(self.before)
            after.update(self.updates())
            after["future_nested"] = bad
            with self.subTest(bad=repr(bad)), self.assertRaises(ValueError):
                binding.audit_transition(self.before, after, self.updates())


if __name__ == "__main__":
    unittest.main(verbosity=2)
