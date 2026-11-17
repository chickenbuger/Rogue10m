"""Explicit owned unattended cold verification; no authoring, save or PIE.

The public QuitEditor API has no exit-status argument. The caller must require
both this fresh result's semantic pass and an observed process exit0; a normal
exit alone never converts failed validation into success.
"""
from __future__ import annotations

import importlib.util
import hashlib
import json
import os
import re
from pathlib import Path
import sys
import time
import traceback

ROOT = Path("D:/Project/Rogue10m")
BASE = ROOT / "Feature/doc/evidence/caves20261118"
OUT = BASE / "cold-final-validated-live"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
state = {"phase": "registry", "ticks": 0, "busy": False, "started": None}


def require(value, message):
    if not value:
        raise RuntimeError(message)


def write(name, value):
    with (OUT/(name+".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")


def file_sha256(file):
    digest=hashlib.sha256()
    with file.open("rb") as stream:
        while chunk:=stream.read(1024*1024):
            digest.update(chunk)
    return digest.hexdigest()


def loaded_project_module():
    """Read the current owned process's module path, never a guessed DLL."""
    import ctypes
    from ctypes import wintypes
    kernel=ctypes.WinDLL("kernel32",use_last_error=True)
    kernel.GetModuleHandleW.argtypes=[wintypes.LPCWSTR]
    kernel.GetModuleHandleW.restype=wintypes.HMODULE
    kernel.GetModuleFileNameW.argtypes=[wintypes.HMODULE,wintypes.LPWSTR,wintypes.DWORD]
    kernel.GetModuleFileNameW.restype=wintypes.DWORD
    handle=kernel.GetModuleHandleW("UnrealEditor-Rogue10m.dll")
    require(bool(handle),"Current process project DLL module was not found")
    buffer=ctypes.create_unicode_buffer(32768)
    length=kernel.GetModuleFileNameW(handle,buffer,len(buffer))
    require(0<length<len(buffer)-1,"Current process module filename unavailable or truncated")
    file=Path(buffer.value).resolve()
    require(file==ROOT/"Binaries/Win64/UnrealEditor-Rogue10m.dll","Unexpected loaded project DLL path")
    return {"path":file.relative_to(ROOT).as_posix(),"sha256":file_sha256(file),
            "source":"Current process GetModuleHandleW/GetModuleFileNameW; read-only metadata"}


def guid_string(value):
    # KismetGuidLibrary.h exposes Conv_GuidToString as ScriptMethod=ToString.
    # FGuid's individual words are not Python properties in installed UE5.8.
    text = value.to_string().replace("-", "").upper()
    require(re.fullmatch(r"[0-9A-F]{32}", text) is not None, "Invalid public GUID string")
    return text


def terrain_actor_inventory():
    # LandscapeGuid is reflected but not Python-readable: it lacks CPF_Edit /
    # BlueprintVisible. Use the existing native read-only public inspector,
    # then independently join every row to public Python actor metadata.
    native=json.loads(u.Rogue10mCrop20261114WorldAuthoring.inspect_candidate_terrain())
    require(native.get("success") is True and native.get("read_only_inspection") is True
            and native.get("dirty_package_names_unchanged") is True and native.get("map_package")==TARGET
            and native.get("packages_saved") is False and native.get("selection_performed") is False
            and native.get("persistent_terrain_data_written") is False,
            "Native read-only exact candidate metadata inspection failed")
    native_rows=native.get("proxies",[])
    native_by_path={row["path"]:row for row in native_rows}
    require(len(native_rows)==len(native_by_path)==17,"Exact17 unique native terrain rows required")
    def native_guid(text):
        value=str(text).replace("-","").upper()
        require(re.fullmatch(r"[0-9A-F]{32}",value) is not None,"Invalid observed native GUID")
        return value
    rows=[]
    for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
        if not isinstance(actor,u.LandscapeProxy):
            continue
        row={"package":actor.get_package().get_name(),"actor_path":actor.get_path_name(),
            "class":actor.get_class().get_path_name(),
            "actor_guid":guid_string(actor.get_editor_property("actor_guid")),
            "render_components":len(actor.get_components_by_class(u.LandscapeComponent)),
            "collision_components":len(actor.get_components_by_class(u.LandscapeHeightfieldCollisionComponent))}
        observed=native_by_path.get(row["actor_path"])
        require(observed is not None and observed["package"]==row["package"] and observed["class"]==row["class"]
                and native_guid(observed["actor_guid"])==row["actor_guid"]
                and observed["component_count"]==row["render_components"]
                and observed["collision_component_count"]==row["collision_components"],
                "Native/public Python actor metadata correspondence failed")
        row["landscape_guid"]=native_guid(observed["landscape_guid"])
        rows.append(row)
    rows.sort(key=lambda row:row["package"])
    require(len(rows)==len({row["package"] for row in rows})==17,"Exact17 observed Landscape/proxy actor packages required")
    require({row["actor_path"] for row in rows}==set(native_by_path)
            and len({row["actor_guid"] for row in rows})==17,"Native/public terrain actor sets differ")
    require(sum(row["render_components"] for row in rows)==sum(row["collision_components"] for row in rows)==45,
            "Actual terrain package inventory must account for45 render/collision components")
    require(all(row["landscape_guid"]==native_guid(native["landscape_guid"])
                =="147B73D247543E06FDA1A7AF9D467458" for row in rows),"Observed terrain landscape GUID differs")
    return {"schema":"Rogue10m.CaveColdTerrainActors.20261118.v1",
        "source":"Existing native read-only inspector GetLandscapeGuid joined to actual public Python actors by path/package/class/actor GUID/render and collision counts; no receipt-derived classification",
        "actor_count":len(rows),"render_components":45,"collision_components":45,"actors":rows,
        "native_python_actor_correspondence_count":len(rows),"native_read_only_inspection":True,
        "native_dirty_package_names_unchanged":True,
        "asset_mutations":False}


def restore_baseline_culture():
    """Match the Korean baseline after English startup smoke tests finish.

    These public setters affect only this owned cold process. They do not save
    user settings, assets or alter the exact graph/material comparison gates.
    """
    library=u.InternationalizationLibrary
    before={"language":library.get_current_language(),"locale":library.get_current_locale()}
    requested={"language":"ko","locale":"ko-KR"}
    require(before=={"language":"en","locale":"en"},"English startup language/locale required before baseline restoration")
    results={"language":library.set_current_language(requested["language"],False),
             "locale":library.set_current_locale(requested["locale"],False)}
    after={"language":library.get_current_language(),"locale":library.get_current_locale()}
    record={"schema":"Rogue10m.CaveColdCultureRestoration.20261118.v1","before":before,"requested":requested,
            "setter_results":results,"after":after,"save_to_config":False,"assets_saved":False,
            "comparison_fields_normalized_or_dropped":False,
            "stage":"After English engine startup and settled region; before metadata and complete validation",
            "reason":"MaterialExpressionBreakMaterialAttributes input name is NSLOCTEXT; compare in the exact original baseline language/locale."}
    require(all(results.values()) and after==requested,"Exact baseline language/locale restoration failed")
    return record


def quit_owned():
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
        require(time.monotonic()-state["started"] < 360, "Owned cave cold watchdog exceeded")
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
            require(native.get("success"), "Independent cold native inspection failed")
            if not native.get("landscape_reports_up_to_date"):
                return
            state["phase"] = "validating"
            write("culture-restoration",restore_baseline_culture())
            write("terrain-actor-packages",terrain_actor_inventory())
            packages = {}
            prefixes = ("/Game/__ExternalObjects__/"+TARGET[6:]+"/", "/Game/__ExternalActors__/"+TARGET[6:]+"/")
            for obj in u.ObjectIterator(u.Object):
                package = obj.get_package().get_name()
                if package.startswith(prefixes):
                    row = packages.setdefault(package, {"classes": set(), "object_paths": []})
                    row["classes"].add(obj.get_class().get_path_name())
                    row["object_paths"].append(obj.get_path_name())
            write("package-inventory", {"source": "Public loaded UObject class/package metadata; no property mutation",
                "packages": [{"package": k, "classes": sorted(v["classes"]), "object_paths": sorted(v["object_paths"])}
                             for k, v in sorted(packages.items())]})
            spec = importlib.util.spec_from_file_location("independent_cave_cold_validator", ROOT/"Scripts/Editor/ValidateCaves20261118.py")
            validator = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(validator)
            result = validator.validate_caves(require_saved_clean=True)
            write("validation", result)
            write("result", {"passed": result.get("passed") is True, "pid": os.getpid(),
                "assets_saved": False, "checks": len(result.get("checks", {})),
                "capsule_queries": result.get("capsule_query_count"), "diagnostics": result.get("diagnostics"),
                "traceback": result.get("traceback"), "runtime_camera_certified": False,
                "validation_language":"ko","validation_locale":"ko-KR","culture_saved_to_config":False,
                "elapsed_seconds": time.monotonic()-state["started"],
                "process_exit_code_observed_by_caller": None,
                "acceptance": "Require semantic passed=true AND separately observed process exit0."})
            if not result.get("passed"):
                u.log_error("Caves cold validation failed; retained report is authoritative")
            quit_owned()
    except Exception:
        error = traceback.format_exc()
        failure = {"passed": False, "pid": os.getpid(), "assets_saved": False,
                   "phase": state["phase"], "traceback": error}
        write("failure", failure)
        if not (OUT/"result.json").exists():
            write("result", failure)
        u.log_error("CAVES COLD "+error)
        quit_owned()
    finally:
        state["busy"] = False


def bootstrap():
    global u
    import unreal as u
    command = u.SystemLibrary.get_command_line().lower()
    # Check ownership before any quit, load, callback or report creation.
    require("-roguecavestage=cold" in command and "-unattended" in command and "-nullrhi" in command,
            "Explicit owned unattended nullrhi cave cold launch required")
    require("bautosaveenable=false" in command and "bautostartserver=false" in command,
            "Cold launch must explicitly disable autosave and MCP auto-start")
    require(Path(u.Paths.project_dir()).resolve() == ROOT, "Exact physical project required")
    sys.dont_write_bytecode = True
    OUT.mkdir(parents=True, exist_ok=False)
    state["started"] = time.monotonic()
    write("startup", {"pid": os.getpid(), "target": TARGET, "unattended": True,
        "nullrhi": True, "autosave_and_mcp_autostart_disabled_by_launch": True,
        "assets_saved": False, "command_line_stage": "RogueCaveStage=cold",
        "startup_culture":{"language":u.InternationalizationLibrary.get_current_language(),
                           "locale":u.InternationalizationLibrary.get_current_locale()},
        "validation_culture_requested":{"language":"ko","locale":"ko-KR"},
        "culture_restore_save_to_config":False,
        "script_hashes":{relative:file_sha256(ROOT/relative) for relative in
            ("Scripts/Editor/ValidateCaves20261118.py","Scripts/Editor/ColdCaves20261118.py")},
        "loaded_project_dll":loaded_project_module()})
    state["handle"] = u.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    bootstrap()
