"""Prepared rendered-PIE movement QA; importing this module does nothing.

The editor owner starts/stops PIE. With the exact three QA switches, call
prepare_menu(allow_developer_pie_travel=True), wait for L_Menu, then call
create_profile_from_menu(). This uses normal lobby callbacks and normal Hub
travel. In Hub call enter_candidate_from_hub(allow_developer_pie_travel=True).
That explicit developer travel is setup, not verification of the Hub portal.

After the candidate settles, call begin_round(1, fresh_pie_confirmed=True).
For rounds 2 and 3 the owner stops PIE, starts fresh PIE in the saved candidate
at its PlayerStart, and calls begin_round(n, fresh_pie_confirmed=True,
allow_default_run_start=True). The latter calls public GameState.StartRun
only if WaitingToStart, with the production 1200-second duration unchanged.
Call summarize() only after all three reports finish. No editor asset writes,
editor travel, PIE lifecycle, speed/vital changes, or position setters occur.
QA SaveGames are created/updated through ordinary gameplay persistence.
"""
from __future__ import annotations

from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import time
import traceback


ROOT = Path("D:/Project/Rogue10m")
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
MENU = "/Game/Rogue10m/Maps/L_Menu"
HUB = "/Game/Rogue10m/Maps/StartHub"
EVIDENCE = ROOT / "Feature/doc/evidence/landscape-production-20261116/runtime-walk"
PROFILE_SLOT = "Rogue10m_Town_QA_Landscape20261116A"
PROGRESSION_PREFIX = "Rogue10m_Progression_QA_Landscape20261116A"
KEY_SLOT = "Rogue10m_Fighter_QA_Landscape20261116A"
CHARACTER_NAME = "CropWalkQA"
START_X, START_Y, ENTRANCE_Y = 15000.0, 32550.0, 62550.0
_runner = None


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def utc():
    return datetime.now(timezone.utc).isoformat()


