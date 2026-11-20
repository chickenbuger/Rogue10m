"""Exercise both StartHub bypasses and the actual departure in an owned PIE.

Load StartHub in an isolated rendered editor launched with -UserDir. Then
import this module and call arm(r"<new output directory>"). The current
editor play profile should be Play In Viewport. No actor/world settings,
progression, speed, or camera-component settings are written.
"""

import hashlib
import json
import math
from pathlib import Path
import struct
import time
import traceback

import unreal


V = unreal.Vector
HUB = "/Game/Rogue10m/Maps/StartHub.StartHub"
WALK = (
    ((0, -2040), "01-gameplay-arrival", (0, 500)),
    ((650, -1100), None, None),
    ((650, 0), "02-gameplay-east-of-well", (0, 2135)),
    ((650, 950), None, None),
    ((0, 1730), "03-gameplay-north-apron", (0, 2135)),
    ((-650, 950), None, None),
    ((-650, 0), "04-gameplay-west-of-well", (-1280, 1460)),
    ((-650, -1000), None, None),
    ((0, -1940), None, None),
    ((650, -1100), None, None),
    ((650, 950), None, None),
)
NORTH_APPROACH = (0, 1800)
_active = None


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def current_pose(controller, pawn):
    manager = controller.player_camera_manager
    rotation = manager.get_camera_rotation()
    return {"pawn_cm": vector(pawn.get_actor_location()), "camera_cm": vector(manager.get_camera_location()),
            "pitch_yaw_roll": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
            "fov": float(manager.get_fov_angle())}


def departure_waypoints(game):
    gates = unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mRunPortal)
    if len(gates) != 1 or str(gates[0].get_editor_property("target_level_name")) != "/Game/Rogue10m/Maps/OpenRunMap":
        raise RuntimeError("The single authored OpenRunMap departure gate is required.")
    gate = gates[0]
    center = gate.get_actor_location()
    # Native portal replay approaches along the gate's local Y (right) vector.
    # Follow the same sign from the southern apron, then walk through the box.
    direction = gate.get_actor_right_vector()
    side = (NORTH_APPROACH[0] - center.x) * direction.x + (NORTH_APPROACH[1] - center.y) * direction.y
    axis = direction if side <= 0 else direction * -1.0
    before = center - axis * 300.0
    after = center + axis * 300.0
    return ((before.x, before.y), None, None), ((after.x, after.y), None, None)


