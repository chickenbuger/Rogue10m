"""Read-only candidate preflight/postflight. Never starts an engine, shell or build.

The separate owner dispatcher is the only execution boundary. Seal and release
hashes come from the coordinator's handoff, not from their own JSON contents.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import types
from datetime import datetime, timezone

SCOPES = ("Source", "Config", "Content", "Build", "Scripts", "Plugins")
PRODUCTS = ("UnrealEditor-Rogue10m.dll", "UnrealEditor-Rogue10m.pdb",
            "UnrealEditor-Rogue10mEditor.dll", "UnrealEditor-Rogue10mEditor.pdb",
            "UnrealEditor.modules", "Rogue10mEditor.target")
OUTPUTS = ("evidence", "archive", "cook", "stage", "cook_user", "qa_user")
ORIGINAL = r"D:\Project\Rogue10m"
RED = re.compile(r"(?i)\bLog[^\s:]*:\s*(?:Error|Fatal)\s*:|Fatal error|Assertion failed|"
                 r"\b(?:Handled\s+)?ensure\s+(?:condition|failed|failure)|Traceback \(most recent call last\)")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def canonical(obj):
    return json.dumps(obj, sort_keys=True, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def sha(value):
    require(isinstance(value, str) and re.fullmatch(r"[A-F0-9]{64}", value), "Missing/invalid uppercase SHA256")
    return value


def physical(value, exists=False):
    require(isinstance(value, (str, Path)) and str(value), "Missing path")
    p = Path(value)
    require(p.is_absolute(), f"Absolute path required: {p}")
    require(not any(c in str(p) for c in '\r\n\x00\"%&|<>^!`$;'), f"Unsupported shell/control character: {p}")
    require(".." not in p.parts and "." not in p.parts, f"Noncanonical path: {p}")
    require(str(p) == os.path.abspath(p), f"Canonical absolute path required: {p}")
    for q in [p, *p.parents]:
        try:
            st = q.lstat()
        except FileNotFoundError:
            continue
        require(not stat.S_ISLNK(st.st_mode) and not (getattr(st, "st_file_attributes", 0) & 1024),
                f"Reparse/symlink refused: {q}")
    require(not exists or p.exists(), f"Missing path: {p}")
    return p


def child(p, parent):
    p, parent = physical(p), physical(parent)
    try:
        return p != parent and p.is_relative_to(parent)
    except ValueError:
        return False


def stable_bytes(path):
    p = physical(path, True)
    require(p.is_file(), f"Expected regular file: {p}")
    with p.open("rb") as stream:
        before = os.fstat(stream.fileno())
        require(before.st_nlink == 1, f"Hard-linked input refused: {p}")
        data = stream.read()
        after = os.fstat(stream.fileno())
    current = p.stat()
    identity = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
    require(identity(before) == identity(after) == identity(current) and len(data) == before.st_size,
            f"File changed during read: {p}")
    return data


def file_pin(path):
    p = physical(path, True)
    data = stable_bytes(p)
    return {"path": str(p), "sha256": digest(data), "size": len(data)}


def read_pin(pin):
    require(isinstance(pin, dict) and "path" in pin, "Missing file pin")
    data = stable_bytes(pin["path"])
    require(digest(data) == sha(pin.get("sha256")), f"SHA mismatch: {pin['path']}")
    require("size" not in pin or type(pin["size"]) is int and pin["size"] == len(data), "Pin size mismatch")
    return data


def load(data):
    def unique(pairs):
        result = {}
        for k, v in pairs:
            require(k not in result, f"Duplicate JSON key: {k}")
            result[k] = v
        return result
    return json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique)


def read_json_pin(pin):
    return load(read_pin(pin))


def relative(value):
    require(isinstance(value, str) and "\\" not in value, "Relative manifest paths use forward slashes")
    p = PurePosixPath(value)
    require(value and not p.is_absolute() and str(p) == value and all(x not in (".", "..") for x in p.parts),
            f"Unsafe relative path: {value}")
    require(not any(c in value for c in ':\r\n\x00'), "Unsafe relative path characters")
    return value


def unique_paths(records):
    result = {}
    for record in records:
        key = relative(record["path"])
        require(key.casefold() not in result, f"Duplicate/case-colliding path: {key}")
        result[key.casefold()] = record
    return result


def scan_tree(root, include_attributes=False):
    root = physical(root)
    if not root.exists():
        return {"exists": False, "directories": [], "files": []}
    require(root.is_dir(), f"Expected directory: {root}")
    dirs, files = [], []
    for base, names, leaves in os.walk(root, followlinks=False):
        names.sort(); leaves.sort()
        for name in names:
            p = physical(Path(base) / name, True)
            dirs.append(p.relative_to(root).as_posix())
        for name in leaves:
            p = physical(Path(base) / name, True)
            pin = file_pin(p)
            pin["path"] = p.relative_to(root).as_posix()
            if include_attributes:
                pin["attributes"] = getattr(p.stat(), "st_file_attributes", 0)
            files.append(pin)
    unique_paths(files)
    require(len({x.casefold() for x in dirs}) == len(dirs), "Case-colliding directories")
    return {"exists": True, "directories": sorted(dirs), "files": sorted(files, key=lambda x: x["path"])}


def input_inventory(root):
    root = physical(root, True)
    states, dirs, files = {}, [], []
    for scope in SCOPES:
        tree = scan_tree(root / scope)
        states[scope] = tree["exists"]
        if tree["exists"]:
            dirs.append(scope)
            dirs.extend(scope + "/" + d for d in tree["directories"])
            files.extend({**f, "path": scope + "/" + f["path"]} for f in tree["files"])
    projects = sorted(root.glob("*.uproject"))
    require(len(projects) == 1 and projects[0].name == "Rogue10m.uproject", "Exactly one Rogue10m.uproject required")
    f = file_pin(projects[0]); f["path"] = projects[0].name; files.append(f)
    require(all(states[s] for s in ("Source", "Config", "Content")), "Source/Config/Content required")
    return {"schema": "R10-candidate-inputs-v1", "root": str(root), "scopes": list(SCOPES),
            "present_roots": states, "directories": sorted(dirs), "files": sorted(files, key=lambda x: x["path"])}


def compare_inventory(expected, actual, label):
    # Exact equality includes absent optional roots, empty directories, byte size and raw SHA.
    unique_paths(expected.get("files", []))
    require(expected == actual, f"Exact {label} manifest changed (added, missing, path, attributes, size or SHA)")


def utc(value):
    require(isinstance(value, str), "Missing UTC timestamp")
    result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    require(result.tzinfo is not None and result.utcoffset().total_seconds() == 0, "Explicit UTC required")
    return result


def exact_cooker_command(command, project, user_dir):
    projects = re.findall(r'(?:^|\s)(?:-project=)?(?:"([^"\r\n]+\.uproject)"|([^\s"\r\n]+\.uproject))(?=\s|$)', command, re.I)
    require(len(projects) == 1 and (projects[0][0] or projects[0][1]) == project, "Actual cooker exact unique project mismatch")
    users = re.findall(r'(?:^|\s)-UserDir=(?:"([^"\r\n]+)"|([^\s"\r\n]+))(?=\s|$)', command, re.I)
    require(len(users) == 1 and (users[0][0] or users[0][1]) == user_dir, "Actual cooker exact unique UserDir mismatch")
    for flag in ("-DisablePython", "-NoLiveCoding"):
        require(len(re.findall(r'(?:^|\s)' + re.escape(flag) + r'(?=\s|$)', command, re.I)) == 1,
                "Actual cooker isolation flag missing/ambiguous: " + flag)
    ini_prefix = "-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer="
    ini = re.findall(r'(?:^|\s)' + re.escape(ini_prefix) + r'([^\s"]+)(?=\s|$)', command, re.I)
    require(ini == ["False"], "Actual cooker exact MCP override mismatch")


def validate_maps(s, manifest):
    maps = s["cook_maps"]
    require(isinstance(maps, list) and maps and len(set(x.casefold() for x in maps)) == len(maps), "Empty/duplicate cook maps")
    require(s["active_destination"] in maps, "Final active destination must explicitly be cooked")
    require(s["map_selection_final"] is True, "Map selection is not final")
    require(all(re.fullmatch(r"/Game/[A-Za-z0-9_/]+", x) and "//" not in x for x in maps), "Invalid map package")
    selected = read_json_pin(s["map_manifest"])
    require(selected.get("schema") == "R10-candidate-maps-v1" and selected.get("root") == s["root"], "Wrong map manifest identity")
    entries = selected.get("maps", [])
    require([e["package"] for e in entries] == maps, "Map manifest order/identity mismatch")
    all_files = {f["path"]: f for f in manifest["files"]}
    for e in entries:
        suffix = e["package"][6:]
        map_file = "Content/" + suffix + ".umap"
        external = ["Content/__ExternalActors__/" + suffix, "Content/__ExternalObjects__/" + suffix]
        require(e.get("external_roots") == external, "Both derived external-package roots must be explicit")
        want = [f for name, f in all_files.items() if name == map_file or any(name.startswith(x + "/") for x in external)]
        want.sort(key=lambda x: x["path"])
        require(map_file in all_files and e.get("files") == want, "Exact map/external-package manifest mismatch")
        require(e.get("external_roots_present") == {x: x in manifest["directories"] for x in external}, "External-root presence changed")
    required = s["required_iostore_packages"]
    require(isinstance(required, list) and required and set(maps) <= set(required), "IoStore required packages omit a cook map")
    require(len({x.casefold() for x in required}) == len(required), "Duplicate IoStore requirements")
    require(all(re.fullmatch(r"/Game/[A-Za-z0-9_/]+", x) for x in required), "Invalid IoStore package")
    return selected


def validate_editor(s, manifest_sha):
    root = Path(s["root"])
    products = s["editor_products"]
    require(isinstance(products, list) and len(products) == 6, "Exactly six Editor products required")
    expected = [str(root / "Binaries" / "Win64" / p) for p in PRODUCTS]
    require(sorted(x["path"] for x in products) == sorted(expected), "Editor product names/paths/duplicates differ")
    for p in products:
        read_pin(p)
    receipt = read_json_pin(s["editor_build_receipt"])
    require(receipt.get("schema") == "R10-candidate-editor-build-v1" and receipt.get("project") == s["project"]
            and receipt.get("input_manifest_sha256") == manifest_sha and receipt.get("exit") == 0
            and receipt.get("target") == "Rogue10mEditor" and receipt.get("platform") == "Win64"
            and receipt.get("configuration") == "Development" and receipt.get("engine_build_id") == s["engine_build_id"]
            and receipt.get("products") == products and receipt.get("inputs_unchanged") is True,
            "Canonical Editor build receipt is wrong, stale, incomplete or not bound to final inputs")
    require(utc(receipt["started_utc"]) < utc(receipt["finished_utc"]), "Invalid Editor build time interval")
    log = read_pin(receipt["build_log"]).decode("utf-8-sig", errors="strict")
    require(s["project"] in log and "Rogue10mEditor" in log and "Result: Succeeded" in log,
            "Actual Editor build log identity/result missing")
    require(not RED.search(log), "Editor build log has error/ensure")
    native = load(stable_bytes(root / "Binaries" / "Win64" / "Rogue10mEditor.target"))
    require(native.get("TargetName") == "Rogue10mEditor" and native.get("TargetType") == "Editor"
            and native.get("Platform") == "Win64" and native.get("Configuration") == "Development"
            and native.get("Version", {}).get("BuildId") == s["engine_build_id"], "Native Editor .target identity differs")
    native_project = os.path.abspath(root / "Binaries" / "Win64" / native.get("Project", ""))
    require(native_project == s["project"], "Native Editor receipt resolves to a different project")
    modules = load(stable_bytes(root / "Binaries" / "Win64" / "UnrealEditor.modules"))
    engine_modules = load(stable_bytes(Path(s["engine"]) / "Engine" / "Binaries" / "Win64" / "UnrealEditor.modules"))
    require(modules.get("BuildId") == engine_modules.get("BuildId") == s["engine_build_id"], "Engine/project Editor modules BuildId mismatch")
    require(modules.get("Modules") == {"Rogue10m": PRODUCTS[0], "Rogue10mEditor": PRODUCTS[2]}, "Exact Editor module DLL mapping mismatch")
    return receipt


def validate_release(s, seal_sha, release_pin, owner_id):
    r = read_json_pin(release_pin)
    require(r.get("schema") == "R10-candidate-execution-release-v1" and r.get("status") == "released_for_candidate_packaging",
            "No reviewed candidate execution release")
    for key, value in {"seal_sha256": seal_sha, "project": s["project"], "owner_id": owner_id,
                       "run_tag": s["run_tag"], "active_destination": s["active_destination"]}.items():
        require(r.get(key) == value and value, f"Release {key} mismatch")
    require(r.get("scopes") == ["Win64 Development packaging"], "Wrong release scope")
    now = datetime.now(timezone.utc)
    require(utc(r["released_utc"]) <= now < utc(r["expires_utc"]), "Execution release not active or expired")
    holder = r.get("owner_process", {})
    require(type(holder.get("pid")) is int and holder["pid"] > 0 and type(holder.get("start_utc_ticks")) is int
            and holder["start_utc_ticks"] > 0, "Missing exact owner process identity")
    require(holder.get("executable") == s["runtime"]["powershell"]["path"], "Owner process executable mismatch")
    require(holder.get("sha256") == s["runtime"]["powershell"]["sha256"], "Owner process binary hash mismatch")
    return r


def validate_integration_review(s, manifest_sha, dll_sha):
    # This is a coordinator's scoped attestation, not fabricated raw UE proof.
    review = read_json_pin(s["integration_review"])
    require(review.get("schema") == "R10-candidate-integration-review-v1" and review.get("status") == "reviewed_for_packaging"
            and review.get("project") == s["project"] and review.get("input_manifest_sha256") == manifest_sha
            and review.get("editor_dll_sha256") == dll_sha and review.get("active_destination") == s["active_destination"],
            "Integration owner has not reviewed this exact candidate/input/DLL/map")
    required_scopes = ["editor_and_loot_regressions", "compact_map_reload_and_basic_play", "explicit_cook_and_external_packages"]
    require(review.get("reviewed_scopes") == required_scopes and isinstance(review.get("reviewer"), str) and review["reviewer"],
            "Integration review scopes/reviewer incomplete")
    evidence = review.get("evidence", [])
    require(evidence and all(isinstance(x, dict) for x in evidence), "Integration review raw evidence pins missing")
    for p in evidence:
        read_pin(p)
    return review


def immutable(seal_path, seal_sha, release_path, release_sha, owner_id, fresh=True):
    s = read_json_pin({"path": seal_path, "sha256": seal_sha})
    require(s.get("schema") == "R10-candidate-windows-seal-v1", "Wrong seal schema")
    require(s.get("status") == "sealed", "UNSEALED: final candidate/input/Editor/QA/map identities have not been reviewed and sealed")
    root, owner, engine = physical(s["root"], True), physical(s["owner_area"], True), physical(s["engine"], True)
    require(not re.search(r"\s", str(root)), "Candidate path whitespace is unsupported until the BAT argv boundary is proven")
    original = physical(ORIGINAL)
    require(root != original and not child(root, original) and not child(original, root) and child(root, owner),
            "Require a physical isolated candidate under owner_area, outside original")
    require(not child(owner, root) and not child(owner, engine) and not child(root, engine)
            and not child(engine, root) and root != engine, "Owner/candidate/engine overlap")
    project = root / "Rogue10m.uproject"
    require(s["project"] == str(project) and s.get("target") == "Rogue10m" and s.get("platform") == "Win64"
            and s.get("configuration") == "Development", "Exact project/target/platform/configuration mismatch")
    require(re.fullmatch(r"[A-Za-z0-9]{1,20}", s["run_tag"]), "Invalid run tag")
    for key in ("python", "powershell"):
        read_pin(s["runtime"][key])
    require(s["runtime"]["python"]["path"] == str(Path(sys.executable)), "Run inspector with sealed exact Python executable")
    local = Path(__file__).absolute()
    require(s["validator"]["path"] == str(local), "Wrong validator path")
    read_pin(s["validator"])
    require(s["runner"]["path"] == str(local.parent / "Invoke-SealedPackage.ps1"), "Wrong owner dispatcher path")
    read_pin(s["runner"])
    # Pin imported validator/spec as executable evidence, not only the entry point.
    require(s["qa_validator"]["path"] == str(local.parent / "qa_validation.py"), "Wrong QA validator path")
    qa_module_bytes = read_pin(s["qa_validator"])
    require(s["quit_requirements"]["path"] == str(local.parent / "quit-requirements.json"), "Wrong reviewed quit requirements path")
    tools = s["tool_pins"]
    required_tools = [engine / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat",
                      engine / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe",
                      engine / "Engine" / "Binaries" / "Win64" / "UnrealPak.exe",
                      engine / "Engine" / "Build" / "Build.version",
                      engine / "Engine" / "Binaries" / "Win64" / "UnrealEditor.modules"]
    require(len({p["path"].casefold() for p in tools}) == len(tools), "Duplicate tool pin")
    require(set(map(str, required_tools)) <= {p["path"] for p in tools}, "Missing exact engine tool pins")
    require({str(local), s["qa_validator"]["path"], s["runner"]["path"], s["quit_requirements"]["path"],
             s["runtime"]["python"]["path"], s["runtime"]["powershell"]["path"]} <= {p["path"] for p in tools},
            "Validator/spec/dispatcher/runtime bootstrap pins missing")
    for p in tools:
        read_pin(p)
    version = load(stable_bytes(engine / "Engine" / "Build" / "Build.version"))
    require(version.get("MajorVersion") == 5 and version.get("MinorVersion") == 8, "Require reviewed UE5.8 engine")
    # Full tool manifest pins UAT/UBT directories, so a bat wrapper alone is never enough.
    tool_inventory = read_json_pin(s["engine_tool_manifest"])
    actual_tools = {x: scan_tree(engine / x) for x in ("Engine/Binaries/DotNET/AutomationTool", "Engine/Binaries/DotNET/UnrealBuildTool")}
    require(all(v["exists"] and v["files"] for v in actual_tools.values()), "Installed UAT/UBT tools missing")
    require(tool_inventory == actual_tools, "Engine UAT/UBT tool inventory changed")
    manifest = read_json_pin(s["input_manifest"])
    compare_inventory(manifest, input_inventory(root), "candidate input")
    validate_maps(s, manifest)
    editor = validate_editor(s, s["input_manifest"]["sha256"])
    dll = s["editor_products"][next(i for i, p in enumerate(s["editor_products"]) if Path(p["path"]).name == PRODUCTS[0])]
    requirements = read_json_pin(s["quit_requirements"])
    gate = read_json_pin(s["quit_gate"])
    qa_module = types.ModuleType("sealed_quit_validation")
    exec(compile(qa_module_bytes, s["qa_validator"]["path"], "exec"), qa_module.__dict__)
    qa = qa_module.validate_quit_gate(gate, {"project": s["project"], "root": root,
        "input_manifest_sha256": s["input_manifest"]["sha256"], "editor_dll": Path(dll["path"]),
        "editor_dll_sha256": dll["sha256"], "editor_finished_utc": editor["finished_utc"],
        "requirements": requirements, "pin_reader": read_pin, "file_pin": file_pin,
        "editor_executable": str(required_tools[1])})
    validate_integration_review(s, s["input_manifest"]["sha256"], dll["sha256"])
    protected = s["protected_saves"]
    require(isinstance(protected, list) and len(protected) >= 2 and len({x["root"].casefold() for x in protected}) == len(protected),
            "Pin original and default-user save inventories separately")
    require(os.environ.get("LOCALAPPDATA"), "LOCALAPPDATA must resolve for default-save preservation")
    mandatory_saves = {str(original / "Saved" / "SaveGames").casefold(),
                       str(physical(os.environ["LOCALAPPDATA"]) / "Rogue10m" / "Saved" / "SaveGames").casefold()}
    require(mandatory_saves <= {x["root"].casefold() for x in protected}, "Original/default-user save roots missing")
    for entry in protected:
        saved = physical(entry["root"])
        require(not child(saved, root) and saved != root, "Protected production saves must be outside candidate")
        baseline = read_json_pin(entry["manifest"])
        compare_inventory(baseline, scan_tree(saved, True), "protected saves")
        expected = {str(saved / f["path"]).casefold(): {"hash": f["sha256"], "attributes": f["attributes"]} for f in baseline["files"]}
        # Receipt-local preservation alone cannot prove that protected files
        # were inventoried at all. Require complete real-root coverage as well.
        for run in gate["receipts"]:
            raw = read_json_pin(run["receipt"])
            for phase in ("before", "after"):
                observed = {}
                for path, facts in raw[phase].items():
                    p = physical(path)
                    if child(p, saved):
                        identity = str(p).casefold()
                        require(identity not in observed, "Duplicate protected QA save identity")
                        observed[identity] = {"hash": facts["hash"].upper(), "attributes": facts["attributes"]}
                require(observed == expected, "Quit QA protected-save coverage differs from actual sealed baseline")
    release = validate_release(s, seal_sha, {"path": release_path, "sha256": release_sha}, owner_id)
    # Inputs and canonical six products must remain stable throughout evidence reads.
    compare_inventory(manifest, input_inventory(root), "candidate input after evidence reads")
    for p in s["editor_products"]:
        read_pin(p)
    out_root = physical(s["output_root"], True)
    require(out_root.is_dir() and child(out_root, owner) and not child(out_root, root) and not child(root, out_root)
            and not child(out_root, engine) and not child(out_root, original) and out_root != original,
            "Output parent must be an existing independent owner directory")
    output = s["outputs"]
    require(set(output) == set(OUTPUTS), "Exactly six output paths required")
    paths = [physical(output[k]) for k in OUTPUTS]
    require(len({str(x).casefold() for x in paths}) == 6 and all(x.parent == out_root for x in paths),
            "Six distinct direct sibling output paths required")
    require(all(not child(p, root) and p != root and not child(p, engine) for p in paths), "Output overlaps input/engine")
    for entry in protected:
        protected_root = physical(entry["root"])
        require(not child(out_root, protected_root) and not child(protected_root, out_root) and out_root != protected_root,
                "Outputs overlap protected saves")
    lease = physical(str(out_root / (s["run_tag"] + ".lease")))
    if fresh:
        require(all(not p.exists() for p in paths) and not lease.exists(), "Fresh output6/lease required; preserve prior data")
        for name in ("Rogue10m.exe", "Rogue10m.target"):
            require(not (root / "Binaries" / "Win64" / name).exists(), "Old Game product present; use a fresh candidate, never delete/reuse")
    return s, release, qa


def argument_plan(s, seal_sha, release_sha):
    out = s["outputs"]
    require(not re.search(r"\s", s["root"]) and all(not re.search(r"\s", p) for p in out.values()),
            "Candidate/output path whitespace is unsupported until the BAT argv boundary is proven")
    uat = str(Path(s["engine"]) / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat")
    ubt = "-WaitMutex -NoXGE -NoSNDBS -NoFASTBuild -UBA -UBADisableRemote -MaxParallelActions=4 -NoLiveCoding -NoHotReload"
    cooker = ('-DisablePython -NoLiveCoding -NoSound -NoRemoteShaderCompile -language=en -locale=en-US '
              '-UserDir="' + out["cook_user"] + '" '
              '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False')
    sdk = ["-NoCompileUAT", "-NoCompile", "-WaitForUATMutex", "Turnkey", "-command=VerifySdk", "-platform=Win64",
           "-project=" + s["project"], "-unattended", "-utf8output"]
    bcr = ["-NoCompileUAT", "-NoCompile", "-WaitForUATMutex", "BuildCookRun", "-nocompileeditor", "-UbtArgs=" + ubt,
           "-project=" + s["project"], "-noP4", "-platform=Win64", "-clientconfig=Development", "-target=Rogue10m",
           "-build", "-cook", "-stage", "-pak", "-iostore", "-package", "-prereqs", "-archive",
           "-archivedirectory=" + out["archive"], "-map=" + "+".join(s["cook_maps"]), "-unattended", "-utf8output",
           "-installed", "-AdditionalCookerOptions=" + cooker, "-CookOutputDir=" + out["cook"], "-stagingdirectory=" + out["stage"]]
    result = {"schema": "R10-candidate-package-plan-v1", "status": "ready_arguments_only", "executed": False,
              "seal_sha256": seal_sha, "release_sha256": release_sha, "project": s["project"], "engine": s["engine"],
              "output_root": s["output_root"], "outputs": out, "run_tag": s["run_tag"],
              "lease": str(Path(s["output_root"]) / (s["run_tag"] + ".lease")), "runtime": s["runtime"],
              "tool_pins": list({p["path"]: p for p in s["tool_pins"] + list(s["runtime"].values())}.values()),
              "uat": uat, "sdk_args": sdk, "bcr_args": bcr,
              "maps": s["cook_maps"], "active_destination": s["active_destination"],
              "packaged_qa": {"user_dir": out["qa_user"], "namespace": "R10QA_" + s["run_tag"] + "_",
                              "routing_probe_required": True, "profile_slot_override_available": False},
              "execution_owner_live_check": "PowerShell dispatcher checks exact current PID/start ticks/exe/SHA; no process termination"}
    result["plan_sha256"] = digest(canonical(result))
    return result


def payload_manifest(windows):
    tree = scan_tree(windows)
    require(tree["exists"] and tree["files"], "Empty package archive")
    # Runtime Saved is explicitly mutable, all other bytes remain payload identity.
    return [p for p in tree["files"] if "saved" not in [x.casefold() for x in PurePosixPath(p["path"]).parts]]


def postflight(s, plan, evidence):
    evidence = physical(evidence, True)
    require(evidence == Path(s["outputs"]["evidence"]), "Wrong package evidence directory")
    journal = load(stable_bytes(evidence / "wrapper-journal.json"))
    require(journal.get("plan_sha256") == plan["plan_sha256"] and journal.get("sdk_exit") == 0
            and journal.get("buildcookrun_exit") == 0, "Completed exact-plan SDK/BCR execution journal missing")
    log = stable_bytes(evidence / "buildcookrun.log").decode("utf-8-sig")
    require(not RED.search(log), "Cook/build log has error/ensure")
    require(s["project"] in log and "Rogue10m Win64 Development" in log and "BUILD SUCCESSFUL" in log,
            "Exact Game target build success not proven")
    ubt_files = [p for p in (evidence / "uat-bcr-logs").glob("*.txt")
                 if re.match(r"^(?:UBA|UBT)-Rogue10m-Win64-Development", p.name)]
    require(len(ubt_files) == 1, "Exactly one dedicated current Game UBT log required")
    ubt_log = stable_bytes(ubt_files[0]).decode("utf-8-sig")
    require(not RED.search(ubt_log), "Game UBT log has error/ensure")
    for flag in ("-WaitMutex", "-NoXGE", "-NoSNDBS", "-NoFASTBuild", "-UBADisableRemote", "-MaxParallelActions=4"):
        require(flag in ubt_log, "Actual UBT flag missing: " + flag)
    require("Link [x64] Rogue10m.exe" in ubt_log and "WriteMetadata Rogue10m.target" in ubt_log, "Fresh Game link/metadata not proven")
    require("Using Unreal Build Accelerator local executor" in ubt_log and "Executing up to 4 actions" in ubt_log,
            "Actual local executor/four-action cap not proven")
    require("Using XGE" not in ubt_log and "Using FASTBuild" not in ubt_log and "Using SN-DBS" not in ubt_log, "Unexpected remote executor")
    root = Path(s["root"])
    game = root / "Binaries" / "Win64" / "Rogue10m.exe"
    native_path = root / "Binaries" / "Win64" / "Rogue10m.target"
    native = load(stable_bytes(native_path))
    require(native.get("TargetName") == "Rogue10m" and native.get("TargetType") == "Game"
            and native.get("Platform") == "Win64" and native.get("Configuration") == "Development"
            and native.get("Version", {}).get("BuildId") == s["engine_build_id"]
            and native.get("Launch") == "$(ProjectDir)/Binaries/Win64/Rogue10m.exe", "Game target receipt identity mismatch")
    require(os.path.abspath(native_path.parent / native.get("Project", "")) == s["project"], "Game receipt resolves to foreign project")
    started = utc(journal["buildcookrun_started_utc"]).timestamp()
    require(game.stat().st_mtime >= started and native_path.stat().st_mtime >= started, "Game products predate current build")
    windows = Path(s["outputs"]["archive"]) / "Windows"
    archived = windows / "Rogue10m" / "Binaries" / "Win64" / "Rogue10m.exe"
    require(file_pin(game)["sha256"] == file_pin(archived)["sha256"], "Archive inner EXE differs from fresh linked Game")
    cook_lines = [line for line in log.splitlines() if re.search(r"Running:.*UnrealEditor-Cmd\.exe.*-run=Cook", line, re.I)]
    require(len(cook_lines) == 1, "Require one actual Cook invocation")
    line = cook_lines[0]
    exact_cooker_command(line, s["project"], s["outputs"]["cook_user"])
    # Zen project identity is path-derived and must be measured/reviewed per candidate.
    require(s["zen_project"] and f"Zen oplog '{s['zen_project']}/Windows' created" in log, "Exact candidate Zen identity not proven")
    payload = payload_manifest(windows)
    pak = windows / "Rogue10m" / "Content" / "Paks"
    utoc = sorted(pak.glob("*.utoc"))
    require(utoc and all(p.with_suffix(".ucas").is_file() for p in utoc), "IoStore containers/pairs absent")
    return {"schema": "R10-candidate-package-result-v1", "status": "package_built_runtime_validation_pending",
            "executed_by_this_validator": False, "project": s["project"], "plan_sha256": plan["plan_sha256"],
            "seal_sha256": plan["seal_sha256"], "release_sha256": plan["release_sha256"],
            "input_manifest_sha256": s["input_manifest"]["sha256"], "editor_products": s["editor_products"],
            "game": file_pin(game), "game_receipt": file_pin(native_path), "archive_game": file_pin(archived),
            "build_log": file_pin(evidence / "buildcookrun.log"), "ubt_log": file_pin(ubt_files[0]),
            "payload": payload, "payload_sha256": digest(canonical(payload)), "iostore_containers": [file_pin(p) for p in utoc],
            "next_iostore": {"exe": str(Path(s["engine"]) / "Engine" / "Binaries" / "Win64" / "UnrealPak.exe"),
                "args": ["-ListContainer=" + str(pak / "*.utoc"), "-Csv=" + str(evidence / "iostore.csv"),
                         "-unattended", "-utf8output", "-DisablePython", "-NoLiveCoding", "-UserDir=" + s["outputs"]["qa_user"],
                         "-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False"],
                "required_packages": s["required_iostore_packages"]},
            "pending": ["Owned UnrealPak listing and exact ExportBundleData identity", "Fresh QA UserDir actual Saved routing probe",
                        "DX12 bootstrap/menu/cancel/confirm/restart save verification in actual package",
                        "Compact direct load and public portal/travel play", "Post-runtime protected-save and payload rehash"]}


def normalize_identity(value, filename=False):
    require(isinstance(value, str), "Invalid IoStore identity type")
    value = value.strip().replace("\\", "/")
    if not value:
        return None
    if filename:
        # Normalize exact mount roots, including legitimate engine/plugin rows.
        # Non-project exports cannot satisfy any required /Game identity.
        if value.startswith("../../../Rogue10m/Content/"):
            value = "/Game/" + value[len("../../../Rogue10m/Content/"):]
        elif value.startswith("../../../Engine/Content/"):
            value = "/Engine/" + value[len("../../../Engine/Content/"):]
        else:
            mount = re.fullmatch(r"\.\./\.\./\.\./(?:Engine|Rogue10m)/Plugins/([A-Za-z0-9_/]+)/Content/(.+)", value)
            require(mount is not None, "Unknown IoStore filename mount")
            value = "/" + mount[1].split("/")[-1] + "/" + mount[2]
        value = re.sub(r"\.(?:umap|uasset|uexp|ubulk)$", "", value, flags=re.I)
    require(re.fullmatch(r"/[\w-]+(?:/[\w-]+)+", value),
            "Invalid exact IoStore package identity")
    return value


def validate_iostore(csv_data, required):
    reader = csv.DictReader(io.StringIO(csv_data.decode("utf-8-sig")), skipinitialspace=True)
    if reader.fieldnames:
        reader.fieldnames = [x.strip() for x in reader.fieldnames]
    require(reader.fieldnames and "ChunkType" in reader.fieldnames and ("PackageName" in reader.fieldnames or "Filename" in reader.fieldnames),
            "IoStore CSV requires identity and ChunkType columns")
    require(len(set(reader.fieldnames)) == len(reader.fieldnames), "Duplicate CSV columns")
    found, exports = set(), 0
    for row in reader:
        require(None not in row and all(isinstance(x, str) for x in row.values()), "Malformed IoStore CSV row")
        if row["ChunkType"].strip() != "ExportBundleData":
            continue
        exports += 1
        package = normalize_identity(row.get("PackageName", ""))
        filename = normalize_identity(row.get("Filename", ""), True)
        require(not package or not filename or package == filename, "IoStore PackageName/Filename conflict")
        identity = package or filename
        require(identity, "Empty IoStore export identity")
        found.add(identity)
    require(exports and set(required) <= found, "Required exact map/package missing ExportBundleData")
    return {"export_rows": exports, "required_packages": required,
            "world_partition_cells": sorted(x for x in found if "/_Generated_/" in x),
            "scope": "Exact cooked identities only; generated cells are reported, not inferred editor external-package identities"}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--seal", required=True); p.add_argument("--seal-sha256", required=True)
    p.add_argument("--release"); p.add_argument("--release-sha256"); p.add_argument("--owner-id", default="")
    phase = p.add_mutually_exclusive_group()
    phase.add_argument("--plan", action="store_true")
    phase.add_argument("--check-immutable", action="store_true")
    phase.add_argument("--postflight")
    phase.add_argument("--verify-iostore")
    a = p.parse_args(argv)
    try:
        # UNSEALED is rejected before release/root probing. No executable argv leaks from templates.
        seed = read_json_pin({"path": a.seal, "sha256": a.seal_sha256})
        require(seed.get("status") == "sealed", "UNSEALED: final candidate/map/input/Editor/QA values are absent")
        require(a.release and a.release_sha256 and a.owner_id, "Exact owner release, external release SHA and owner-id required")
        s, release, qa = immutable(a.seal, a.seal_sha256, a.release, a.release_sha256, a.owner_id,
                                   fresh=not (a.check_immutable or a.postflight or a.verify_iostore))
        plan = argument_plan(s, a.seal_sha256, a.release_sha256)
        result = postflight(s, plan, a.postflight) if a.postflight else plan
        if a.verify_iostore:
            # File must be the fresh phase output; listing execution provenance stays in owner journal.
            require(physical(a.verify_iostore) == Path(s["outputs"]["evidence"]) / "iostore.csv", "Wrong listing path")
            result = validate_iostore(stable_bytes(a.verify_iostore), s["required_iostore_packages"])
            result.update(status="iostore_identity_only_passed", executed=False)
        if a.postflight or a.verify_iostore:
            immutable(a.seal, a.seal_sha256, a.release, a.release_sha256, a.owner_id, fresh=False)
        print(json.dumps(result, ensure_ascii=False, indent=2)); return 0
    except (ValueError, KeyError, TypeError, OSError, json.JSONDecodeError, UnicodeError) as e:
        print(json.dumps({"status": "blocked", "executed": False, "blocker": str(e)}, ensure_ascii=False, indent=2)); return 2


if __name__ == "__main__":
    raise SystemExit(main())
