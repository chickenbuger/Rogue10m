"""Owner-started PIE walk through both caves using the existing game camera.

Import is inert. Arm before a fresh Play-In-Viewport session. Only normal public
profile preparation, StartRun, AddMovementInput and bounded control-rotation aim
are used. No transform, speed, health, camera-component or time setters occur.
Six actual game-viewport PNGs are captured by AutomationLibrary with Camera=None
and the engine's high-res bShowUI=false path; owner reviews each before resume.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import struct
import sys
import time
import traceback


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Feature/doc/evidence/caves20261118"
OUT = BASE / "runtime-camera-final"
PLAN = BASE / "plan.json"
PLAN_SHA256 = "39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74"
V1_GRID_SHA256 = "ef9d5119c138e680001442fac76499e2e0ff40e9f51e428a33bd834aa3b4ffc1"
V1_NATIVE_BLAKE3 = "b9f28e82d7661e83555d5f72942ea999ca6aea9554969753a889ea50f87e1970"
ORIGINAL_LAYER = "7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955"
COLLAR_GRID_SHA256 = "a2c8846963f7d08f7df561b6c800f2f389255e6f52c14a1453a6e5e3f7674726"
COLLAR_NATIVE_BLAKE3 = "67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc"
HEIGHT_PINS = {
    "target-height-plan.json": "96abbc526da80ba3637341261a57892ade0078b8fc08df55fd3bbe3a14b3a898",
    "author-live/apply-height-confirmed.json": "c4d5b5024ff1389c2bc8879730f546f639111445c9bd3304163176a7f0b0c93f",
    "collar-spec.json": "4f3d5dacfdee8af8e00e32c6db3597208061e2593e488f74cdd88de334a3696a",
    "collar-preflight.json": "2fd5f9dd3c8d57eb723d215ac724724b54234e8b23389971fe4e91f67a8e84ca",
    "target-height-plan-collar.json": "cd7928198b0f9c3cfc0d2ad5944a49cca4debc3d1b3ca69b63c946b35873752c",
}
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
ROUTE_KEYS = ("main_forward_ground_cm", "main_reverse_ground_cm", "secret_from_surface_ground_cm",
              "secret_to_surface_ground_cm", "shortcut_reverse_ground_cm", "shortcut_return_ground_cm")
ARRIVAL_CM = 35.0
INPUT_POLICY = {
    "schema": "Rogue10m.CaveQAAnalogApproach.20261118.v1",
    "predecessor_observer_sha256": "689e5aaa10194639b271523a329f46fa7007ef3f607151be1904ebc38d31f70e",
    "formula": "scale=min(1,distance_cm/max(120,620*raw_sim_frame_seconds*1.1))",
    "reason": "Use normal analog input to approach the same waypoint at low frame rates without a stale120cm stopping window.",
    "normal_walk_cap_cm_s": 620, "arrival_radius_cm": ARRIVAL_CM,
    "route_camera_ground_thresholds_changed": False,
    "max_walk_speed_velocity_time_or_camera_setters": False,
    "independent_known_geometry_repair_and_original_failure_ray_acceptance_required": True,
}
_runner = None


def require(value, message):
    if not value:
        raise RuntimeError(message)


def utc():
    return datetime.now(timezone.utc).isoformat()


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec)
    old = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(value)
    finally:
        sys.dont_write_bytecode = old
    return value


def write_new(name, value):
    with (OUT / (name + ".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")


def normalize_angle(angle):
    return (angle + 180.0) % 360.0 - 180.0


def capture_pose_delta(before, after):
    """Compare actual PCM samples, not the controller's pending rotation."""
    return {"camera_translation_cm": math.dist(before["camera_cm"], after["camera_cm"]),
            "pawn_translation_cm": math.dist(before["pawn_cm"], after["pawn_cm"]),
            "rotation_max_degrees": max(abs(normalize_angle(a - b)) for a, b in
                zip(before["camera_rotation_pitch_yaw_roll"], after["camera_rotation_pitch_yaw_roll"])),
            "FOV_delta_degrees": abs(before["camera_FOV"] - after["camera_FOV"])}


def stable_capture_pose(delta):
    return (delta["camera_translation_cm"] <= 2.0 and delta["pawn_translation_cm"] <= 2.0
            and delta["rotation_max_degrees"] <= 1.0 and delta["FOV_delta_degrees"] <= .01)


def rotation_axes(row):
    """UE FRotationMatrix local axes expressed in world coordinates."""
    pitch, yaw, roll = map(math.radians, row["rotation_pitch_yaw_roll_deg"])
    sp, cp, sy, cy, sr, cr = math.sin(pitch), math.cos(pitch), math.sin(yaw), math.cos(yaw), math.sin(roll), math.cos(roll)
    return ((cp * cy, cp * sy, sp),
            (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp),
            (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp))


