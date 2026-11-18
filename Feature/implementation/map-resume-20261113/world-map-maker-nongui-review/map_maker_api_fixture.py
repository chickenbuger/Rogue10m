"""UNEXECUTED owner-run Actor data API persistence fixture. No UI/selection API.

Default invocation is a pure plan and does not import unreal. Execution requires a
separate physical project, a verified official WP duplicate, and Root's QA handoff.
This never invokes Rogue10mMapMaker::FModel or its Slate panel.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import sys
from datetime import datetime, timezone

ORIGIN = Path("D:/Project/Rogue10m")
PACKAGE = re.compile(r"^/Game/Rogue10m/Maps/QA/MapMakerMoveReload_[0-9A-Fa-f]{32}$")
HASH = re.compile(r"^[0-9A-Fa-f]{64}$")
ROOTS = ("Source", "Config", "Scripts", "Content", "Plugins", "Build", "Binaries",
         "Rogue10m.uproject", "Saved/SaveGames")
EDITOR_FILES = ("UnrealEditor-Rogue10m.dll", "UnrealEditor-Rogue10m.pdb",
                "UnrealEditor-Rogue10mEditor.dll", "UnrealEditor-Rogue10mEditor.pdb",
                "UnrealEditor.modules", "Rogue10mEditor.target")
FORBIDDEN_LABELS = ("Boss", "Boundary", "PlayerStart", "Bootstrap", "StreamProbe", "Checkpoint")
FOLDERS = ("Scenery", "Camp", "Roads", "River", "Bridges", "Wayfinding", "MapMaker")


class FixtureError(RuntimeError):
    pass


def require(value, message):
    if not value:
        raise FixtureError(message)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def no_links(path):
    """Reject Windows reparse points as well as POSIX symbolic links."""
    path = Path(os.path.abspath(path))
    for part in (path, *path.parents):
        if not os.path.lexists(part):
            continue
        info = part.lstat()
        require(not stat.S_ISLNK(info.st_mode) and not
                (getattr(info, "st_file_attributes", 0) & 0x400),
                "Physical path contains a link/reparse point: " + str(part))
    return path


def package_paths(package):
    require(bool(PACKAGE.fullmatch(package)), "Require a fresh GUID-named QA map package.")
    tail = package[len("/Game/"):]
    return ("Content/" + tail + ".umap", "Content/" + tail + "_BuiltData.uasset",
            "Content/__ExternalActors__/" + tail, "Content/__ExternalObjects__/" + tail)


def belongs(package, candidate):
    tail = candidate[len("/Game/"):]
    return package == candidate or package == candidate + "_BuiltData" or any(
        package.startswith(prefix + tail + "/")
        for prefix in ("/Game/__ExternalActors__/", "/Game/__ExternalObjects__/"))


def vector(value):
    result = [float(value.x), float(value.y), float(value.z)]
    require(all(math.isfinite(x) for x in result), "Nonfinite actor vector.")
    return result


def close(left, right, epsilon=0.001):
    return len(left) == len(right) and all(abs(a - b) <= epsilon for a, b in zip(left, right))


def cell_center(cell):
    require(len(cell) == 2 and all(type(x) is int and -4 <= x <= 3 for x in cell),
            "Destination cell must contain two integers in -4..3.")
    return [(max(-90000, x * 25600) + min(90000, (x + 1) * 25600)) / 2 for x in cell]


def current_process_identity():
    """Read only this process's Windows creation identity; never inspect/stop another PID."""
    import ctypes
    from ctypes import wintypes
    require(os.name == "nt", "Actual fixture execution requires the Windows owner host.")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = wintypes.HANDLE
    kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    kernel.GetProcessTimes.restype = wintypes.BOOL
    created, exited, system, user = (wintypes.FILETIME() for _ in range(4))
    require(kernel.GetProcessTimes(kernel.GetCurrentProcess(), ctypes.byref(created), ctypes.byref(exited),
                                   ctypes.byref(system), ctypes.byref(user)), "Current process creation time unavailable.")
    return {"pid": os.getpid(), "creation_filetime_100ns": str((created.dwHighDateTime << 32) | created.dwLowDateTime)}


