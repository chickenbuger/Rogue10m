"""Run an asynchronous PIE smoke test for Menu -> Lobby -> StartHub travel."""

import time
import unreal


TIMEOUT_SECONDS = 45.0
state = {
    "started_at": time.time(),
    "phase": "wait_menu",
    "callback": None,
}


def finish(message, failed=False):
    callback = state.get("callback")
    if callback is not None:
        unreal.unregister_slate_post_tick_callback(callback)
        state["callback"] = None
    if failed:
        unreal.log_error(f"[Rogue10mMenuMapPIE] FAILED: {message}")
    else:
        unreal.log(f"[Rogue10mMenuMapPIE] PASSED: {message}")
    unreal.EditorLevelLibrary.editor_end_play()


def tick_impl(_delta_seconds):
    if time.time() - state["started_at"] > TIMEOUT_SECONDS:
        finish(f"Timed out in phase {state['phase']}", failed=True)
        return

    world = unreal.EditorLevelLibrary.get_game_world()
    if not world:
        return
    world_path = world.get_path_name()
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    if not controller:
        return

    if state["phase"] == "wait_menu":
        if "L_Menu" not in world_path:
            finish(f"PIE started in unexpected world: {world_path}", failed=True)
            return
        controller.open_character_lobby_from_main_menu()
        if not controller.is_character_lobby_visible():
            finish("Character Lobby did not open.", failed=True)
            return
        controller.enter_selected_character()
        state["phase"] = "wait_hub"
        return

    if state["phase"] == "wait_hub" and "StartHub" in world_path:
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        paused = unreal.GameplayStatics.is_game_paused(world)
        move_ignored = controller.is_move_input_ignored()
        look_ignored = controller.is_look_input_ignored()
        if not pawn:
            finish("Gameplay world has no possessed Pawn.", failed=True)
            return
        if paused or move_ignored or look_ignored:
            finish(
                "Gameplay input was not restored: "
                f"Paused={paused} MoveIgnored={move_ignored} "
                f"LookIgnored={look_ignored}",
                failed=True,
            )
            return
        game_state = unreal.GameplayStatics.get_game_state(world)
        if not game_state or not isinstance(game_state, unreal.Rogue10mGameState):
            finish("Starting hub has no Rogue10mGameState.", failed=True)
            return
        if (
            game_state.get_run_phase() != unreal.Rogue10mRunPhase.WAITING_TO_START
            or game_state.is_run_active()
            or abs(
                game_state.get_remaining_run_seconds()
                - game_state.get_run_duration_seconds()
            ) > 0.01
        ):
            finish(
                "Starting hub unexpectedly started the combat run: "
                f"Phase={game_state.get_run_phase()} "
                f"Remaining={game_state.get_remaining_run_seconds()}",
                failed=True,
            )
            return
        finish(
            f"World={world_path} Pawn={pawn.get_class().get_name()} "
            "MoveIgnored=False LookIgnored=False Paused=False "
            "RunPhase=WaitingToStart TimerStarted=False"
        )



def tick(delta_seconds):
    try:
        tick_impl(delta_seconds)
    except Exception as error:
        finish(f"Validator exception: {error}", failed=True)

state["callback"] = unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(
    unreal.LevelEditorSubsystem
).editor_request_begin_play()
unreal.log("[Rogue10mMenuMapPIE] Started Menu Map PIE smoke test.")
