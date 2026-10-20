"""Import Refined Arin's four baked atlases and update the existing guide.

Execute main() in the saved idle StartHub editor. After a fresh editor/map load,
execute main(validate_only=True) to inspect persistence without modifying assets.
All imports use a new owned RefinedV2 scope. Existing partial or changed-source
scopes abort with a recorded checkpoint; this script never deletes or overwrites
them. LOD generation preserves imported LOD0 and the single shared skeleton.
"""

from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_DIR = PROJECT / "Art/NPC/Arin/Refined"
SOURCE_FBX = SOURCE_DIR / "Arin.fbx"
SURFACE_MANIFEST = SOURCE_DIR / "surface-manifest.json"
EVIDENCE = PROJECT / "Feature/doc/evidence/arin-quality-20261007"
ROOT = "/Game/Rogue10m/Characters/Arin/RefinedV2"
MATERIAL_ROOT = ROOT + "/Materials"
TEXTURE_ROOT = ROOT + "/Textures"
PARENT_PATH = MATERIAL_ROOT + "/M_Arin_PBR"
MESH_PATH = ROOT + "/SK_Arin"
SKELETON_PATH = ROOT + "/SK_Arin_Skeleton"
ANIMATION_PATH = ROOT + "/ArinIdle"
GUIDE_ROOT = "/Game/Rogue10m/World/StartVillage/Tutorial"
DATA_PATH = GUIDE_ROOT + "/DA_StartVillageGuide"
BLUEPRINT_PATH = GUIDE_ROOT + "/BP_StartVillageGuide"
MAP_PATH = "/Game/Rogue10m/Maps/StartHub"
GUIDE_LABEL = "SV_TutorialGuide_Arin"
CHECKPOINT = EVIDENCE / "before-refined-guide-application.json"
STATE_FILE = EVIDENCE / "unreal-refined-import-state.json"
GROUP_RESOLUTIONS = {"Face": 2048, "Hair": 2048, "Cloth": 4096, "Gear": 2048}
CHANNELS = ("BaseColor", "Roughness", "Metallic", "Normal")
TEXTURE_PARAMETERS = {channel: channel + "Texture" for channel in CHANNELS}
REQUIRED_BONES = {
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "neck", "head",
    *{part + "_" + side for side in ("l", "r") for part in (
        "clavicle", "upperarm", "lowerarm", "hand", "thigh", "calf", "foot", "ball")},
}
PROTECTED_DATA_PROPERTIES = (
    "guide_name", "guide_role", "welcome_text", "reward_received_text",
    "reward_already_received_text", "reward_id", "starting_item", "starting_item_quantity",
)
PROVENANCE_TAGS = ("Arin.RefinedFBXSHA256", "Arin.SurfaceManifestSHA256", "Arin.TextureBundleSHA256")


def _path(value):
    return value.get_path_name() if value is not None else None


def _package_path(value):
    return value.get_path_name().split(".")[0]


def _json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def _require(path, cls=None):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if value is None or (cls and not isinstance(value, cls)):
        raise RuntimeError("Missing/wrong asset class: " + path)
    return value


def _digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _surface_sources():
    if not SOURCE_FBX.is_file() or not SURFACE_MANIFEST.is_file():
        raise RuntimeError("Refined FBX and surface-manifest must both exist before importing")
    manifest = json.loads(SURFACE_MANIFEST.read_text(encoding="utf-8-sig"))
    if set(manifest.get("groups", {})) != set(GROUP_RESOLUTIONS):
        raise RuntimeError("Surface manifest must contain exactly Face/Hair/Cloth/Gear")
    if manifest.get("image_projection") is not False:
        raise RuntimeError("Refined handoff must declare no image projection")
    sources = {}
    for group, resolution in GROUP_RESOLUTIONS.items():
        description = manifest["groups"][group]
        actual_resolution = description.get("resolution")
        if actual_resolution not in ([resolution, resolution], resolution):
            raise RuntimeError("Atlas resolution differs: " + group)
        if description.get("material") != "M_Arin_" + group or set(description.get("channels", {})) != set(CHANNELS):
            raise RuntimeError("Atlas material/channel names differ: " + group)
        for channel in CHANNELS:
            filename = "T_Arin_" + group + "_" + channel + ".png"
            if description["channels"][channel] != filename:
                raise RuntimeError("Unexpected texture filename: " + group + " " + channel)
            file = SOURCE_DIR / "Textures" / filename
            if not file.is_file() or file.stat().st_size == 0:
                raise RuntimeError("Missing external baked PNG: " + str(file))
            actual_hash = _digest(file)
            file_description = manifest.get("files", {}).get(filename)
            if not file_description or file_description.get("sha256") != actual_hash or file_description.get("bytes") != file.stat().st_size:
                raise RuntimeError("Surface manifest does not match actual PNG bytes: " + filename)
            sources[(group, channel)] = {
                "file": file, "name": file.stem, "resolution": resolution, "sha256": actual_hash,
                "asset": TEXTURE_ROOT + "/" + file.stem,
            }
    if set(manifest.get("files", {})) != {source["file"].name for source in sources.values()}:
        raise RuntimeError("Surface manifest must describe exactly the 16 delivered PBR PNGs")
    texture_digest = hashlib.sha256(json.dumps({source["file"].name: source["sha256"]
        for source in sources.values()}, sort_keys=True).encode("utf-8")).hexdigest()
    hashes = {PROVENANCE_TAGS[0]: _digest(SOURCE_FBX), PROVENANCE_TAGS[1]: _digest(SURFACE_MANIFEST),
              PROVENANCE_TAGS[2]: texture_digest}
    return manifest, sources, hashes


