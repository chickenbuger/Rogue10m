"""Build the character skin materials from the existing body and head atlases.

Run this file in the Unreal Editor Python commandlet. The calibration input
records source atlas luminance at the mesh neck, plus the head UV masks.
Only the listed character packages and project-owned derivatives are saved.
"""

import argparse
import hashlib
import json
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from RepairCharacterBodyMaterials20261119 import (
    ARMS_ROOT, ASSETS, CATALOG, MATERIALS, OUTPUT_ROOT, STYLE_ROOT, VARIANTS,
    backup_inputs, compile_material, disk_path, effective_parameters, input_named, link, load, node, package, path,
)


ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "Feature/doc/evidence/character-skin-color-20261119"
OUTPUT = "/Game/Character/Customization/SkinTone"
REPORT_ROOT = ROOT / "tmp/CharacterSkinTone20261119"
SOURCE_TAG = "Rogue10m.SkinSource"
RECIPE_TAG = "Rogue10m.SkinRecipe"
RECIPE = "CalibratedSkinAlbedo20261119"
SKIN_COLOR = "RogueSkinColor"
SKIN_REFERENCE = "RogueSkinReference"
SKIN_MASK = "RogueSkinMask"
SKIN_ATTRIBUTES = {"Body": "MaterialExpressionMakeMaterialAttributes_1", "Head": "MaterialExpressionMakeMaterialAttributes_9"}
DEFAULT_SRGB = {"Hu": "C89070", "Dw": "AE7551", "Or": "74924C"}


def color_from_hex(value):
    srgb = [int(value[offset:offset + 2], 16) / 255.0 for offset in (0, 2, 4)]
    rgb = [channel / 12.92 if channel <= 0.04045 else ((channel + 0.055) / 1.055)**2.4 for channel in srgb]
    return unreal.LinearColor(*rgb, 1.0)


def duplicate(source, destination, asset_class):
    if ASSETS.does_asset_exist(destination):
        existing = load(destination, asset_class)
        if ASSETS.get_metadata_tag(existing, SOURCE_TAG) != path(source):
            raise RuntimeError(f"Unexpected source for {destination}")
        if ASSETS.get_metadata_tag(existing, RECIPE_TAG) != RECIPE:
            raise RuntimeError(f"Unexpected recipe for {destination}")
        return existing, False
    derivative = ASSETS.duplicate_asset(package(source), destination)
    if not isinstance(derivative, asset_class) or package(derivative) != destination:
        raise RuntimeError(f"Could not derive {destination} from {path(source)}")
    ASSETS.set_metadata_tag(derivative, SOURCE_TAG, path(source))
    ASSETS.set_metadata_tag(derivative, RECIPE_TAG, RECIPE)
    return derivative, True


def wire_skin_layer(material, group, race, default_mask=None):
    skin = unreal.find_object(material, SKIN_ATTRIBUTES[group])
    if not isinstance(skin, unreal.MaterialExpressionMakeMaterialAttributes):
        raise RuntimeError(f"Missing checked skin input in {path(material)}")
    parameter = node(material, unreal.MaterialExpressionVectorParameter, -2220, -950)
    parameter.set_editor_property("parameter_name", SKIN_COLOR)
    parameter.set_editor_property("default_value", color_from_hex(DEFAULT_SRGB[race]))
    parameter.set_editor_property("group", "Skin")

    coefficients = node(material, unreal.MaterialExpressionConstant3Vector, -2880, -610)
    coefficients.set_editor_property("constant", unreal.LinearColor(0.2126, 0.7152, 0.0722, 1.0))
    luminance = node(material, unreal.MaterialExpressionDotProduct, -2650, -800)
    # Preserve the source branch and its alpha derivatives. The new branch feeds
    # only the skin layer's Base Color, before the vendor overlay blends.
    sample = unreal.find_object(material, "MaterialExpressionTextureSampleParameter2D_1"
                                if group == "Body" else "MaterialExpressionTextureSampleParameter2D_36")
    if (not isinstance(sample, unreal.MaterialExpressionTextureSampleParameter2D)
            or str(sample.get_editor_property("parameter_name")) != f"{group}_D"):
        raise RuntimeError(f"Unexpected albedo sample in {path(material)}")
    link(sample, "RGB", luminance, "A")
    link(coefficients, "", luminance, "B")
    gamma = node(material, unreal.MaterialExpressionSquareRoot, -2480, -800)
    link(luminance, "", gamma, "")
    neutral = node(material, unreal.MaterialExpressionScalarParameter, -2480, -630)
    neutral.set_editor_property("parameter_name", SKIN_REFERENCE)
    neutral.set_editor_property("default_value", 0.30)
    neutral.set_editor_property("group", "Skin")
    contrast = node(material, unreal.MaterialExpressionSubtract, -2320, -800)
    link(gamma, "", contrast, "A")
    link(neutral, "", contrast, "B")
    detail = node(material, unreal.MaterialExpressionMultiply, -2150, -800)
    detail.set_editor_property("const_b", 1.25)
    link(contrast, "", detail, "A")
    bounded = node(material, unreal.MaterialExpressionClamp, -1960, -800)
    bounded.set_editor_property("min_default", -0.50)
    bounded.set_editor_property("max_default", 0.45)
    link(detail, "", bounded, "")
    shade = node(material, unreal.MaterialExpressionAdd, -1790, -800)
    shade.set_editor_property("const_b", 1.0)
    link(bounded, "", shade, "A")
    tinted = node(material, unreal.MaterialExpressionMultiply, -1620, -925)
    link(parameter, "", tinted, "A")
    link(shade, "", tinted, "B")
    output = tinted
    if group == "Head":
        # The visible cranial skin is one atlas shell. The imported UV mask
        # keeps the inner mouth, teeth and other detached shells as authored.
        mask = node(material, unreal.MaterialExpressionTextureSampleParameter2D, -1820, -570)
        mask.set_editor_property("parameter_name", SKIN_MASK)
        mask.set_editor_property("group", "Skin")
        mask.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        mask.set_editor_property("texture", default_mask)
        masked_color = node(material, unreal.MaterialExpressionLinearInterpolate, -1420, -775)
        link(sample, "RGB", masked_color, "A")
        link(tinted, "", masked_color, "B")
        link(mask, "R", masked_color, "Alpha")
        output = masked_color
    link(output, "", skin, input_named(skin, "BaseColor"))


