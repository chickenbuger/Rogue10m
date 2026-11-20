"""Capture actual StartHub editor views, restoring the viewport afterward.

Load the saved StartHub in a rendered UE editor, then import this module and
call arm(r"<new output directory>"). No Play, save, or content changes occur.
"""

import hashlib
import json
from pathlib import Path
import re
import struct
import time
import traceback

import unreal


HUB = "/Game/Rogue10m/Maps/StartHub.StartHub"
VIEWS = (
    ("01-overview-from-south", (240, -3740, 3370), (0, 105, 130)),
    ("02-arrival-eye-level", (0, -2290, 210), (30, 50, 185)),
    ("03-equipment-west", (-240, -980, 225), (-925, -820, 160)),
    ("04-forge-and-arin-east", (125, -1310, 235), (1010, -980, 145)),
    ("05-training-west", (-750, 1090, 225), (-1280, 1460, 120)),
    ("06-north-services-and-portal", (95, 550, 305), (0, 1860, 195)),
)
_active = None


def dirty_packages():
    utils = unreal.EditorLoadingAndSavingUtils
    return sorted(package.get_name() for package in [*utils.get_dirty_map_packages(), *utils.get_dirty_content_packages()])


def arm(output_dir, *, views=VIEWS, width=1536, height=1024):
    """Schedule captures with the actual editor viewport, one task at a time."""
    global _active
    if _active is not None:
        raise RuntimeError("A StartHub capture sequence is active.")
    if not views or any(not re.fullmatch(r"[A-Za-z0-9_-]+", row[0]) for row in views):
        raise ValueError("Provide at least one simple, ASCII-named view.")
    if len({row[0] for row in views}) != len(views):
        raise ValueError("View names must be unique.")

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = editor.get_editor_world()
    if not world or world.get_path_name() != HUB or editor.get_game_world():
        raise RuntimeError("Load the saved StartHub and stop PIE before editor viewport captures.")
    viewport = levels.get_active_viewport_config_key()
    camera_readback = levels.get_level_viewport_camera_info(viewport)
    if not camera_readback:
        raise RuntimeError("A perspective level viewport is required.")
    camera = next(item for item in camera_readback if isinstance(item, unreal.Vector))
    rotation = next(item for item in camera_readback if isinstance(item, unreal.Rotator))
    previous_game_view = levels.editor_get_game_view(viewport)
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=False)
    report = {"map": HUB, "source": "actual Unreal rendered level-editor viewport, force_game_view=True",
              "gameplay_camera": False, "initial_dirty_packages": dirty_packages(), "views": []}
    state = {"index": 0, "phase": "aim", "started": time.monotonic(), "handle": None}

    def finish(error=None):
        global _active
        unreal.unregister_slate_post_tick_callback(state["handle"])
        _active = None
        try:
            levels.set_level_viewport_camera_info(camera, rotation, viewport)
            levels.editor_set_game_view(previous_game_view, viewport)
        except Exception:
            report["viewport_restore_error"] = traceback.format_exc()
        report["final_dirty_packages"] = dirty_packages()
        if error:
            report["error"] = error
        report["passed"] = (not error and "viewport_restore_error" not in report
                            and report["initial_dirty_packages"] == report["final_dirty_packages"]
                            and len(report["views"]) == len(views))
        (output / "capture-receipt.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if report["passed"]:
            unreal.log(f"START_HUB_DETAIL_EDITOR_CAPTURES_READY count={len(views)} report={output}")
        else:
            unreal.log_error(f"StartHub editor captures failed; report={output}")

    def tick(_delta):
        if state.get("busy"):
            return
        state["busy"] = True
        try:
            if time.monotonic() - state["started"] > 120:
                raise RuntimeError("Timed out waiting for rendered StartHub screenshots.")
            if editor.get_editor_world() != world or editor.get_game_world():
                raise RuntimeError("Another map or PIE opened during the StartHub captures.")
            name, camera_cm, look_at_cm = views[state["index"]]
            image = output / (name + ".png")
            if state["phase"] == "aim":
                position = unreal.Vector(*camera_cm)
                look_at = unreal.Vector(*look_at_cm)
                aim = unreal.MathLibrary.find_look_at_rotation(position, look_at)
                levels.set_level_viewport_camera_info(position, aim, viewport)
                levels.editor_invalidate_viewports()
                state.update(phase="settle", since=time.monotonic(), ticks=0)
                return
            if state["phase"] == "settle":
                state["ticks"] += 1
                if state["ticks"] < 12 or time.monotonic() - state["since"] < 2.0:
                    return
                task = unreal.AutomationLibrary.take_high_res_screenshot(width, height, str(image), force_game_view=True)
                if not task or not task.is_valid_task():
                    raise RuntimeError(f"High-res screenshot did not return a live task: {image}")
                state.update(phase="capture", task=task)
                return
            if not state["task"].is_task_done() or not image.is_file():
                return
            blob = image.read_bytes()
            if (len(blob) <= 24 or blob[:8] != b"\x89PNG\r\n\x1a\n"
                    or struct.unpack(">II", blob[16:24]) != (width, height)):
                raise RuntimeError(f"Screenshot is not the requested {width}×{height} PNG: {image}")
            report["views"].append({"image": image.name, "camera_cm": camera_cm, "look_at_cm": look_at_cm,
                                    "size_px": [width, height], "size_bytes": len(blob),
                                    "sha256": hashlib.sha256(blob).hexdigest()})
            state["index"] += 1
            if state["index"] == len(views):
                finish()
            else:
                state["phase"] = "aim"
        except Exception:
            finish(traceback.format_exc())
        finally:
            state["busy"] = False

    levels.editor_set_game_view(True, viewport)
    state["handle"] = unreal.register_slate_post_tick_callback(tick)
    _active = state
    unreal.log(f"START_HUB_DETAIL_EDITOR_CAPTURES_ARMED count={len(views)} report={output}")