def _expected_paths(sources):
    return {MESH_PATH, SKELETON_PATH, ANIMATION_PATH, PARENT_PATH} | {
        MATERIAL_ROOT + "/MI_Arin_" + group for group in GROUP_RESOLUTIONS
    } | {source["asset"] for source in sources.values()}


def _scope_assets():
    return {_package_path(_require(path)): _require(path) for path in
            unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False)}


def _set_provenance(value, hashes):
    for name, digest in hashes.items():
        unreal.EditorAssetLibrary.set_metadata_tag(value, name, digest)


def _verify_provenance(value, hashes):
    for name, digest in hashes.items():
        if unreal.EditorAssetLibrary.get_metadata_tag(value, name) != digest:
            raise RuntimeError("Changed source or missing provenance: " + _path(value) + " " + name)


def _disk_path(path):
    if not path.startswith("/Game/"):
        raise RuntimeError("Not a project asset package: " + path)
    return PROJECT / "Content" / (path[len("/Game/"):] + ".uasset")


def _save(value):
    path = _package_path(value)
    if not path.startswith(ROOT + "/") and path not in {DATA_PATH, BLUEPRINT_PATH}:
        raise RuntimeError("Save is outside the RefinedV2/guide scope: " + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(value, only_if_is_dirty=False):
        raise RuntimeError("Explicit asset save failed: " + path)
    if not _disk_path(path).is_file():
        raise RuntimeError("Saved dependency has no on-disk .uasset: " + path)
    return path


def _stage(state, phase, **details):
    state["phase"] = phase
    state.setdefault("events", []).append({"utc": datetime.now(timezone.utc).isoformat(), "phase": phase, **details})
    _json(STATE_FILE, state)


def _texture_settings(texture, channel):
    # One PreEditChange/PostEditChange pair avoids cancelling/restarting the
    # texture build once per property and leaving a stale proxy resource.
    texture.set_editor_properties({"srgb": channel == "BaseColor", "compression_settings":
        unreal.TextureCompressionSettings.TC_NORMALMAP if channel == "Normal" else
        unreal.TextureCompressionSettings.TC_MASKS if channel in {"Roughness", "Metallic"} else
        unreal.TextureCompressionSettings.TC_DEFAULT,
        "flip_green_channel": channel == "Normal", "lod_bias": 0, "max_texture_size": 0})


def _finish_texture_compilation():
    # Official registered console command; no invented reflected manager API.
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,"Editor.AsyncTextureCompilationFinishAll")


def _validate_textures(textures, sources, hashes):
    report = []
    for key, source in sources.items():
        group, channel = key
        texture = textures[key]
        if not isinstance(texture, unreal.Texture2D) or _package_path(texture) != source["asset"]:
            raise RuntimeError("Wrong scoped atlas texture: " + str(key))
        _verify_provenance(texture, hashes)
        if unreal.EditorAssetLibrary.get_metadata_tag(texture, "Arin.TextureSHA256") != source["sha256"]:
            raise RuntimeError("Texture source hash differs: " + str(key))
        actual_size = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
        if actual_size != [source["resolution"], source["resolution"]]:
            raise RuntimeError("Imported atlas size differs: " + str(key) + " " + str(actual_size))
        srgb = bool(texture.get_editor_property("srgb"))
        flip_green = bool(texture.get_editor_property("flip_green_channel"))
        compression = texture.get_editor_property("compression_settings")
        expected_compression = (unreal.TextureCompressionSettings.TC_NORMALMAP if channel == "Normal" else
            unreal.TextureCompressionSettings.TC_MASKS if channel in {"Roughness", "Metallic"} else
            unreal.TextureCompressionSettings.TC_DEFAULT)
        if srgb != (channel == "BaseColor") or flip_green != (channel == "Normal") or compression != expected_compression:
            raise RuntimeError("Incorrect atlas color/compression/normal convention: " + str(key))
        report.append({"group": group, "channel": channel, "texture": _path(texture), "size": actual_size,
                       "srgb": srgb, "compression": str(compression), "flip_green_channel": flip_green,
                       "source_sha256": source["sha256"]})
    return report


