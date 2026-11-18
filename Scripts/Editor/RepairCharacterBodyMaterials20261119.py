"""Restore authored character body contrast and replace exact debug material slots.

Run in the project Unreal Editor Python commandlet after the skeleton repair.
The authoring pass backs up the existing body, owner arms and catalog packages,
then saves only those packages and the explicitly named project material copies.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil

import unreal


ROOT = Path(__file__).resolve().parents[2]
REPORT_ROOT = ROOT / "tmp/CharacterAssetRepair20261119"
STYLE_ROOT = "/Game/StylizedCharacter"
OUTPUT_ROOT = "/Game/Character/Customization/BodyMaterials"
CATALOG = "/Game/DataAsset/Character/Customization/DA_CharacterCustomizationCatalog"
DEBUG = "/Engine/ArtTools/RenderToTexture/Materials/Debug/M_2sidedLit.M_2sidedLit"
ARMS_ROOT = "/Game/Rogue10m/Character/OwnerArms"
SOURCE_TAG = "Rogue10m.CharacterBodySource"
RECIPE_TAG = "Rogue10m.CharacterBodyRecipe"
RECIPE = "AuthoredAlbedoGrade20261119"
RACES = (("Hu", "Human"), ("Dw", "Dwarf"), ("Or", "Orc"))
GENDERS = (("M", "Male"), ("F", "Female"))
VARIANTS = tuple((f"{code}_{sex}", code, race, gender)
                 for code, race in RACES for sex, gender in GENDERS)
# Linear dark and light fabric colors. The authored luminance and colored
# details still drive the result, including the detail on Orc tattoo variants.
PALETTES = {
    "Hu": ((0.024, 0.034, 0.043), (0.25, 0.38, 0.46)),
    "Dw": ((0.039, 0.018, 0.011), (0.46, 0.235, 0.103)),
    "Or": ((0.014, 0.030, 0.017), (0.244, 0.38, 0.165)),
}
DWARF_COMPLEXION = ((0.045, 0.022, 0.016), (0.56, 0.33, 0.21))
ASSETS = unreal.EditorAssetLibrary
MATERIALS = unreal.MaterialEditingLibrary


def path(asset):
    return asset.get_path_name() if asset else None


def package(asset):
    return path(asset).split(".", 1)[0]


def load(asset_path, asset_class):
    asset = ASSETS.load_asset(asset_path)
    if not isinstance(asset, asset_class):
        raise RuntimeError(f"Missing or wrong asset class: {asset_path}")
    return asset


def disk_path(package_path):
    if not package_path.startswith("/Game/"):
        raise RuntimeError(f"Non-project package cannot be backed up: {package_path}")
    return ROOT / "Content" / (package_path.removeprefix("/Game/") + ".uasset")


def backup_inputs(assets, output):
    backup = output / "originals"
    records = []
    for asset in assets:
        source = disk_path(package(asset))
        target = backup / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        sha = hashlib.sha256(source.read_bytes()).hexdigest()
        if hashlib.sha256(target.read_bytes()).hexdigest() != sha:
            raise RuntimeError(f"Backup did not match source: {source}")
        records.append({"package": package(asset), "sha256": sha,
                        "archive": str(target.relative_to(output))})
    return records


def duplicate(source, destination, asset_class):
    if ASSETS.does_asset_exist(destination):
        asset = load(destination, asset_class)
        if (ASSETS.get_metadata_tag(asset, SOURCE_TAG) != path(source)
                or ASSETS.get_metadata_tag(asset, RECIPE_TAG) != RECIPE):
            raise RuntimeError(f"Refusing to replace an unrelated asset: {destination}")
        return asset, False
    asset = ASSETS.duplicate_asset(package(source), destination)
    if not isinstance(asset, asset_class) or package(asset) != destination:
        raise RuntimeError(f"Could not duplicate {path(source)} to {destination}")
    ASSETS.set_metadata_tag(asset, SOURCE_TAG, path(source))
    ASSETS.set_metadata_tag(asset, RECIPE_TAG, RECIPE)
    return asset, True


def node(material, expression, x, y):
    value = MATERIALS.create_material_expression(material, expression, x, y)
    if not value:
        raise RuntimeError(f"Could not create material expression: {expression}")
    return value


def link(source, output, target, input_name):
    if not MATERIALS.connect_material_expressions(source, output, target, input_name):
        pins = list(MATERIALS.get_material_expression_input_names(target))
        raise RuntimeError(f"Material connection {source.get_class().get_name()}:{output} -> "
                           f"{target.get_class().get_name()}:{input_name} failed, pins={pins}")


def constant_color(material, color, x, y):
    value = node(material, unreal.MaterialExpressionConstant3Vector, x, y)
    value.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    return value


def scalar_output(material, number, prop, x, y):
    value = node(material, unreal.MaterialExpressionConstant, x, y)
    value.set_editor_property("r", number)
    if not MATERIALS.connect_material_property(value, "", prop):
        raise RuntimeError(f"Failed to connect {prop}")


def compile_material(material):
    if not material.get_editor_property("used_with_skeletal_mesh"):
        raise RuntimeError(f"Skeletal mesh usage is disabled: {path(material)}")
    errors = list(MATERIALS.recompile_material(material))
    if errors:
        raise RuntimeError(f"Failed to compile {path(material)}: {errors}")


def input_named(expression, canonical):
    for name in MATERIALS.get_material_expression_input_names(expression):
        if str(name).replace(" ", "").casefold() == canonical.casefold():
            return str(name)
    raise RuntimeError(f"Missing input {canonical} on {expression.get_class().get_name()}")


def find_albedo(material):
    base = unreal.MaterialProperty.MP_BASE_COLOR
    attributes = unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES
    if not material.get_editor_property("use_material_attributes"):
        source = MATERIALS.get_material_property_input_node(material, base)
        if not source:
            raise RuntimeError(f"No authored Base Color in {path(material)}")
        return source, MATERIALS.get_material_property_input_node_output_name(material, base), base, None

    source = MATERIALS.get_material_property_input_node(material, attributes)
    if not source:
        raise RuntimeError(f"No authored Material Attributes in {path(material)}")
    output = MATERIALS.get_material_property_input_node_output_name(material, attributes)
    base_guid, valid = unreal.GuidLibrary.parse_string_to_guid("69B8D33616ED4D499AA497292F050F7A")
    if not valid:
        raise RuntimeError("Cannot resolve UE BaseColor material attribute")
    get_attributes = node(material, unreal.MaterialExpressionGetMaterialAttributes, 500, -500)
    set_attributes = node(material, unreal.MaterialExpressionSetMaterialAttributes, 1600, -250)
    get_attributes.set_editor_property("attribute_get_types", [base_guid])
    set_attributes.set_editor_property("attribute_set_types", [base_guid])
    link(source, output, get_attributes, input_named(get_attributes, "MaterialAttributes"))
    link(source, output, set_attributes, input_named(set_attributes, "MaterialAttributes"))
    return get_attributes, input_named(set_attributes, "BaseColor"), attributes, set_attributes


def add_color_grade(material, palette):
    source, output, prop, attributes = find_albedo(material)
    weights = constant_color(material, (0.2126, 0.7152, 0.0722), 490, -320)
    luma = node(material, unreal.MaterialExpressionDotProduct, 695, -500)
    link(source, output, luma, "A")
    link(weights, "", luma, "B")
    curve = node(material, unreal.MaterialExpressionSmoothStep, 890, -500)
    curve.set_editor_property("const_min", 0.010)
    curve.set_editor_property("const_max", 0.24)
    link(luma, "", curve, "Value")
    shade, highlight = palette
    dark = constant_color(material, shade, 695, -300)
    bright = constant_color(material, highlight, 695, -140)
    color = node(material, unreal.MaterialExpressionLinearInterpolate, 1050, -500)
    link(dark, "", color, "A")
    link(bright, "", color, "B")
    link(curve, "", color, "Alpha")
    chroma = node(material, unreal.MaterialExpressionSubtract, 1050, -260)
    link(source, output, chroma, "A")
    link(luma, "", chroma, "B")
    combine = node(material, unreal.MaterialExpressionAdd, 1230, -500)
    link(color, "", combine, "A")
    link(chroma, "", combine, "B")
    graded = node(material, unreal.MaterialExpressionSaturate, 1420, -500)
    link(combine, "", graded, "")
    if attributes:
        link(graded, "", attributes, input_named(attributes, "BaseColor"))
    if not MATERIALS.connect_material_property(attributes or graded, "", prop):
        raise RuntimeError(f"Cannot connect graded body albedo: {path(material)}")


def build_palette(race_code, group, palette):
    source = load(f"{STYLE_ROOT}/Materials/M_{race_code}_{group}_Master", unreal.Material)
    parent, created = duplicate(source, f"{OUTPUT_ROOT}/M_Rogue_{race_code}_{group}", unreal.Material)
    if created:
        parent.set_editor_property("used_with_skeletal_mesh", True)
        parent.set_editor_property("automatically_set_usage_in_editor", False)
        add_color_grade(parent, palette)
        compile_material(parent)
    elif not parent.get_editor_property("used_with_skeletal_mesh"):
        raise RuntimeError(f"Skeletal mesh usage is disabled: {path(parent)}")
    return parent


def effective_parameters(material):
    result = {}
    for kind in ("texture", "scalar", "vector", "static_switch"):
        read = getattr(MATERIALS, f"get_material_instance_{kind}_parameter_value")
        names = getattr(MATERIALS, f"get_{kind}_parameter_names")(material)
        values = {}
        for name in names:
            value = read(material, name)
            if kind == "texture":
                value = path(value)
            elif kind == "vector":
                value = (value.r, value.g, value.b, value.a)
            values[str(name)] = value
        result[kind] = values
    return result


def build_instance(source_path, parent):
    source = load(source_path, unreal.MaterialInstanceConstant)
    destination = f"{OUTPUT_ROOT}/{source.get_name()}_Rogue"
    parameters = effective_parameters(source)
    material, created = duplicate(source, destination, unreal.MaterialInstanceConstant)
    if created:
        MATERIALS.set_material_instance_parent(material, parent)
        MATERIALS.update_material_instance(material)
    if material.get_editor_property("parent") != parent or effective_parameters(material) != parameters:
        raise RuntimeError(f"Vendor parameter values changed in {destination}")
    return material


def build_accessory():
    destination = f"{OUTPUT_ROOT}/M_Rogue_FacialBrass"
    if ASSETS.does_asset_exist(destination):
        material = load(destination, unreal.Material)
        if ASSETS.get_metadata_tag(material, RECIPE_TAG) != RECIPE:
            raise RuntimeError(f"Refusing to replace {destination}")
        if not material.get_editor_property("used_with_skeletal_mesh"):
            raise RuntimeError(f"Skeletal mesh usage is disabled: {path(material)}")
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_Rogue_FacialBrass", OUTPUT_ROOT, unreal.Material, unreal.MaterialFactoryNew())
        if not isinstance(material, unreal.Material):
            raise RuntimeError("Could not create the facial accessory material")
        material.set_editor_property("used_with_skeletal_mesh", True)
        material.set_editor_property("automatically_set_usage_in_editor", False)
        color = constant_color(material, (0.23, 0.15, 0.071), -260, -100)
        if not MATERIALS.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError("Could not connect facial accessory Base Color")
        scalar_output(material, 0.61, unreal.MaterialProperty.MP_METALLIC, -260, 60)
        scalar_output(material, 0.44, unreal.MaterialProperty.MP_ROUGHNESS, -260, 200)
        ASSETS.set_metadata_tag(material, RECIPE_TAG, RECIPE)
        compile_material(material)
    return material


def instance_body_root(race, gender):
    return f"{STYLE_ROOT}/Materials/Instances/Character/{race}/{gender}/Body"


def material_plan(mesh, token, body, eye, brass, head):
    slots = list(mesh.get_editor_property("materials"))
    names = [str(slot.get_editor_property("material_slot_name")) for slot in slots]
    eyes = "M_Eye" if token == "Hu_M" else "M_Eyes"
    expected = {"M_Body", "M_Head", eyes}
    if token in ("Hu_F", "Or_F"):
        expected.add("M_Facials")
    if len(names) != len(expected) or set(names) != expected:
        raise RuntimeError(f"Unexpected material slots for {path(mesh)}: {names}")
    replacements = {"M_Body": body, eyes: eye, "M_Facials": brass}
    if head:
        replacements["M_Head"] = head
    summary = []
    for index, (slot, name) in enumerate(zip(slots, names)):
        original = slot.get_editor_property("material_interface")
        replacement = replacements.get(name, original)
        if name == "M_Body" or (name == "M_Head" and head):
            source = ASSETS.get_metadata_tag(replacement, SOURCE_TAG)
            if path(original) not in (source, path(replacement)):
                raise RuntimeError(f"Unexpected vendor material on {path(mesh)}:{name}: {path(original)}")
        if name in (eyes, "M_Facials") and path(original) not in (DEBUG, path(replacement)):
            raise RuntimeError(f"Unexpected material in repair slot {name} on {path(mesh)}: {path(original)}")
        if not replacement or path(replacement) == DEBUG:
            raise RuntimeError(f"Debug or missing material remains on {path(mesh)}:{name}")
        if replacement != original:
            slot.set_editor_property("material_interface", replacement)
        summary.append({"index": index, "slot": name, "before": path(original), "after": path(replacement)})
    return slots, summary


def apply_body_material_repair(output):
    meshes = {}
    for token, _, race, gender in VARIANTS:
        meshes[token] = (
            load(f"{STYLE_ROOT}/Meshes/Character/{race}/{gender}/SK_{token}_FullBody", unreal.SkeletalMesh),
            load(f"{ARMS_ROOT}/SK_OwnerArms_{token}", unreal.SkeletalMesh),
        )
    catalog = ASSETS.load_asset(CATALOG)
    if not isinstance(catalog, unreal.Rogue10mCharacterCustomizationDataAsset):
        raise RuntimeError("Customization catalog unavailable")
    existing_assets = [mesh for body_and_arms in meshes.values() for mesh in body_and_arms] + [catalog]
    report = {"backups": backup_inputs(existing_assets, output), "meshes": {}, "body_details": {}, "saved": []}
    report_file = output / "material-verification.json"

    def record():
        report_file.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    record()
    try:
        eyes = {}
        for code, race in RACES:
            set_name = "Orc" if race == "Orc" else "Human"
            eye_name = "MI_OR_Eye_BrBln" if race == "Orc" else "MI_HU_Eye_BrBln"
            eyes[code] = load(f"{STYLE_ROOT}/Materials/Instances/Character/{set_name}/Eye/{eye_name}",
                              unreal.MaterialInstanceConstant)
            eye_parent = eyes[code].get_editor_property("parent")
            if not eye_parent.get_editor_property("used_with_skeletal_mesh"):
                raise RuntimeError(f"Eye material is not usable on skeletal meshes: {path(eyes[code])}")
        palettes = {code: build_palette(code, "Body", PALETTES[code]) for code, _ in RACES}
        dwarf_face = build_palette("Dw", "Head", DWARF_COMPLEXION)
        brass = build_accessory()
        body_instances = {}
        face_instances = {}
        tattoo_instances = {}
        planned = []
        for token, code, race, gender in VARIANTS:
            source_root = instance_body_root(race, gender)
            body_instances[token] = build_instance(f"{source_root}/MI_{token}_Body_Preview", palettes[code])
            if code == "Dw":
                face_instances[token] = build_instance(
                    f"{STYLE_ROOT}/Materials/Instances/Character/Dwarf/{gender}/Head/MI_{token}_Head_Preview",
                    dwarf_face,
                )
            for mesh in meshes[token]:
                slots, summary = material_plan(mesh, token, body_instances[token], eyes[code], brass,
                                               face_instances.get(token))
                if any(row["before"] != row["after"] for row in summary):
                    planned.append((mesh, slots))
                report["meshes"][package(mesh)] = summary
            if code == "Or":
                tattoo_instances[token] = [build_instance(
                    f"{source_root}/Presets/MI_{token}_Body_Tattoo_{i:02d}", palettes[code]) for i in range(1, 5)]

        entries = list(catalog.get_editor_property("archetypes"))
        if len(entries) != len(VARIANTS):
            raise RuntimeError("Unexpected customization archetype count")
        processed = set()
        catalog_changed = False
        for entry in entries:
            entry_mesh = entry.get_editor_property("body_mesh")
            token = next((name for name, pair in meshes.items() if pair[0] == entry_mesh), None)
            if token is None or token in processed:
                raise RuntimeError(f"Unexpected or duplicate catalog body: {path(entry_mesh)}")
            processed.add(token)
            options = list(entry.get_editor_property("body_detail_materials"))
            if token not in tattoo_instances:
                if options:
                    raise RuntimeError(f"Unexpected body detail options on {token}")
                continue
            derivatives = tattoo_instances[token]
            allowed = [(ASSETS.get_metadata_tag(derivative, SOURCE_TAG), path(derivative)) for derivative in derivatives]
            if len(options) != len(allowed) or any(path(option) not in pair for option, pair in zip(options, allowed)):
                raise RuntimeError(f"Refusing to change tattoo order or overwrite custom materials on {token}")
            report["body_details"][token] = {"before": [path(option) for option in options],
                                              "after": [path(item) for item in derivatives]}
            if options != derivatives:
                entry.set_editor_property("body_detail_materials", derivatives)
                catalog_changed = True

        for mesh, slots in planned:
            mesh.modify()
            mesh.set_editor_property("materials", slots)
        if catalog_changed:
            catalog.modify()
            catalog.set_editor_property("archetypes", entries)
        all_instances = [*body_instances.values(), *face_instances.values(),
                         *(item for items in tattoo_instances.values() for item in items)]
        authored = [*palettes.values(), dwarf_face, brass, *all_instances, *(mesh for mesh, _ in planned)]
        if catalog_changed:
            authored.append(catalog)
        dirty = {item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        for asset in authored:
            if package(asset) not in dirty:
                continue
            if not ASSETS.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError(f"Package save failed: {path(asset)}")
            report["saved"].append(package(asset))
            record()
        report["success"] = True
    except Exception as error:
        report["success"] = False
        report["error"] = str(error)
        raise
    finally:
        record()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=REPORT_ROOT / "MaterialsA")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if not output.is_relative_to(REPORT_ROOT.resolve()):
        raise RuntimeError("Material repair output must be inside this project's repair folder")
    output.mkdir(parents=True, exist_ok=False)
    apply_body_material_repair(output)
    unreal.log("ROGUE_CHARACTER_BODY_MATERIAL_REPAIR_PASSED")


if __name__ == "__main__":
    main()
