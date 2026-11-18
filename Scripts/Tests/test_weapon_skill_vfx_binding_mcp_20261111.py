"""Offline fake-MCP workflow checks; never contacts Unreal or shared assets."""
from pathlib import Path
import copy
import contextlib
import io
import json
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "Scripts"), str(ROOT / "Scripts/Editor"), str(ROOT / "Scripts/Tests")]
import StageWeaponSkillVfxBinding20261111 as workflow
import test_weapon_skill_vfx_preflight_20261111 as fixtures


def native_name(name):
    parts = name.split("_")
    camel = parts[0] + "".join(part[:1].upper() + part[1:] for part in parts[1:])
    if name in ("enable_attack_effects", "use_first_person_effect_overrides", "advance_before_hit"):
        return "b" + camel[:1].upper() + camel[1:]
    return camel


class FakeWeaponNative:
    """Detached properties and files under an owned short temporary project."""
    def __init__(self, plan, fixture, fields):
        self.plan = plan
        self.root = fixture.root
        self.calls = []
        self.props = {}
        self.dirty = set()
        self.locked = set()
        self.wrong_class = set()
        self.materials = {}
        self.material_parents = {}
        self.compiling = False
        self.pie = False
        self.setter_ok = True
        self.save_ok = True
        self.duplicate_result = True
        self.duplicate_partial = False
        self.collateral_field = None
        self.collateral_asset = None
        for path, snap in fixture.snapshots["skills"].items():
            values = dict.fromkeys(fields)
            values.update(copy.deepcopy(snap["properties"]))
            values.update(attack_effect=None, effect_attach_socket="hand_r", input_slot="Primary",
                          hit_mode="Single", attack_shape="Sphere", allowed_slot_policy="Legacy",
                          skill_tree_node_type="ActiveTechnique", advance_before_hit=False,
                          advance_distance=0., advance_duration=.12,
                          first_person_effect_offset={"x": 0., "y": 0., "z": 0.},
                          first_person_effect_rotation={"pitch": 0., "yaw": 0., "roll": 0.})
            # Follow this checkout's entire declaration without inventing fields
            # from a different live build. This is synthetic reflection only.
            values = {k: v for k, v in values.items() if k in fields}
            self.props[path] = {native_name(k): v for k, v in values.items()}
        for profile in plan["profile_assets"]:
            weapon = profile.rsplit("_", 1)[-1]
            self.props[profile] = {"weaponType": weapon, "activeTechniqueCatalog": [],
                "skillTreeSkills": [{"refPath": workflow.native.object_path(workflow.preflight.skill_path(weapon, v))}
                                    for v in workflow.preflight.VARIANTS]}
            fixture.write_package(profile, profile.encode())
        self.props[plan["character_asset"]] = {"cameraMode": "ThirdPerson", "weaponSkillProfiles": [
            {"refPath": workflow.native.object_path(p)} for p in plan["profile_assets"]]}
        fixture.write_package(plan["character_asset"], b"third-person-character")
        self.niagara = set(fixture.pins)

    def call(self, toolset, name, args):
        self.calls.append((toolset, name, copy.deepcopy(args)))
        if name == "IsPIERunning":
            return self.pie
        if name == "ListSkills":
            return {"skill://project/vfx-test": {"description": "Keep cancellation and third person"}}
        if name == "GetSkills":
            return {"skill://project/vfx-test": "Preserve timing, hit interruption and camera. Do not add immunity."}
        if name == "is_dirty":
            return args["asset_path"] in self.dirty
        if name == "can_edit_asset":
            return args["asset_path"] not in self.locked
        if name == "get_asset_class":
            path = args["asset_path"]
            if path in self.wrong_class:
                return "WrongClass"
            if path in self.niagara:
                return "NiagaraSystem"
            if path in self.materials:
                return self.materials[path]
            if path in self.plan["profile_assets"]:
                return "Rogue10mWeaponSkillProfileDataAsset"
            if path == self.plan["character_asset"]:
                return "Rogue10mCharacterDataAsset"
            return "Rogue10mAttackSkillData"
        if name == "GetSystemCompileState":
            return {"bIsCompiling": self.compiling, "bIsStale": False, "bHasErrors": False,
                    "aggregateStatus": "UpToDate"}
        if name == "GetStackIssues":
            return {"numErrors": 0, "numWarnings": 0}
        if name == "exists":
            return args["path"] in self.props
        if name == "duplicate":
            destination = args["new_path"]
            self.props[destination] = copy.deepcopy(self.props[args["path"]])
            self.dirty.add(destination)
            if self.duplicate_partial:
                self.props[destination].pop("damage")
            if self.duplicate_result is not True:
                return self.duplicate_result
            return {"refPath": workflow.native.object_path(destination)}
        if name in ("list_properties", "get_properties", "set_properties"):
            path = args["instance"]["refPath"].split(".", 1)[0]
            if path in self.materials:
                if name == "list_properties":
                    return {"parent": {"type": "object"}}
                if name == "get_properties":
                    return {"parent": {"refPath": workflow.native.object_path(self.material_parents[path])}}
                raise AssertionError("Material setters are never allowed")
            values = self.props[path]
            if name == "list_properties":
                return {k: {"type": "synthetic"} for k in values}
            if name == "get_properties":
                return copy.deepcopy({k: values[k] for k in args["properties"]})
            values.update(json.loads(args["values"]))
            self.dirty.add(path)
            if self.collateral_field:
                values[self.collateral_field] = True if self.collateral_field == "futureHitImmunity" else 999.
            if self.collateral_asset:
                self.props[self.collateral_asset]["damage"] = 999.
            return self.setter_ok
        if name == "save_assets":
            if not self.save_ok:
                return False
            for path in args["asset_paths"]:
                target = workflow.native.package_file(path)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(json.dumps(self.props[path], sort_keys=True), encoding="utf-8")
                self.dirty.discard(path)
            return True
        raise AssertionError("Unexpected fake tool: " + name)


class WeaponBindingWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.WeaponVfxPreflightTests(
            "test_exact_eight_inspection_intents_protect_active_two_and_all_trajectories")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.project = self.fixture.root
        header = (ROOT / "Source/Rogue10m/Data/Rogue10mAttackSkillData.h").read_text(encoding="utf-8-sig")
        header_path = self.project / "Source/Rogue10m/Data/Rogue10mAttackSkillData.h"
        header_path.parent.mkdir(parents=True, exist_ok=True)
        header_path.write_text(header, encoding="utf-8")
        fields = workflow.preflight.schema_fields(header)
        self.plan = workflow.contract.build_plan("GreatSword", "Primary01", "TrialA")
        self.client = FakeWeaponNative(self.plan, self.fixture, fields)
        for row in self.fixture.manifest["candidates"]:
            material = row["source"].replace("/NS_", "/M_")
            self.fixture.write_package(material, material.encode())
            self.client.materials[material] = "Material"
            row["readback"] = {"renderer": {"propertyValues": json.dumps({
                "Material": {"refPath": workflow.native.object_path(material)}})}}
        (self.project / "Rogue10m.uproject").write_text("{}", encoding="utf-8")
        (self.project / "Config").mkdir()
        (self.project / "Config/DefaultGame.ini").write_text("[synthetic]\n", encoding="utf-8")
        for target in (workflow, workflow.native):
            changed = patch.object(target, "ROOT", self.project)
            changed.start()
            self.addCleanup(changed.stop)
        self.manifest = self.fixture.manifest
        self.pins = self.fixture.pins
        self.baseline = workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.checkpoint = workflow.fingerprints(self.plan, self.manifest)
        self.client.calls.clear()

    def writes(self):
        return [c for c in self.client.calls if c[1] in ("duplicate", "set_properties", "save_assets")]

    def stage(self):
        report = {"phase": "stage", "plan": self.plan, "live": copy.deepcopy(self.baseline),
                  "mutation_attempted": False, "asset_saved": False,
                  "fingerprints_before": copy.deepcopy(self.checkpoint)}
        workflow.stage(self.client, self.plan, self.baseline, self.manifest, self.pins, report)
        return report

    def activate(self, staged=None):
        staged = self.stage() if staged is None else staged
        report = {"phase": "activate", "plan": self.plan, "mutation_attempted": False, "asset_saved": False,
                  "fingerprints_before": copy.deepcopy(self.checkpoint)}
        workflow.activate(self.client, self.plan, self.baseline, staged, self.manifest, self.pins, report)
        return report

    def test_real_preflight_full_inspection_protects_other_nine_profiles_and_character(self):
        self.assertEqual(len(self.baseline["properties"]), 13)
        self.assertEqual(self.baseline["preflight"]["candidate_hash_matches"], 30)
        self.assertEqual(set(self.baseline["planned_updates"]), workflow.contract.ALLOWED_FIELDS)
        self.assertEqual(self.baseline["properties"][self.plan["character_asset"]]["cameraMode"], "ThirdPerson")
        self.assertEqual(len(workflow.material_assets(self.manifest)), 4)

    def test_stage_then_activate_and_verify_preserve_timing_and_all_protected_assets(self):
        staged = self.stage()
        self.assertEqual(staged["status"], "single_unbound_weapon_candidate_saved_review_pending")
        self.assertEqual(self.client.props[self.plan["source_asset"]], self.baseline["properties"][self.plan["source_asset"]])
        self.client.calls.clear()
        applied = self.activate(staged)
        self.assertEqual([c[1] for c in self.writes()], ["set_properties", "save_assets"])
        self.assertEqual(next(c for c in self.writes() if c[1] == "save_assets")[2]["asset_paths"], [self.plan["source_asset"]])
        self.assertTrue(applied["nonvisual_fields_preserved"])
        self.assertTrue(applied["saved_clean_readback_verified"])
        hashes = workflow.fingerprints(self.plan, self.manifest)
        applied.update(protected_files_preserved=True, fingerprints_after=hashes)
        self.client.calls.clear()
        result = {}
        workflow.verify(self.client, self.plan, self.baseline, applied, self.manifest, self.pins, hashes, result)
        self.assertEqual(result["status"], "single_weapon_binding_readback_verified_runtime_qa_pending")
        self.assertEqual(self.writes(), [])
        after, _ = workflow.contract.snake_snapshot(result["source_properties_after"])
        before = self.baseline["source_snake"]
        for field in set(before) - workflow.contract.ALLOWED_FIELDS:
            self.assertEqual(before[field], after[field], field)

    def test_idempotent_activation_pins_saved_source_without_setter_or_save(self):
        source = self.plan["source_asset"]
        self.client.props[source].update(workflow.native_updates(
            self.plan["updates"], self.baseline["source_name_map"]))
        self.client.call(workflow.native.A, "save_assets", {"asset_paths": [source]})
        self.baseline = workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.checkpoint = workflow.fingerprints(self.plan, self.manifest)
        staged = self.stage()
        self.client.calls.clear()
        applied = self.activate(staged)
        self.assertEqual(self.writes(), [])
        self.assertFalse(applied["asset_saved"])
        self.assertTrue(applied["saved_clean_readback_verified"])
        self.assertEqual(applied["saved_asset_sha256"], workflow.native.file_sha(workflow.native.package_file(source)))

    def test_inspect_refuses_pie_dirty_locked_wrong_class_and_compile_before_writes(self):
        for kind in ("pie", "dirty", "locked", "class", "compile"):
            with self.subTest(kind=kind):
                self.client.pie = kind == "pie"
                self.client.compiling = kind == "compile"
                self.client.dirty = {self.plan["protected_assets"][0]} if kind == "dirty" else set()
                self.client.locked = {self.plan["source_asset"]} if kind == "locked" else set()
                self.client.wrong_class = {self.plan["source_asset"]} if kind == "class" else set()
                self.client.calls.clear()
                with self.assertRaises(RuntimeError):
                    workflow.inspect(self.client, self.plan, self.manifest, self.pins)
                self.assertEqual(self.writes(), [])

    def test_dirty_saved_material_dependency_blocks_before_mutation(self):
        material = workflow.material_assets(self.manifest)[0]
        self.client.dirty.add(material)
        with self.assertRaisesRegex(RuntimeError, "material dependency has unsaved changes"):
            workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.assertEqual(self.writes(), [])

    def use_material_instances(self):
        materials = set()
        for index, row in enumerate(self.manifest["candidates"]):
            material = "/Game/Rogue10m/VFX/EditedStyle/MI_Candidate_" + str(index)
            self.fixture.write_package(material, material.encode())
            self.client.materials[material] = "MaterialInstanceConstant"
            self.client.material_parents[material] = row["source"].replace("/NS_", "/M_")
            row["readback"]["renderer"]["propertyValues"] = json.dumps({
                "Material": {"refPath": workflow.native.object_path(material)}})
            materials.add(material)
        return materials

    def test_editor_authored_per_skill_material_instances_follow_saved_references(self):
        materials = self.use_material_instances()
        self.assertEqual(set(workflow.material_assets(self.manifest)), materials)
        live = workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.assertEqual(set(live["clean_saved_material_dependencies"]), materials)
        self.assertEqual(len(live["material_parent_graph"]), 34)
        self.assertEqual(self.writes(), [])

    def test_parent_material_file_change_is_protected_without_reverting_it(self):
        self.use_material_instances()
        self.baseline = workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.checkpoint = workflow.fingerprints(self.plan, self.manifest, self.baseline["material_parent_graph"])
        parent = next(iter(self.client.material_parents.values()))
        key = workflow.native.package_file(parent).relative_to(self.project).as_posix()
        self.assertIn(key, self.checkpoint)
        self.fixture.write_package(parent, b"other-Codex-parent-edit")
        with patch.object(workflow, "inspect", return_value=copy.deepcopy(self.baseline)):
            with self.assertRaisesRegex(RuntimeError, "parent chain changed"):
                self.stage()
        self.assertEqual(workflow.native.package_file(parent).read_bytes(), b"other-Codex-parent-edit")
        self.assertEqual(self.writes(), [])

    def test_dirty_or_cyclic_material_parent_stops_readonly_inspection(self):
        self.use_material_instances()
        child, parent = next(iter(self.client.material_parents.items()))
        self.client.dirty.add(parent)
        with self.assertRaisesRegex(RuntimeError, "unsaved changes"):
            workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.client.dirty.clear()
        self.client.material_parents[child] = child
        with self.assertRaisesRegex(RuntimeError, "parent cycle"):
            workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.assertEqual(self.writes(), [])

    def test_profile_tree_and_third_person_character_state_are_read_only(self):
        profile = self.plan["profile_assets"][0]
        self.client.props[profile]["skillTreeSkills"] = []
        with self.assertRaisesRegex(RuntimeError, "absent from its weapon tree"):
            workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.assertEqual(self.writes(), [])

    def test_passive_node_and_missing_or_malformed_catalog_schema_are_blocked(self):
        source = self.plan["source_asset"]
        self.client.props[source]["skillTreeNodeType"] = "PassiveTechnique"
        with self.assertRaisesRegex(RuntimeError, "active technique"):
            workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.client.props[source]["skillTreeNodeType"] = "ActiveTechnique"
        profile = self.plan["profile_assets"][0]
        for catalog in (None, {"unproven": True}, [{"techniqueId": "OnlyOne"}]):
            with self.subTest(catalog=catalog):
                self.client.props[profile]["activeTechniqueCatalog"] = catalog
                with self.assertRaises(RuntimeError):
                    workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.assertEqual(self.writes(), [])

    def test_explicit_enable_only_occurs_for_null_charge_and_legacy_feedback(self):
        source = self.plan["source_asset"]
        self.client.props[source]["bEnableAttackEffects"] = False
        self.baseline = workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.checkpoint = workflow.fingerprints(self.plan, self.manifest)
        self.client.calls.clear()
        self.stage()
        setter = next(c for c in self.writes() if c[1] == "set_properties")
        values = json.loads(setter[2]["values"])
        self.assertEqual(set(values), {"bEnableAttackEffects", "castEffect", "impactEffect"})
        self.assertIs(values["bEnableAttackEffects"], True)
        self.assertIs(self.client.props[source]["bEnableAttackEffects"], False)

    def test_charge_legacy_and_timer_truncation_stop_before_stage(self):
        source = self.plan["source_asset"]
        original = copy.deepcopy(self.client.props[source])
        for field in ("chargeEffect", "attackEffect"):
            with self.subTest(field=field):
                self.client.props[source] = copy.deepcopy(original)
                self.client.props[source]["bEnableAttackEffects"] = False
                self.client.props[source][field] = {"refPath": "/Game/User/ExistingEffect.ExistingEffect"}
                with self.assertRaises(ValueError):
                    workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.client.props[source] = original
        self.client.props[source]["castEffectEmissionDuration"] = .01
        with self.assertRaisesRegex(ValueError, "preflight blocker"):
            workflow.inspect(self.client, self.plan, self.manifest, self.pins)
        self.assertEqual(self.writes(), [])

    def test_stale_property_or_skills_snapshot_stops_before_duplicate(self):
        self.client.props[self.plan["source_asset"]]["animationPlayRate"] = 1.4
        with self.assertRaisesRegex(RuntimeError, "Stale"):
            self.stage()
        self.assertEqual(self.writes(), [])

    def test_existing_destination_and_uncertain_or_partial_duplicate_are_not_retried(self):
        self.client.props[self.plan["staged_asset"]] = copy.deepcopy(self.client.props[self.plan["source_asset"]])
        with self.assertRaisesRegex(RuntimeError, "Fresh unbound"):
            self.stage()
        self.assertEqual(self.writes(), [])
        self.client.props.pop(self.plan["staged_asset"])
        self.client.duplicate_result = False
        with self.assertRaisesRegex(RuntimeError, "Duplicate failed/uncertain"):
            self.stage()
        self.assertEqual([c[1] for c in self.writes()], ["duplicate"])
        self.client.props.pop(self.plan["staged_asset"])
        self.client.dirty.discard(self.plan["staged_asset"])
        self.client.duplicate_result = True
        self.client.duplicate_partial = True
        self.client.calls.clear()
        with self.assertRaisesRegex(RuntimeError, "retain full source state"):
            self.stage()
        self.assertEqual([c[1] for c in self.writes()], ["duplicate"])

    def test_setter_failure_is_not_saved_or_retried(self):
        self.client.setter_ok = False
        report = {"mutation_attempted": False}
        with self.assertRaisesRegex(RuntimeError, "Setter failed/uncertain"):
            workflow.mutate(self.client, self.plan["source_asset"],
                self.baseline["properties"][self.plan["source_asset"]], self.baseline["planned_updates"], report)
        self.assertTrue(report["mutation_attempted"])
        self.assertEqual([c[1] for c in self.writes()], ["set_properties"])

    def test_collateral_damage_and_future_immunity_change_stop_before_save(self):
        for field in ("damage", "futureHitImmunity"):
            with self.subTest(field=field):
                before = copy.deepcopy(self.baseline["properties"][self.plan["source_asset"]])
                if field == "futureHitImmunity":
                    before[field] = False
                self.client.props[self.plan["source_asset"]] = copy.deepcopy(before)
                self.client.dirty.discard(self.plan["source_asset"])
                self.client.collateral_field = field
                self.client.calls.clear()
                with self.assertRaisesRegex(ValueError, "Protected reflected field changed"):
                    workflow.mutate(self.client, self.plan["source_asset"], before,
                                    self.baseline["planned_updates"], {})
                self.assertEqual([c[1] for c in self.writes()], ["set_properties"])

    def test_protected_other_skill_drift_blocks_save(self):
        self.client.collateral_asset = self.plan["protected_assets"][0]
        with self.assertRaisesRegex(RuntimeError, "Protected reflected asset changed"):
            self.stage()
        self.assertEqual([c[1] for c in self.writes()], ["duplicate", "set_properties"])

    def test_save_failure_is_not_retried_or_reported_saved(self):
        self.client.save_ok = False
        report = {"asset_saved": False, "mutation_attempted": False,
                  "fingerprints_before": copy.deepcopy(self.checkpoint)}
        with self.assertRaisesRegex(RuntimeError, "Scoped save failed/uncertain"):
            workflow.stage(self.client, self.plan, self.baseline, self.manifest, self.pins, report)
        self.assertFalse(report["asset_saved"])
        self.assertTrue(report["save_attempted"])
        self.assertEqual([c[1] for c in self.writes()], ["duplicate", "set_properties", "save_assets"])

    def test_tampered_staged_artifact_or_wrong_plan_stops_activation(self):
        staged = self.stage()
        staged["plan"] = workflow.contract.build_plan("Knuckle", "Primary01", "TrialA")
        self.client.calls.clear()
        with self.assertRaisesRegex(RuntimeError, "stage lineage"):
            self.activate(staged)
        self.assertEqual(self.writes(), [])
        staged["plan"] = self.plan
        workflow.native.package_file(self.plan["staged_asset"]).write_bytes(b"changed")
        with self.assertRaisesRegex(RuntimeError, "changed after review"):
            self.activate(staged)
        self.assertEqual(self.writes(), [])

    def test_readback_rejects_changed_files_or_reflected_damage(self):
        applied = self.activate()
        hashes = workflow.fingerprints(self.plan, self.manifest)
        applied.update(protected_files_preserved=True, fingerprints_after=hashes)
        with self.assertRaisesRegex(RuntimeError, "activation/current dependencies"):
            workflow.verify(self.client, self.plan, self.baseline, applied, self.manifest, self.pins, {}, {})
        self.client.props[self.plan["source_asset"]]["damage"] = 999.
        self.client.calls.clear()
        with self.assertRaisesRegex(RuntimeError, "Full reflected values"):
            workflow.verify(self.client, self.plan, self.baseline, applied, self.manifest, self.pins, hashes, {})
        self.assertEqual(self.writes(), [])

    def test_hash_gate_detects_other_codex_changes_without_reverting_them(self):
        before = workflow.fingerprints(self.plan, self.manifest)
        config = self.project / "Config/DefaultGame.ini"
        config.write_text("[other-codex]\nChanged=True\n", encoding="utf-8")
        after = workflow.fingerprints(self.plan, self.manifest)
        with self.assertRaisesRegex(RuntimeError, "Concurrent/protected file change"):
            workflow.assert_hash_transition(before, after)
        self.assertIn("Changed=True", config.read_text(encoding="utf-8"))
        with self.assertRaisesRegex(RuntimeError, "inventory changed"):
            workflow.assert_hash_transition(before, {**before, "Source/New.cpp": "new"})

    def test_file_checkpoint_drift_stops_before_duplicate(self):
        config = self.project / "Config/DefaultGame.ini"
        config.write_text("[other-codex]\nKeepThis=True\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Concurrent/protected file change"):
            self.stage()
        self.assertEqual(self.writes(), [])
        self.assertIn("KeepThis=True", config.read_text(encoding="utf-8"))

    def test_setter_rechecks_exact_latest_state_and_dirty_flag(self):
        source = self.plan["source_asset"]
        before = self.baseline["properties"][source]
        self.client.props[source]["castEffect"] = {"refPath": "/Game/User/NewCast.NewCast"}
        with self.assertRaisesRegex(RuntimeError, "changed.*before setter"):
            workflow.mutate(self.client, source, before, self.baseline["planned_updates"], {})
        self.assertEqual(self.writes(), [])
        self.assertEqual(self.client.props[source]["castEffect"], {"refPath": "/Game/User/NewCast.NewCast"})
        self.client.props[source] = copy.deepcopy(before)
        self.client.dirty.add(source)
        with self.assertRaisesRegex(RuntimeError, "dirty before setter"):
            workflow.mutate(self.client, source, before, self.baseline["planned_updates"], {})
        self.assertEqual(self.writes(), [])

    def test_all_eight_target_paths_stage_activate_and_read_back_individually(self):
        count = 0
        for weapon in workflow.preflight.WEAPONS:
            for variant in workflow.preflight.VARIANTS:
                if (weapon, variant) in workflow.preflight.ACTIVE:
                    continue
                with self.subTest(weapon=weapon, variant=variant):
                    self.plan = workflow.contract.build_plan(weapon, variant, "EachA")
                    self.client.plan = self.plan
                    self.baseline = workflow.inspect(self.client, self.plan, self.manifest, self.pins)
                    self.checkpoint = workflow.fingerprints(self.plan, self.manifest)
                    applied = self.activate()
                    hashes = workflow.fingerprints(self.plan, self.manifest)
                    applied.update(protected_files_preserved=True, fingerprints_after=hashes)
                    result = {}
                    self.client.calls.clear()
                    workflow.verify(self.client, self.plan, self.baseline, applied, self.manifest, self.pins, hashes, result)
                    self.assertTrue(result["saved_clean_readback_verified"])
                    self.assertEqual(self.writes(), [])
                    count += 1
        self.assertEqual(count, 8)

    def test_cold_readback_needs_new_pid_and_old_process_shutdown(self):
        applied = {"listener_identity": {"pid": 17}}
        for pid, old_alive, expected in ((17, False, False), (18, True, False), (18, False, True)):
            with self.subTest(pid=pid, old_alive=old_alive):
                report = {"listener_identity": {"pid": pid}}
                workflow.owner.cold_readback(applied, report, lambda _: old_alive)
                self.assertIs(report["cold_reload_validated"], expected)

    def test_prepare_never_constructs_client_or_probes_editor(self):
        argv = ["bind", "--weapon", "GreatSword", "--variant", "Primary01", "--label", "OfflineA",
                "--evidence-label", "PrepA"]
        with patch.object(sys, "argv", argv), patch.object(workflow.native, "NativeClient", side_effect=AssertionError("no live client")), \
                patch.object(workflow.owner, "listener_identity", side_effect=AssertionError("no process probe")), \
                contextlib.redirect_stdout(io.StringIO()):
            workflow.main()
        report = json.loads((self.project / "Feature/doc/evidence/weapon-skill-vfx-binding-20261111/PrepA/summary.json").read_text(encoding="utf-8"))
        self.assertFalse(report["mcp_dispatched"])
        self.assertFalse(report["mutation_attempted"])
        self.assertFalse(report["editor_or_build_launched"])
        self.assertFalse(report["visual_quality_approved"])

    def test_missing_lease_stops_live_phase_before_any_output_or_probe(self):
        argv = ["bind", "--phase", "inspect", "--weapon", "GreatSword", "--variant", "Primary01",
                "--label", "OfflineA", "--evidence-label", "NoLeaseA"]
        with patch.object(sys, "argv", argv), patch.object(workflow.native, "NativeClient", side_effect=AssertionError("no live client")), \
                patch.object(workflow.owner, "listener_identity", side_effect=AssertionError("no process probe")):
            with self.assertRaisesRegex(RuntimeError, "exclusive editor lease required"):
                workflow.main()
        self.assertFalse((self.project / "Feature/doc/evidence/weapon-skill-vfx-binding-20261111/NoLeaseA").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