def _import_textures(sources, hashes):
    tasks = []
    for source in sources.values():
        task = unreal.AssetImportTask()
        task.filename = str(source["file"])
        task.destination_path = TEXTURE_ROOT
        task.destination_name = source["name"]
        task.automated, task.replace_existing, task.save = True, False, False
        task.factory = unreal.TextureFactory()
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    textures = {}
    for key, source in sources.items():
        texture = _require(source["asset"], unreal.Texture2D)
        _texture_settings(texture, key[1])
        _set_provenance(texture, hashes)
        unreal.EditorAssetLibrary.set_metadata_tag(texture, "Arin.TextureSHA256", source["sha256"])
        textures[key] = texture
    _finish_texture_compilation()
    _validate_textures(textures, sources, hashes)
    return textures


def _properties():
    return {"BaseColor": unreal.MaterialProperty.MP_BASE_COLOR,
            "Roughness": unreal.MaterialProperty.MP_ROUGHNESS,
            "Metallic": unreal.MaterialProperty.MP_METALLIC, "Normal": unreal.MaterialProperty.MP_NORMAL}


def _sampler(channel):
    return (unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel == "Normal" else
            unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if channel in {"Roughness", "Metallic"} else
            unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)


def _validate_parent(parent):
    library = unreal.MaterialEditingLibrary
    if not isinstance(parent, unreal.Material) or _package_path(parent) != PARENT_PATH:
        raise RuntimeError("Expected the dedicated RefinedV2 PBR parent")
    if parent.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_OPAQUE or parent.get_editor_property("shading_model") != unreal.MaterialShadingModel.MSM_DEFAULT_LIT:
        raise RuntimeError("Refined PBR parent must be opaque/default-lit")
    if not parent.get_editor_property("used_with_skeletal_mesh") or parent.get_editor_property("automatically_set_usage_in_editor"):
        raise RuntimeError("Parent skeletal usage must be explicit and automatic usage disabled")
    expressions = list(library.get_material_expressions(parent))
    if len(expressions) != 4 or any(not isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D) for node in expressions):
        raise RuntimeError("PBR parent must contain exactly four texture parameter sample expressions")
    wiring = []
    for channel, prop in _properties().items():
        node = library.get_material_property_input_node(parent, prop)
        if not isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D):
            raise RuntimeError("PBR output has no texture sample: " + channel)
        if str(node.get_editor_property("parameter_name")) != TEXTURE_PARAMETERS[channel] or node.get_editor_property("sampler_type") != _sampler(channel):
            raise RuntimeError("Incorrect PBR sample parameter/sampler: " + channel)
        output = str(library.get_material_property_input_node_output_name(parent, prop))
        expected_output = "R" if channel in {"Roughness", "Metallic"} else "RGB"
        if output != expected_output:
            raise RuntimeError("Incorrect PBR channel output pin: " + channel + " " + output)
        wiring.append({"channel": channel, "parameter": TEXTURE_PARAMETERS[channel], "output_pin": output,
                       "sampler_type": str(node.get_editor_property("sampler_type"))})
    return wiring


def _validate_materials(materials, parent, textures):
    _validate_parent(parent)
    library = unreal.MaterialEditingLibrary
    report = []
    for group, material in materials.items():
        if not isinstance(material, unreal.MaterialInstanceConstant) or _package_path(material) != MATERIAL_ROOT + "/MI_Arin_" + group or material.get_editor_property("parent") != parent:
            raise RuntimeError("Incorrect dedicated atlas instance: " + group)
        parameters = {}
        for channel in CHANNELS:
            actual = library.get_material_instance_texture_parameter_value(material, TEXTURE_PARAMETERS[channel])
            if actual != textures[(group, channel)]:
                raise RuntimeError("Atlas parameter does not reference the actual baked texture: " + group + " " + channel)
            parameters[channel] = _path(actual)
        report.append({"group": group, "material": _path(material), "parent": _path(parent), "textures": parameters})
    return report


