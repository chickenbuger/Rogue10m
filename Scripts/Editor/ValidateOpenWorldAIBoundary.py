"""Nonblocking native-AI melee boundary regression for the saved OpenRunMap.

Only ai-boundary.json is written. The two existing forest monster types and
the player are repositioned in this script's own PIE session; editor actors,
assets and Blueprint defaults are never saved. Root owns editor invocation.
"""

import json
import math
from pathlib import Path
import sys
import time
import traceback

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SCRIPT_DIR = PROJECT / "Scripts/Editor"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from OpenWorldLayout import POIS, walkable_height_at  # noqa: E402


MAP = "/Game/Rogue10m/Maps/OpenRunMap"
WORLD_TAG = "Rogue10m.OpenWorld"
SINGLETON = "_rogue10m_open_world_ai_boundary_validator"
REPORT_PATH = PROJECT / "Feature/doc/evidence/open-world-20261005/ai-boundary.json"
CASES = (("Sporeling", 150.0), ("VerdantSlime", 120.0))
BOUNDARY_OFFSET_CM = 5.0
STATIONARY_TOLERANCE_CM = 1.0
GLOBAL_TIMEOUT_SECONDS = 300.0
state = {
    "callback": None,
    "owns_pie": False,
    "finished": False,
    "started": time.monotonic(),
    "phase": "static",
    "phase_started": time.monotonic(),
    "phase_timeout": 90.0,
    "case_index": 0,
    "report": {
        "status": "running",
        "map": MAP,
        "checks": [],
        "cases": [],
        "limitations": [
            "Two existing authored normal forest types are tested in a standalone owned PIE session.",
            "Player input suppression, AI isolation, regeneration suppression and actor placement are PIE-only setup.",
            "The script applies a one-point native damage perception stimulus to the selected monster, never to the player.",
            "The old combined-radius threshold is calculated from live capsule radii and UE 5.8's default 1.1 agent radius multiplier; the engine multiplier and MoveTo return value have no reflected Python accessor.",
            "Pursuit proof uses native Blackboard targeting, actual monster movement into melee range and native player damage with other AI stopped; it does not call AI decisions or movement requests.",
            "This is a short melee boundary regression, not whole-map, boss, network or packaged-performance verification.",
        ],
    },
}


def vector_data(value):
    return [float(value.x), float(value.y), float(value.z)]


def distance_2d(a, b):
    return math.hypot(a.x - b.x, a.y - b.y)


def write_report():
    state["report"]["phase"] = state["phase"]
    state["report"]["elapsed_wall_seconds"] = time.monotonic() - state["started"]
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(state["report"], ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def record(name, passed, **details):
    state["report"]["checks"].append({"name": name, "passed": bool(passed), **details})
    if not passed:
        raise RuntimeError(f"{name}: {details}")
    write_report()


def phase(name, timeout=45.0):
    state["phase"] = name
    state["phase_started"] = time.monotonic()
    state["phase_timeout"] = timeout
    state["last_sample"] = -1.0
    write_report()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def close_report(message, failed):
    state["finished"] = True
    callback = state.pop("callback", None)
    try:
        if callback is not None:
            unreal.unregister_slate_post_tick_callback(callback)
    except Exception as error:
        failed = True
        state["report"].setdefault("cleanup_errors", []).append(str(error))
    state["report"].update(status="failed" if failed else "passed", message=message,
                           owned_pie_cleanup_confirmed=not state["owns_pie"])
    write_report()
    log = unreal.log_error if failed else unreal.log
    log(f"[Rogue10mOpenWorldAIBoundary] {state['report']['status'].upper()}: {message}; report={REPORT_PATH}")


def finish(message, failed=False):
    if state["finished"] or state["phase"] == "cleanup":
        return
    state["outcome"] = (message, failed)
    # Restore independent settings separately so one destroyed actor cannot
    # prevent the remaining restores or the owned PIE shutdown.
    restores = []
    pawn = state.get("pawn")
    controller = state.get("controller")
    regeneration = state.get("regeneration")
    brain = state.get("brain")
    if pawn is not None:
        restores.append(lambda: pawn.set_editor_property("can_be_damaged", state["original_can_be_damaged"]))
    if controller is not None and state.get("suppressed_move_input"):
        restores.append(lambda: controller.set_ignore_move_input(False))
    if controller is not None and state.get("suppressed_look_input"):
        restores.append(lambda: controller.set_ignore_look_input(False))
    if regeneration is not None:
        restores.append(lambda: regeneration.configure_regeneration(*state["original_regeneration"]))
    if brain is not None:
        restores.append(lambda: brain.stop_logic("OpenWorld AI boundary validation finished"))
    for restore in restores:
        try:
            restore()
        except Exception as error:
            state["report"].setdefault("cleanup_errors", []).append(f"Runtime restore: {error}")
            state["outcome"] = (message, True)
    # No Python UObject references are kept across the asynchronous End PIE.
    for key in ("pawn", "controller", "regeneration", "monster", "brain", "ai"):
        state.pop(key, None)
    if state["owns_pie"]:
        try:
            unreal.EditorLevelLibrary.editor_end_play()
            phase("cleanup", 20.0)
        except Exception as error:
            state["report"].setdefault("cleanup_errors", []).append(f"End PIE: {error}")
            close_report(message, True)
    else:
        close_report(message, state["outcome"][1])


def bootstrap(world):
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mOpenWorldBootstrap)
    if len(actors) != 1:
        raise RuntimeError(f"Expected one runtime bootstrap, found {len(actors)}.")
    return actors[0]