def current_executable():
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetModuleFileNameW.argtypes = [wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD]
    kernel.GetModuleFileNameW.restype = wintypes.DWORD
    buffer = ctypes.create_unicode_buffer(32768)
    length = kernel.GetModuleFileNameW(None, buffer, len(buffer))
    require(0 < length < len(buffer) - 1, "Current executable path unavailable/truncated.")
    return no_links(buffer.value)


def require_commandlet(command, executable, engine_root):
    runs = re.findall(r'(?:^|\s)-run=(?:"([^"]+)"|([^\s]+))', command, re.I)
    require(len(runs) == 1 and (runs[0][0] or runs[0][1]).lower() == "pythonscript",
            "Only the PythonScript commandlet may execute this fixture.")
    require(all(re.search(r'(?:^|\s)-' + flag + r'(?:\s|$)', command, re.I)
                for flag in ("Unattended", "NullRHI")), "Unattended and NullRHI switches are required.")
    require(Path(executable) == Path(engine_root) / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe",
            "The exact installed UnrealEditor-Cmd executable is required.")


def validate_move_exit(receipt, previous, root, engine_root):
    require(receipt.get("schema_version") == 1 and receipt.get("status") == "owned_process_exited"
            and receipt.get("identity") == previous["process_identity"] and receipt.get("exit_code") == 0
            and receipt.get("exit_confirmed") is True
            and receipt.get("receipt_written_after_wait_for_exit") is True
            and Path(receipt.get("project_file", "")) == root / "Rogue10m.uproject"
            and Path(receipt.get("executable", "")) == Path(engine_root) / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe",
            "Cold readback requires the pinned owner launcher receipt for the exact exited move process.")
    finished = datetime.fromisoformat(previous["finished_utc"])
    ended = datetime.fromisoformat(receipt["ended_utc"])
    require(finished.tzinfo is not None and ended.tzinfo is not None and ended >= finished,
            "Move exit receipt has an invalid chronological/UTC boundary.")


def protected_snapshot(root, exclusions=(), require_single_link=False):
    root = no_links(root)
    files, missing = {}, []
    exclusions = tuple(Path(x).as_posix().rstrip("/") for x in exclusions)
    for relative in ROOTS:
        target = no_links(root / relative)
        if not target.exists():
            missing.append(relative)
            continue
        leaves = [target] if target.is_file() else []
        if target.is_dir():
            for directory, directories, names in os.walk(target, followlinks=False):
                no_links(directory)
                for name in directories:
                    no_links(Path(directory) / name)
                leaves.extend(Path(directory) / name for name in names)
        for leaf in leaves:
            no_links(leaf)
            if require_single_link:
                require(leaf.stat().st_nlink == 1, "QA input must be a physical copy, never a hardlink: " + str(leaf))
            key = leaf.relative_to(root).as_posix()
            if any(key == x or key.startswith(x + "/") for x in exclusions):
                continue
            files[key] = sha(leaf)
    return {"files": dict(sorted(files.items())), "missing_roots": sorted(missing)}


def local_save_snapshot():
    require(bool(os.environ.get("LOCALAPPDATA")), "LOCALAPPDATA is required on the Windows owner host.")
    folder = no_links(Path(os.environ["LOCALAPPDATA"]) / "Rogue10m/Saved/SaveGames")
    rows = {}
    if folder.exists():
        for directory, directories, names in os.walk(folder, followlinks=False):
            for name in directories:
                no_links(Path(directory) / name)
            for name in names:
                path = no_links(Path(directory) / name)
                rows[path.relative_to(folder).as_posix()] = sha(path)
    return {"exists": folder.exists(), "files": dict(sorted(rows.items()))}