def _create_materials(textures, hashes):
    library = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    parent = tools.create_asset("M_Arin_PBR", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(parent, unreal.Material):
        raise RuntimeError("Could not create the new owned PBR parent")
    parent.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    parent.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    parent.set_editor_property("used_with_skeletal_mesh", True)
    parent.set_editor_property("automatically_set_usage_in_editor", False)
    for index, (channel, prop) in enumerate(_properties().items()):
        node = library.create_material_expression(parent, unreal.MaterialExpressionTextureSampleParameter2D, -400, index * 210)
        node.set_editor_property("parameter_name", TEXTURE_PARAMETERS[channel])
        node.set_editor_property("texture", textures[("Face", channel)])
        node.set_editor_property("sampler_type", _sampler(channel))
        output = "R" if channel in {"Roughness", "Metallic"} else "RGB"
        if not library.connect_material_property(node, output, prop):
            raise RuntimeError("Could not connect PBR parent output: " + channel)
    errors = list(library.recompile_material(parent))
    if errors:
        raise RuntimeError("Refined PBR parent compile errors: " + str(errors))
    _set_provenance(parent, hashes)
    unreal.EditorAssetLibrary.set_metadata_tag(parent, "Arin.ParentAuthorship", "Four baked PBR atlas channels; owned RefinedV2 graph")
    materials = {}
    for group in GROUP_RESOLUTIONS:
        material = tools.create_asset("MI_Arin_" + group, MATERIAL_ROOT,
            unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(material, unreal.MaterialInstanceConstant):
            raise RuntimeError("Could not create atlas material instance: " + group)
        library.set_material_instance_parent(material, parent)
        for channel in CHANNELS:
            # UE's parameter setter bool is not reliable; read back the actual
            # parameter to establish that the override took effect.
            library.set_material_instance_texture_parameter_value(material, TEXTURE_PARAMETERS[channel], textures[(group, channel)])
        library.update_material_instance(material)
        _set_provenance(material, hashes)
        materials[group] = material
    _validate_materials(materials, parent, textures)
    return parent, materials


def _slot_group(slot):
    name = str(slot.get_editor_property("imported_material_slot_name"))
    if not name or name == "None":
        name = str(slot.get_editor_property("material_slot_name"))
    name = re.sub(r"(?:\.\d{3}|_\d{3})$", "", name.strip())
    if not name.startswith("M_Arin_") or name[len("M_Arin_"):] not in GROUP_RESOLUTIONS:
        raise RuntimeError("Unknown FBX atlas slot: " + name)
    return name[len("M_Arin_"):]


def _slots(mesh, materials, assign=False):
    slots = list(mesh.get_editor_property("materials"))
    groups = [_slot_group(slot) for slot in slots]
    if len(slots) != 4 or set(groups) != set(GROUP_RESOLUTIONS):
        raise RuntimeError("FBX must contain exactly four atlas slots: " + str(groups))
    report = []
    for index, (slot, group) in enumerate(zip(slots, groups)):
        if assign:
            slot.set_editor_property("material_interface", materials[group])
        actual = slot.get_editor_property("material_interface")
        if actual != materials[group]:
            raise RuntimeError("Unexpected material in refined mesh slot: " + group)
        report.append({"index": index, "group": group, "material": _path(actual)})
    if assign:
        mesh.modify()
        mesh.set_editor_property("materials", slots)
    return report


def _validate_core(mesh, animation, skeleton):
    if not isinstance(mesh, unreal.SkeletalMesh) or _package_path(mesh) != MESH_PATH:
        raise RuntimeError("Expected one dedicated refined SK_Arin")
    if not isinstance(animation, unreal.AnimSequence) or _package_path(animation) != ANIMATION_PATH:
        raise RuntimeError("Expected one dedicated refined ArinIdle")
    if not isinstance(skeleton, unreal.Skeleton) or _package_path(skeleton) != SKELETON_PATH:
        raise RuntimeError("Expected the dedicated refined SK_Arin_Skeleton")
    if mesh.get_editor_property("skeleton") != skeleton or animation.get_editor_property("skeleton") != skeleton:
        raise RuntimeError("Refined mesh and Idle must reference the same Skeleton object")
    duration = float(animation.get_editor_property("sequence_length"))
    if not math.isfinite(duration) or abs(duration - 2) > .0001:
        raise RuntimeError("Refined Idle duration must be 2 seconds")
    bounds = mesh.get_imported_bounds()
    size = [float(value) * 2 for value in (bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z)]
    if not all(math.isfinite(value) for value in size) or abs(size[2] - 170) > 1:
        raise RuntimeError("Known imported character dimension must be 170cm: " + str(size))
    if min(size[0], size[1]) < 15 or max(size[0], size[1]) < 135:
        raise RuntimeError("Imported rest-pose mesh has implausible flat or collapsed bounds: " + str(size))
    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(mesh)
    bone_names = [str(probe.get_bone_name(index)) for index in range(probe.get_num_bones())]
    if not REQUIRED_BONES <= set(bone_names) or len(bone_names) not in {23, 24}:
        raise RuntimeError("Refined Unreal mesh must retain the 23 authored bones plus optional FBX armature root: " + str(bone_names))
    if animation.get_editor_property("enable_root_motion"):
        raise RuntimeError("Stationary NPC Idle must have root motion disabled")
    import_data = mesh.get_editor_property("asset_import_data")
    normal_settings = {name: str(import_data.get_editor_property(name)) for name in
        ("normal_import_method", "normal_generation_method", "compute_weighted_normals")}
    if import_data.get_editor_property("normal_import_method") != unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS or import_data.get_editor_property("normal_generation_method") != unreal.FBXNormalGenerationMethod.MIKK_T_SPACE or import_data.get_editor_property("compute_weighted_normals"):
        raise RuntimeError("Imported normals/MikkTSpace must match the baked normal maps: " + str(normal_settings))
    return {"mesh": _path(mesh), "skeleton": _path(skeleton), "idle": _path(animation),
            "idle_seconds": duration, "same_skeleton": True, "bounds_size_cm": size,
            "bounds_origin_cm": list(bounds.origin.to_tuple()), "bone_names": bone_names,
            "known_dimension_test": {"expected_height_cm": 170, "observed_height_cm": size[2]},
            "normal_import_settings": normal_settings,
            "mesh_collision_plan": "existing detection sphere and non-colliding guide mesh preserved; no mesh physics asset"}


def _lod_metrics(mesh):
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    count = subsystem.get_lod_count(mesh)
    metrics = []
    # get_editor_property is reflected. Plain C++ USkeletalMesh::GetLODInfo
    # is not a Python UFUNCTION and must not be assumed to exist.
    try:
        source_models = mesh.get_editor_property("source_models")
    except Exception:
        source_models = []
    for index in range(count):
        item = {"index": index, "vertices": subsystem.get_num_verts(mesh, index),
                "sections": subsystem.get_num_sections(mesh, index), "triangles": None,
                "triangle_measurement": "not_exposed_by_skeletal_editor_subsystem"}
        if index < len(source_models):
            try:
                reduction = source_models[index].get_editor_property("reduction_settings")
                item["target_triangle_ratio"] = float(reduction.get_editor_property("num_of_triangles_percentage"))
                item["reduction_base_lod"] = int(reduction.get_editor_property("base_lod"))
                item["lock_edges"] = bool(reduction.get_editor_property("bLockEdges"))
                item["volume_importance"] = float(reduction.get_editor_property("VolumeImportance"))
            except Exception as exc:
                item["reduction_probe_limitation"] = str(exc)
            try:
                source_triangles = int(source_models[index].get_editor_property("triangle_count"))
                # Auto-generated source models may deliberately have no stored
                # mesh description. Zero is not a measured empty runtime LOD.
                item["source_mesh_description_triangles"] = source_triangles
                if source_triangles > 0:
                    item["triangles"] = source_triangles
                    item["triangle_measurement"] = "source_mesh_description_cached_count; runtime indices not inspected"
            except Exception as exc:
                item["source_model_probe_limitation"] = str(exc)
        metrics.append(item)
    if count != 3 or any(item["vertices"] <= 0 or not 1 <= item["sections"] <= 4 for item in metrics):
        raise RuntimeError("Expected three non-empty skeletal LODs with at most four sections: " + str(metrics))
    if not metrics[0]["vertices"] > metrics[1]["vertices"] > metrics[2]["vertices"]:
        raise RuntimeError("Generated skeletal LOD vertex counts did not decrease: " + str(metrics))
    return {"count": count, "metrics": metrics,
            "generation": "SkeletalMeshEditorSubsystem.regenerate_lod; imported LOD0 preserved",
            "lod0_regeneration": False,
            "requested_ratios": [item.get('target_triangle_ratio') for item in metrics],
            "same_skeleton_and_materials": True,
            "visual_review_required": True,
            "runtime_triangle_count_limitation": "Exact runtime triangles need a separate exposed render-data/export inspection; do not infer them from vertices or reduction targets."}


def _import_mesh():
    options = unreal.FbxImportUI()
    for name, value in {
        "automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "original_import_type": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "import_as_skeletal": True, "import_mesh": True, "import_animations": True,
        "import_materials": False, "import_textures": False,
        "create_physics_asset": False, "skeleton": None, "override_animation_name": "ArinIdle",
    }.items():
        options.set_editor_property(name, value)
    for name, value in {"use_t0_as_ref_pose": False, "update_skeleton_reference_pose": False,
        "import_morph_targets": False, "import_uniform_scale": 1., "convert_scene": True,
        "convert_scene_unit": True,
        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
        "normal_generation_method": unreal.FBXNormalGenerationMethod.MIKK_T_SPACE,
        "compute_weighted_normals": False}.items():
        options.get_editor_property("skeletal_mesh_import_data").set_editor_property(name, value)
    for name, value in {"use_default_sample_rate": True, "remove_redundant_keys": False,
        "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
        "import_uniform_scale": 1., "convert_scene": True, "convert_scene_unit": True}.items():
        options.get_editor_property("anim_sequence_import_data").set_editor_property(name, value)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(SOURCE_FBX), ROOT, "SK_Arin"
    task.automated, task.replace_existing, task.save = True, False, False
    task.options, task.factory = options, unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assets = _scope_assets()
    mesh, animation, skeleton = (_require(MESH_PATH, unreal.SkeletalMesh),
        _require(ANIMATION_PATH, unreal.AnimSequence), _require(SKELETON_PATH, unreal.Skeleton))
    if {path for path, asset in assets.items() if isinstance(asset, (unreal.SkeletalMesh, unreal.AnimSequence, unreal.Skeleton))} != {MESH_PATH, ANIMATION_PATH, SKELETON_PATH}:
        raise RuntimeError("FBX import produced unexpected core assets")
    animation.set_editor_property("enable_root_motion", False)
    _validate_core(mesh, animation, skeleton)
    return mesh, animation, skeleton


def _configure_generated_lods(mesh):
    """Write through nested reflected value copies; preserve all source data."""
    models = mesh.get_editor_property('source_models')
    if len(models) != 3:
        raise RuntimeError('Configure generated LODs only after creating exactly three models')
    for index, ratio in ((1, .85), (2, .65)):
        model = models[index]
        reduction = model.get_editor_property('reduction_settings')
        reduction.set_editor_properties({'NumOfTrianglesPercentage': ratio, 'BaseLOD': 0,
            'bLockEdges': True, 'VolumeImportance': 1., 'MaxBonesPerVertex': 4})
        model.set_editor_property('reduction_settings', reduction)
        models[index] = model
    mesh.set_editor_property('source_models', models)
    for index, ratio in ((1, .85), (2, .65)):
        actual = mesh.get_editor_property('source_models')[index].get_editor_property('reduction_settings')
        if abs(actual.get_editor_property('NumOfTrianglesPercentage') - ratio) > .00001 or not actual.get_editor_property('bLockEdges'):
            raise RuntimeError('Generated LOD settings did not reach the owning mesh')


def _load_or_import(sources, hashes, validate_only, state):
    expected, existing = _expected_paths(sources), _scope_assets()
    if existing:
        if set(existing) != expected:
            raise RuntimeError("Partial/unexpected refined scope; inspect checkpoint before recovery: " + json.dumps({
                "missing": sorted(expected - set(existing)), "unexpected": sorted(set(existing) - expected)}))
        for asset in existing.values():
            _verify_provenance(asset, hashes)
            if not _disk_path(_package_path(asset)).is_file():
                raise RuntimeError("Existing refined dependency has not been explicitly persisted: " + _path(asset))
        mesh, animation, skeleton = (_require(MESH_PATH, unreal.SkeletalMesh),
            _require(ANIMATION_PATH, unreal.AnimSequence), _require(SKELETON_PATH, unreal.Skeleton))
        textures = {key: _require(source["asset"], unreal.Texture2D) for key, source in sources.items()}
        parent = _require(PARENT_PATH, unreal.Material)
        materials = {group: _require(MATERIAL_ROOT + "/MI_Arin_" + group, unreal.MaterialInstanceConstant) for group in GROUP_RESOLUTIONS}
        return mesh, animation, skeleton, textures, parent, materials, [], True
    if validate_only:
        raise RuntimeError("Validation requires a previously persisted complete refined import")
    for directory in (ROOT, MATERIAL_ROOT, TEXTURE_ROOT):
        unreal.EditorAssetLibrary.make_directory(directory)
    _stage(state, "import_fbx")
    mesh, animation, skeleton = _import_mesh()
    _stage(state, "import_16_textures", observed_assets=sorted(_scope_assets()))
    textures = _import_textures(sources, hashes)
    _stage(state, "create_pbr_parent_and_4_instances", observed_assets=sorted(_scope_assets()))
    parent, materials = _create_materials(textures, hashes)
    _slots(mesh, materials, assign=True)
    _stage(state, "generate_skeletal_lods", observed_assets=sorted(_scope_assets()))
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    before_lod0_vertices = subsystem.get_num_verts(mesh, 0)
    before_skeleton = mesh.get_editor_property("skeleton")
    if not subsystem.regenerate_lod(mesh, 3, False, False):
        raise RuntimeError("Skeletal LOD generation is unavailable/failed; guide has not been replaced")
    _configure_generated_lods(mesh)
    if not subsystem.regenerate_lod(mesh, 3, False, False):
        raise RuntimeError('Boundary-protected skeletal LOD generation failed')
    _lod_metrics(mesh)
    if subsystem.get_num_verts(mesh, 0) != before_lod0_vertices or mesh.get_editor_property("skeleton") != before_skeleton:
        raise RuntimeError("LOD generation changed imported LOD0 or its Skeleton")
    _stage(state, "save_24_owned_dependencies", observed_assets=sorted(_scope_assets()))
    saved = []
    for asset in [*textures.values(), parent, *materials.values(), skeleton, animation, mesh]:
        _set_provenance(asset, hashes)
        saved.append(_save(asset))
    if set(_scope_assets()) != expected or len(saved) != 24:
        raise RuntimeError("Refined import must contain exactly the 24 expected saved asset dependencies")
    return mesh, animation, skeleton, textures, parent, materials, saved, False


def _protected_data(data):
    result = {}
    for name in PROTECTED_DATA_PROPERTIES:
        value = data.get_editor_property(name)
        result[name] = _path(value) if name == "starting_item" else int(value) if name == "starting_item_quantity" else str(value)
    return result


def _component_state(owner):
    component = owner.get_editor_property("guide_mesh")
    if not isinstance(component, unreal.SkeletalMeshComponent):
        raise RuntimeError("Existing guide has no SkeletalMeshComponent")
    animation = component.get_editor_property("animation_data")
    return {"mesh": _path(component.get_skeletal_mesh_asset()),
            "animation": _path(animation.get_editor_property("anim_to_play")),
            "animation_mode": str(component.get_editor_property("animation_mode")),
            "looping": bool(animation.get_editor_property("saved_looping")),
            "playing": bool(animation.get_editor_property("saved_playing")),
            "relative_location": list(component.get_editor_property("relative_location").to_tuple()),
            "relative_rotation": list(component.get_editor_property("relative_rotation").to_tuple()),
            "relative_scale": list(component.get_editor_property("relative_scale3d").to_tuple())}


def _guide_context():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor() or not world or _package_path(world) != MAP_PATH:
        raise RuntimeError("Requires saved idle StartHub editor/map")
    data = _require(DATA_PATH, unreal.Rogue10mTutorialDataAsset)
    blueprint = _require(BLUEPRINT_PATH, unreal.Blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    actors = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
              if actor.get_actor_label() == GUIDE_LABEL]
    if len(actors) != 1 or not isinstance(actors[0], unreal.Rogue10mTutorialNPC):
        raise RuntimeError("Expected the single existing StartHub Arin guide")
    actor = actors[0]
    if any(owner.get_editor_property("tutorial_data") != data for owner in (cdo, actor)):
        raise RuntimeError("Existing guide's DataAsset reference differs")
    return data, blueprint, cdo, actor


def _guide_state(data, blueprint, cdo, actor):
    return {"data": DATA_PATH, "blueprint": BLUEPRINT_PATH, "protected_data": _protected_data(data),
        "appearance": {"mesh": _path(data.get_editor_property("guide_skeletal_mesh")),
                       "idle": _path(data.get_editor_property("guide_idle_animation"))},
        "cdo_data": _path(cdo.get_editor_property("tutorial_data")),
        "cdo_component": _component_state(cdo), "actor_component": _component_state(actor),
        "actor_protected": {"path": _path(actor), "label": actor.get_actor_label(), "class": _path(actor.get_class()),
            "data": _path(actor.get_editor_property("tutorial_data")), "location": list(actor.get_actor_location().to_tuple()),
            "rotation": list(actor.get_actor_rotation().to_tuple()), "scale": list(actor.get_actor_scale3d().to_tuple()),
            "tags": list(map(str, actor.get_editor_property("tags"))), "folder": str(actor.get_folder_path()),
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
                raise RuntimeError("Existing guide component transform changed: " + owner + " " + key)


def _validate_appearance(state, mesh, animation):
    if state["appearance"] != {"mesh": _path(mesh), "idle": _path(animation)}:
        raise RuntimeError("Guide DataAsset has incorrect refined appearance")
    for owner in ("cdo_component", "actor_component"):
        component = state[owner]
        if component["mesh"] != _path(mesh) or component["animation"] != _path(animation) or not component["looping"] or not component["playing"] or "ANIMATION_SINGLE_NODE" not in component["animation_mode"]:
            raise RuntimeError("Refined appearance/looping Idle is not applied to " + owner)


def _save_actor(actor):
    package = actor.get_package()
    name = package.get_name()
    if name != MAP_PATH and not name.startswith("/Game/__ExternalActors__/Rogue10m/Maps/StartHub/"):
        raise RuntimeError("Guide actor package is outside StartHub")
    if not unreal.EditorLoadingAndSavingUtils.save_packages([package], False):
        raise RuntimeError("Could not save the guide-owned actor package")
    return name


def main(validate_only=False):
    state = {"validate_only": bool(validate_only), "root": ROOT, "source_fbx": str(SOURCE_FBX)}
    try:
        data, blueprint, cdo, actor = _guide_context()
        before = _guide_state(data, blueprint, cdo, actor)
        if CHECKPOINT.is_file():
            baseline = json.loads(CHECKPOINT.read_text(encoding="utf-8-sig"))
            _verify_preserved(baseline, before)
        elif validate_only:
            raise RuntimeError("Missing refined guide baseline for persistence verification")
        else:
            _json(CHECKPOINT, before)
            baseline = before
        manifest, sources, hashes = _surface_sources()
        state["source_hashes"] = hashes
        _stage(state, "preflight", expected_assets=sorted(_expected_paths(sources)), observed_assets=sorted(_scope_assets()))
        mesh, animation, skeleton, textures, parent, materials, saved, reused = _load_or_import(sources, hashes, validate_only, state)
        core = _validate_core(mesh, animation, skeleton)
        # Loaded assets may still be asynchronously compiling after a fresh
        # process/map load. Wait without changing texture properties or saves.
        _finish_texture_compilation()
        texture_report = _validate_textures(textures, sources, hashes)
        material_report = _validate_materials(materials, parent, textures)
        slot_report = _slots(mesh, materials)
        lod_report = _lod_metrics(mesh)
        actor_package = None
        if not validate_only:
            _stage(state, "apply_only_existing_guide_appearance")
            data.modify()
            data.set_editor_property("guide_skeletal_mesh", mesh)
            data.set_editor_property("guide_idle_animation", animation)
            blueprint.modify()
            for owner in (cdo, actor):
                owner.modify()
                owner.get_editor_property("guide_mesh").modify()
                owner.apply_guide_appearance()
            applied = _guide_state(data, blueprint, cdo, actor)
            _verify_preserved(before, applied)
            _verify_preserved(baseline, applied)
            _validate_appearance(applied, mesh, animation)
            saved.extend([_save(data), _save(blueprint)])
            actor_package = _save_actor(actor)
        after = _guide_state(data, blueprint, cdo, actor)
        _verify_preserved(baseline, after)
        _validate_appearance(after, mesh, animation)
        expected = _expected_paths(sources)
        if set(_scope_assets()) != expected or any(not _disk_path(path).is_file() for path in expected):
            raise RuntimeError("Final RefinedV2 asset scope/persisted dependencies differ from contract")
        report = {"status": "reload_verified" if validate_only else "imported_and_applied",
            "validate_only": bool(validate_only), "source_fbx": str(SOURCE_FBX), "source_hashes": hashes,
            "surface_manifest": str(SURFACE_MANIFEST), "source_method": manifest.get("method"),
            "reused_verified_scope": reused, "core": core, "texture_count": len(textures),
            "textures": texture_report, "material_count": len(materials), "material_slots": slot_report,
            "materials": material_report, "owned_parent": PARENT_PATH, "parent_wiring": _validate_parent(parent),
            "lods": lod_report, "saved_assets": saved, "saved_actor_package": actor_package,
            "expected_asset_count": len(expected), "package_files_verified": sorted(expected),
            "guide": after, "before_checkpoint": str(CHECKPOINT), "dialogue_and_supplies_unchanged": True,
            "image_projection": False, "runtime_visual_review_required": True}
        _json(EVIDENCE / ("unreal-refined-reload.json" if validate_only else "unreal-refined-application.json"), report)
        _stage(state, "completed", report_status=report["status"])
        unreal.log("ARIN_REFINED_" + ("RELOAD_VERIFIED " if validate_only else "APPLIED ") + json.dumps(report, ensure_ascii=False))
        return report
    except Exception as exc:
        try:
            observed = sorted(_scope_assets())
        except Exception:
            observed = []
        _stage(state, "aborted_requires_inspection", error=str(exc), observed_assets=observed,
               recovery_policy="No automatic overwrite/delete; inspect this exact source hash and observed scope before a scoped recovery")
        raise


if __name__ == "__main__":
    main()