def boxes_from_plan(plan):
    result = []
    for site in plan["sites"]:
        for row in site["components"]:
            axes = rotation_axes(row)
            half = [n / 2 for n in row["dimensions_cm"]]
            extent = [sum(abs(axes[a][w]) * half[a] for a in range(3)) for w in range(3)]
            result.append({"site": site["site_id"], "label": row["label"], "center": row["location_cm"],
                           "axes": axes, "half": half, "extent": extent})
    return result


def camera_embeddings(point, boxes, radius=14.0):
    hits = []
    for box in boxes:
        delta = [point[i] - box["center"][i] for i in range(3)]
        if any(abs(delta[i]) > box["extent"][i] + radius for i in range(3)):
            continue
        local = [sum(delta[i] * axis[i] for i in range(3)) for axis in box["axes"]]
        distance2 = sum(max(abs(local[i]) - box["half"][i], 0.0) ** 2 for i in range(3))
        if distance2 < (radius - 0.5) ** 2:
            hits.append({"site": box["site"], "label": box["label"], "surface_distance_cm": math.sqrt(distance2)})
    return hits


def route_plan(plan):
    """Keep every plan chain in both directions; field transfers use real roads."""
    sites = {site["site_id"]: site for site in plan["sites"]}
    nodes = {"S": [15000, 32550, 9600], "J100": [15000, 42550, 9600],
             "J180": [15000, 50550, 9600], "J240": [15000, 56550, 9600],
             "Q": [-13000, 54550, 9600], "T": [22000, 55550, 9600],
             "O": [36000, 57550, 9600], "Z": [52000, 60050, 9600]}
    result = []

    def add(label, points, site=None, chain=None):
        for index, point in enumerate(points):
            result.append({"label": label + ":" + str(index), "ground_cm": list(point), "site": site,
                           "chain": chain, "point_index": index, "capture": None})

    add("field_to_Q", [nodes[n] for n in ("S", "J100", "J180", "J240", "Q")])
    for site_id in ("Q", "Z"):
        site = sites[site_id]
        routes = site["runtime_routes"]
        for key in ROUTE_KEYS:
            require(key in routes and len(routes[key]) >= 2, "All six bidirectional cave chains required")
            offset = len(result)
            add(site_id + ":" + key, routes[key], site_id, key)
            if key == "main_forward_ground_cm":
                # Point5 is the first floor-cell center: the camera is under
                # the roof after crossing its 500cm entrance border.
                result[offset + 5]["capture"] = "entrance_under_roof"
                bends = []
                points = routes[key]
                for index in range(6, len(points) - 1):
                    a = [points[index][j] - points[index - 1][j] for j in (0, 1)]
                    b = [points[index + 1][j] - points[index][j] for j in (0, 1)]
                    if abs(sum(a[j] * b[j] for j in (0, 1))) < 1.0:
                        bends.append(index)
                require(bends, "Main path must exercise a right-angle corner")
                result[offset + bends[0]]["capture"] = "narrow_right_angle_turn"
            if key == "secret_from_surface_ground_cm":
                result[-1]["capture"] = "secret_dogleg_room"
        if site_id == "Q":
            # Q->J240 is E14, J240->T E18, T->O E22, O->Z E21.
            add("field_Q_to_Z", [nodes[n] for n in ("Q", "J240", "T", "O", "Z")])
    require(sum(row["capture"] is not None for row in result) == 6, "Exactly six representative captures required")
    length = sum(math.dist(a["ground_cm"], b["ground_cm"]) for a, b in zip(result, result[1:]))
    return result, length