def _write_new(name, value):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    with (EVIDENCE / (name + ".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def _context(allowed_maps):
    import unreal

    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong exact project")
    command = unreal.SystemLibrary.get_command_line()
    for flag, expected in (("RogueProfileSaveSlot", PROFILE_SLOT),
                           ("RogueProgressionSavePrefix", PROGRESSION_PREFIX),
                           ("RogueSkillKeySaveSlot", KEY_SLOT)):
        values = re.findall(r"(?:^|\s)-?" + flag + r"=(\"[^\"]*\"|[^\s]+)", command)
        require(len(values) == 1 and values[0].strip('"') == expected,
                "Exact isolated QA command-line switch required: " + flag)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    require(levels.is_in_play_in_editor() and world is not None, "Owner must start normal PIE")
    raw = world.get_path_name().split(".")[0]
    require("/UEDPIE_" in raw, "Expected a PIE game world, not an editor or standalone world")
    package = re.sub(r"/UEDPIE_\d+_", "/", raw)
    require(package in allowed_maps, "Unexpected PIE map: " + package)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    require(isinstance(pc, unreal.Rogue10mPlayerController), "Production controller required")
    return unreal, world, pc


def _selected_profile(unreal, *, require_runtime_name=False):
    """Public subsystem count plus optional live name; no SaveGame field read.

    SelectedProfileId/Profiles are not readable through Python in this build.
    The normal controller consumes its selected profile internally. Its GUID
    remains separate engine-log evidence, never a claimed Python observation.
    """
    require(unreal.GameplayStatics.does_save_game_exist(PROFILE_SLOT, 0), "Create the normal QA profile first")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    require(world is not None, "PIE game world required for the public profile subsystem")
    # SubsystemBlueprintLibrary is Blueprint-internal and absent from the UE
    # Python module. Locate the existing subsystem through the public typed
    # ObjectIterator and exact GameInstance outer; never construct one.
    instance = unreal.GameplayStatics.get_game_instance(world)
    candidates = [obj for obj in unreal.ObjectIterator(unreal.Rogue10mCharacterProfileSubsystem)
                  if obj.get_outer() == instance]
    require(len(candidates) == 1, "Exactly one existing PIE GameInstance profile subsystem required")
    profiles = candidates[0]
    require(isinstance(profiles, unreal.Rogue10mCharacterProfileSubsystem)
            and profiles.get_profile_count() == 1,
            "Public profile subsystem must report exactly one QA profile")
    observed_name = None
    if require_runtime_name:
        pawn = unreal.GameplayStatics.get_player_character(world, 0)
        require(isinstance(pawn, unreal.Rogue10mCharacter), "Production QA pawn required")
        observed_name = str(pawn.get_character_display_name())
        require(observed_name == CHARACTER_NAME, "Public runtime QA character name differs")
    return {"character_name": observed_name, "expected_character_name": CHARACTER_NAME,
            "public_profile_count": 1, "QA_profile_slot_exists": True,
            "public_runtime_name_verified": require_runtime_name,
            "profile_guid": "not_read; engine-log evidence must be recorded separately"}


def prepare_menu(*, allow_developer_pie_travel=False):
    """Optional setup travel from candidate PIE; never opens an editor level."""
    require(allow_developer_pie_travel is True, "Explicit developer PIE travel acknowledgement required")
    u, world, _ = _context({TARGET})
    require(_runner is None, "Movement session already exists")
    require(not u.GameplayStatics.does_save_game_exist(PROFILE_SLOT, 0), "QA profile already exists; inspect it")
    _write_new("setup-menu-travel", {"utc": utc(), "kind": "developer_PIE_setup_travel",
               "from": TARGET, "to": MENU, "counts_as_gameplay_entry_verification": False})
    u.GameplayStatics.open_level(world, u.Name(MENU), True)
    return {"requested": MENU, "wait_for_travel_before_next_call": True}


def create_profile_from_menu():
    """Normal lobby UI callbacks; writes only the explicitly isolated QA saves."""
    u, world, pc = _context({MENU})
    require(not u.GameplayStatics.does_save_game_exist(PROFILE_SLOT, 0), "QA profile exists; preserve it")
    require(not u.GameplayStatics.does_save_game_exist(KEY_SLOT, 0), "QA skill-key slot exists; inspect first")
    require(not (EVIDENCE / "normal-profile-creation.json").exists(), "Profile creation proof already exists")
    pc.open_character_lobby_from_main_menu()
    widgets = u.WidgetLibrary.get_all_widgets_of_class(world, u.Rogue10mCharacterLobbyWidget, False)
    require(len(widgets) == 1, "Exactly one normal character lobby widget required")
    lobby = widgets[0]
    lobby.call_method("HandleNewCharacterClicked")
    lobby.get_editor_property("UI_CharacterNameInput").set_text(CHARACTER_NAME)
    lobby.call_method("HandleCreateCharacterClicked")
    selected = _selected_profile(u)
    _write_new("normal-profile-creation", {"utc": utc(), **selected,
               "profile_slot": PROFILE_SLOT, "skill_key_slot": KEY_SLOT,
               "progression_prefix": PROGRESSION_PREFIX,
               "creation": "normal character lobby UI callbacks, programmatic invocation",
               "physical_UI_input": False, "next": "normal EnterSelectedCharacter travel to StartHub"})
    pc.enter_selected_character()
    return {"requested": HUB, "wait_for_normal_travel_before_next_call": True}


def resume_existing_created_profile_from_menu():
    """Resume the already observed UI creation; never create or select a profile.

    The first UI attempt saved CropWalkQA before its proof failed on a native
    reflected property name. Preserve that log/stack and the existing QA slot.
    This path records the exact existing single selected profile, then uses
    normal EnterSelectedCharacter. Profile count and slot presence are verified
    here; selected name/GUID are not read. Verify the live name after Hub entry.
    It cannot replace an existing proof.
    """
    u, world, pc = _context({MENU})
    require(_runner is None, "Movement session already exists")
    require(not (EVIDENCE / "normal-profile-creation.json").exists(),
            "Normal profile proof already exists; do not repeat entry")
    selected = _selected_profile(u)
    widgets = u.WidgetLibrary.get_all_widgets_of_class(world, u.Rogue10mCharacterLobbyWidget, False)
    require(len(widgets) == 1 and pc.is_character_lobby_visible(),
            "The existing normal character lobby must still be visible")
    _write_new("normal-profile-creation", {"utc": utc(), **selected,
               "profile_slot": PROFILE_SLOT, "skill_key_slot": KEY_SLOT,
               "progression_prefix": PROGRESSION_PREFIX,
               "creation": "previous normal character lobby UI callbacks; public profile count verified",
               "resume_reason": "Initial proof lookup used an unavailable snake-case native property alias",
               "initial_failure_log_preserved": True,
               "profile_recreated": False, "physical_UI_input": False,
               "next": "normal EnterSelectedCharacter travel to StartHub"})
    pc.enter_selected_character()
    return {"requested": HUB, "existing_profile_resumed": True,
            "wait_for_normal_travel_before_next_call": True}


def confirm_existing_profile_in_hub():
    """Read-only public identity proof after the owner's actual normal UI entry.

    The owner's retained observation is provenance for creation/selection/GUID.
    This function confirms the live name, public profile count and exact slot.
    It does not create a profile, inspect protected fields, travel or respawn.
    """
    u, world, _ = _context({HUB})
    require(_runner is None, "Movement session already exists")
    observation_path = EVIDENCE / "owner-normal-profile-observation.json"
    observation = json.loads(observation_path.read_text(encoding="utf-8-sig"))
    require(observation.get("QA_slot_absent_before_creation") is True
            and observation.get("normal_lobby_creation_logged") is True
            and observation.get("normal_lobby_selected_name_observed") == CHARACTER_NAME
            and observation.get("normal_game_connect_button_clicked") is True
            and observation.get("normal_StartHub_arrival_observed") is True,
            "Owner's retained normal UI creation and entry evidence is required")
    selected = _selected_profile(u, require_runtime_name=True)
    pawn = u.GameplayStatics.get_player_character(world, 0)
    record = {"utc": utc(), **selected, "map": HUB,
              "pawn_class": pawn.get_class().get_path_name(),
              "owner_observation": observation_path.relative_to(ROOT).as_posix(),
              "profile_guid_from_owner_log": observation.get("progression_attached_profile_id_from_log"),
              "private_SaveGame_fields_read": False, "profile_recreated": False,
              "travels_or_respawns_by_this_confirmation": 0}
    _write_new("public-profile-confirmation", record)
    return record


def enter_candidate_from_hub(*, allow_developer_pie_travel=False):
    require(allow_developer_pie_travel is True, "Explicit developer PIE travel acknowledgement required")
    u, world, _ = _context({HUB})
    selected = _selected_profile(u, require_runtime_name=True)
    pawn = u.GameplayStatics.get_player_character(world, 0)
    require(isinstance(pawn, u.Rogue10mCharacter)
            and str(pawn.get_character_display_name()) == CHARACTER_NAME,
            "Normal selected production pawn has not entered Hub")
    _write_new("setup-candidate-travel", {"utc": utc(), **selected,
               "from": HUB, "to": TARGET, "options": "StartRun=1",
               "kind": "explicit developer PIE setup travel",
               "normal_hub_portal_verified": False, "after_field_entry_teleports": 0})
    u.GameplayStatics.open_level(world, u.Name(TARGET), True, "StartRun=1")
    return {"requested": TARGET, "wait_for_field_spawn_before_begin_round": True}


def begin_round(round_index, *, fresh_pie_confirmed=False, allow_default_run_start=False):
    """Observe one fresh field spawn then feed ordinary AddMovementInput only."""
    global _runner
    require(type(round_index) is int and round_index in (1, 2, 3), "Round must be 1, 2 or 3")
    require(fresh_pie_confirmed is True, "Owner must confirm a fresh PIE/spawn for each repetition")
    require(_runner is None or _runner.handle is None, "A movement observer is already active")
    for previous in range(1, round_index):
        report = json.loads((EVIDENCE / ("round-%d.json" % previous)).read_text(encoding="utf-8"))
        require(report.get("passed") is True, "Preserve failed prior round; do not silently retry")
    require(not (EVIDENCE / ("round-%d.json" % round_index)).exists(), "Round proof already exists")
    u, world, pc = _context({TARGET})
    selected = _selected_profile(u)
    # Use the same public normal profile/application path as the lobby. This
    # may replace a default pawn at its existing PlayerStart transform; it is
    # setup before timing, with no position setter or fabricated save data.
    pc.enter_selected_character()
    pawn = u.GameplayStatics.get_player_character(world, 0)
    require(isinstance(pawn, u.Rogue10mCharacter), "Production pawn required")
    require(str(pawn.get_character_display_name()) == CHARACTER_NAME, "Normal QA profile was not applied")
    selected.update(character_name=CHARACTER_NAME, public_runtime_name_verified=True)
    require(pawn.is_third_person_gameplay_active(), "Current third-person contract required")
    pos = pawn.get_actor_location()
    require(abs(pos.x - START_X) <= 100 and abs(pos.y - START_Y) <= 100, "Fresh PlayerStart S spawn required; no teleport fallback")
    gs = u.GameplayStatics.get_game_state(world)
    require(isinstance(gs, u.Rogue10mGameState), "Production run GameState required")
    require(abs(gs.get_run_duration_seconds() - 1200.0) < 0.01, "Production 20-minute run duration required")
    setup_start = False
    if not gs.is_run_active():
        require(allow_default_run_start is True, "Run is waiting; explicitly acknowledge normal StartRun as test setup")
        require(gs.get_run_phase() == u.Rogue10mRunPhase.WAITING_TO_START, "Do not reset a terminal or previous run")
        gs.start_run()
        setup_start = True
    require(gs.is_run_active(), "Normal run did not start")
    pc.close_all_blocking_panels()
    require(not u.GameplayStatics.is_game_paused(world), "Gameplay remains paused; no pause override")
    candidate = _Runner(u, world, pc, pawn, gs, round_index, selected, setup_start)
    candidate.handle = u.register_slate_post_tick_callback(candidate.tick)
    _runner = candidate
    return {"started_round": round_index, "programmatic_input": True, "physical_input": False,
            "owner_stops_PIE_after_completion": True, "test_setup_StartRun": setup_start}


class _Runner:
    def __init__(self, u, world, pc, pawn, gs, index, selected, setup_start):
        self.u, self.world, self.pc, self.pawn, self.gs = u, world, pc, pawn, gs
        self.handle = None
        self.path = EVIDENCE / ("round-%d.json" % index)
        self.wall_started = time.monotonic()
        self.walk_wall = self.walk_sim = None
        self.last_sample = 0.0
        self.grounded = self.airborne = 0
        self.max_lateral_cm = 0.0
        self.speed = float(pawn.get_movement_component().max_walk_speed)
        normal_speed = float(pawn.get_character_stat_snapshot().get_editor_property("MoveSpeed"))
        require(math.isfinite(self.speed) and self.speed > 0
                and abs(self.speed - normal_speed) < 0.05, "Pawn speed must equal ordinary equipment/stat walk speed")
        require(abs(self.speed - 620.0) < 0.05, "Expected unchanged baseline equipment walk speed 620cm/s")
        require(not pawn.is_sprinting() and not pawn.is_dodging(), "No sprint/dodge input may be active")
        self.report = {"status": "running", "round": index, "started_utc": utc(), **selected,
                       "map": TARGET, "PIE_world": world.get_path_name(), "fresh_PIE_owner_confirmed": True,
                       "mode": "rendered PIE; programmatic Pawn.AddMovementInput",
                       "physical_input": False, "speed_overrides": 0, "invulnerability": False,
                       "after_field_entry_teleports": 0, "run_duration_override": False,
                       "normal_profile_application": "PlayerController.EnterSelectedCharacter before timing",
                       "default_StartRun_invoked_as_setup": setup_start,
                       "normal_Hub_portal_verified": False, "default_walk_speed_cm_s": self.speed,
                       "production_pawn_class": pawn.get_class().get_path_name(),
                       "spawn": self.xyz(pawn.get_actor_location()), "samples": [],
                       "limitations": ["Developer setup travel/fresh PIE; ordinary Hub portal not exercised.",
                                       "Programmatic movement input, not physical keys or a complete run.",
                                       "Central S-to-boss-entrance only; optional growth/combat route not tested."]}
        _write_new("round-%d" % index, self.report)
        pc.set_control_rotation(u.Rotator(pitch=-10, yaw=90, roll=0))

    @staticmethod
    def xyz(vector):
        return [float(vector.x), float(vector.y), float(vector.z)]

    def flush(self):
        self.path.write_text(json.dumps(self.report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def finish(self, error=None):
        if self.handle is not None:
            self.u.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        self.report.update(status="passed" if error is None else "failed", passed=error is None,
                           error=error, finished_utc=utc(), observer_wall_seconds=time.monotonic() - self.wall_started,
                           grounded_observations=self.grounded, airborne_observations=self.airborne,
                           max_lateral_error_cm=self.max_lateral_cm)
        self.flush()
        print("LANDSCAPE_WALK_20261116 ROUND%d %s" % (self.report["round"], self.report["status"]))

    def tick(self, _delta_seconds):
        try:
            self._tick()
        except Exception:
            self.finish(traceback.format_exc())

    def _tick(self):
        u, world, pc = _context({TARGET})
        require(world == self.world and pc == self.pc, "PIE world/controller changed during measured movement")
        require(u.GameplayStatics.get_player_character(world, 0) == self.pawn, "Production pawn changed")
        now = time.monotonic()
        require(now - self.wall_started <= 180, "180-second observer watchdog")
        movement = self.pawn.get_movement_component()
        require(not self.pawn.is_dead() and self.gs.is_run_active(), "Character died or run ended")
        require(not self.pawn.is_sprinting() and not self.pawn.is_dodging(), "Sprint/dodge detected")
        require(abs(float(movement.max_walk_speed) - self.speed) < 0.05, "Walk speed changed")
        require(abs(u.GameplayStatics.get_global_time_dilation(world) - 1.0) < 0.001,
                "Global time dilation must remain default")
        require(abs(float(self.pawn.get_editor_property("CustomTimeDilation")) - 1.0) < 0.001,
                "Pawn time dilation must remain default")
        require(not u.GameplayStatics.is_game_paused(world), "PIE paused during observer")
        pos = self.pawn.get_actor_location()
        require(all(math.isfinite(v) for v in self.xyz(pos)), "Nonfinite position")
        lateral = abs(pos.x - START_X)
        self.max_lateral_cm = max(self.max_lateral_cm, lateral)
        require(lateral <= 100, "Pawn left central corridor")
        if self.walk_sim is None:
            require(abs(pos.y - START_Y) <= 100, "Pawn moved before measurement")
            if now - self.wall_started < 2:
                return
            require(movement.is_moving_on_ground(), "No grounded S spawn after settling")
            require(math.sqrt(sum(v * v for v in self.xyz(self.pawn.get_velocity()))) < 5.0,
                    "Unexpected movement before input")
            self.walk_sim = u.GameplayStatics.get_time_seconds(world)
            self.walk_wall = now
            self.report["walk_start_utc"] = utc()
            self.report["remaining_seconds_at_start"] = self.gs.get_remaining_run_seconds()
        elapsed = u.GameplayStatics.get_time_seconds(world) - self.walk_sim
        grounded = movement.is_moving_on_ground()
        self.grounded += int(grounded)
        self.airborne += int(not grounded)
        if now - self.last_sample >= 0.5:
            self.last_sample = now
            self.report["samples"].append({"sim_seconds": elapsed, "location": self.xyz(pos),
                       "velocity": self.xyz(self.pawn.get_velocity()), "grounded": grounded,
                       "max_walk_speed_cm_s": float(movement.max_walk_speed),
                       "remaining_run_seconds": self.gs.get_remaining_run_seconds()})
            self.flush()
        if pos.y >= ENTRANCE_Y:
            self.report.update(arrival=self.xyz(pos), travel_sim_seconds=elapsed,
                               travel_wall_seconds=now - self.walk_wall,
                               remaining_seconds_at_arrival=self.gs.get_remaining_run_seconds())
            require(elapsed <= 60.0, "Boss entrance took more than 60 simulation seconds")
            require(grounded and self.airborne == 0, "Ground contact was lost")
            require(self.gs.get_remaining_run_seconds() < self.report["remaining_seconds_at_start"] - 1,
                    "Run countdown did not decrease")
            movement.stop_movement_immediately()
            self.finish()
            return
        require(elapsed <= 65.0, "Boss entrance not reached by 65-second timeout")
        self.pawn.add_movement_input(u.Vector(0, 1, 0), 1.0, False)


def summarize():
    """Aggregate only three completed, separately prepared repetition reports."""
    require(_runner is None or _runner.handle is None, "Observer still running")
    rows = [json.loads((EVIDENCE / ("round-%d.json" % i)).read_text(encoding="utf-8")) for i in (1, 2, 3)]
    checks = [row.get("passed") is True and row.get("round") == i
              and row.get("fresh_PIE_owner_confirmed") is True
              and row.get("travel_sim_seconds", math.inf) <= 60
              and row.get("after_field_entry_teleports") == 0
              and row.get("speed_overrides") == 0
              and row.get("airborne_observations") == 0 for i, row in zip((1, 2, 3), rows)]
    require(all(checks), "All three preserved reports must pass; failed runs cannot be hidden")
    result = {"utc": utc(), "passed": True, "repetitions": 3,
              "travel_sim_seconds": [r["travel_sim_seconds"] for r in rows],
              "travel_wall_seconds": [r["travel_wall_seconds"] for r in rows],
              "physical_input": False, "normal_Hub_portal_verified": False,
              "scope": "Production CharacterMovement, central S-to-boss-entrance, explicitly prepared PIE repetitions"}
    _write_new("summary", result)
    return result
