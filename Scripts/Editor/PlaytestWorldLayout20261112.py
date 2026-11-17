"""Rendered PIE CharacterMovement test; programmatic input, not physical play.

Use an editor launched with the three isolated QA save switches. Start PIE in
L_Menu, call create_profile_from_menu(), wait for StartHub, then call begin().
Normal UI callbacks create the isolated QA character and travel to town.
No editor assets or production saves are edited by this module.
"""
import datetime
import json
import math
from pathlib import Path
import time
import traceback

ROOT = Path('D:/Project/Rogue10m')
EVIDENCE = ROOT / 'Feature/doc/evidence/landscape-world-blockout-20261112'
PROFILE_SLOT = 'Rogue10m_Town_QA_Map20261112_A'
PROGRESSION_PREFIX = 'Rogue10m_Progression_QA_Map20261112_A'
KEY_SLOT = 'Rogue10m_Fighter_QA_Map20261112_A'
_runner = None


def _check_namespace(unreal):
    assert Path(unreal.Paths.project_dir()).resolve() == ROOT
    cmd = unreal.SystemLibrary.get_command_line()
    for flag, value in [('RogueProfileSaveSlot', PROFILE_SLOT),
                        ('RogueProgressionSavePrefix', PROGRESSION_PREFIX),
                        ('RogueSkillKeySaveSlot', KEY_SLOT)]:
        assert flag + '=' + value in cmd, 'Restart editor in the isolated QA namespace: ' + flag


def create_profile_from_menu():
    import unreal
    _check_namespace(unreal)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world and 'L_Menu' in world.get_name()
    assert not unreal.GameplayStatics.does_save_game_exist(PROFILE_SLOT, 0), 'QA profile exists; inspect instead.'
    assert not unreal.GameplayStatics.does_save_game_exist(KEY_SLOT, 0)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    assert pc and isinstance(pc, unreal.Rogue10mPlayerController)
    pc.open_character_lobby_from_main_menu()
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.Rogue10mCharacterLobbyWidget, False)
    assert len(widgets) == 1
    lobby = widgets[0]
    lobby.call_method('HandleNewCharacterClicked')
    lobby.get_editor_property('UI_CharacterNameInput').set_text('MapWalkQA')
    lobby.call_method('HandleCreateCharacterClicked')
    assert unreal.GameplayStatics.does_save_game_exist(PROFILE_SLOT, 0), 'Normal profile creation did not save.'
    pc.enter_selected_character()
    return dict(profile_slot=PROFILE_SLOT, creation='normal lobby UI callbacks', requested_travel='StartHub')


def begin():
    global _runner
    import unreal
    _check_namespace(unreal)
    assert _runner is None, 'Inspect the existing runner; do not replay it.'
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world and 'StartHub' in world.get_name(), 'Start normal PIE in StartHub first.'
    _runner = _Runner(unreal)
    _runner.handle = unreal.register_slate_post_tick_callback(_runner.tick)
    return dict(started=True, input='Pawn.AddMovementInput, no speed/position override after field entry')