def navigation_ready(world):
    helper = bootstrap(world)
    nav = unreal.NavigationSystemV1.get_navigation_system(world)
    return (isinstance(nav, unreal.Rogue10mOpenWorldNavigationSystem)
            and helper.is_world_streaming_ready()
            and helper.is_player_navigation_invoker_registered())


def capsule(actor):
    return actor.get_editor_property("capsule_component")


def health(actor):
    return float(actor.get_rogue_attribute_set().get_editor_property("health").get_editor_property("current_value"))


def ground(world, pawn, x, y):
    expected = walkable_height_at(x, y)
    ignored = [pawn] + list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mBasicMonster))
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, expected + 1500.0), unreal.Vector(x, y, expected - 2500.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignored, unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    fields = hit.to_tuple()  # Native BreakHitResult: blocking=0, impact point=5, normal=7.
    if not fields[0] or fields[7].z < 0.65:
        return None
    return float(fields[5].z)


def grounded(world, pawn, actor):
    location = actor.get_actor_location()
    floor = ground(world, pawn, location.x, location.y)
    if floor is None:
        return False
    clearance = location.z - capsule(actor).get_scaled_capsule_half_height() - floor
    return (-2.0 <= clearance <= 15.0
            and actor.get_editor_property("character_movement").is_moving_on_ground())


def teleport(actor, x, y, z):
    actor.get_editor_property("character_movement").stop_movement_immediately()
    actor.set_actor_location(unreal.Vector(x, y, z), False, True)
    actual = actor.get_actor_location()
    record("runtime_placement_exact", distance_2d(actual, unreal.Vector(x, y, z)) < 0.1
           and abs(actual.z - z) < 0.1, actor=actor.get_path_name(),
           expected=[x, y, z], actual=vector_data(actual))


def stop_other_ai(world, active_monster=None):
    count = 0
    for monster in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mBasicMonster):
        if monster == active_monster:
            continue
        ai = monster.get_controller()
        if ai:
            brain = ai.get_editor_property("brain_component")
            if brain:
                brain.stop_logic("OpenWorld AI boundary isolation")
            ai.stop_movement()
            count += 1
    return count


def local_path(world, pawn, monster):
    extent = unreal.Vector(100.0, 100.0, 300.0)
    start, end = monster.get_actor_location(), pawn.get_actor_location()
    a = unreal.NavigationSystemV1.project_point_to_navigation(world, start, None, None, extent)
    b = unreal.NavigationSystemV1.project_point_to_navigation(world, end, None, None, extent)
    if a is None or b is None or distance_2d(a, start) > 20.0 or distance_2d(b, end) > 20.0:
        return None
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, monster, None)
    if not path or not path.is_valid() or path.is_partial():
        return None
    points = path.get_editor_property("path_points")
    if len(points) < 2:
        return None
    return {"start": vector_data(a), "end": vector_data(b),
            "points": [vector_data(point) for point in points], "valid": True, "partial": False}