def build_masters(masks):
    result = {}
    for token in sorted(DEFAULT_SRGB):
        for group in ("Body", "Head"):
            vendor = load(f"{STYLE_ROOT}/Materials/M_{token}_{group}_Master", unreal.Material)
            destination = f"{OUTPUT}/M_RogueSkin_{token}_{group}"
            master, created = duplicate(vendor, destination, unreal.Material)
            if created:
                master.set_editor_property("used_with_skeletal_mesh", True)
                master.set_editor_property("automatically_set_usage_in_editor", False)
                wire_skin_layer(master, group, token, masks[f"{token}_M"])
                compile_material(master)
            if SKIN_COLOR not in list(map(str, MATERIALS.get_vector_parameter_names(master))):
                raise RuntimeError(f"Missing tint parameter in {destination}")
            result[token, group] = master
    return result


def import_masks(calibration):
    masks = {}
    for token, data in calibration["archetypes"].items():
        file = EVIDENCE / data["mask_file"]
        if hashlib.sha256(file.read_bytes()).hexdigest() != data["mask_sha256"]:
            raise RuntimeError(f"Changed UV mask source for {token}")
        name = f"T_RogueSkin_{token}_Mask"
        destination = f"{OUTPUT}/Masks"
        output_path = f"{destination}/{name}"
        if ASSETS.does_asset_exist(output_path):
            mask = load(output_path, unreal.Texture2D)
            if ASSETS.get_metadata_tag(mask, SOURCE_TAG) != data["mask_sha256"]:
                raise RuntimeError(f"Unexpected source texture at {path(mask)}")
        else:
            task = unreal.AssetImportTask()
            task.filename = str(file)
            task.destination_path = destination
            task.destination_name = name
            task.automated = True
            task.save = False
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            mask = load(output_path, unreal.Texture2D)
            mask.set_editor_property("srgb", False)
            mask.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            mask.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
            mask.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
            ASSETS.set_metadata_tag(mask, SOURCE_TAG, data["mask_sha256"])
            ASSETS.set_metadata_tag(mask, RECIPE_TAG, RECIPE)
        masks[token] = mask
    return masks


def atlas_parameter(instance, group):
    atlas = MATERIALS.get_material_instance_texture_parameter_value(instance, f"{group}_D")
    if not atlas:
        raise RuntimeError(f"No {group} atlas in {path(instance)}")
    return atlas.get_name()


def set_skin_instance(instance, master, group, calibration, mask=None):
    before = effective_parameters(instance)
    if instance.get_editor_property("parent") != master:
        MATERIALS.set_material_instance_parent(instance, master)
    value = calibration["atlas_references"][atlas_parameter(instance, group)]
    current = MATERIALS.get_material_instance_scalar_parameter_value(instance, SKIN_REFERENCE)
    if abs(current - value) > 0.000001:
        MATERIALS.set_material_instance_scalar_parameter_value(instance, SKIN_REFERENCE, value)
    if mask and MATERIALS.get_material_instance_texture_parameter_value(instance, SKIN_MASK) != mask:
        MATERIALS.set_material_instance_texture_parameter_value(instance, SKIN_MASK, mask)
    MATERIALS.update_material_instance(instance)
    after = effective_parameters(instance)
    for kind, previous in before.items():
        kept = {name: val for name, val in previous.items() if name not in (SKIN_COLOR, SKIN_REFERENCE, SKIN_MASK)}
        if any(after[kind].get(name) != val for name, val in kept.items()):
            raise RuntimeError(f"Existing {kind} parameter values changed in {path(instance)}")
    if SKIN_COLOR not in after["vector"] or abs(after["scalar"][SKIN_REFERENCE] - value) > 0.000001:
        raise RuntimeError(f"Incomplete skin tint parameters in {path(instance)}")
    if mask and after["texture"][SKIN_MASK] != path(mask):
        raise RuntimeError(f"Incorrect skin UV mask in {path(instance)}")
    return value


