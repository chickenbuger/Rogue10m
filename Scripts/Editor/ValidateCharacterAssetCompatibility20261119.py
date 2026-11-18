"""Validate the saved lobby appearance assets from a fresh UE 5.8 editor process."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import CreateInheritedCharacterAssets as variants
import RepairLegacyCharacterSkeletons20261119 as skeletons
import ValidateInheritedCharacterAssets as inherited


def materials_for(mesh):
    materials = {}
    for slot in mesh.get_editor_property("materials"):
        name = str(slot.get_editor_property("material_slot_name"))
        interface = slot.get_editor_property("material_interface")
        if not interface or "M_2sidedLit" in interface.get_path_name():
            raise RuntimeError(f"Unusable material in {mesh.get_path_name()} slot {name}")
        master = interface
        while isinstance(master, unreal.MaterialInstanceConstant):
            master = master.get_editor_property("parent")
        if not isinstance(master, unreal.Material) or not master.get_editor_property("used_with_skeletal_mesh"):
            raise RuntimeError(f"Skeletal Mesh usage missing in {mesh.get_path_name()} slot {name}")
        materials[name] = interface.get_path_name()
    return materials


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    output = args.report.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise RuntimeError(f"Validation already exists: {output}")

    inherited.main()
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([
        "/Game/StylizedCharacter/Meshes/Character", "/Game/Rogue10m/Character/OwnerArms"
    ], force_rescan=True)
    options = skeletons.dependency_options()
    report = {"archetypes": [], "success": False}
    for variant in variants.ARCHETYPES:
        code = variant["code"]
        body = variants.require_asset(variant["body"])
        arms = variants.require_asset(f"/Game/Rogue10m/Character/OwnerArms/SK_OwnerArms_{code}")
        skeleton = body.get_editor_property("skeleton")
        package = skeleton.get_path_name().split(".", 1)[0]
        deps = sorted(str(x) for x in registry.get_dependencies(package, options) or [])
        if package not in skeletons.SKELETONS or skeletons.LEGACY_RIG in deps:
            raise RuntimeError(f"Obsolete or unexpected skeleton for {code}: {package}, {deps}")
        if arms.get_editor_property("skeleton") != skeleton:
            raise RuntimeError(f"Body and first-person arms skeleton mismatch for {code}")
        body_slots = materials_for(body)
        if body_slots != materials_for(arms) or not body_slots.get("M_Body", "").startswith(
                "/Game/Character/Customization/BodyMaterials/"):
            raise RuntimeError(f"Body and first-person arms material mismatch for {code}")
        report["archetypes"].append({"code": code, "skeleton": package, "dependencies": deps,
                                     "body_and_arms_materials": body_slots})

    catalog = variants.require_asset(variants.CATALOG_PATH)
    tattoos = [material.get_path_name() for entry in catalog.get_editor_property("archetypes")
               for material in entry.get_editor_property("body_detail_materials")]
    if len(set(tattoos)) != 8 or any(path.startswith("/Game/StylizedCharacter/") for path in tattoos):
        raise RuntimeError(f"Expected all eight leveled tattoo material instances: {tattoos}")
    report["tattoos"] = tattoos
    report["success"] = True
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("CHARACTER_ASSET_COMPATIBILITY_PASS archetypes=6 tattoo_options=8")


if __name__ == "__main__":
    main()
