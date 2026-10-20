"""Import the authored Arin FBX and apply only the existing guide's appearance.

Run inside the idle UE 5.8 StartHub editor with ``main()``. Use
``main(validate_only=True)`` after a map/editor reload to verify persistence.
No assets outside the Blender scope, existing guide assets and guide-owned
StartHub package are saved. Partial imports and changed FBX caches abort.
"""

import ast
import hashlib
import json
import math
from pathlib import Path
import re

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_FBX = PROJECT / "Art/NPC/Arin/Arin.fbx"
BUILD_SCRIPT = PROJECT / "Scripts/Blender/BuildArin.py"
EVIDENCE = PROJECT / "Feature/doc/evidence/arin-blender-20261007"
ROOT = "/Game/Rogue10m/Characters/Arin/Blender"
MATERIAL_ROOT = ROOT + "/Materials"
BASE_MATERIAL_PATH = MATERIAL_ROOT + "/M_Arin_Base"
PARENT_SOURCE_PATH = "/Game/Stylized_Village/Assets/Foliage/Grass/_Materials/M_BaseColorUtility"
MESH_PATH = ROOT + "/SK_Arin"
SKELETON_PATH = ROOT + "/SK_Arin_Skeleton"
ANIMATION_PATH = ROOT + "/ArinIdle"
GUIDE_ROOT = "/Game/Rogue10m/World/StartVillage/Tutorial"
DATA_PATH = GUIDE_ROOT + "/DA_StartVillageGuide"
BLUEPRINT_PATH = GUIDE_ROOT + "/BP_StartVillageGuide"
MAP_PATH = "/Game/Rogue10m/Maps/StartHub"
GUIDE_LABEL = "SV_TutorialGuide_Arin"
FBX_TAG = "Arin.BlenderFBXSHA256"
PALETTE_TAG = "Arin.BlenderPaletteSHA256"
CHECKPOINT = EVIDENCE / "before-guide-application.json"
PROTECTED_DATA_PROPERTIES = (
    "guide_name", "guide_role", "welcome_text", "reward_received_text",
    "reward_already_received_text", "reward_id", "starting_item",
    "starting_item_quantity",
)
MATERIAL_KEYS = {
    "skin", "hair", "hair_light", "eye_white", "iris", "pupil", "lip",
    "cloth_ivory", "cloth_green", "cloth_green_dark", "leather",
    "leather_dark", "metal_gold", "potion_red", "glass",
}


def _path(value):
    return value.get_path_name() if value is not None else None


def _package_path(value):
    return value.get_path_name().split(".")[0]


def _json(path, value):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def _require(path, cls=None):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if value is None or (cls is not None and not isinstance(value, cls)):
        raise RuntimeError("Missing or wrong asset class: " + path)
    return value


def _palette():
    # Read the actual Blender palette as literals, without importing bpy code.
    tree = ast.parse(BUILD_SCRIPT.read_text(encoding="utf-8"))
    for function in tree.body:
        if isinstance(function, ast.FunctionDef) and function.name == "materials":
            for statement in function.body:
                if isinstance(statement, ast.Assign) and any(
                    isinstance(target, ast.Name) and target.id == "palette" for target in statement.targets
                ):
                    result = ast.literal_eval(statement.value)
                    if set(result) != MATERIAL_KEYS or len(result) != 15:
                        raise RuntimeError("Blender palette no longer matches the 15-material import contract")
                    return result
    raise RuntimeError("Could not read the Blender material palette")


def _hashes(palette):
    if not SOURCE_FBX.is_file():
        raise RuntimeError("Authored Blender FBX does not exist: " + str(SOURCE_FBX))
    return {
        FBX_TAG: hashlib.sha256(SOURCE_FBX.read_bytes()).hexdigest(),
        PALETTE_TAG: hashlib.sha256(json.dumps(palette, sort_keys=True).encode("utf-8")).hexdigest(),
    }