def set_head_slot(mesh, head):
    slots = list(mesh.get_editor_property("materials"))
    head_slot = next(slot for slot in slots if str(slot.get_editor_property("material_slot_name")) == "M_Head")
    existing = head_slot.get_editor_property("material_interface")
    if existing != head:
        # Only the default head slot changes, retaining mesh sections, LODs,
        # skeleton, animation associations, and original non-skin slots.
        mesh.modify()
        head_slot.set_editor_property("material_interface", head)
        mesh.set_editor_property("materials", slots)
    return path(existing)


def head_instance(source, master, calibration, mask, variant):
    if not package(source).startswith(OUTPUT + "/"):
        name = source.get_name() + "_Skin"
        directory = f"{OUTPUT}/Heads/Variants" if variant else f"{OUTPUT}/Heads"
        derived, _ = duplicate(source, f"{directory}/{name}", unreal.MaterialInstanceConstant)
    else:
        derived = source
    return derived, set_skin_instance(derived, master, "Head", calibration, mask)


def apply(calibration, output):
    catalog = load(CATALOG, unreal.Rogue10mCharacterCustomizationDataAsset)
    entries = list(catalog.get_editor_property("archetypes"))
    archetypes = {}
    tracked = [catalog]
    for token, code, race, gender in VARIANTS:
        body = load(f"{STYLE_ROOT}/Meshes/Character/{race}/{gender}/SK_{token}_FullBody", unreal.SkeletalMesh)
        arms = load(f"{ARMS_ROOT}/SK_OwnerArms_{token}", unreal.SkeletalMesh)
        entry = next(candidate for candidate in entries if candidate.get_editor_property("body_mesh") == body)
        default_body = load(f"{OUTPUT_ROOT}/MI_{token}_Body_Preview_Rogue", unreal.MaterialInstanceConstant)
        tattoos = list(entry.get_editor_property("body_detail_materials"))
        if len(tattoos) != (4 if code == "Or" else 0) or any(
                not package(tattoo).startswith(OUTPUT_ROOT + "/") for tattoo in tattoos):
            raise RuntimeError(f"Unexpected body detail catalog in {token}")
        archetypes[token] = body, arms, entry, default_body, tattoos
        tracked.extend((body, arms, default_body, *tattoos))
    report = {"backups": backup_inputs(tracked, output), "saved": [], "archetypes": {}}
    receipt = output / "materials.json"

    def record():
        receipt.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    record()
    try:
        masks = import_masks(calibration)
        masters = build_masters(masks)
        save_order = [*masks.values(), *masters.values()]
        for token, code, race, gender in VARIANTS:
            body, arms, entry, default_body, tattoos = archetypes[token]
            assets = {}
            for material in (default_body, *tattoos):
                reference = set_skin_instance(material, masters[code, "Body"], "Body", calibration)
                assets[package(material)] = {"reference": reference, "atlas": atlas_parameter(material, "Body")}
                save_order.append(material)
            head_vendor = load(f"{STYLE_ROOT}/Materials/Instances/Character/{race}/{gender}/Head/"
                               f"MI_{token}_Head_Preview", unreal.MaterialInstanceConstant)
            head, reference = head_instance(head_vendor, masters[code, "Head"], calibration, masks[token], False)
            assets[package(head)] = {"reference": reference, "atlas": atlas_parameter(head, "Head")}
            existing = {package(mesh): set_head_slot(mesh, head) for mesh in (body, arms)}
            save_order.extend((head, body, arms))
            options = list(entry.get_editor_property("head_materials"))
            expected_count = {"Hu": 20, "Dw": 0, "Or": 15}[code]
            if len(options) != expected_count:
                raise RuntimeError(f"Unexpected head options on {token}")
            replacements = []
            for option in options:
                variant, reference = head_instance(option, masters[code, "Head"], calibration, masks[token], True)
                assets[package(variant)] = {"reference": reference, "atlas": atlas_parameter(variant, "Head")}
                replacements.append(variant)
            if options != replacements:
                entry.set_editor_property("head_materials", replacements)
            save_order.extend(replacements)
            report["archetypes"][token] = {
                "prior_head_slots": existing, "head_default": package(head), "body_default": package(default_body),
                "tint_srgb": DEFAULT_SRGB[code], "head_options": list(map(package, replacements)),
                "body_details": list(map(package, tattoos)), "material_parameters": assets,
            }
        catalog.modify()
        catalog.set_editor_property("archetypes", entries)
        save_order.append(catalog)
        dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        for asset in dict.fromkeys(save_order):
            package_name = package(asset)
            if package_name not in dirty:
                continue
            if not ASSETS.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError(f"Package save failed: {path(asset)}")
            report["saved"].append({"package": package_name, "sha256": hashlib.sha256(
                disk_path(package_name).read_bytes()).hexdigest()})
            record()
        report["success"] = True
    except Exception as error:
        report["success"] = False
        report["error"] = str(error)
        raise
    finally:
        record()


