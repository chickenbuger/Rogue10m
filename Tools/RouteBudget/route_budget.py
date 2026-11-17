"""Offline design arithmetic, never a measurement or Unreal runtime validator."""
from __future__ import annotations

import argparse
import copy
import hashlib
import heapq
import itertools
import json
import math
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = ROOT.parents[1]
DEFAULT_PLAN = PROJECT_ROOT / "Feature" / "doc" / "evidence" / "five-village-route-spec-20261110" / "route-plan.json"
EPS = 1e-8


class ModelError(ValueError):
    pass


def number(value, name, *, positive=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ModelError(f"{name}: expected finite number")
    if not math.isfinite(value) or (value <= 0 if positive else value < 0):
        raise ModelError(f"{name}: expected finite {'positive' if positive else 'nonnegative'} number")
    return float(value)


def identifier(value, name):
    if not isinstance(value, str) or not value.strip():
        raise ModelError(f"{name}: expected nonempty string")
    return value


def unique(items, name):
    if len(items) != len(set(items)):
        raise ModelError(f"{name}: duplicate identifier")


def near(a, b):
    return abs(a - b) <= 0.01


def activity_total(activities):
    """IDs and coverage keys are local to one itinerary, never across alternatives."""
    ids, covered, total = set(), set(), 0.0
    for item in activities:
        activity_id = identifier(item["id"], "activity.id")
        if activity_id in ids:
            raise ModelError(f"duplicate activity: {activity_id}")
        ids.add(activity_id)
        total += number(item["seconds"], f"activity.{activity_id}.seconds")
        keys = item.get("coverage_keys", [activity_id])
        if not isinstance(keys, list) or not keys:
            raise ModelError(f"activity.{activity_id}.coverage_keys: expected nonempty list")
        for key in keys:
            identifier(key, "coverage_key")
            if key in covered:
                raise ModelError(f"double-counted content: {key}")
            covered.add(key)
    return total


def _cross(a, b):
    return a[0] * b[1] - a[1] * b[0]


def _sub(a, b):
    return a[0] - b[0], a[1] - b[1]


class Plan:
    def __init__(self, data):
        self.data = copy.deepcopy(data)
        d = self.data
        if not isinstance(d, dict):
            raise ModelError("plan must be a JSON object")
        if d.get("schema_version") != 1 or isinstance(d.get("schema_version"), bool):
            raise ModelError("unsupported schema_version")
        if d.get("runtime_measured") is not False or d.get("evidence_kind") != "design_static_only":
            raise ModelError("input must be explicitly unmeasured design_static_only")
        coordinates = d["coordinates"]
        if not isinstance(coordinates, dict):
            raise ModelError("coordinates must be an object")
        if coordinates.get("units") != "meters" or coordinates.get("world_anchor_applied") is not False:
            raise ModelError("expected unanchored local coordinates in meters")
        timing = d["timing"]
        if not isinstance(timing, dict):
            raise ModelError("timing must be an object")
        for key in ("nodes", "edges", "routes", "activity_budgets"):
            if not isinstance(d[key], list) or not d[key] or any(not isinstance(item, dict) for item in d[key]):
                raise ModelError(f"{key}: expected nonempty list of objects")
        for key in ("walk_speed_mps_assumed", "direct_entry_target_seconds",
                    "content_entry_target_seconds", "run_cap_seconds"):
            number(timing[key], f"timing.{key}", positive=True)
        for key in ("boss_combat_budget_seconds", "contingency_budget_seconds"):
            number(timing[key], f"timing.{key}")
        if timing.get("endpoint") != "boss_entrance_crossing" or timing.get("clock") != "active_run_time":
            raise ModelError("expected active_run_time to boss_entrance_crossing")
        if timing.get("sprint_used") is not False:
            raise ModelError("baseline must not depend on sprint")
        self.speed = float(timing["walk_speed_mps_assumed"])
        self.nodes, self.adj = {}, {}
        for node in d["nodes"]:
            nid = identifier(node["id"], "node.id")
            if nid in self.nodes:
                raise ModelError(f"duplicate node: {nid}")
            xy = node["xy"]
            if not isinstance(xy, list) or len(xy) != 2:
                raise ModelError(f"node.{nid}.xy: expected two finite coordinates")
            if any(isinstance(v, bool) or not isinstance(v, (float, int)) or not math.isfinite(v) for v in xy):
                raise ModelError(f"node.{nid}.xy: expected finite coordinates")
            if any(tuple(xy) == tuple(n["xy"]) for n in self.nodes.values()):
                raise ModelError(f"node.{nid}: duplicate coordinate without shared junction")
            self.nodes[nid] = node
            self.adj[nid] = {}
        self._known("S"); self._known("B")
        if self.nodes["S"].get("kind") != "entry" or self.nodes["B"].get("kind") != "boss_entrance":
            raise ModelError("S/B must be entry/boss_entrance")
        self.villages = sorted(n for n in self.nodes if self.nodes[n].get("kind") == "village")
        if len(self.villages) != 5:
            raise ModelError("expected five villages")
        edges = d["edges"]
        for edge in edges:
            a, b = edge["a"], edge["b"]
            self._known(a); self._known(b)
            if a == b or b in self.adj[a]:
                raise ModelError(f"duplicate or self edge: {a}-{b}")
            if edge.get("bidirectional") is not True or edge.get("required_quests") != [] or edge.get("transition") != "walk_same_world":
                raise ModelError(f"edge.{a}-{b}: expected ungated bidirectional same-world walk")
            distance = math.dist(self.nodes[a]["xy"], self.nodes[b]["xy"])
            self.adj[a][b] = self.adj[b][a] = distance
        self._validate_intersections(edges)
        for nid in self.nodes:
            self.shortest("S", nid)
        self.shortest("S", "B", blocked=self.villages)
        self.routes = {}
        for route in d["routes"]:
            rid = identifier(route["id"], "route.id")
            if rid in self.routes:
                raise ModelError(f"duplicate route: {rid}")
            path = route["nodes"]
            if not isinstance(path, list) or len(path) < 2 or path[0] != "S" or path[-1] != "B":
                raise ModelError(f"route.{rid}: must begin S and end B")
            distance = self.path_distance(path)
            expected_distance = number(route["expected_length_m"], "expected_length_m", positive=True)
            expected_time = number(route["expected_walk_seconds"], "expected_walk_seconds", positive=True)
            if not near(distance, expected_distance) or not near(distance / self.speed, expected_time):
                raise ModelError(f"route.{rid}: expected geometry/time mismatch")
            if route.get("entry_budget_seconds") is not None:
                if number(route["entry_budget_seconds"], "entry_budget_seconds", positive=True) + EPS < distance / self.speed:
                    raise ModelError(f"route.{rid}: entry budget below geometric lower bound")
            self.routes[rid] = route
        if "direct" not in self.routes:
            raise ModelError("missing direct route")
        direct = self.routes["direct"]
        if set(direct["nodes"]) & set(self.villages):
            raise ModelError("direct route visits village")
        if not near(direct["entry_budget_seconds"], timing["direct_entry_target_seconds"]):
            raise ModelError("direct route target mismatch")
        if self.path_distance(direct["nodes"]) / self.speed > timing["direct_entry_target_seconds"] + EPS:
            raise ModelError("direct geometry exceeds design target")
        self.budgets = {}
        for budget in d["activity_budgets"]:
            rid = budget["route"]
            if rid not in self.routes or rid in self.budgets:
                raise ModelError(f"missing/duplicate budget route: {rid}")
            amount = number(budget["seconds"], "budget.seconds", positive=True)
            if not isinstance(budget["activities"], list) or not budget["activities"] or any(not isinstance(a, dict) for a in budget["activities"]):
                raise ModelError(f"budget.{rid}: expected nonempty list of activity objects")
            for activity in budget["activities"]:
                identifier(activity.get("kind"), "activity.kind")
            if not near(activity_total(budget["activities"]), amount):
                raise ModelError(f"budget.{rid}: activities do not sum to seconds")
            if not near(amount, timing["content_entry_target_seconds"]) or not near(amount, self.routes[rid]["entry_budget_seconds"]):
                raise ModelError(f"budget.{rid}: boss entrance target mismatch")
            travel = [a for a in budget["activities"] if a.get("kind") == "travel"]
            if len(travel) != 1:
                raise ModelError(f"budget.{rid}: expected exactly one travel component")
            if travel[0]["seconds"] + EPS < self.path_distance(self.routes[rid]["nodes"]) / self.speed:
                raise ModelError(f"budget.{rid}: travel does not cover geometry")
            self.budgets[rid] = budget
        for rid, route in self.routes.items():
            if rid != "direct" and route.get("entry_budget_seconds") is not None and rid not in self.budgets:
                raise ModelError(f"route.{rid}: missing declared content entry budget")
        if (timing["content_entry_target_seconds"] + timing["boss_combat_budget_seconds"] >= timing["run_cap_seconds"]):
            raise ModelError("assumed boss completion must precede zero-second timeout")
        if (timing["content_entry_target_seconds"] + timing["boss_combat_budget_seconds"] + timing["contingency_budget_seconds"] > timing["run_cap_seconds"] + EPS):
            raise ModelError("entry + boss + reserve exceed active run cap")

    def _known(self, node):
        if node not in self.nodes:
            raise ModelError(f"unknown node: {node}")

    def _validate_intersections(self, edges):
        for first, second in itertools.combinations(edges, 2):
            p, p2 = (self.nodes[first[k]]["xy"] for k in ("a", "b"))
            q, q2 = (self.nodes[second[k]]["xy"] for k in ("a", "b"))
            r, s, delta = _sub(p2, p), _sub(q2, q), _sub(q, p)
            den = _cross(r, s)
            shared = {first["a"], first["b"]} & {second["a"], second["b"]}
            if abs(den) < EPS:
                if abs(_cross(delta, r)) >= EPS:
                    continue
                axis = 0 if abs(r[0]) >= abs(r[1]) else 1
                lo = max(min(p[axis], p2[axis]), min(q[axis], q2[axis]))
                hi = min(max(p[axis], p2[axis]), max(q[axis], q2[axis]))
                if hi - lo > EPS or (hi >= lo - EPS and not shared):
                    raise ModelError("collinear overlap or unshared touching junction")
            else:
                t, u = _cross(delta, s) / den, _cross(delta, r) / den
                if -EPS <= t <= 1 + EPS and -EPS <= u <= 1 + EPS and not shared:
                    raise ModelError("unmodelled geometric crossing/T-junction")

    def path_distance(self, path):
        if not path:
            raise ModelError("empty path")
        for node in path:
            self._known(node)
        distance = 0.0
        for a, b in zip(path, path[1:]):
            if b not in self.adj[a]:
                raise ModelError(f"illegal route leg: {a}->{b}")
            distance += self.adj[a][b]
        return distance

    def shortest(self, start, end, *, blocked=()):
        self._known(start); self._known(end)
        blocked = set(blocked)
        for node in blocked:
            self._known(node)
        if start in blocked or end in blocked:
            raise ModelError("start/end blocked")
        queue, best = [(0.0, (start,))], {start: 0.0}
        while queue:
            distance, path = heapq.heappop(queue)
            current = path[-1]
            if distance > best[current] + EPS:
                continue
            if current == end:
                return {"nodes": list(path), "distance_m_assumed": distance}
            for nxt, leg in sorted(self.adj[current].items()):
                candidate = distance + leg
                if nxt not in blocked and candidate < best.get(nxt, math.inf) - EPS:
                    best[nxt] = candidate
                    heapq.heappush(queue, (candidate, path + (nxt,)))
        raise ModelError(f"no route: {start}->{end} with blocked nodes")

    def itinerary(self, route_id, *, speed=None, distance_factor=1.0,
                  selected=None, additional=(), external_pause_seconds=0):
        if route_id not in self.routes:
            raise ModelError(f"unknown route: {route_id}")
        speed = number(self.speed if speed is None else speed, "speed", positive=True)
        factor = number(distance_factor, "distance_factor", positive=True)
        if factor < 1:
            raise ModelError("distance_factor cannot shorten the geometric lower bound")
        pause = number(external_pause_seconds, "external_pause_seconds")
        additional = list(additional)
        route = self.routes[route_id]
        distance = self.path_distance(route["nodes"])
        baseline = distance / self.speed
        budget = self.budgets.get(route_id)
        optional = [a for a in budget["activities"] if a["kind"] != "travel"] if budget else []
        if selected is not None:
            selected = list(selected)
            unique(selected, "selected activities")
            unknown = set(selected) - {a["id"] for a in optional}
            if unknown:
                raise ModelError(f"unknown optional activities: {sorted(unknown)}")
            optional = [a for a in optional if a["id"] in selected]
        original_travel = next(a for a in budget["activities"] if a["kind"] == "travel") if budget else {"id": "__route_travel__"}
        slack = original_travel["seconds"] - baseline if budget else 0.0
        travel = distance * factor / speed + slack
        travel_component = {**original_travel, "seconds": travel}
        activities = [travel_component] + optional + additional
        active = activity_total(activities)
        timing = self.data["timing"]
        boss = timing["boss_combat_budget_seconds"]
        reserve = timing["contingency_budget_seconds"]
        cap = timing["run_cap_seconds"]
        target = timing["direct_entry_target_seconds"] if route_id == "direct" else timing["content_entry_target_seconds"]
        return {
            "route": route_id, "nodes": route["nodes"], "runtime_measured": False,
            "budget_coverage": "travel_and_selected_activity_assumptions" if budget else "geometry_only_lower_bound_no_content_budget",
            "distance_m_assumed": distance * factor, "speed_mps_assumed": speed,
            "distance_factor_assumed": factor, "geometric_walk_seconds_assumed": distance * factor / speed,
            "travel_slack_seconds_assumed": slack, "travel_seconds_assumed": travel,
            "activity_ids": [a["id"] for a in optional + additional],
            "entry_active_seconds_assumed": active,
            "entry_wall_seconds_if_external_pause_assumed": active + pause,
            "entry_target_seconds": target, "entry_target_delta_seconds": active - target,
            "at_or_below_entry_target": active <= target + EPS,
            "boss_completion_seconds_assumed": active + boss,
            "run_budget_with_reserve_seconds_assumed": active + boss + reserve,
            "can_finish_before_timeout": active + boss < cap - EPS,
            "reserve_preserved": active + boss + reserve <= cap + EPS,
            "reserve_slack_seconds_assumed": cap - active - boss - reserve,
        }

    def return_decision(self, *, current="V4", village="V1", remaining_seconds,
                        objective_seconds=0, report_seconds=30, speed=None,
                        distance_factor=1.0, boss_seconds=None, reserve_seconds=None):
        timing = self.data["timing"]
        remaining = number(remaining_seconds, "remaining_seconds")
        objective = number(objective_seconds, "objective_seconds")
        report = number(report_seconds, "report_seconds")
        speed = number(self.speed if speed is None else speed, "speed", positive=True)
        factor = number(distance_factor, "distance_factor", positive=True)
        if factor < 1:
            raise ModelError("distance_factor cannot shorten the geometric lower bound")
        boss = number(timing["boss_combat_budget_seconds"] if boss_seconds is None else boss_seconds, "boss_seconds")
        reserve = number(timing["contingency_budget_seconds"] if reserve_seconds is None else reserve_seconds, "reserve_seconds")
        if village not in self.villages:
            raise ModelError("return destination must be a village")
        outward = self.shortest(current, village)
        backward = self.shortest(village, current)
        onward = self.shortest(current, "B")
        round_trip = (outward["distance_m_assumed"] + backward["distance_m_assumed"]) * factor / speed
        onward_time = onward["distance_m_assumed"] * factor / speed
        complete = round_trip + objective + report + onward_time + boss
        required = complete + reserve
        return {"runtime_measured": False, "outward": outward, "backward": backward,
                "onward": onward, "remaining_active_seconds": remaining,
                "round_trip_walk_seconds_assumed": round_trip,
                "objective_seconds_assumed": objective, "report_seconds_assumed": report,
                "onward_walk_seconds_assumed": onward_time, "boss_seconds_assumed": boss,
                "reserve_seconds_assumed": reserve, "completion_seconds_from_now_assumed": complete,
                "required_with_reserve_seconds_assumed": required,
                "reserve_slack_seconds_assumed": remaining - required,
                "can_finish_before_timeout": complete < remaining - EPS,
                "reserve_preserved": required <= remaining + EPS,
                "recommended_by_assumptions_only": complete < remaining - EPS and required <= remaining + EPS}


def load_plan(path):
    try:
        raw = Path(path).read_bytes()
        plan = Plan(json.loads(raw.decode("utf-8-sig")))
        plan.input_sha256 = hashlib.sha256(raw).hexdigest()
        return plan
    except (KeyError, TypeError, IndexError) as exc:
        raise ModelError(f"malformed plan: {exc}") from exc


def report(plan):
    timing = plan.data["timing"]
    skip_cases = []
    for count in range(6):
        for subset in itertools.combinations(plan.villages, count):
            result = plan.shortest("S", "B", blocked=subset)
            skip_cases.append({"skipped_villages": list(subset), **result})
    revisit = plan.itinerary("revisit")
    boundary = plan.return_decision(remaining_seconds=1200)
    required = boundary["required_with_reserve_seconds_assumed"]
    return {
        "evidence_kind": "design_static_only", "runtime_measured": False,
        "status": "offline_model_validated", "schema_version": 1,
        "node_count": len(plan.nodes), "edge_count": len(plan.data["edges"]),
        "routes": [plan.itinerary(rid) for rid in sorted(plan.routes)],
        "all_village_skip_subsets": skip_cases,
        "ordered_pair_count_reachable": sum(1 for a in plan.nodes for b in plan.nodes if a != b and plan.shortest(a, b)),
        "direct_speed_sensitivity_assumed": [plan.itinerary("direct", speed=s) for s in (4, 5, 6, 7.2, 9)],
        "revisit_distance_sensitivity_assumed": [plan.itinerary("revisit", distance_factor=f) for f in (1, 1.1, 1.2)],
        "optional_content_comparison_assumed": {
            "no_activities": plan.itinerary("revisit", selected=[]),
            "all_reference_activities": revisit,
            "one_extra_dungeon_180s": plan.itinerary("revisit", additional=[{"id": "extra_dungeon_assumption", "seconds": 180}]),
            "external_pause_120s_counterfactual": plan.itinerary("revisit", external_pause_seconds=120)},
        "return_to_village_cases_assumed": [
            plan.return_decision(remaining_seconds=600),
            plan.return_decision(remaining_seconds=required),
            plan.return_decision(remaining_seconds=required - 1),
            plan.return_decision(remaining_seconds=600, objective_seconds=180)],
        "endpoint_comparison_assumed": {
            "legacy_architecture_entry_seconds": 255 + 390 + 30 + 45,
            "legacy_architecture_including_boss_seconds": 255 + 390 + 30 + 45 + 180,
            "snapshot_json_entry_seconds": revisit["entry_active_seconds_assumed"],
            "snapshot_json_including_boss_seconds": revisit["boss_completion_seconds_assumed"],
            "entrance_target_seconds": timing["content_entry_target_seconds"]},
        "limitations": [
            "Every distance, speed and duration is a design assumption, not measured gameplay.",
            "2D straight edges omit terrain, collision, acceleration, hazards and streaming delays.",
            "A named route can revisit nodes; graph reachability does not prove quest persistence.",
            "Original activity IDs have no shared-event coverage keys; semantic overlap is unverified.",
            "Activity location/order and quests/rewards are not simulated; optional selection is arithmetic only.",
            "External pause is a counterfactual clock input, not implemented village pause behavior.",
            "No editor, build, PIE, physical input or full-run test was executed.",
        ]}


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--plan", type=Path, default=DEFAULT_PLAN)
    result.add_argument("--output", type=Path, help="Explicit JSON output; otherwise stdout only")
    commands = result.add_subparsers(dest="command")
    commands.add_parser("report")
    path = commands.add_parser("path")
    path.add_argument("--start", default="S"); path.add_argument("--end", default="B")
    path.add_argument("--skip", nargs="*", default=[])
    scenario = commands.add_parser("scenario")
    scenario.add_argument("route"); scenario.add_argument("--speed", type=float)
    scenario.add_argument("--distance-factor", type=float, default=1)
    scenario.add_argument("--select", nargs="*", help="Omit for all; empty list means no optional activities")
    scenario.add_argument("--extra-seconds", type=float, default=0)
    scenario.add_argument("--external-pause-seconds", type=float, default=0)
    back = commands.add_parser("return-budget")
    back.add_argument("--current", default="V4"); back.add_argument("--village", default="V1")
    back.add_argument("--remaining-seconds", type=float, required=True)
    back.add_argument("--objective-seconds", type=float, default=0)
    back.add_argument("--report-seconds", type=float, default=30)
    back.add_argument("--speed", type=float); back.add_argument("--distance-factor", type=float, default=1)
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    try:
        if args.output and args.output.resolve() == args.plan.resolve():
            raise ModelError("output must not overwrite the input plan")
        plan = load_plan(args.plan)
        if args.command == "path":
            result = {"evidence_kind": "design_static_only", "runtime_measured": False,
                      **plan.shortest(args.start, args.end, blocked=args.skip)}
        elif args.command == "scenario":
            extra = number(args.extra_seconds, "extra_seconds")
            result = plan.itinerary(args.route, speed=args.speed, distance_factor=args.distance_factor,
                                    selected=args.select, external_pause_seconds=args.external_pause_seconds,
                                    additional=[{"id": "cli_extra_assumption", "seconds": extra}] if extra else [])
        elif args.command == "return-budget":
            result = plan.return_decision(current=args.current, village=args.village,
                    remaining_seconds=args.remaining_seconds, objective_seconds=args.objective_seconds,
                    report_seconds=args.report_seconds, speed=args.speed, distance_factor=args.distance_factor)
        else:
            result = report(plan)
        result["input_sha256"] = plan.input_sha256
        text = json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
        if args.output:
            with args.output.open("x", encoding="utf-8", newline="\n") as output:
                output.write(text)
            print(f"Wrote {args.output}; design assumptions only, runtime not measured.")
        else:
            print(text, end="")
        return 0
    except (ModelError, OSError, ValueError) as exc:
        print(f"Model validation failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
