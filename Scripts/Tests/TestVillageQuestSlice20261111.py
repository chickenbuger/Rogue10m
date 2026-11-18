"""Isolated positive/negative placement cases. Never imports or starts Unreal."""
from __future__ import annotations

import argparse
import copy
import datetime
import hashlib
import importlib.util
import io
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "Scripts/PrepareVillageQuestSlice20261111.py"
spec = importlib.util.spec_from_file_location("village_quest_placement", MODULE_PATH)
placement = importlib.util.module_from_spec(spec)
spec.loader.exec_module(placement)
SCENE_BYTES = (ROOT / placement.SCENE_REL).read_bytes()
SCENE = json.loads(SCENE_BYTES.decode("utf-8-sig"))
PROVENANCE = {"health_potion": {"local_exists": True}}


class PlacementTests(unittest.TestCase):
    def setUp(self):
        self.scene = copy.deepcopy(SCENE)

    def actor(self, label):
        return next(a for a in self.scene["actors"] if a["label"] == label)

    def plan(self):
        return placement.build_placement_plan(self.scene, copy.deepcopy(PROVENANCE))

    def test_real_scene_fixed_roles_and_xy(self):
        plan = self.plan()
        self.assertEqual(plan["additive_actor_count"], 2)
        self.assertEqual([a["properties"]["role"] for a in plan["actors"]], ["Board", "Recovery"])
        for actor, settings in zip(plan["actors"], placement.PLACEMENTS):
            original = self.actor(settings[0])
            self.assertEqual(actor["transform"]["location_cm"][:2], original["center_cm"][:2])
            self.assertEqual(actor["transform"]["location_cm"][2], settings[3])
            self.assertIsNone(actor["properties"]["quest_definition"])
            self.assertEqual(actor["properties"]["objective_id"], "V1_MissingSupply")
            self.assertFalse(actor["collision"]["actor_enabled"])
        self.assertEqual(plan["quest_id"], "V1_SupplyRecovery")
        self.assertEqual(plan["existing_actor_updates"], [])
        self.assertEqual(plan["existing_actor_removals"], [])
        self.assertFalse(plan["live_map_geometry_verified"])
        self.assertFalse(plan["unreal_editor_executed"])

    def test_aabb_distance_matches_not_bounding_circle(self):
        board, recovery = self.plan()["actors"]
        self.assertAlmostEqual(board["access"]["nearest_building_distance_cm"], 292.820323, places=5)
        self.assertAlmostEqual(board["access"]["full_circle_margin_cm"], 0.820323, places=5)
        self.assertAlmostEqual(recovery["access"]["full_circle_margin_cm"], 59.754097, places=5)
        self.assertTrue(board["access"]["thin_circle_margin"])

    def test_full_circle_and_inward_approach(self):
        for actor in self.plan()["actors"]:
            access = actor["access"]
            self.assertEqual(access["mode"], "full_circle_clear")
            self.assertLess(access["interaction_distance_3d_cm"], 250)
            for field in ("stand_capsule_margin_cm", "village_to_stand_capsule_margin_cm", "stand_to_actor_capsule_margin_cm", "central_corridor_margin_cm"):
                self.assertGreater(access[field], 0)

    def test_circle_failure_only_uses_verified_inward_approach(self):
        self.actor("GB_V1_BUILDING_02")["center_cm"][0] += 10
        board = self.plan()["actors"][0]
        self.assertEqual(board["access"]["mode"], "inward_only_clear")
        self.assertFalse(board["access"]["full_circle_clear"])
        self.assertLess(board["access"]["full_circle_margin_cm"], 0)
        self.assertEqual(board["transform"]["location_cm"][:2], self.actor("GB_V1_QUEST_RESERVED")["center_cm"][:2])

    def test_wrong_map(self):
        self.scene["map_package"] = "/Game/Rogue10m/Maps/OpenRunMap"
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_wrong_units(self):
        self.scene["units"] = "meters"
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_missing_marker(self):
        self.scene["actors"] = [a for a in self.scene["actors"] if a["label"] != "GB_V1_QUEST_RESERVED"]
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_duplicate_marker(self):
        self.scene["actors"].append(copy.deepcopy(self.actor("GB_V3_DUNGEON_RESERVED")))
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_colliding_marker(self):
        self.actor("GB_V1_QUEST_RESERVED")["collision"] = True
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_collision_string_is_not_boolean(self):
        self.actor("GB_V1_QUEST_RESERVED")["collision"] = "false"
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_wrong_marker_service(self):
        self.actor("GB_V3_DUNGEON_RESERVED")["service"] = "shop"
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_nonfinite_marker(self):
        self.actor("GB_V1_QUEST_RESERVED")["center_cm"][0] = math.nan
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_nonfinite_unrelated_actor_is_rejected(self):
        self.actor("GB_V5_BUILDING_01")["center_cm"][1] = math.inf
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_nonfinite_node(self):
        next(n for n in self.scene["nodes"] if n["id"] == "V1")["xy"][0] = math.inf
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_rotated_building_not_misreported_as_axis_aligned(self):
        self.actor("GB_V1_BUILDING_02")["yaw_degrees"] = 45
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_blocked_inward_point(self):
        point = self.plan()["actors"][0]["access"]["recommended_stand_cm"]
        self.actor("GB_V1_BUILDING_02")["center_cm"][:2] = point[:2]
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_blocked_village_to_stand_path(self):
        stand = self.plan()["actors"][0]["access"]["recommended_stand_cm"]
        building = self.actor("GB_V2_BUILDING_03")
        building["center_cm"][:2] = [(-14000 + stand[0]) / 2, (8000 + stand[1]) / 2]
        building["size_cm"][:2] = [100, 100]
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_actor_inside_a_building_is_rejected(self):
        self.actor("GB_V1_QUEST_RESERVED")["center_cm"][:2] = self.actor("GB_V1_BUILDING_02")["center_cm"][:2]
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_central_corridor_is_preserved(self):
        self.actor("GB_V1_QUEST_RESERVED")["center_cm"][:2] = [0, 8000]
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_existing_new_actor_label_cannot_be_duplicated(self):
        extra = copy.deepcopy(self.actor("GB_V1_QUEST_RESERVED"))
        extra["label"] = placement.PLACEMENTS[0][4]
        self.scene["actors"].append(extra)
        with self.assertRaises(placement.PlacementError): self.plan()

    def test_missing_local_reward_blocks_handoff(self):
        with self.assertRaises(placement.PlacementError):
            placement.build_placement_plan(self.scene, {"health_potion": {"local_exists": False}})

    def test_exact_segment_rectangle_math(self):
        rect = (0, 0, 10, 20)
        self.assertEqual(placement.segment_aabb_distance((-5, 10), (15, 10), rect), 0)
        self.assertEqual(placement.segment_aabb_distance((-5, 0), (15, 0), rect), 0)
        self.assertEqual(placement.segment_aabb_distance((-5, -3), (15, -3), rect), 3)
        self.assertEqual(placement.point_aabb_distance((-3, -4), rect), 5)
        self.assertEqual(placement.segment_aabb_distance((-3, -4), (-3, -4), rect), 5)

    def test_pure_planning_does_not_modify_scene(self):
        before = copy.deepcopy(self.scene)
        self.plan()
        self.assertEqual(self.scene, before)

    def test_prepare_exclusive_and_source_bytes_preserved(self):
        with tempfile.TemporaryDirectory(prefix="Rogue10mQuestPlacement_") as directory:
            root = Path(directory).resolve()
            self.assertTrue(root.is_relative_to(Path(tempfile.gettempdir()).resolve()))
            inputs = {placement.SCENE_REL: SCENE_BYTES, placement.MAP_REL: b"isolated-map-placeholder",
                      placement.POTION_REL: b"isolated-potion-placeholder",
                      placement.ACTOR_SOURCE_REL / "Rogue10mVillageQuestActor.h": b"isolated-header",
                      placement.ACTOR_SOURCE_REL / "Rogue10mVillageQuestActor.cpp": b"isolated-source"}
            for path, content in inputs.items():
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                (root / path).write_bytes(content)
            plan = placement.prepare(project_root=root)
            output = root / placement.EVIDENCE_REL / "placement-plan.json"
            old = output.read_bytes()
            self.assertEqual(plan["provenance"]["current_map_file"]["sha256"], hashlib.sha256(inputs[placement.MAP_REL]).hexdigest())
            with self.assertRaises(FileExistsError): placement.prepare(project_root=root)
            self.assertEqual(output.read_bytes(), old)
            for path, content in inputs.items(): self.assertEqual((root / path).read_bytes(), content)
            forbidden = root / "Source/must-not-write.json"
            with self.assertRaises(placement.PlacementError): placement.prepare(project_root=root, output_path=forbidden)
            self.assertFalse(forbidden.exists())

    def test_no_unreal_import(self):
        self.plan()
        self.assertNotIn("unreal", sys.modules)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, default=ROOT / placement.EVIDENCE_REL / "placement-tests.json")
    args = parser.parse_args()
    report_path = args.report.resolve()
    if not report_path.is_relative_to((ROOT / placement.EVIDENCE_REL).resolve()):
        raise ValueError("Report must remain in the dedicated quest evidence directory.")
    if report_path.exists():
        raise FileExistsError("Existing test evidence is preserved: " + str(report_path))
    protected_paths = [ROOT / placement.SCENE_REL, ROOT / placement.MAP_REL, ROOT / placement.POTION_REL]
    protected_before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected_paths}
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(PlacementTests)
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(suite)
    protected_after = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected_paths}
    preserved = protected_before == protected_after
    passed = result.wasSuccessful() and preserved
    report = {"status": "offline_placement_tests_passed" if passed else "offline_placement_tests_failed",
              "recorded_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "tests": result.testsRun, "failures": len(result.failures), "errors": len(result.errors),
              "protected_files_unchanged": preserved, "protected_sha256_before": protected_before,
              "protected_sha256_after": protected_after, "test_scope": "pure geometry and new temporary fixture files only",
              "source_modified": False, "unreal_editor": "not_run", "unreal_build_tool": "not_run",
              "actual_placement": "not_run", "unreal_imported": "unreal" in sys.modules,
              "details": stream.getvalue()}
    if not passed:
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        report_path = report_path.with_name(report_path.stem + "-failed-" + stamp + ".json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with report_path.open("x", encoding="utf-8", newline="\n") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2, allow_nan=False)
        handle.write("\n")
    print(stream.getvalue())
    print(json.dumps({"status": report["status"], "tests": result.testsRun,
                      "failures": len(result.failures), "errors": len(result.errors), "report": str(report_path)}))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