def choose_case(world, pawn):
    kind, expected_range = CASES[state["case_index"]]
    expected_data = f"/Game/DataAsset/Monster/Normal/DA_Monster_Normal_{kind}.DA_Monster_Normal_{kind}"
    candidates = []
    for monster in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mBasicMonster):
        data = monster.get_monster_data()
        if (monster.is_dead() or not data or data.get_path_name() != expected_data
                or WORLD_TAG not in map(str, monster.get_editor_property("tags"))
                or not isinstance(monster.get_controller(), unreal.Rogue10mMonsterAIController)):
            continue
        location = monster.get_actor_location()
        if math.hypot(location.x - POIS["forest"]["x"], location.y - POIS["forest"]["y"]) <= 6000.0:
            candidates.append(monster)
    if not candidates:
        return
    # Existing spawner locations retain their native Blackboard home/leash.
    # Prefer a grounded pair without modifying any AI or monster tuning data.
    for monster in sorted(candidates, key=lambda actor: actor.get_path_name()):
        location = monster.get_actor_location()
        floor = ground(world, pawn, location.x, location.y)
        player_floor = ground(world, pawn, location.x + expected_range + BOUNDARY_OFFSET_CM, location.y)
        if floor is None or player_floor is None:
            continue
        break
    else:
        return
    ai = monster.get_controller()
    brain = ai.get_editor_property("brain_component")
    record("existing_authored_native_monster", brain is not None, kind=kind,
           actor=monster.get_path_name(), data=expected_data, controller=ai.get_class().get_path_name())
    skill = monster.get_editor_property("attack_skill_data")
    attack_range = float(skill.get_editor_property("attack_range") if skill
                         else monster.get_editor_property("attack_range"))
    record("expected_live_attack_range", abs(attack_range - expected_range) < 0.01,
           kind=kind, attack_range_cm=attack_range,
           attack_skill=skill.get_path_name() if skill else None)
    state.update(monster=monster, ai=ai, brain=brain, attack_range=attack_range)
    state["report"]["cases"].append({"kind": kind, "status": "running", "samples": []})
    state["case"] = state["report"]["cases"][-1]
    state["case"].update(monster=monster.get_path_name(), data=expected_data, attack_range_cm=attack_range)
    pawn.set_editor_property("can_be_damaged", False)
    state["case"]["isolated_other_ai_count"] = stop_other_ai(world)
    teleport(monster, location.x, location.y, floor + capsule(monster).get_scaled_capsule_half_height() + 2.0)
    monster.set_actor_rotation(unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), True)
    teleport(pawn, location.x + attack_range + BOUNDARY_OFFSET_CM, location.y,
             player_floor + capsule(pawn).get_scaled_capsule_half_height() + 2.0)
    pawn.consume_movement_input_vector()
    phase("settle_boundary", 45.0)


def begin_observation(world, controller, pawn):
    monster = state["monster"]
    initial_distance = distance_2d(monster.get_actor_location(), pawn.get_actor_location())
    attack_range = state["attack_range"]
    stop_distance = float(monster.get_editor_property("stop_distance"))
    agent_radius = float(capsule(monster).get_scaled_capsule_radius())
    goal_radius = float(capsule(pawn).get_scaled_capsule_radius())
    desired = max(0.0, min(stop_distance, attack_range * 0.8))
    old_acceptance = max(0.0, desired - agent_radius)
    old_threshold = old_acceptance + goal_radius + 1.1 * agent_radius
    record("outside_attack_inside_old_reach_threshold",
           attack_range + 2.0 < initial_distance <= old_threshold
           and abs(initial_distance - attack_range - BOUNDARY_OFFSET_CM) <= 1.0,
           kind=CASES[state["case_index"]][0], initial_distance_cm=initial_distance,
           attack_range_cm=attack_range, old_acceptance_radius_cm=old_acceptance,
           old_combined_radius_threshold_cm=old_threshold,
           agent_capsule_radius_cm=agent_radius, goal_capsule_radius_cm=goal_radius)
    state["case"].update(initial_distance_cm=initial_distance,
                          old_combined_radius_threshold_cm=old_threshold,
                          monster_origin=vector_data(monster.get_actor_location()),
                          player_origin=vector_data(pawn.get_actor_location()),
                          health_before=health(pawn))
    velocity = pawn.get_velocity()
    speed = math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
    record("player_input_suppressed_for_stationary_boundary",
           controller.is_move_input_ignored() and speed < 1.0, speed_cm_per_second=speed)
    state["pursuit_verified"] = False
    state["target_verified"] = False
    pawn.set_editor_property("can_be_damaged", True)
    state["brain"].restart_logic()
    # Same native perception path as ValidateOpenWorld.py; no Blackboard writes,
    # ExecuteBehaviorDecision/MoveTo calls or direct player damage are used.
    stimulus = unreal.GameplayStatics.apply_damage(monster, 1.0, controller, pawn, unreal.DamageType)
    record("native_monster_damage_perception_stimulus", float(stimulus) > 0.0,
           kind=CASES[state["case_index"]][0], damage_to_monster=float(stimulus),
           directly_applied_player_damage=False)
    phase("observe_boundary", 20.0)


