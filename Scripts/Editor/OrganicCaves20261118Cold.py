"""Inert owned-process cold regression for saved continuous cave shells.

Explicit bootstrap requires its stage/config fingerprint and unattended nullrhi
launch. Only that owned process is quit. Semantic validation and separately
observed same-PID exit0 are both required; normal exit never means QA passed.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import time
import traceback

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Feature/doc/evidence/cave-organic20261118"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
state = {"phase": "not_started", "ticks": 0, "busy": False}


def require(value, message):
    if not value:
        raise RuntimeError(message)


def write(name, value):
    with (state["out"] / (name+".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")


def quit_owned():
    require(state.get("owned") is True, "Quit requires the explicit owned process guard")
    handle = state.pop("handle", None)
    if handle is not None:
        u.unregister_slate_post_tick_callback(handle)
    state["phase"] = "exiting"
    u.SystemLibrary.quit_editor()


def tick(_):
    if state["busy"]:
        return
    state["busy"] = True
    try:
        require(time.monotonic()-state["started"] < 480, "Organic cold watchdog expired")
        if u.AssetRegistryHelpers.get_asset_registry().is_loading_assets():
            return
        editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
        require(not editor.get_game_world() and editor.get_editor_world().get_path_name().split(".")[0] == TARGET,
                "Exact idle cold candidate world required")
        if state["phase"] == "registry":
            loaded = json.loads(u.Rogue10mCrop20261114WorldAuthoring.load_candidate_region())
            write("region-load", loaded)
            require(loaded.get("success") is True, "Independent cold full-region load failed")
            state.update(phase="settling", ticks=0)
            return
        state["ticks"] += 1
        if state["phase"] == "settling":
            if state["ticks"] < 30:
                return
            native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
            require(native.get("success"), "Cold native terrain inspection failed")
            if not native.get("landscape_reports_up_to_date"):
                return
            state["phase"] = "validating"
            # Pinned corrected predecessor helpers already distinguish public
            # GUID serialization and true native/public17 actor correspondence.
            write("culture-restoration", state["helpers"].restore_baseline_culture())
            write("terrain-actor-packages", state["helpers"].terrain_actor_inventory())
            prefixes = ("/Game/__ExternalActors__/"+TARGET[6:]+"/",
                        "/Game/__ExternalObjects__/"+TARGET[6:]+"/",
                        "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/"
                        "NaturalBoundary20261117/Caves20261118/Organic20261118/")
            packages = {}
            for obj in u.ObjectIterator(u.Object):
                package = obj.get_package().get_name()
                if package.startswith(prefixes):
                    row = packages.setdefault(package, {"classes": set(), "object_paths": []})
                    row["classes"].add(obj.get_class().get_path_name())
                    row["object_paths"].append(obj.get_path_name())
            write("package-inventory", {"source": "Public loaded UObject class/package metadata; no property mutation",
                "packages": [{"package": key, "classes": sorted(value["classes"]), "object_paths": sorted(value["object_paths"])}
                             for key, value in sorted(packages.items())]})
            result = state["qa"].validate_organic_caves(state["config"], require_saved_clean=True)
            write("validation", result)
            write("result", {"passed": result.get("passed") is True, "pid": os.getpid(),
                "assets_saved": False, "checks": len(result.get("checks", {})),
                "capsule_queries": result.get("capsule_query_count"), "diagnostics": result.get("diagnostics"),
                "traceback": result.get("traceback"), "runtime_camera_certified": False,
                "validation_language": "ko", "validation_locale": "ko-KR", "culture_saved_to_config": False,
                "elapsed_seconds": time.monotonic()-state["started"],
                "process_exit_code_observed_by_caller": None,
                "acceptance": "Require semantic passed=true AND separately observed same-PID process exit0."})
            if not result.get("passed"):
                u.log_error("Organic cave cold validation failed; retained report is authoritative")
            quit_owned()
    except Exception:
        error = traceback.format_exc()
        failure = {"passed": False, "pid": os.getpid(), "assets_saved": False,
                   "phase": state["phase"], "traceback": error}
        write("failure", failure)
        if not (state["out"] / "result.json").exists():
            write("result", failure)
        u.log_error("ORGANIC_CAVES_COLD " + error)
        quit_owned()
    finally:
        state["busy"] = False


def bootstrap():
    global u
    import unreal as u
    command = u.SystemLibrary.get_command_line()
    lower = command.lower()
    require("-rogueorganiccavestage=cold" in lower and "-unattended" in lower and "-nullrhi" in lower
            and "bautosaveenable=false" in lower and "bautostartserver=false" in lower,
            "Explicit owned unattended nullrhi organic-cave cold launch with autosave/MCP disabled required")
    require(Path(u.Paths.project_dir()).resolve() == ROOT, "Exact physical project required")
    match = re.search(r"(?:^|\s)-RogueOrganicCaveConfigSHA256=([0-9a-f]{64})(?:\s|$)", command, re.I)
    run = re.search(r"(?:^|\s)-RogueOrganicCaveRun=(cold-[a-z0-9_-]{1,64})(?:\s|$)", command, re.I)
    require(match is not None and run is not None, "Explicit config fingerprint and bounded fresh cold-* run name required")
    config_file = BASE / "validation-config.json"
    require(hashlib.sha256(config_file.read_bytes()).hexdigest() == match[1].lower(), "Owned cold config changed")
    spec = importlib.util.spec_from_file_location("owned_organic_cave_qa", ROOT / "Scripts/Editor/OrganicCaves20261118QA.py")
    qa = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True
    spec.loader.exec_module(qa)
    helpers = qa.pinned_module("owned_organic_cold_public_helpers", "Scripts/Editor/ColdCaves20261118.py", qa.COLD_SHA256)
    helpers.u = u
    output = BASE / run[1]
    require(not output.exists(), "Preserve existing cold evidence")
    output.mkdir(parents=True, exist_ok=False)
    state.update(out=output, owned=True, started=time.monotonic(), phase="registry", qa=qa, helpers=helpers,
                 config=json.loads(config_file.read_text(encoding="utf-8-sig")))
    write("startup", {"pid": os.getpid(), "target": TARGET, "unattended": True, "nullrhi": True,
        "autosave_and_mcp_autostart_disabled_by_launch": True, "assets_saved": False,
        "command_line_stage": "RogueOrganicCaveStage=cold", "validation_config_file_sha256": match[1].lower(),
        "startup_culture": {"language": u.InternationalizationLibrary.get_current_language(),
                            "locale": u.InternationalizationLibrary.get_current_locale()},
        "validation_culture_requested": {"language": "ko", "locale": "ko-KR"},
        "culture_restore_save_to_config": False, "loaded_project_dll": helpers.loaded_project_module(),
        "script_hashes": {relative: hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() for relative in
            ("Scripts/Editor/OrganicCaves20261118QA.py", "Scripts/Editor/OrganicCaves20261118Cold.py")},
        "corrected_public_terrain_and_culture_helper_sha256": qa.COLD_SHA256})
    state["handle"] = u.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    bootstrap()
