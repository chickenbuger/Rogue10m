"""Internal Development artifact preparation. No process/GUI/engine execution.

This is a separate smoke contract. Strict release/quit gates are never relaxed.
The owner dispatcher is the only packaging execution boundary.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import sys
import types

HERE = Path(__file__).absolute().parent
HELPER_SHA = "D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF"
FLAGS = {"UI7": "UNVERIFIED", "visual_review": "UNVERIFIED", "release_eligible": False}
LIMITS = {"physical_play": "UNVERIFIED", "montage_vfx_visuals": "UNVERIFIED", "save_routing": "UNVERIFIED",
          "production_npc_behavior": "UNVERIFIED", "metadata_clone_QA": "BLOCKED_PENDING_SEPARATE_APPROVAL"}


def helpers():
    import hashlib
    p = HERE / "strict_primitives.py"
    data = p.read_bytes()
    if hashlib.sha256(data).hexdigest().upper() != HELPER_SHA:
        raise ValueError("Pinned isolated Cook-Fix-E helper changed; upstream strict packets remain immutable")
    module = types.ModuleType("sealed_strict_primitives")
    module.__file__ = str(p)
    exec(compile(data, str(p), "exec"), module.__dict__)
    module.read_pin({"path": str(p), "sha256": HELPER_SHA})
    return module


def stamp(result):
    return {**result, "scope": "internal_development_smoke", "acceptance_flags": dict(FLAGS),
            "release_eligible": False, "not_executed_by_this_validator": True, "known_limitations": dict(LIMITS)}


def required_tools(c, s):
    engine = Path(s["engine"])
    names = ("Engine/Build/BatchFiles/RunUAT.bat", "Engine/Binaries/Win64/UnrealEditor-Cmd.exe",
             "Engine/Binaries/Win64/UnrealPak.exe", "Engine/Build/Build.version", "Engine/Binaries/Win64/UnrealEditor.modules")
    return [str(engine / p) for p in names] + [str(HERE / p) for p in (
        "strict_primitives.py", "internal_smoke.py", "native51_validation.py", "native51-policy.json", "Invoke-InternalSmoke.ps1")] + [p["path"] for p in s["runtime"].values()]


def editor(c, s):
    root = Path(s["root"]); binary = root / "Binaries/Win64"
    products = s["editor_products"]
    c.require(len(products) == 6 and sorted(p["path"] for p in products) == sorted(str(binary / n) for n in c.PRODUCTS), "Exact Editor6 names/paths required")
    for pin in products:
        c.read_pin(pin)
    dll = next(p for p in products if Path(p["path"]).name == c.PRODUCTS[0])
    build = c.read_json_pin(s["editor_build"]["result"])
    c.require(type(build.get("exit_code")) is int and build["exit_code"] == 0 and build.get("dll_sha256", "").upper() == dll["sha256"], "Actual Editor build exit/DLL mismatch")
    c.require(c.utc(build["started_utc"]) < c.utc(build["ended_utc"]), "Editor build time interval invalid")
    log = c.read_pin(s["editor_build"]["log"]).decode("utf-8-sig")
    c.require(s["project"] in log and "Rogue10mEditor" in log and "Result: Succeeded" in log and not c.RED.search(log), "Actual Editor build log result/identity/error mismatch")
    target = c.load(c.stable_bytes(binary / "Rogue10mEditor.target"))
    c.require(target.get("TargetName") == "Rogue10mEditor" and target.get("TargetType") == "Editor" and target.get("Platform") == "Win64"
              and target.get("Configuration") == "Development" and target.get("Version", {}).get("BuildId") == s["engine_build_id"]
              and os.path.abspath(binary / target.get("Project", "")) == s["project"], "Native Editor target identity mismatch")
    project_modules = c.load(c.stable_bytes(binary / "UnrealEditor.modules"))
    engine_modules = c.load(c.stable_bytes(Path(s["engine"]) / "Engine/Binaries/Win64/UnrealEditor.modules"))
    c.require(project_modules.get("BuildId") == engine_modules.get("BuildId") == s["engine_build_id"] and
              project_modules.get("Modules") == {"Rogue10m": c.PRODUCTS[0], "Rogue10mEditor": c.PRODUCTS[2]}, "Engine/project module identity mismatch")
    return dll, build


def release(c, s, sha, pin, owner):
    r = c.read_json_pin(pin)
    c.require(r.get("schema") == "R10-native51-smoke-release-v1" and r.get("status") == "released_for_native51_smoke", "A separate Native51 artifact owner release is required")
    for key, expected in {"seal_sha256": sha, "project": s["project"], "owner_id": owner, "run_tag": s["run_tag"], "active_destination": s["active_destination"]}.items():
        c.require(r.get(key) == expected and expected, "Smoke release binding mismatch: " + key)
    c.require(r.get("scopes") == ["Win64 Development Native51 internal smoke"] and r.get("project_build_outputs_authorized") is True and
              r.get("gui_forbidden") is True and r.get("release_eligible") is False, "Exact artifact-only/non-GUI/project-build write authority required")
    now = datetime.now(timezone.utc)
    c.require(c.utc(r["released_utc"]) <= now < c.utc(r["expires_utc"]), "Owner execution window not active/expired")
    p = r["owner_process"]
    c.require(type(p.get("pid")) is int and p["pid"] > 0 and type(p.get("start_utc_ticks")) is int and p["start_utc_ticks"] > 0
              and p.get("executable") == s["runtime"]["powershell"]["path"] and p.get("sha256") == s["runtime"]["powershell"]["sha256"], "Exact current PowerShell identity is required; dispatcher checks it live")
    return r


def content_freeze(c, s, dll):
    frozen = c.read_json_pin(s["content_freeze"])
    expected = {"schema": "R10-native51-content-freeze-v1", "status": "frozen_for_internal_smoke", "project": s["project"],
                "active_destination": s["active_destination"], "input_manifest_sha256": s["input_manifest"]["sha256"],
                "map_manifest_sha256": s["map_manifest"]["sha256"], "editor_dll_sha256": dll["sha256"], "content_state": s["content_state"]}
    c.require(all(frozen.get(k) == v for k, v in expected.items()), "Final content freeze is absent or bound to different inputs/map/DLL/state")
    c.require(c.utc(frozen["recorded_utc"]) <= datetime.now(timezone.utc) and isinstance(frozen.get("recorder"), str) and frozen["recorder"].strip(), "Recorded final content identity required")
    state = s["content_state"]
    c.require(isinstance(state, dict) and state.get("status") == "OWNER_RECORDED_FINAL" and isinstance(state.get("map_extent_m"), list)
              and len(state["map_extent_m"]) == 2 and all(type(n) is int and n > 0 for n in state["map_extent_m"])
              and all(isinstance(state.get(k), str) and state[k].strip() for k in ("npc_placement", "default_map")), "Final additive content state is still UNSEALED")
    c.require(isinstance(frozen.get("evidence"), list) and frozen["evidence"], "Final map/NPC/DA/Director evidence pins required")
    for pin in frozen["evidence"]:
        c.read_pin(pin)
    return frozen


def disjoint_preserved_outputs(c, output, delivery):
    output = c.physical(output)
    for key in ("archive", "source_archive"):
        old = c.physical(delivery[key])
        c.require(output != old and not c.child(output, old) and not c.child(old, output),
                  "New output overlaps preserved E5 " + key)


def validate(c, seal_pin, release_pin, owner, fresh=True):
    s = c.read_json_pin(seal_pin)
    c.require(s.get("schema") == "R10-native51-smoke-seal-v1" and s.get("status") == "sealed", "UNSEALED: Native51/final content inputs are not frozen")
    c.require(s.get("scope") == "internal_development_smoke" and s.get("acceptance_flags") == FLAGS and s.get("release_eligible") is False
              and s.get("known_limitations") == LIMITS, "UI7/visual/map limitations cannot be upgraded by a smoke seal")
    c.require(s.get("packaging_status") == "NOT_EXECUTED" and s.get("not_executed_by_this_validator") is True, "Input seal must remain a not-executed preparation record")
    root, engine = c.physical(s["root"], True), c.physical(s["engine"], True)
    c.require(root != engine and not c.child(root, engine) and not c.child(engine, root), "Project/engine overlap")
    c.require(s.get("build_root_policy") == "integration_owner_exact_root" and s["project"] == str(root / "Rogue10m.uproject"), "Exact integration-owned build root required")
    c.require(s.get("target") == "Rogue10m" and s.get("configuration") == "Development" and s.get("platform") == "Win64", "Development Game Win64 only")
    c.require(re.fullmatch(r"[A-Za-z0-9]{1,20}", s["run_tag"]), "Invalid run tag")
    c.require(s["active_destination"] == "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A", "Smoke map selection differs from this checkpoint contract")
    for key, filename in (("helper", "strict_primitives.py"), ("validator", "internal_smoke.py"), ("native_validator", "native51_validation.py"),
                          ("native_policy", "native51-policy.json"), ("runner", "Invoke-InternalSmoke.ps1")):
        c.require(s[key]["path"] == str(HERE / filename), "Bootstrap path differs: " + key)
        c.read_pin(s[key])
    c.require(s["helper"]["sha256"] == HELPER_SHA, "Cook-Fix-E helper identity changed")
    for pin in s["runtime"].values():
        c.read_pin(pin)
    c.require(set(s["runtime"]) == {"python", "powershell"} and s["runtime"]["python"]["path"] == str(Path(sys.executable)), "Sealed exact Python/PowerShell required")
    tool_pins = s["tool_pins"]
    c.require(len({p["path"].casefold() for p in tool_pins}) == len(tool_pins) and set(required_tools(c, s)) <= {p["path"] for p in tool_pins}, "Exact bootstrap/engine tool pins missing/duplicated")
    for pin in tool_pins:
        c.read_pin(pin)
    v = c.load(c.stable_bytes(engine / "Engine/Build/Build.version"))
    c.require(v.get("MajorVersion") == 5 and v.get("MinorVersion") == 8, "UE5.8 required")
    tools = {x: c.scan_tree(engine / x) for x in ("Engine/Binaries/DotNET/AutomationTool", "Engine/Binaries/DotNET/UnrealBuildTool")}
    c.require(all(x["exists"] and x["files"] for x in tools.values()) and c.read_json_pin(s["engine_tool_manifest"]) == tools, "UAT/UBT installed tool manifest differs")
    inputs = c.read_json_pin(s["input_manifest"])
    c.compare_inventory(inputs, c.input_inventory(root), "smoke inputs")
    c.require(s.get("smoke_map_selection_final") is True, "Explicit smoke cook selection is not frozen")
    c.validate_maps({**s, "map_selection_final": True}, inputs)
    dll, build = editor(c, s)
    native_module = types.ModuleType("sealed_native51")
    native_module.__file__ = s["native_validator"]["path"]
    exec(compile(c.read_pin(s["native_validator"]), s["native_validator"]["path"], "exec"), native_module.__dict__)
    source_config = {p["path"]: p["sha256"] for p in inputs["files"] if p["path"].startswith(("Source/", "Config/"))}
    c.require(s["native51"]["build_result"] == s["editor_build"]["result"], "Native51 and canonical Editor build receipt pins differ")
    native = native_module.validate_native51(s["native51"], {"root": root, "project": s["project"], "engine": s["engine"],
        "dll_sha256": dll["sha256"], "source_config": source_config, "editor_finished_utc": build["ended_utc"],
        "read_pin": c.read_pin, "policy": c.read_json_pin(s["native_policy"])})
    content_freeze(c, s, dll)
    preserved = s["preserved_artifacts"]
    c.require(isinstance(preserved, list) and preserved, "Preserved E5 delivery receipt must be pinned")
    for pin in preserved:
        c.read_pin(pin)
    delivery = c.read_json_pin(preserved[0])
    c.require(delivery.get("status") == "UNVERIFIED_INTERNAL_DEVELOPMENT_PACKAGE_PRESERVED" and delivery.get("release_eligible") is False
              and delivery.get("game_execution") == 0, "Existing E5 preservation provenance differs")
    c.read_pin(delivery["delivery_manifest"])
    protected = s["protected_saves"]
    wanted = {str(c.physical(c.ORIGINAL) / "Saved/SaveGames").casefold(), str(root / "Saved/SaveGames").casefold(),
              str(c.physical(os.environ["LOCALAPPDATA"]) / "Rogue10m/Saved/SaveGames").casefold()}
    c.require(wanted <= {p["root"].casefold() for p in protected} and len({p["root"].casefold() for p in protected}) == len(protected), "All original/build-root/default user SaveGames roots must be inventoried")
    for p in protected:
        c.compare_inventory(c.read_json_pin(p["manifest"]), c.scan_tree(p["root"], True), "protected SaveGames")
    out_root, area = c.physical(s["output_root"], True), c.physical(s["owner_area"], True)
    c.require(out_root.is_dir() and c.child(out_root, area) and out_root != root and not c.child(out_root, root) and not c.child(root, out_root)
              and out_root != engine and not c.child(out_root, engine), "Outputs need an independent existing owner directory")
    disjoint_preserved_outputs(c, out_root, delivery)
    c.require(set(s["outputs"]) == set(c.OUTPUTS), "Exactly six fresh outputs required")
    paths = [c.physical(p) for p in s["outputs"].values()]
    c.require(len({str(p).casefold() for p in paths}) == 6 and all(p.parent == out_root for p in paths), "Distinct six direct sibling outputs required")
    for p in protected:
        saved = c.physical(p["root"])
        c.require(saved != out_root and not c.child(out_root, saved) and not c.child(saved, out_root), "Outputs overlap protected SaveGames")
    prior_game = s["prior_game_products"]
    if fresh:
        c.require(not any(p.exists() for p in paths) and not (out_root / (s["run_tag"] + ".lease")).exists(), "Fresh outputs/lease required; old outputs are preserved")
        actual_game = [c.file_pin(root / "Binaries/Win64" / n) for n in ("Rogue10m.exe", "Rogue10m.target") if (root / "Binaries/Win64" / n).exists()]
        c.require(actual_game == prior_game, "Prior Game product snapshot differs; do not silently reuse or delete")
    release(c, s, seal_pin["sha256"], release_pin, owner)
    c.compare_inventory(inputs, c.input_inventory(root), "smoke inputs after evidence")
    for pin in s["editor_products"]:
        c.read_pin(pin)
    return s, native


def plan(c, s, seal_sha, release_sha, native):
    value = c.argument_plan(s, seal_sha, release_sha)
    value.pop("plan_sha256")
    value.update(schema="R10-native51-smoke-plan-v1", native51=native, prior_game_products=s["prior_game_products"],
                 packaging_status="NOT_EXECUTED", native51_save_isolation="NOT_ESTABLISHED_BY_RETAINED_TESTS",
                 content_state=s["content_state"], content_freeze=s["content_freeze"], preserved_artifacts=s["preserved_artifacts"])
    value = stamp(value)
    value["plan_sha256"] = c.digest(c.canonical(value))
    return value


def stage_archive_chain(c, s, built, log):
    commands = re.findall(r'^Parsing command line:\s*(.+)$', log, re.M)
    c.require(len(commands) == 1, "One BCR archive command required")
    values = re.findall(r'(?:^|\s)-archivedirectory=(?:"([^"\r\n]+)"|([^\s"\r\n]+))(?=\s|$)', commands[0], re.I)
    c.require(len(values) == 1 and (values[0][0] or values[0][1]) == s["outputs"]["archive"], "Actual BCR archive path differs from new physical output")
    staged = Path(s["outputs"]["stage"]) / "Windows/Rogue10m/Binaries/Win64/Rogue10m.exe"
    pin = c.file_pin(staged)
    c.require(pin["sha256"] == built["game"]["sha256"] == built["archive_game"]["sha256"], "Staged/Game/archive EXE identity differs")
    return {"staged_game": pin, "stage_archive_chain": "EXACT_NEW_OUTPUTS"}


def bind_retained_references(c, s):
    # Observational pins only. No seal/release/approval and no engine call.
    root = Path(s["root"])
    base = root / "Feature/doc/evidence/npc-combat-credit-20261113A"
    native = base / "Native51B"
    paths = {k: native / name for k, name in {"process": "process.json", "result": "result.json", "reviewed": "reviewed-result.json",
             "report": "report/index.json", "log": "editor.log", "selection": "selection.json"}.items()}
    paths.update(build_result=base / "editor-build-fixture-result.json", launcher=root / "Feature/implementation/npc-combat-credit-20261113A/run_native51_b.py", preservation=base / "preservation-result.json")
    if all(p.is_file() for p in paths.values()):
        s["native51"] = {k: c.file_pin(p) for k, p in paths.items()}
        s["editor_build"] = {"result": s["native51"]["build_result"], "log": c.file_pin(base / "editor-build-fixture.log")}
    s["engine_build_id"] = c.load(c.stable_bytes(root / "Binaries/Win64/UnrealEditor.modules"))["BuildId"]
    preserved = root / "Feature/doc/evidence/map-resume-20261113/InternalSmokeD/persistent-delivery05.json"
    if preserved.is_file():
        s["preserved_artifacts"] = [c.file_pin(preserved)]
    return s


def capture(c, root, engine, area, output, maps, powershell):
    root, engine, area, output = c.physical(root, True), c.physical(engine, True), c.physical(area, True), c.physical(output)
    c.require(not output.exists() and output.parent.is_dir() and c.child(output, area) and not c.child(output, root) and not c.child(output, engine), "Fresh independent owner capture output required")
    previous = root / "Feature/doc/evidence/map-resume-20261113/InternalSmokeD/persistent-delivery05.json"
    if previous.is_file():
        disjoint_preserved_outputs(c, output, c.read_json_pin(c.file_pin(previous)))
    inputs = c.input_inventory(root)
    files = {p["path"]: p for p in inputs["files"]}
    entries = []
    for package in maps:
        c.require(re.fullmatch(r"/Game/[A-Za-z0-9_/]+", package), "Exact /Game map package required")
        rel = package[6:]; external = ["Content/__ExternalActors__/" + rel, "Content/__ExternalObjects__/" + rel]
        c.require("Content/" + rel + ".umap" in files, "Selected smoke map absent")
        entries.append({"package": package, "external_roots": external, "external_roots_present": {p: p in inputs["directories"] for p in external},
                        "files": [p for p in inputs["files"] if p["path"] == "Content/" + rel + ".umap" or any(p["path"].startswith(e + "/") for e in external)]})
    tools = {p: c.scan_tree(engine / p) for p in ("Engine/Binaries/DotNET/AutomationTool", "Engine/Binaries/DotNET/UnrealBuildTool")}
    save_roots = {c.physical(c.ORIGINAL) / "Saved/SaveGames", root / "Saved/SaveGames", c.physical(os.environ["LOCALAPPDATA"]) / "Rogue10m/Saved/SaveGames"}
    saves = [(p, c.scan_tree(p, True)) for p in sorted(save_roots)]
    c.require(inputs == c.input_inventory(root), "Inputs moved during capture")
    c.require(all(not c.child(output, p) and not c.child(p, output) for p, _ in saves), "Capture overlaps protected saves")
    output.mkdir(); c.physical(output, True)
    def write(name, data):
        c.physical(output, True)
        c.require(output.is_dir() and c.child(output, area) and not c.child(output, root) and not c.child(output, engine)
                  and all(not c.child(output, p) and not c.child(p, output) for p, _ in saves), "Capture scope changed before write")
        p = output / name
        c.physical(p)
        with p.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(data, stream, ensure_ascii=False, indent=2); stream.write("\n")
        return c.file_pin(p)
    s = stamp({"schema": "R10-native51-smoke-seal-v1", "status": "UNSEALED", "packaging_status": "NOT_EXECUTED", "root": str(root),
        "project": str(root / "Rogue10m.uproject"), "engine": str(engine), "owner_area": str(area), "build_root_policy": "integration_owner_exact_root",
        "engine_build_id": None, "target": "Rogue10m", "platform": "Win64", "configuration": "Development", "run_tag": None,
        "active_destination": "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A", "smoke_map_selection_final": False,
        "cook_maps": maps, "required_iostore_packages": maps, "input_manifest": write("inputs.json", inputs),
        "map_manifest": write("maps.json", {"schema": "R10-candidate-maps-v1", "root": str(root), "maps": entries}),
        "engine_tool_manifest": write("engine-tools.json", tools), "protected_saves": [], "native51": None, "editor_build": None,
        "content_freeze": None, "content_state": {"status": "UNSEALED_PENDING_FINAL_CONTENT", "last_reported_extent_m": [1800, 1800], "npc_placement": "ADDITIVE_WORK_IN_PROGRESS", "default_map": "OWNER_TO_RECORD"},
        "preserved_artifacts": [],
        "editor_products": [c.file_pin(root / "Binaries/Win64" / n) for n in c.PRODUCTS], "output_root": None,
        "outputs": {k: None for k in c.OUTPUTS}, "zen_project": None, "tool_pins": [],
        "runtime": {"python": c.file_pin(Path(sys.executable)), "powershell": c.file_pin(powershell)},
        "prior_game_products": [c.file_pin(root / "Binaries/Win64" / n) for n in ("Rogue10m.exe", "Rogue10m.target") if (root / "Binaries/Win64" / n).exists()]})
    for key, name in (("helper", "strict_primitives.py"), ("validator", "internal_smoke.py"), ("native_validator", "native51_validation.py"), ("native_policy", "native51-policy.json"), ("runner", "Invoke-InternalSmoke.ps1")):
        s[key] = c.file_pin(HERE / name)
    s["tool_pins"] = [c.file_pin(p) for p in required_tools(c, s)]
    for i, (p, inventory) in enumerate(saves):
        s["protected_saves"].append({"root": str(p), "manifest": write(f"protected-save-{i}.json", inventory)})
    bind_retained_references(c, s)
    c.compare_inventory(inputs, c.input_inventory(root), "capture inputs after retained evidence")
    for pin in s["editor_products"]:
        c.read_pin(pin)
    write("smoke.UNSEALED.json", s)
    return stamp({"status": "captured_UNSEALED", "executed": False, "capture_directory": str(output)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seal"); parser.add_argument("--seal-sha256"); parser.add_argument("--release"); parser.add_argument("--release-sha256"); parser.add_argument("--owner-id")
    phases = parser.add_mutually_exclusive_group(); phases.add_argument("--plan", action="store_true"); phases.add_argument("--check-immutable", action="store_true"); phases.add_argument("--postflight"); phases.add_argument("--verify-iostore"); phases.add_argument("--capture", action="store_true")
    parser.add_argument("--root"); parser.add_argument("--engine"); parser.add_argument("--owner-area"); parser.add_argument("--capture-output"); parser.add_argument("--maps", nargs="+"); parser.add_argument("--powershell")
    a = parser.parse_args()
    try:
        c = helpers()
        if a.capture:
            result = capture(c, a.root, a.engine, a.owner_area, a.capture_output, a.maps, a.powershell)
        else:
            seal = {"path": a.seal, "sha256": a.seal_sha256}
            seed = c.read_json_pin(seal)
            c.require(seed.get("schema") == "R10-native51-smoke-seal-v1" and seed.get("status") == "sealed", "UNSEALED: Native51/final content has no smoke seal; no executable arguments")
            s, native = validate(c, seal, {"path": a.release, "sha256": a.release_sha256}, a.owner_id, not (a.check_immutable or a.postflight or a.verify_iostore))
            result = plan(c, s, a.seal_sha256, a.release_sha256, native)
            if a.postflight:
                cook_log = c.stable_bytes(Path(a.postflight) / "buildcookrun.log").decode("utf-8-sig")
                identities = re.findall(r"Zen oplog '(Rogue10m\.[0-9A-Fa-f]{8})/Windows' created", cook_log)
                c.require(len(identities) == 1, "One actual smoke Zen oplog identity required")
                c.require(s.get("zen_project") in (None, identities[0]), "Explicit sealed Zen identity differs from cook")
                built = c.postflight({**s, "zen_project": identities[0]}, result, a.postflight)
                chain = stage_archive_chain(c, s, built, cook_log)
                result = stamp({**built, **chain, "schema": "R10-native51-smoke-result-v1", "status": "internal_smoke_artifact_built_UNVERIFIED",
                    "native51": native, "content_state": s["content_state"], "content_freeze": s["content_freeze"], "iostore_identity": "UNVERIFIED", "packaging_status": "OWNER_UAT_COMPLETED",
                    "observed_zen_project": identities[0], "zen_presealed": s.get("zen_project") is not None})
            if a.verify_iostore:
                c.require(c.physical(a.verify_iostore) == Path(s["outputs"]["evidence"]) / "iostore.csv", "Wrong smoke CSV path")
                package = c.load(c.stable_bytes(Path(s["outputs"]["evidence"]) / "postflight.json"))
                c.require(package.get("schema") == "R10-native51-smoke-result-v1" and package.get("plan_sha256") == result["plan_sha256"] and package.get("release_eligible") is False, "Exact Native51 smoke package receipt required")
                payload = c.payload_manifest(Path(s["outputs"]["archive"]) / "Windows")
                c.require(c.digest(c.canonical(payload)) == package["payload_sha256"], "Package payload changed since build")
                result = stamp({**c.validate_iostore(c.stable_bytes(a.verify_iostore), s["required_iostore_packages"]),
                    "schema": "R10-native51-smoke-result-v1", "status": "iostore_csv_identity_only_UNVERIFIED", "executed": False,
                    "listing_execution_provenance": "OWNER_RECEIPT_REQUIRED", "physical_ui_and_save_routing": "UNVERIFIED"})
            if a.postflight or a.verify_iostore:
                validate(c, seal, {"path": a.release, "sha256": a.release_sha256}, a.owner_id, False)
        print(json.dumps(result, ensure_ascii=False, indent=2)); return 0
    except (ValueError, TypeError, KeyError, OSError, UnicodeError) as error:
        print(json.dumps(stamp({"schema": "R10-native51-smoke-result-v1", "status": "blocked", "executed": False, "blocker": str(error)}), ensure_ascii=False, indent=2)); return 2


if __name__ == "__main__":
    raise SystemExit(main())
