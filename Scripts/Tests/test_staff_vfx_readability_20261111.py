"""Offline contract and simulated MCP checks. No engine/process/network is used."""
from pathlib import Path
import copy
import json
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "Scripts"), str(ROOT / "Scripts/Editor"), str(ROOT / "Scripts/Tests")]
import StaffVfxReadability20261111 as contract
import StageStaffVfxReadability20261111 as stage
from test_magic_skill_vfx_mcp_20261111 import FakeNative


class ReadabilityContractTests(unittest.TestCase):
    def setUp(self):
        self.plan = contract.build_plan("TrialA")
        self.client = FakeNative(stage.native.build_binding_plan(ROOT))
        source = self.client.props[self.plan["source_asset"]]
        source.update(stage.native.native_updates(stage.native.build_binding_plan(ROOT)["targets"][0]["updates"]))
        source.update({"firstPersonEffectRotation": {"pitch": 0., "yaw": 0., "roll": 0.},
                       "thirdPersonCastEffectOffset": {"x": 0., "y": 0., "z": 0.},
                       "thirdPersonCastEffectRotation": {"pitch": 0., "yaw": 0., "roll": 0.},
                       "bUseThirdPersonImpactBoundsSurface": False,
                       "thirdPersonImpactEffectSurfaceOffset": 0.})
        self.before = copy.deepcopy(source)
        self.after = copy.deepcopy(source)
        self.after.update(copy.deepcopy(contract.UPDATES))

    def test_exact_four_fields_only_and_trial_stays_unbound(self):
        contract.validate_plan(self.plan)
        report = contract.audit_transition(self.before, self.after)
        self.assertEqual(set(report["changed_fields"]), set(contract.UPDATES) - {"thirdPersonCastEffectRotation"})
        self.assertNotIn("castEffectScale", contract.UPDATES)
        self.assertEqual(self.before["hitStartDelaySeconds"], self.after["hitStartDelaySeconds"])
        self.assertNotEqual(self.plan["source_asset"], self.plan["staged_asset"])

    def test_runtime_profiles_cannot_be_mislabeled_after_activation(self):
        self.assertEqual(contract.validate_runtime_profile(self.before, False), "baseline_zero_placement")
        self.assertEqual(contract.validate_runtime_profile(self.after, True), "readability_candidate")
        for properties, selector in ((self.after, False), (self.before, True)):
            with self.assertRaisesRegex(ValueError, "Wrong runtime presentation profile"):
                contract.validate_runtime_profile(properties, selector)
        with self.assertRaisesRegex(ValueError, "selector must be boolean"):
            contract.validate_runtime_profile(self.before, 0)

    def test_baseline_capture_requires_exact_complete_readback_and_zero_placement(self):
        baseline = stage.inspect(self.client, self.plan)["properties"]
        applied = {"properties_after": copy.deepcopy(baseline), "nonvisual_fields_preserved": True}
        readback = {"live": {"properties": copy.deepcopy(baseline)}}
        self.assertEqual(contract.audit_baseline_capture_reports(applied, readback), "baseline_zero_placement")
        applied["properties_after"][self.plan["source_asset"]] = copy.deepcopy(self.after)
        readback["live"]["properties"][self.plan["source_asset"]] = copy.deepcopy(self.after)
        with self.assertRaisesRegex(ValueError, "Wrong runtime presentation profile"):
            contract.audit_baseline_capture_reports(applied, readback)
        applied["properties_after"].pop(next(iter(self.plan["protected_assets"])))
        with self.assertRaisesRegex(ValueError, "complete property scope"):
            contract.audit_baseline_capture_reports(applied, readback)

    def test_future_gameplay_first_person_montage_timing_and_shared_vfx_changes_are_rejected(self):
        for field, value in {"damage": 999., "futureGameplayContract": "changed",
                             "hitStartDelaySeconds": .2, "animationPlayRate": 2.,
                             "firstPersonEmissionDurationMultiplier": 1.,
                             "firstPersonCastScaleMultiplier": 1., "castEffectScale": 2.,
                             "castEffectEmissionDuration": .8, "attackEffectTimeDilation": .5}.items():
            with self.subTest(field=field):
                before = copy.deepcopy(self.before)
                after = copy.deepcopy(self.after)
                if field == "futureGameplayContract":
                    before[field] = "original"
                after[field] = value
                with self.assertRaisesRegex(ValueError, "Protected reflected field"):
                    contract.audit_transition(before, after)

    def test_schema_missing_after_unbuilt_cpp_and_shared_tuning_drift_fail_closed(self):
        missing = copy.deepcopy(self.before)
        del missing["thirdPersonCastEffectOffset"]
        with self.assertRaisesRegex(ValueError, "schema required"):
            contract.validate_source(missing)
        drift = copy.deepcopy(self.before)
        drift["impactEffectEmissionDuration"] = .4
        with self.assertRaisesRegex(ValueError, "Shared visual tuning drift"):
            contract.validate_source(drift)

    def test_plan_injection_and_nonfinite_offsets_rejected(self):
        for bad in ("../escape", "trial/child", "", "x" * 25):
            with self.assertRaises(ValueError):
                contract.build_plan(bad)
        plan = copy.deepcopy(self.plan)
        plan["updates"]["hitStartDelaySeconds"] = .1
        with self.assertRaises(ValueError):
            contract.validate_plan(plan)
        self.after["thirdPersonCastEffectOffset"]["x"] = float("nan")
        with self.assertRaises(ValueError):
            contract.audit_transition(self.before, self.after)

    def test_setter_failure_is_not_retried_or_saved(self):
        self.client.setter_ok = False
        report = {"mutation_attempted": False}
        with self.assertRaisesRegex(RuntimeError, "Setter failed"):
            stage.checked_set(self.client, self.plan["source_asset"], self.before, report)
        writes = [c for c in self.client.calls if c[1] in ("set_properties", "save_assets")]
        self.assertEqual([c[1] for c in writes], ["set_properties"])
        self.assertTrue(report["mutation_attempted"])

    def test_only_new_four_fields_are_dispatched(self):
        report = {}
        after, changed = stage.checked_set(self.client, self.plan["source_asset"], self.before, report)
        self.assertTrue(changed)
        setter = next(c for c in self.client.calls if c[1] == "set_properties")
        self.assertLessEqual(set(json.loads(setter[2]["values"])), set(contract.UPDATES))
        self.assertEqual(after, self.after)
        self.assertFalse(any(c[1] == "save_assets" for c in self.client.calls))

    def test_stale_snapshot_and_existing_candidate_stop_before_duplicate(self):
        baseline = stage.inspect(self.client, self.plan)
        self.client.props[self.plan["source_asset"]]["damage"] += 1
        self.client.calls.clear()
        with self.assertRaisesRegex(RuntimeError, "Stale"):
            stage.stage(self.client, self.plan, baseline, {})
        self.assertFalse(any(c[1] in ("duplicate", "set_properties", "save_assets") for c in self.client.calls))

    def test_duplicate_stage_preserves_active_source_and_protected_assets(self):
        original = self.client.call
        candidate = self.plan["staged_asset"]
        def simulated(ts, name, args):
            if name == "exists":
                return args["path"] in self.client.props
            if name == "duplicate":
                self.client.calls.append((ts, name, copy.deepcopy(args)))
                self.client.props[args["new_path"]] = copy.deepcopy(self.client.props[args["path"]])
                return {"refPath": stage.native.object_path(args["new_path"])}
            return original(ts, name, args)
        self.client.call = simulated
        baseline = stage.inspect(self.client, self.plan)
        report = {"mutation_attempted": False, "asset_saved": False}
        with patch.object(stage.native, "file_sha", return_value="saved-sha"):
            stage.stage(self.client, self.plan, baseline, report)
        self.assertEqual(report["status"], "staged_unbound_candidate_saved_runtime_review_pending")
        self.assertEqual(self.client.props[self.plan["source_asset"]], self.before)
        self.assertEqual(self.client.props[candidate], self.after)
        saved = [c for c in self.client.calls if c[1] == "save_assets"]
        self.assertEqual(saved[-1][2]["asset_paths"], [candidate])

    def _staged(self):
        baseline = stage.inspect(self.client, self.plan)
        self.client.props[self.plan["staged_asset"]] = copy.deepcopy(self.after)
        report = {"plan": self.plan, "status": "staged_unbound_candidate_saved_runtime_review_pending",
                  "asset_saved": True, "live": baseline, "saved_asset_sha256": "staged-sha",
                  "candidate_properties": copy.deepcopy(self.after)}
        return baseline, report

    def _hash(self, path):
        return "staged-sha" if "ReadabilityCandidates" in str(path) else "primary-sha"

    def test_activation_only_changes_four_fields_and_pins_both_saved_assets(self):
        baseline, staged = self._staged()
        report = {"phase": "activate", "plan": self.plan, "mutation_attempted": False, "asset_saved": False}
        with patch.object(stage.native, "file_sha", side_effect=self._hash):
            stage.activate(self.client, self.plan, baseline, staged, report)
        self.assertEqual(report["staged_asset_sha256"], "staged-sha")
        self.assertEqual(report["saved_asset_sha256"], "primary-sha")
        self.assertTrue(report["saved_clean_readback_verified"])
        self.assertEqual(report["properties_after"][self.plan["source_asset"]], self.after)
        self.assertTrue(report["nonvisual_fields_preserved"])
        self.assertEqual([c[2]["asset_paths"] for c in self.client.calls if c[1] == "save_assets"], [[self.plan["source_asset"]]])

    def test_idempotent_activation_pins_clean_disk_without_save(self):
        self.client.props[self.plan["source_asset"]] = copy.deepcopy(self.after)
        baseline, staged = self._staged()
        self.client.calls.clear()
        report = {"asset_saved": False}
        with patch.object(stage.native, "file_sha", side_effect=self._hash):
            stage.activate(self.client, self.plan, baseline, staged, report)
        self.assertEqual(report["saved_asset_sha256"], "primary-sha")
        self.assertTrue(report["saved_clean_readback_verified"])
        self.assertFalse(report["asset_saved"])
        self.assertFalse(any(c[1] in ("set_properties", "save_assets") for c in self.client.calls))

    def test_readback_audits_every_field_and_rejects_saved_or_property_drift(self):
        baseline, staged = self._staged()
        applied = {"phase": "activate", "plan": self.plan, "mutation_attempted": False, "asset_saved": False}
        with patch.object(stage.native, "file_sha", side_effect=self._hash):
            stage.activate(self.client, self.plan, baseline, staged, applied)
        hashes = {"primary": "primary-sha", "implementation": "pinned"}
        applied.update(protected_files_preserved=True, fingerprints_after=hashes)
        report = {}
        with patch.object(stage.native, "file_sha", side_effect=self._hash):
            stage.verify(self.client, self.plan, baseline, applied, hashes, report)
        self.assertEqual(report["status"], "four_property_trial_readback_verified_gameplay_qa_pending")
        self.assertEqual(report["staged_asset_sha256"], "staged-sha")
        self.client.props[self.plan["source_asset"]]["firstPersonCastScaleMultiplier"] = .99
        with patch.object(stage.native, "file_sha", side_effect=self._hash):
            with self.assertRaisesRegex(ValueError, "Protected reflected field"):
                stage.verify(self.client, self.plan, baseline, applied, hashes, {})
        with self.assertRaisesRegex(RuntimeError, "pinned activation/current artifact"):
            stage.verify(self.client, self.plan, baseline, applied, {"primary": "wrong"}, {})

    def test_readability_fingerprints_pin_all_six_trial_implementation_files(self):
        with patch.object(stage.native, "fingerprints", return_value={"old": "baseline"}), patch.object(stage.native, "file_sha", return_value="hash"):
            hashes = stage.readability_fingerprints(self.plan)
        self.assertEqual(len(hashes), 7)
        self.assertIn("Source/Rogue10m/Components/Rogue10mCombatVfxPlacement.h", hashes)
        self.assertIn("Scripts/VerifyStaffVfxRuntime.ps1", hashes)

    def test_direct_listener_rejects_proxy_and_other_checkout(self):
        target = ROOT / "Rogue10m.uproject"
        row = {"Name": "UnrealEditor.exe", "ProcessId": 17, "CommandLine": "test"}
        self.assertEqual(stage.validate_listener_identity(row, target, lambda _: ["UE", str(target)]), row)
        wrong = copy.deepcopy(row)
        wrong["Name"] = "python.exe"
        with self.assertRaisesRegex(RuntimeError, "not direct"):
            stage.validate_listener_identity(wrong, target, lambda _: ["UE", str(target)])
        with self.assertRaisesRegex(RuntimeError, "different/unproven"):
            stage.validate_listener_identity(row, target, lambda _: ["UE", str(ROOT.parent / "clone/Rogue10m.uproject")])

    def test_cold_flag_requires_new_pid_and_old_process_gone(self):
        applied = {"listener_identity": {"pid": 10}}
        same = {"listener_identity": {"pid": 10}}
        stage.cold_readback(applied, same, lambda _: False)
        self.assertFalse(same["cold_reload_validated"])
        alive = {"listener_identity": {"pid": 11}}
        stage.cold_readback(applied, alive, lambda _: True)
        self.assertFalse(alive["cold_reload_validated"])
        cold = {"listener_identity": {"pid": 11}}
        stage.cold_readback(applied, cold, lambda _: False)
        self.assertTrue(cold["cold_reload_validated"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
