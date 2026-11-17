"""Independent regression tests for design arithmetic; no Unreal/project writes."""
import copy
import hashlib
import itertools
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import route_budget as rb


ROOT = Path(__file__).resolve().parent
SCRIPT = ROOT / "route_budget.py"
SOURCE = ROOT / "inputs" / "route-plan.json"


class BudgetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "evidence").mkdir(exist_ok=True)
        cls.raw = json.loads(SOURCE.read_text(encoding="utf-8-sig"))
        cls.plan = rb.Plan(cls.raw)

    def fresh(self):
        return copy.deepcopy(self.raw)

    def reject(self, data, pattern=None):
        with self.assertRaisesRegex(rb.ModelError, pattern or "."):
            rb.Plan(data)

    def run_cli(self, *args):
        return subprocess.run([sys.executable, "-B", str(SCRIPT), "--plan", str(SOURCE), *args], cwd=ROOT,
                              capture_output=True, text=True, encoding="utf-8")

    def test_snapshot_shape_and_unmeasured_provenance(self):
        self.assertEqual(len(self.plan.nodes), 10)
        self.assertEqual(len(self.raw["edges"]), 19)
        self.assertEqual(len(self.plan.villages), 5)
        self.assertIs(self.raw["runtime_measured"], False)
        self.assertEqual(self.raw["timing"]["endpoint"], "boss_entrance_crossing")

    def test_all_90_ordered_node_pairs_are_reachable_and_reversible(self):
        count = 0
        for a, b in itertools.permutations(self.plan.nodes, 2):
            with self.subTest(a=a, b=b):
                forward = self.plan.shortest(a, b)
                backward = self.plan.shortest(b, a)
                self.assertEqual(forward["nodes"][0], a)
                self.assertEqual(forward["nodes"][-1], b)
                self.assertAlmostEqual(forward["distance_m_assumed"], backward["distance_m_assumed"])
                self.assertAlmostEqual(self.plan.path_distance(forward["nodes"]), forward["distance_m_assumed"])
                count += 1
        self.assertEqual(count, 90)

    def test_all_32_village_skip_subsets_retain_direct_boss_access(self):
        count = 0
        for size in range(6):
            for skipped in itertools.combinations(self.plan.villages, size):
                with self.subTest(skipped=skipped):
                    result = self.plan.shortest("S", "B", blocked=skipped)
                    self.assertFalse(set(result["nodes"]) & set(skipped))
                    self.assertAlmostEqual(result["distance_m_assumed"], 300)
                    count += 1
        self.assertEqual(count, 32)

    def test_reference_routes_use_actual_edges_and_match_documented_distances(self):
        expected = {"direct": 300, "revisit": 1332.758, "east": 712.758,
                    "five_visits": 1462.075, "skip_west": 423.441}
        for rid, distance in expected.items():
            with self.subTest(route=rid):
                self.assertAlmostEqual(self.plan.path_distance(self.plan.routes[rid]["nodes"]), distance, delta=.001)
        revisit = self.plan.routes["revisit"]["nodes"]
        self.assertEqual(revisit.count("V1"), 2)
        self.assertNotIn("V2", revisit)
        self.assertNotIn("V5", revisit)
        self.assertTrue(set(self.plan.villages) <= set(self.plan.routes["five_visits"]["nodes"]))

    def test_truncated_direct_route_cannot_pass_with_adjusted_expected_values(self):
        data = self.fresh()
        direct = data["routes"][0]
        direct.update(nodes=["S", "J80"], expected_length_m=80, expected_walk_seconds=80 / 6)
        self.reject(data, "begin S and end B")

    def test_wrong_start_is_rejected(self):
        data = self.fresh()
        data["routes"][0]["nodes"] = ["J80", "J185", "J230", "B"]
        self.reject(data, "begin S and end B")

    def test_illegal_route_leg_and_unknown_node_are_rejected(self):
        for nodes in (["S", "B"], ["S", "missing", "B"]):
            with self.subTest(nodes=nodes):
                data = self.fresh()
                data["routes"][0]["nodes"] = nodes
                self.reject(data, "illegal route leg|unknown node")

    def test_nonfinite_nonpositive_and_boolean_speed_rejected(self):
        for value in (0, -1, math.nan, math.inf, -math.inf, True, "6"):
            with self.subTest(value=value):
                data = self.fresh()
                data["timing"]["walk_speed_mps_assumed"] = value
                self.reject(data, "finite")

    def test_invalid_coordinates_rejected(self):
        for xy in ([math.nan, 0], [0, math.inf], [True, 1], [0], [0, 1, 2], ["0", 0]):
            with self.subTest(xy=xy):
                data = self.fresh()
                data["nodes"][0]["xy"] = xy
                self.reject(data, "coordinate")

    def test_duplicate_coordinate_rejected(self):
        data = self.fresh()
        data["nodes"][1]["xy"] = data["nodes"][0]["xy"][:]
        self.reject(data, "duplicate coordinate")

    def test_negative_or_nonfinite_activity_duration_rejected(self):
        for value in (-1, math.nan, math.inf, True):
            with self.subTest(value=value):
                data = self.fresh()
                data["activity_budgets"][0]["activities"][1]["seconds"] = value
                self.reject(data, "finite")

    def test_duplicate_node_route_budget_and_activity_ids_rejected(self):
        for section in ("nodes", "routes", "activity_budgets"):
            with self.subTest(section=section):
                data = self.fresh()
                data[section].append(copy.deepcopy(data[section][0]))
                self.reject(data, "duplicate")
        data = self.fresh()
        activities = data["activity_budgets"][0]["activities"]
        activities[1]["id"] = activities[0]["id"]
        self.reject(data, "duplicate activity")

    def test_duplicate_edge_and_self_edge_rejected(self):
        for edge in (copy.deepcopy(self.raw["edges"][0]),
                     {"a": "S", "b": "S", "bidirectional": True, "required_quests": [], "transition": "walk_same_world"}):
            with self.subTest(edge=edge):
                data = self.fresh()
                data["edges"].append(edge)
                self.reject(data, "duplicate or self edge")

    def test_quest_gates_one_way_and_level_transition_rejected(self):
        for field, value in (("required_quests", ["quest"]), ("bidirectional", False),
                             ("transition", "open_level")):
            with self.subTest(field=field):
                data = self.fresh()
                data["edges"][0][field] = value
                self.reject(data, "ungated bidirectional same-world")

    def test_runtime_measurement_claim_and_anchored_or_nonmeter_input_rejected(self):
        for field, value in (("runtime_measured", True), ("evidence_kind", "runtime")):
            data = self.fresh()
            data[field] = value
            self.reject(data, "unmeasured")
        for field, value in (("units", "centimeters"), ("world_anchor_applied", True)):
            data = self.fresh()
            data["coordinates"][field] = value
            self.reject(data, "unanchored")

    def test_non_entrance_clock_and_sprint_baseline_rejected(self):
        for field, value in (("endpoint", "boss_death"), ("clock", "wall_time"), ("sprint_used", True)):
            with self.subTest(field=field):
                data = self.fresh()
                data["timing"][field] = value
                self.reject(data, "active_run_time|sprint")

    def test_unmodelled_crossing_and_t_junction_rejected(self):
        for first, second in (([-10, 40], [10, 40]), ([0, 40], [10, 40])):
            with self.subTest(first=first):
                data = self.fresh()
                data["nodes"] += [{"id": "X", "kind": "junction", "xy": first},
                                  {"id": "Y", "kind": "junction", "xy": second}]
                data["edges"].append({"a": "X", "b": "Y", "bidirectional": True,
                                      "required_quests": [], "transition": "walk_same_world"})
                self.reject(data, "crossing/T-junction")

    def test_collinear_overlap_even_with_shared_endpoint_rejected(self):
        data = self.fresh()
        data["edges"].append({"a": "S", "b": "J185", "bidirectional": True,
                              "required_quests": [], "transition": "walk_same_world"})
        self.reject(data, "collinear overlap")
        data = self.fresh()
        data["nodes"] += [{"id": "X", "kind": "junction", "xy": [0, 10]},
                          {"id": "Y", "kind": "junction", "xy": [0, 60]}]
        data["edges"].append({"a": "X", "b": "Y", "bidirectional": True,
                              "required_quests": [], "transition": "walk_same_world"})
        self.reject(data, "collinear overlap")

    def test_travel_component_is_single_and_covers_geometry(self):
        data = self.fresh()
        data["activity_budgets"][0]["activities"][1]["kind"] = "travel"
        self.reject(data, "exactly one travel")
        data = self.fresh()
        activities = data["activity_budgets"][0]["activities"]
        activities[0]["seconds"] -= 40
        activities[1]["seconds"] += 40
        self.reject(data, "travel does not cover")

    def test_route_budget_and_global_target_cannot_silently_diverge(self):
        data = self.fresh()
        data["routes"][1]["entry_budget_seconds"] = 901
        self.reject(data, "entrance target mismatch")
        data = self.fresh()
        data["routes"][0]["entry_budget_seconds"] = 61
        self.reject(data, "direct route target mismatch")

    def test_shared_event_coverage_prevents_different_ids_counting_same_combat(self):
        with self.assertRaisesRegex(rb.ModelError, "double-counted content"):
            rb.activity_total([{"id": "quest", "seconds": 40, "coverage_keys": ["patrol-kill-01"]},
                               {"id": "encounter", "seconds": 40, "coverage_keys": ["patrol-kill-01"]}])
        self.assertEqual(rb.activity_total([{"id": "quest", "seconds": 40},
                                            {"id": "encounter", "seconds": 40}]), 80)

    def test_zero_duration_valid_but_duplicate_coverage_key_invalid(self):
        self.assertEqual(rb.activity_total([{"id": "done", "seconds": 0}]), 0)
        for keys in ([], ["same", "same"]):
            with self.assertRaises(rb.ModelError):
                rb.activity_total([{"id": "event", "seconds": 10, "coverage_keys": keys}])

    def test_two_reference_content_itineraries_arrive_at_900_then_boss1080_and_cap1200(self):
        for rid in ("revisit", "east"):
            with self.subTest(route=rid):
                result = self.plan.itinerary(rid)
                self.assertAlmostEqual(result["entry_active_seconds_assumed"], 900)
                self.assertAlmostEqual(result["boss_completion_seconds_assumed"], 1080)
                self.assertAlmostEqual(result["run_budget_with_reserve_seconds_assumed"], 1200)
                self.assertTrue(result["can_finish_before_timeout"])
                self.assertTrue(result["reserve_preserved"])
                self.assertIs(result["runtime_measured"], False)

    def test_direct_speed_sensitivity_4_5_6_mps(self):
        for speed, expected, meets_target in ((4, 75, False), (5, 60, True), (6, 50, True)):
            with self.subTest(speed=speed):
                result = self.plan.itinerary("direct", speed=speed)
                self.assertAlmostEqual(result["entry_active_seconds_assumed"], expected)
                self.assertEqual(result["at_or_below_entry_target"], meets_target)

    def test_geometry_not_added_twice_and_slack_preserved_when_speed_changes(self):
        baseline = self.plan.itinerary("revisit")
        slower = self.plan.itinerary("revisit", speed=5)
        self.assertAlmostEqual(baseline["travel_seconds_assumed"], 255)
        self.assertAlmostEqual(slower["entry_active_seconds_assumed"] - 900,
                               self.plan.path_distance(self.plan.routes["revisit"]["nodes"]) * (1 / 5 - 1 / 6))
        self.assertAlmostEqual(slower["travel_slack_seconds_assumed"], baseline["travel_slack_seconds_assumed"])

    def test_distance_inflation_preserves_existing_slack_and_increases_only_walk_time(self):
        normal = self.plan.itinerary("revisit")
        inflated = self.plan.itinerary("revisit", distance_factor=1.2)
        self.assertAlmostEqual(inflated["entry_active_seconds_assumed"] - normal["entry_active_seconds_assumed"],
                               normal["geometric_walk_seconds_assumed"] * .2)

    def test_selecting_optional_content_and_extra_dungeon_consumes_reserve(self):
        none = self.plan.itinerary("revisit", selected=[])
        self.assertAlmostEqual(none["entry_active_seconds_assumed"], 255)
        one = self.plan.itinerary("revisit", selected=["Q_V1_supply"])
        self.assertAlmostEqual(one["entry_active_seconds_assumed"], 360)
        extra = self.plan.itinerary("revisit", additional=[{"id": "new-dungeon", "seconds": 180}])
        self.assertAlmostEqual(extra["entry_active_seconds_assumed"], 1080)
        self.assertFalse(extra["reserve_preserved"])
        self.assertFalse(extra["can_finish_before_timeout"])

    def test_unknown_duplicate_selection_and_duplicate_extra_activity_rejected(self):
        for selected in (["missing"], ["Q_V1_supply", "Q_V1_supply"], ["between_villages"]):
            with self.subTest(selected=selected), self.assertRaises(rb.ModelError):
                self.plan.itinerary("revisit", selected=selected)
        with self.assertRaisesRegex(rb.ModelError, "duplicate activity"):
            self.plan.itinerary("revisit", additional=[{"id": "Q_V1_supply", "seconds": 1}])

    def test_original_travel_id_and_shared_coverage_are_preserved_for_duplicates(self):
        with self.assertRaisesRegex(rb.ModelError, "duplicate activity"):
            self.plan.itinerary("revisit", additional=[{"id": "between_villages", "seconds": 255, "kind": "travel"}])
        data = self.fresh()
        data["activity_budgets"][0]["activities"][0]["coverage_keys"] = ["road_walk"]
        covered_plan = rb.Plan(data)
        with self.assertRaisesRegex(rb.ModelError, "double-counted content"):
            covered_plan.itinerary("revisit", additional=[{"id": "extra_walk", "seconds": 1, "coverage_keys": ["road_walk"]}])

    def test_explicit_content_route_requires_budget_and_geometry_only_routes_are_labelled(self):
        data = self.fresh()
        data["activity_budgets"].pop(0)
        self.reject(data, "budget")
        for rid in ("five_visits", "skip_west", "direct"):
            with self.subTest(route=rid):
                self.assertIn("budget_coverage", self.plan.itinerary(rid))

    def test_invalid_scenario_parameters_rejected(self):
        for kwargs in ({"speed": 0}, {"speed": math.nan}, {"distance_factor": .9},
                       {"distance_factor": math.inf}, {"external_pause_seconds": -1}):
            with self.subTest(kwargs=kwargs), self.assertRaises(rb.ModelError):
                self.plan.itinerary("revisit", **kwargs)

    def test_external_pause_is_counterfactual_wall_time_only(self):
        baseline = self.plan.itinerary("revisit")
        paused = self.plan.itinerary("revisit", external_pause_seconds=120)
        self.assertEqual(baseline["entry_active_seconds_assumed"], paused["entry_active_seconds_assumed"])
        self.assertEqual(paused["entry_wall_seconds_if_external_pause_assumed"], 1020)
        self.assertEqual(baseline["reserve_preserved"], paused["reserve_preserved"])

    def test_v4_v1_v4_round_trip_500m_83_333s(self):
        result = self.plan.return_decision(remaining_seconds=600)
        self.assertAlmostEqual(result["outward"]["distance_m_assumed"], 250)
        self.assertAlmostEqual(result["backward"]["distance_m_assumed"], 250)
        self.assertAlmostEqual(result["round_trip_walk_seconds_assumed"], 500 / 6)
        self.assertAlmostEqual(result["onward_walk_seconds_assumed"], math.hypot(60, 70) / 6)
        self.assertTrue(result["recommended_by_assumptions_only"])

    def test_return_formula_objective_report_onward_boss_and_reserve_counted_once(self):
        result = self.plan.return_decision(remaining_seconds=600, objective_seconds=180, report_seconds=30)
        expected = 500 / 6 + 180 + 30 + math.hypot(60, 70) / 6 + 180 + 120
        self.assertAlmostEqual(result["required_with_reserve_seconds_assumed"], expected)
        self.assertEqual(result["reserve_preserved"], expected <= 600)

    def test_return_exact_reserve_boundary_passes_one_second_less_loses_reserve(self):
        required = self.plan.return_decision(remaining_seconds=1000)["required_with_reserve_seconds_assumed"]
        exact = self.plan.return_decision(remaining_seconds=required)
        below = self.plan.return_decision(remaining_seconds=required - 1)
        self.assertTrue(exact["recommended_by_assumptions_only"])
        self.assertTrue(exact["reserve_preserved"])
        self.assertAlmostEqual(exact["reserve_slack_seconds_assumed"], 0)
        self.assertFalse(below["reserve_preserved"])
        self.assertTrue(below["can_finish_before_timeout"])

    def test_return_exact_zero_seconds_is_timeout_even_without_reserve(self):
        complete = self.plan.return_decision(remaining_seconds=1000, reserve_seconds=0)["completion_seconds_from_now_assumed"]
        exact = self.plan.return_decision(remaining_seconds=complete, reserve_seconds=0)
        self.assertTrue(exact["reserve_preserved"])
        self.assertFalse(exact["can_finish_before_timeout"])
        self.assertFalse(exact["recommended_by_assumptions_only"])

    def test_return_invalid_values_and_nonvillage_rejected(self):
        for kwargs in ({"remaining_seconds": -1}, {"remaining_seconds": 0, "objective_seconds": -1},
                       {"remaining_seconds": 600, "village": "B"}, {"remaining_seconds": 600, "speed": 0}):
            with self.subTest(kwargs=kwargs), self.assertRaises(rb.ModelError):
                self.plan.return_decision(**kwargs)

    def test_unsafe_global_completion_at_timeout_and_over_cap_reserve_rejected(self):
        for field, value, pattern in (("boss_combat_budget_seconds", 300, "precede zero-second"),
                                      ("contingency_budget_seconds", 121, "exceed active run cap")):
            data = self.fresh()
            data["timing"][field] = value
            self.reject(data, pattern)

    def test_report_retains_legacy_endpoint_difference_without_measurement_claim(self):
        result = rb.report(self.plan)
        comparison = result["endpoint_comparison_assumed"]
        self.assertEqual(comparison["legacy_architecture_entry_seconds"], 720)
        self.assertEqual(comparison["legacy_architecture_including_boss_seconds"], 900)
        self.assertEqual(comparison["snapshot_json_entry_seconds"], 900)
        self.assertEqual(comparison["snapshot_json_including_boss_seconds"], 1080)
        self.assertEqual(result["ordered_pair_count_reachable"], 90)
        self.assertEqual(len(result["all_village_skip_subsets"]), 32)
        self.assertIs(result["runtime_measured"], False)

    def test_cli_stdout_json_and_input_hash(self):
        process = self.run_cli("scenario", "direct", "--speed", "4")
        self.assertEqual(process.returncode, 0, process.stderr)
        result = json.loads(process.stdout)
        self.assertEqual(result["entry_active_seconds_assumed"], 75)
        self.assertEqual(result["input_sha256"], hashlib.sha256(SOURCE.read_bytes()).hexdigest())

    def test_cli_path_skipping_all_villages_and_zero_optional_content(self):
        process = self.run_cli("path", "--skip", *self.plan.villages)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(json.loads(process.stdout)["distance_m_assumed"], 300)
        process = self.run_cli("scenario", "revisit", "--select")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(json.loads(process.stdout)["entry_active_seconds_assumed"], 255)

    def test_cli_explicit_output_file_and_malformed_input_failure(self):
        with tempfile.TemporaryDirectory(prefix="route-budget-test-", dir=ROOT / "evidence") as directory:
            output = Path(directory) / "report.json"
            process = self.run_cli("--output", str(output), "report")
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["status"], "offline_model_validated")
            malformed = Path(directory) / "bad.json"
            malformed.write_text('{"timing":', encoding="utf-8")
            failed = self.run_cli("--plan", str(malformed), "report")
            self.assertEqual(failed.returncode, 2)
            self.assertIn("Model validation failed", failed.stderr)

    def test_cli_bad_speed_rejected_without_json_result(self):
        for speed in ("0", "-1", "nan", "inf"):
            with self.subTest(speed=speed):
                process = self.run_cli("scenario", "direct", "--speed", speed)
                self.assertEqual(process.returncode, 2)
                self.assertEqual(process.stdout, "")
                self.assertIn("Model validation failed", process.stderr)

    def test_cli_refuses_to_overwrite_input_and_preserves_bytes(self):
        with tempfile.TemporaryDirectory(prefix="route-budget-test-", dir=ROOT / "evidence") as directory:
            plan_copy = Path(directory) / "plan.json"
            plan_copy.write_bytes(SOURCE.read_bytes())
            original = plan_copy.read_bytes()
            process = self.run_cli("--plan", str(plan_copy), "--output", str(plan_copy), "report")
            self.assertEqual(process.returncode, 2)
            self.assertIn("must not overwrite", process.stderr)
            self.assertEqual(plan_copy.read_bytes(), original)

    def test_cli_preserves_existing_output_and_returns_failure(self):
        with tempfile.TemporaryDirectory(prefix="route-budget-test-", dir=ROOT / "evidence") as directory:
            existing = Path(directory) / "existing-report.json"
            original = b'keep this earlier evidence unchanged\n'
            existing.write_bytes(original)
            process = self.run_cli("--output", str(existing), "report")
            self.assertEqual(process.returncode, 2)
            self.assertIn("Model validation failed", process.stderr)
            self.assertEqual(existing.read_bytes(), original)

    def test_cli_malformed_container_is_validation_failure_without_traceback(self):
        with tempfile.TemporaryDirectory(prefix="route-budget-test-", dir=ROOT / "evidence") as directory:
            plan_copy = Path(directory) / "bad.json"
            for malformed in ([], None, {"schema_version": 1}):
                with self.subTest(malformed=malformed):
                    plan_copy.write_text(json.dumps(malformed), encoding="utf-8")
                    process = self.run_cli("--plan", str(plan_copy), "report")
                    self.assertEqual(process.returncode, 2)
                    self.assertNotIn("Traceback", process.stderr)

    def test_cli_report_is_byte_reproducible_for_same_snapshot(self):
        first, second = self.run_cli("report"), self.run_cli("report")
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(first.stdout, second.stdout)


if __name__ == "__main__":
    unittest.main()