def reviewed_height_contract(plan):
    """Frozen v1 ->150cm collar chain; a fresh arbitrary hash is insufficient."""
    values = {}
    for relative, expected in HEIGHT_PINS.items():
        raw = (BASE / relative).read_bytes()
        require(hashlib.sha256(raw).hexdigest() == expected, "Immutable height predecessor/recipe changed: " + relative)
        values[relative] = json.loads(raw)
    v1, target = values["target-height-plan.json"], values["target-height-plan-collar.json"]
    spec, proof = values["collar-spec.json"], values["collar-preflight.json"]
    old_apply = values["author-live/apply-height-confirmed.json"]
    old, new = v1["target_heights"], target["target_heights"]
    require(len(old) == len(new) == 179488
            and hashlib.sha256(struct.pack("<" + "H" * len(old), *old)).hexdigest() == V1_GRID_SHA256
            and hashlib.sha256(struct.pack("<" + "H" * len(new), *new)).hexdigest() == COLLAR_GRID_SHA256,
            "Exact predecessor and latest full179488 grids required")
    require(spec["schema"] == "Rogue10m.CaveFloorCollarSpec.20261118.v1" and spec["map"] == TARGET
            and spec["structural_plan_sha256"] == PLAN_SHA256 and spec["floor_collar_cm"] == spec["remaining_blend_cm"] == 150
            and spec["old_outer_blend_envelope_cm"] == 300 and spec["geometry_actor_material_camera_changes"] == 0
            and spec["probe_cases_and_expectations_changed"] is False
            and spec["source_v1_grid_uint16le_sha256"] == V1_GRID_SHA256
            and spec["source_v1_native_blake3"] == V1_NATIVE_BLAKE3
            and spec["original_layer_blake3"] == ORIGINAL_LAYER and spec["target_uint16le_sha256"] == COLLAR_GRID_SHA256,
            "Exact150cm collar without route/camera/probe changes required")
    require(proof["schema"] == "Rogue10m.CaveFloorCollarPreflight.20261118.v1" and proof["passed"] is True
            and proof["recipe_sha256"] == HEIGHT_PINS["collar-spec.json"]
            and proof["target_height_plan_file_sha256"] == HEIGHT_PINS["target-height-plan-collar.json"]
            and proof["target_uint16le_sha256"] == COLLAR_GRID_SHA256
            and proof["source_v1_native_blake3"] == V1_NATIVE_BLAKE3
            and all(proof[key] is True for key in ("all_29_road_sample_heights_preserved",
                "all_three_village_circles_and_grid_collar_preserved", "all_new_changes_inside_old_pit300cm_envelope",
                "all_new_changes_lower_only", "all_original_pit_interior_and_ramp_heights_unchanged",
                "all_wall_midface_quads_flat8600", "outside_changed_index_set_exactly_preserved")),
            "Frozen collar protection preflight required")
    changed = [i for i, pair in enumerate(zip(old, new)) if pair[0] != pair[1]]
    require(len(changed) == spec["target_changed_from_v1_samples"] == proof["target_changed_from_v1_samples"] == 447
            and all(new[i] <= old[i] for i in changed), "Only447 reviewed lowering changes permitted")
    for i in changed:
        x, y = -90000 + (315 + i % 568) * (1250 / 7), -90000 + (630 + i // 568) * (1250 / 7)
        require(any(math.hypot(max(s["pit_rect_cm"][0] - x, 0, x - s["pit_rect_cm"][2]),
                               max(s["pit_rect_cm"][1] - y, 0, y - s["pit_rect_cm"][3])) <= 300.00001
                    for s in plan["sites"]), "Collar escaped the previously approved300cm pit envelope")
    receipt_path = BASE / "author-live/apply-height-collar.json"
    raw = receipt_path.read_bytes()
    applied = json.loads(raw)
    require(old_apply.get("success") is True and old_apply["target_composite_blake3"] == V1_NATIVE_BLAKE3
            and old_apply["expected_original_layer_blake3"] == ORIGINAL_LAYER
            and applied.get("success") is True and applied.get("original_layer_preserved") is True
            and applied.get("existing_layer_revised") is True and applied.get("layers_created") == 0
            and applied["previous_composite_blake3"] == V1_NATIVE_BLAKE3
            and applied["expected_original_layer_blake3"] == ORIGINAL_LAYER
            and applied["new_layer_guid"] == spec["new_layer_guid"]
            and applied["changed_samples"] == 447 and applied["target_composite_blake3"] == COLLAR_NATIVE_BLAKE3,
            "Successful native revision from exact v1B9 is required; no arbitrary fresh predecessor")
    pins = dict(HEIGHT_PINS)
    pins["author-live/apply-height-collar.json"] = hashlib.sha256(raw).hexdigest()
    return new, {"source_v1_uint16le_sha256": V1_GRID_SHA256, "source_v1_native_blake3": V1_NATIVE_BLAKE3,
                 "target_uint16le_sha256": COLLAR_GRID_SHA256, "target_native_blake3": applied["target_composite_blake3"],
                 "original_layer_blake3": ORIGINAL_LAYER, "collar_changed_samples": 447,
                 "full_changed_from_rounded_samples": proof["changed_samples"], "guarded_input_sha256": pins}


def arm_cave_traversal_camera(*, owner_confirms_play_in_viewport=False,
                              allow_approved_3d_capture=False, expected_plan_sha256,
                              output_directory_name="runtime-camera-final"):
    global _runner, OUT
    require(isinstance(output_directory_name, str) and output_directory_name.startswith("runtime-camera-")
            and len(output_directory_name) <= 80
            and all(c.isascii() and (c.isalnum() or c in "-_") for c in output_directory_name),
            "Bounded fresh runtime-camera-* evidence directory name required")
    selected_out = BASE / output_directory_name
    require(_runner is None and not selected_out.exists(), "One fresh observation only; retain existing results")
    require(owner_confirms_play_in_viewport is True and allow_approved_3d_capture is True,
            "Explicit Play-In-Viewport mode and previously approved3D capture required")
    require(expected_plan_sha256 == PLAN_SHA256 and hashlib.sha256(PLAN.read_bytes()).hexdigest() == PLAN_SHA256,
            "Reviewed cave plan changed")
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    require(plan["schema"] == "Rogue10m.CaveLayoutPlan.20261118.v1" and plan["map"] == TARGET, "Wrong cave plan")
    import unreal as u
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    require(Path(u.Paths.project_dir()).resolve() == ROOT and str(u.SystemLibrary.get_engine_version()).startswith("5.8.")
            and editor.get_editor_world().get_path_name().split(".")[0] == TARGET and not editor.get_game_world(),
            "Owner must arm in exact idle editor before fresh Play-In-Viewport")
    require(os.getpid() == json.loads((BASE / "baseline-live/result.json").read_text())["pid"], "Exact inspected editor PID required")
    required = ((u.AutomationLibrary, ("take_high_res_screenshot",)),
                (u.AutomationEditorTask, ("is_valid_task", "is_task_done")),
                (u.PlayerCameraManager, ("get_camera_location", "get_camera_rotation", "get_fov_angle")),
                (u.SpringArmComponent, ("get_unfixed_camera_position", "is_collision_fix_applied", "get_world_location")),
                (u.Rogue10mCharacter, ("is_third_person_gameplay_active", "add_movement_input")),
                (u.PlayerController, ("get_control_rotation", "set_control_rotation", "is_move_input_ignored", "is_look_input_ignored")))
    bindings = {cls.__name__: {name: callable(getattr(cls, name, None)) for name in names} for cls, names in required}
    require(all(all(values.values()) for values in bindings.values()), "Required public camera/capture binding unavailable before PIE")
    target_values, height_guard = reviewed_height_contract(plan)
    native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(True))
    heights = native.get("composite_heights_uint16", [])
    require(native.get("success") and native.get("read_only") and native.get("dirty_package_names_unchanged")
            and native.get("landscape_reports_up_to_date") and native.get("layer_count") == 2
            and native.get("component_count") == native.get("collision_count") == 45
            and native.get("original_layer_blake3") == ORIGINAL_LAYER
            and native.get("composite_blake3") == height_guard["target_native_blake3"]
            and heights == target_values and len(heights) == 179488
            and hashlib.sha256(struct.pack("<" + "H" * len(heights), *heights)).hexdigest()
            == COLLAR_GRID_SHA256, "Actual authored latest collar terrain required before runtime QA")
    sequence, length = route_plan(plan)
    require(length / 620 < 700, "Planned walk needs architecture review before spending the default20-minute run")
    OUT = selected_out
    OUT.mkdir(parents=True, exist_ok=False)
    (OUT / "captures").mkdir(exist_ok=False)
    helpers = module("cave_runtime_public_profile_helpers", "Scripts/Editor/PlaytestLandscapeCrop20261116.py")
    runner = _Observer(u, helpers, plan, sequence)
    runner.height_guard = height_guard
    _runner = runner
    runner.handle = u.register_slate_post_tick_callback(runner.tick)
    write_new("armed", {"pid": os.getpid(), "utc": utc(), "plan_sha256": PLAN_SHA256, "bindings": bindings,
              "height_guard": height_guard,
              "input_policy": INPUT_POLICY,
              "owner_starts_PIE": "PlayInViewport; no separate game window", "route_waypoints": len(sequence),
              "planned_walk_length_cm": length, "ideal_walk_sim_seconds_at620": length / 620,
              "captures": 6, "capture_API": "AutomationLibrary.take_high_res_screenshot1280x720,Camera=None,ForceGameView=True",
              "capture_pose_gate": "At least2 advancing actual-PCM frames at target aim, then before/after rotation<=1deg, translation<=2cm, FOV<=.01deg",
              "capture_UI": "Engine high-res request bShowUI=false; actual active PIE viewport",
              "physical_input": False, "teleports": 0, "speed_overrides": 0, "camera_component_changes": 0,
              "watchdog_sim_seconds": 1100, "normal_run_duration_seconds": 1200})
    print("CAVE_CAMERA_OBSERVER ARMED")
    return {"armed": True, "owner_starts_PIE": True, "output": str(OUT)}


