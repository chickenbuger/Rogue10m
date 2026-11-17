"""Owner-launched, isolated null-RHI editor loader for the saved road candidate.

Import is inert. Execute this file with ``py`` in a NEW UnrealEditor-Cmd process
whose startup map is the exact candidate, using -unattended -nullrhi and one
unique -RogueRoadSurfaceColdRun=<tag>. Do not use a Python commandlet: this
loader needs normal editor ticks. It loads only the existing bounded native
editor region, waits for it to settle, and invokes the separate strict verifier
once. It never saves, authors, selects, recompiles, starts PIE or repairs data.
It closes only the explicitly tagged unattended/null-RHI process on completion.
The host must preserve launch/PID/exit/log evidence and confirm the prior save.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import time
import traceback


ROOT = Path("D:/Project/Rogue10m")
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
ROAD = ROOT / "Feature/doc/evidence/landscape-road-surface20261117"
RESUME = ROOT / "Feature/doc/evidence/landscape-resume-20261117"
VERIFIER = ROOT / "Scripts/Editor/VerifyLandscapeRoadSurface20261117.py"
MAX_WALL_SECONDS = 300.0
_state = None


def require(value, message):
    if not value:
        raise RuntimeError(message)


def fingerprint(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            value.update(chunk)
    return {"path": path.relative_to(ROOT).as_posix(),
            "bytes": path.stat().st_size, "sha256": value.hexdigest()}


def start():
    """Run once in the explicitly tagged NEW headless editor; never attach."""
    global _state
    require(_state is None, "This loader may be started only once per process")
    import unreal as u

    command = u.SystemLibrary.get_command_line()
    tags = re.findall(r"(?:^|\s)-RogueRoadSurfaceColdRun=([A-Za-z0-9-]{1,64})(?=\s|$)", command)
    require(len(tags) == 1 and command.lower().count("-rogueroadsurfacecoldrun=") == 1,
            "Exactly one unique -RogueRoadSurfaceColdRun=<tag> is required")
    require(re.search(r"(?:^|\s)-unattended(?=\s|$)", command, re.I)
            and re.search(r"(?:^|\s)-nullrhi(?=\s|$)", command, re.I),
            "Only an explicitly unattended null-RHI editor process is permitted")
    require(not re.search(r"(?:^|\s)-(?:run=|executepythonscript=|game(?=\s|$)|server(?=\s|$))", command, re.I),
            "Use normal editor startup plus ExecCmds py; commandlet/game/server execution is unsupported")
    require(Path(u.Paths.project_dir()).resolve() == ROOT,
            "Only the exact physical D:/Project/Rogue10m project is permitted")
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
    require(not editor.get_game_world() and not levels.is_in_play_in_editor(), "PIE must be stopped")

    out = ROAD / ("cold-loader-" + tags[0])
    out.mkdir(parents=True, exist_ok=False)
    state = {"started": time.monotonic(), "phase": "wait-registry", "ticks": 0,
             "settle_started": None, "handle": None, "completed": False}
    _state = state

    def write(name, value):
        with (out / (name + ".json")).open("x", encoding="utf-8") as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2)
            stream.write("\n")

    def dirty():
        return sorted({p.get_path_name() for p in
            list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
            + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})

    def guard():
        require(time.monotonic() - state["started"] < MAX_WALL_SECONDS, "Cold verification watchdog exceeded")
        require(not editor.get_game_world() and not levels.is_in_play_in_editor(), "Unexpected PIE/game world")
        world = editor.get_editor_world()
        require(world is not None and world.get_path_name().split(".")[0] == TARGET, "Wrong exact candidate world")
        require(not dirty(), "Cold checkpoint became dirty; no save or repair is permitted")

    def finish(passed, error=None):
        if state["completed"]:
            return
        state["completed"] = True
        if state["handle"] is not None:
            u.unregister_slate_post_tick_callback(state["handle"])
            state["handle"] = None
        write("result", {"passed": passed, "error": error, "phase": state["phase"],
                         "elapsed_wall_seconds": time.monotonic() - state["started"],
                         "finished_utc": datetime.now(timezone.utc).isoformat(),
                         "process_id": os.getpid(), "assets_saved": False,
                         "repair_or_recompile_requested": False,
                         "strict_comparison": "Feature/doc/evidence/landscape-road-surface20261117/final-cold/comparison.json",
                         "scope": "CPU saved-data and loaded reflected-property inspection; no rendered, physical-input or project-wide dependency certification"})
        if passed:
            print("LANDSCAPE_ROAD_SURFACE_HEADLESS_20261117 PASS " + str(out))
        else:
            u.log_error("LANDSCAPE_ROAD_SURFACE_HEADLESS_20261117 FAIL " + str(error))
        # This is the uniquely tagged unattended/null-RHI process required above.
        # No other editor process is enumerated, attached, closed or terminated.
        u.SystemLibrary.quit_editor()

    def fail():
        error = traceback.format_exc()
        write("failure", {"passed": False, "phase": state["phase"], "traceback": error,
                          "dirty_packages": dirty(), "assets_saved": False})
        finish(False, error)

    def tick(_delta):
        try:
            guard()
            registry = u.AssetRegistryHelpers.get_asset_registry()
            if registry.is_loading_assets():
                return
            helper = u.Rogue10mCrop20261114WorldAuthoring
            if state["phase"] == "wait-registry":
                loaded = json.loads(helper.load_candidate_region())
                write("bounded-region-load", loaded)
                require(loaded.get("success") is True and loaded.get("editor_state_only") is True
                        and loaded.get("editor_region_loaded") is True
                        and loaded.get("dirty_package_names_unchanged") is True,
                        "Bounded native editor-region load failed; no retry/import/repair")
                guard()
                state["phase"] = "settling"
                state["settle_started"] = time.monotonic()
                return
            if state["phase"] == "settling":
                state["ticks"] += 1
                if state["ticks"] < 15 or time.monotonic() - state["settle_started"] < 3.0:
                    return
                terrain = json.loads(helper.inspect_candidate_terrain())
                write("ready-terrain", terrain)
                require(terrain.get("success") is True and terrain.get("all_components_registered") is True
                        and terrain.get("retained_9_by_5_lattice_observed") is True
                        and all(terrain.get(key) == 45 for key in ("component_count", "collision_component_count",
                            "unique_component_count", "unique_collision_component_count")),
                        "Saved candidate is not the exact registered retained45 lattice")
                guard()
                state["phase"] = "strict-verification"
                spec = importlib.util.spec_from_file_location("road_cold_verify_20261117", VERIFIER)
                require(spec is not None and spec.loader is not None, "Cannot load strict verifier")
                verifier = importlib.util.module_from_spec(spec)
                previous = sys.dont_write_bytecode
                sys.dont_write_bytecode = True
                try:
                    spec.loader.exec_module(verifier)
                finally:
                    sys.dont_write_bytecode = previous
                # A new host-owned process loaded the saved exact candidate;
                # the separate verifier still requires clean packages and saved files.
                report = verifier.validate(reload_confirmed=True)
                require(report.get("passed") is True and report.get("status") == "passed_read_only_saved_data",
                        "Strict comparison did not pass")
                guard()
                state["phase"] = "complete"
                finish(True)
        except Exception:
            fail()

    try:
        guard()
        require(not (ROAD / "final-cold").exists(), "Preserve existing final-cold evidence; do not rerun over it")
        inputs = [Path(__file__).resolve(), VERIFIER, ROAD / "preflight.json",
                  RESUME / "workbench/cold-reload/retained-height-paint-layers.json",
                  RESUME / "workbench/cold-reload/retained-dependencies.json",
                  RESUME / "baseline-files.json", RESUME / "protected-save-files.json"]
        write("context", {"process_id": os.getpid(), "run_tag": tags[0], "command_line": command,
                          "started_utc": datetime.now(timezone.utc).isoformat(), "project": str(ROOT),
                          "target": TARGET, "headless_null_RHI": True, "host_fresh_process_and_prior_save_required": True,
                          "input_fingerprints": [fingerprint(path) for path in inputs],
                          "native_region_load_limit_cm": [[-91000, -91000, -100000], [91000, 91000, 100000]],
                          "assets_saved": False, "force_update_or_recompile": False})
        state["handle"] = u.register_slate_post_tick_callback(tick)
    except Exception:
        fail()


if __name__ == "__main__":
    start()
