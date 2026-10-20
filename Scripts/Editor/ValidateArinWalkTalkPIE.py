"""Owned StartHub PIE: real Arin locomotion/dialogue clips and all 67 prior checks.

Run with runpy.run_path(..., run_name="__main__") in an idle StartHub editor.
Movement is a disposable validation stimulus, not a new patrol behavior.
No asset, map, editor camera, or project setting is saved by this wrapper.
"""
import hashlib
import importlib
import json
import math
from pathlib import Path
import sys
import time
import traceback

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
EVIDENCE = ROOT / "Feature/doc/evidence/arin-walk-talk-20261009"
REPORT = EVIDENCE / "tutorial-runtime.json"
BASELINE_REPORT = ROOT / "Feature/doc/evidence/arin-reference-refinement-20261009/tutorial-runtime.json"
MESH_PATH = "/Game/Rogue10m/Characters/Arin/RefinedV3/SK_Arin"
SKELETON_PATH = "/Game/Rogue10m/Characters/Arin/RefinedV3/SK_Arin_Skeleton"
IDLE_PATH = "/Game/Rogue10m/Characters/Arin/RefinedV3/ArinIdle"
WALK_PATH = "/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Walk"
TALK_PATH = "/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Talk"
WRONG_SKELETON_PATH = "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"
ANIMATION_FIELDS = ("guide_walk_animation", "guide_talk_animation",
                    "guide_walk_reference_speed", "guide_minimum_walk_speed")


def package_path(value):
    return value.get_path_name().split(".")[0] if value else None


def tuple3(value):
    return list(value.to_tuple())


def vector(values):
    return unreal.Vector(*values)


def angle_delta(first, second):
    return max(abs((a - b + 180.0) % 360.0 - 180.0) for a, b in zip(first, second))