class _Observer:
    def __init__(self, u, helpers, plan, sequence):
        self.u, self.helpers, self.plan, self.sequence = u, helpers, plan, sequence
        self.boxes = boxes_from_plan(plan)
        self.handle = None
        self.phase = "waiting_for_fresh_PIE"
        self.started_wall = time.monotonic()
        self.seen = self.start_sim = self.previous_sim = None
        self.index = 0
        self.samples, self.arrivals, self.captures = [], [], []
        self.last_sample = self.last_progress_write = -1e9
        self.progress_distance = float("inf")
        self.progress_sim = self.severe_since = None
        self.capture_state = None
        self.min_camera_distance = float("inf")
        self.max_pull_in = 0.0
        self.worst_camera_sample = None
        self.grounded = self.airborne = 0
        self.finished = False

    def finish(self, error=None):
        if self.finished:
            return
        self.finished = True
        previous_phase = self.phase
        self.phase = "passed" if error is None else "failed"
        cleanup_errors = []
        if self.handle is not None:
            try:
                self.u.unregister_slate_post_tick_callback(self.handle)
            except Exception as cleanup_error:
                cleanup_errors.append(str(cleanup_error))
            self.handle = None
        if getattr(self, "pawn", None):
            try:
                self.pawn.get_movement_component().stop_movement_immediately()
            except Exception as cleanup_error:
                cleanup_errors.append(str(cleanup_error))
        elapsed_sim = None
        if self.start_sim is not None:
            try:
                elapsed_sim = self.u.GameplayStatics.get_time_seconds(self.world) - self.start_sim
            except Exception as cleanup_error:
                cleanup_errors.append(str(cleanup_error))
        summary = {"schema": "Rogue10m.CaveTraversalCamera.20261118.v1", "passed": error is None,
                   "status": self.phase, "phase_before_finish": previous_phase, "error": error, "finished_utc": utc(),
                   "elapsed_wall_seconds": time.monotonic() - self.started_wall,
                   "elapsed_sim_seconds": elapsed_sim, "cleanup_errors": cleanup_errors,
                   "waypoints_planned": len(self.sequence), "waypoints_arrived": len(self.arrivals),
                   "height_guard": self.height_guard,
                   "route_keys_per_site": list(ROUTE_KEYS), "arrivals": self.arrivals, "captures": self.captures,
                   "all_six_images_reviewed": len(self.captures) == 6 and all(c.get("visual_review_passed") for c in self.captures),
                   "sample_count": len(self.samples), "min_camera_to_pivot_cm": self.min_camera_distance
                       if math.isfinite(self.min_camera_distance) else None,
                   "max_camera_pull_in_cm": self.max_pull_in, "worst_camera_sample": self.worst_camera_sample,
                   "grounded_observations": self.grounded, "airborne_observations": self.airborne,
                   "physical_input": False, "teleports": 0, "speed_overrides": 0, "run_timer_overrides": 0,
                   "input_policy": INPUT_POLICY,
                   "camera_component_changes": 0, "jump_inputs": 0, "sprint_inputs": 0,
                   "input": "normal AddMovementInput + public control-rotation aim bounded180deg/s; no offset setters",
                   "scope": "One prepared normal-profile PIE route, every planned cave chain both directions; no full combat/NavMesh/physical-keyboard acceptance"}
        write_new("samples", {"samples": self.samples})
        write_new("result", summary)
        print("CAVE_CAMERA_OBSERVER " + self.phase.upper())
        if error:
            self.u.log_error("CAVE_CAMERA_OBSERVER " + error)

    def tick(self, _delta):
        if getattr(self, "busy", False) or self.finished:
            return
        self.busy = True
        try:
            self.step()
        except Exception:
            self.finish(traceback.format_exc())
        finally:
            self.busy = False

    def prepare(self, world):
        u, self.world, self.pc = self.helpers._context({TARGET})
        self.pc.enter_selected_character()
        self.pc.close_all_blocking_panels()
        self.identity = self.helpers._selected_profile(u, require_runtime_name=True)
        self.pawn = u.GameplayStatics.get_player_character(world, 0)
        self.player_state = u.GameplayStatics.get_player_state(world, 0)
        self.gs = u.GameplayStatics.get_game_state(world)
        p = self.pawn.get_actor_location()
        require(isinstance(self.pawn, u.Rogue10mCharacter) and math.hypot(p.x - 15000, p.y - 32550) <= 60,
                "Normal fresh S spawn required; no teleport fallback")
        require(self.pawn.is_third_person_gameplay_active(), "Existing runtime third-person mode required")
        arms = list(self.pawn.get_components_by_class(u.SpringArmComponent))
        require(len(arms) == 1, "Exactly one existing gameplay spring arm required")
        self.arm = arms[0]
        self.camera_manager = self.pc.player_camera_manager
        require(isinstance(self.camera_manager, u.PlayerCameraManager), "Existing PlayerCameraManager required")
        self.arm_contract = self.arm_state()
        require(abs(self.arm_contract["target_arm_length"] - 320) < .01 and abs(self.arm_contract["probe_size"] - 14) < .01
                and self.arm.get_editor_property("probe_channel") == u.CollisionChannel.ECC_CAMERA
                and self.arm_contract["do_collision_test"] and not self.arm_contract["enable_camera_lag"]
                and not self.arm_contract["enable_camera_rotation_lag"], "Normal320cm/Camera14/no-lag spring arm required")
        require(abs(self.gs.get_run_duration_seconds() - 1200) < .01, "Production1200-second run duration required")
        if not self.gs.is_run_active():
            require(self.gs.get_run_phase() == u.Rogue10mRunPhase.WAITING_TO_START, "Cannot restart an ended run")
            self.gs.start_run()
        require(self.gs.is_run_active(), "Default public StartRun preparation failed")
        self.phase = "settling_at_S"
        self.settle_wall = time.monotonic()
        write_new("runtime-start", {"utc": utc(), "PIE_world": world.get_path_name(), "identity": self.identity,
                  "spawn_cm": xyz(p), "spring_arm": self.arm_contract,
                  "third_person_gameplay": True, "run_duration_seconds": self.gs.get_run_duration_seconds(),
                  "remaining_run_seconds": self.gs.get_remaining_run_seconds()})

    def arm_state(self):
        return {"target_arm_length": float(self.arm.get_editor_property("target_arm_length")),
                "probe_size": float(self.arm.get_editor_property("probe_size")),
                "probe_channel": str(self.arm.get_editor_property("probe_channel")),
                "target_offset": xyz(self.arm.get_editor_property("target_offset")),
                "socket_offset": xyz(self.arm.get_editor_property("socket_offset")),
                "do_collision_test": bool(self.arm.get_editor_property("do_collision_test")),
                "enable_camera_lag": bool(self.arm.get_editor_property("enable_camera_lag")),
                "enable_camera_rotation_lag": bool(self.arm.get_editor_property("enable_camera_rotation_lag"))}

    def guard(self, world):
        u = self.u
        require(world == self.world and u.GameplayStatics.get_player_character(world, 0) == self.pawn
                and u.GameplayStatics.get_player_controller(world, 0) == self.pc, "Runtime world/controller/pawn changed")
        require(not self.pawn.is_dead() and not self.pawn.is_sprinting() and not self.pawn.is_dodging()
                and self.pawn.is_third_person_gameplay_active() and self.gs.is_run_active(), "Normal alive walking/third-person run contract failed")
        require(abs(self.gs.get_run_duration_seconds() - 1200) < .01, "Production run duration changed")
        require(u.GameplayStatics.get_player_state(world, 0) == self.player_state
                and str(self.player_state.get_character_display_name()) == self.helpers.CHARACTER_NAME
                and not self.pc.is_move_input_ignored() and not self.pc.is_look_input_ignored(),
                "Normal selected profile and unblocked gameplay input required")
        movement = self.pawn.get_movement_component()
        require(abs(float(movement.max_walk_speed) - 620) < .05
                and abs(float(self.pawn.get_character_stat_snapshot().get_editor_property("MoveSpeed")) - 620) < .05,
                "Normal620cm/s equipment/stat walk speed required")
        require(abs(u.GameplayStatics.get_global_time_dilation(world) - 1) < .001
                and abs(float(self.pawn.get_editor_property("CustomTimeDilation")) - 1) < .001
                and not u.GameplayStatics.is_game_paused(world), "Normal unpaused time required")
        require(self.arm_state() == self.arm_contract, "Camera component settings changed during observation")
        return movement

    def telemetry(self, sim, movement):
        location = xyz(self.camera_manager.get_camera_location())
        rotation = self.camera_manager.get_camera_rotation()
        pivot = self.arm.get_world_location() + self.arm.get_editor_property("target_offset")
        unfixed = xyz(self.arm.get_unfixed_camera_position())
        pivot = xyz(pivot)
        distance = math.dist(location, pivot)
        pull_in = max(0.0, math.dist(unfixed, pivot) - distance)
        sample = {"sim_seconds": sim - self.start_sim, "phase": self.phase, "waypoint_index": self.index,
                  "waypoint": self.sequence[min(self.index, len(self.sequence) - 1)]["label"],
                  "pawn_cm": xyz(self.pawn.get_actor_location()), "velocity_cm_s": xyz(self.pawn.get_velocity()),
                  "grounded": bool(movement.is_moving_on_ground()), "camera_cm": location,
                  "camera_rotation_pitch_yaw_roll": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
                  "camera_FOV": float(self.camera_manager.get_fov_angle()), "pivot_cm": pivot,
                  "unfixed_camera_cm": unfixed, "camera_to_pivot_cm": distance,
                  "pull_in_cm": pull_in, "collision_fix_applied": bool(self.arm.is_collision_fix_applied()),
                  "remaining_run_seconds": float(self.gs.get_remaining_run_seconds())}
        require(all(math.isfinite(v) for v in location + unfixed + pivot + sample["pawn_cm"] + sample["velocity_cm_s"]
                    + sample["camera_rotation_pitch_yaw_roll"] + [sample["camera_FOV"], sample["remaining_run_seconds"]]),
                "Nonfinite camera/pawn telemetry")
        require(math.hypot(*sample["velocity_cm_s"][:2]) <= 620.1, "Actual horizontal velocity exceeds normal walk cap")
        self.min_camera_distance = min(self.min_camera_distance, distance)
        if self.worst_camera_sample is None or pull_in > self.max_pull_in:
            self.max_pull_in = pull_in
            self.worst_camera_sample = sample
        self.samples.append(sample)
        self.grounded += int(sample["grounded"])
        self.airborne += int(not sample["grounded"])
        require(sample["grounded"], "Ground contact lost during ordinary walk")
        require(distance >= 140, "Severe camera collapse below140cm pivot distance")
        if pull_in > 180:
            self.severe_since = sim if self.severe_since is None else self.severe_since
            require(sim - self.severe_since <= .5, "Camera pull-in exceeded180cm for more than0.5sim seconds")
        else:
            self.severe_since = None
        embeddings = camera_embeddings(location, self.boxes)
        sample["planned_core_box_embeddings"] = embeddings
        require(not embeddings, "Camera14cm sphere embedded in planned cave core: " + str(embeddings))
        return sample

    def aim(self, target_yaw, dt):
        rotation = self.pc.get_control_rotation()
        difference = normalize_angle(target_yaw - rotation.yaw)
        delta = max(-180 * dt, min(180 * dt, difference))
        self.pc.set_control_rotation(self.u.Rotator(pitch=-12, yaw=normalize_angle(rotation.yaw + delta), roll=0))
        return abs(difference - delta)

    def capture_step(self, sim, movement, dt):
        cap = self.capture_state
        yaw_error = self.aim(cap["aim_yaw"], dt)
        require(math.dist(xyz(self.pawn.get_actor_location()), cap["arrival_pawn_cm"]) <= 140,
                "Pawn moved away while holding a capture; no warp recovery")
        if self.phase == "capture_settle":
            velocity = self.pawn.get_velocity()
            if (math.sqrt(velocity.x ** 2 + velocity.y ** 2 + velocity.z ** 2) >= 5
                    or sim - cap["arrived_sim"] < .5 or yaw_error > 1):
                cap["stable_PCM_frames"] = []
                return
            before = self.telemetry(sim, movement)
            actual_rotation = before["camera_rotation_pitch_yaw_roll"]
            actual_aim_error = max(abs(normalize_angle(actual_rotation[0] + 12)),
                                   abs(normalize_angle(actual_rotation[1] - cap["aim_yaw"])),
                                   abs(normalize_angle(actual_rotation[2])))
            if actual_aim_error > 1:
                cap["stable_PCM_frames"] = []
                return
            stable_frames = cap.setdefault("stable_PCM_frames", [])
            if stable_frames and before["sim_seconds"] <= stable_frames[-1]["sim_seconds"]:
                return
            if stable_frames and not stable_capture_pose(capture_pose_delta(stable_frames[-1], before)):
                stable_frames.clear()
            stable_frames.append(before)
            if len(stable_frames) < 2:
                return
            image = OUT / "captures" / (str(cap["index"]).zfill(2) + "_" + cap["site"] + "_" + cap["label"] + ".png")
            require(not image.exists(), "Preserve existing runtime capture")
            cap.update(image=image, pose_before=before, capture_wall=time.monotonic())
            # Defaults are Camera=None, mask=False, HDR=False, ForceGameView=True.
            # In Play-In-Viewport, ActiveViewport is the actual PIE FSceneViewport.
            cap["task"] = self.u.AutomationLibrary.take_high_res_screenshot(1280, 720, str(image))
            require(cap["task"] and cap["task"].is_valid_task(), "Invalid actual PIE screenshot task")
            self.phase = "capture_pending"
            return
        if self.phase == "capture_pending":
            require(time.monotonic() - cap["capture_wall"] < 90, "Actual game screenshot task timed out")
            if not cap["task"].is_task_done():
                return
            image = cap["image"]
            require(image.is_file() and image.stat().st_size > 1024, "Actual PIE PNG missing/empty")
            data = image.read_bytes()
            require(data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", data[16:24]) == (1280, 720),
                    "Expected1280x720 PNG from actual game viewport")
            checksum = hashlib.sha256(data).hexdigest()
            after = self.telemetry(sim, movement)
            pose_delta = capture_pose_delta(cap["pose_before"], after)
            pose_proof = {"stable_advancing_PCM_frames": len(cap["stable_PCM_frames"]),
                          "target_pitch_yaw_roll_degrees": [-12, cap["aim_yaw"], 0],
                          "before_after_delta": pose_delta,
                          "passed": stable_capture_pose(pose_delta)}
            write_new("camera-pose-" + str(cap["index"]), pose_proof)
            require(pose_proof["passed"], "Actual game camera changed during screenshot; preserve PNG and pose proof, reject capture")
            cap["ack"] = {"capture_index": cap["index"], "visual_review_passed": True, "image_sha256": checksum}
            request = {"schema": "Rogue10m.CaveGameCameraReview.20261118.v1", "capture_index": cap["index"],
                       "site": cap["site"], "label": cap["label"], "image": str(image), "image_sha256": checksum,
                       "pose_before": cap["pose_before"], "pose_after": after, "capture_pose_proof": pose_proof,
                       "actual_existing_PCM_camera": True, "camera_override_actor": None, "editor_camera_changed": False,
                       "capture_API": "AutomationLibrary.TakeHighResScreenshot; active PIE viewport, Camera=None",
                       "engine_high_res_bShowUI": False, "gameclock_keeps_running": True,
                       "owner_review_required": True, "acknowledgment_file": str(OUT / ("camera-done-" + str(cap["index"]) + ".json")),
                       "ack_payload_required": cap["ack"]}
            write_new("camera-request-" + str(cap["index"]), request)
            cap["record"] = request
            cap["task"] = None
            self.phase = "await_visual_review"
            print("CAVE_CAMERA_REVIEW_REQUIRED " + str(cap["index"]) + " " + str(image))
            return
        ackfile = OUT / ("camera-done-" + str(cap["index"]) + ".json")
        if not ackfile.exists():
            return
        require(json.loads(ackfile.read_text(encoding="utf-8-sig")) == cap["ack"], "Camera visual review rejected or acknowledgment mismatched")
        require(hashlib.sha256(cap["image"].read_bytes()).hexdigest() == cap["ack"]["image_sha256"], "Captured image changed after review")
        self.captures.append({"capture_index": cap["index"], "site": cap["site"], "label": cap["label"],
                              "image": str(cap["image"]), "image_sha256": cap["ack"]["image_sha256"],
                              "visual_review_passed": True, "reviewed_at_sim": sim - self.start_sim})
        self.capture_state = None
        self.phase = "walking"
        self.index += 1
        self.progress_distance, self.progress_sim = float("inf"), sim

    def step(self):
        u = self.u
        require(time.monotonic() - self.started_wall <= 1600, "Bounded1600-second wall watchdog expired")
        world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if self.phase == "waiting_for_fresh_PIE":
            if world is None:
                return
            if self.seen is None:
                self.seen = time.monotonic()
                return
            if time.monotonic() - self.seen < 3:
                return
            self.prepare(world)
            return
        movement = self.guard(world)
        sim = u.GameplayStatics.get_time_seconds(world)
        if self.phase == "settling_at_S":
            if time.monotonic() - self.settle_wall < 2:
                return
            require(movement.is_moving_on_ground(), "Stable grounded fresh S required")
            self.phase, self.start_sim, self.previous_sim = "walking", sim, sim
            self.progress_sim = sim
        raw_sim_dt = max(0.0, sim - self.previous_sim)
        dt = min(.25, raw_sim_dt)
        self.previous_sim = sim
        require(sim - self.start_sim <= 1100, "1100sim-second observation budget exceeded; default run unchanged")
        require(hashlib.sha256(PLAN.read_bytes()).hexdigest() == PLAN_SHA256 if sim - self.last_progress_write >= 2 else True,
                "Plan changed during runtime observation")
        if sim - self.last_progress_write >= 2:
            require(all(hashlib.sha256((BASE / relative).read_bytes()).hexdigest() == expected
                        for relative, expected in self.height_guard["guarded_input_sha256"].items()),
                    "Frozen collar/predecessor/native revision evidence changed during runtime observation")
        if sim - self.last_sample >= .1:
            self.telemetry(sim, movement)
            self.last_sample = sim
        if sim - self.last_progress_write >= 2:
            (OUT / "progress.json").write_text(json.dumps({"phase": self.phase, "waypoint_index": self.index,
                "waypoints_total": len(self.sequence), "elapsed_sim_seconds": sim - self.start_sim,
                "remaining_run_seconds": self.gs.get_remaining_run_seconds(), "captures_reviewed": len(self.captures),
                "latest_sample": self.samples[-1] if self.samples else None}, ensure_ascii=False, indent=2), encoding="utf-8")
            self.last_progress_write = sim
        if self.capture_state is not None:
            self.capture_step(sim, movement, dt)
            return
        if self.index >= len(self.sequence):
            require(len(self.captures) == 6, "All six runtime camera images must be captured and reviewed")
            require(len(self.arrivals) == len(self.sequence), "All cave/field waypoints must be actually reached")
            self.finish()
            return
        row = self.sequence[self.index]
        position = self.pawn.get_actor_location()
        target = row["ground_cm"]
        distance = math.hypot(target[0] - position.x, target[1] - position.y)
        if distance <= ARRIVAL_CM:
            capsule = self.pawn.get_editor_property("capsule_component")
            ground = position.z - capsule.get_scaled_capsule_half_height()
            require(abs(ground - target[2]) <= 65, "Waypoint ground height differs; did not reach the intended floor")
            self.arrivals.append({"waypoint_index": self.index, **row, "observed_pawn_cm": xyz(position),
                                  "arrival_error_xy_cm": distance, "observed_ground_z_cm": ground,
                                  "sim_seconds": sim - self.start_sim})
            if row["capture"]:
                require(self.index + 1 < len(self.sequence), "Capture needs a normal look-ahead direction")
                look = next((future["ground_cm"] for future in self.sequence[self.index + 1:]
                             if math.hypot(future["ground_cm"][0] - target[0], future["ground_cm"][1] - target[1]) > 100), None)
                require(look is not None, "Capture needs a distinct normal look-ahead waypoint")
                yaw = math.degrees(math.atan2(look[1] - position.y, look[0] - position.x))
                self.capture_state = {"index": len(self.captures) + 1, "site": row["site"], "label": row["capture"],
                                      "arrived_sim": sim, "arrival_pawn_cm": xyz(position), "aim_yaw": yaw}
                self.phase = "capture_settle"
                return
            self.index += 1
            self.progress_distance, self.progress_sim = float("inf"), sim
            return
        if distance <= self.progress_distance - 10:
            self.progress_distance, self.progress_sim = distance, sim
        require(sim - self.progress_sim <= 10, "Normal movement stalled at waypoint; preserve result, no warp recovery")
        yaw = math.degrees(math.atan2(target[1] - position.y, target[0] - position.x))
        error = self.aim(yaw, dt)
        if error > 35:
            return
        scale = min(1.0, distance / max(120.0, 620.0 * raw_sim_dt * 1.1))
        self.pawn.add_movement_input(u.Vector((target[0] - position.x) / distance,
                                             (target[1] - position.y) / distance, 0), scale, False)