def _expected_paths(palette):
    return {MESH_PATH, SKELETON_PATH, ANIMATION_PATH, BASE_MATERIAL_PATH} | {
        MATERIAL_ROOT + "/M_Arin_" + key for key in palette
    }


def _scope_assets():
    paths = unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False)
    return {_package_path(_require(path)): _require(path) for path in paths}


def _asset_disk_path(path):
    if not path.startswith("/Game/"):
        raise RuntimeError("Asset is not a project package: " + path)
    return PROJECT / "Content" / (path[len("/Game/"):] + ".uasset")


def _save_asset(value):
    path = _package_path(value)
    if not (path.startswith(ROOT + "/") or path in {DATA_PATH, BLUEPRINT_PATH}):
        raise RuntimeError("Refusing to save an asset outside the Arin/guide scope: " + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(value, only_if_is_dirty=False):
        raise RuntimeError("Explicit asset save failed: " + path)
    if not _asset_disk_path(path).is_file():
        raise RuntimeError("Saved asset dependency is missing from disk: " + path)
    return path


def _set_provenance(value, hashes):
    for tag, digest in hashes.items():
        unreal.EditorAssetLibrary.set_metadata_tag(value, tag, digest)


def _verify_provenance(value, hashes):
    for tag, digest in hashes.items():
        if unreal.EditorAssetLibrary.get_metadata_tag(value, tag) != digest:
            raise RuntimeError("FBX/palette changed or provenance is missing: " + _path(value) + " " + tag)


def _validate_core(mesh, animation, skeleton):
    if not isinstance(mesh, unreal.SkeletalMesh) or _package_path(mesh) != MESH_PATH:
        raise RuntimeError("Expected one scoped SK_Arin SkeletalMesh")
    if not isinstance(skeleton, unreal.Skeleton) or _package_path(skeleton) != SKELETON_PATH:
        raise RuntimeError("Imported skeleton is not the dedicated SK_Arin_Skeleton")
    if not isinstance(animation, unreal.AnimSequence) or _package_path(animation) != ANIMATION_PATH:
        raise RuntimeError("Expected one scoped ArinIdle AnimSequence")
    if mesh.get_editor_property("skeleton") != skeleton or animation.get_editor_property("skeleton") != skeleton:
        raise RuntimeError("Arin mesh and idle must reference the identical Skeleton asset")
    seconds = float(animation.get_editor_property("sequence_length"))
    if not math.isfinite(seconds) or abs(seconds - 2.0) > 0.0001:
        raise RuntimeError("ArinIdle must be exactly 2 seconds; imported length=" + str(seconds))
    bounds = mesh.get_imported_bounds()
    extent = bounds.box_extent
    size = [float(extent.x) * 2, float(extent.y) * 2, float(extent.z) * 2]
    if not all(math.isfinite(value) for value in size) or not 166.0 <= size[2] <= 174.0:
        raise RuntimeError("Arin height must be approximately 170cm; imported bounds=" + str(size))
    if not 140.0 <= max(size[0], size[1]) <= 175.0 or min(size[0], size[1]) < 15.0:
        raise RuntimeError("Arin T-pose must have three-dimensional width and depth: " + str(size))
    return {"mesh": _path(mesh), "skeleton": _path(skeleton), "idle": _path(animation),
            "idle_seconds": seconds, "bounds_size_cm": size,
            "bounds_origin_cm": [float(bounds.origin.x), float(bounds.origin.y), float(bounds.origin.z)],
            "same_skeleton": True}


def _slot_key(slot):
    imported = str(slot.get_editor_property("imported_material_slot_name"))
    name = imported if imported and imported != "None" else str(slot.get_editor_property("material_slot_name"))
    # Rebuilding a Blender scene may append .001 to its otherwise identical names.
    name = re.sub(r"(?:\.\d{3}|_\d{3})$", "", name.strip())
    if not name.startswith("M_Arin_"):
        raise RuntimeError("Unexpected/default FBX material slot: " + name)
    return name[len("M_Arin_"):]


def _validate_slots(mesh, palette, materials, assign=False):
    slots = list(mesh.get_editor_property("materials"))
    keys = [_slot_key(slot) for slot in slots]
    if len(slots) != 15 or set(keys) != set(palette):
        raise RuntimeError("Expected exactly 15 named Blender material slots; found=" + str(keys))
    report = []
    for index, (slot, key) in enumerate(zip(slots, keys)):
        expected = materials[key]
        if assign:
            slot.set_editor_property("material_interface", expected)
        actual = slot.get_editor_property("material_interface")
        if actual != expected or not _package_path(actual).startswith(MATERIAL_ROOT + "/"):
            raise RuntimeError("Checker/default/wrong material in slot " + str(index))
        report.append({"index": index, "key": key, "material": _path(actual)})
    if assign:
        mesh.modify()
        mesh.set_editor_property("materials", slots)
    return report


def _create_materials(palette, hashes):
    results = {}
    library = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    # The existing 3-node PBR utility has the correct wiring but no skeletal use.
    # Duplicate it so its shared foliage package/automatic usage flag stay intact.
    source_parent = _require(PARENT_SOURCE_PATH, unreal.Material)
    if source_parent.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_OPAQUE:
        raise RuntimeError("The reviewed source parent is no longer opaque")
    parent = unreal.EditorAssetLibrary.duplicate_asset(PARENT_SOURCE_PATH, BASE_MATERIAL_PATH)
    if not isinstance(parent, unreal.Material):
        raise RuntimeError("Dedicated Arin parent duplication failed")
    parent.set_editor_property("used_with_skeletal_mesh", True)
    parent.set_editor_property("automatically_set_usage_in_editor", False)
    library.recompile_material(parent)
    _set_provenance(parent, hashes)
    unreal.EditorAssetLibrary.set_metadata_tag(parent, "Arin.MaterialParentSource", PARENT_SOURCE_PATH)
    for key, (rgb, roughness, metallic) in palette.items():
        name = "M_Arin_" + key
        material = tools.create_asset(name, MATERIAL_ROOT, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(material, unreal.MaterialInstanceConstant):
            raise RuntimeError("Dedicated material creation failed: " + name)
        library.set_material_instance_parent(material, parent)
        _set_instance_palette(material, key, palette)
        _set_provenance(material, hashes)
        results[key] = material
    _validate_materials(results, palette, parent)
    return results, parent


def _set_instance_palette(material, key, palette):
    library = unreal.MaterialEditingLibrary
    rgb, roughness, metallic = palette[key]
    # UE 5.8 implementation sets the value but unconditionally returns False.
    # Verify the actual values instead of treating that broken bool as failure.
    library.set_material_instance_vector_parameter_value(material, "Base Color", unreal.LinearColor(*rgb, 1.0))
    color = library.get_material_instance_vector_parameter_value(material, "Base Color")
    if max(abs(float(actual) - expected) for actual, expected in zip((color.r, color.g, color.b), rgb)) > 0.0001:
        raise RuntimeError("Cannot read back Arin Base Color override: " + key)
    for parameter, value in [("Roughness", roughness), ("Metallic", metallic)]:
        library.set_material_instance_scalar_parameter_value(material, parameter, float(value))
        actual = library.get_material_instance_scalar_parameter_value(material, parameter)
        if abs(float(actual) - value) > 0.0001:
            raise RuntimeError("Cannot read back Arin " + parameter + " override: " + key)
    library.update_material_instance(material)


def _validate_materials(materials, palette, parent):
    if not isinstance(parent, unreal.Material) or _package_path(parent) != BASE_MATERIAL_PATH:
        raise RuntimeError("The Arin parent material is not the dedicated scoped parent")
    if parent.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_OPAQUE:
        raise RuntimeError("The Arin parent must use the opaque material pipeline")
    if not parent.get_editor_property("used_with_skeletal_mesh") or parent.get_editor_property("automatically_set_usage_in_editor"):
        raise RuntimeError("Arin parent skeletal usage must be explicit, with automatic usage disabled")
    if parent.get_editor_property("shading_model") != unreal.MaterialShadingModel.MSM_DEFAULT_LIT:
        raise RuntimeError("The Arin parent must retain the reviewed default-lit PBR pipeline")
    library = unreal.MaterialEditingLibrary
    for key, material in materials.items():
        if not isinstance(material, unreal.MaterialInstanceConstant) or material.get_editor_property("parent") != parent:
            raise RuntimeError("Arin material instance has the wrong parent: " + key)
        rgb, roughness, metallic = palette[key]
        color = library.get_material_instance_vector_parameter_value(material, "Base Color")
        if max(abs(float(actual) - expected) for actual, expected in zip((color.r, color.g, color.b), rgb)) > 0.0001:
            raise RuntimeError("Arin instance BaseColor differs from Blender: " + key)
        for parameter, expected in [("Roughness", roughness), ("Metallic", metallic)]:
            actual = library.get_material_instance_scalar_parameter_value(material, parameter)
            if abs(float(actual) - expected) > 0.0001:
                raise RuntimeError("Arin instance parameter differs from Blender: " + key + " " + parameter)


def _import_new(palette, hashes):
    options = unreal.FbxImportUI()
    for key, value in {
        "automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "original_import_type": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "import_as_skeletal": True, "import_mesh": True, "import_animations": True,
        "import_materials": False, "import_textures": False,
        "create_physics_asset": False, "skeleton": None,
        "override_animation_name": "ArinIdle",
    }.items():
        options.set_editor_property(key, value)
    mesh_options = options.get_editor_property("skeletal_mesh_import_data")
    for key, value in {"use_t0_as_ref_pose": False, "update_skeleton_reference_pose": False,
                       "import_morph_targets": False, "import_uniform_scale": 1.0,
                       "convert_scene": True, "convert_scene_unit": True}.items():
        mesh_options.set_editor_property(key, value)
    anim_options = options.get_editor_property("anim_sequence_import_data")
    for key, value in {"use_default_sample_rate": True, "remove_redundant_keys": False,
                       "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
                       "import_uniform_scale": 1.0, "convert_scene": True,
                       "convert_scene_unit": True}.items():
        anim_options.set_editor_property(key, value)
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE_FBX)
    task.destination_path = ROOT
    task.destination_name = "SK_Arin"
    task.automated = True
    task.replace_existing = False
    task.save = False
    task.options = options
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = _scope_assets()
    meshes = [asset for asset in imported.values() if isinstance(asset, unreal.SkeletalMesh)]
    animations = [asset for asset in imported.values() if isinstance(asset, unreal.AnimSequence)]
    skeletons = [asset for asset in imported.values() if isinstance(asset, unreal.Skeleton)]
    if len(meshes) != 1 or len(animations) != 1 or len(skeletons) != 1:
        raise RuntimeError("FBX import must produce 1 mesh/1 animation/1 skeleton; found="
                           + str([len(meshes), len(animations), len(skeletons)]))
    mesh, animation, skeleton = meshes[0], animations[0], skeletons[0]
    core = _validate_core(mesh, animation, skeleton)
    materials, parent = _create_materials(palette, hashes)
    slots = _validate_slots(mesh, palette, materials, assign=True)
    animation.set_editor_property("enable_root_motion", False)
    saved = []
    for value in [parent, *materials.values(), skeleton, animation, mesh]:
        _set_provenance(value, hashes)
        saved.append(_save_asset(value))
    if set(_scope_assets()) != _expected_paths(palette):
        raise RuntimeError("The completed import contains unexpected assets or missing dependencies")
    return mesh, animation, skeleton, materials, core, slots, saved


def _load_or_import(palette, hashes, validate_only):
    existing = _scope_assets()
    expected = _expected_paths(palette)
    if existing:
        if set(existing) != expected:
            raise RuntimeError("Partial/unexpected Arin scope found; refusing overwrite: "
                               + json.dumps({"missing": sorted(expected - set(existing)),
                                             "unexpected": sorted(set(existing) - expected)}))
        for value in existing.values():
            _verify_provenance(value, hashes)
            if not _asset_disk_path(_package_path(value)).is_file():
                raise RuntimeError("Reused dependency is not persisted: " + _path(value))
        mesh = _require(MESH_PATH, unreal.SkeletalMesh)
        animation = _require(ANIMATION_PATH, unreal.AnimSequence)
        skeleton = _require(SKELETON_PATH, unreal.Skeleton)
        materials = {key: _require(MATERIAL_ROOT + "/M_Arin_" + key, unreal.MaterialInstanceConstant) for key in palette}
        _validate_materials(materials, palette, _require(BASE_MATERIAL_PATH, unreal.Material))
        core = _validate_core(mesh, animation, skeleton)
        slots = _validate_slots(mesh, palette, materials)
        return mesh, animation, skeleton, materials, core, slots, [], True
    if validate_only:
        raise RuntimeError("Validation requires an already persisted complete Arin import")
    unreal.EditorAssetLibrary.make_directory(ROOT)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
    return (*_import_new(palette, hashes), False)


def recover_known_material_partial(expected_paths, expected_hashes, checkpoint_sha256, apply_guide=True):
    """Resume only the inspected first-material failure from this import attempt.

    The caller supplies the exact five observed package paths, original FBX and
    palette hashes, and immutable before-guide checkpoint SHA256. This function
    imports/duplicates nothing and keeps normal ``main`` partial-abort behavior.
    It configures the known skin instance, creates the 14 absent instances, saves
    every dependency, and can then run the normal verified appearance application.
    """
    allowed = {MESH_PATH, SKELETON_PATH, ANIMATION_PATH, BASE_MATERIAL_PATH,
               MATERIAL_ROOT + "/M_Arin_skin"}
    if set(expected_paths) != allowed or len(expected_paths) != len(allowed):
        raise RuntimeError("Recovery requires the exact five inspected first-material partial paths")
    if not CHECKPOINT.is_file() or hashlib.sha256(CHECKPOINT.read_bytes()).hexdigest() != checkpoint_sha256:
        raise RuntimeError("Recovery checkpoint identity changed or is missing")
    baseline = json.loads(CHECKPOINT.read_text(encoding="utf-8"))
    context = _guide_context()
    before = _guide_state(*context)
    _verify_preserved(baseline, before)
    if before["appearance"] != baseline["appearance"]:
        raise RuntimeError("Recovery requires guide appearance to remain at the original checkpoint")
    for owner in ("cdo_component", "actor_component"):
        if before[owner] != baseline[owner]:
            raise RuntimeError("Guide component changed after the failed import: " + owner)
    palette = _palette()
    hashes = _hashes(palette)
    if hashes != expected_hashes:
        raise RuntimeError("Recovery FBX or palette changed after the inspected import attempt")
    roundtrip = json.loads((EVIDENCE / "roundtrip/validation.json").read_text(encoding="utf-8"))
    if (roundtrip.get("status") != "passed_roundtrip"
            or roundtrip.get("source_sha256") != hashes[FBX_TAG]
            or not roundtrip.get("checks") or not all(roundtrip["checks"].values())):
        raise RuntimeError("Recovery requires the passing roundtrip report for this exact FBX")
    content_before = json.loads((EVIDENCE / "content-before.json").read_text(encoding="utf-8"))
    scope_prefix = "Content/" + ROOT[len("/Game/"):] + "/"
    if any(str(path).replace("\\", "/").startswith(scope_prefix) for path in content_before):
        raise RuntimeError("Recovery scope was not empty in this invocation's Content checkpoint")
    existing = _scope_assets()
    if set(existing) != allowed:
        raise RuntimeError("Recovery partial scope no longer matches the inspected five packages")
    mesh = _require(MESH_PATH, unreal.SkeletalMesh)
    animation = _require(ANIMATION_PATH, unreal.AnimSequence)
    skeleton = _require(SKELETON_PATH, unreal.Skeleton)
    core = _validate_core(mesh, animation, skeleton)
    for value in (mesh, animation):
        import_data = value.get_editor_property("asset_import_data")
        filenames = import_data.extract_filenames() if import_data else []
        if len(filenames) != 1 or str(Path(filenames[0]).resolve()).casefold() != str(SOURCE_FBX.resolve()).casefold():
            raise RuntimeError("Recovery asset did not import the inspected Arin FBX: " + _path(value))
    source_parent = _require(PARENT_SOURCE_PATH, unreal.Material)
    source_flags = {key: bool(source_parent.get_editor_property(key))
                    for key in ("used_with_skeletal_mesh", "automatically_set_usage_in_editor")}
    parent = _require(BASE_MATERIAL_PATH, unreal.Material)
    _verify_provenance(parent, hashes)
    if unreal.EditorAssetLibrary.get_metadata_tag(parent, "Arin.MaterialParentSource") != PARENT_SOURCE_PATH:
        raise RuntimeError("Recovery parent was not duplicated from the reviewed existing utility")
    skin = _require(MATERIAL_ROOT + "/M_Arin_skin", unreal.MaterialInstanceConstant)
    if skin.get_editor_property("parent") != parent:
        raise RuntimeError("Recovery skin instance is not a child of the owned Arin parent")
    # Validate its original identity before changing the unfinished scalar values.
    color = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(skin, "Base Color")
    if max(abs(float(actual) - expected) for actual, expected in zip((color.r, color.g, color.b), palette["skin"][0])) > 0.0001:
        raise RuntimeError("The inspected skin Base Color write is no longer present")
    materials = {}
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for key in palette:
        if key == "skin":
            material = skin
        else:
            name = "M_Arin_" + key
            path = MATERIAL_ROOT + "/" + name
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                raise RuntimeError("Recovery refuses to overwrite an unexpected material: " + path)
            material = tools.create_asset(name, MATERIAL_ROOT, unreal.MaterialInstanceConstant,
                                          unreal.MaterialInstanceConstantFactoryNew())
            if not isinstance(material, unreal.MaterialInstanceConstant):
                raise RuntimeError("Recovery material creation failed: " + path)
            unreal.MaterialEditingLibrary.set_material_instance_parent(material, parent)
        _set_instance_palette(material, key, palette)
        _set_provenance(material, hashes)
        materials[key] = material
    _validate_materials(materials, palette, parent)
    slots = _validate_slots(mesh, palette, materials, assign=True)
    animation.set_editor_property("enable_root_motion", False)
    saved = []
    for value in [parent, *materials.values(), skeleton, animation, mesh]:
        _set_provenance(value, hashes)
        saved.append(_save_asset(value))
    if set(_scope_assets()) != _expected_paths(palette):
        raise RuntimeError("Recovery did not finish the exact 19-asset scope")
    _verify_preserved(baseline, _guide_state(*context))
    if source_flags != {key: bool(source_parent.get_editor_property(key)) for key in source_flags}:
        raise RuntimeError("Recovery changed a usage flag on the shared source parent")
    report = {"status": "known_first_material_failure_recovered", "source_hashes": hashes,
              "checkpoint_sha256": checkpoint_sha256, "inspected_partial_paths": sorted(allowed),
              "created_instances": 14, "reimported_fbx": False, "reduplicated_parent": False,
              "original_parent_modified": False, "saved_assets": saved,
              "content_checkpoint_scope_was_empty": True, "exact_fbx_roundtrip_verified": True,
              "source_parent_usage_flags": source_flags,
              "core": core, "material_slots": slots}
    _json(EVIDENCE / "unreal-material-recovery.json", report)
    if apply_guide:
        report["application"] = main()
    return report


def _protected_data(data):
    result = {}
    for key in PROTECTED_DATA_PROPERTIES:
        value = data.get_editor_property(key)
        result[key] = _path(value) if key == "starting_item" else int(value) if key == "starting_item_quantity" else str(value)
    return result


def _component_state(owner):
    component = owner.get_editor_property("guide_mesh")
    if not isinstance(component, unreal.SkeletalMeshComponent):
        raise RuntimeError("Guide has no SkeletalMeshComponent: " + _path(owner))
    mesh = component.get_skeletal_mesh_asset()
    animation_data = component.get_editor_property("animation_data")
    return component, {"mesh": _path(mesh),
                       "animation": _path(animation_data.get_editor_property("anim_to_play")),
                       "animation_mode": str(component.get_editor_property("animation_mode")),
                       "looping": bool(animation_data.get_editor_property("saved_looping")),
                       "playing": bool(animation_data.get_editor_property("saved_playing")),
                       "relative_location": list(component.get_editor_property("relative_location").to_tuple()),
                       "relative_rotation": list(component.get_editor_property("relative_rotation").to_tuple()),
                       "relative_scale": list(component.get_editor_property("relative_scale3d").to_tuple())}


def _guide_context():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or not world or _package_path(world) != MAP_PATH:
        raise RuntimeError("Requires the idle saved StartHub editor")
    data = _require(DATA_PATH, unreal.Rogue10mTutorialDataAsset)
    blueprint = _require(BLUEPRINT_PATH, unreal.Blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    found = [actor for actor in actors if actor.get_actor_label() == GUIDE_LABEL]
    if len(found) != 1:
        raise RuntimeError("Expected exactly one existing StartHub Arin guide; found=" + str(len(found)))
    actor = found[0]
    if not isinstance(actor, unreal.Rogue10mTutorialNPC):
        raise RuntimeError("The existing guide is not Rogue10mTutorialNPC")
    for owner in (cdo, actor):
        if owner.get_editor_property("tutorial_data") != data:
            raise RuntimeError("Guide DataAsset reference differs; refusing to replace existing references")
    return data, blueprint, cdo, actor


def _guide_state(data, blueprint, cdo, actor):
    _, cdo_component = _component_state(cdo)
    _, actor_component = _component_state(actor)
    return {"data": DATA_PATH, "blueprint": BLUEPRINT_PATH,
            "protected_data": _protected_data(data),
            "appearance": {"mesh": _path(data.get_editor_property("guide_skeletal_mesh")),
                           "idle": _path(data.get_editor_property("guide_idle_animation"))},
            "cdo_data": _path(cdo.get_editor_property("tutorial_data")),
            "cdo_component": cdo_component,
            "actor_component": actor_component,
            "actor_protected": {"path": _path(actor), "label": actor.get_actor_label(),
                                "class": _path(actor.get_class()),
                                "data": _path(actor.get_editor_property("tutorial_data")),
                                "location": list(actor.get_actor_location().to_tuple()),
                                "rotation": list(actor.get_actor_rotation().to_tuple()),
                                "scale": list(actor.get_actor_scale3d().to_tuple()),
                                "tags": list(map(str, actor.get_editor_property("tags"))),
                                "folder": str(actor.get_folder_path()),
                                "spatially_loaded": bool(actor.get_editor_property("is_spatially_loaded")),
                                "interaction_radius": float(actor.get_editor_property("interaction_radius")),
                                "detection_radius": float(actor.get_editor_property("detection_radius"))},
            "actor_package": actor.get_package().get_name()}


def _verify_preserved(before, after):
    for key in ("protected_data", "actor_protected", "cdo_data", "actor_package"):
        if before[key] != after[key]:
            raise RuntimeError("Guide settings changed outside appearance: " + key)
    for owner in ("cdo_component", "actor_component"):
        for key in ("relative_location", "relative_rotation", "relative_scale"):
            if before[owner][key] != after[owner][key]:
                raise RuntimeError("Guide mesh component transform was altered: " + owner + " " + key)


def _validate_appearance(state, mesh, animation):
    if state["appearance"] != {"mesh": _path(mesh), "idle": _path(animation)}:
        raise RuntimeError("Guide DataAsset does not reference the authored Arin mesh/idle")
    for owner in ("cdo_component", "actor_component"):
        component = state[owner]
        if component["mesh"] != _path(mesh) or component["animation"] != _path(animation):
            raise RuntimeError("Guide appearance was not applied to " + owner)
        if not component["looping"] or not component["playing"]:
            raise RuntimeError("Guide idle is not configured to loop/play: " + owner)
        if "ANIMATION_SINGLE_NODE" not in component["animation_mode"]:
            raise RuntimeError("Guide component is not in single-animation mode: " + owner)


def _save_guide_actor(actor):
    package = actor.get_package()
    name = package.get_name()
    # UObject.GetPackage includes the external actor package in partitioned maps.
    external_root = "/Game/__ExternalActors__/Rogue10m/Maps/StartHub/"
    if name != MAP_PATH and not name.startswith(external_root):
        raise RuntimeError("Guide package is not owned by StartHub: " + name)
    if not unreal.EditorLoadingAndSavingUtils.save_packages([package], False):
        raise RuntimeError("Guide-owned StartHub package save failed: " + name)
    return name


def main(validate_only=False):
    data, blueprint, cdo, actor = _guide_context()
    before = _guide_state(data, blueprint, cdo, actor)
    if CHECKPOINT.is_file():
        baseline = json.loads(CHECKPOINT.read_text(encoding="utf-8"))
        _verify_preserved(baseline, before)
    elif validate_only:
        raise RuntimeError("Missing preserved guide checkpoint for reload verification")
    else:
        _json(CHECKPOINT, before)
        baseline = before
    palette = _palette()
    hashes = _hashes(palette)
    mesh, animation, skeleton, materials, core, slots, saved, reused = _load_or_import(palette, hashes, validate_only)
    actor_saved = None
    if not validate_only:
        data.modify()
        data.set_editor_property("guide_skeletal_mesh", mesh)
        data.set_editor_property("guide_idle_animation", animation)
        blueprint.modify()
        for owner in (cdo, actor):
            owner.modify()
            component = owner.get_editor_property("guide_mesh")
            component.modify()
            owner.apply_guide_appearance()
        applied = _guide_state(data, blueprint, cdo, actor)
        _verify_preserved(before, applied)
        _verify_preserved(baseline, applied)
        _validate_appearance(applied, mesh, animation)
        saved.extend([_save_asset(data), _save_asset(blueprint)])
        actor_saved = _save_guide_actor(actor)
    after = _guide_state(data, blueprint, cdo, actor)
    _verify_preserved(baseline, after)
    _validate_appearance(after, mesh, animation)
    report = {"status": "reload_verified" if validate_only else "imported_and_applied",
              "validate_only": bool(validate_only), "source_fbx": str(SOURCE_FBX),
              "source_hashes": hashes, "reused_verified_scope": reused,
              "core": core, "material_slots": slots,
              "materials_palette": palette, "dedicated_material_count": len(materials),
              "parameterized_parent": BASE_MATERIAL_PATH,
              "material_parent_source": PARENT_SOURCE_PATH,
              "material_parameters": {"group": "preserved_from_existing_parent", "names": ["Base Color", "Roughness", "Metallic"]},
              "single_mesh_single_skeleton_single_idle": True,
              "dependencies_explicitly_saved": True, "saved_assets": saved,
              "saved_actor_package": actor_saved,
              "before_checkpoint": str(CHECKPOINT), "guide": after,
              "dialogue_and_supplies_unchanged": True,
              "projection_textures": False,
              "package_files_verified": sorted(_expected_paths(palette))}
    _json(EVIDENCE / ("unreal-reload-verification.json" if validate_only else "unreal-application.json"), report)
    unreal.log("ARIN_BLENDER_" + ("RELOAD_VERIFIED " if validate_only else "APPLIED ") + json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    main()