def install():
    previous = getattr(unreal, "_rogue10m_tutorial_npc_validator", None)
    if previous and not previous.get("finished"):
        raise RuntimeError("An NPC validator is already running; inspect it before another run.")
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("Requires an idle StartHub editor; will not interrupt an existing PIE.")
    sys.path.insert(0, str(ROOT / "Scripts/Editor"))
    import ApplyReferenceArin as application
    import ValidateTutorialNPC as base
    base = importlib.reload(base)
    base.REPORT = REPORT
    original_prepare = base.prepare_fixture
    original_dialogue = base.begin_dialogue_checks
    original_tick = base.tick
    original_restore = base.restore_fixture
    original_finish = base.finish
    frame_limit = {"original": None, "applied": False, "restored": False}
    old_report = json.loads(BASELINE_REPORT.read_text(encoding="utf-8-sig"))
    required_names = {check["name"] for check in old_report["checks"]}
    if old_report.get("status") != "passed" or len(required_names) != 67:
        raise RuntimeError("The preserved 67-check RefinedV3 baseline is unavailable or invalid.")
    base.state["report"].update({
        "runtime": "Actual owned Unreal Editor StartHub PIE, followed by the real portal",
        "asset_mutation_requested": False,
        "fixture_movement_only": True,
        "baseline_check_count": len(required_names),
        "baseline_report_sha256": hashlib.sha256(BASELINE_REPORT.read_bytes()).hexdigest(),
        "validator_source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "animation_observations": [],
    })
    base.state["report"]["limitations"].extend([
        "Controlled actor displacement tests native clip selection, not autonomous NPC navigation or path following.",
        "A temporary always-tick-pose setting is used only on the owned PIE mesh for reliable bone observations.",
        "Talking is a body/head gesture loop; facial lip sync, individual fingers and recorded speech are outside this rig.",
        "Numerical playback and bone changes require the separate Blender/engine visual review for deformation quality.",
        "This owned fixture temporarily sets the process t.MaxFPS console value to 5 and restores its exact observed value on finish; no configuration is saved.",
    ])

    def now(world):
        return float(unreal.GameplayStatics.get_time_seconds(world))

    def snapshot(world, npc):
        mesh = npc.get_editor_property("guide_mesh")
        component = npc.get_editor_property("guide_animation")
        instance = mesh.get_anim_instance()
        asset = instance.get_animation_asset() if isinstance(instance, unreal.AnimSingleNodeInstance) else None
        return {
            "world_seconds": now(world),
            "wall_seconds": time.monotonic() - base.state["started"],
            "console_max_fps": float(unreal.SystemLibrary.get_console_variable_float_value("t.MaxFPS")),
            "state": {unreal.Rogue10mTutorialAnimationState.IDLE: "IDLE",
                      unreal.Rogue10mTutorialAnimationState.WALK: "WALK",
                      unreal.Rogue10mTutorialAnimationState.TALK: "TALK"}[component.get_animation_state()],
            "requested_dialogue": bool(component.is_dialogue_animation_requested()),
            "dialogue_open": bool(npc.is_dialogue_open()),
            "selected_clip": package_path(component.get_current_animation()),
            "native_clip": package_path(asset),
            "single_node": isinstance(instance, unreal.AnimSingleNodeInstance),
            "playing": bool(mesh.is_playing()),
            "position_seconds": float(mesh.get_position()),
            "play_rate": float(mesh.get_play_rate()),
            "measured_speed_cm_s": float(component.get_measured_speed()),
            "actor_location": tuple3(npc.get_actor_location()),
            "head_rotation": tuple3(mesh.get_socket_rotation("head")),
            "thigh_rotation": tuple3(mesh.get_socket_rotation("thigh_l")),
            "forearm_rotation": tuple3(mesh.get_socket_rotation("lowerarm_r")),
        }

    def expect_snapshot(name, observed, expected_state, expected_clip, request=None):
        base.check(name + "_native_state_and_clip",
                   observed["state"] == expected_state and observed["selected_clip"] == expected_clip
                   and observed["native_clip"] == expected_clip and observed["single_node"]
                   and observed["playing"], observed=observed,
                   expected_state=expected_state, expected_clip=expected_clip)
        if request is not None:
            base.check(name + "_dialogue_request", observed["requested_dialogue"] == request
                       and observed["dialogue_open"] == request, observed=observed)

    def set_nearby(world, npc):
        location = npc.get_actor_location()
        pawn = base.state["pawn"]
        pawn.get_editor_property("character_movement").stop_movement_immediately()
        pawn.set_actor_location(unreal.Vector(location.x + 150.0, location.y, location.z + 100.0), False, True)

    def open_dialogue(name, world, npc, expected_clip):
        set_nearby(world, npc)
        base.check(name + "_real_begin_dialogue", npc.begin_dialogue(base.state["pawn"]))
        expect_snapshot(name + "_immediate", snapshot(world, npc),
                        "TALK" if expected_clip == TALK_PATH else "IDLE", expected_clip, True)

    # Duration and sampling use game time, not wall time. Moving phases leave
    # enough actual frames even at 3 Hz to observe a clip change and progression.
    phases = [
        {"name": "initial_idle", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": False},
        {"name": "walk_55", "duration": 1.8, "state": "WALK", "clip": WALK_PATH, "speed": 55.0},
        {"name": "walk_110", "duration": 1.8, "state": "WALK", "clip": WALK_PATH, "speed": 110.0},
        {"name": "walk_220", "duration": 1.8, "state": "WALK", "clip": WALK_PATH, "speed": 220.0},
        {"name": "movement_stopped", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": False},
        {"name": "talk_open", "duration": 1.8, "state": "TALK", "clip": TALK_PATH, "request": True},
        {"name": "talk_priority_while_moving", "duration": 1.8, "state": "TALK", "clip": TALK_PATH,
         "speed": 110.0, "request": True},
        {"name": "talk_stationary_before_close", "duration": .45, "state": "TALK", "clip": TALK_PATH, "request": True},
        {"name": "talk_closed", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": False},
        {"name": "missing_walk", "duration": 1.8, "state": "IDLE", "clip": IDLE_PATH, "speed": 110.0},
        {"name": "walk_restored_stop", "duration": .45, "state": "IDLE", "clip": IDLE_PATH},
        {"name": "wrong_skeleton_walk", "duration": 1.8, "state": "IDLE", "clip": IDLE_PATH, "speed": 110.0},
        {"name": "wrong_walk_restored_stop", "duration": .45, "state": "IDLE", "clip": IDLE_PATH},
        {"name": "missing_talk", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": True},
        {"name": "talk_restored", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": False},
        {"name": "wrong_skeleton_talk", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": True},
        {"name": "wrong_talk_restored", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": False},
        {"name": "before_teleport_walk", "duration": 1.8, "state": "WALK", "clip": WALK_PATH, "speed": 110.0},
        {"name": "teleported", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": False},
        {"name": "teleport_restored", "duration": .45, "state": "IDLE", "clip": IDLE_PATH},
        {"name": "range_dialogue", "duration": .45, "state": "TALK", "clip": TALK_PATH, "request": True},
        {"name": "left_dialogue_range", "duration": .55, "state": "IDLE", "clip": IDLE_PATH, "request": False},
        {"name": "restored_idle", "duration": .45, "state": "IDLE", "clip": IDLE_PATH, "request": False},
    ]

    def enter_phase(index, world, npc):
        spec = phases[index]
        name = spec["name"]
        fixture = base.state["fixture_data"]
        clips = base.state["animation_clips"]
        if name == "talk_open":
            open_dialogue(name, world, npc, TALK_PATH)
        elif name == "talk_closed":
            npc.close_dialogue()
            expect_snapshot(name + "_immediate", snapshot(world, npc), "IDLE", IDLE_PATH, False)
        elif name == "missing_walk":
            fixture.set_editor_property("guide_walk_animation", None)
        elif name == "wrong_skeleton_walk":
            fixture.set_editor_property("guide_walk_animation", clips["wrong"])
        elif name in ("walk_restored_stop", "wrong_walk_restored_stop"):
            fixture.set_editor_property("guide_walk_animation", clips["walk"])
        elif name in ("missing_talk", "wrong_skeleton_talk"):
            fixture.set_editor_property("guide_talk_animation", None if name == "missing_talk" else clips["wrong"])
            open_dialogue(name, world, npc, IDLE_PATH)
        elif name in ("talk_restored", "wrong_talk_restored"):
            npc.close_dialogue()
            fixture.set_editor_property("guide_talk_animation", clips["talk"])
        elif name == "teleported":
            before = tuple3(npc.get_actor_location())
            npc.set_actor_location(unreal.Vector(before[0] + 400.0, before[1], before[2]), False, True)
            base.state["report"]["teleport_stimulus_cm"] = 400.0
        elif name == "teleport_restored":
            npc.set_actor_location(vector(base.state["animation_original_npc_location"]), False, True)
            set_nearby(world, npc)
        elif name == "range_dialogue":
            open_dialogue(name, world, npc, TALK_PATH)
        elif name == "left_dialogue_range":
            location = npc.get_actor_location()
            distance = float(npc.get_editor_property("detection_radius")) + 1000.0
            base.state["pawn"].set_actor_location(
                unreal.Vector(location.x + distance, location.y, location.z + 100.0), False, True)
        elif name == "restored_idle":
            fixture.set_editor_property("guide_walk_animation", clips["walk"])
            fixture.set_editor_property("guide_talk_animation", clips["talk"])
            npc.close_dialogue()
            npc.set_actor_location(vector(base.state["animation_original_npc_location"]), False, True)
            set_nearby(world, npc)
        base.state["animation_phase"] = {"index": index, "spec": spec, "started": now(world),
                                          "origin": tuple3(npc.get_actor_location()), "samples": []}
        base.state["phase"] = "animation_" + name
        base.write_report()

    def validate_progress(name, samples, clip, bone):
        # At a low frame rate, the first observation can precede the next native
        # movement sample. Observe progression only after the selected clip has
        # actually reached the single-node instance, never across a clip change.
        samples = [sample for sample in samples if sample["native_clip"] == package_path(clip)
                   and sample["selected_clip"] == package_path(clip) and sample["playing"]]
        if len(samples) < 3:
            raise RuntimeError(name + ": fewer than three native playback observations.")
        length = float(clip.get_play_length())
        observed = 0.0
        expected = 0.0
        wrapped = False
        for before, after in zip(samples, samples[1:]):
            delta = after["position_seconds"] - before["position_seconds"]
            if delta < -.0001:
                wrapped = True
            observed += delta % length
            elapsed = after["world_seconds"] - before["world_seconds"]
            expected += elapsed * (before["play_rate"] + after["play_rate"]) * .5
        tolerance = max(.10, expected * .30)
        base.check(name + "_playback_advances_without_timer_restart", expected > .08
                   and observed > .05 and abs(observed - expected) <= tolerance,
                   observed_seconds=observed, expected_seconds=expected, tolerance_seconds=tolerance,
                   first=samples[0], last=samples[-1])
        motion = max(angle_delta(sample[bone], samples[0][bone]) for sample in samples[1:])
        base.check(name + "_actual_bone_motion", motion > .01, bone=bone, max_degrees=motion)
        if name == "walk_55":
            maximum_interval = max(after["world_seconds"] - before["world_seconds"]
                                   for before, after in zip(samples, samples[1:]))
            maximum_wall_interval = max(after["wall_seconds"] - before["wall_seconds"]
                                        for before, after in zip(samples, samples[1:]))
            base.check("walk_55_low_frame_timer_catchup_preserves_motion", maximum_interval > .10001
                       and maximum_wall_interval > .10001
                       and all(sample["state"] == "WALK" and sample["measured_speed_cm_s"] > 10.0
                               and abs(sample["console_max_fps"] - 5.0) < .0001
                               for sample in samples),
                       movement_sample_seconds=.1, observed_frame_interval_seconds=maximum_interval,
                       observed_wall_frame_interval_seconds=maximum_wall_interval,
                       native_samples=len(samples),
                       requirement="Owned fixture temporarily caps t.MaxFPS at 5; the observed prior value is restored without saving configuration.")
        if name == "walk_220":
            base.check(name + "_actually_loops", wrapped, duration_seconds=length,
                       positions=[sample["position_seconds"] for sample in samples])

    def complete_phase(world, npc):
        phase = base.state["animation_phase"]
        spec = phase["spec"]
        name = spec["name"]
        observed = snapshot(world, npc)
        expect_snapshot(name, observed, spec["state"], spec["clip"], spec.get("request"))
        samples = phase["samples"]
        base.state["report"]["animation_observations"].append({
            "phase": name, "stimulus_speed_cm_s": spec.get("speed", 0.0),
            "observations": samples, "final": observed})
        if spec.get("speed"):
            target = spec["speed"]
            base.check(name + "_real_displacement_detected", observed["measured_speed_cm_s"] > 10.0
                       and abs(observed["measured_speed_cm_s"] - target) <= target * .35,
                       target_cm_s=target, observed_cm_s=observed["measured_speed_cm_s"])
            if spec["state"] == "WALK":
                reference = float(base.state["fixture_data"].get_editor_property("guide_walk_reference_speed"))
                expected_rate = max(.01, min(10.0, observed["measured_speed_cm_s"] / reference))
                base.check(name + "_native_rate_tracks_real_speed",
                           abs(observed["play_rate"] - expected_rate) < .0001,
                           observed_rate=observed["play_rate"], expected_rate=expected_rate,
                           nominal_rate=target / reference)
                validate_progress(name, samples, base.state["animation_clips"]["walk"], "thigh_rotation")
            elif spec["state"] == "TALK":
                base.check(name + "_talk_rate_unaffected_by_walk_speed", abs(observed["play_rate"] - 1.0) < .0001)
                validate_progress(name, samples, base.state["animation_clips"]["talk"], "forearm_rotation")
        elif name in ("initial_idle", "movement_stopped", "talk_stationary_before_close", "teleported", "restored_idle"):
            base.check(name + "_stationary_speed_zero", observed["measured_speed_cm_s"] < .01,
                       observed_cm_s=observed["measured_speed_cm_s"])
        if name == "talk_open":
            validate_progress(name, samples, base.state["animation_clips"]["talk"], "forearm_rotation")
        if name == "restored_idle":
            base.check("animation_fixture_restores_authored_npc_location",
                       observed["actor_location"] == base.state["animation_original_npc_location"])
            base.state["arin_head_rotation"] = observed["head_rotation"]
            base.state["arin_actor_location"] = observed["actor_location"]
            base.state["phase"] = "nearby"
            base.state["phase_started"] = time.monotonic()
            base.state["report"]["animation_fixture_complete"] = True
            base.write_report()
        else:
            enter_phase(phase["index"] + 1, world, npc)

    def prepare_fixture(world, pawn, npc):
        original_prepare(world, pawn, npc)
        # The real player controller applies its saved FPS setting in BeginPlay.
        # Install the disposable frame cap only after that initialization, while
        # retaining the pre-PIE console value for guaranteed final restoration.
        frame_limit["applied"] = True
        before_cap = float(unreal.SystemLibrary.get_console_variable_float_value("t.MaxFPS"))
        unreal.SystemLibrary.execute_console_command(world, "t.MaxFPS 5")
        applied = float(unreal.SystemLibrary.get_console_variable_float_value("t.MaxFPS"))
        base.state["report"]["frame_limit_fixture"] = {
            "original": frame_limit["original"], "observed_after_player_begin_play": before_cap,
            "requested": 5.0, "observed": applied, "configuration_saved": False}
        base.check("animation_low_frame_limit_applied", abs(applied - 5.0) < .0001,
                   **base.state["report"]["frame_limit_fixture"])
        mesh = npc.get_editor_property("guide_mesh")
        asset = mesh.get_skeletal_mesh_asset()
        idle = mesh.get_editor_property("animation_data").get_editor_property("anim_to_play")
        base.check("reference_mesh_in_real_pie", package_path(asset) == MESH_PATH)
        base.check("reference_idle_in_real_pie", package_path(idle) == IDLE_PATH)
        skeleton = asset.get_editor_property("skeleton")
        base.check("reference_same_skeleton_in_pie", skeleton == idle.get_editor_property("skeleton"))
        base.check("reference_two_second_idle_in_pie", abs(idle.get_play_length() - 2.0) < .0001)
        bone_names = [str(mesh.get_bone_name(index)) for index in range(mesh.get_num_bones())]
        bone_contract = application._validate_bone_names(bone_names)
        base.check("reference_exact_authored_bones_in_pie", True, bone_names=bone_names,
                   authored_bone_count=bone_contract["bone_count"])
        height = mesh.get_socket_location("head").z - mesh.get_world_location().z
        expected_head = bone_contract["head_joint_z_m"] * 100.0
        base.check("reference_visible_physical_head_height_in_pie", abs(height - expected_head) <= 3.0,
                   observed_cm=height, expected_rest_cm=expected_head)
        materials = list(asset.get_editor_property("materials"))
        base.check("reference_single_real_pbr_slot_in_pie", len(materials) == 1 and all(
            slot.get_editor_property("material_interface").get_path_name().startswith(
                "/Game/Rogue10m/Characters/Arin/RefinedV3/Materials/") for slot in materials))
        subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        base.check("reference_three_persisted_lods_in_pie", subsystem.get_lod_count(asset) == 3)
        data = base.state["original_data"]
        fixture = base.state["fixture_data"]
        for field in ANIMATION_FIELDS:
            fixture.set_editor_property(field, data.get_editor_property(field))
        walk = data.get_editor_property("guide_walk_animation")
        talk = data.get_editor_property("guide_talk_animation")
        base.check("animation_authored_clip_paths", package_path(walk) == WALK_PATH and package_path(talk) == TALK_PATH,
                   walk=package_path(walk), talk=package_path(talk))
        base.check("animation_clips_share_unchanged_v3_skeleton", package_path(skeleton) == SKELETON_PATH
                   and walk.get_editor_property("skeleton") == skeleton and talk.get_editor_property("skeleton") == skeleton)
        base.check("animation_authored_clip_durations", abs(walk.get_play_length() - 1.2) < .0001
                   and abs(talk.get_play_length() - 3.0) < .0001,
                   walk_seconds=walk.get_play_length(), talk_seconds=talk.get_play_length())
        base.check("animation_walk_reference_speed_110", abs(float(data.get_editor_property("guide_walk_reference_speed")) - 110.0) < .0001)
        component = npc.get_editor_property("guide_animation")
        base.check("animation_native_component_loaded", isinstance(component, unreal.Rogue10mTutorialAnimationComponent))
        base.check("animation_component_tick_disabled", not component.is_component_tick_enabled())
        wrong = unreal.load_asset(WRONG_SKELETON_PATH)
        base.check("animation_wrong_skeleton_fixture_available", isinstance(wrong, unreal.AnimSequence)
                   and wrong.get_editor_property("skeleton") != skeleton, clip=package_path(wrong))
        base.state.update({"animation_clips": {"idle": idle, "walk": walk, "talk": talk, "wrong": wrong},
                           "animation_original_npc_location": tuple3(npc.get_actor_location()),
                           "animation_original_tick_option": mesh.get_editor_property("visibility_based_anim_tick_option"),
                           "animation_direction": tuple3(npc.get_actor_forward_vector())})
        mesh.set_editor_property("visibility_based_anim_tick_option",
                                 unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        enter_phase(0, world, npc)

    def begin_dialogue_checks(world, pawn, npc):
        rotation = tuple3(npc.get_editor_property("guide_mesh").get_socket_rotation("head"))
        delta = angle_delta(rotation, base.state["arin_head_rotation"])
        base.check("reference_idle_really_moves_head_in_pie", delta > .0001, delta_degrees=delta)
        base.check("reference_idle_stationary_actor_in_pie",
                   tuple3(npc.get_actor_location()) == base.state["arin_actor_location"])
        original_dialogue(world, pawn, npc)

    def restore_fixture():
        if not base.state.get("traveled"):
            npc = base.state.get("npc")
            if npc and unreal.SystemLibrary.is_valid(npc):
                fixture = base.state.get("fixture_data")
                clips = base.state.get("animation_clips")
                if fixture and clips:
                    fixture.set_editor_property("guide_walk_animation", clips["walk"])
                    fixture.set_editor_property("guide_talk_animation", clips["talk"])
                if "animation_original_npc_location" in base.state:
                    npc.set_actor_location(vector(base.state["animation_original_npc_location"]), False, True)
                if "animation_original_tick_option" in base.state:
                    npc.get_editor_property("guide_mesh").set_editor_property(
                        "visibility_based_anim_tick_option", base.state["animation_original_tick_option"])
        original_restore()

    def finish(failed=False, message="Arin animation and tutorial integration verified."):
        if base.state["finished"]:
            return
        try:
            if not failed:
                observed_names = {check["name"] for check in base.state["report"]["checks"] if check["passed"]}
                missing = sorted(required_names - observed_names)
                base.state["report"]["baseline_missing_checks"] = missing
                base.state["report"]["baseline_checks_passed"] = len(required_names & observed_names)
                if missing or not base.state["report"].get("animation_fixture_complete"):
                    failed = True
                    message = "Missing preserved tutorial checks or incomplete native animation fixture."
                else:
                    message = "Native Walk/Talk playback, rate/priority/fallbacks and all 67 prior tutorial checks passed."
        except Exception:
            failed = True
            message = "Animation verification could not finish its completion checks."
            base.state["report"]["completion_exception"] = traceback.format_exc()
        finally:
            if frame_limit["applied"]:
                try:
                    context = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
                    unreal.SystemLibrary.execute_console_command(context, "t.MaxFPS " + format(frame_limit["original"], ".9g"))
                    restored = float(unreal.SystemLibrary.get_console_variable_float_value("t.MaxFPS"))
                    frame_limit["restored"] = abs(restored - frame_limit["original"]) <= max(.0001, abs(frame_limit["original"]) * .000001)
                    base.state["report"]["frame_limit_restoration"] = {
                        "original": frame_limit["original"], "observed_after_restore": restored,
                        "restored": frame_limit["restored"], "configuration_saved": False}
                    base.state["report"]["checks"].append({
                        "name": "animation_frame_limit_restored", "passed": frame_limit["restored"],
                        **base.state["report"]["frame_limit_restoration"]})
                    if not frame_limit["restored"]:
                        failed = True
                        message = "Owned fixture could not restore the prior t.MaxFPS value."
                except Exception:
                    failed = True
                    message = "Owned fixture failed while restoring its temporary frame limit."
                    base.state["report"]["frame_limit_restore_exception"] = traceback.format_exc()
        original_finish(failed, message)

    def tick(delta):
        if not base.state["phase"].startswith("animation_"):
            original_tick(delta)
            return
        try:
            if base.state["finished"]:
                return
            if time.monotonic() - base.state["started"] > 180.0:
                raise RuntimeError("Arin native animation fixture timed out.")
            world = base.game_world()
            if not world or "StartHub" not in world.get_path_name():
                raise RuntimeError("The owned animation fixture lost its StartHub PIE world.")
            npc = base.state["npc"]
            if not unreal.SystemLibrary.is_valid(npc):
                raise RuntimeError("The authored guide was destroyed during the native animation fixture.")
            phase = base.state["animation_phase"]
            elapsed = now(world) - phase["started"]
            spec = phase["spec"]
            if spec.get("speed"):
                direction = base.state["animation_direction"]
                location = [origin + axis * elapsed * spec["speed"]
                            for origin, axis in zip(phase["origin"], direction)]
                npc.set_actor_location(vector(location), False, True)
            if elapsed >= .25 and (not phase["samples"]
                                   or now(world) - phase["samples"][-1]["world_seconds"] >= .035):
                phase["samples"].append(snapshot(world, npc))
            if elapsed >= spec["duration"]:
                complete_phase(world, npc)
        except Exception as error:
            base.state["report"]["exception"] = traceback.format_exc()
            finish(True, str(error))

    base.prepare_fixture = prepare_fixture
    base.begin_dialogue_checks = begin_dialogue_checks
    base.restore_fixture = restore_fixture
    base.finish = finish
    base.tick = tick
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    try:
        # Restrict the environment change to this owned, idle StartHub session.
        # Console changes are in-memory; this never writes an ini or config asset.
        context = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if not context or "StartHub" not in context.get_path_name() or base.game_world() is not None:
            raise RuntimeError("Temporary frame limit requires an idle StartHub editor before owned PIE starts.")
        frame_limit["original"] = float(unreal.SystemLibrary.get_console_variable_float_value("t.MaxFPS"))
        if not math.isfinite(frame_limit["original"]):
            raise RuntimeError("Could not snapshot a valid t.MaxFPS console value for restoration.")
        base.state["report"]["frame_limit_before_pie"] = frame_limit["original"]
        base.main()
    except Exception as error:
        base.state["report"]["setup_exception"] = traceback.format_exc()
        finish(True, str(error))
    return base.state


if __name__ == "__main__":
    install()
