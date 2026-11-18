"""Prepare a NEW internal smoke seal from capture03 and real NativeTestsD evidence.

Only this preparation writes a new output directory, seal and receipt. It never
captures inputs or starts UE, UBT, UAT, Cook, Game, MCP, GUI or an owner release.
The integration owner executes capture and later issues its live-session release.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import types


ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / "Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e"
EVIDENCE = ROOT / "Feature/doc/evidence/map-resume-20261113"
PINS = {
    "internal_smoke.py": "9AED247D8C4AE9DE43A502B33758C6816EADA3840BE9AFFD0AA4AEF1135B48DF",
    "native43_validation.py": "985500591447274B367B97A89257D1CA992BA7891890D220608D7B13B383CF98",
    "Invoke-InternalSmoke.ps1": "32A95E82FB7C68485FD90086D0F185E583BAC86A9B28287B2E69F1367C4CFBD9",
    "strict_primitives.py": "D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF",
}


def run_tag(value):
    if not re.fullmatch(r"[A-Za-z0-9]{1,20}", value):
        raise argparse.ArgumentTypeError("run-tag must contain 1..20 ASCII letters/digits")
    return value


def seal_name(value):
    reserved = {"CON", "PRN", "AUX", "NUL"} | {f"{p}{n}" for p in ("COM", "LPT") for n in range(1, 10)}
    if (not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,79}\.json", value)
            or value.split(".", 1)[0].upper() in reserved):
        raise argparse.ArgumentTypeError("seal-name must be one nonreserved ASCII .json filename")
    return value


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--native-label", choices=("NativeTestsD",), required=True)
    parser.add_argument("--run-tag", type=run_tag, required=True)
    parser.add_argument("--seal-name", type=seal_name, required=True)
    return parser


def load_pinned_module(path, digest, name):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest().upper() != digest:
        raise ValueError("Frozen runtime changed: " + str(path))
    module = types.ModuleType(name)
    module.__file__ = str(path)
    exec(compile(data, str(path), "exec"), module.__dict__)
    return module


def prepare(args):
    # Pin code before loading it; no import from an unrelated packet/sys.path.
    runtime = load_pinned_module(PACKET / "internal_smoke.py", PINS["internal_smoke.py"], "smoke_e_runtime")
    c = runtime.helpers()
    c.require(c.physical(ROOT, True) == c.physical(c.ORIGINAL, True), "Exact original integration source root required")
    for filename, digest in PINS.items():
        c.read_pin({"path": str(PACKET / filename), "sha256": digest})

    capture = c.physical(args.capture, True)
    c.require(capture.is_dir(), "Existing owner-created capture directory required; this tool does not capture")
    captured = c.physical(capture / "smoke.UNSEALED.json", True)
    seal = c.load(c.stable_bytes(captured))
    c.require(seal.get("schema") == "R10-internal-smoke-seal-v1" and seal.get("status") == "UNSEALED", "Fresh UNSEALED capture required")
    c.require(seal.get("root") == str(ROOT) and seal.get("project") == str(ROOT / "Rogue10m.uproject")
              and seal.get("build_root_policy") == "integration_owner_exact_root", "Capture source/project identity differs")
    c.require(seal.get("scope") == "internal_development_smoke" and seal.get("acceptance_flags") == runtime.FLAGS
              and seal.get("known_limitations") == runtime.LIMITS and seal.get("release_eligible") is False
              and seal.get("packaging_status") == "NOT_EXECUTED" and seal.get("not_executed_by_this_validator") is True,
              "Internal-only acceptance/limitations changed")
    c.require(seal.get("target") == "Rogue10m" and seal.get("platform") == "Win64"
              and seal.get("configuration") == "Development", "Exact Game Development target required")
    c.require(seal.get("active_destination") == "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A",
              "Captured active destination changed")
    engine = c.physical(seal["engine"], True)
    area = c.physical(seal["owner_area"], True)
    c.require(c.child(capture, area) and not c.child(capture, ROOT) and not c.child(capture, engine), "Capture is outside its independent owner area")
    c.require(set(seal["runtime"]) == {"python", "powershell"}
              and seal["runtime"]["python"]["path"] == str(Path(sys.executable)), "Use the exact captured Python/PowerShell runtimes")
    for pin in seal["runtime"].values():
        c.read_pin(pin)
    for key, filename in (("helper", "strict_primitives.py"), ("validator", "internal_smoke.py"),
                          ("native_validator", "native43_validation.py"), ("native_policy", "native43-policy.json"),
                          ("runner", "Invoke-InternalSmoke.ps1")):
        c.require(seal[key]["path"] == str(PACKET / filename), "Capture belongs to a different packet: " + key)
        c.read_pin(seal[key])
        if filename in PINS:
            c.require(seal[key]["sha256"] == PINS[filename], "Captured runtime pin differs: " + key)
    tool_pins = seal["tool_pins"]
    c.require(len({p["path"].casefold() for p in tool_pins}) == len(tool_pins)
              and set(runtime.required_tools(c, seal)) <= {p["path"] for p in tool_pins}, "Captured tool scope differs")
    for pin in tool_pins:
        c.read_pin(pin)
    version = c.load(c.stable_bytes(engine / "Engine/Build/Build.version"))
    c.require(version.get("MajorVersion") == 5 and version.get("MinorVersion") == 8, "Captured UE5.8 engine required")
    installed = {p: c.scan_tree(engine / p) for p in ("Engine/Binaries/DotNET/AutomationTool", "Engine/Binaries/DotNET/UnrealBuildTool")}
    c.require(all(tree["exists"] and tree["files"] for tree in installed.values())
              and c.read_json_pin(seal["engine_tool_manifest"]) == installed, "Installed UAT/UBT tree changed since capture")
    inputs = c.read_json_pin(seal["input_manifest"])
    c.compare_inventory(inputs, c.input_inventory(ROOT), "fresh smoke E capture inputs")
    c.validate_maps({**seal, "map_selection_final": True}, inputs)

    # Never replace stale captured Editor6 with new hashes to manufacture freshness.
    current_editor = [c.file_pin(ROOT / "Binaries/Win64" / name) for name in c.PRODUCTS]
    c.require(seal["editor_products"] == current_editor, "Captured Editor6 differs; make a new capture")
    seal.update(engine_build_id=c.load(c.stable_bytes(ROOT / "Binaries/Win64/UnrealEditor.modules"))["BuildId"],
                editor_build={"result": c.file_pin(EVIDENCE / "GameGuardFixD/result.json"),
                              "log": c.file_pin(EVIDENCE / "GameGuardFixD/editor-build.log")})
    dll, build = runtime.editor(c, seal)
    native = EVIDENCE / args.native_label
    refs = {key: c.file_pin(native / filename) for key, filename in {
        "process": "process.json", "result": "result.json", "reviewed": "reviewed-result.json",
        "report": "report/index.json", "log": "editor.log"}.items()}
    refs["selection"] = c.file_pin(ROOT / "Feature/implementation/map-resume-20261113/native-no-gui-tests-20261113.json")
    native_validator = load_pinned_module(PACKET / "native43_validation.py", PINS["native43_validation.py"], "smoke_e_native43")
    source_config = {p["path"]: p["sha256"] for p in inputs["files"] if p["path"].startswith(("Source/", "Config/"))}
    native_summary = native_validator.validate_native43(refs, {
        "root": ROOT, "project": seal["project"], "engine": seal["engine"], "dll_sha256": dll["sha256"],
        "source_config": source_config, "editor_finished_utc": build["ended_utc"],
        "read_pin": c.read_pin, "policy": c.read_json_pin(seal["native_policy"])})

    protected = seal["protected_saves"]
    wanted = {str(c.physical(c.ORIGINAL) / "Saved/SaveGames").casefold(), str(ROOT / "Saved/SaveGames").casefold(),
              str(c.physical(os.environ["LOCALAPPDATA"]) / "Rogue10m/Saved/SaveGames").casefold()}
    c.require(wanted <= {p["root"].casefold() for p in protected}
              and len({p["root"].casefold() for p in protected}) == len(protected), "Protected save roots differ")
    for item in protected:
        c.compare_inventory(c.read_json_pin(item["manifest"]), c.scan_tree(item["root"], True), "protected saves before E seal")
    current_game = [c.file_pin(ROOT / "Binaries/Win64" / name) for name in ("Rogue10m.exe", "Rogue10m.target")
                    if (ROOT / "Binaries/Win64" / name).exists()]
    c.require(current_game == seal["prior_game_products"], "Prior Game products changed since capture")

    output = c.physical(args.output_root)
    c.require(output.parent.is_dir() and c.child(output, area) and output != ROOT and not c.child(output, ROOT)
              and not c.child(ROOT, output) and output != engine and not c.child(output, engine), "Independent owner output root required")
    for item in protected:
        saved = c.physical(item["root"])
        c.require(output != saved and not c.child(output, saved) and not c.child(saved, output), "Output overlaps protected saves")
    seal_path = c.physical(capture.parent / args.seal_name)
    receipt_path = c.physical(capture.parent / (args.seal_name[:-5] + ".prepared.json"))
    c.require(not output.exists() and not seal_path.exists() and not receipt_path.exists(), "Existing output/seal/receipt preserved; choose new names")
    seal.update(status="sealed", run_tag=args.run_tag, smoke_map_selection_final=True, native43=refs,
                output_root=str(output), outputs={name: str(output / name) for name in c.OUTPUTS},
                note="Internal Development only. Fresh GameGuardFixD/NativeTestsD at the exact root; exact18 culture args and global RED0. Old startup15 not approved. No crop/NPC/default-map switch or UI/visual/physical-play approval.")
    c.argument_plan(seal, "0" * 64, "0" * 64)  # Pure argv/path check; this is not an execution release.
    c.compare_inventory(inputs, c.input_inventory(ROOT), "smoke E inputs after evidence validation")
    for pin in current_editor:
        c.read_pin(pin)

    # All missing/stale evidence is rejected before the first mutation. No cleanup
    # or overwrite follows a partial I/O failure: preserve the owned artifacts.
    output.mkdir(exist_ok=False)
    with seal_path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(seal, stream, ensure_ascii=False, indent=2); stream.write("\n")
    result = {"seal": c.file_pin(seal_path), "owner_area": seal["owner_area"], "output_root": str(output),
              "receipt": str(receipt_path), "maps": seal["cook_maps"], "native43": native_summary,
              "release_eligible": False, "status": "sealed_internal_inputs_owner_release_required",
              "owner_release_created": False, "ue_ubt_uat_cook_game_mcp_gui_calls": 0}
    with receipt_path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2); stream.write("\n")
    return result


def main(argv=None):
    args = argument_parser().parse_args(argv)
    try:
        print(json.dumps(prepare(args), ensure_ascii=False, indent=2))
        return 0
    except (ValueError, KeyError, TypeError, OSError, UnicodeError) as error:
        print(json.dumps({"status": "blocked_preparation", "error": str(error),
                          "owner_release_created": False, "release_eligible": False}, ensure_ascii=False, indent=2))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
