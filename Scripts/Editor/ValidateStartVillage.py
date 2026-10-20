"""Validate saved StartHub settings and real PIE walking / portal / run timing.

Run in Unreal Editor after CreateStartVillage.py. No editor assets or character
profiles are saved by this validator; only the evidence JSON is written.
"""

import json
import math
from pathlib import Path
import time
import traceback

import unreal


HUB_MAP = "/Game/Rogue10m/Maps/StartHub"
COMBAT_MAP = "/Game/FirstPerson/Lvl_FirstPerson"
VILLAGE_TAG = "Rogue10m.StartVillage"
TIMEOUT_SECONDS = 90.0
OBSERVE_SECONDS = 2.0
RUN_SECONDS = 1200.0
SPAWN_LANDING_TIMEOUT_SECONDS = 10.0
COMBAT_ARRIVAL_MIN_SECONDS = RUN_SECONDS - 5.0
PLAZA_WALK_START = (350.0, 0.0)
EVIDENCE_DIR = (
    Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    / "Feature/doc/evidence/start-village-20261005"
)
REPORT_PATH = EVIDENCE_DIR / "runtime.json"
SINGLETON_NAME = "_rogue10m_start_village_validator"
state = {
    "callback": None,
    "owns_pie": False,
    "finished": False,
    "started_at": time.monotonic(),
    "phase": "static",
    "report": {
        "status": "running",
        "hub_map": HUB_MAP,
        "combat_map": COMBAT_MAP,
        "checks": [],
        "spawn_samples": [],
        "hub_samples": [],
        "walking_samples": [],
        "combat_samples": [],
    },
}