def observe(world, pawn, elapsed):
    monster, case = state["monster"], state["case"]
    if monster.is_dead():
        raise RuntimeError("Boundary monster died before pursuit/damage verification.")
    stop_other_ai(world, monster)
    pawn_location, monster_location = pawn.get_actor_location(), monster.get_actor_location()
    distance = distance_2d(monster_location, pawn_location)
    origin = case["monster_origin"]
    displacement = math.hypot(monster_location.x - origin[0], monster_location.y - origin[1])
    player_origin = case["player_origin"]
    player_displacement = math.hypot(pawn_location.x - player_origin[0], pawn_location.y - player_origin[1])
    if player_displacement > STATIONARY_TOLERANCE_CM:
        raise RuntimeError(f"Player moved {player_displacement:.3f} cm during stationary boundary observation.")
    blackboard = state["ai"].get_component_by_class(unreal.BlackboardComponent)
    target_is_player = bool(blackboard and blackboard.get_value_as_object("TargetActor") == pawn)
    current_health = health(pawn)
    sample = {"elapsed_seconds": elapsed, "distance_to_player_cm": distance,
              "monster_displacement_cm": displacement, "player_displacement_cm": player_displacement,
              "monster_location": vector_data(monster_location), "blackboard_target_is_player": target_is_player,
              "player_health": current_health}
    if elapsed - state["last_sample"] >= 0.1:
        case["samples"].append(sample)
        state["last_sample"] = elapsed
        write_report()
    if target_is_player and not state["target_verified"]:
        record("native_perception_acquired_player_target", True, kind=case["kind"])
        state["target_verified"] = True
    if (target_is_player and not state["pursuit_verified"] and displacement >= 2.0
            and case["initial_distance_cm"] - distance >= 2.0 and distance <= state["attack_range"]):
        record("native_pursuit_crossed_melee_boundary", True, kind=case["kind"],
               initial_distance_cm=case["initial_distance_cm"], final_distance_cm=distance,
               monster_displacement_cm=displacement, player_displacement_cm=player_displacement)
        state["pursuit_verified"] = True
    if state["pursuit_verified"] and target_is_player and case["health_before"] - current_health >= 0.1:
        case["samples"].append({**sample, "native_damage_observed": True})
        case.update(status="passed", health_after=current_health,
                    player_damage_taken=case["health_before"] - current_health)
        record("native_monster_attack_damaged_stationary_player", True, kind=case["kind"],
               health_before=case["health_before"], health_after=current_health,
               directly_applied_player_damage=False)
        pawn.set_editor_property("can_be_damaged", False)
        state["brain"].stop_logic("OpenWorld AI boundary case completed")
        state["ai"].stop_movement()
        state["case_index"] += 1
        if state["case_index"] == len(CASES):
            finish("Sporeling and VerdantSlime pursued from outside melee inside the old reach threshold and natively damaged a stationary player.")
        else:
            phase("choose_case", 45.0)


