"""Resave the six player skeletons in UE 5.8 to remove obsolete Rig imports.

Run with UnrealEditor-Cmd -run=pythonscript. Use forward-slash paths in
the -Script argument. Inspect in one process, apply in another, and verify
in a third so the final dependency check reads a fresh disk registry.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal


ROOT = Path(__file__).resolve().parents[2]
LEGACY_RIG = "/Engine/EngineMeshes/Humanoid"
SKELETONS = {
    "/Game/StylizedCharacter/Meshes/Character/Human/Male/SKEL_Hu_M",
    "/Game/StylizedCharacter/Meshes/Character/Human/Female/SKEL_Hu_F",
    "/Game/StylizedCharacter/Meshes/Character/Dwarf/Male/SKEL_Dw_M",
    "/Game/StylizedCharacter/Meshes/Character/Dwarf/Female/SKEL_Dw_F",
    "/Game/StylizedCharacter/Meshes/Character/Orc/Male/SKEL_Or_M",
    "/Game/StylizedCharacter/Meshes/Character/Orc/Female/SKEL_Or_F",
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def write_report(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def dependency_options():
    options = unreal.AssetRegistryDependencyOptions()
    for name in (
        "include_soft_package_references",
        "include_hard_package_references",
        "include_game_package_references",
        "include_editor_only_package_references",
    ):
        options.set_editor_property(name, True)
    return options


def package_path(package):
    return Path("Content") / (package.removeprefix("/Game/") + ".uasset")


def disk_inventory(registry, options):
    rows = {}
    for package in sorted(SKELETONS):
        path = ROOT / package_path(package)
        dependencies = registry.get_dependencies(package, options)
        if dependencies is None:
            raise RuntimeError(f"Missing disk registry data: {package}")
        deps = sorted(str(dependency) for dependency in dependencies)
        rows[package] = {
            "sha256": sha256(path),
            "bytes": path.stat().st_size,
            "legacy_rig_dependency": LEGACY_RIG in deps,
            "dependencies": deps,
        }
    return rows


def optional_text_property(obj, name):
    try:
        value = obj.get_editor_property(name)
    except Exception:
        # Unreal Python can raise its base Exception for unreflected properties.
        return None
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value) if value is not None else None


def skeleton_state(asset):
    state = {"object_path": asset.get_path_name()}
    try:
        bone_tree = asset.get_editor_property("bone_tree")
        state["bone_count"] = len(bone_tree)
        state["bone_translation_modes"] = [
            str(node.get_editor_property("translation_retargeting_mode")) for node in bone_tree
        ]
    except Exception:
        # BoneTree is not BlueprintType on some engine builds. The material,
        # Character inheritance and retarget validation must still run separately.
        state["bone_count"] = None
        state["bone_translation_modes"] = None
    state["preview_skeletal_mesh"] = optional_text_property(asset, "preview_skeletal_mesh")
    return state


def load_skeletons():
    assets = {}
    for package in sorted(SKELETONS):
        asset = unreal.EditorAssetLibrary.load_asset(package)
        expected_object_path = f"{package}.{package.rsplit('/', 1)[1]}"
        if not isinstance(asset, unreal.Skeleton) or asset.get_path_name() != expected_object_path:
            raise RuntimeError(f"Expected an exact Skeleton asset at {expected_object_path}")
        assets[package] = asset
    return assets


def local_backup(output_dir, original):
    archive = output_dir / "original-assets"
    archive.mkdir(exist_ok=False)
    for package, row in original.items():
        path = package_path(package)
        source = ROOT / path
        copy = archive / path
        copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, copy)
        if sha256(copy) != row["sha256"] or sha256(source) != row["sha256"]:
            raise RuntimeError(f"Archive/hash mismatch: {package}")
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("probe", "apply", "verify"), required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, help="Probe JSON from the same project checkout")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if output == ROOT / "tmp" or not output.is_relative_to((ROOT / "tmp").resolve()):
        raise RuntimeError("Use a fresh output directory under the project's tmp directory")
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / "skeleton-repair.json"
    if report_path.exists():
        raise RuntimeError(f"Report already exists: {report_path}")

    report = {"mode": args.mode, "success": False, "legacy_package": LEGACY_RIG, "saved_packages": []}
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        directories = [package.rsplit("/", 1)[0] for package in sorted(SKELETONS)]
        registry.scan_paths_synchronous(directories, force_rescan=True)
        options = dependency_options()
        inventory = disk_inventory(registry, options)
        report["disk_before"] = inventory
        baseline = None
        if args.mode != "probe":
            if args.baseline is None:
                raise RuntimeError("Apply and verify require the prior probe report")
            baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
            if (not baseline.get("success") or baseline.get("mode") != "probe"
                    or set(baseline.get("disk_before", {})) != SKELETONS
                    or baseline.get("legacy_package") != LEGACY_RIG):
                raise RuntimeError("Baseline must be a successful exact six-package probe")

        if args.mode == "apply":
            if inventory != baseline["disk_before"]:
                raise RuntimeError("One or more target packages changed since the probe")
            if not all(row["legacy_rig_dependency"] for row in inventory.values()):
                raise RuntimeError("All six legacy package imports must be observed before this migration")
            report["backup_directory"] = str(local_backup(output, inventory))
            write_report(report_path, report)

        assets = load_skeletons()
        state = {package: skeleton_state(asset) for package, asset in assets.items()}
        report["loaded_skeletons"] = state
        if baseline is not None and state != baseline.get("loaded_skeletons"):
            raise RuntimeError("Loaded Skeleton path, bone or preview metadata differs from the probe")

        if args.mode == "apply":
            for package, asset in assets.items():
                # Modern serialization drops the removed RigConfig property. Do
                # not mutate bones, assign a replacement rig or save dependencies.
                if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                    raise RuntimeError(f"Editor could not save {package}")
                report["saved_packages"].append(package)
                write_report(report_path, report)
            report["disk_after"] = {
                package: {"sha256": sha256(ROOT / package_path(package)),
                          "bytes": (ROOT / package_path(package)).stat().st_size}
                for package in sorted(SKELETONS)
            }
            if any(report["disk_after"][package]["sha256"] == inventory[package]["sha256"] for package in SKELETONS):
                raise RuntimeError("At least one Skeleton did not serialize new bytes")

        if args.mode == "verify":
            for package, row in inventory.items():
                old = baseline["disk_before"][package]
                if not old["legacy_rig_dependency"] or row["legacy_rig_dependency"]:
                    raise RuntimeError(f"Legacy dependency did not clear: {package}")
                if row["sha256"] == old["sha256"]:
                    raise RuntimeError(f"Package bytes are still the legacy input: {package}")
                unexpected_removed = set(old["dependencies"]) - set(row["dependencies"]) - {LEGACY_RIG}
                if unexpected_removed:
                    raise RuntimeError(f"Non-legacy dependencies disappeared from {package}: {sorted(unexpected_removed)}")

        report["success"] = True
        unreal.log(f"ROGUE_CHARACTER_SKELETON_REPAIR {args.mode.upper()} PASS skeletons=6 saved={len(report['saved_packages'])}")
    except Exception:
        report["error"] = traceback.format_exc()
        raise
    finally:
        write_report(report_path, report)


if __name__ == "__main__":
    main()