def verify(calibration, output):
    catalog = load(CATALOG, unreal.Rogue10mCharacterCustomizationDataAsset)
    entries = list(catalog.get_editor_property("archetypes"))
    report = {}
    for token, code, race, gender in VARIANTS:
        body = load(f"{STYLE_ROOT}/Meshes/Character/{race}/{gender}/SK_{token}_FullBody", unreal.SkeletalMesh)
        arms = load(f"{ARMS_ROOT}/SK_OwnerArms_{token}", unreal.SkeletalMesh)
        entry = next(candidate for candidate in entries if candidate.get_editor_property("body_mesh") == body)
        slot_maps = []
        for mesh in (body, arms):
            slot_maps.append({
                str(slot.get_editor_property("material_slot_name")): slot.get_editor_property("material_interface")
                for slot in mesh.get_editor_property("materials")
            })
        if slot_maps[0] != slot_maps[1]:
            raise RuntimeError(f"Body/owner arms slot maps differ for {token}")
        slots = slot_maps[0]
        head_options = [slots["M_Head"], *entry.get_editor_property("head_materials")]
        body_options = [slots["M_Body"], *entry.get_editor_property("body_detail_materials")]
        expected = calibration["archetypes"][token]
        if ([atlas_parameter(instance, "Head") for instance in head_options] != expected["head_atlases"]
                or any(atlas_parameter(instance, "Body") != expected["body_atlas"] for instance in body_options)):
            raise RuntimeError(f"Atlas identity or catalog order changed for {token}")
        default_color = color_from_hex(DEFAULT_SRGB[code])
        for group, materials in (("Head", head_options), ("Body", body_options)):
            for instance in materials:
                atlas = atlas_parameter(instance, group)
                params = effective_parameters(instance)
                master = instance.get_editor_property("parent")
                if (package(master) != f"{OUTPUT}/M_RogueSkin_{code}_{group}"
                        or not master.get_editor_property("used_with_skeletal_mesh")):
                    raise RuntimeError(f"Unexpected parent on {path(instance)}")
                if (any(abs(a - b) > 0.000003 for a, b in zip(params["vector"][SKIN_COLOR],
                            (default_color.r, default_color.g, default_color.b, default_color.a)))
                        or abs(params["scalar"][SKIN_REFERENCE] - calibration["atlas_references"][atlas]) > 0.000003):
                    raise RuntimeError(f"Color or neutral reference not calibrated in {path(instance)}")
                if group == "Head":
                    mask = MATERIALS.get_material_instance_texture_parameter_value(instance, SKIN_MASK)
                    if (not mask or ASSETS.get_metadata_tag(mask, SOURCE_TAG) != expected["mask_sha256"]
                            or mask.get_editor_property("srgb")
                            or mask.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_MASKS):
                        raise RuntimeError(f"Incorrect UV mask in {path(instance)}")
        report[token] = {
            "tint_srgb": DEFAULT_SRGB[code],
            "head_variants_with_calibrated_skin": len(head_options),
            "body_variants_with_calibrated_skin": len(body_options),
            "body_and_owner_arms_slot_maps_match": True,
        }
    (output / "material-cold-load.json").write_text(
        json.dumps({"success": True, "archetypes": report}, indent=2) + "\n", encoding="utf-8"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if not output.is_relative_to(REPORT_ROOT.resolve()):
        raise RuntimeError("Output must be inside this project's skin material verification folder")
    output.mkdir(parents=True, exist_ok=False)
    calibration = json.loads((EVIDENCE / "skin-calibration.json").read_text(encoding="utf-8"))
    if args.verify:
        verify(calibration, output)
        unreal.log("ROGUE_SKIN_MATERIAL_VALIDATION_PASSED")
    else:
        apply(calibration, output)
        unreal.log("ROGUE_SKIN_MATERIAL_APPLICATION_PASSED")


if __name__ == "__main__":
    main()
