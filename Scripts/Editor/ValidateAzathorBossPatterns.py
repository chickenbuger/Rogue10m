"""Disposable, asynchronous native Azathor integration QA in saved OpenRunMap.

Root owns invocation and all Editor operations. This script never saves assets.
Every player health loss comes from the native attack component, not a scripted
ApplyDamage(player). Read runtime-validation.json after the callback finishes.
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
PROFILE_PATH = OWN + "/Azathor/DA_Azathor_BossPatterns"
REPORT = PROJECT / "Feature/doc/evidence/azathor-boss-patterns-20261011/runtime-validation.json"
SINGLETON = "_rogue10m_azathor_boss_validator"
TIMEOUT = 360.0
MAX_TIMING_SAMPLE_GAP = 0.08
PROFILE_PROPERTIES = (
    "attack_range", "attack_arc_degrees", "attack_half_height", "attack_windup_seconds",
    "attack_recovery_seconds", "attack_cooldown_seconds", "attack_damage_multiplier",
    "enable_boss_patterns", "frontal_wave_range", "frontal_wave_half_width", "frontal_wave_half_height",
    "frontal_wave_windup_seconds", "frontal_wave_recovery_seconds", "frontal_wave_cooldown_seconds",
    "frontal_wave_damage_multiplier", "abyss_zone_cast_range", "abyss_zone_radius", "abyss_zone_half_height",
    "abyss_zone_windup_seconds", "abyss_zone_recovery_seconds", "abyss_zone_cooldown_seconds",
    "abyss_zone_damage_multiplier", "enrage_health_threshold", "enrage_windup_multiplier",
    "enrage_recovery_multiplier", "enrage_cooldown_multiplier", "enrage_damage_multiplier",
    "frontal_wave_telegraph_material", "abyss_zone_telegraph_material", "telegraph_depth",
    "telegraph_ground_offset", "frontal_wave_animation", "abyss_zone_animation", "idle_animation",
    "movement_animation", "attack_animation", "hit_animation", "death_animation",
)
CASES = (
    ("basic_hit", "BASIC", "hit"),
    ("frontal_hit", "FRONTAL_WAVE", "hit"),
    ("frontal_sidestep", "FRONTAL_WAVE", "side"),
    ("frontal_out_of_range", "FRONTAL_WAVE", "far"),
    ("frontal_rear", "FRONTAL_WAVE", "rear"),
    ("frontal_height", "FRONTAL_WAVE", "height"),
    ("frontal_occlusion", "FRONTAL_WAVE", "wall"),
    ("zone_hit", "ABYSS_ZONE", "hit"),
    ("zone_escape_fixed_location", "ABYSS_ZONE", "side"),
    ("zone_height", "ABYSS_ZONE", "height"),
    ("zone_occlusion", "ABYSS_ZONE", "wall"),
    ("cooldown_selection", "FRONTAL_WAVE", "cooldown"),
    ("explicit_cancel", "FRONTAL_WAVE", "cancel"),
    ("profile_removed", "ABYSS_ZONE", "profile_removed"),
    ("selected_target_death", "ABYSS_ZONE", "target_death"),
    ("invalid_selected_target_rejected", "BASIC", "invalid"),
    ("phase_above_threshold", "FRONTAL_WAVE", "above_threshold"),
    ("phase_at_threshold", "FRONTAL_WAVE", "enrage"),
    ("phase_latched_after_healing", "ABYSS_ZONE", "latched"),
    ("monster_death", "FRONTAL_WAVE", "monster_death"),
    ("destroy_endplay", "ABYSS_ZONE", "destroy"),
    ("selected_target_destroyed", "ABYSS_ZONE", "target_destroy"),
)
state = {
    "callback": None, "owns_pie": False, "finished": False,
    "started": time.monotonic(), "phase": "static", "phase_started": time.monotonic(),
    "phase_timeout": 90.0, "case_index": 0, "isolated": set(), "suppressed_spawners": set(),
    "fixtures": [],
    "report": {"status": "running", "map": MAP, "checks": [], "cases": [], "limitations": [
        "Saved boss Blueprint and profile are exercised through a native forest spawner on loaded terrain; this is not a traversal of the northern ruins.",
        "AI/input/regen suppression, fixed actor positions and increased player maximum health are disposable PIE setup. No assets are saved.",
        "Every case uses the saved production profile without writing its properties. Timings and damage are never shortened or changed. Native ConfigureProfile resets only the PIE component's selection/cooldowns between independent cases.",
        "Selected-target death uses the real PlayerState dead flag, then restores it; the defeat UI is not exercised. Boss-only damage drives phase and monster death.",
        "Shape evasion teleports the real player during windup. Physical dodge input, full balance, networking and packaged play require separate checks.",
        "Occlusion temporarily reuses a distant small StaticMeshActor in the PIE world; mesh, mobility, collision profile/enabled state and transform are restored after each case. No Editor-world actor is created or modified.",
        "Decal component/material/visibility are checked here; root records viewport evidence separately.",
        "Background CPU throttling is temporarily disabled in the Editor settings CDO with change notification suppressed, then restored on all exits without saving config. Phase timing claims require game-time observation brackets around both native state transitions no wider than 0.08 seconds; unrelated gaps inside a stable state remain diagnostic.",
    ]},
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


def value(actor, name):
    return float(actor.get_rogue_attribute_set().get_editor_property(name).get_editor_property("current_value"))


def health(actor):
    return value(actor, "health")


def capsule(actor):
    return actor.get_editor_property("capsule_component")


def write_report():
    state["report"].update(phase=state["phase"], elapsed_wall_seconds=time.monotonic() - state["started"])
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(state["report"], ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def check(name, passed, **details):
    case = state.get("case")
    state["report"]["checks"].append({"name": name, "passed": bool(passed),
                                       "case": case["name"] if case else None, **details})
    write_report()
    if not passed:
        raise RuntimeError(f"{name}: {details}")


def phase(name, timeout=30.0):
    state.update(phase=name, phase_started=time.monotonic(), phase_timeout=timeout)
    write_report()


def ground(game, pawn, x, y):
    expected = walkable_height_at(x, y)
    ignored = [pawn] + list(unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mBasicMonster))
    hit = unreal.SystemLibrary.line_trace_single(game, unreal.Vector(x, y, expected + 1500.0),
        unreal.Vector(x, y, expected - 2500.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False,
        ignored, unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    fields = hit.to_tuple()
    return float(fields[5].z) if fields[0] and fields[7].z >= 0.65 else None


def place(actor, x, y, z):
    actor.get_editor_property("character_movement").stop_movement_immediately()
    actor.set_actor_location(unreal.Vector(x, y, z), False, True)
    check("runtime_fixture_placement", distance(actor.get_actor_location(), unreal.Vector(x, y, z)) < 0.1
          and abs(actor.get_actor_location().z - z) < 0.1, actor=path_of(actor), position=[x, y, z])


def navigation_ready(game):
    helpers = unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mOpenWorldBootstrap)
    nav = unreal.NavigationSystemV1.get_navigation_system(game)
    return (len(helpers) == 1 and isinstance(nav, unreal.Rogue10mOpenWorldNavigationSystem)
            and helpers[0].is_world_streaming_ready() and helpers[0].is_player_navigation_invoker_registered())


def stop_ai(monster):
    ai = monster.get_controller()
    if ai:
        brain = ai.get_editor_property("brain_component")
        if brain and brain.is_running():
            brain.stop_logic("Azathor disposable controlled QA")
        ai.stop_movement()


def isolate(game):
    for spawner in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mMonsterSpawner):
        identity = path_of(spawner)
        if identity not in state["suppressed_spawners"]:
            spawner.set_editor_property("maintain_population", False)
            spawner.set_editor_property("max_alive_monsters", 0)
            state["suppressed_spawners"].add(identity)
    for monster in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mBasicMonster):
        if monster.is_dead():
            continue
        stop_ai(monster)
        if monster != state.get("monster"):
            monster.get_attack_pattern_component().cancel_attack()
            monster.set_editor_property("can_be_damaged", False)


def spawn_boss(game, pawn):
    point = POIS["forest"]
    x, y = point["x"] + 300.0, point["y"] + 900.0
    floor = ground(game, pawn, x, y)
    if floor is None:
        return False
    spawners = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.Rogue10mMonsterSpawner)
                if distance(actor.get_actor_location(), unreal.Vector(x, y, floor)) < 6500.0]
    if not spawners:
        return False
    spawner = min(spawners, key=lambda actor: distance(actor.get_actor_location(), unreal.Vector(x, y, floor)))
    spawner.set_editor_property("monster_class", state["blueprint"].generated_class())
    spawner.set_editor_property("project_spawn_location_to_ground", True)
    spawner.set_editor_property("ground_trace_distance", 2000.0)
    spawner.get_editor_property("spawn_area").set_box_extent(unreal.Vector(1.0, 1.0, 30.0), False)
    spawner.set_actor_location(unreal.Vector(x, y, floor + 30.0), False, True)
    spawner.set_editor_property("max_alive_monsters", max(16, spawner.get_alive_monster_count() + 2))
    try:
        monster = spawner.spawn_one_monster()
    finally:
        spawner.set_editor_property("max_alive_monsters", 0)
    check("native_owned_boss_spawned", isinstance(monster, unreal.Rogue10mBasicMonster)
          and monster.get_monster_data() == state["data"], spawner=path_of(spawner), monster=path_of(monster))
    state["monster"] = monster
    component = monster.get_attack_pattern_component()
    check("saved_profile_loaded_at_native_beginplay", component.get_profile() == state["saved_profile"]
          and component.is_enabled(), profile=path_of(component.get_profile()))
    check("production_runtime_base_stats_preserved", abs(health(monster) - 10000.0) < 0.1
          and abs(float(monster.get_editor_property("attack_damage")) - 70.0) < 0.1
          and abs(float(monster.get_editor_property("character_movement").get_editor_property("max_walk_speed")) - 240.0) < 0.1,
          health=health(monster), damage=float(monster.get_editor_property("attack_damage")))
    stop_ai(monster)
    monster.get_editor_property("character_movement").disable_movement()
    monster.get_editor_property("mesh").set_editor_property("visibility_based_anim_tick_option",
        unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    regen = monster.get_component_by_class(unreal.Rogue10mVitalRegenerationComponent)
    if regen:
        regen.configure_regeneration(0.0, 0.0, 0.0)
    monster.set_editor_property("can_be_damaged", True)
    profile = state["saved_profile"]
    state.update(component=component, profile=profile,
                 origin=[x, y, floor + capsule(monster).get_scaled_capsule_half_height() + 2.0])
    state["report"].setdefault("fixtures", []).append({"boss": path_of(monster), "spawner": path_of(spawner),
                                                     "profile": path_of(profile), "timing_overrides": {}})
    return True


def spawn_wall(game, pawn):
    source, target = state["monster"].get_actor_location(), pawn.get_actor_location()
    location = unreal.Vector((source.x + target.x) * 0.5, (source.y + target.y) * 0.5, (source.z + target.z) * 0.5)
    # Deferred spawn/component registration are not exposed in this Editor's
    # Python binding. Reuse only an existing PIE prop, never Editor spawn APIs
    # (which target GetEditorWorld even while a PIE world is running).
    candidates = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.StaticMeshActor):
        mesh = actor.get_editor_property("static_mesh_component")
        bounds = actor.get_actor_bounds(False)
        extent = bounds[1]
        if (distance(actor.get_actor_location(), source) > 2000.0
                and mesh.get_editor_property("static_mesh") is not None
                and str(mesh.get_collision_profile_name()) in ("BlockAll", "NoCollision")
                and max(extent.x, extent.y, extent.z) < 1000.0):
            candidates.append(actor)
    check("distant_small_pie_occlusion_prop_available", len(candidates) > 0)
    wall = min(candidates, key=lambda actor: path_of(actor))
    mesh = wall.get_editor_property("static_mesh_component")
    snapshot = {"transform": wall.get_actor_transform(), "mesh": mesh.get_editor_property("static_mesh"),
                "location": wall.get_actor_location(), "scale": wall.get_actor_scale3d(),
                "mobility": mesh.get_editor_property("mobility"),
                "collision_profile": mesh.get_collision_profile_name(), "collision_enabled": mesh.get_collision_enabled()}
    # Publish the snapshot before the first mutation so error cleanup can
    # restore a partially configured fixture as well.
    state.update(wall=wall, wall_snapshot=snapshot)
    mesh.set_mobility(unreal.ComponentMobility.MOVABLE)
    mesh.set_static_mesh(state["cube"])
    mesh.set_collision_profile_name("BlockAll")
    mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    wall.set_actor_location(location, False, True)
    wall.set_actor_rotation(unreal.Rotator(), True)
    wall.set_actor_scale3d(unreal.Vector(0.2, 6.0, 8.0))
    state["case"]["occlusion_reused_pie_actor"] = path_of(wall)
    # Assert the fixture really occludes Visibility, rather than trusting setup.
    hit = unreal.SystemLibrary.line_trace_single(game, source, target, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, [state["monster"]], unreal.DrawDebugTrace.NONE, True)
    check("actual_visibility_occlusion_fixture", hit is not None and hit.to_tuple()[0]
          and hit.to_tuple()[9] == wall, wall=path_of(wall))


def remove_wall():
    wall = state.pop("wall", None)
    snapshot = state.pop("wall_snapshot", None)
    if valid(wall) and snapshot:
        mesh = wall.get_editor_property("static_mesh_component")
        mesh.set_mobility(unreal.ComponentMobility.MOVABLE)
        mesh.set_static_mesh(snapshot["mesh"])
        mesh.set_collision_profile_name(snapshot["collision_profile"])
        mesh.set_collision_enabled(snapshot["collision_enabled"])
        wall.set_actor_transform(snapshot["transform"], False, True)
        mesh.set_mobility(snapshot["mobility"])
        restored_location, restored_scale = wall.get_actor_location(), wall.get_actor_scale3d()
        check("reused_pie_occlusion_prop_restored", mesh.get_editor_property("static_mesh") == snapshot["mesh"]
              and mesh.get_editor_property("mobility") == snapshot["mobility"]
              and mesh.get_collision_profile_name() == snapshot["collision_profile"]
              and mesh.get_collision_enabled() == snapshot["collision_enabled"]
              and distance(restored_location, snapshot["location"]) < 0.1
              and abs(restored_location.z - snapshot["location"].z) < 0.1
              and all(abs(a - b) < 0.001 for a, b in zip(vector_data(restored_scale), vector_data(snapshot["scale"]))),
              actor=path_of(wall))


def prepare_case(game, pawn):
    if not valid(state.get("monster")) or state["monster"].is_dead():
        if not spawn_boss(game, pawn):
            return
    name, kind, action = CASES[state["case_index"]]
    case = {"name": name, "kind": kind, "action": action, "status": "running", "samples": []}
    state["case"] = case
    state["report"]["cases"].append(case)
    state["profile"] = state["saved_profile"]
    monster, component, profile = state["monster"], state["component"], state["profile"]
    case["runtime_profile"] = path_of(profile)
    remove_wall()
    pawn.get_editor_property("player_state").set_character_dead(False)
    pawn.get_rogue_attribute_set().restore_vitals()
    pawn.set_editor_property("can_be_damaged", True)
    if action != "latched":
        monster.get_rogue_attribute_set().restore_vitals()
        component.configure_profile(profile)
    x, y, z = state["origin"]
    place(monster, x, y, z)
    # A fixed, collision-free start offset is inside basic range and both special
    # cast ranges; special tests move after selection during the real windup.
    separation = max(capsule(monster).get_scaled_capsule_radius() + capsule(pawn).get_scaled_capsule_radius() + 8.0,
                     float(profile.get_editor_property("attack_range")) * 0.75)
    check("fixture_start_is_inside_basic_range", separation <= float(profile.get_editor_property("attack_range")),
          separation=separation)
    place(pawn, x + separation, y, z)
    if action in ("above_threshold", "enrage"):
        threshold = value(monster, "max_health") * float(profile.get_editor_property("enrage_health_threshold"))
        target_health = threshold + 1.0 if action == "above_threshold" else threshold
        applied = unreal.GameplayStatics.apply_damage(monster, health(monster) - target_health, None, pawn, unreal.DamageType)
        stop_ai(monster)
        check("phase_uses_actual_boss_health_damage", abs(health(monster) - target_health) < 0.1 and float(applied) > 0.0,
              actual_health=health(monster), threshold=threshold, applied_to_player=False)
        check("phase_threshold_immediate_delegate", component.is_enraged() == (action == "enrage"),
              enraged=component.is_enraged())
    elif action == "latched":
        monster.get_rogue_attribute_set().restore_vitals()
        check("phase_remains_latched_after_healing", component.is_enraged() and health(monster) > 5000.0,
              health=health(monster), enraged=component.is_enraged())
    else:
        check("normal_phase_after_fixture_reset", not component.is_enraged())
    phase("start_case", 20.0)


def start_case(game, pawn):
    case, component, profile = state["case"], state["component"], state["profile"]
    kind, action = case["kind"], case["action"]
    target_kind = getattr(unreal.Rogue10mMonsterAttackType, kind)
    if action == "invalid":
        check("null_target_rejected_without_pending_attack", not component.try_start_attack(None)
              and not component.is_attack_active() and not component.is_telegraph_visible())
        complete_case(game, pawn)
        return
    # Selected starts advance the native sequence. Cancellation prevents warmup
    # hits; successful counts remain cumulative, so record deltas afterward.
    if action != "latched":
        warmups = {"BASIC": (), "FRONTAL_WAVE": ("BASIC",), "ABYSS_ZONE": ("BASIC", "FRONTAL_WAVE", "BASIC")}[kind]
        for expected in warmups:
            check("native_selection_warmup_started", component.try_start_attack(pawn))
            check("native_selection_warmup_type", component.get_current_attack_type() == getattr(unreal.Rogue10mMonsterAttackType, expected),
                  expected=expected, actual=str(component.get_current_attack_type()))
            component.cancel_attack()
            check("canceled_warmup_has_no_decal", not component.is_attack_active() and not component.is_telegraph_visible())
    else:
        # Prior phase-at-threshold ended FrontalWave. The next actual sequence
        # slot is Basic, then AbyssZone; wait for the basic global cooldown.
        if not component.try_start_attack(pawn):
            return
        check("latched_phase_next_basic", component.get_current_attack_type() == unreal.Rogue10mMonsterAttackType.BASIC)
        component.cancel_attack()
    state.update(start_health=health(pawn), start_hits=component.get_successful_hit_count(),
                 start_attacks=component.get_started_attack_count())
    check("requested_native_attack_started", component.try_start_attack(pawn), requested=kind)
    check("requested_native_type_selected", component.get_current_attack_type() == target_kind,
          actual=str(component.get_current_attack_type()))
    check("native_windup_delays_health_damage", component.get_pattern_state() == unreal.Rogue10mMonsterPatternState.WINDUP
          and abs(health(pawn) - state["start_health"]) < 0.01)
    check("native_reentry_rejected_during_windup", not component.try_start_attack(pawn))
    telegraph = kind != "BASIC"
    check("native_visible_telegraph_for_special_only", component.is_telegraph_visible() == telegraph,
          special=telegraph, visible=component.is_telegraph_visible())
    if telegraph:
        decals = state["monster"].get_components_by_class(unreal.DecalComponent)
        check("registered_runtime_decal_exists", len(decals) == 1 and decals[0].get_decal_material() is not None,
              decals=[path_of(item) for item in decals])
    state.update(attack_start=unreal.GameplayStatics.get_time_seconds(game),
                 locked_location=vector_data(component.get_locked_attack_location()),
                 locked_forward=vector_data(component.get_locked_attack_forward()), first_hit_time=None,
                 first_recovery_time=None, first_idle_time=None)
    prefix = {"BASIC": "attack", "FRONTAL_WAVE": "frontal_wave", "ABYSS_ZONE": "abyss_zone"}[kind]
    windup = float(profile.get_editor_property(prefix + "_windup_seconds"))
    recovery = float(profile.get_editor_property(prefix + "_recovery_seconds"))
    if kind != "BASIC" and component.is_enraged():
        windup *= float(profile.get_editor_property("enrage_windup_multiplier"))
        recovery *= float(profile.get_editor_property("enrage_recovery_multiplier"))
    state.update(expected_windup=windup, expected_recovery=recovery, wait_duration=windup + recovery + 0.35)
    case.update(health_before=state["start_health"], expected_windup_seconds=windup,
                expected_recovery_seconds=recovery, enraged=component.is_enraged(),
                locked_location=state["locked_location"], locked_forward=state["locked_forward"])
    x, y, z = state["origin"]
    if action in ("hit", "cooldown", "above_threshold", "enrage", "latched") and kind == "FRONTAL_WAVE":
        place(pawn, x + min(450.0, float(profile.get_editor_property("frontal_wave_range")) * 0.6), y, z)
    elif action == "side":
        padding = capsule(pawn).get_scaled_capsule_radius() + 200.0
        lateral = float(profile.get_editor_property("frontal_wave_half_width" if kind == "FRONTAL_WAVE" else "abyss_zone_radius")) + padding
        here = pawn.get_actor_location()
        place(pawn, here.x, here.y + lateral, here.z)
    elif action == "far":
        place(pawn, x + float(profile.get_editor_property("frontal_wave_range")) + 400.0, y, z)
    elif action == "rear":
        place(pawn, x - capsule(pawn).get_scaled_capsule_radius() - 250.0, y, z)
    elif action == "height":
        here = pawn.get_actor_location()
        height = float(profile.get_editor_property(prefix + "_half_height")) + capsule(pawn).get_scaled_capsule_half_height() + 250.0
        place(pawn, here.x, here.y, here.z + height)
    elif action == "wall":
        if kind == "FRONTAL_WAVE":
            place(pawn, x + 450.0, y, z)
        spawn_wall(game, pawn)
    elif action == "cancel":
        component.cancel_attack()
        check("explicit_cancel_cleans_pending_attack_and_decal", not component.is_attack_active() and not component.is_telegraph_visible())
    elif action == "profile_removed":
        component.configure_profile(None)
        check("profile_loss_cleans_pending_attack_and_decal", not component.is_enabled()
              and not component.is_attack_active() and not component.is_telegraph_visible())
    elif action == "target_death":
        pawn.get_editor_property("player_state").set_character_dead(True)
        check("selected_target_native_dead_state", pawn.is_dead())
    elif action == "monster_death":
        unreal.GameplayStatics.apply_damage(state["monster"], health(state["monster"]) + 1000.0, None, pawn, unreal.DamageType)
        check("native_monster_death_cleans_pending_attack_and_decal", state["monster"].is_dead()
              and not component.is_attack_active() and not component.is_telegraph_visible())
    elif action == "destroy":
        state["monster"].destroy_actor()
        check("native_endplay_fixture_destroyed", not valid(state["monster"]))
    elif action == "target_destroy":
        pawn.destroy_actor()
        check("selected_target_actor_destroyed", not valid(pawn))
    phase("observe_case", 20.0)


def observe_case(game, pawn):
    case, component = state["case"], state["component"]
    elapsed = unreal.GameplayStatics.get_time_seconds(game) - state["attack_start"]
    current = health(pawn)
    component_valid = valid(component)
    pattern = component.get_pattern_state() if component_valid else None
    previous = case["samples"][-1] if case["samples"] else None
    case["samples"].append({"elapsed_seconds": elapsed, "health": current, "state": str(pattern),
                            "visible": bool(component.is_telegraph_visible()) if component_valid else False})
    if current < state["start_health"] - 0.01 and state["first_hit_time"] is None:
        state["first_hit_time"] = elapsed
    if pattern == unreal.Rogue10mMonsterPatternState.RECOVERY and state["first_recovery_time"] is None:
        state["first_recovery_time"] = elapsed
        case["windup_to_recovery_bracket"] = {
            "lower_seconds": previous["elapsed_seconds"] if previous else 0.0, "upper_seconds": elapsed,
            "width_seconds": elapsed - (previous["elapsed_seconds"] if previous else 0.0),
            "previous_state": previous["state"] if previous else None, "observed_state": str(pattern),
        }
        check("native_reentry_rejected_during_recovery", not component.try_start_attack(pawn))
        check("telegraph_removed_on_native_execution", not component.is_telegraph_visible())
    if pattern == unreal.Rogue10mMonsterPatternState.IDLE and state["first_idle_time"] is None:
        state["first_idle_time"] = elapsed
        case["recovery_to_idle_bracket"] = {
            "lower_seconds": previous["elapsed_seconds"] if previous else 0.0, "upper_seconds": elapsed,
            "width_seconds": elapsed - (previous["elapsed_seconds"] if previous else 0.0),
            "previous_state": previous["state"] if previous else None, "observed_state": str(pattern),
        }
    if case["action"] == "side" and component_valid and pattern == unreal.Rogue10mMonsterPatternState.WINDUP:
        check_location = vector_data(component.get_locked_attack_location())
        check_forward = vector_data(component.get_locked_attack_forward())
        if any(abs(a - b) > 0.01 for a, b in zip(check_location, state["locked_location"])):
            raise RuntimeError("Telegraph center chased moving player.")
        if any(abs(a - b) > 0.01 for a, b in zip(check_forward, state["locked_forward"])):
            raise RuntimeError("Telegraph direction chased moving player.")
    if case["action"] == "target_death" and elapsed >= 0.15 and not case.get("target_guard_checked"):
        check("target_death_guard_clears_warning", not component.is_attack_active() and not component.is_telegraph_visible())
        case["target_guard_checked"] = True
    if case["action"] == "cooldown" and state["first_idle_time"] is not None:
        # This first check is made on the first observed idle tick, rather than
        # after the wait duration has already passed the global cooldown.
        check("native_global_cooldown_rejects_immediate_reentry", not component.try_start_attack(pawn))
        phase("observe_cooldown", 15.0)
        return
    if elapsed >= state["wait_duration"]:
        if case["name"] in ("frontal_hit", "phase_above_threshold", "phase_at_threshold", "phase_latched_after_healing"):
            samples = [0.0] + [row["elapsed_seconds"] for row in case["samples"]]
            max_gap = max((b - a for a, b in zip(samples, samples[1:])), default=0.0)
            case["maximum_observation_gap_seconds"] = max_gap
            windup_bracket = case.get("windup_to_recovery_bracket")
            recovery_bracket = case.get("recovery_to_idle_bracket")
            check("phase_state_transitions_have_precise_observation_brackets",
                  windup_bracket is not None and recovery_bracket is not None
                  and windup_bracket["previous_state"] == str(unreal.Rogue10mMonsterPatternState.WINDUP)
                  and recovery_bracket["previous_state"] == str(unreal.Rogue10mMonsterPatternState.RECOVERY)
                  and windup_bracket["width_seconds"] <= MAX_TIMING_SAMPLE_GAP
                  and recovery_bracket["width_seconds"] <= MAX_TIMING_SAMPLE_GAP,
                  windup_to_recovery=windup_bracket, recovery_to_idle=recovery_bracket,
                  required_maximum_bracket_seconds=MAX_TIMING_SAMPLE_GAP,
                  maximum_observation_gap_seconds=max_gap)
            # A transition happened between its last old-state observation and
            # its first new-state observation. Propagate both uncertainties into
            # an interval for the actual native recovery duration.
            case["recovery_duration_bounds_seconds"] = [
                max(0.0, recovery_bracket["lower_seconds"] - windup_bracket["upper_seconds"]),
                recovery_bracket["upper_seconds"] - windup_bracket["lower_seconds"],
            ]
        should_hit = case["action"] in ("hit", "above_threshold", "enrage", "latched")
        hit_delta = component.get_successful_hit_count() - state["start_hits"] if component_valid else None
        damage = state["start_health"] - current
        case.update(health_after=current, native_player_damage=damage, hit_delta=hit_delta,
                    first_hit_seconds=state["first_hit_time"], first_recovery_seconds=state["first_recovery_time"],
                    first_idle_seconds=state["first_idle_time"])
        if should_hit:
            check("native_single_hit_changes_real_player_health", damage > 0.1 and hit_delta == 1,
                  damage=damage, hit_delta=hit_delta, direct_player_damage=False)
            check("native_hit_observed_after_configured_windup", state["first_hit_time"] is not None
                  and state["first_hit_time"] >= state["expected_windup"] - 0.05,
                  actual=state["first_hit_time"], expected=state["expected_windup"])
            check("native_recovery_completes_after_configured_duration", state["first_idle_time"] is not None
                  and state["first_idle_time"] >= state["expected_windup"] + state["expected_recovery"] - 0.05,
                  actual=state["first_idle_time"])
            if case["action"] == "enrage":
                normal = next(row for row in state["report"]["cases"] if row["name"] == "frontal_hit")
                check("enrage_actually_shortens_windup_and_increases_damage",
                      case["first_hit_seconds"] < normal["first_hit_seconds"] - 0.05
                      and damage > normal["native_player_damage"] + 0.1,
                      normal_hit_seconds=normal["first_hit_seconds"], enraged_hit_seconds=case["first_hit_seconds"],
                      normal_damage=normal["native_player_damage"], enraged_damage=damage)
                normal_recovery = normal["first_idle_seconds"] - normal["first_recovery_seconds"]
                enraged_recovery = case["first_idle_seconds"] - case["first_recovery_seconds"]
                check("enrage_actually_shortens_recovery", enraged_recovery < normal_recovery - 0.04,
                      normal_recovery_seconds=normal_recovery, enraged_recovery_seconds=enraged_recovery)
                check("enrage_recovery_is_shorter_after_observation_uncertainty",
                      case["recovery_duration_bounds_seconds"][1] < normal["recovery_duration_bounds_seconds"][0],
                      normal_bounds_seconds=normal["recovery_duration_bounds_seconds"],
                      enraged_bounds_seconds=case["recovery_duration_bounds_seconds"])
        else:
            check("native_miss_or_cleanup_prevents_late_player_damage", abs(damage) < 0.01
                  and (hit_delta is None or hit_delta == 0), damage=damage, hit_delta=hit_delta)
        if component_valid:
            check("case_ends_without_pending_attack_or_decal", not component.is_attack_active() and not component.is_telegraph_visible())
        complete_case(game, pawn)


def observe_cooldown(game, pawn):
    component, profile = state["component"], state["profile"]
    elapsed = unreal.GameplayStatics.get_time_seconds(game) - state["attack_start"]
    ready = state["first_idle_time"] + float(profile.get_editor_property("attack_cooldown_seconds")) + 0.15
    if elapsed < ready:
        return
    check("cooldown_attack_had_one_real_hit", component.get_successful_hit_count() - state["start_hits"] == 1
          and health(pawn) < state["start_health"] - 0.1)
    x, y, z = state["origin"]
    place(pawn, x + float(profile.get_editor_property("attack_range")) * 0.75, y, z)
    # Advance Basic -> Zone -> Basic by genuine canceled starts, then prove the
    # unavailable wave is skipped to Basic within its still-running cooldown.
    for expected in ("BASIC", "ABYSS_ZONE", "BASIC", "BASIC"):
        check("cooldown_selection_start", component.try_start_attack(pawn), expected=expected)
        check("cooldown_selection_type", component.get_current_attack_type() == getattr(unreal.Rogue10mMonsterAttackType, expected),
              expected=expected, actual=str(component.get_current_attack_type()), elapsed_since_wave=elapsed)
        component.cancel_attack()
    state["case"].update(health_after=health(pawn), native_player_damage=state["start_health"] - health(pawn),
                          cooldown_selection_elapsed=elapsed)
    complete_case(game, pawn)


def complete_case(game, pawn):
    remove_wall()
    if valid(pawn):
        pawn.get_editor_property("player_state").set_character_dead(False)
        pawn.set_editor_property("can_be_damaged", False)
    state["case"]["status"] = "passed"
    state["case_index"] += 1
    write_report()
    if state["case_index"] >= len(CASES):
        finish("All owned Azathor native hit/evasion/phase/cooldown and cleanup cases passed.")
    else:
        phase("prepare_case", 30.0)


def close(message, failed):
    state["finished"] = True
    callback = state.pop("callback", None)
    if callback is not None:
        try:
            unreal.unregister_slate_post_tick_callback(callback)
        except Exception as error:
            failed = True
            state["report"].setdefault("cleanup_errors", []).append(str(error))
    settings = state.pop("performance_settings", None)
    if settings is not None:
        try:
            original = state["original_background_throttle"]
            settings.set_editor_property("bThrottleCPUWhenNotForeground", original,
                                        notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
            restored = bool(settings.get_editor_property("bThrottleCPUWhenNotForeground")) == original
            state["report"]["editor_background_throttle_restored"] = restored
            if not restored:
                raise RuntimeError("Editor background CPU throttle setting did not restore.")
        except Exception as error:
            failed = True
            state["report"].setdefault("cleanup_errors", []).append(str(error))
    state["report"].update(status="failed" if failed else "passed", message=message,
                           owned_pie_cleanup_confirmed=not state["owns_pie"])
    write_report()
    (unreal.log_error if failed else unreal.log)(f"[AzathorBossPatterns] {state['report']['status']}: {message}; {REPORT}")


def finish(message, failed=False):
    if state["finished"] or state["phase"] == "cleanup":
        return
    state["outcome"] = (message, failed)
    try:
        remove_wall()
        if valid(state.get("pawn")):
            state["pawn"].get_editor_property("player_state").set_character_dead(False)
        if valid(state.get("component")):
            state["component"].cancel_attack()
        for fixture in state["fixtures"]:
            if valid(fixture):
                fixture.destroy_actor()
    except Exception as error:
        state["report"].setdefault("cleanup_errors", []).append(str(error))
        state["outcome"] = (message, True)
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
            close("Owned PIE did not end within 20 seconds.", True)
        return
    if time.monotonic() - state["started"] > TIMEOUT or elapsed > state["phase_timeout"]:
        raise RuntimeError(f"Validation timed out in {state['phase']} after {elapsed:.1f}s.")
    game = world()
    if game is None:
        return
    check_world = game.get_name().endswith("OpenRunMap")
    if not check_world:
        raise RuntimeError("Unexpected PIE world: " + game.get_path_name())
    # The final target-loss case intentionally destroys the owned player, so
    # normal player lookup can no longer drive its post-windup observations.
    if state["phase"] == "observe_case" and state.get("case", {}).get("action") == "target_destroy":
        component = state["component"]
        attack_elapsed = unreal.GameplayStatics.get_time_seconds(game) - state["attack_start"]
        if attack_elapsed >= 0.15 and not state["case"].get("target_guard_checked"):
            check("target_loss_guard_cleans_pending_attack_and_decal", not component.is_attack_active()
                  and not component.is_telegraph_visible())
            state["case"]["target_guard_checked"] = True
        if attack_elapsed >= state["wait_duration"]:
            check("destroyed_target_produces_no_late_native_hit", component.get_successful_hit_count() == state["start_hits"]
                  and not component.is_attack_active() and not component.is_telegraph_visible())
            state["case"].update(health_after=None, health_after_reason="Owned target destroyed; hit counter and cleanup are observed.",
                                  hit_delta=component.get_successful_hit_count() - state["start_hits"])
            complete_case(game, None)
        return
    controller = unreal.GameplayStatics.get_player_controller(game, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(game, 0)
    if not controller or not pawn:
        return
    if not isinstance(pawn, unreal.Rogue10mCharacter) or pawn.get_controller() != controller:
        raise RuntimeError("Expected possessed Rogue10m player.")
    if unreal.GameplayStatics.is_game_paused(game):
        raise RuntimeError("PIE unexpectedly paused.")
    if pawn.is_dead() and state.get("case", {}).get("action") != "target_death":
        raise RuntimeError("Player died outside selected-target-death fixture.")
    isolate(game)
    if state["phase"] == "wait_pie":
        state.update(pawn=pawn, controller=controller)
        pawn.set_editor_property("can_be_damaged", False)
        controller.set_ignore_move_input(True)
        controller.set_ignore_look_input(True)
        attributes = pawn.get_rogue_attribute_set()
        attributes.initialize_vitals(2000.0, value(pawn, "max_stamina"), value(pawn, "max_mana"))
        regen = pawn.get_component_by_class(unreal.Rogue10mVitalRegenerationComponent)
        if regen:
            regen.configure_regeneration(0.0, 0.0, 0.0)
        point = POIS["forest"]
        pawn.set_actor_location(unreal.Vector(point["x"] + 1000.0, point["y"] + 900.0, point["placement_z"] + 500.0), False, True)
        phase("wait_forest", 75.0)
    elif state["phase"] == "wait_forest":
        if navigation_ready(game) and ground(game, pawn, pawn.get_actor_location().x, pawn.get_actor_location().y) is not None:
            pawn.get_editor_property("character_movement").disable_movement()
            check("loaded_forest_ground_and_native_navigation_ready", True)
            phase("prepare_case", 30.0)
    elif state["phase"] == "prepare_case":
        prepare_case(game, pawn)
    elif state["phase"] == "start_case":
        start_case(game, pawn)
    elif state["phase"] == "observe_case":
        observe_case(game, pawn)
    elif state["phase"] == "observe_cooldown":
        observe_cooldown(game, pawn)


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
        unreal.log_error("[AzathorBossPatterns] Existing validator is running; duplicate ignored.")
        return
    setattr(unreal, SINGLETON, state)
    try:
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        check("no_existing_pie", world() is None and not levels.is_in_play_in_editor())
        check("exact_project_and_saved_idle_map", PROJECT.resolve() == Path("D:/Project/Rogue10m").resolve()
              and editor.get_path_name().split(".")[0] == MAP)
        dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
        check("no_unsaved_editor_packages", len(dirty) == 0, dirty=[path_of(item) for item in dirty])
        data = unreal.load_asset(OWN + "/DA_OW_AbyssOverlordAzathor")
        profile = unreal.load_asset(PROFILE_PATH)
        check("saved_owned_boss_profile_connected", isinstance(data, unreal.Rogue10mMonsterDataAsset)
              and isinstance(profile, unreal.Rogue10mMonsterAttackPatternProfile)
              and data.get_editor_property("attack_pattern_profile") == profile)
        check("saved_boss_patterns_opted_in", profile.get_editor_property("enable_boss_patterns"))
        check("base_data_preserved", abs(float(data.get_editor_property("max_health")) - 10000.0) < 0.1
              and abs(float(data.get_editor_property("attack_damage")) - 70.0) < 0.1
              and int(data.get_editor_property("experience_reward")) == 3000
              and abs(float(data.get_editor_property("walk_speed")) - 240.0) < 0.1)
        legacy = unreal.load_asset(OWN + "/DA_OW_BasicAttack_AbyssOverlordAzathor")
        for name in PROFILE_PROPERTIES[:7] + PROFILE_PROPERTIES[-5:]:
            check("basic_profile_field_preserved", profile.get_editor_property(name) == legacy.get_editor_property(name), property=name)
        for kind in ("VerdantSlime", "Sporeling", "CrimsonSlime", "IronShell", "ThornWarden"):
            other = unreal.load_asset(OWN + "/DA_OW_" + kind)
            expected = unreal.load_asset(OWN + "/DA_OW_BasicAttack_" + kind)
            check("other_species_basic_profile_preserved", other.get_editor_property("attack_pattern_profile") == expected
                  and not expected.get_editor_property("enable_boss_patterns"), kind=kind)
        inputs = json.loads((PROJECT / "Feature/doc/evidence/open-world-monsters-20261005/inputs.json").read_text(encoding="utf-8"))
        blueprint = unreal.load_asset(inputs["species"]["AbyssOverlordAzathor"]["blueprint"].split(".")[0])
        check("saved_owned_blueprint_data", unreal.get_default_object(blueprint.generated_class()).get_monster_data() == data,
              blueprint=path_of(blueprint))
        state.update(data=data, saved_profile=profile, blueprint=blueprint,
                     cube=unreal.load_asset("/Engine/BasicShapes/Cube"))
        check("occlusion_fixture_mesh_available", state["cube"] is not None)
        settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
        state.update(performance_settings=settings,
                     original_background_throttle=bool(settings.get_editor_property("bThrottleCPUWhenNotForeground")))
        settings.set_editor_property("bThrottleCPUWhenNotForeground", False,
                                    notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        check("editor_background_cpu_throttle_temporarily_disabled",
              not settings.get_editor_property("bThrottleCPUWhenNotForeground"),
              original=state["original_background_throttle"], config_saved=False, property_notification="Never")
        phase("wait_pie", 90.0)
        state["callback"] = unreal.register_slate_post_tick_callback(tick)
        state["owns_pie"] = True
        levels.editor_request_begin_play()
        unreal.log("[AzathorBossPatterns] Started owned asynchronous native Azathor PIE validation.")
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        finish(str(error), True)


if __name__ == "__main__":
    main()
