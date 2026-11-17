"""Owner-invoked fourth PIE walk; importing this module performs no work.

After three independent central-lane runs, the owner starts fresh PIE in the
saved crop at S, imports this module, and calls begin(). The only movement
input is ordinary Pawn.AddMovementInput. Public GameState.StartRun is called
as preparation only when the current phase is WaitingToStart; its duration is
never changed. Editor/PIE lifecycle, travel, position, speed, health, collision,
navigation and assets are never changed by this module. Evidence writes only.

Route: S-J100-M-A-G-F-Z-O-BE, on actual catalog edges. In particular M-G and
F-O shortcuts are forbidden. A successful finite-capsule CharacterMovement
walk does not turn the earlier point-ray E19/E27 misses into passing rays.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys
import time
import traceback


ROOT = Path("D:/Project/Rogue10m")
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OUT = ROOT / "Feature/doc/evidence/landscape-resume-20261117/optional-walk.json"
LAYOUT = ROOT / "Docs/References/WorldMap/world-layout-20261112.json"
TERRAIN = ROOT / "Scripts/WorldLayoutTerrain20261112.py"
WALK_HELPERS = ROOT / "Scripts/Editor/PlaytestLandscapeCrop20261116.py"
SEQUENCE = ("S", "J100", "M", "A", "G", "F", "Z", "O", "BE")
EDGE_SEQUENCE = ("E01", "E08", "E15", "E16", "E19", "E20", "E21", "E27")
ARRIVAL_CM = 60.0
ROAD_HALF_WIDTH_CM = 400.0
WALK_SPEED_CM_S = 620.0
WATCHDOG_SECONDS = 300.0
_runner = None


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def utc():
    return datetime.now(timezone.utc).isoformat()


def _sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, "Missing prepared helper: " + str(path))
    result = importlib.util.module_from_spec(spec)
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(result)
    finally:
        sys.dont_write_bytecode = previous
    return result


def _plan():
    """Read pure graph coordinates; historical heights do not determine success."""
    layout = json.loads(LAYOUT.read_text(encoding="utf-8-sig"))
    terrain = _module("landscape_optional_pure_terrain_20261117", TERRAIN)
    nodes = terrain.nodes_world()
    catalog = {row["id"]: row for row in layout["edges"]}
    pure_edges = {row["id"]: row for row in terrain.roadsegments()}
    require(len(catalog) == len(pure_edges) == 29, "Expected the retained 29-road graph")
    edges = []
    for index, edge_id in enumerate(EDGE_SEQUENCE):
        source, target = SEQUENCE[index:index + 2]
        row, pure = catalog[edge_id], pure_edges[edge_id]
        require(row.get("bidirectional") is True and pure.get("bidirectional") is True
                and {row["from"], row["to"]} == {source, target}
                and {pure["from"], pure["to"]} == {source, target},
                "Route contains an absent/nonbidirectional catalog connection: " + edge_id)
        a, b = [float(nodes[source][key]) for key in ("x", "y")], [float(nodes[target][key]) for key in ("x", "y")]
        require(all(math.isfinite(value) for value in a + b) and math.dist(a, b) > 0,
                "Invalid pure waypoint coordinates")
        edges.append({"edge_id": edge_id, "from": source, "to": target,
                      "a_xy_cm": a, "b_xy_cm": b, "length_xy_cm": math.dist(a, b)})
    return edges


def begin():
    """Start one owner-prepared, fresh-S-spawn optional-road observation."""
    global _runner
    require(_runner is None, "Preserve the current/failed observer; no automatic retry")
    require(not OUT.exists(), "Preserve the existing optional walk report")
    helpers = _module("landscape_optional_public_helpers_20261117", WALK_HELPERS)
    u, world, pc = helpers._context({TARGET})
    identity = helpers._selected_profile(u, require_runtime_name=True)
    pawn = u.GameplayStatics.get_player_character(world, 0)
    state = u.GameplayStatics.get_player_state(world, 0)
    gs = u.GameplayStatics.get_game_state(world)
    require(isinstance(pawn, u.Rogue10mCharacter) and isinstance(state, u.Rogue10mPlayerState)
            and isinstance(gs, u.Rogue10mGameState), "Production character, PlayerState and GameState required")
    require(str(state.get_character_display_name()) == helpers.CHARACTER_NAME
            and pawn.is_third_person_gameplay_active(), "Normal QA PlayerState identity and third person required")
    position = pawn.get_actor_location()
    require(abs(position.x - 15000) <= 60 and abs(position.y - 32550) <= 60,
            "Fresh S spawn within 60cm required; no teleport fallback")
    require(abs(gs.get_run_duration_seconds() - 1200.0) < 0.01, "Default 1200-second run required")
    waiting = gs.get_run_phase() == u.Rogue10mRunPhase.WAITING_TO_START
    require(waiting or gs.is_run_active(), "Cannot restart a completed/previous run")
    require(not u.GameplayStatics.is_game_paused(world), "PIE is paused")
    runner = _Runner(u, helpers, world, pc, pawn, state, gs, identity, _plan(), waiting)
    _runner = runner
    try:
        if waiting:
            gs.start_run()
        require(gs.is_run_active(), "Default preparation StartRun did not start")
        runner.handle = u.register_slate_post_tick_callback(runner.tick)
    except Exception:
        runner.finish(traceback.format_exc())
        raise
    return {"started": True, "route": SEQUENCE, "edges": EDGE_SEQUENCE,
            "physical_input": False, "default_StartRun_preparation": waiting,
            "owner_controls_PIE_lifecycle": True}


class _Runner:
    def __init__(self, u, helpers, world, pc, pawn, state, gs, identity, edges, waiting):
        self.u, self.helpers, self.world, self.pc, self.pawn, self.state, self.gs = u, helpers, world, pc, pawn, state, gs
        self.edges = edges
        self.index = 0
        self.handle = None
        self.started_wall = time.monotonic()
        self.walk_wall = self.walk_sim = None
        self.last_sample = 0.0
        self.grounded = self.airborne = 0
        self.max_lateral = self.max_horizontal_speed = self.max_3d_speed = 0.0
        self.segment_max_lateral = [0.0 for _ in edges]
        self.hashes = {"layout": _sha(LAYOUT), "terrain": _sha(TERRAIN), "public_helpers": _sha(WALK_HELPERS)}
        self.report = {"status": "settling_at_S", "started_utc": utc(), "map": TARGET,
                       "PIE_world": world.get_path_name(), "identity": identity,
                       "PlayerState_class": state.get_class().get_path_name(),
                       "character_class": pawn.get_class().get_path_name(),
                       "route": list(SEQUENCE), "edges": edges, "visited": [], "samples": [],
                       "source_sha256": self.hashes, "physical_input": False,
                       "input": "Pawn.AddMovementInput; finite-capsule CharacterMovement",
                       "default_StartRun_as_preparation": waiting, "run_duration_override": False,
                       "teleports": 0, "speed_overrides": 0, "invulnerability": False,
                       "jump_inputs": 0, "arrival_radius_xy_cm": ARRIVAL_CM,
                       "road_half_width_cm": ROAD_HALF_WIDTH_CM,
                       "max_walk_speed_cm_s": WALK_SPEED_CM_S,
                       "horizontal_speed_float_tolerance_cm_s": 0.1,
                       "watchdog_wall_seconds": WATCHDOG_SECONDS,
                       "previous_point_ray_results_changed": False,
                       "limitations": ["One prepared optional-route PIE walk; ordinary Hub portal not tested.",
                           "Only these eight road centerlines are walked; no complete gameplay/combat/NavMesh certification.",
                           "Earlier E19/E27 point-ray misses remain failed point-ray observations.",
                           "Horizontal walking speed is bounded; 3D velocity on slopes is recorded separately."]}
        self.guard()
        OUT.parent.mkdir(parents=True, exist_ok=True)
        with OUT.open("x", encoding="utf-8") as stream:
            json.dump(self.report, stream, ensure_ascii=False, indent=2)
            stream.write("\n")

    @staticmethod
    def xyz(vector):
        return [float(vector.x), float(vector.y), float(vector.z)]

    def guard(self):
        u, world, pc = self.helpers._context({TARGET})
        require(world == self.world and pc == self.pc
                and u.GameplayStatics.get_player_character(world, 0) == self.pawn
                and u.GameplayStatics.get_player_state(world, 0) == self.state,
                "PIE world/controller/character/PlayerState changed")
        require(str(self.state.get_character_display_name()) == self.helpers.CHARACTER_NAME
                and not self.pawn.is_dead() and not self.pawn.is_sprinting() and not self.pawn.is_dodging(),
                "Normal QA identity/alive/no-sprint/no-dodge contract failed")
        movement = self.pawn.get_movement_component()
        require(abs(float(movement.max_walk_speed) - WALK_SPEED_CM_S) < 0.05
                and abs(float(self.pawn.get_character_stat_snapshot().get_editor_property("MoveSpeed")) - WALK_SPEED_CM_S) < 0.05,
                "Default equipment/stat movement speed must remain 620cm/s")
        require(abs(u.GameplayStatics.get_global_time_dilation(world) - 1.0) < 0.001
                and abs(float(self.pawn.get_editor_property("CustomTimeDilation")) - 1.0) < 0.001,
                "Default global and pawn time dilation required")
        require(not u.GameplayStatics.is_game_paused(world), "PIE paused")
        return movement

    def flush(self):
        OUT.write_text(json.dumps(self.report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def finish(self, error=None):
        if self.handle is not None:
            self.u.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        self.report.update(status="passed" if error is None else "failed", passed=error is None,
                           error=error, finished_utc=utc(), elapsed_wall_seconds=time.monotonic() - self.started_wall,
                           grounded_observations=self.grounded, airborne_observations=self.airborne,
                           max_lateral_error_cm=self.max_lateral,
                           max_horizontal_velocity_cm_s=self.max_horizontal_speed,
                           max_3d_velocity_cm_s=self.max_3d_speed)
        self.flush()
        print("LANDSCAPE_OPTIONAL_WALK_20261117 " + self.report["status"])

    def tick(self, _delta):
        try:
            self._tick()
        except Exception:
            self.finish(traceback.format_exc())

    def _tick(self):
        movement = self.guard()
        now = time.monotonic()
        require(now - self.started_wall <= WATCHDOG_SECONDS, "300-second optional-route watchdog expired")
        require(self.gs.is_run_active(), "Production run ended during the walk")
        position = self.pawn.get_actor_location()
        xyz = self.xyz(position)
        velocity = self.xyz(self.pawn.get_velocity())
        require(all(math.isfinite(value) for value in xyz + velocity), "Nonfinite position/velocity")
        horizontal = math.hypot(velocity[0], velocity[1])
        speed3d = math.sqrt(sum(value * value for value in velocity))
        require(horizontal <= WALK_SPEED_CM_S + 0.1, "Observed horizontal velocity exceeds default walking cap")
        self.max_horizontal_speed = max(self.max_horizontal_speed, horizontal)
        self.max_3d_speed = max(self.max_3d_speed, speed3d)
        if self.walk_sim is None:
            require(math.hypot(position.x - 15000, position.y - 32550) <= ARRIVAL_CM,
                    "S spawn moved before measurement")
            if now - self.started_wall < 2.0:
                return
            require(movement.is_moving_on_ground() and speed3d < 5.0, "Stable grounded S spawn required")
            self.walk_sim = self.u.GameplayStatics.get_time_seconds(self.world)
            self.walk_wall = now
            self.report.update(status="walking", walk_started_utc=utc(),
                               remaining_run_seconds_at_start=self.gs.get_remaining_run_seconds())
        grounded = movement.is_moving_on_ground()
        self.grounded += int(grounded)
        self.airborne += int(not grounded)
        require(grounded, "Ground contact lost during finite-capsule walk")
        edge = self.edges[self.index]
        ax, ay = edge["a_xy_cm"]
        bx, by = edge["b_xy_cm"]
        dx, dy = bx - ax, by - ay
        fraction = max(0.0, min(1.0, ((position.x - ax) * dx + (position.y - ay) * dy) / (dx * dx + dy * dy)))
        lateral = math.hypot(position.x - ax - fraction * dx, position.y - ay - fraction * dy)
        self.max_lateral = max(self.max_lateral, lateral)
        self.segment_max_lateral[self.index] = max(self.segment_max_lateral[self.index], lateral)
        require(lateral <= ROAD_HALF_WIDTH_CM, "Walk left the 8m corridor: " + edge["edge_id"])
        elapsed = self.u.GameplayStatics.get_time_seconds(self.world) - self.walk_sim
        if now - self.last_sample >= 0.25:
            self.last_sample = now
            self.report["samples"].append({"sim_seconds": elapsed, "edge_id": edge["edge_id"],
                "location": xyz, "velocity": velocity, "horizontal_speed_cm_s": horizontal,
                "grounded": grounded, "lateral_error_cm": lateral,
                "remaining_run_seconds": self.gs.get_remaining_run_seconds()})
            self.flush()
        if math.hypot(position.x - bx, position.y - by) <= ARRIVAL_CM:
            self.report["visited"].append({"node": edge["to"], "edge_id": edge["edge_id"],
                "sim_seconds": elapsed, "location": xyz,
                "arrival_error_xy_cm": math.hypot(position.x - bx, position.y - by),
                "segment_max_lateral_error_cm": self.segment_max_lateral[self.index]})
            self.index += 1
            if self.index == len(self.edges):
                require([row["edge_id"] for row in self.report["visited"]] == list(EDGE_SEQUENCE),
                        "Not all catalog edges were traversed in order")
                require(self.gs.get_remaining_run_seconds() < self.report["remaining_run_seconds_at_start"] - 1,
                        "Run countdown did not decrease")
                require(self.hashes == {"layout": _sha(LAYOUT), "terrain": _sha(TERRAIN), "public_helpers": _sha(WALK_HELPERS)},
                        "Read-only source inputs changed during observation")
                self.report.update(travel_sim_seconds=elapsed, travel_wall_seconds=now - self.walk_wall,
                                   remaining_run_seconds_at_arrival=self.gs.get_remaining_run_seconds())
                self.pawn.get_movement_component().stop_movement_immediately()
                self.finish()
                return
            edge = self.edges[self.index]
            bx, by = edge["b_xy_cm"]
        dx, dy = bx - position.x, by - position.y
        length = math.hypot(dx, dy)
        require(length > 0, "Zero-length next waypoint input")
        self.pawn.add_movement_input(self.u.Vector(dx / length, dy / length, 0), 1.0, False)
