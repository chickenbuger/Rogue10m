"""Async six-species basic monster integration check; root owns invocation.

Uses saved OpenRunMap assets in an owned PIE session, never saves assets.
Real AI pursuit and native delayed attacks are observed without direct player
damage. Missing streamed species use a loaded forest spawner's native
SpawnOneMonster on loaded ground; this is not a whole-map traversal test.
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
OWN = "/Game/Rogue10m/World/OpenWorld/Monsters"
WORLD_TAG = "Rogue10m.OpenWorld"
EVIDENCE = PROJECT / "Feature/doc/evidence/open-world-monsters-20261005"
REPORT = EVIDENCE / "runtime-validation.json"
SINGLETON = "_rogue10m_open_world_basic_monster_validator"
SPECIES = ("VerdantSlime", "Sporeling", "CrimsonSlime", "IronShell", "ThornWarden", "AbyssOverlordAzathor")
TIMEOUT = 480.0
state = {
    "callback": None, "owns_pie": False, "finished": False,
    "started": time.monotonic(), "phase": "static", "phase_started": time.monotonic(),
    "phase_timeout": 90.0, "case_index": 0, "isolated": set(), "suppressed_spawners": set(),
    "report": {
        "status": "running", "map": MAP, "checks": [], "cases": [],
        "limitations": [
            "Six saved owned classes are tested in loaded forest terrain; missing regional classes are spawned by a loaded native forest spawner in PIE, not teleported editor actors.",
            "Player input suppression, AI isolation, regeneration suppression, actor placement and off-screen mesh pose ticking are PIE-only setup.",
            "Pursuit uses a one-point damage stimulus to the selected monster and observes native targeting and movement; it issues no MoveTo or AI decision calls.",
            "The delayed damage case observes native AI combat; miss and death cancellation cases start the same native component directly for deterministic timing.",
            "No damage is applied directly to the player. Lethal damage is applied only to the selected test monster.",
            "Basic movement, Idle, attack and cancellation are checked; special patterns, balance, full terrain traversal, networking and packaged performance are outside this check.",
        ],
    },
}


def path_of(value):
    return value.get_path_name() if value else None


def valid(value):
    return value is not None and unreal.SystemLibrary.is_valid(value)


def vector_data(value):
    return [float(value.x), float(value.y), float(value.z)]


def distance(a, b):
    return math.hypot(a.x - b.x, a.y - b.y)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def health(actor):
    return float(actor.get_rogue_attribute_set().get_editor_property("health").get_editor_property("current_value"))


def capsule(actor):
    return actor.get_editor_property("capsule_component")


def write_report():
    state["report"].update(phase=state["phase"], elapsed_wall_seconds=time.monotonic() - state["started"])
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(state["report"], ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def check(name, passed, **details):
    state["report"]["checks"].append({"name": name, "passed": bool(passed), **details})
    write_report()
    if not passed:
        raise RuntimeError(f"{name}: {details}")


def phase(name, timeout=30.0):
    state.update(phase=name, phase_started=time.monotonic(), phase_timeout=timeout, last_sample=-1.0)
    write_report()


def animation_sample(actor):
    mesh = actor.get_editor_property("mesh")
    instance = mesh.get_anim_instance()
    asset = instance.get_animation_asset() if isinstance(instance, unreal.AnimSingleNodeInstance) else None
    return {"asset": path_of(asset), "position_seconds": float(mesh.get_position()),
            "mode": str(mesh.get_animation_mode())}


def animation_progressed(samples, expected):
    times = [row["position_seconds"] for row in samples if row["asset"] == expected]
    return len(times) >= 2 and any(abs(b - a) > 0.01 for a, b in zip(times, times[1:]))


def ground(game, pawn, x, y):
    expected = walkable_height_at(x, y)
    ignored = [pawn] + list(unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mBasicMonster))
    hit = unreal.SystemLibrary.line_trace_single(game,
        unreal.Vector(x, y, expected + 1500.0), unreal.Vector(x, y, expected - 2500.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignored, unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    fields = hit.to_tuple()
    return float(fields[5].z) if fields[0] and fields[7].z >= 0.65 else None


def place(game, pawn, actor, x, y):
    floor = ground(game, pawn, x, y)
    if floor is None:
        return False
    actor.get_editor_property("character_movement").stop_movement_immediately()
    actor.set_actor_location(unreal.Vector(x, y, floor + capsule(actor).get_scaled_capsule_half_height() + 2.0), False, True)
    return distance(actor.get_actor_location(), unreal.Vector(x, y, floor)) < 0.1


def on_ground(game, pawn, actor):
    location = actor.get_actor_location()
    floor = ground(game, pawn, location.x, location.y)
    if floor is None:
        return False
    clearance = location.z - capsule(actor).get_scaled_capsule_half_height() - floor
    return -2.0 <= clearance <= 15.0 and actor.get_editor_property("character_movement").is_moving_on_ground()


def navigation_ready(game):
    helpers = unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mOpenWorldBootstrap)
    nav = unreal.NavigationSystemV1.get_navigation_system(game)
    return (len(helpers) == 1 and isinstance(nav, unreal.Rogue10mOpenWorldNavigationSystem)
            and helpers[0].is_world_streaming_ready() and helpers[0].is_player_navigation_invoker_registered())


def local_path(game, pawn, monster):
    extent = unreal.Vector(100.0, 100.0, 300.0)
    a, b = monster.get_actor_location(), pawn.get_actor_location()
    start = unreal.NavigationSystemV1.project_point_to_navigation(game, a, None, None, extent)
    end = unreal.NavigationSystemV1.project_point_to_navigation(game, b, None, None, extent)
    if start is None or end is None or distance(start, a) > 20.0 or distance(end, b) > 20.0:
        return None
    route = unreal.NavigationSystemV1.find_path_to_location_synchronously(game, start, end, monster, None)
    if not route or not route.is_valid() or route.is_partial():
        return None
    points = route.get_editor_property("path_points")
    return [vector_data(point) for point in points] if len(points) >= 2 else None


def isolate(game, active=None):
    for spawner in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mMonsterSpawner):
        identity = spawner.get_path_name()
        if identity not in state["suppressed_spawners"]:
            spawner.set_editor_property("maintain_population", False)
            # A timer already started at BeginPlay reads the live population
            # limit. Zero suppresses it without touching gameplay source.
            spawner.set_editor_property("max_alive_monsters", 0)
            state["suppressed_spawners"].add(identity)
    for monster in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mBasicMonster):
        if monster == active or monster.is_dead():
            continue
        identity = monster.get_path_name()
        ai = monster.get_controller()
        if ai:
            brain = ai.get_editor_property("brain_component")
            if brain and (identity not in state["isolated"] or brain.is_running()):
                brain.stop_logic("Basic monster validation isolation")
                ai.stop_movement()
                monster.get_attack_pattern_component().cancel_attack()
        monster.set_editor_property("can_be_damaged", False)
        state["isolated"].add(identity)


def stop_selected():
    if valid(state.get("brain")):
        state["brain"].stop_logic("Basic monster validation controlled case")
    if valid(state.get("ai")):
        state["ai"].stop_movement()
    if valid(state.get("component")):
        state["component"].cancel_attack()


def choose_case(game, pawn):
    kind = SPECIES[state["case_index"]]
    row = state["inputs"]["species"][kind]
    data = unreal.load_asset(OWN + "/DA_OW_" + kind)
    candidates = [monster for monster in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mBasicMonster)
                  if not monster.is_dead() and monster.get_monster_data() == data
                  and distance(monster.get_actor_location(), pawn.get_actor_location()) < 5000.0]
    origin = None
    monster = None
    for candidate in sorted(candidates, key=lambda value: value.get_path_name()):
        location = candidate.get_actor_location()
        attack_range = float(data.get_editor_property("attack_pattern_profile").get_editor_property("attack_range"))
        if ground(game, pawn, location.x, location.y) is not None and ground(game, pawn, location.x + attack_range + 650.0, location.y) is not None:
            monster, origin = candidate, location
            break
    spawned = monster is None
    fixture_spawner = None
    if spawned:
        point = POIS["forest"]
        x, y = point["x"] + 300.0, point["y"] + 900.0
        floor = ground(game, pawn, x, y)
        if floor is None:
            return
        blueprint = unreal.load_asset(row["blueprint"].split(".")[0])
        klass = blueprint.generated_class()
        spawners = [value for value in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mMonsterSpawner)
                    if distance(value.get_actor_location(), unreal.Vector(point["x"], point["y"], floor)) < 6500.0]
        if not spawners:
            return
        spawner = min(spawners, key=lambda value: distance(value.get_actor_location(), unreal.Vector(x, y, floor)))
        fixture_spawner = spawner.get_path_name()
        spawner.set_editor_property("maintain_population", False)
        spawner.set_editor_property("monster_class", klass)
        spawner.set_editor_property("project_spawn_location_to_ground", True)
        spawner.set_editor_property("ground_trace_distance", 2000.0)
        spawner.get_editor_property("spawn_area").set_box_extent(unreal.Vector(1.0, 1.0, 30.0), False)
        spawner.set_actor_location(unreal.Vector(x, y, floor + 30.0), False, True)
        # The live native method supplies capsule ground clearance and the
        # spawner's streaming level. Only this synchronous spawn can run while
        # its population limit is temporarily raised; all edits are PIE-only.
        spawner.set_editor_property("max_alive_monsters", max(16, spawner.get_alive_monster_count() + 2))
        try:
            monster = spawner.spawn_one_monster()
        finally:
            spawner.set_editor_property("max_alive_monsters", 0)
        if not monster:
            raise RuntimeError("Native forest spawner fixture failed: " + kind)
        origin = monster.get_actor_location()
    ai = monster.get_controller()
    brain = ai.get_editor_property("brain_component") if ai else None
    component = monster.get_attack_pattern_component()
    profile = data.get_editor_property("attack_pattern_profile")
    check("owned_runtime_species_and_native_profile", isinstance(ai, unreal.Rogue10mMonsterAIController)
          and brain is not None and component.is_enabled() and component.get_profile() == profile
          and monster.get_monster_data() == data, kind=kind, monster=monster.get_path_name(),
          data=path_of(data), profile=path_of(profile), runtime_spawn_supplement=spawned,
          native_fixture_spawner=fixture_spawner)
    attack_range = float(profile.get_editor_property("attack_range"))
    mesh = monster.get_editor_property("mesh")
    check("owned_runtime_model", mesh.get_skeletal_mesh_asset() == data.get_editor_property("skeletal_mesh"),
          kind=kind, mesh=path_of(mesh.get_skeletal_mesh_asset()))
    mesh.set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    regeneration = monster.get_component_by_class(unreal.Rogue10mVitalRegenerationComponent)
    if regeneration:
        regeneration.configure_regeneration(0.0, 0.0, 0.0)
    state.update(monster=monster, ai=ai, brain=brain, component=component, profile=profile,
                 attack_range=attack_range, origin=vector_data(origin))
    case = {"kind": kind, "status": "running", "monster": monster.get_path_name(),
            "data": path_of(data), "profile": path_of(profile), "runtime_spawn_supplement": spawned,
            "native_fixture_spawner": fixture_spawner,
            "attack_range_cm": attack_range, "idle_samples": [], "pursuit_samples": [], "attack_samples": []}
    state["report"]["cases"].append(case)
    state["case"] = case
    state["isolated"].discard(monster.get_path_name())
    stop_selected()
    isolate(game, monster)
    pawn.set_editor_property("can_be_damaged", False)
    monster.set_editor_property("can_be_damaged", False)
    pawn.get_rogue_attribute_set().restore_vitals()
    check("runtime_grounded_pair_placement", place(game, pawn, monster, origin.x, origin.y)
          and place(game, pawn, pawn, origin.x + attack_range + 650.0, origin.y), kind=kind)
    phase("settle_case", 40.0)


def start_pursuit(game, pawn):
    monster, case = state["monster"], state["case"]
    initial = distance(monster.get_actor_location(), pawn.get_actor_location())
    check("pursuit_starts_outside_attack_range", initial > state["attack_range"] + 100.0,
          kind=case["kind"], distance_cm=initial)
    case.update(pursuit_origin=vector_data(monster.get_actor_location()), health_before=health(pawn), initial_distance_cm=initial)
    state.update(saw_movement=False, saw_windup=False, saw_delayed_health=False,
                 hits_before=state["component"].get_successful_hit_count(), attacks_before=state["component"].get_started_attack_count())
    pawn.set_editor_property("can_be_damaged", True)
    monster.set_editor_property("can_be_damaged", True)
    state["brain"].restart_logic()
    stimulus = unreal.GameplayStatics.apply_damage(monster, 1.0, state["controller"], pawn, unreal.DamageType)
    check("native_perception_stimulus_monster_only", float(stimulus) > 0.0,
          kind=case["kind"], applied_to_monster=float(stimulus), direct_player_damage=False)
    phase("pursuit", 30.0)


def observe_pursuit(game, pawn, elapsed):
    monster, case, component = state["monster"], state["case"], state["component"]
    if monster.is_dead():
        raise RuntimeError("Selected monster died during native pursuit.")
    bb = state["ai"].get_component_by_class(unreal.BlackboardComponent)
    target = bb is not None and bb.get_value_as_object("TargetActor") == pawn
    location = monster.get_actor_location()
    origin = case["pursuit_origin"]
    displacement = math.hypot(location.x - origin[0], location.y - origin[1])
    current_health = health(pawn)
    sample = {"elapsed_seconds": elapsed, "distance_cm": distance(location, pawn.get_actor_location()),
              "monster_displacement_cm": displacement, "target_is_player": target,
              "player_health": current_health, "state": str(component.get_pattern_state()), **animation_sample(monster)}
    if elapsed - state["last_sample"] >= 0.04:
        case["pursuit_samples"].append(sample)
        state["last_sample"] = elapsed
    movement = path_of(state["profile"].get_editor_property("movement_animation"))
    if target and displacement >= 20.0 and animation_progressed(case["pursuit_samples"], movement):
        state["saw_movement"] = True
    if component.get_pattern_state() == unreal.Rogue10mMonsterPatternState.WINDUP:
        case["attack_samples"].append(sample)
        if not state["saw_windup"]:
            state["saw_windup"] = True
            state["windup_seen_at"] = unreal.GameplayStatics.get_time_seconds(game)
            check("native_basic_attack_is_delayed", abs(current_health - case["health_before"]) < 0.01,
                  kind=case["kind"], health_before=case["health_before"], health_during_windup=current_health)
            expected = path_of(state["profile"].get_editor_property("attack_animation"))
            check("native_basic_attack_animation", sample["asset"] == expected, kind=case["kind"], expected=expected, actual=sample["asset"])
    if component.get_successful_hit_count() > state["hits_before"] and current_health < case["health_before"] - 0.1:
        expected = path_of(state["profile"].get_editor_property("attack_animation"))
        check("native_ai_pursuit_moves_and_plays_movement", state["saw_movement"] and target,
              kind=case["kind"], displacement_cm=displacement, expected_clip=movement)
        check("native_attack_clip_timeline_progresses", animation_progressed(case["attack_samples"], expected),
              kind=case["kind"], expected_clip=expected)
        delay = unreal.GameplayStatics.get_time_seconds(game) - state.get("windup_seen_at", unreal.GameplayStatics.get_time_seconds(game))
        check("native_basic_attack_real_health_damage", state["saw_windup"] and delay > 0.01,
              kind=case["kind"], health_before=case["health_before"], health_after=current_health,
              observed_delay_seconds=delay, direct_player_damage=False)
        case["native_player_damage"] = case["health_before"] - current_health
        pawn.set_editor_property("can_be_damaged", False)
        stop_selected()
        phase("setup_miss", 15.0)


def setup_close_pair(game, pawn):
    monster = state["monster"]
    origin = state["origin"]
    separation = max(capsule(monster).get_scaled_capsule_radius() + capsule(pawn).get_scaled_capsule_radius() + 5.0,
                     state["attack_range"] * 0.75)
    check("basic_attack_start_pair_in_range", separation <= state["attack_range"],
          kind=state["case"]["kind"], distance_cm=separation, range_cm=state["attack_range"])
    check("runtime_close_pair_placement", place(game, pawn, monster, origin[0], origin[1])
          and place(game, pawn, pawn, origin[0] + separation, origin[1]), kind=state["case"]["kind"])


def begin_miss(game, pawn):
    component = state["component"]
    if not component.try_start_attack(pawn):
        return
    check("miss_case_native_windup_started", component.get_pattern_state() == unreal.Rogue10mMonsterPatternState.WINDUP,
          kind=state["case"]["kind"])
    state.update(miss_health=health(pawn), miss_hits=component.get_successful_hit_count(),
                 miss_start_game_time=unreal.GameplayStatics.get_time_seconds(game))
    pawn.set_editor_property("can_be_damaged", True)
    origin = state["origin"]
    far = state["attack_range"] + capsule(pawn).get_scaled_capsule_radius() + 250.0
    check("player_left_native_hit_range_during_windup", place(game, pawn, pawn, origin[0] + far, origin[1]),
          kind=state["case"]["kind"], distance_cm=far)
    phase("observe_miss", 15.0)


def begin_death(game, pawn):
    component, monster = state["component"], state["monster"]
    if not component.try_start_attack(pawn):
        return
    check("death_case_pending_native_hit", component.get_pattern_state() == unreal.Rogue10mMonsterPatternState.WINDUP,
          kind=state["case"]["kind"])
    pawn.set_editor_property("can_be_damaged", True)
    monster.set_editor_property("can_be_damaged", True)
    state["brain"].restart_logic()
    check("death_case_ai_was_running", state["brain"].is_running(), kind=state["case"]["kind"])
    state.update(death_health=health(pawn), death_hits=component.get_successful_hit_count(),
                 death_start_game_time=unreal.GameplayStatics.get_time_seconds(game),
                 death_wait_seconds=float(state["profile"].get_editor_property("attack_windup_seconds")) + 0.4)
    lethal = health(monster) + 1000.0
    unreal.GameplayStatics.apply_damage(monster, lethal, state["controller"], pawn, unreal.DamageType)
    check("native_death_cancels_pending_basic_attack", monster.is_dead() and not component.is_attack_active(),
          kind=state["case"]["kind"])
    expected = path_of(state["profile"].get_editor_property("death_animation"))
    check("native_death_animation_started", animation_sample(monster)["asset"] == expected,
          kind=state["case"]["kind"], expected=expected, actual=animation_sample(monster)["asset"])
    state["death_ai_stopped"] = False
    phase("observe_death", 15.0)


def close(message, failed):
    state["finished"] = True
    callback = state.pop("callback", None)
    if callback is not None:
        try:
            unreal.unregister_slate_post_tick_callback(callback)
        except Exception as error:
            failed = True
            state["report"].setdefault("cleanup_errors", []).append(str(error))
    state["report"].update(status="failed" if failed else "passed", message=message,
                           owned_pie_cleanup_confirmed=not state["owns_pie"])
    write_report()
    log = unreal.log_error if failed else unreal.log
    log(f"[OpenWorldBasicMonsters] {state['report']['status']}: {message}; {REPORT}")


def finish(message, failed=False):
    if state["finished"] or state["phase"] == "cleanup":
        return
    state["outcome"] = (message, failed)
    restores = []
    pawn, controller, regen = state.get("pawn"), state.get("controller"), state.get("regeneration")
    if valid(pawn):
        restores.append(lambda: pawn.set_editor_property("can_be_damaged", state["original_can_be_damaged"]))
    if valid(controller) and state.get("suppressed_move"):
        restores.append(lambda: controller.set_ignore_move_input(False))
    if valid(controller) and state.get("suppressed_look"):
        restores.append(lambda: controller.set_ignore_look_input(False))
    if valid(regen):
        restores.append(lambda: regen.configure_regeneration(*state["original_regeneration"]))
    restores.append(stop_selected)
    for restore in restores:
        try:
            restore()
        except Exception as error:
            state["report"].setdefault("cleanup_errors", []).append(str(error))
            state["outcome"] = (message, True)
    for key in ("pawn", "controller", "regeneration", "monster", "ai", "brain", "component", "profile"):
        state.pop(key, None)
    if state["owns_pie"]:
        try:
            unreal.EditorLevelLibrary.editor_end_play()
            phase("cleanup", 20.0)
        except Exception as error:
            state["report"].setdefault("cleanup_errors", []).append(str(error))
            close(message, True)
    else:
        close(*state["outcome"])


def tick_impl(delta_seconds):
    elapsed = time.monotonic() - state["phase_started"]
    if state["phase"] == "cleanup":
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if world() is None and not levels.is_in_play_in_editor():
            state["owns_pie"] = False
            close(*state["outcome"])
        elif elapsed > 20.0:
            state["report"].setdefault("cleanup_errors", []).append("Owned PIE did not end within 20 seconds.")
            close(state["outcome"][0], True)
        return
    if time.monotonic() - state["started"] > TIMEOUT or elapsed > state["phase_timeout"]:
        raise RuntimeError(f"Validation timed out in {state['phase']} after {elapsed:.1f}s.")
    game = world()
    if game is None:
        return
    if not game.get_name().endswith("OpenRunMap"):
        raise RuntimeError("Unexpected PIE world: " + game.get_path_name())
    controller = unreal.GameplayStatics.get_player_controller(game, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(game, 0)
    if not controller or not pawn:
        return
    if not isinstance(pawn, unreal.Rogue10mCharacter) or pawn.get_controller() != controller:
        raise RuntimeError("Expected possessed Rogue10m player.")
    if unreal.GameplayStatics.is_game_paused(game) or pawn.is_dead():
        raise RuntimeError("PIE paused or player died.")
    isolate(game, state.get("monster") if state["phase"] not in ("wait_pie", "wait_forest", "choose_case") else None)
    if state["phase"] == "wait_pie":
        state.update(pawn=pawn, controller=controller,
                     original_can_be_damaged=bool(pawn.get_editor_property("can_be_damaged")))
        pawn.set_editor_property("can_be_damaged", False)
        if not controller.is_move_input_ignored():
            controller.set_ignore_move_input(True)
            state["suppressed_move"] = True
        if not controller.is_look_input_ignored():
            controller.set_ignore_look_input(True)
            state["suppressed_look"] = True
        regen = pawn.get_component_by_class(unreal.Rogue10mVitalRegenerationComponent)
        if regen:
            names = ("health_regeneration_per_second", "stamina_regeneration_per_second", "mana_regeneration_per_second")
            state.update(regeneration=regen, original_regeneration=tuple(float(regen.get_editor_property(name)) for name in names))
            regen.configure_regeneration(0.0, 0.0, 0.0)
        point = POIS["forest"]
        pawn.set_actor_location(unreal.Vector(point["x"] + 1000.0, point["y"] + 900.0, point["placement_z"] + 500.0), False, True)
        phase("wait_forest", 65.0)
    elif state["phase"] == "wait_forest":
        if navigation_ready(game) and on_ground(game, pawn, pawn):
            check("forest_streaming_ground_and_player_navigation_ready", True)
            phase("choose_case", 30.0)
    elif state["phase"] == "choose_case":
        if navigation_ready(game):
            choose_case(game, pawn)
    elif state["phase"] == "settle_case":
        if elapsed >= 0.5 and on_ground(game, pawn, pawn) and on_ground(game, pawn, state["monster"]):
            route = local_path(game, pawn, state["monster"])
            if route is not None:
                check("actual_complete_navigation_path", True, kind=state["case"]["kind"], points=route)
                phase("idle", 10.0)
    elif state["phase"] == "idle":
        state["case"]["idle_samples"].append(animation_sample(state["monster"]))
        if elapsed >= 0.8:
            expected = path_of(state["profile"].get_editor_property("idle_animation"))
            check("idle_clip_timeline_progresses", animation_progressed(state["case"]["idle_samples"], expected),
                  kind=state["case"]["kind"], expected_clip=expected)
            start_pursuit(game, pawn)
    elif state["phase"] == "pursuit":
        observe_pursuit(game, pawn, elapsed)
    elif state["phase"] == "setup_miss":
        setup_close_pair(game, pawn)
        phase("begin_miss", 10.0)
    elif state["phase"] == "begin_miss":
        if elapsed >= 0.2:
            begin_miss(game, pawn)
    elif state["phase"] == "observe_miss":
        duration = float(state["profile"].get_editor_property("attack_windup_seconds")) + float(state["profile"].get_editor_property("attack_recovery_seconds"))
        if unreal.GameplayStatics.get_time_seconds(game) - state["miss_start_game_time"] >= duration + 0.15:
            check("out_of_range_windup_attack_misses", abs(health(pawn) - state["miss_health"]) < 0.01
                  and state["component"].get_successful_hit_count() == state["miss_hits"],
                  kind=state["case"]["kind"], health_before=state["miss_health"], health_after=health(pawn))
            pawn.set_editor_property("can_be_damaged", False)
            setup_close_pair(game, pawn)
            phase("begin_death", 10.0)
    elif state["phase"] == "begin_death":
        if elapsed >= 0.2:
            begin_death(game, pawn)
    elif state["phase"] == "observe_death":
        if valid(state["monster"]):
            if valid(state["brain"]) and not state["brain"].is_running():
                state["death_ai_stopped"] = True
            if state["component"].is_attack_active():
                raise RuntimeError("Dead monster restarted a pending basic attack.")
        if unreal.GameplayStatics.get_time_seconds(game) - state["death_start_game_time"] >= state["death_wait_seconds"]:
            check("death_stops_running_ai_and_prevents_late_player_damage", state["death_ai_stopped"]
                  and abs(health(pawn) - state["death_health"]) < 0.01, kind=state["case"]["kind"],
                  health_before=state["death_health"], health_after=health(pawn))
            state["case"]["status"] = "passed"
            pawn.set_editor_property("can_be_damaged", False)
            state["case_index"] += 1
            for key in ("monster", "ai", "brain", "component", "profile"):
                state.pop(key, None)
            if state["case_index"] == len(SPECIES):
                finish("All six owned models played Idle/movement/basic attacks, pursued and natively damaged the player; misses and death cancellation passed.")
            else:
                phase("choose_case", 30.0)


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
            close(str(error), True)
        else:
            finish(str(error), True)


def main():
    global state
    previous = getattr(unreal, SINGLETON, None)
    if previous and not previous.get("finished") and previous.get("callback") is not None:
        state = previous
        unreal.log_error("[OpenWorldBasicMonsters] Existing validator is running; duplicate ignored.")
        return
    setattr(unreal, SINGLETON, state)
    try:
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        check("no_existing_pie", world() is None and not levels.is_in_play_in_editor())
        check("exact_project_and_idle_map", PROJECT.resolve() == Path("D:/Project/Rogue10m").resolve()
              and editor.get_path_name().split(".")[0] == MAP)
        state["inputs"] = json.loads((EVIDENCE / "inputs.json").read_text(encoding="utf-8"))
        state["animation_assets"] = []
        for kind in SPECIES:
            data = unreal.load_asset(OWN + "/DA_OW_" + kind)
            profile = unreal.load_asset(OWN + "/DA_OW_BasicAttack_" + kind)
            check("saved_owned_basic_profile", isinstance(data, unreal.Rogue10mMonsterDataAsset)
                  and isinstance(profile, unreal.Rogue10mMonsterAttackPatternProfile)
                  and data.get_editor_property("attack_pattern_profile") == profile, kind=kind)
            row = state["inputs"]["species"][kind]
            blueprint = unreal.load_asset(row["blueprint"].split(".")[0])
            check("saved_owned_blueprint_data", unreal.get_default_object(blueprint.generated_class()).get_monster_data() == data,
                  kind=kind, blueprint=path_of(blueprint))
            mesh = unreal.load_asset(row["mesh"]["path"])
            skeleton = mesh.get_editor_property("skeleton")
            tokens = (("idle_animation", "IdleNormal"), ("movement_animation", "walk"),
                      ("attack_animation", "Attack01"), ("hit_animation", "GetHit"), ("death_animation", "Die"))
            for name, token in tokens:
                candidates = [item for item in row["available_animations"] if token.lower() in item["path"].lower()]
                check("unambiguous_owned_animation", len(candidates) == 1, kind=kind, property=name)
                animation = unreal.load_asset(candidates[0]["path"])
                state["animation_assets"].append(animation)
                check("saved_owned_animation_reference", isinstance(animation, unreal.AnimSequence)
                      and profile.get_editor_property(name) == animation and animation.get_editor_property("skeleton") == skeleton,
                      kind=kind, property=name, asset=path_of(animation), skeleton=path_of(skeleton))
        phase("wait_pie", 90.0)
        state["callback"] = unreal.register_slate_post_tick_callback(tick)
        state["owns_pie"] = True
        levels.editor_request_begin_play()
        unreal.log("[OpenWorldBasicMonsters] Started owned async six-species PIE validation.")
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        finish(str(error), True)


if __name__ == "__main__":
    main()