class _Runner:
    def __init__(self, unreal):
        self.u = unreal
        self.handle = None
        self.phase = 'hub'
        self.started = time.monotonic()
        self.phase_at = self.started
        self.last_sample = 0
        self.walk_world_start = None
        self.walk_real_start = None
        self.grounded_samples = 0
        self.falling_samples = 0
        self.report = dict(started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                           evidence_kind='rendered PIE, programmatic Pawn movement input', physical_input=False,
                           after_field_entry_teleports=0, speed_overrides=0, invulnerability=False,
                           profile_slot=PROFILE_SLOT, samples=[], checks=[])

    def check(self, key, passed, facts=None):
        self.report['checks'].append(dict(name=key, passed=bool(passed), facts=facts))
        if not passed:
            raise RuntimeError(key + ': ' + str(facts))

    def finish(self, error=None):
        if self.handle is not None:
            self.u.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        self.report['passed'] = error is None
        self.report['error'] = error
        self.report['elapsed_wall_seconds'] = time.monotonic() - self.started
        (EVIDENCE / 'runtime-walk.json').write_text(json.dumps(self.report, ensure_ascii=False, indent=2), encoding='utf-8')
        print('WORLD_LAYOUT_WALK ' + ('PASSED' if error is None else 'FAILED ' + error))

    def tick(self, delta_seconds):
        try:
            self._tick(delta_seconds)
        except Exception:
            self.finish(traceback.format_exc())

    def _tick(self, delta_seconds):
        u = self.u
        now = time.monotonic()
        if now - self.started > 180:
            raise RuntimeError('180-second watchdog expired: ' + self.phase)
        world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        pawn = u.GameplayStatics.get_player_character(world, 0)
        pc = u.GameplayStatics.get_player_controller(world, 0)
        gs = u.GameplayStatics.get_game_state(world)
        if not pawn or not pc or not gs:
            return
        if self.phase == 'hub':
            self.check('hub_waiting', 'WAITING_TO_START' in str(gs.get_run_phase()))
            self.check('hub_1200_seconds', abs(gs.get_remaining_run_seconds() - 1200) < 0.1)
            self.check('production_pawn', isinstance(pawn, u.Rogue10mCharacter), pawn.get_class().get_path_name())
            self.check('production_controller', isinstance(pc, u.Rogue10mPlayerController), pc.get_class().get_path_name())
            pc.close_all_blocking_panels()
            # Test setup only: approach the real portal trigger from outside.
            pawn.set_actor_location(u.Vector(-720, 3900, 112), False, True)
            pc.set_control_rotation(u.Rotator(pitch=-10, yaw=90, roll=0))
            self.phase, self.phase_at = 'portal', now
            return
        if self.phase == 'portal':
            if 'WorldLayout20261112' in world.get_name():
                self.check('portal_started_normal_run', gs.is_run_active())
                self.check('run_duration_1200', abs(gs.get_run_duration_seconds() - 1200) < 0.1)
                self.report['field_spawn'] = self.xyz(pawn.get_actor_location())
                self.report['max_walk_speed_cm_s'] = pawn.get_movement_component().max_walk_speed
                self.report['pawn_class'] = pawn.get_class().get_path_name()
                pc.set_control_rotation(u.Rotator(pitch=-10, yaw=90, roll=0))
                self.phase, self.phase_at = 'settle', now
                return
            if now - self.phase_at > 30:
                raise RuntimeError('Normal portal travel did not finish.')
            pawn.add_movement_input(u.Vector(0, 1, 0), 1.0, False)
            return
        if self.phase == 'settle':
            if now - self.phase_at < 2:
                return
            pos = pawn.get_actor_location()
            self.check('spawn_at_S', abs(pos.x - 15000) < 100 and abs(pos.y - 32550) < 100, self.xyz(pos))
            self.check('grounded_before_walk', pawn.get_movement_component().is_moving_on_ground())
            self.walk_world_start = u.GameplayStatics.get_time_seconds(world)
            self.walk_real_start = now
            self.phase = 'walk'
        if self.phase == 'walk':
            movement = pawn.get_movement_component()
            pos = pawn.get_actor_location()
            sim_elapsed = u.GameplayStatics.get_time_seconds(world) - self.walk_world_start
            if movement.is_moving_on_ground():
                self.grounded_samples += 1
            else:
                self.falling_samples += 1
            if now - self.last_sample >= 0.5:
                self.last_sample = now
                self.report['samples'].append(dict(sim_seconds=sim_elapsed, location=self.xyz(pos),
                    velocity=self.xyz(pawn.get_velocity()), grounded=movement.is_moving_on_ground(),
                    max_walk_speed=movement.max_walk_speed, remaining_seconds=gs.get_remaining_run_seconds()))
            if pos.y >= 62550:
                self.report.update(arrival=self.xyz(pos), travel_sim_seconds=sim_elapsed,
                                   travel_wall_seconds=now - self.walk_real_start,
                                   grounded_frames=self.grounded_samples, falling_frames=self.falling_samples)
                self.check('boss_entrance_under_60_seconds', sim_elapsed <= 60, sim_elapsed)
                self.check('straight_lane_kept', abs(pos.x - 15000) < 100, self.xyz(pos))
                self.check('arrival_grounded', movement.is_moving_on_ground())
                self.check('no_lost_ground_during_walk', self.falling_samples == 0, self.falling_samples)
                self.check('timer_decreased', gs.get_remaining_run_seconds() < 1190, gs.get_remaining_run_seconds())
                movement.stop_movement_immediately()
                self.finish()
                return
            if sim_elapsed > 65:
                raise RuntimeError('Could not reach the boss entrance within 65 simulation seconds.')
            pawn.add_movement_input(u.Vector(0, 1, 0), 1.0, False)

    @staticmethod
    def xyz(v):
        return [float(v.x), float(v.y), float(v.z)]
