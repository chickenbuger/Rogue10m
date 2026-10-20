"""Focused, disposable StartHub PIE checks for dialogue and real supply grants.

Root owns invocation. No Editor asset/map is modified or saved. Fixture-only
inventory contents and reward definitions live inside this validator's PIE.
"""
import json
from pathlib import Path
import time
import traceback
import uuid

import unreal

PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Feature/doc/evidence/open-world-monsters-20261005/tutorial-runtime.json"
SINGLETON = "_rogue10m_tutorial_npc_validator"
state = {
    "callback": None,
    "owns_pie": False,
    "finished": False,
    "started": time.monotonic(),
    "phase": "wait_pie",
    "report": {"status": "running", "checks": [], "limitations": [
        "This fixture calls the real reflected dialogue and inventory APIs; physical F-key input and Korean text layout also require a visual check.",
        "Receipts use PlayerState plus a GameInstance supply record across the real portal, not permanent character-profile saves. Network play is outside this single-player prototype check.",
        "Runtime fixtures are disposable PIE data; a small damage stimulus verifies actual potion use before real portal travel. No assets or maps are saved.",
    ]},
}


def write_report():
    state["report"]["phase"] = state["phase"]
    state["report"]["elapsed_seconds"] = time.monotonic() - state["started"]
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(state["report"], ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def check(name, passed, **details):
    state["report"]["checks"].append({"name": name, "passed": bool(passed), **details})
    write_report()
    if not passed:
        raise RuntimeError(f"{name}: {details}")


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def item_count(inventory, item):
    return sum(int(entry.get_editor_property("quantity"))
               for container in inventory.get_inventory_containers()
               for entry in container.get_editor_property("entries")
               if entry.get_editor_property("item_data") == item)


def set_empty_inventory(inventory):
    # InstanceId and the owning Containers array are deliberately read-only.
    # Use the actual inventory removal API, never write reflected grid structs.
    removals = [(index, entry.get_editor_property("instance_id"))
                for index, container in enumerate(inventory.get_inventory_containers())
                for entry in container.get_editor_property("entries")]
    for index, instance_id in removals:
        before = total_entry_count(inventory)
        inventory.remove_grid_item(index, instance_id)
        if total_entry_count(inventory) != before - 1:
            raise RuntimeError("Native RemoveGridItem failed to remove a fixture entry.")
    if total_entry_count(inventory):
        raise RuntimeError("Inventory did not empty through native removals.")


def total_entry_count(inventory):
    return sum(len(container.get_editor_property("entries"))
               for container in inventory.get_inventory_containers())


def prepare_fixture(world, pawn, npc):
    inventory = pawn.get_editor_property("inventory_component")
    data = npc.get_editor_property("tutorial_data")
    check("authored_tutorial_data_loaded", isinstance(data, unreal.Rogue10mTutorialDataAsset))
    item = data.get_editor_property("starting_item")
    quantity = int(data.get_editor_property("starting_item_quantity"))
    check("real_starting_item_and_stack_valid", isinstance(item, unreal.Rogue10mItemDataAsset)
          and 0 < quantity <= item.get_editor_property("max_stack_size"),
          item=item.get_path_name() if item else None, quantity=quantity)
    check("authored_reward_id_not_empty", str(data.get_editor_property("reward_id")) not in ("None", ""))
    check("npc_is_not_damageable", not npc.get_editor_property("can_be_damaged"))
    check("npc_tick_disabled", not npc.is_actor_tick_enabled())
    mesh = npc.get_editor_property("guide_mesh")
    check("guide_mesh_does_not_block_player", mesh.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION)
    mesh_asset = mesh.get_skeletal_mesh_asset()
    idle = mesh.get_editor_property("animation_data").get_editor_property("anim_to_play")
    check("npc_has_mesh_and_idle", mesh_asset is not None and idle is not None,
          mesh=mesh_asset.get_path_name() if mesh_asset else None,
          idle=idle.get_path_name() if idle else None)
    state.update({"pawn": pawn, "npc": npc, "inventory": inventory, "original_data": data,
                  "original_location": pawn.get_actor_location(),
                  "item": item, "quantity": quantity})
    fixture_data = unreal.new_object(unreal.Rogue10mTutorialDataAsset, outer=npc)
    for name in ("guide_name", "guide_role", "welcome_text", "reward_received_text", "reward_already_received_text",
                 "starting_item", "starting_item_quantity", "guide_skeletal_mesh", "guide_idle_animation"):
        fixture_data.set_editor_property(name, data.get_editor_property(name))
    fixture_data.set_editor_property("reward_id", "ValidationSupply_" + uuid.uuid4().hex)
    npc.set_editor_property("tutorial_data", fixture_data)
    state["fixture_data"] = fixture_data
    set_empty_inventory(inventory)
    npc.close_dialogue()
    check("claim_requires_open_dialogue", not npc.claim_starting_supplies())
    radius = float(npc.get_editor_property("interaction_radius"))
    location = npc.get_actor_location()
    pawn.get_editor_property("character_movement").stop_movement_immediately()
    pawn.set_actor_location(unreal.Vector(location.x + radius + 1300, location.y, location.z + 100), False, True)
    check("dialogue_denied_out_of_range", not npc.begin_dialogue(pawn))
    pawn.set_actor_location(unreal.Vector(location.x + 150, location.y, location.z + 100), False, True)
    state["phase"] = "nearby"
    state["phase_started"] = time.monotonic()


def begin_dialogue_checks(world, pawn, npc):
    controller = pawn.get_controller()
    controller.set_ignore_move_input(True)
    try:
        check("dialogue_denied_when_movement_input_blocked", not npc.begin_dialogue(pawn))
    finally:
        controller.set_ignore_move_input(False)
    check("dialogue_opens_nearby", npc.begin_dialogue(pawn))
    check("dialogue_open_state", npc.is_dialogue_open())
    widget = npc.get_tutorial_widget()
    check("native_dialogue_ui_created", isinstance(widget, unreal.Rogue10mTutorialWidget))
    check("dialogue_text_visible", bool(str(widget.get_displayed_dialogue())))
    check("dialogue_keeps_move_input", not controller.is_move_input_ignored())
    check("dialogue_keeps_look_input", not controller.is_look_input_ignored())
    check("dialogue_does_not_pause_game", not unreal.GameplayStatics.is_game_paused(world))
    check("receipt_initially_unclaimed", not npc.has_claimed_reward(pawn))
    # Display is verified separately by the real F-key viewport screenshot.
    # UUserWidget's public cached geometry can remain zero even while its
    # native child tree is visibly painted, so it is not a valid display gate.
    exercise_supply_grant(world, pawn, npc)


def exercise_supply_grant(world, pawn, npc):
    inventory, item, quantity = state["inventory"], state["item"], state["quantity"]
    before = item_count(inventory, item)
    check("real_inventory_grant_succeeds", npc.claim_starting_supplies())
    after = item_count(inventory, item)
    check("exact_starting_quantity_added", after - before == quantity, before=before, after=after, quantity=quantity)
    check("playerstate_receipt_recorded", npc.has_claimed_reward(pawn))
    check("grant_confirmation_message", bool(str(npc.get_dialogue_status())), message=str(npc.get_dialogue_status()))
    check("repeat_grant_denied", not npc.claim_starting_supplies())
    check("repeat_grant_keeps_inventory", item_count(inventory, item) == after)
    npc.close_dialogue()
    check("dialogue_closes", not npc.is_dialogue_open())
    check("return_visit_can_talk", npc.begin_dialogue(pawn))
    check("return_visit_still_cannot_repeat_grant", not npc.claim_starting_supplies())
    # An unclaimed reward with a full inventory must remain claimable after space is freed.
    fixture_data = state["fixture_data"]
    fixture_data.set_editor_property("reward_id", "ValidationFullInventory_" + uuid.uuid4().hex)
    set_empty_inventory(inventory)
    # Use the actual supplied potion asset, whose authored footprint is 1x1.
    # Item definitions are EditDefaultsOnly; no instance writes or temporary
    # stand-in Item Data Assets are needed to exercise native inventory filling.
    filler = item
    footprint = filler.get_clamped_inventory_size()
    check("authored_potion_is_one_cell", footprint.x == 1 and footprint.y == 1,
          footprint=[int(footprint.x), int(footprint.y)])
    capacity = sum(container.get_editor_property("grid_size").x
                   * container.get_editor_property("grid_size").y
                   for container in inventory.get_inventory_containers())
    if capacity <= 0 or capacity > 4000:
        raise RuntimeError(f"Unexpected fixture inventory capacity: {capacity}")
    for _ in range(capacity + 1):
        before = total_entry_count(inventory)
        inventory.try_add_grid_item(filler, 1)
        after = total_entry_count(inventory)
        if after == before:
            break
        if after != before + 1:
            raise RuntimeError("Native TryAddGridItem produced an unexpected fixture entry count.")
    check("native_api_fills_inventory", total_entry_count(inventory) == capacity,
          filled_cells=total_entry_count(inventory), capacity=capacity)
    check("full_inventory_grant_denied", not npc.claim_starting_supplies())
    check("full_inventory_does_not_consume_receipt", not npc.has_claimed_reward(pawn))
    check("full_inventory_explained", "공간" in str(npc.get_dialogue_status()), message=str(npc.get_dialogue_status()))
    set_empty_inventory(inventory)
    check("grant_retries_after_space_freed", npc.claim_starting_supplies())
    check("retry_adds_exact_quantity", item_count(inventory, item) == quantity)
    check("retry_records_receipt_once", npc.has_claimed_reward(pawn))
    location = npc.get_actor_location()
    pawn.set_actor_location(unreal.Vector(location.x + float(npc.get_editor_property("detection_radius")) + 1000,
                                          location.y, location.z + 100), False, True)
    state["phase"] = "out_of_range"
    state["phase_started"] = time.monotonic()


def restore_fixture():
    if state.get("traveled"):
        # Source actors were destroyed by actual hard travel. The owned PIE
        # session is discarded instead of writing through invalid wrappers.
        return
    if state.get("npc") and unreal.SystemLibrary.is_valid(state["npc"]):
        state["npc"].close_dialogue()
        state["npc"].set_editor_property("tutorial_data", state["original_data"])
    if state.get("pawn") and unreal.SystemLibrary.is_valid(state["pawn"]):
        state["pawn"].get_editor_property("character_movement").stop_movement_immediately()
        state["pawn"].set_actor_location(state["original_location"], False, True)


def finish(failed=False, message="Dialogue and real one-time supply grant verified."):
    if state["finished"]:
        return
    state["finished"] = True
    handle = state.get("callback")
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
        state["callback"] = None
    try:
        restore_fixture()
    except Exception:
        failed = True
        state["report"]["restore_exception"] = traceback.format_exc()
    state["report"]["status"] = "failed" if failed else "passed"
    state["report"]["message"] = message
    write_report()
    if state["owns_pie"]:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    unreal.log("[TutorialNPCValidation] " + state["report"]["status"] + ": " + message)


def tick(_delta):
    try:
        if state["finished"]:
            return
        if time.monotonic() - state["started"] > 180:
            raise RuntimeError("Tutorial NPC validation timed out.")
        world = game_world()
        if not world:
            return
        pawn = unreal.GameplayStatics.get_player_character(world, 0)
        if not pawn or not isinstance(pawn, unreal.Rogue10mCharacter):
            return
        if "StartHub" not in world.get_path_name() and state["phase"] not in ("wait_open_world", "observe_open_world"):
            raise RuntimeError("Tutorial NPC validation requires StartHub PIE before portal travel.")
        game_state = unreal.GameplayStatics.get_game_state(world)
        if not isinstance(game_state, unreal.Rogue10mGameState):
            return
        if state["phase"] == "wait_pie":
            if float(unreal.GameplayStatics.get_time_seconds(world)) < 2.0:
                return
            check("hub_waiting_before_dialogue", game_state.get_run_phase() == unreal.Rogue10mRunPhase.WAITING_TO_START)
            state["initial_timer"] = float(game_state.get_remaining_run_seconds())
            guides = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mTutorialNPC)
            check("exactly_one_authored_tutorial_guide", len(guides) == 1, count=len(guides))
            prepare_fixture(world, pawn, guides[0])
        elif state["phase"] == "nearby" and time.monotonic() - state["phase_started"] > 0.35:
            begin_dialogue_checks(world, pawn, state["npc"])
        elif state["phase"] == "out_of_range" and time.monotonic() - state["phase_started"] > 0.35:
            npc = state["npc"]
            check("leaving_range_closes_dialogue", not npc.is_dialogue_open())
            check("out_of_range_cannot_claim", not npc.claim_starting_supplies())
            check("hub_remains_waiting", game_state.get_run_phase() == unreal.Rogue10mRunPhase.WAITING_TO_START)
            check("hub_timer_unchanged", abs(game_state.get_remaining_run_seconds() - state["initial_timer"]) < 0.01,
                  before=state["initial_timer"], after=float(game_state.get_remaining_run_seconds()))
            check("player_healthy_after_dialogue", not pawn.is_dead())
            # Return near the guide, consume one of its actual grant stack, then
            # pass through the real collision portal. Source filler/test grants
            # removed earlier are captured as zero and must not reappear.
            location = npc.get_actor_location()
            pawn.set_actor_location(unreal.Vector(location.x + 150, location.y, location.z + 100), False, True)
            inventory = state["inventory"]
            item, quantity = state["item"], state["quantity"]
            check("portal_test_has_two_potions", quantity >= 2 and item_count(inventory, item) == quantity)
            damage = unreal.GameplayStatics.apply_damage(pawn, 10.0, pawn.get_controller(), npc, unreal.DamageType)
            check("potion_test_real_damage_applied", float(damage) > 0.0, damage=float(damage))
            entry_to_use = None
            for index, container in enumerate(inventory.get_inventory_containers()):
                for entry in container.get_editor_property("entries"):
                    if entry.get_editor_property("item_data") == item:
                        entry_to_use = (index, entry.get_editor_property("instance_id"))
                        break
            check("granted_potion_stack_located", entry_to_use is not None)
            check("one_granted_potion_really_consumed", inventory.try_use_grid_consumable(*entry_to_use))
            check("one_granted_potion_remaining", item_count(inventory, item) == quantity - 1)
            state["source_remaining"] = quantity - 1
            state["source_guid"] = str(entry_to_use[1])
            portals = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Rogue10mRunPortal)
            check("real_start_hub_portal_found", len(portals) == 1)
            portal = portals[0]
            check("portal_targets_open_world", "OpenRunMap" in str(portal.get_editor_property("target_level_name")))
            destination = portal.get_editor_property("portal_collision").get_world_location()
            state["phase"] = "wait_open_world"
            state["phase_started"] = time.monotonic()
            state["traveled"] = True
            pawn.get_editor_property("character_movement").stop_movement_immediately()
            pawn.set_actor_location(destination, False, True)
        elif state["phase"] == "wait_open_world":
            if "OpenRunMap" not in world.get_path_name():
                return
            if game_state.get_run_phase() != unreal.Rogue10mRunPhase.RUNNING:
                return
            inventory = pawn.get_editor_property("inventory_component")
            item = state["item"]
            # Existing prototype setup creates five copies of stackable items.
            # The NPC's tracked remaining stack must arrive as an additional,
            # independent one-unit entry, not a new two-unit reward.
            entries = [entry for container in inventory.get_inventory_containers()
                       for entry in container.get_editor_property("entries")
                       if entry.get_editor_property("item_data") == item]
            quantities = [int(entry.get_editor_property("quantity")) for entry in entries]
            check("real_portal_arrived_open_world", True, world=world.get_path_name())
            check("remaining_supply_survives_hard_travel", state["source_remaining"] in quantities,
                  source_quantity=state["source_remaining"], destination_stack_quantities=quantities,
                  source_guid=state["source_guid"])
            check("consumed_potion_not_recreated", quantities.count(state["source_remaining"]) == 1
                  and state["quantity"] not in quantities, destination_stack_quantities=quantities)
            check("run_starts_only_after_portal", game_state.is_run_active()
                  and abs(game_state.get_run_duration_seconds() - 1200.0) < 0.01)
            state["arrival_timer"] = float(game_state.get_remaining_run_seconds())
            state["phase"] = "observe_open_world"
            state["phase_started"] = time.monotonic()
        elif state["phase"] == "observe_open_world" and time.monotonic() - state["phase_started"] > 1.5:
            check("restored_supply_not_duplicated_by_inventory_events", item_count(
                pawn.get_editor_property("inventory_component"), state["item"])
                == min(5, state["item"].get_editor_property("max_stack_size")) + state["source_remaining"])
            check("open_world_timer_counts_down", game_state.get_remaining_run_seconds() < state["arrival_timer"])
            finish(message="Dialogue, exact one-time grant, full-bag retry, actual potion use and real portal carry verified.")
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        finish(True, str(error))


def main():
    global state
    previous = getattr(unreal, SINGLETON, None)
    if previous and not previous.get("finished"):
        state = previous
        unreal.log_warning("Tutorial NPC validator already running; duplicate ignored.")
        return
    setattr(unreal, SINGLETON, state)
    try:
        check("no_existing_pie", game_world() is None)
        editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        check("current_editor_map_start_hub", "StartHub" in editor_world.get_path_name())
        state["callback"] = unreal.register_slate_post_tick_callback(tick)
        state["owns_pie"] = True
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        write_report()
    except Exception as error:
        state["report"]["exception"] = traceback.format_exc()
        finish(True, str(error))


if __name__ == "__main__":
    main()
