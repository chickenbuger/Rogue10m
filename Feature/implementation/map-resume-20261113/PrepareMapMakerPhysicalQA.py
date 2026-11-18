"""Validate, or explicitly copy, one sealed physical MapMaker QA project.

Default: strict read-only validation, stdout counts only, no destination/receipt.
Only --execute creates the exact fresh Temp owner/Project and exclusive receipt.
Use standalone Python with -I -B. No project helpers, UE, build, GUI or subprocesses.
Partial failures are preserved: no cleanup, overwrite, resume, link or deletion.
"""
import argparse
import ctypes
from ctypes import wintypes
from contextlib import ExitStack
from datetime import datetime, timezone
import hashlib
import json
import ntpath
import os
import re
import shutil
import sys


SOURCE = r"D:\Project\Rogue10m"
TEMP = r"C:\Users\PC\AppData\Local\Temp"
CAPTURE_AREA = TEMP + r"\R10InternalSmoke20261113D"
OWNER = TEMP + r"\R10MMQA20261113A"
PROJECT = OWNER + r"\Project"
RECEIPT = OWNER + r"\physical-clone-receipt.json"
BUILD_ID = "55116800"
SCOPES = ("Content", "Source", "Config", "Build")
EDITOR = ("UnrealEditor-Rogue10m.dll", "UnrealEditor-Rogue10m.pdb",
          "UnrealEditor-Rogue10mEditor.dll", "UnrealEditor-Rogue10mEditor.pdb",
          "UnrealEditor.modules", "Rogue10mEditor.target")
SAVE_ROOTS = (r"C:\Users\PC\AppData\Local\Rogue10m\Saved\SaveGames", SOURCE + r"\Saved\SaveGames")
RESERVE = 2 * 1024 ** 3
CHUNK = 4 * 1024 ** 2
REPARSE = 0x400
DIRECTORY = 0x10
SHA_RE = re.compile(r"[0-9A-Fa-f]{64}\Z")
LIMITS = [
    "Physical snapshot only; no Editor, panel, rendering, physics, input or gameplay test was run.",
    "No World Partition duplication/import/crop/save was run; that receipt belongs to a separate owner step.",
    "Only Content/Source/Config/Build/uproject and exact sealed Editor6 are copied; no Scripts, project Plugins, Saved, Intermediate, caches, .git, Game or HotReload binaries.",
    "Installed matching UE5.8 engine/plugins are external dependencies, not copied or launched.",
    "Regular distinct file identities/single links are verified; ACL/timestamp/storage-level dedup equivalence is not claimed.",
    "A partial destination is intentionally retained and blocks any retry at the same exact path.",
]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def absolute(path):
    require(isinstance(path, str) and path and not path.startswith(("\\\\", "//")), "Local DOS path required")
    require(re.match(r"^[A-Za-z]:[\\/]", path) is not None, "Absolute drive path required")
    require("\x00" not in path, "NUL path rejected")
    return ntpath.normpath(path.replace("/", "\\"))


def same(left, right):
    return absolute(left).casefold() == absolute(right).casefold()


def extended(path):
    return "\\\\?\\" + absolute(path)


def inside(path, root):
    path, root = absolute(path), absolute(root)
    if ntpath.splitdrive(path)[0].casefold() != ntpath.splitdrive(root)[0].casefold():
        return False
    return ntpath.commonpath((path, root)).casefold() == root.casefold()


def relative(value):
    require(isinstance(value, str) and value and "\\" not in value, "Canonical slash-relative path required")
    parts = value.split("/")
    reserved = {"CON", "PRN", "AUX", "NUL"} | {f"{p}{n}" for p in ("COM", "LPT") for n in range(1, 10)}
    for part in parts:
        require(part not in ("", ".", "..") and ":" not in part and "\x00" not in part,
                "Traversal/ADS/empty path component rejected")
        require(part == part.rstrip(" .") and not any(c in part for c in '<>"|?*'), "Windows path alias rejected")
        require(part.split(".", 1)[0].upper() not in reserved, "Reserved Windows name rejected")
    return value