def tick_impl(delta_seconds):
    elapsed = time.monotonic() - state["phase_started"]
    if state["phase"] == "cleanup":
        if game_world() is None and not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            state["owns_pie"] = False
            close_report(*state["outcome"])
        elif elapsed > 20.0:
            state["report"].setdefault("cleanup_errors", []).append("Owned PIE did not stop within 20 seconds.")
            close_report(state["outcome"][0], True)
        return
    if time.monotonic() - state["started"] > GLOBAL_TIMEOUT_SECONDS or elapsed > state["phase_timeout"]:
        if state.get("case") and state["case"]["status"] == "running":
            state["case"]["status"] = "failed"
        raise RuntimeError(f"Boundary validation timeout in {state['phase']} ({elapsed:.1f}s).")
    world = game_world()
    if world is None:
        return
    if not world.get_name().endswith("OpenRunMap"):
        raise RuntimeError(f"Unexpected PIE world {world.get_path_name()}.")
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not controller or not pawn:
        return
    if not isinstance(pawn, unreal.Rogue10mCharacter) or pawn.get_controller() != controller:
        raise RuntimeError("Boundary validation requires the possessed Rogue10m player.")
    if unreal.GameplayStatics.is_game_paused(world) or pawn.is_dead():
        raise RuntimeError("Boundary PIE paused or the player died.")
    if state["phase"] == "wait_pie":
        state.update(pawn=pawn, controller=controller,
                     original_can_be_damaged=bool(pawn.get_editor_property("can_be_damaged")))
        pawn.set_editor_property("can_be_damaged", False)
        if not controller.is_move_input_ignored():
            controller.set_ignore_move_input(True)
            state["suppressed_move_input"] = True
        if not controller.is_look_input_ignored():
            controller.set_ignore_look_input(True)
            state["suppressed_look_input"] = True
        regeneration = pawn.get_component_by_class(unreal.Rogue10mVitalRegenerationComponent)
        if regeneration:
            state["regeneration"] = regeneration
            state["original_regeneration"] = tuple(float(regeneration.get_editor_property(name)) for name in
                                                     ("health_regeneration_per_second", "stamina_regeneration_per_second", "mana_regeneration_per_second"))
            regeneration.configure_regeneration(0.0, state["original_regeneration"][1], state["original_regeneration"][2])
        record("player_has_health_for_two_native_hits", health(pawn) >= 30.0, health=health(pawn))
        point = POIS["forest"]
        teleport(pawn, point["x"] + 350.0, point["y"], point["placement_z"] + 500.0)
        phase("wait_forest", 60.0)
    elif state["phase"] == "wait_forest":
        stop_other_ai(world)
        if (navigation_ready(world) and grounded(world, pawn, pawn)
                and len(unreal.GameplayStatics.get_all_actors_with_tag(world, WORLD_TAG + ".StreamProbe.forest")) == 1):
            record("forest_streaming_and_player_invoker_ready", True)
            phase("choose_case", 45.0)
    elif state["phase"] == "choose_case":
        stop_other_ai(world)
        if navigation_ready(world):
            choose_case(world, pawn)
    elif state["phase"] == "settle_boundary":
        stop_other_ai(world)
        pawn.get_editor_property("character_movement").stop_movement_immediately()
        if (elapsed < 0.5 or not navigation_ready(world) or not grounded(world, pawn, pawn)
                or not grounded(world, pawn, state["monster"])):
            return
        path = local_path(world, pawn, state["monster"])
        if path is None:
            return
        record("boundary_actual_complete_navigation_path", True, kind=state["case"]["kind"], **path)
        begin_observation(world, controller, pawn)
    elif state["phase"] == "observe_boundary":
        observe(world, pawn, elapsed)


def tick(delta_seconds):
    if state["finished"]:
        return
    try:
        tick_impl(delta_seconds)
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        if state.get("case") and state["case"]["status"] == "running":
            state["case"]["status"] = "failed"
        if state["phase"] == "cleanup":
            close_report(str(error), True)
        else:
            finish(str(error), failed=True)


def main():
    global state
    previous = getattr(unreal, SINGLETON, None)
    if previous and not previous.get("finished") and previous.get("callback") is not None:
        state = previous
        unreal.log_error("[Rogue10mOpenWorldAIBoundary] Existing validator is running; duplicate invocation ignored.")
        return
    setattr(unreal, SINGLETON, state)
    try:
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        record("no_existing_pie", game_world() is None and not levels.is_in_play_in_editor())
        record("correct_project", PROJECT.resolve() == Path("D:/Project/Rogue10m").resolve())
        record("saved_map_exists", unreal.EditorAssetLibrary.does_asset_exist(MAP))
        record("load_saved_editor_map", levels.load_level(MAP))
        editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        record("exact_editor_map", editor_world is not None and editor_world.get_path_name().split(".")[0] == MAP)
        phase("wait_pie", 90.0)
        state["callback"] = unreal.register_slate_post_tick_callback(tick)
        state["owns_pie"] = True
        levels.editor_request_begin_play()
        unreal.log("[Rogue10mOpenWorldAIBoundary] Started owned nonblocking PIE boundary regression.")
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        finish(str(error), failed=True)


if __name__ == "__main__":
    main()