def write_new(path, data):
    path = no_links(path)
    require(not path.exists(), "Preserve existing evidence: " + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")


def build_origin_guard(output):
    """Owner may run this without UE before launching either fresh process."""
    output = no_links(output)
    require(output.is_relative_to(ORIGIN / "Feature/implementation/map-resume-20261113/world-map-maker-nongui-review")
            or output.is_relative_to(Path(os.environ["TEMP"])), "Guard output must be in this packet or TEMP.")
    payload = {"schema_version": 1, "origin": str(ORIGIN), "roots": list(ROOTS),
               "snapshot": protected_snapshot(ORIGIN), "local_saves": local_save_snapshot()}
    write_new(output, payload)
    return {"guard": str(output), "sha256": sha(output), "unreal_imported": False}


def read_pinned(path, expected):
    no_links(path)
    require(bool(HASH.fullmatch(expected or "")) and sha(path).lower() == expected.lower(),
            "Pinned JSON identity differs: " + str(path))
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def plan(config):
    candidate = config.get("candidate_map")
    if candidate:
        package_paths(candidate)
    if config.get("destination_cell") is not None:
        cell_center(config["destination_cell"])
    return {"status": "UNEXECUTED", "scope": "underlying_Actor_API_only",
            "actual_map_maker_model_tested": False, "panel_tested": False,
            "selection_or_ui_input_used": False, "unreal_imported": False,
            "candidate": candidate, "requires": ["Root exclusive QA execution handoff", "fresh physical QA project",
            "official WP duplicate without Rename and pinned receipt", "pinned original/save guard",
            "canonical Editor6 SHA pins", "audited Python startup scripts", "PythonScript commandlet only",
            "separate UserDir", "explicit actor paths",
            "move process exit then a different cold-verify process"]}


def preflight(config, owner_handoff, require_actors=True):
    require(config.get("root_exclusive_qa_execution_authorized") is True and bool(owner_handoff.strip()),
            "Root must explicitly authorize isolated QA execution and provide the exclusive engine handoff.")
    candidate = config["candidate_map"]
    excluded = package_paths(candidate)
    root = no_links(config["execution_project_root"])
    require(root.is_absolute() and root != ORIGIN
            and not root.is_relative_to(ORIGIN) and not ORIGIN.is_relative_to(root),
            "Execution requires a separate physical QA project, never the original or its child/parent.")
    require((root / "Rogue10m.uproject").is_file(), "Independent project file missing.")
    no_links(root / "Content")
    original = read_pinned(config["origin_guard"], config["origin_guard_sha256"])
    require(original.get("roots") == list(ROOTS) and Path(original.get("origin", "")) == ORIGIN,
            "Origin guard coverage does not match this fixture.")
    require(protected_snapshot(ORIGIN) == original["snapshot"] and local_save_snapshot() == original["local_saves"],
            "Original project or default user saves changed since the pinned guard.")
    receipt = read_pinned(config["copy_receipt"], config["copy_receipt_sha256"])
    require(receipt.get("schema_version") == 1 and receipt.get("status") == "independent_wp_copy_verified"
            and Path(receipt.get("project_root", "")) == root and receipt.get("candidate_map") == candidate
            and receipt.get("source_map") == "/Game/Rogue10m/Maps/OpenRunMap"
            and receipt.get("physical_snapshot_verified") is True
            and receipt.get("destination_absent_before") is True
            and receipt.get("rename_switch") is False and receipt.get("builder_exit_code") == 0
            and receipt.get("python_startup_audited_no_project_writes_or_ui") is True
            and receipt.get("source_hash_changes") == [], "Verified physical WP duplicate receipt is required.")
    require((root / excluded[0]).is_file(), "New copied QA map is missing.")
    require(all((root / "Binaries/Win64" / name).is_file() for name in EDITOR_FILES), "Canonical Editor6 missing.")
    pins = receipt.get("editor6_sha256", {})
    require(set(pins) == set(EDITOR_FILES) and all(HASH.fullmatch(pins[name] or "")
            and sha(root / "Binaries/Win64" / name).lower() == pins[name].lower() for name in EDITOR_FILES),
            "All six canonical Editor files require matching frozen SHA pins.")
    require(sha(root / "Binaries/Win64/UnrealEditor-Rogue10m.dll").lower()
            == config.get("editor_runtime_sha256", "").lower(), "Runtime DLL pin differs.")
    if require_actors:
        require(len(config.get("actor_paths", [])) in range(1, 17), "Provide 1..16 explicit candidate actor paths.")
        require(len(set(config["actor_paths"])) == len(config["actor_paths"]), "Actor paths must be unique.")
        for path in config["actor_paths"] + [config["witness_actor_path"]]:
            require(path.startswith(candidate + "." + candidate.rsplit("/", 1)[1] + ":PersistentLevel."),
                    "Actor path is outside the independent candidate.")
        require(config["witness_actor_path"] not in config["actor_paths"], "Witness must be outside the move group.")
    user_dir = no_links(config["user_dir"])
    require(user_dir.is_relative_to(root.parent) and not user_dir.is_relative_to(root)
            and not user_dir.is_relative_to(ORIGIN), "UserDir must be a separate sibling of this QA clone.")
    return root, candidate, excluded, original, user_dir


def get_descs(unreal):
    result = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    if isinstance(result, tuple) and len(result) == 2 and type(result[0]) is bool:
        require(result[0], "Descriptor API returned failure.")
        result = result[1]
    require(result is not None and not isinstance(result, (str, bool)), "Unexpected descriptor API result.")
    return list(result)


def dirty_packages(unreal):
    utils = unreal.EditorLoadingAndSavingUtils
    rows = {}
    for package in list(utils.get_dirty_map_packages()) + list(utils.get_dirty_content_packages()):
        rows[package.get_name()] = package
    return rows


def actor_record(actor):
    component = actor.get_component_by_class(__import__("unreal").StaticMeshComponent)
    require(component is not None, "Missing StaticMeshComponent.")
    mesh = component.get_editor_property("static_mesh")
    require(mesh is not None, "Missing static mesh reference.")
    rotation = actor.get_actor_rotation()
    owner = actor.get_owner()
    return {"path": actor.get_path_name(), "name": actor.get_name(),
            "guid": str(actor.get_editor_property("actor_guid")), "package": actor.get_package().get_name(),
            "class": actor.get_class().get_path_name(), "location": vector(actor.get_actor_location()),
            "rotation": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
            "scale": vector(actor.get_actor_scale3d()), "mesh": mesh.get_path_name(),
            "materials": [m.get_path_name() if m else None for m in
                          (component.get_material(i) for i in range(component.get_num_materials()))],
            "owner": owner.get_path_name() if owner else None,
            "folder": str(actor.get_folder_path()), "tags": [str(x) for x in actor.get_editor_property("tags")]}


def allowed_actor(actor, world, candidate):
    require(actor.get_class().get_path_name() == "/Script/Engine.StaticMeshActor", "Only exact native StaticMeshActor is allowed.")
    require(actor.get_typed_outer(__import__("unreal").World) == world
            and actor.get_level() == world.get_editor_property("persistent_level"),
            "Actor must belong to the copied persistent world.")
    require(belongs(actor.get_package().get_name(), candidate) and actor.is_package_external(),
            "Actor package is not independently externalized for this exact candidate.")
    require(not actor.get_editor_property("lock_location") and not actor.get_level().get_editor_property("locked"),
            "Movement/level lock is active.")
    require(actor.get_attach_parent_actor() is None and not list(actor.get_attached_actors()), "Attached actor hierarchies are excluded.")
    root = actor.get_editor_property("root_component")
    require(root is not None and not list(root.get_editor_property("attach_children")), "Root attachment children are excluded.")
    label, folder = actor.get_actor_label(), str(actor.get_folder_path())
    require("Rogue10m.OpenWorld" in [str(x) for x in actor.get_editor_property("tags")]
            and any(folder == "OpenWorld/" + x or folder.startswith("OpenWorld/" + x + "/") for x in FOLDERS)
            and not any(x in label for x in FORBIDDEN_LABELS), "Protected/unowned environment actor.")


def run_phase(config, phase, owner_handoff, move_exit_receipt=None, move_exit_sha256=None):
    root, candidate, excluded, original, user_dir = preflight(config, owner_handoff, phase != "discover")
    evidence = no_links(root / config["evidence_relative"])
    require(evidence.is_relative_to(root / "Feature") and ".." not in Path(config["evidence_relative"]).parts,
            "Evidence must stay below this QA project's Feature directory.")
    output = evidence / (phase + "-result.json")
    marker = evidence / (phase + "-started.json")
    require(not output.exists() and not marker.exists(), "Phase evidence/start marker exists; never replay partial work.")
    identity = current_process_identity()
    previous = None
    if phase == "cold_verify":
        previous = json.loads((evidence / "move-result.json").read_text(encoding="utf-8"))
        require(previous.get("status") == "moved_saved_cold_verify_pending" and previous["process_identity"] != identity,
                "Cold verification requires a different process after a successful saved move.")
        require(previous["config_sha256"] == config["_config_sha256"], "Move/cold config pin differs.")
        require(previous["fixture_sha256"] == sha(__file__), "Move/cold fixture source changed.")
        require(move_exit_receipt is not None, "Owner's exact exited-move receipt is required.")
        validate_move_exit(read_pinned(move_exit_receipt, move_exit_sha256), previous, root, config["engine_root"])
    clone_before = protected_snapshot(root, excluded, require_single_link=True)
    baseline = evidence / "move-clone-baseline.json"
    if phase == "cold_verify":
        require(sha(baseline) == previous["clone_baseline_sha256"]
                and json.loads(baseline.read_text(encoding="utf-8")) == clone_before,
                "Protected QA inputs changed between move and cold readback.")
    write_new(marker, {"status": "started_do_not_replay", "phase": phase, "identity": identity,
                       "config_sha256": config["_config_sha256"], "fixture_sha256": sha(__file__)})
    if phase == "move":
        write_new(baseline, clone_before)
    import unreal  # Deliberately deferred until all execution gates above passed.
    report = {"status": "started", "phase": phase, "pid": os.getpid(), "candidate": candidate,
              "process_identity": identity,
              "started_utc": datetime.now(timezone.utc).isoformat(), "config_sha256": config["_config_sha256"],
              "fixture_sha256": sha(__file__), "scope": "underlying_Actor_API_only",
              "panel_tested": False, "actual_map_maker_model_tested": False,
              "selection_or_ui_input_used": False, "full_run_tested": False}
    if phase == "move":
        report["clone_baseline_sha256"] = sha(baseline)
    try:
        actual = no_links(Path(unreal.Paths.get_project_file_path()).parent)
        require(actual == root, "Unreal is attached to the wrong project; no map operation allowed.")
        command = unreal.SystemLibrary.get_command_line()
        require_commandlet(command, current_executable(), config["engine_root"])
        matches = re.findall(r'(?:^|\s)-UserDir=(?:"([^"]+)"|([^\s]+))', command, re.I)
        require(len(matches) == 1, "Require exactly one isolated UserDir argument.")
        match = re.search(r'(?:^|\s)-UserDir=(?:"([^"]+)"|([^\s]+))', command, re.I)
        require(match is not None and no_links(match.group(1) or match.group(2)) == user_dir, "Exact isolated UserDir argument missing.")
        engine_modules = no_links(Path(config["engine_root"]) / "Engine/Binaries/Win64/UnrealEditor.modules")
        engine_id = json.loads(engine_modules.read_text(encoding="utf-8-sig"))["BuildId"]
        module = json.loads((root / "Binaries/Win64/UnrealEditor.modules").read_text(encoding="utf-8-sig"))
        require(module["BuildId"] == engine_id and module["Modules"].get("Rogue10m") == "UnrealEditor-Rogue10m.dll"
                and module["Modules"].get("Rogue10mEditor") == "UnrealEditor-Rogue10mEditor.dll", "Engine/module identity differs.")
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        require(editor.get_game_world() is None and not dirty_packages(unreal), "Fresh non-PIE clean editor context required.")
        world = unreal.EditorLoadingAndSavingUtils.load_map(str(root / excluded[0]))
        require(world is not None and world.get_outermost().get_name() == candidate, "Exact independent map load failed.")
        descriptors = get_descs(unreal)
        require(descriptors and all(belongs(str(x.get_editor_property("actor_package")), candidate) for x in descriptors),
                "Copied actor/external packages are mixed with another map; refuse movement.")
        if phase == "discover":
            meshes = []
            for descriptor in descriptors:
                native = descriptor.get_editor_property("native_class")
                if native is not None and native.get_path_name() == "/Script/Engine.StaticMeshActor":
                    meshes.append({"path": str(descriptor.get_editor_property("actor_path")),
                                   "guid": str(descriptor.get_editor_property("guid")),
                                   "label": str(descriptor.get_editor_property("label")),
                                   "package": str(descriptor.get_editor_property("actor_package"))})
            report["descriptor_count"] = len(descriptors)
            report["native_static_mesh_descriptors"] = sorted(meshes, key=lambda x: x["path"])
            report["actor_eligibility_checked"] = False
            require(meshes, "No native StaticMeshActor descriptors found; movement remains unexecuted.")
            require(not dirty_packages(unreal), "Discovery changed package dirty state.")
            report["status"] = "discovery_only_movement_unexecuted"
        else:
            desc_by_path = {str(x.get_editor_property("actor_path")): x for x in descriptors}
            wanted = config["actor_paths"] + [config["witness_actor_path"]]
            require(all(x in desc_by_path for x in wanted), "Explicit actors are missing from this copy's descriptors.")
            unreal.WorldPartitionBlueprintLibrary.load_actors([desc_by_path[x].get_editor_property("guid") for x in wanted])
            actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            loaded = {x.get_path_name(): x for x in actors_api.get_all_level_actors()}
            require(all(x in loaded for x in wanted), "Explicit API actor loading did not complete.")
            actors, witness = [loaded[x] for x in config["actor_paths"]], loaded[config["witness_actor_path"]]
            for actor in actors + [witness]:
                allowed_actor(actor, world, candidate)
                require(str(desc_by_path[actor.get_path_name()].get_editor_property("actor_package")) == actor.get_package().get_name(),
                        "Descriptor and live external package do not match.")
                require(str(desc_by_path[actor.get_path_name()].get_editor_property("guid")) == str(actor.get_editor_property("actor_guid")),
                        "Descriptor and live Actor GUID do not match.")
            require(not dirty_packages(unreal), "Map/actor loading dirtied packages; inspect the fresh copy before movement.")
            if phase == "move":
                report["before_fixture_reference"] = [actor_record(x) for x in actors]
                if config.get("author_owner_reference") is True:
                    require(actors[0].get_owner() is None, "Authored owner witness requires an originally null owner.")
                    actors[0].modify()
                    actors[0].set_owner(witness)
                before = [actor_record(x) for x in actors]
                report["before_movement"], report["witness"] = before, actor_record(witness)
                center = cell_center(config["destination_cell"])
                average = [sum(x["location"][i] for x in before) / len(before) for i in (0, 1)]
                delta = [center[i] - average[i] for i in (0, 1)]
                for actor, saved in zip(actors, before):
                    bounds_origin, extent = actor.get_actor_bounds(False, False)
                    bounds, half = vector(bounds_origin), vector(extent)
                    require(all(-90000 <= bounds[i] + delta[i] - half[i] and bounds[i] + delta[i] + half[i] <= 90000 for i in (0, 1)),
                            "Entire moved group must remain within the authored XY bounds.")
                    require(-100000 <= bounds[2] - half[2] and bounds[2] + half[2] <= 100000, "Actor exceeds Z bounds.")
                for actor, saved in zip(actors, before):
                    location = unreal.Vector(saved["location"][0] + delta[0], saved["location"][1] + delta[1], saved["location"][2])
                    transform = unreal.MathLibrary.make_transform(location, actor.get_actor_rotation(), actor.get_actor_scale3d())
                    require(actors_api.set_actor_transform(actor, transform), "Official editor Actor transform API rejected movement.")
                    require(close(vector(actor.get_actor_location()), vector(location)), "Movement did not reach the exact target.")
                report["after_movement"] = [actor_record(x) for x in actors]
                for saved, moved in zip(before, report["after_movement"]):
                    require(close(saved["rotation"], moved["rotation"]) and close(saved["scale"], moved["scale"]),
                            "Actor API changed rotation or scale.")
                    require(all(saved[k] == moved[k] for k in saved if k not in ("location", "rotation", "scale")),
                            "Actor API changed identity, owner or asset references.")
                    require(abs(saved["location"][2] - moved["location"][2]) <= 0.001, "Actor API changed height.")
                require(actor_record(witness) == report["witness"], "Movement changed the independent witness actor.")
                packages = dirty_packages(unreal)
                expected_dirty = {candidate, candidate + "_BuiltData", *(x.get_package().get_name() for x in actors)}
                require(packages and set(packages).issubset(expected_dirty), "Dirty package scope escaped the exact moved actors/map.")
                report["saved_packages"] = sorted(packages)
                require(unreal.EditorLoadingAndSavingUtils.save_packages(list(packages.values()), True), "Exact-map package save failed.")
                require(not dirty_packages(unreal), "Dirty packages remain after the candidate-only save.")
                report["status"] = "moved_saved_cold_verify_pending"
            else:
                observed = [actor_record(x) for x in actors]
                for expected, value in zip(previous["after_movement"], observed):
                    require(all(close(expected[k], value[k]) for k in ("location", "rotation", "scale")), "Cold transform differs.")
                    require(all(expected[k] == value[k] for k in expected if k not in ("location", "rotation", "scale")), "Cold actor identity/reference differs.")
                require(actor_record(witness) == previous["witness"], "Witness actor changed during movement/save/reload.")
                require(not dirty_packages(unreal), "Cold readback changed package dirty state.")
                report["observed"] = observed
                report["status"] = "underlying_actor_api_cold_readback_passed"
    except Exception as exc:
        report["status"], report["error"] = "failed_partial_preserved", str(exc)
    finally:
        try:
            report["origin_preserved"] = protected_snapshot(ORIGIN) == original["snapshot"]
            report["default_user_saves_preserved"] = local_save_snapshot() == original["local_saves"]
            report["qa_inputs_outside_candidate_preserved"] = protected_snapshot(root, excluded, require_single_link=True) == clone_before
            if not all(report[x] for x in ("origin_preserved", "default_user_saves_preserved", "qa_inputs_outside_candidate_preserved")):
                report["status"] = "failed_preservation_inspect_without_reverting"
        except Exception as exc:
            report["status"], report["preservation_error"] = "failed_preservation_uncertain", str(exc)
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        write_new(output, report)
    require(report["status"] in ("discovery_only_movement_unexecuted", "moved_saved_cold_verify_pending", "underlying_actor_api_cold_readback_passed"),
            "Fixture did not pass; retain the independent clone and evidence: " + str(output))
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config")
    parser.add_argument("--phase", choices=("plan", "discover", "move", "cold_verify"), default="plan")
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--owner-handoff", default="")
    parser.add_argument("--move-exit-receipt")
    parser.add_argument("--move-exit-sha256")
    parser.add_argument("--build-origin-guard")
    args = parser.parse_args()
    if args.build_origin_guard:
        require(not args.execute and args.phase == "plan", "Guard preparation is not an Unreal execution phase.")
        result = build_origin_guard(args.build_origin_guard)
    else:
        require(args.config is not None, "Provide an explicit config file.")
        config = json.loads(Path(args.config).read_text(encoding="utf-8-sig"))
        config["_config_sha256"] = sha(args.config)
        result = plan(config) if args.phase == "plan" or not args.execute else run_phase(
            config, args.phase, args.owner_handoff, args.move_exit_receipt, args.move_exit_sha256)
    print(json.dumps(result, ensure_ascii=False, allow_nan=False))


if __name__ == "__main__":
    main()
