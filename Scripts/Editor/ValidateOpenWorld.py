"""Nonblocking PIE checks for the saved OpenRunMap and the real StartHub portal.

Only runtime.json is written. Editor maps, assets, navigation, profiles and
Blueprint defaults are never saved. Runtime Pawn/AI test changes end with this
validator's own PIE session. Root owns invocation of this file in the editor.
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
from OpenWorldLayout import BRIDGES, POIS, walkable_height_at  # noqa: E402


RUN_MAP = "/Game/Rogue10m/Maps/OpenRunMap"
HUB_MAP = "/Game/Rogue10m/Maps/StartHub"
WORLD_TAG = "Rogue10m.OpenWorld"
PROBE_PREFIX = WORLD_TAG + ".StreamProbe."
SINGLETON = "_rogue10m_open_world_validator"
REPORT_PATH = PROJECT / "Feature/doc/evidence/open-world-20261005/runtime.json"
RUN_SECONDS = 1200.0
GLOBAL_TIMEOUT = 600.0
WALK_SECONDS = 1.5
GROUND_TOLERANCE_CM = 150.0
state = {
    "callback": None,
    "owns_pie": False,
    "finished": False,
    "started": time.monotonic(),
    "phase": "static",
    "phase_started": time.monotonic(),
    "phase_timeout": 90.0,
    "report": {
        "status": "running",
        "hub_map": HUB_MAP,
        "run_map": RUN_MAP,
        "checks": [],
        "timer_samples": [],
        "ground_samples": [],
        "navigation_samples": [],
        "streaming_samples": [],
        "combat_samples": [],
        "limitations": [
            "Ground and walking coverage consists of six POIs and three samples on each of three bridges; it is not a full terrain traversal.",
            "Streaming unload/reentry is required for the camp sentinel on a camp-to-boss-to-camp round trip; every POI sentinel must load on arrival.",
            "Navigation paths are local to the player invoker. One authored normal monster is checked for real AI pursuit and player damage.",
            "Pawn damage immunity during traversal, isolated AI logic, and a one-point monster damage stimulus are PIE-only test adjustments.",
            "Final-boss victory, a full 20-minute timeout, all monsters, network play and packaged performance are not checked.",
        ],
    },
}


def vector_data(value):
    return [float(value.x), float(value.y), float(value.z)]


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


def phase(name, timeout=40.0):
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
    finally:
        state["report"].update(status="failed" if failed else "passed", message=message, owned_pie_cleanup_confirmed=not state["owns_pie"])
        write_report()
        log = unreal.log_error if failed else unreal.log
        log(f"[Rogue10mOpenWorldValidation] {state['report']['status'].upper()}: {message}; report={REPORT_PATH}")


def finish(message, failed=False):
    if state["finished"] or state["phase"] == "cleanup":
        return
    state["outcome"] = (message, failed)
    try:
        pawn = state.get("protected_pawn")
        if pawn is not None:
            pawn.set_editor_property("can_be_damaged", state.get("original_can_be_damaged", True))
        regeneration = state.get("regeneration")
        if regeneration is not None:
            regeneration.configure_regeneration(*state["original_regeneration"])
        brain = state.get("combat_brain")
        if brain is not None:
            brain.stop_logic("OpenWorld validation finished")
    except Exception as error:
        state["report"].setdefault("cleanup_errors", []).append(f"Runtime restore: {error}")
        state["outcome"] = (message, True)
    finally:
        # Drop Python UObject references before ending PIE so they cannot retain its world.
        for key in ("protected_pawn", "regeneration", "combat_monster", "combat_brain"):
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


def load_editor_map(path):
    record("load_editor_map", unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(path), map=path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    record("exact_editor_map", world is not None and world.get_path_name().split(".")[0] == path, map=path)
    return world


def validate_static():
    record("correct_project", PROJECT.resolve() == Path("D:/Project/Rogue10m").resolve())
    record("run_map_exists", unreal.EditorAssetLibrary.does_asset_exist(RUN_MAP))
    record("hub_map_exists", unreal.EditorAssetLibrary.does_asset_exist(HUB_MAP))
    world = load_editor_map(RUN_MAP)
    settings = world.get_world_settings()
    config = settings.get_editor_property("navigation_system_config")
    # FSoftClassPath's native BreakSoftClassPath produces the single path-string tuple item.
    nav_class = str(config.get_editor_property("navigation_system_class").to_tuple()[0]) if config else ""
    record("map_owned_navigation_config", config is not None and config.get_outer() == settings and nav_class == "/Script/Rogue10m.Rogue10mOpenWorldNavigationSystem", navigation_system_class=nav_class)
    partition = settings.get_editor_property("world_partition")
    # UE5.8 keeps bEnableStreaming protected and without a Python accessor.
    terrain_facts=json.loads(unreal.Rogue10mOpenWorldAuthoring.inspect_open_world_terrain())
    record("world_partition_streaming_enabled", partition is not None and bool(terrain_facts.get("streaming_enabled")))
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    record("single_run_player_start", len(starts) == 1, labels=[actor.get_actor_label() for actor in starts])
    state["expected_spawn"] = vector_data(starts[0].get_actor_location())
    bootstraps = [actor for actor in actors if isinstance(actor, unreal.Rogue10mOpenWorldBootstrap)]
    record("single_always_loaded_bootstrap", len(bootstraps) == 1 and not bootstraps[0].get_editor_property("is_spatially_loaded"), count=len(bootstraps))
    mode_class = settings.get_editor_property("default_game_mode")
    mode = unreal.get_default_object(mode_class) if mode_class else None
    record("run_game_mode", isinstance(mode, unreal.Rogue10mGameMode))
    record("prototype_run_duration_disabled", not mode.get_editor_property("use_prototype_run_duration"))
    load_editor_map(HUB_MAP)
    portals = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(actor, unreal.Rogue10mRunPortal)]
    record("single_hub_portal", len(portals) == 1, count=len(portals))
    target = str(portals[0].get_editor_property("target_level_name"))
    record("hub_portal_targets_open_run", target == RUN_MAP and bool(portals[0].get_editor_property("start_run_timer_on_travel")), target=target)


def context(world):
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    run = unreal.GameplayStatics.get_game_state(world)
    if not controller or not pawn or not run:
        return None
    if not isinstance(pawn, unreal.Rogue10mCharacter) or not isinstance(run, unreal.Rogue10mGameState):
        raise RuntimeError("PIE does not have the Rogue10m Pawn and GameState.")
    if pawn.get_controller() != controller or unreal.GameplayStatics.is_game_paused(world) or controller.is_move_input_ignored() or controller.is_look_input_ignored():
        raise RuntimeError("Player possession, pause state or movement input is invalid.")
    return controller, pawn, run


def require_running(run):
    remaining = float(run.get_remaining_run_seconds())
    if run.get_run_phase() != unreal.Rogue10mRunPhase.RUNNING or not run.is_run_active() or abs(float(run.get_run_duration_seconds()) - RUN_SECONDS) > 0.01 or not 0.0 < remaining <= RUN_SECONDS:
        raise RuntimeError(f"Expected a running 1200-second run; phase={run.get_run_phase()}, remaining={remaining}.")
    return remaining


def bootstrap(world):
    values = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mOpenWorldBootstrap)
    if len(values) != 1:
        raise RuntimeError(f"Expected one runtime bootstrap, found {len(values)}.")
    return values[0]


def probes(world):
    return {key: len(unreal.GameplayStatics.get_all_actors_with_tag(world, PROBE_PREFIX + key)) for key in POIS}


def ground(world, pawn, x, y, expected_z):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, expected_z + 1500.0), unreal.Vector(x, y, expected_z - 2500.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    fields = hit.to_tuple()  # Native BreakHitResult: blocking=0, impact point=5, normal=7.
    if not fields[0] or fields[7].z < 0.65:
        return None
    return float(fields[5].z)


def landing_sample(world, pawn, expected_xy=None):
    location = pawn.get_actor_location()
    z = ground(world, pawn, location.x, location.y, walkable_height_at(location.x, location.y))
    if z is None:
        return None
    half_height = pawn.get_editor_property("capsule_component").get_scaled_capsule_half_height()
    clearance = float(location.z - half_height - z)
    if clearance < -12.0:
        raise RuntimeError(f"Pawn penetrates ground by {-clearance:.1f} cm at {vector_data(location)}.")
    if expected_xy and math.hypot(location.x - expected_xy[0], location.y - expected_xy[1]) > 150.0:
        raise RuntimeError("Pawn did not arrive at the expected PlayerStart/teleport position.")
    return {"location": vector_data(location), "ground_z": z, "capsule_bottom_clearance_cm": clearance, "moving_on_ground": bool(pawn.get_editor_property("character_movement").is_moving_on_ground())}


def teleport(pawn, x, y, z):
    pawn.get_editor_property("character_movement").stop_movement_immediately()
    pawn.set_actor_location(unreal.Vector(x, y, z), False, True)
    location = pawn.get_actor_location()
    record("runtime_teleport_exact", math.hypot(location.x - x, location.y - y) < 5.0 and abs(location.z - z) < 5.0, destination=[x, y, z], actual=vector_data(location))


def local_path(world, pawn, start, end):
    # The Python generator packs bool + out FVector as FVector on success or None on failure.
    extent = unreal.Vector(250.0, 250.0, 500.0)
    a = unreal.NavigationSystemV1.project_point_to_navigation(world, start, None, None, extent)
    b = unreal.NavigationSystemV1.project_point_to_navigation(world, end, None, None, extent)
    if a is None or b is None:
        return None
    if math.hypot(a.x-start.x,a.y-start.y)>250.0 or math.hypot(b.x-end.x,b.y-end.y)>250.0:
        return None
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, pawn, None)
    if not path or not path.is_valid() or path.is_partial():
        return None
    points = path.get_editor_property("path_points")
    if len(points) < 2:
        return None
    return {"start": vector_data(a), "end": vector_data(b), "point_count": len(points), "points": [vector_data(point) for point in points], "valid": True, "partial": False}


def prepare_waypoints():
    points = [{"id": "poi_" + key, "poi": key, "x": value["x"] + 350.0, "y": value["y"],
               "direction": (0.0,1.0) if key=="crossroads" else (1.0,0.0)} for key, value in POIS.items()]
    points.append({"id": "camp_reentry", "poi": "camp", "x": POIS["camp"]["x"] + 350.0, "y": POIS["camp"]["y"], "direction": (1.0, 0.0), "reentry": True})
    for bridge in BRIDGES:
        for name, along, direction in (("center", 0.0, 1.0), ("south_end", -bridge["length"] * 0.5 + 150.0, -1.0), ("north_end", bridge["length"] * 0.5 - 150.0, 1.0)):
            points.append({"id": f"bridge_{bridge['id']}_{name}", "x": bridge["x"], "y": bridge["y"] + along, "direction": (0.0, direction), "bridge": bridge["id"]})
    state["waypoints"] = points
    state["waypoint_index"] = 0


def next_waypoint(world, pawn):
    index = state["waypoint_index"]
    if index >= len(state["waypoints"]):
        teleport(pawn, POIS["forest"]["x"] + 350.0, POIS["forest"]["y"], POIS["forest"]["placement_z"] + 500.0)
        phase("choose_encounter", 45.0)
        return
    point = state["waypoints"][index]
    expected_z = walkable_height_at(point["x"], point["y"])
    half_height = pawn.get_editor_property("capsule_component").get_scaled_capsule_half_height()
    teleport(pawn, point["x"], point["y"], expected_z + half_height + 350.0)
    state["waypoint_ground_recorded"] = False
    phase("waypoint_ready", 45.0)


def health(pawn):
    return float(pawn.get_rogue_attribute_set().get_editor_property("health").get_editor_property("current_value"))


def choose_encounter(world, controller, pawn):
    if not bootstrap(world).is_world_streaming_ready() or probes(world)["forest"] != 1:
        return
    monsters = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mBasicMonster)
    candidates = []
    for monster in monsters:
        data = monster.get_monster_data()
        tags = {str(tag) for tag in monster.get_editor_property("tags")}
        if WORLD_TAG not in tags or monster.is_dead() or not data or data.get_editor_property("monster_rank") != unreal.Rogue10mMonsterRank.NORMAL:
            continue
        location = monster.get_actor_location()
        distance = math.hypot(location.x - POIS["forest"]["x"], location.y - POIS["forest"]["y"])
        if distance <= 6000.0 and isinstance(monster.get_controller(), unreal.Rogue10mMonsterAIController):
            candidates.append((distance, monster))
    if not candidates:
        return
    monster = min(candidates, key=lambda item: item[0])[1]
    ai = monster.get_controller()
    brain = ai.get_editor_property("brain_component")
    record("authored_normal_monster_has_ai_brain", brain is not None, monster=monster.get_path_name(), monster_data=monster.get_monster_data().get_path_name(), controller_class=ai.get_class().get_path_name())
    # Isolate a single real encounter; preserve its own AI for the chase/damage phase.
    for other in monsters:
        other_ai = other.get_controller()
        if other_ai:
            other_brain = other_ai.get_editor_property("brain_component")
            if other_brain:
                other_brain.stop_logic("OpenWorld validation encounter setup")
            other_ai.stop_movement()
    state["combat_monster"] = monster
    state["combat_brain"] = brain
    location = monster.get_actor_location()
    destination = (location.x + 900.0, location.y)
    teleport(pawn, destination[0], destination[1], walkable_height_at(*destination) + 450.0)
    phase("encounter_navigation", 45.0)


def start_chase(world, controller, pawn):
    monster = state["combat_monster"]
    sample = landing_sample(world, pawn)
    if sample is None or not sample["moving_on_ground"] or sample["capsule_bottom_clearance_cm"] > 15.0 or not bootstrap(world).is_player_navigation_invoker_registered():
        return
    path = local_path(world, pawn, monster.get_actor_location(), pawn.get_actor_location())
    if path is None:
        return
    record("encounter_actual_complete_navigation_path", True, **path)
    state["combat_origin"] = vector_data(monster.get_actor_location())
    state["combat_initial_distance"] = math.hypot(monster.get_actor_location().x - pawn.get_actor_location().x, monster.get_actor_location().y - pawn.get_actor_location().y)
    record("encounter_starts_outside_melee", state["combat_initial_distance"] >= 700.0, distance_cm=state["combat_initial_distance"])
    if health(pawn) < 30.0:
        pawn.get_rogue_attribute_set().initialize_vitals(200.0, 100.0, 100.0)
        record("pie_only_low_health_test_reset", True, health=health(pawn))
    regeneration = pawn.get_component_by_class(unreal.Rogue10mVitalRegenerationComponent)
    if regeneration:
        rates = tuple(float(regeneration.get_editor_property(name)) for name in ("health_regeneration_per_second", "stamina_regeneration_per_second", "mana_regeneration_per_second"))
        state["regeneration"] = regeneration
        state["original_regeneration"] = rates
        regeneration.configure_regeneration(0.0, rates[1], rates[2])
    state["combat_initial_health"] = health(pawn)
    state["combat_chase_verified"] = False
    pawn.set_editor_property("can_be_damaged", True)
    state["combat_brain"].restart_logic()
    # Exercise the existing native damage perception event; do not set Blackboard targets or call AI decisions.
    stimulus = unreal.GameplayStatics.apply_damage(monster, 1.0, controller, pawn, unreal.DamageType)
    record("native_damage_perception_stimulus", float(stimulus) > 0.0, damage_to_monster=float(stimulus))
    phase("observe_encounter", 20.0)


def tick_impl(delta_seconds):
    if state["phase"] == "cleanup":
        if game_world() is None and not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            state["owns_pie"] = False
            close_report(*state["outcome"])
        elif time.monotonic() - state["phase_started"] > 20.0:
            state["report"].setdefault("cleanup_errors", []).append("Owned PIE did not stop within 20 seconds.")
            close_report(state["outcome"][0], True)
        return
    elapsed = time.monotonic() - state["phase_started"]
    if time.monotonic() - state["started"] > GLOBAL_TIMEOUT or elapsed > state["phase_timeout"]:
        raise RuntimeError(f"Validation timeout in {state['phase']} ({elapsed:.1f}s).")
    world = game_world()
    if world is None:
        return
    active = context(world)
    if active is None:
        return
    controller, pawn, run = active
    if state["phase"] == "wait_hub":
        if not world.get_name().endswith("StartHub"):
            raise RuntimeError("PIE did not start in StartHub.")
        record("hub_run_waiting", run.get_run_phase() == unreal.Rogue10mRunPhase.WAITING_TO_START and not run.is_run_active() and abs(run.get_remaining_run_seconds() - RUN_SECONDS) <= 0.01)
        portals = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mRunPortal)
        record("runtime_hub_portal", len(portals) == 1)
        collision = portals[0].get_editor_property("portal_collision")
        capsule = pawn.get_editor_property("capsule_component")
        record("portal_overlap_events_enabled", bool(collision.get_editor_property("generate_overlap_events")) and bool(capsule.get_editor_property("generate_overlap_events")) and collision.is_query_collision_enabled() and collision.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == unreal.CollisionResponseType.ECR_OVERLAP)
        destination = collision.get_world_location()
        state["report"]["portal_entry"] = {"method": "real_native_overlap", "destination": vector_data(destination)}
        phase("wait_run", 90.0)
        pawn.set_actor_location(destination, False, True)
        return
    if state["phase"] == "wait_run" and world.get_name().endswith("StartHub"):
        return
    if not world.get_name().endswith("OpenRunMap"):
        raise RuntimeError(f"Unexpected PIE world {world.get_path_name()}.")
    remaining = require_running(run)
    if pawn.is_dead():
        raise RuntimeError("Player died during validation.")
    if state["phase"] == "wait_run":
        record("portal_starts_1200_second_open_world_run", remaining >= RUN_SECONDS - 8.0, duration=run.get_run_duration_seconds(), arrival_remaining=remaining)
        state["timer_at_arrival"] = remaining
        state["protected_pawn"] = pawn
        state["original_can_be_damaged"] = bool(pawn.get_editor_property("can_be_damaged"))
        pawn.set_editor_property("can_be_damaged", False)
        phase("spawn_landing", 20.0)
    elif state["phase"] == "spawn_landing":
        sample = landing_sample(world, pawn, state["expected_spawn"])
        if sample and sample["moving_on_ground"] and sample["capsule_bottom_clearance_cm"] <= 15.0:
            record("actual_player_start_landing_before_teleport", True, **sample)
            state["report"]["ground_samples"].append({"id": "player_start", **sample})
            phase("initial_navigation", 45.0)
    elif state["phase"] == "initial_navigation":
        helper = bootstrap(world)
        present = probes(world)
        if not helper.is_world_streaming_ready() or not helper.is_player_navigation_invoker_registered() or present["camp"] != 1:
            return
        nav = unreal.NavigationSystemV1.get_navigation_system(world)
        # The native getter checks IsActiveTilesGenerationEnabled plus the engine's
        # actual active invoker cache; the flag itself has no public Python getter.
        record("runtime_invoker_navigation_mode", isinstance(nav, unreal.Rogue10mOpenWorldNavigationSystem) and helper.is_player_navigation_invoker_registered())
        record("initial_camp_sentinel_loaded", True, counts=present)
        state["report"]["streaming_samples"].append({"id": "initial_camp", "counts": present})
        state["report"]["timer_samples"].append({"id": "initial", "remaining": remaining})
        prepare_waypoints()
        next_waypoint(world, pawn)
    elif state["phase"] == "waypoint_ready":
        point = state["waypoints"][state["waypoint_index"]]
        helper = bootstrap(world)
        if not helper.is_world_streaming_ready() or not helper.is_player_navigation_invoker_registered():
            return
        present = probes(world)
        if point.get("poi") and present[point["poi"]] != 1:
            return
        z = ground(world, pawn, point["x"], point["y"], walkable_height_at(point["x"], point["y"]))
        if z is None:
            return
        if not state["waypoint_ground_recorded"]:
            record("waypoint_blocking_ground", abs(z - walkable_height_at(point["x"], point["y"])) <= GROUND_TOLERANCE_CM, id=point["id"], ground_z=z, expected_walkable_z=walkable_height_at(point["x"], point["y"]))
            state["waypoint_ground_recorded"] = True
        sample = landing_sample(world, pawn, (point["x"], point["y"]))
        if sample is None or not sample["moving_on_ground"] or sample["capsule_bottom_clearance_cm"] > 15.0:
            return
        end = pawn.get_actor_location() + unreal.Vector(point["direction"][0] * 600.0, point["direction"][1] * 600.0, 0.0)
        path = local_path(world, pawn, pawn.get_actor_location(), end)
        if path is None:
            return
        record("waypoint_actual_complete_navigation_path", True, id=point["id"], **path)
        state["report"]["navigation_samples"].append({"id": point["id"], **path})
        state["report"]["streaming_samples"].append({"id": point["id"], "counts": present})
        if point.get("poi") == "boss":
            record("camp_sentinel_unloaded_at_distant_boss", present["camp"] == 0, counts=present)
            bosses=[actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Rogue10mBasicMonster)
                    if actor.get_monster_data() and actor.get_monster_data().get_editor_property("monster_rank")==unreal.Rogue10mMonsterRank.FINAL_BOSS]
            record("persistent_boss_stays_grounded_before_arrival",len(bosses)==1
                   and not bosses[0].is_dead()
                   and POIS["boss"]["placement_z"]+20<bosses[0].get_actor_location().z<POIS["boss"]["placement_z"]+600,
                   locations=[vector_data(actor.get_actor_location()) for actor in bosses])
            state["camp_unload_verified"] = True
        if point.get("reentry"):
            record("camp_sentinel_loaded_again_after_unload", bool(state.get("camp_unload_verified")) and present["camp"] == 1, counts=present)
        state["walk_origin"] = vector_data(pawn.get_actor_location())
        state["walk_game_started"] = float(unreal.GameplayStatics.get_time_seconds(world))
        state["report"]["ground_samples"].append({"id": point["id"], **sample})
        phase("waypoint_walk", 10.0)
    elif state["phase"] == "waypoint_walk":
        point = state["waypoints"][state["waypoint_index"]]
        game_elapsed = float(unreal.GameplayStatics.get_time_seconds(world)) - state["walk_game_started"]
        if elapsed - state["last_sample"] >= 0.25:
            sample = landing_sample(world, pawn)
            if sample is None or not -12.0 <= sample["capsule_bottom_clearance_cm"] <= 45.0:
                raise RuntimeError(f"Lost physical ground while walking {point['id']}: {sample}.")
            state["report"]["ground_samples"].append({"id": point["id"] + "_walk", **sample})
            state["last_sample"] = elapsed
        if game_elapsed < WALK_SECONDS:
            pawn.add_movement_input(unreal.Vector(*point["direction"], 0.0), 1.0, False)
            return
        pawn.get_editor_property("character_movement").stop_movement_immediately()
        location = pawn.get_actor_location()
        origin = state["walk_origin"]
        forward = (location.x - origin[0]) * point["direction"][0] + (location.y - origin[1]) * point["direction"][1]
        record("waypoint_actual_walking", forward >= 100.0 and pawn.get_editor_property("character_movement").is_moving_on_ground(), id=point["id"], forward_displacement_cm=forward, elapsed_game_seconds=game_elapsed, elapsed_wall_seconds=elapsed, used_add_movement_input=True)
        state["waypoint_index"] += 1
        next_waypoint(world, pawn)
    elif state["phase"] == "choose_encounter":
        choose_encounter(world, controller, pawn)
    elif state["phase"] == "encounter_navigation":
        start_chase(world, controller, pawn)
    elif state["phase"] == "observe_encounter":
        monster = state["combat_monster"]
        if monster.is_dead():
            raise RuntimeError("Test monster died before the pursuit/damage check.")
        ai = monster.get_controller()
        blackboard = ai.get_component_by_class(unreal.BlackboardComponent) if ai else None
        target_is_player = bool(blackboard and blackboard.get_value_as_object("TargetActor") == pawn)
        location = monster.get_actor_location()
        distance = math.hypot(location.x - pawn.get_actor_location().x, location.y - pawn.get_actor_location().y)
        origin = state["combat_origin"]
        displacement = math.hypot(location.x - origin[0], location.y - origin[1])
        current_health = health(pawn)
        if elapsed - state["last_sample"] >= 0.25:
            state["report"]["combat_samples"].append({"elapsed_seconds": elapsed, "monster_location": vector_data(location), "distance_to_player_cm": distance, "monster_displacement_cm": displacement, "blackboard_target_is_player": target_is_player, "player_health": current_health})
            state["last_sample"] = elapsed
        if not state["combat_chase_verified"] and target_is_player and displacement >= 150.0 and state["combat_initial_distance"] - distance >= 150.0:
            record("native_monster_ai_actual_pursuit", True, displacement_cm=displacement, distance_decrease_cm=state["combat_initial_distance"] - distance, blackboard_target_is_player=True)
            state["combat_chase_verified"] = True
        if state["combat_chase_verified"] and target_is_player and state["combat_initial_health"] - current_health >= 0.1:
            record("native_monster_attack_damages_player", True, health_before=state["combat_initial_health"], health_after=current_health, player_damage_taken=state["combat_initial_health"] - current_health, directly_applied_player_damage=False)
            record("run_timer_decreased_during_validation", state["timer_at_arrival"] - remaining >= 1.0, countdown_decrease=state["timer_at_arrival"] - remaining, remaining_seconds=remaining, duration_seconds=run.get_run_duration_seconds())
            state["report"]["timer_samples"].append({"id": "final", "remaining": remaining})
            finish("Hub portal, 1200-second run, PlayerStart landing, local navigation, POI/bridge walking, camp streaming round trip and native monster pursuit/damage verified.")


def tick(delta_seconds):
    if state["finished"]:
        return
    try:
        tick_impl(delta_seconds)
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        if state["phase"] == "cleanup":
            close_report(str(error), True)
        else:
            finish(str(error), failed=True)


def main():
    global state
    previous = getattr(unreal, SINGLETON, None)
    if previous and not previous.get("finished") and previous.get("callback") is not None:
        state = previous
        unreal.log_error("[Rogue10mOpenWorldValidation] An existing validator is running; duplicate invocation ignored.")
        return
    setattr(unreal, SINGLETON, state)
    try:
        record("no_existing_pie", game_world() is None and not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor())
        validate_static()
        phase("wait_hub", 90.0)
        state["callback"] = unreal.register_slate_post_tick_callback(tick)
        state["owns_pie"] = True
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        unreal.log("[Rogue10mOpenWorldValidation] Started nonblocking hub-to-open-world PIE validation.")
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        finish(str(error), failed=True)


if __name__ == "__main__":
    main()