def join(root, rel):
    path = ntpath.join(root, relative(rel).replace("/", "\\"))
    require(inside(path, root) and not same(path, root), "Path escaped its fixed root")
    return path


def pin_record(item, rel=None):
    require(isinstance(item, dict) and isinstance(item.get("size"), int) and not isinstance(item["size"], bool)
            and item["size"] >= 0 and SHA_RE.fullmatch(item.get("sha256", "")), "Malformed captured file pin")
    return {"path": relative(rel if rel is not None else item["path"]),
            "size": item["size"], "sha256": item["sha256"].upper()}


class Info(ctypes.Structure):
    _fields_ = [("attributes", wintypes.DWORD), ("created", wintypes.FILETIME),
                ("accessed", wintypes.FILETIME), ("written", wintypes.FILETIME),
                ("volume", wintypes.DWORD), ("size_hi", wintypes.DWORD),
                ("size_lo", wintypes.DWORD), ("links", wintypes.DWORD),
                ("index_hi", wintypes.DWORD), ("index_lo", wintypes.DWORD)]


def ticks(value):
    return (value.dwHighDateTime << 32) | value.dwLowDateTime


class Physical:
    """Windows handles reject reparse leaves, path redirection and extra hardlinks."""
    def __init__(self):
        import msvcrt
        self.msvcrt = msvcrt
        self.api = ctypes.WinDLL("kernel32", use_last_error=True)
        self.api.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                        wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        self.api.CreateFileW.restype = wintypes.HANDLE
        self.api.CloseHandle.argtypes = [wintypes.HANDLE]
        self.api.CloseHandle.restype = wintypes.BOOL
        self.api.GetFileInformationByHandle.argtypes = [wintypes.HANDLE, ctypes.POINTER(Info)]
        self.api.GetFileInformationByHandle.restype = wintypes.BOOL
        self.api.GetFinalPathNameByHandleW.argtypes = [wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD]
        self.api.GetFinalPathNameByHandleW.restype = wintypes.DWORD
        self.stack = ExitStack()
        self.directories = {}

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return self.stack.__exit__(*args)

    def metadata(self, handle, path, directory=False):
        info = Info()
        if not self.api.GetFileInformationByHandle(handle, ctypes.byref(info)):
            raise ctypes.WinError(ctypes.get_last_error())
        require(not info.attributes & REPARSE, "Reparse point rejected: " + path)
        require(bool(info.attributes & DIRECTORY) == directory, "Unexpected object type: " + path)
        if not directory:
            require(info.links == 1, "Extra hardlinks rejected: " + path)
        buffer = ctypes.create_unicode_buffer(32768)
        length = self.api.GetFinalPathNameByHandleW(handle, buffer, len(buffer), 0)
        require(0 < length < len(buffer), "Could not prove final physical path: " + path)
        actual = buffer.value
        require(actual.startswith("\\\\?\\") and not actual.startswith("\\\\?\\UNC\\"), "Non-DOS physical path rejected")
        require(same(actual[4:], path), "Resolved physical path differs: " + path)
        return {"size": (info.size_hi << 32) | info.size_lo, "attributes": info.attributes,
                "created_100ns": ticks(info.created), "written_100ns": ticks(info.written),
                "file_id": f"{info.volume:08X}:{info.index_hi:08X}{info.index_lo:08X}", "links": info.links}

    def handle(self, path, directory=False, create=False):
        # OPEN_REPARSE_POINT never follows a reparse leaf. Held directory handles
        # deny write/delete opens, preventing parent replacement while traversing.
        access = 0x80 if directory else (0xC0000000 if create else 0x80000000)
        flags = 0x00200000 | (0x02000000 if directory else 0x08000000)
        handle = self.api.CreateFileW(extended(path), access, 1 if not create else 0,
                                      None, 1 if create else 3, flags, None)
        if handle == ctypes.c_void_p(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            info = self.metadata(handle, path, directory)
        except BaseException:
            self.api.CloseHandle(handle)
            raise
        return handle, info

    def hold_chain(self, path):
        path = absolute(path)
        drive, tail = ntpath.splitdrive(path)
        current = drive + "\\"
        components = [current]
        for part in tail.strip("\\").split("\\"):
            if part:
                current = ntpath.join(current, part)
                components.append(current)
        for item in components:
            key = item.casefold()
            if key in self.directories:
                continue
            require(os.path.lexists(extended(item)), "Required physical directory missing: " + item)
            handle, info = self.handle(item, directory=True)
            self.directories[key] = (handle, info)
            self.stack.callback(self.api.CloseHandle, handle)

    def file(self, path, create=False):
        if create:
            require(inside(path, OWNER) and not same(path, OWNER), "Exclusive writes are restricted to the exact QA owner area")
        self.hold_chain(ntpath.dirname(path))
        handle, info = self.handle(path, create=create)
        try:
            fd = self.msvcrt.open_osfhandle(handle, (os.O_RDWR if create else os.O_RDONLY) | os.O_BINARY)
        except BaseException:
            self.api.CloseHandle(handle)
            raise
        return os.fdopen(fd, "w+b" if create else "rb", buffering=0), info

    def current_info(self, stream, path):
        return self.metadata(self.msvcrt.get_osfhandle(stream.fileno()), path)

    def read(self, path, expected=None, keep=False):
        stream, before = self.file(path)
        with stream:
            digest = hashlib.sha256()
            chunks = [] if keep else None
            while True:
                data = stream.read(CHUNK)
                if not data:
                    break
                digest.update(data)
                if keep:
                    chunks.append(data)
            require(self.current_info(stream, path) == before, "Source stat changed while reading: " + path)
        actual = {"path": path, "size": before["size"], "sha256": digest.hexdigest().upper()}
        if expected:
            require(actual["size"] == expected["size"] and actual["sha256"] == expected["sha256"].upper(),
                    "Captured bytes differ: " + path)
        return actual, before, b"".join(chunks) if keep else None

    def scan(self, root):
        files, directories = {}, []
        if not os.path.lexists(extended(root)):
            ancestor = ntpath.dirname(root)
            while not os.path.lexists(extended(ancestor)):
                parent = ntpath.dirname(ancestor)
                require(parent != ancestor, "No existing physical ancestor")
                ancestor = parent
            self.hold_chain(ancestor)
            require(not os.path.lexists(extended(root)), "Absent protected root appeared while checking")
            return {"exists": False, "directories": [], "files": files}
        self.hold_chain(root)
        pending = [(root, "")]
        while pending:
            current, prefix = pending.pop()
            with os.scandir(extended(current)) as entries:
                children = sorted(entries, key=lambda entry: entry.name.casefold())
            for entry in children:
                rel = relative(prefix + entry.name)
                path = join(root, rel)
                stat = entry.stat(follow_symlinks=False)
                require(not stat.st_file_attributes & REPARSE, "Reparse entry rejected: " + path)
                if entry.is_dir(follow_symlinks=False):
                    self.hold_chain(path)
                    directories.append(rel)
                    pending.append((path, rel + "/"))
                else:
                    # Windows DirEntry.stat can report st_nlink=0 from cached
                    # enumeration data. Every file's real link count is checked
                    # by GetFileInformationByHandle during the mandatory hash pass.
                    require(entry.is_file(follow_symlinks=False), "Nonregular file rejected: " + path)
                    key = rel.casefold()
                    require(key not in files, "Case-alias duplicate rejected")
                    files[key] = rel
        return {"exists": True, "directories": sorted(directories), "files": files}

    def mkdir(self, path):
        require(inside(path, OWNER) and not same(path, OWNER), "Directory creation is restricted to the exact QA project area")
        self.hold_chain(ntpath.dirname(path))
        os.mkdir(extended(path))  # Exclusive: an existing path is always an error.
        self.hold_chain(path)

    def write_json(self, path, value):
        data = (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
        stream, _ = self.file(path, create=True)
        with stream:
            require(stream.write(data) == len(data), "Short receipt write")
            stream.flush()
            os.fsync(stream.fileno())
            stream.seek(0)
            require(stream.read() == data, "Receipt readback differs")
        return {"path": path, "size": len(data), "sha256": hashlib.sha256(data).hexdigest().upper()}


def load_json(io, path, expected_sha, expected_size=None):
    expected = {"sha256": expected_sha, "size": expected_size} if expected_size is not None else None
    pin, stat, data = io.read(path, expected, keep=True)
    require(pin["sha256"] == expected_sha.upper(), "JSON pin differs: " + path)
    return json.loads(data.decode("utf-8-sig")), pin, stat


def source_inventory(io):
    files, directories = {}, []
    for scope in SCOPES:
        tree = io.scan(join(SOURCE, scope))
        require(tree["exists"], "Required source scope missing: " + scope)
        directories.extend([scope] + [scope + "/" + item for item in tree["directories"]])
        for item in tree["files"].values():
            rel = scope + "/" + item
            files[rel.casefold()] = rel
    require(not os.path.lexists(extended(SOURCE + r"\Plugins")), "Project Plugins must remain absent")
    files["rogue10m.uproject"] = "Rogue10m.uproject"
    return files, sorted(directories)


def verify_files(io, pins, prior=None, root=SOURCE):
    actual = {}
    for rel, pin in sorted(pins.items(), key=lambda pair: pair[0].casefold()):
        _, stat, _ = io.read(join(root, rel), pin)
        if prior is not None:
            require(stat == prior[rel], "File identity/stat changed since initial validation: " + rel)
        actual[rel] = stat
    return actual


def verify_saves(io, protected, capture):
    summary = []
    for index, entry in enumerate(protected):
        manifest = entry["manifest"]
        expected_path = capture + f"\\protected-save-{index}.json"
        require(same(entry["root"], SAVE_ROOTS[index]) and same(manifest["path"], expected_path), "Save seal root/path differs")
        expected, manifest_pin, _ = load_json(io, expected_path, manifest["sha256"], manifest["size"])
        tree = io.scan(SAVE_ROOTS[index])
        require(tree["exists"] is expected["exists"] and tree["directories"] == expected["directories"], "Protected SaveGames presence/directories changed")
        save_pins = {relative(item["path"]): pin_record(item) for item in expected["files"]}
        require(set(tree["files"]) == {rel.casefold() for rel in save_pins}, "Protected SaveGames file set changed")
        physical_states = []
        for rel, pin in save_pins.items():
            _, stat, _ = io.read(join(SAVE_ROOTS[index], rel), pin)
            original = next(item for item in expected["files"] if item["path"] == rel)
            require(stat["attributes"] == original["attributes"], "Protected SaveGames attributes changed")
            physical_states.append({"path": rel, **stat})
        if tree["exists"]:
            for rel in [""] + tree["directories"]:
                directory = join(SAVE_ROOTS[index], rel) if rel else SAVE_ROOTS[index]
                handle, _ = io.directories[absolute(directory).casefold()]
                physical_states.append({"directory": rel, **io.metadata(handle, directory, directory=True)})
        summary.append({"root": SAVE_ROOTS[index], "manifest_pin": manifest_pin,
                        "exists": tree["exists"], "file_count": len(save_pins), "verified": True,
                        "physical_state_digest": hashlib.sha256(json.dumps(physical_states, sort_keys=True).encode()).hexdigest().upper()})
    return summary


def free_disk(required):
    free = shutil.disk_usage(extended(TEMP)).free
    require(free >= required + RESERVE, "Insufficient free Temp disk space including 2 GiB reserve")
    return free


def fresh_destination(io):
    io.hold_chain(TEMP)
    for protected in (SOURCE, *SAVE_ROOTS, CAPTURE_AREA):
        require(not inside(OWNER, protected) and not inside(protected, OWNER), "Destination overlaps protected input")
    require(same(PROJECT, TEMP + r"\R10MMQA20261113A\Project") and not os.path.lexists(extended(OWNER)),
            "Exact QA owner root must be wholly fresh; no retry/overwrite/cleanup")


def copy_one(io, rel, pin, original_stat):
    src, dst = join(SOURCE, rel), join(PROJECT, rel)
    reader, before = io.file(src)
    with reader:
        require(before == original_stat, "Source stat changed before copy: " + rel)
        writer, _ = io.file(dst, create=True)
        with writer:
            digest = hashlib.sha256()
            while True:
                data = reader.read(CHUNK)
                if not data:
                    break
                digest.update(data)
                require(writer.write(data) == len(data), "Short destination write: " + rel)
            require(io.current_info(reader, src) == before, "Source stat changed during copy: " + rel)
            require(digest.hexdigest().upper() == pin["sha256"], "Source hash changed during copy: " + rel)
            writer.flush()
            os.fsync(writer.fileno())
            writer.seek(0)
            target_hash = hashlib.sha256()
            while True:
                data = writer.read(CHUNK)
                if not data:
                    break
                target_hash.update(data)
            after = io.current_info(writer, dst)
            require(after["file_id"] != before["file_id"] and after["size"] == pin["size"]
                    and target_hash.hexdigest().upper() == pin["sha256"], "Physical destination verification failed: " + rel)
    # Windows finalizes last-write metadata on close. Capture the closed-file stat,
    # and require the same newly-created identity before later whole-tree checks.
    check_stream, closed_stat = io.file(dst)
    with check_stream:
        require(closed_stat["file_id"] == after["file_id"] and closed_stat["size"] == after["size"], "Destination identity changed after close")
    return {"relative_path": rel, "size": pin["size"], "sha256": pin["sha256"],
            "source_pin": {"path": src, "sha256": pin["sha256"], **before},
            "target_pin": {"path": dst, "sha256": pin["sha256"], **closed_stat}}


def prepare(capture, seal_path, seal_sha256, execute=False):
    require(os.name == "nt" and sys.version_info >= (3, 11), "Windows standalone Python 3.11+ required")
    require(sys.flags.isolated and sys.dont_write_bytecode, "Invoke Python with -I -B")
    capture, seal_path = absolute(capture), absolute(seal_path)
    require(same(ntpath.dirname(capture), CAPTURE_AREA) and re.fullmatch(r"capture[0-9]{2,3}", ntpath.basename(capture)),
            "Capture must be a direct captureNN directory in the exact capture owner area")
    require(same(ntpath.dirname(seal_path), CAPTURE_AREA) and re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,90}\.sealed\.json", ntpath.basename(seal_path)),
            "Sealed JSON must be a direct sealed file in the exact capture owner area")
    require(SHA_RE.fullmatch(seal_sha256), "Explicit trusted seal SHA256 is required")
    inputs_path = capture + r"\inputs.json"
    owned = False
    result = {"schema": "R10-mapmaker-physical-snapshot-v1", "physical_snapshot_verified": False,
              "project_root": PROJECT, "source_root": SOURCE, "execute_requested": execute,
              "engine_or_gui_executed": False, "known_limits": LIMITS}
    with Physical() as io:
        try:
            fresh_destination(io)
            seal, seal_pin, seal_stat = load_json(io, seal_path, seal_sha256)
            require(seal.get("schema") == "R10-internal-smoke-seal-v1" and seal.get("status") == "sealed"
                    and same(seal["root"], SOURCE) and same(seal["project"], SOURCE + r"\Rogue10m.uproject")
                    and same(seal["owner_area"], CAPTURE_AREA) and same(seal["engine"], r"D:\Program Files\UE_5.8")
                    and seal.get("build_root_policy") == "integration_owner_exact_root"
                    and seal.get("engine_build_id") == BUILD_ID, "Exact sealed source identity required")
            require(same(seal["input_manifest"]["path"], inputs_path), "Seal names a different capture input manifest")
            manifest, input_pin, input_stat = load_json(io, inputs_path, seal["input_manifest"]["sha256"], seal["input_manifest"]["size"])
            result["capture_pin"] = {"seal": seal_pin, "inputs": input_pin}
            require(manifest.get("schema") == "R10-candidate-inputs-v1" and same(manifest["root"], SOURCE)
                    and manifest["present_roots"].get("Plugins") is False
                    and all(manifest["present_roots"].get(scope) is True for scope in SCOPES), "Capture schema/root/plugins differ")
            captured, captured_names = {}, set()
            for entry in manifest["files"]:
                rel = relative(entry["path"])
                if rel.split("/", 1)[0] in SCOPES or rel == "Rogue10m.uproject":
                    require(rel.casefold() not in captured_names, "Duplicate captured file")
                    captured_names.add(rel.casefold())
                    captured[rel] = pin_record(entry)
            counts = {scope: sum(rel == scope or rel.startswith(scope + "/") for rel in captured) for scope in (*SCOPES, "Rogue10m.uproject")}
            require(all(counts[scope] > 0 for scope in SCOPES) and counts["Rogue10m.uproject"] == 1,
                    "Capture must contain all exact source scopes and one uproject")
            wanted_dirs = sorted(relative(item) for item in manifest["directories"] if item.split("/", 1)[0] in SCOPES)
            current, current_dirs = source_inventory(io)
            require(set(current) == {rel.casefold() for rel in captured} and current_dirs == wanted_dirs, "Selected source file/directory set differs from capture")
            editor_pins = seal["editor_products"]
            require(len(editor_pins) == 6 and {absolute(item["path"]).casefold() for item in editor_pins}
                    == {join(SOURCE, "Binaries/Win64/" + name).casefold() for name in EDITOR}, "Canonical Editor6 set differs")
            for entry in editor_pins:
                name = ntpath.basename(entry["path"])
                rel = "Binaries/Win64/" + name
                captured[rel] = pin_record(entry, rel)
            protected = seal["protected_saves"]
            require(len(protected) == 2, "Both exact protected SaveGames seals are required")
            required_bytes = sum(pin["size"] for pin in captured.values())
            initial_free = free_disk(required_bytes)
            saves_before = verify_saves(io, protected, capture)
            result["protected_save_seals_before"] = saves_before
            print("Validating captured source bytes (read-only); no clone has been created.", file=sys.stderr, flush=True)
            source_before = verify_files(io, captured)
            modules, _, _ = load_json(io, SOURCE + r"\Binaries\Win64\UnrealEditor.modules",
                                      captured["Binaries/Win64/UnrealEditor.modules"]["sha256"])
            target, _, _ = load_json(io, SOURCE + r"\Binaries\Win64\Rogue10mEditor.target",
                                     captured["Binaries/Win64/Rogue10mEditor.target"]["sha256"])
            require(modules.get("BuildId") == BUILD_ID and modules.get("Modules") == {"Rogue10m": EDITOR[0], "Rogue10mEditor": EDITOR[2]}
                    and target.get("TargetName") == "Rogue10mEditor" and target.get("TargetType") == "Editor"
                    and target.get("Platform") == "Win64" and target.get("Configuration") == "Development"
                    and target.get("Version", {}).get("BuildId") == BUILD_ID, "Editor modules/target BuildId identity differs")
            self_pin, _, _ = io.read(absolute(os.path.abspath(__file__)))
            result.update(capture_pin={"seal": seal_pin, "inputs": input_pin}, copier_pin=self_pin,
                          selected_counts=counts, selected_file_count=len(captured), selected_bytes=required_bytes,
                          editor6=editor_pins, buildId=BUILD_ID, initial_free_bytes=initial_free,
                          reserve_bytes=RESERVE, userdir_candidate=OWNER + r"\User",
                          protected_save_seals_before=saves_before, source_inputs_verified=True)
            selected = []
            if execute:
                fresh_destination(io)
                free_disk(required_bytes)
                # Mark ownership immediately after exclusive root creation,
                # before any later directory-handle validation can fail.
                os.mkdir(extended(OWNER))
                owned = True
                io.hold_chain(OWNER)
                io.mkdir(PROJECT)
                for rel in sorted(set(wanted_dirs + ["Binaries", "Binaries/Win64"]), key=lambda item: (item.count("/"), item.casefold())):
                    io.mkdir(join(PROJECT, rel))
                remaining = required_bytes
                for index, (rel, pin) in enumerate(sorted(captured.items(), key=lambda pair: pair[0].casefold())):
                    if index % 128 == 0:
                        free_disk(remaining)
                    selected.append(copy_one(io, rel, pin, source_before[rel]))
                    remaining -= pin["size"]
                expected_targets = {row["relative_path"]: {key: value for key, value in row["target_pin"].items() if key not in ("path", "sha256")} for row in selected}
                verify_files(io, captured, expected_targets, PROJECT)
                target_tree = io.scan(PROJECT)
                require(set(target_tree["files"]) == {rel.casefold() for rel in captured}
                        and target_tree["directories"] == sorted(set(wanted_dirs + ["Binaries", "Binaries/Win64"])), "Clone contains missing or unexpected entries")
            # Re-hash all selected originals and both save roots after validation/copy.
            verify_files(io, captured, source_before)
            end_files, end_dirs = source_inventory(io)
            require(set(end_files) == set(current) and end_dirs == current_dirs, "Source scope set changed after operation")
            saves_after = verify_saves(io, protected, capture)
            require(saves_after == saves_before, "Protected SaveGames seals changed")
            _, seal_after, _ = io.read(seal_path, seal_pin)
            _, input_after, _ = io.read(inputs_path, input_pin)
            require(seal_after == seal_stat and input_after == input_stat, "Captured input manifest identity/stat changed")
            io.read(self_pin["path"], self_pin)
            result.update(status="physical_snapshot_verified" if execute else "read_only_plan_inputs_verified",
                          physical_snapshot_verified=execute, source_before_after_unchanged=True,
                          capture_before_after_unchanged=True, protected_save_seals_after=saves_after,
                          final_free_bytes=free_disk(0), created_utc=datetime.now(timezone.utc).isoformat())
            if execute:
                result["selected_source_and_target_file_pins"] = selected
                receipt_pin = io.write_json(RECEIPT, result)
                return {key: value for key, value in result.items() if key != "selected_source_and_target_file_pins"} | {"receipt": receipt_pin}
            return result
        except BaseException as error:
            result.update(status="partial_failure_preserved" if owned else "blocked_before_copy",
                          physical_snapshot_verified=False, error=str(error), copied_file_count=len(locals().get("selected", [])),
                          cleanup_performed=False, overwrite_performed=False)
            if "saves_before" in locals():
                try:
                    result["protected_save_seals_after"] = verify_saves(io, protected, capture)
                    result["protected_saves_before_after_unchanged"] = result["protected_save_seals_after"] == saves_before
                except BaseException as save_error:
                    result["protected_saves_recheck_error"] = str(save_error)
            if "input_pin" in locals():
                try:
                    io.read(seal_path, seal_pin)
                    io.read(inputs_path, input_pin)
                    result["capture_before_after_unchanged"] = True
                except BaseException as pin_error:
                    result["capture_recheck_error"] = str(pin_error)
            if owned:
                try:
                    result["selected_source_and_target_file_pins"] = locals().get("selected", [])
                    result["receipt"] = io.write_json(RECEIPT, result)
                except BaseException as receipt_error:
                    result["receipt_error"] = str(receipt_error)
            print(json.dumps(result, ensure_ascii=False, indent=2), file=sys.stderr)
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", required=True, help="Exact owner captureNN directory")
    parser.add_argument("--seal", required=True, help="Exact owner-area sealed JSON")
    parser.add_argument("--seal-sha256", required=True, help="Trusted owner-provided SHA256 of that sealed JSON")
    parser.add_argument("--execute", action="store_true", help="Root owner only: create the exact new physical Temp clone and receipt")
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args.capture, args.seal, args.seal_sha256, args.execute), ensure_ascii=False, indent=2))
        return 0
    except (ValueError, OSError, KeyError, TypeError, UnicodeError) as error:
        print("Physical QA preparation blocked: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
