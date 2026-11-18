"""Create six project-owned arm derivatives through existing Unreal Editor APIs.

Compile the Editor target first. This script refuses every preexisting target/report,
saves only the selected new targets and retains any partial output when a step fails.
For the diagnosed interrupted run, -RogueOwnerArmsOnly=Or_F requires the other five
saved derivatives/reports and preserves their package hashes. It never rebuilds them.
It never calls SaveAll, saves a map, deletes an asset or edits a source mesh.
"""
from datetime import datetime, timezone
import hashlib
import json
import re
from pathlib import Path

import unreal


VARIANTS = (
    ("Human", "Male", "Hu_M"),
    ("Human", "Female", "Hu_F"),
    ("Dwarf", "Male", "Dw_M"),
    ("Dwarf", "Female", "Dw_F"),
    ("Orc", "Male", "Or_M"),
    ("Orc", "Female", "Or_F"),
)
PROJECT = Path(unreal.Paths.project_dir()).resolve()
CONTENT = Path(unreal.Paths.project_content_dir()).resolve()
ENGINE_CONTENT = Path(unreal.Paths.engine_content_dir()).resolve()
RESULT_ROOT = PROJECT / "tmp/owner-arms"


def package_files(asset_path):
    """Read only existing package payloads; never infer a write destination here."""
    package = asset_path.split(".", 1)[0]
    if package.startswith("/Game/"):
        base = CONTENT / package.removeprefix("/Game/")
    elif package.startswith("/Engine/"):
        base = ENGINE_CONTENT / package.removeprefix("/Engine/")
    else:
        raise RuntimeError(f"Unsupported package root for preservation: {package}")
    main = base.with_suffix(".uasset")
    if not main.is_file():
        raise RuntimeError(f"Original package is not readable: {main}")
    return [p for ext in (".uasset", ".uexp", ".ubulk") if (p := base.with_suffix(ext)).is_file()]