def vector_data(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def record(name, passed, **details):
    state["report"]["checks"].append(
        {"name": name, "passed": bool(passed), **details}
    )
    if not passed:
        raise RuntimeError(f"{name}: {details}")


def write_report():
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(
        json.dumps(state["report"], ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def finish(message, failed=False):
    if state["finished"]:
        return
    state["finished"] = True
    cleanup_errors = []
    callback = state["callback"]
    state["callback"] = None
    if callback is not None:
        try:
            unreal.unregister_slate_post_tick_callback(callback)
        except Exception as error:
            cleanup_errors.append(f"Unregister callback: {error}")
    if state["owns_pie"]:
        try:
            unreal.EditorLevelLibrary.editor_end_play()
        except Exception as error:
            cleanup_errors.append(f"End PIE: {error}")
    state["report"].update(
        status="failed" if failed or cleanup_errors else "passed",
        message=message,
        final_phase=state["phase"],
        elapsed_wall_seconds=time.monotonic() - state["started_at"],
    )
    if cleanup_errors:
        state["report"]["cleanup_errors"] = cleanup_errors
    write_report()
    log = unreal.log_error if state["report"]["status"] == "failed" else unreal.log
    log(
        f"[Rogue10mStartVillageValidation] {state['report']['status'].upper()}: "
        f"{message}; report={REPORT_PATH}"
    )


def actor_tags(actor):
    return {str(tag) for tag in actor.get_editor_property("tags")}


def village_actors(world, actor_class):
    return [
        actor
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, actor_class)
        if VILLAGE_TAG in actor_tags(actor)
    ]


def validate_roles(actors):
    """Match four role markers against the creation script's saved manifest."""
    creation_path = EVIDENCE_DIR / "creation.json"
    record("creation_manifest_exists", creation_path.is_file(), path=str(creation_path))
    creation = json.loads(creation_path.read_text(encoding="utf-8-sig"))
    record(
        "creation_manifest_matches_saved_hub",
        creation.get("status") == "created" and creation.get("map") == HUB_MAP,
        status=creation.get("status"),
        map=creation.get("map"),
    )
    roles = creation.get("role_markers", creation.get("preparation_areas", []))
    if isinstance(roles, dict):
        roles = [{"role": key, **value} for key, value in roles.items()]
    record("four_role_markers_in_manifest", len(roles) == 4, count=len(roles))
    record(
        "expected_preparation_roles",
        {role.get("role") for role in roles}
        == {"Equipment", "Traits", "Difficulty", "Story"},
        roles=[role.get("role") for role in roles],
    )
    created_labels = set(creation.get("created_actor_labels", []))
    for role in roles:
        label = role.get("label", role.get("actor_label")) or (
            f"SV_{role['role']}_RoleSign"
        )
        tag = role.get("tag", role.get("actor_tag"))
        tags = set(str(value) for value in role.get("tags", []))
        if tag:
            tags.add(str(tag))
        record("role_marker_identity", bool(label or tags), marker=role)
        record("role_marker_in_creation_manifest", label in created_labels, label=label)
        matches = [
            actor
            for actor in actors
            if isinstance(actor, unreal.TextRenderActor)
            and (not label or actor.get_actor_label() == label)
            and (not tags or tags.issubset(actor_tags(actor)))
        ]
        record(
            "role_marker_present",
            len(matches) == 1,
            role=role.get("role", label),
            expected_label=label,
            expected_tags=sorted(tags),
            matches=[actor.get_actor_label() for actor in matches],
        )


def validate_static():
    record("hub_map_exists", unreal.EditorAssetLibrary.does_asset_exist(HUB_MAP))
    record("combat_map_exists", unreal.EditorAssetLibrary.does_asset_exist(COMBAT_MAP))
    record("load_hub_only", unreal.EditorLevelLibrary.load_level(HUB_MAP))
    world = unreal.EditorLevelLibrary.get_editor_world()
    record("editor_world_is_hub", bool(world) and "StartHub" in world.get_path_name())
    all_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    tagged = [actor for actor in all_actors if VILLAGE_TAG in actor_tags(actor)]
    record("village_tagged_actors", bool(tagged), count=len(tagged))
    starts = [actor for actor in tagged if isinstance(actor, unreal.PlayerStart)]
    portals = [actor for actor in tagged if isinstance(actor, unreal.Rogue10mRunPortal)]
    record("single_tagged_player_start", len(starts) == 1, count=len(starts))
    all_starts = [actor for actor in all_actors if isinstance(actor, unreal.PlayerStart)]
    record(
        "single_player_start_in_entire_hub",
        len(all_starts) == 1 and all_starts[0] == starts[0],
        count=len(all_starts),
        labels=[actor.get_actor_label() for actor in all_starts],
    )
    state["expected_spawn"] = vector_data(starts[0].get_actor_location())
    record("single_tagged_run_portal", len(portals) == 1, count=len(portals))
    record(
        "spawn_south_portal_north",
        starts[0].get_actor_location().y < 0.0
        and portals[0].get_actor_location().y > 0.0,
        player_start=vector_data(starts[0].get_actor_location()),
        portal=vector_data(portals[0].get_actor_location()),
    )
    portal = portals[0]
    target = str(portal.get_editor_property("target_level_name"))
    record("portal_targets_combat_map", target in (COMBAT_MAP, "Lvl_FirstPerson"), target=target)
    record("portal_starts_run", portal.get_editor_property("start_run_timer_on_travel") is True)
    blockers = [
        actor for actor in tagged
        if actor.get_actor_label().startswith("SV_BoundaryBlocking_")
    ]
    expected_boundaries = {
        f"SV_BoundaryBlocking_{side}" for side in ("North", "South", "West", "East")
    }
    record(
        "four_outer_boundary_blockers",
        len(blockers) >= 4
        and expected_boundaries.issubset({actor.get_actor_label() for actor in blockers}),
        labels=[actor.get_actor_label() for actor in blockers],
    )
    for blocker in blockers:
        component = blocker.get_component_by_class(unreal.StaticMeshComponent)
        record("boundary_mesh_component", bool(component), label=blocker.get_actor_label())
        record(
            "boundary_hidden_and_block_all",
            blocker.get_editor_property("hidden")
            and not component.get_editor_property("visible")
            and str(component.get_collision_profile_name()) == "BlockAll"
            and component.is_query_collision_enabled(),
            label=blocker.get_actor_label(),
            actor_hidden=bool(blocker.get_editor_property("hidden")),
            component_visible=bool(component.get_editor_property("visible")),
            collision_profile=str(component.get_collision_profile_name()),
            collision_enabled=str(component.get_collision_enabled()),
        )
    validate_roles(tagged)
    game_mode_class = world.get_world_settings().get_editor_property("default_game_mode")
    record("hub_game_mode_configured", bool(game_mode_class), game_mode=str(game_mode_class))
    game_mode = unreal.get_default_object(game_mode_class)
    record("hub_rogue_game_mode", isinstance(game_mode, unreal.Rogue10mGameMode))
    record(
        "hub_auto_run_disabled",
        not game_mode.get_editor_property("start_run_on_start_play"),
    )
    record(
        "hub_prototype_duration_disabled",
        not game_mode.get_editor_property("use_prototype_run_duration"),
    )


def gameplay_context(world):
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    game_state = unreal.GameplayStatics.get_game_state(world)
    if not controller or not pawn or not game_state:
        return None
    if not isinstance(pawn, unreal.Rogue10mCharacter):
        raise RuntimeError(f"Possessed Pawn is not Rogue10mCharacter: {pawn}")
    if not isinstance(game_state, unreal.Rogue10mGameState):
        raise RuntimeError(f"GameState is not Rogue10mGameState: {game_state}")
    if pawn.get_controller() != controller:
        raise RuntimeError("Player Pawn is not possessed by the local controller.")
    if (
        unreal.GameplayStatics.is_game_paused(world)
        or controller.is_move_input_ignored()
        or controller.is_look_input_ignored()
    ):
        raise RuntimeError("Gameplay remains paused or movement/look input is ignored.")
    return controller, pawn, game_state


def timer_sample(world, game_state):
    return {
        "world": world.get_path_name(),
        "world_seconds": float(unreal.GameplayStatics.get_time_seconds(world)),
        "phase": str(game_state.get_run_phase()),
        "active": bool(game_state.is_run_active()),
        "remaining_seconds": float(game_state.get_remaining_run_seconds()),
        "duration_seconds": float(game_state.get_run_duration_seconds()),
    }


def require_hub_waiting(world, game_state):
    sample = timer_sample(world, game_state)
    if (
        game_state.get_run_phase() != unreal.Rogue10mRunPhase.WAITING_TO_START
        or sample["active"]
        or abs(sample["remaining_seconds"] - RUN_SECONDS) > 0.01
        or abs(sample["duration_seconds"] - RUN_SECONDS) > 0.01
    ):
        raise RuntimeError(f"Hub unexpectedly started or changed the run timer: {sample}")
    return sample


def ground_height(world, pawn, x, y, trace_z):
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(x, y, trace_z),
        unreal.Vector(x, y, trace_z - 2000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False,
        [pawn],
        unreal.DrawDebugTrace.NONE,
        True,
    )
    if hit is None:
        raise RuntimeError(f"No blocking ground under ({x}, {y}, {trace_z}).")
    # HitResult uses the native BreakHitResult order in the local UE Python
    # plugin: blocking_hit=0, impact_point=5, impact_normal=7.
    fields = hit.to_tuple()
    if not fields[0] or fields[7].z < 0.7:
        raise RuntimeError("Ground trace did not find an upward-facing blocking surface.")
    return float(fields[5].z)


def begin_plaza_walk(world, pawn):
    movement = pawn.get_editor_property("character_movement")
    capsule = pawn.get_editor_property("capsule_component")
    half_height = capsule.get_scaled_capsule_half_height()
    x, y = PLAZA_WALK_START
    ground_z = ground_height(world, pawn, x, y, 1000.0)
    movement.stop_movement_immediately()
    destination = unreal.Vector(x, y, ground_z + half_height + 2.0)
    pawn.set_actor_location(destination, False, True)
    location = pawn.get_actor_location()
    record(
        "plaza_walk_start_position",
        abs(location.x - x) < 5.0 and abs(location.y - y) < 5.0
        and abs(location.z - destination.z) < 5.0,
        location=vector_data(location),
        ground_z=ground_z,
        capsule_half_height=float(half_height),
    )
    state["walk_origin"] = vector_data(location)
    state["phase_started"] = unreal.GameplayStatics.get_time_seconds(world)
    state["last_sample"] = -1.0
    state["phase"] = "walk_plaza"


def observe_spawn_landing(world, pawn, game_state):
    require_hub_waiting(world, game_state)
    now = unreal.GameplayStatics.get_time_seconds(world)
    elapsed = now - state["phase_started"]
    if elapsed > SPAWN_LANDING_TIMEOUT_SECONDS:
        raise RuntimeError("PlayerStart Pawn did not land within 10 gameplay seconds.")
    location = pawn.get_actor_location()
    expected = state["expected_spawn"]
    displacement = math.hypot(location.x - expected[0], location.y - expected[1])
    if displacement > 150.0:
        raise RuntimeError(f"Initial Pawn did not spawn at the sole PlayerStart: {vector_data(location)}")
    capsule = pawn.get_editor_property("capsule_component")
    movement = pawn.get_editor_property("character_movement")
    ground_z = ground_height(world, pawn, location.x, location.y, location.z + 250.0)
    clearance = location.z - capsule.get_scaled_capsule_half_height() - ground_z
    sample = {
        "elapsed_seconds": float(elapsed),
        "location": vector_data(location),
        "expected_player_start": expected,
        "horizontal_spawn_displacement_cm": float(displacement),
        "ground_z": ground_z,
        "capsule_bottom_clearance_cm": float(clearance),
        "moving_on_ground": bool(movement.is_moving_on_ground()),
    }
    if elapsed - state["last_spawn_sample"] >= 0.25:
        state["report"]["spawn_samples"].append(sample)
        state["last_spawn_sample"] = elapsed
    if clearance < -5.0:
        raise RuntimeError(f"PlayerStart Pawn penetrated the ground before plaza teleport: {sample}")
    if elapsed >= 0.2 and movement.is_moving_on_ground() and clearance <= 10.0:
        record("player_start_actual_landing_before_teleport", True, **sample)
        state["report"]["spawn_samples"].append(sample)
        state["phase_started"] = now
        state["phase"] = "observe_hub"


def observe_walk(world, pawn, game_state):
    require_hub_waiting(world, game_state)
    now = unreal.GameplayStatics.get_time_seconds(world)
    elapsed = now - state["phase_started"]
    if elapsed - state["last_sample"] >= 0.25:
        location = pawn.get_actor_location()
        capsule = pawn.get_editor_property("capsule_component")
        half_height = capsule.get_scaled_capsule_half_height()
        ground_z = ground_height(world, pawn, location.x, location.y, location.z + 250.0)
        bottom_clearance = location.z - half_height - ground_z
        sample = {
            "elapsed_seconds": float(elapsed),
            "location": vector_data(location),
            "ground_z": ground_z,
            "capsule_bottom_clearance_cm": float(bottom_clearance),
        }
        state["report"]["walking_samples"].append(sample)
        state["last_sample"] = elapsed
        if bottom_clearance < -5.0 or bottom_clearance > 30.0:
            raise RuntimeError(f"Pawn penetrated or fell away from plaza ground: {sample}")
    if elapsed < OBSERVE_SECONDS:
        pawn.add_movement_input(unreal.Vector(1.0, 0.0, 0.0), 1.0, False)
        return
    movement = pawn.get_editor_property("character_movement")
    movement.stop_movement_immediately()
    location = pawn.get_actor_location()
    origin = state["walk_origin"]
    displacement = math.hypot(location.x - origin[0], location.y - origin[1])
    record(
        "plaza_actual_walking",
        elapsed >= OBSERVE_SECONDS and displacement >= 150.0
        and location.x - origin[0] >= 150.0 and movement.is_moving_on_ground(),
        elapsed_seconds=float(elapsed),
        horizontal_displacement_cm=float(displacement),
        start=origin,
        end=vector_data(location),
        used_add_movement_input=True,
        moving_on_ground=bool(movement.is_moving_on_ground()),
    )
    portals = village_actors(world, unreal.Rogue10mRunPortal)
    record("runtime_portal_exists", len(portals) == 1, count=len(portals))
    collision = portals[0].get_editor_property("portal_collision")
    destination = collision.get_world_location()
    capsule = pawn.get_editor_property("capsule_component")
    response = collision.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
    # PyGenUtil::IsScriptExposedFunction excludes BlueprintGetter methods.
    # GenerateOverlapEvents is exported as the reflected component property.
    portal_generates_overlap = bool(collision.get_editor_property("generate_overlap_events"))
    pawn_generates_overlap = bool(capsule.get_editor_property("generate_overlap_events"))
    record(
        "portal_overlap_target",
        portal_generates_overlap
        and pawn_generates_overlap
        and collision.is_query_collision_enabled()
        and response == unreal.CollisionResponseType.ECR_OVERLAP,
        collision_center=vector_data(destination),
        collision_enabled=str(collision.get_collision_enabled()),
        pawn_overlap_response=str(response),
        portal_generates_overlap_events=portal_generates_overlap,
        pawn_generates_overlap_events=pawn_generates_overlap,
    )
    # Set the next phase before moving: the native overlap may schedule travel
    # immediately. Never call OpenLevel or the portal handler from this script.
    state["phase"] = "wait_combat"
    state["report"]["portal_trigger"] = {
        "method": "set_actor_location_into_collision_center",
        "target": vector_data(destination),
    }
    pawn.set_actor_location(destination, False, True)


def tick_impl(_delta_seconds):
    if time.monotonic() - state["started_at"] > TIMEOUT_SECONDS:
        raise RuntimeError(f"90-second timeout in phase {state['phase']}.")
    world = unreal.EditorLevelLibrary.get_game_world()
    if not world:
        return
    world_path = world.get_path_name()
    in_hub = "StartHub" in world_path
    in_combat = "Lvl_FirstPerson" in world_path
    if state["phase"] in ("wait_hub", "land_spawn", "observe_hub", "walk_plaza") and not in_hub:
        raise RuntimeError(f"Expected StartHub, received {world_path}.")
    if state["phase"] == "wait_combat" and in_hub:
        return
    if state["phase"] in ("wait_combat", "observe_combat") and not in_combat:
        raise RuntimeError(f"Portal traveled to an unexpected world: {world_path}.")
    context = gameplay_context(world)
    if context is None:
        return
    controller, pawn, game_state = context
    now = unreal.GameplayStatics.get_time_seconds(world)
    if state["phase"] == "wait_hub":
        sample = require_hub_waiting(world, game_state)
        record(
            "hub_possession_and_input",
            True,
            world=world_path,
            pawn_class=pawn.get_class().get_path_name(),
            controller_class=controller.get_class().get_path_name(),
            paused=False,
            move_ignored=False,
            look_ignored=False,
        )
        state["report"]["hub_samples"].append(sample)
        state["phase_started"] = now
        state["last_spawn_sample"] = -1.0
        state["phase"] = "land_spawn"
    elif state["phase"] == "land_spawn":
        observe_spawn_landing(world, pawn, game_state)
    elif state["phase"] == "observe_hub":
        sample = require_hub_waiting(world, game_state)
        if now - state["phase_started"] >= OBSERVE_SECONDS:
            state["report"]["hub_samples"].append(sample)
            record(
                "hub_timer_unchanged_for_two_seconds",
                True,
                elapsed_seconds=float(now - state["phase_started"]),
                remaining_seconds=sample["remaining_seconds"],
                run_phase=sample["phase"],
                timer_active=False,
            )
            begin_plaza_walk(world, pawn)
    elif state["phase"] == "walk_plaza":
        observe_walk(world, pawn, game_state)
    elif state["phase"] == "wait_combat":
        sample = timer_sample(world, game_state)
        record(
            "portal_arrival_starts_combat_run",
            game_state.get_run_phase() == unreal.Rogue10mRunPhase.RUNNING
            and sample["active"]
            and abs(sample["duration_seconds"] - RUN_SECONDS) <= 0.01
            and COMBAT_ARRIVAL_MIN_SECONDS <= sample["remaining_seconds"] <= RUN_SECONDS + 0.01,
            accepted_arrival_remaining_seconds=[COMBAT_ARRIVAL_MIN_SECONDS, RUN_SECONDS],
            **sample,
        )
        state["report"]["combat_samples"].append(sample)
        state["combat_initial_remaining"] = sample["remaining_seconds"]
        state["phase_started"] = now
        state["phase"] = "observe_combat"
    elif state["phase"] == "observe_combat":
        sample = timer_sample(world, game_state)
        if (
            game_state.get_run_phase() != unreal.Rogue10mRunPhase.RUNNING
            or not sample["active"]
            or abs(sample["duration_seconds"] - RUN_SECONDS) > 0.01
        ):
            raise RuntimeError(f"Combat run stopped during observation: {sample}")
        if now - state["phase_started"] >= OBSERVE_SECONDS:
            state["report"]["combat_samples"].append(sample)
            decrease = state["combat_initial_remaining"] - sample["remaining_seconds"]
            record(
                "combat_timer_decreased_after_two_seconds",
                decrease >= 1.0,
                elapsed_seconds=float(now - state["phase_started"]),
                countdown_decrease_seconds=float(decrease),
                remaining_seconds=sample["remaining_seconds"],
            )
            finish("Static hub, 2s waiting, ground walking, real portal overlap, and combat timer verified.")


def tick(delta_seconds):
    if state["finished"]:
        return
    try:
        tick_impl(delta_seconds)
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        finish(str(error), failed=True)


def main():
    global state
    previous = getattr(unreal, SINGLETON_NAME, None)
    if previous and not previous.get("finished") and previous.get("callback") is not None:
        # Re-executing a file can reuse its globals. Restore the live state so
        # the already-registered function cannot accidentally use a new dict.
        state = previous
        unreal.log_error(
            "[Rogue10mStartVillageValidation] A validator is already running; duplicate invocation ignored."
        )
        return
    setattr(unreal, SINGLETON_NAME, state)
    try:
        record("no_existing_pie_session", unreal.EditorLevelLibrary.get_game_world() is None)
        validate_static()
        state["phase"] = "wait_hub"
        write_report()
        state["callback"] = unreal.register_slate_post_tick_callback(tick)
        state["owns_pie"] = True
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        unreal.log("[Rogue10mStartVillageValidation] Started asynchronous StartHub PIE validation.")
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        finish(str(error), failed=True)


if __name__ == "__main__":
    main()