def arm(output_dir, *, width=1280, height=720):
    """Start and own a fresh Play-In-Viewport test, then restore edit mode."""
    global _active
    if _active is not None:
        raise RuntimeError("A StartHub PIE verification is already running.")
    if not unreal.SystemLibrary.parse_param_value(unreal.SystemLibrary.get_command_line(), "UserDir="):
        raise RuntimeError("Use the owned rendered editor's private -UserDir for the PIE run.")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = editor.get_editor_world()
    if not world or world.get_path_name() != HUB or editor.get_game_world():
        raise RuntimeError("Load the saved StartHub with no running PIE session before arming.")
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=False)
    state = {"phase": "spawn", "index": 0, "started": time.monotonic(), "last_sample": 0.0,
             "shot_start": None, "task": None, "first_remaining": None, "handle": None}
    report = {"input_method": "programmatic AddMovementInput; no teleports or override camera",
              "physical_input": False, "gameplay_camera": True, "waypoints": [], "captures": [],
              "grounded_samples": 0, "airborne_samples": 0}

    def finish(error=None):
        global _active
        unreal.unregister_slate_post_tick_callback(state["handle"])
        _active = None
        if editor.get_game_world():
            levels.editor_request_end_play()
        if error:
            report["error"] = error
        report["passed"] = not error
        report["wall_seconds"] = time.monotonic() - state["started"]
        (output / "walk-receipt.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if error:
            unreal.log_error(f"StartHub rendered walk failed in {state['phase']}; report={output}")
        else:
            unreal.log(f"START_HUB_DETAIL_RENDERED_WALK_PASSED waypoints={len(report['waypoints'])} report={output}")

    def observe_field(game, pawn, game_state, now):
        if state["index"] < len(state["route"]) - 1:
            raise RuntimeError("The portal activated before the designated final approach.")
        if not pawn.get_movement_component().is_moving_on_ground():
            return
        remaining = float(game_state.get_remaining_run_seconds())
        duration = float(game_state.get_run_duration_seconds())
        if (game_state.get_run_phase() != unreal.Rogue10mRunPhase.RUNNING
                or not game_state.is_run_active() or abs(duration - 1200.0) > 0.01):
            raise RuntimeError("Field arrival did not start a normal 1200-second run.")
        if state["first_remaining"] is None:
            if not 1185.0 <= remaining <= 1200.0:
                raise RuntimeError(f"Field countdown starts outside its normal window: {remaining}")
            state.update(first_remaining=remaining, field_started=now, phase="field_countdown")
        elif now - state["field_started"] > 2 and state["first_remaining"] - remaining >= 1.0:
            report["field_arrival"] = {"map": game.get_path_name(), "timer_duration_seconds": duration,
                                       "initial_remaining_seconds": state["first_remaining"],
                                       "after_remaining_seconds": remaining}
            report["departure"] = {"origin": "native authored portal overlap",
                                   "crossing_axis": "portal local Y/right vector",
                                   "approach_xy_cm": state["route"][-2][0], "requested_exit_xy_cm": state["route"][-1][0]}
            finish()

    def capture(game, controller, pawn, now, name, target):
        pawn_location = pawn.get_actor_location()
        desired = unreal.MathLibrary.find_look_at_rotation(pawn_location, V(*target, pawn_location.z))
        controller.set_control_rotation(unreal.Rotator(pitch=-8.0, yaw=desired.yaw, roll=0.0))
        if state["task"] is None:
            if state["shot_start"] is None:
                state["shot_start"] = now
            if now - state["shot_start"] < 0.65 or pawn.get_velocity().length() >= 5.0:
                return False
            state["pose_before"] = current_pose(controller, pawn)
            image = output / (name + ".png")
            task = unreal.AutomationLibrary.take_high_res_screenshot(width, height, str(image))
            if not task or not task.is_valid_task():
                raise RuntimeError(f"No actual PIE screenshot task for {name}.")
            state.update(task=task, image=image)
            return False
        if now - state["shot_start"] > 30:
            raise RuntimeError(f"Timed out capturing {name}.")
        if not state["task"].is_task_done() or not state["image"].is_file():
            return False
        image = state["image"]
        data = image.read_bytes()
        if data[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", data[16:24]) != (width, height):
            raise RuntimeError(f"PIE screenshot is not the requested PNG: {image}")
        after = current_pose(controller, pawn)
        before = state["pose_before"]
        if math.dist(before["camera_cm"], after["camera_cm"]) > 6:
            raise RuntimeError(f"Gameplay camera moved materially during {name}; inspect screenshot.")
        report["captures"].append({"png": image.name, "sha256": hashlib.sha256(data).hexdigest(),
                                   "before": before, "after": after, "source_pie_world": game.get_path_name()})
        state.update(task=None, shot_start=None)
        return True

    def tick_impl(delta):
        now = time.monotonic()
        if now - state["started"] > 150:
            raise RuntimeError(f"Rendered walk timed out at waypoint {state['index']}.")
        game = editor.get_game_world()
        if not game:
            route = state.get("route")
            portal_loading = route and state["index"] >= len(route) - 1
            if state["phase"] != "spawn" and not portal_loading:
                raise RuntimeError("PIE closed before the route stopped or departed.")
            return
        controller = unreal.GameplayStatics.get_player_controller(game, 0)
        pawn = unreal.GameplayStatics.get_player_pawn(game, 0)
        game_state = unreal.GameplayStatics.get_game_state(game)
        if not (controller and isinstance(pawn, unreal.Rogue10mCharacter) and isinstance(game_state, unreal.Rogue10mGameState)):
            return
        name = unreal.GameplayStatics.get_current_level_name(game, True)
        if name == "OpenRunMap":
            observe_field(game, pawn, game_state, now)
            return
        if name != "StartHub":
            raise RuntimeError(f"PIE is in an unexpected map: {name}")
        if (game_state.get_run_phase() != unreal.Rogue10mRunPhase.WAITING_TO_START or game_state.is_run_active()
                or abs(game_state.get_remaining_run_seconds() - 1200.0) > 0.01):
            raise RuntimeError("The run activated inside StartHub.")
        if not pawn.is_third_person_gameplay_active():
            raise RuntimeError("The standard third-person StartHub camera is required.")
        if controller.is_move_input_ignored() or controller.is_look_input_ignored():
            raise RuntimeError("Movement or look input is blocked in StartHub.")
        movement = pawn.get_movement_component()
        location = pawn.get_actor_location()
        if state["phase"] == "spawn":
            if not movement.is_moving_on_ground():
                return
            if math.dist((location.x, location.y), WALK[0][0]) > 55.0:
                raise RuntimeError(f"The avatar did not spawn at the existing south arrival: {location}.")
            state.update(phase="walk", latest_advanced=now, route=(*WALK, *departure_waypoints(game)))
            report["spawn_cm"] = vector(location)
            report["hub_remaining_seconds"] = float(game_state.get_remaining_run_seconds())
        if now - state["last_sample"] >= 0.2:
            sample_kind = "grounded_samples" if movement.is_moving_on_ground() else "airborne_samples"
            report[sample_kind] += 1
            state["last_sample"] = now
        if state["index"] == len(state["route"]):
            if now - state["latest_advanced"] > 4.0:
                raise RuntimeError("The pawn walked into the actual departure gate and remained in StartHub.")
            return
        goal, shot_name, shot_direction = state["route"][state["index"]]
        dx, dy = goal[0] - location.x, goal[1] - location.y
        distance = math.hypot(dx, dy)
        if distance > 45.0 and state["shot_start"] is None:
            if now - state["latest_advanced"] > 14.0:
                raise RuntimeError(f"Walk stalled at {vector(location)} approaching {goal}.")
            speed = movement.max_walk_speed
            strength = min(1.0, distance / max(100.0, speed * min(delta, 0.1) * 1.1))
            pawn.add_movement_input(V(dx / distance, dy / distance, 0), strength, False)
            return
        if shot_name and not capture(game, controller, pawn, now, shot_name, shot_direction):
            return
        report["waypoints"].append({"xy_cm": goal, "observed_cm": vector(pawn.get_actor_location())})
        state["index"] += 1
        state["latest_advanced"] = now

    def tick(delta):
        if state.get("busy"):
            return
        state["busy"] = True
        try:
            tick_impl(delta)
        except Exception:
            finish(traceback.format_exc())
        finally:
            state["busy"] = False

    state["handle"] = unreal.register_slate_post_tick_callback(tick)
    _active = state
    levels.editor_request_begin_play()
    unreal.log(f"START_HUB_DETAIL_RENDERED_WALK_ARMED waypoints={len(WALK) + 2} report={output}")