def hash_packages(paths):
    result = {}
    for asset_path in sorted(paths):
        for path in package_files(asset_path):
            result[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def write_manifest(path, data):
    # Only this fresh run's own manifest is updated; previous manifests are preserved.
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def read_report(path):
    raw = path.read_bytes()
    return raw.decode("utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")



def selected_variants():
    command_line = unreal.SystemLibrary.get_command_line()
    matches = re.findall(r"(?:^|\s)-RogueOwnerArmsOnly=([A-Za-z_]+)(?=\s|$)", command_line)
    if "-RogueOwnerArmsOnly" not in command_line:
        return VARIANTS, False
    if matches != ["Or_F"]:
        raise RuntimeError("The explicit recovery selector must occur exactly once as -RogueOwnerArmsOnly=Or_F.")
    return tuple(variant for variant in VARIANTS if variant[2] == "Or_F"), True


def main():
    selected, recovering_or_f = selected_variants()
    RESULT_ROOT.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
    manifest_path = RESULT_ROOT / f"create-owner-arms-{stamp}.json"
    if manifest_path.exists():
        raise RuntimeError("Preserve existing run manifest; choose a fresh execution.")
    manifest = {"status": "preflight", "created_utc": stamp, "outputs": [],
                "source_hashes_before": {}, "source_hashes_after": {},
                "saved_packages": [], "partial_outputs_retained": True,
                "selected_variants": [variant[2] for variant in selected],
                "preexisting_targets_before": {}, "preexisting_targets_after": {}}
    write_manifest(manifest_path, manifest)
    original_assets = set()
    originals_before = {}
    preexisting_targets = set()
    preexisting_before = {}
    try:
        jobs = []
        # Validate the entire batch before the first duplicate. In particular, an
        # interrupted prior run never becomes permission to replace its outputs.
        for race, gender, suffix in VARIANTS:
            source_path = f"/Game/StylizedCharacter/Meshes/Character/{race}/{gender}/SK_{suffix}_FullBody"
            short = f"SK_OwnerArms_{suffix}"
            target_path = f"/Game/Rogue10m/Character/OwnerArms/{short}"
            report_path = RESULT_ROOT / f"{short}.txt"
            target_file = CONTENT / target_path.removeprefix("/Game/")
            is_selected = (race, gender, suffix) in selected
            if is_selected:
                if (unreal.EditorAssetLibrary.does_asset_exist(target_path)
                        or any(target_file.with_suffix(ext).exists() for ext in (".uasset", ".uexp", ".ubulk"))
                        or report_path.exists()):
                    raise RuntimeError(f"Preserve preexisting target/report; no overwrite: {target_path}")
            elif recovering_or_f:
                if not target_file.with_suffix(".uasset").is_file() or not report_path.is_file():
                    raise RuntimeError(f"Or_F recovery requires each of the five previous derivatives/reports: {target_path}")
                previous_report = read_report(report_path)
                if "OWNER_ARMS_PASSED" not in previous_report or "OWNER_ARMS_FAILED" in previous_report:
                    raise RuntimeError(f"Preserve and inspect the previous geometry report: {report_path}")
                preexisting_targets.add(target_path)
            source = unreal.load_asset(source_path)
            if not isinstance(source, unreal.SkeletalMesh):
                raise RuntimeError(f"Missing source skeletal mesh: {source_path}")
            skeleton = source.get_editor_property("skeleton")
            if skeleton is None:
                raise RuntimeError(f"Missing source skeleton: {source_path}")
            original_assets.update((source_path, skeleton.get_path_name()))
            for slot in source.get_editor_property("materials"):
                material = slot.get_editor_property("material_interface")
                if material is not None:
                    original_assets.add(material.get_path_name())
            if is_selected:
                jobs.append((source_path, target_path, report_path, source))

        preexisting_before = hash_packages(preexisting_targets)
        manifest["preexisting_targets_before"] = preexisting_before
        originals_before = hash_packages(original_assets)
        manifest["source_hashes_before"] = originals_before
        manifest["status"] = "creating"
        write_manifest(manifest_path, manifest)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        for source_path, target_path, report_path, source in jobs:
            state = {"source": source_path, "target": target_path, "report": str(report_path),
                     "stage": "duplicate_requested", "saved": False}
            manifest["outputs"].append(state)
            write_manifest(manifest_path, manifest)
            target = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
            if not isinstance(target, unreal.SkeletalMesh) or target == source:
                raise RuntimeError(f"Expected a fresh independent duplicate: {target_path}")
            state["stage"] = "duplicate_created_not_saved_by_script"
            write_manifest(manifest_path, manifest)
            if target.get_editor_property("skeleton") != source.get_editor_property("skeleton"):
                raise RuntimeError(f"Duplicate skeleton mismatch: {target_path}")
            unreal.SystemLibrary.execute_console_command(
                world, f"Rogue10m.CreateOwnerArms {source_path} {target_path}")
            if not report_path.is_file():
                raise RuntimeError(f"C++ authoring did not produce its exact report: {target_path}")
            report = read_report(report_path)
            state["report_text"] = report
            if "OWNER_ARMS_PASSED" not in report or "OWNER_ARMS_FAILED" in report:
                raise RuntimeError(f"Arms geometry validation failed: {target_path}; {report_path}")
            state["stage"] = "geometry_validated"
            write_manifest(manifest_path, manifest)
            if not unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False):
                raise RuntimeError(f"Failed to save the new arms derivative: {target_path}")
            state["saved"] = True
            state["stage"] = "saved"
            state["package_hashes"] = hash_packages((target_path,))
            manifest["saved_packages"].append(target_path)
            write_manifest(manifest_path, manifest)
            unreal.log(f"OWNER_ARMS_SAVED asset={target_path} report={report_path}")

        originals_after = hash_packages(original_assets)
        manifest["source_hashes_after"] = originals_after
        if originals_after != originals_before:
            raise RuntimeError("Original source/skeleton/material package hashes changed; preserve and inspect.")
        manifest["preexisting_targets_after"] = hash_packages(preexisting_targets)
        if manifest["preexisting_targets_after"] != preexisting_before:
            raise RuntimeError("Previously saved derivative hashes changed; preserve and inspect.")
        manifest["status"] = "passed"
        unreal.log(f"RESULT=OWNER_ARMS_CREATED count={len(manifest['saved_packages'])} manifest={manifest_path}")
    except Exception as error:
        manifest["status"] = "failed"
        manifest["error"] = repr(error)
        unreal.log_error(f"RESULT=OWNER_ARMS_CREATE_FAILED manifest={manifest_path} error={error}; partial outputs retained")
        raise
    finally:
        preservation_error = None
        if originals_before:
            try:
                manifest["source_hashes_after"] = hash_packages(original_assets)
                manifest["original_hashes_unchanged"] = manifest["source_hashes_after"] == originals_before
                if not manifest["original_hashes_unchanged"]:
                    preservation_error = "Original source/skeleton/material package hashes changed."
            except Exception as hash_error:
                manifest["preservation_check_error"] = repr(hash_error)
                preservation_error = f"Could not finish original package hash verification: {hash_error}"
        if preexisting_before:
            try:
                manifest["preexisting_targets_after"] = hash_packages(preexisting_targets)
                manifest["preexisting_target_hashes_unchanged"] = manifest["preexisting_targets_after"] == preexisting_before
                if not manifest["preexisting_target_hashes_unchanged"]:
                    preservation_error = "Previously saved derivative hashes changed."
            except Exception as hash_error:
                manifest["preexisting_preservation_check_error"] = repr(hash_error)
                preservation_error = f"Could not verify previous derivative hashes: {hash_error}"
        if preservation_error is not None:
            manifest["status"] = "failed"
            manifest["preservation_failure"] = preservation_error
        write_manifest(manifest_path, manifest)
        if preservation_error is not None:
            raise RuntimeError(preservation_error)


if __name__ == "__main__":
    main()
