"""Temporary v3 component/viewport review; never save assets or config."""
import time
import uuid

import unreal


PROBE_ATTRIBUTE = "_rogue10m_arin_reference_capture_frame_probe"
MESHES = {
    "baseline": "/Game/Rogue10m/Characters/Arin/RefinedV2/SK_Arin",
    "refined": "/Game/Rogue10m/Characters/Arin/RefinedV3/SK_Arin",
}


def component(phase="refined"):
    if phase not in MESHES:
        raise ValueError("Expected baseline or refined capture phase")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or not world or world.get_path_name().split(".")[0] != "/Game/Rogue10m/Maps/StartHub":
        raise RuntimeError("Requires idle StartHub editor")
    guides = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
              if actor.get_actor_label() == "SV_TutorialGuide_Arin"]
    if len(guides) != 1:
        raise RuntimeError("Expected exactly one existing Arin guide")
    mesh = guides[0].get_editor_property("guide_mesh")
    asset = mesh.get_skeletal_mesh_asset()
    if asset is None or asset.get_path_name().split(".")[0] != MESHES[phase]:
        raise RuntimeError("Capture requires the actual " + phase + " mesh: " + MESHES[phase])
    return mesh


def _performance():
    result = unreal.load_object(None, "/Script/UnrealEd.Default__EditorPerformanceSettings")
    if result is None:
        raise RuntimeError("EditorPerformanceSettings default object is unavailable")
    return result


def inspect(phase="refined"):
    mesh = component(phase)
    asset = mesh.get_skeletal_mesh_asset()
    performance = _performance()
    probe = getattr(unreal, PROBE_ATTRIBUTE, None)
    return {
        "phase": phase,
        "forced_value": mesh.get_forced_lod(),
        "predicted_lod_index": mesh.get_predicted_lod_level(),
        "update_animation_in_editor": bool(mesh.get_editor_property("update_animation_in_editor")),
        "background_throttle": bool(performance.get_editor_property("bThrottleCPUWhenNotForeground")),
        "mesh": asset.get_path_name(),
        "mesh_source_fbx_sha256": unreal.EditorAssetLibrary.get_metadata_tag(asset, "Arin.RefinedFBXSHA256"),
        "probe_ticks": probe["ticks"] if probe and not probe["finished"] and probe["phase"] == phase else None,
        "asset_saved": False,
    }


def request(value=None, update_animation=None, background_throttle=None, phase="refined"):
    """None preserves forced LOD; baseline requests never write forced LOD."""
    if value is not None and value not in (0, 1, 2, 3):
        raise ValueError("None=unchanged, 0=automatic, 1/2/3=LOD0/1/2")
    if phase == "baseline" and value is not None:
        raise ValueError("Baseline review must preserve the existing forced LOD")
    mesh = component(phase)
    previous = mesh.get_forced_lod()
    previous_update = bool(mesh.get_editor_property("update_animation_in_editor"))
    performance = _performance()
    previous_throttle = bool(performance.get_editor_property("bThrottleCPUWhenNotForeground"))
    if background_throttle is not None:
        # Runtime CDO value only. Never SaveConfig or save packages.
        performance.set_editor_property("bThrottleCPUWhenNotForeground", bool(background_throttle))
    if update_animation is not None:
        mesh.set_update_animation_in_editor(bool(update_animation))
    if value is not None:
        mesh.set_forced_lod(value)
    result = inspect(phase)
    result.update({
        "previous_forced_value": previous,
        "previous_update_animation_in_editor": previous_update,
        "previous_background_throttle": previous_throttle,
    })
    return result


def start_frame_probe(phase="refined", token=None):
    """Count actual editor Slate post ticks across independent remote calls."""
    component(phase)
    if token is None:
        token = uuid.uuid4().hex
    if not isinstance(token, str) or len(token) != 32 or any(character not in "0123456789abcdef" for character in token):
        raise ValueError("Expected a fresh lowercase hexadecimal UUID token")
    previous = getattr(unreal, PROBE_ATTRIBUTE, None)
    if previous and not previous["finished"]:
        if previous["token"] == token and previous["phase"] == phase:
            return frame_probe_status(token, phase)
        raise RuntimeError("An owned Arin capture frame probe is already active; preserve it")
    state = {"token": token, "phase": phase, "ticks": 0,
             "started": time.monotonic(), "finished": False, "handle": None}
    def count_editor_frame(delta_seconds):
        if not state["finished"]:
            state["ticks"] += 1
    state["handle"] = unreal.register_slate_post_tick_callback(count_editor_frame)
    setattr(unreal, PROBE_ATTRIBUTE, state)
    return frame_probe_status(state["token"], phase)


def frame_probe_status(token, phase="refined"):
    state = getattr(unreal, PROBE_ATTRIBUTE, None)
    if not state or state["token"] != token or state["phase"] != phase or state["finished"]:
        raise RuntimeError("Missing/mismatched owned Arin frame probe")
    return {"token": token, "phase": phase, "ticks": state["ticks"],
            "elapsed_seconds": time.monotonic() - state["started"],
            "component": inspect(phase), "asset_saved": False}


def stop_frame_probe(token):
    state = getattr(unreal, PROBE_ATTRIBUTE, None)
    if not state or state["token"] != token:
        raise RuntimeError("Refuse to remove a different editor callback")
    if not state["finished"] and state["handle"] is not None:
        unreal.unregister_slate_post_tick_callback(state["handle"])
        state["handle"] = None
    state["finished"] = True
    return {"token": token, "phase": state["phase"], "ticks": state["ticks"],
            "callback_removed": state["handle"] is None, "asset_saved": False}
