"""Create an isolated Manny Martelo preview animation with UE 5.8 IK retargeting.

The supplied animation-only FBX is preserved. source_bind_proxy.cpp adds a tiny
weighted triangle per source bone solely to import its reference skeleton. It
uses FBXSDK_TIME_INFINITE (static reference transforms), not animation frame 0.
All created/saved assets stay under Animation/Preview/Martelo.
"""
from pathlib import Path
import json
import hashlib
import unreal

ROOT = "/Game/Rogue10m/Animation/Preview/Martelo"
SOURCE_ROOT = ROOT + "/Source/Persistent"
TARGET_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
OUTPUT_ANIMATION = ROOT + "/A_Martelo2_Manny"
PROJECT = Path(unreal.Paths.project_dir())
SOURCE_FBX = PROJECT / "tmp/martelo-game/source_martelo_bind.fbx"
ORIGINAL_FBX = Path("C:/Users/PC/Downloads/Martelo 2.fbx")
SOURCE_MESH_PATH = SOURCE_ROOT + "/SK_MarteloSource"
SOURCE_ANIMATION_PATH = SOURCE_ROOT + "/SK_MarteloSource_Anim"
SOURCE_SKELETON_PATH = SOURCE_ROOT + "/SK_MarteloSource_Skeleton"


def source_hashes():
    return {
        "Martelo.OriginalSHA256": hashlib.sha256(ORIGINAL_FBX.read_bytes()).hexdigest(),
        "Martelo.ProxySHA256": hashlib.sha256(SOURCE_FBX.read_bytes()).hexdigest(),
    }


def validate_source(mesh, animation):
    skeleton = mesh.get_editor_property("skeleton")
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError("Source mesh has no persistent skeleton")
    if skeleton.get_path_name().split(".")[0] != SOURCE_SKELETON_PATH:
        raise RuntimeError("Unexpected source skeleton path: " + skeleton.get_path_name())
    if animation.get_editor_property("skeleton") != skeleton:
        raise RuntimeError("Source animation and mesh skeletons differ")
    if not 1.25 <= animation.get_editor_property("sequence_length") <= 1.35:
        raise RuntimeError("Unexpected source animation duration")
    return skeleton


def require(path):
    result = unreal.EditorAssetLibrary.load_asset(path)
    if not result:
        raise RuntimeError("Missing asset: " + path)
    return result


def asset(name, cls, factory):
    path = ROOT + "/" + name
    result = unreal.EditorAssetLibrary.load_asset(path)
    if not result:
        result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, cls, factory)
    if not isinstance(result, cls):
        raise RuntimeError("Asset creation/type failed: " + path)
    return result


def save(value):
    if not value.get_path_name().startswith(ROOT + "/"):
        raise RuntimeError("Refusing to save shared asset: " + value.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(value, only_if_is_dirty=False):
        raise RuntimeError("Save failed: " + value.get_path_name())


def import_source():
    if not SOURCE_FBX.is_file():
        raise RuntimeError("Build/run tmp/martelo-game/source_bind_proxy.cpp first")
    hashes = source_hashes()
    expected = (SOURCE_MESH_PATH, SOURCE_ANIMATION_PATH, SOURCE_SKELETON_PATH)
    exists = [unreal.EditorAssetLibrary.does_asset_exist(path) for path in expected]
    if all(exists):
        mesh = require(SOURCE_MESH_PATH)
        animation = require(SOURCE_ANIMATION_PATH)
        validate_source(mesh, animation)
        for tag, digest in hashes.items():
            if unreal.EditorAssetLibrary.get_metadata_tag(mesh, tag) != digest:
                raise RuntimeError("Source FBX changed or cache provenance missing: " + tag)
        unreal.log("MARTELO_SOURCE_REUSED: persistent source assets and both FBX hashes verified")
        return mesh, animation
    if any(exists):
        raise RuntimeError("Partial source import found; refusing to overwrite persistent source assets")
    options = unreal.FbxImportUI()
    for key, value in {
        "automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "original_import_type": unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "import_as_skeletal": True, "import_mesh": True,
        "import_animations": True, "import_materials": False,
        "import_textures": False, "create_physics_asset": False,
    }.items():
        options.set_editor_property(key, value)
    mesh_options = options.get_editor_property("skeletal_mesh_import_data")
    mesh_options.set_editor_property("use_t0_as_ref_pose", False)
    mesh_options.set_editor_property("update_skeleton_reference_pose", False)
    mesh_options.set_editor_property("import_morph_targets", False)
    anim_options = options.get_editor_property("anim_sequence_import_data")
    anim_options.set_editor_property("use_default_sample_rate", True)
    anim_options.set_editor_property("remove_redundant_keys", False)
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE_FBX)
    task.destination_path = SOURCE_ROOT
    task.destination_name = "SK_MarteloSource"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = [unreal.EditorAssetLibrary.load_asset(p) for p in task.imported_object_paths]
    meshes = [v for v in imported if isinstance(v, unreal.SkeletalMesh)]
    animations = [v for v in imported if isinstance(v, unreal.AnimSequence)]
    if not meshes or not animations:
        # Some importer builds report only the primary asset in imported_object_paths.
        imported = [unreal.EditorAssetLibrary.load_asset(p) for p in
                    unreal.EditorAssetLibrary.list_assets(SOURCE_ROOT, recursive=True)]
        meshes = [v for v in imported if isinstance(v, unreal.SkeletalMesh)]
        animations = [v for v in imported if isinstance(v, unreal.AnimSequence)]
    if len(meshes) != 1 or len(animations) != 1:
        raise RuntimeError(f"Expected one source mesh/animation, found {len(meshes)}/{len(animations)}")
    mesh, animation = meshes[0], animations[0]
    skeleton = validate_source(mesh, animation)
    # AssetImportTask.save persists only its primary mesh in this UE version.
    # Explicitly save every generated dependency before the editor can exit.
    for tag, digest in hashes.items():
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, tag, digest)
    save(skeleton)
    save(animation)
    save(mesh)
    for path in expected:
        relative = path.removeprefix("/Game/") + ".uasset"
        if not (PROJECT / "Content" / relative).is_file():
            raise RuntimeError("Imported dependency was not persisted: " + path)
    return mesh, animation


def make_rig(name, mesh):
    rig = asset(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)
    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError("Cannot assign rig mesh: " + name)
    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError("Known humanoid template not found: " + name)
    # FK chains retain the full high kick; FBIK provides UE's standard limb goals.
    if not controller.apply_auto_fbik():
        raise RuntimeError("Cannot create standard humanoid IK: " + name)
    save(rig)
    return rig, controller


def main():
    unreal.EditorAssetLibrary.make_directory(ROOT)
    source_mesh, source_animation = import_source()
    target_mesh = require(TARGET_MESH)
    source_rig, source_controller = make_rig("IK_MarteloSource", source_mesh)
    target_rig, target_controller = make_rig("IK_MarteloManny", target_mesh)
    retargeter = asset("RTG_MarteloSource_Manny", unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    source = unreal.RetargetSourceOrTarget.SOURCE
    target = unreal.RetargetSourceOrTarget.TARGET
    controller.set_ik_rig(source, source_rig)
    controller.set_ik_rig(target, target_rig)
    controller.set_preview_mesh(source, source_mesh)
    controller.set_preview_mesh(target, target_mesh)
    # Mirror UE's procedural retarget asset setup so rerunning this script does
    # not accumulate operations or apply alignment on top of a previous pose.
    controller.remove_all_ops()
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    target_pose = controller.get_current_retarget_pose_name(target)
    controller.reset_retarget_pose(target_pose, [], target)
    controller.auto_align_all_bones(target)
    save(retargeter)
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(source_animation.get_path_name())]
    inputs.source_mesh = source_mesh
    inputs.target_mesh = target_mesh
    inputs.ik_retarget_asset = retargeter
    inputs.search = source_animation.get_name()
    inputs.replace = "A_Martelo2_Manny"
    inputs.target_path = ROOT
    inputs.use_source_path = False
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    animation = require(OUTPUT_ANIMATION)
    if not isinstance(animation, unreal.AnimSequence):
        raise RuntimeError("Result is not AnimSequence")
    if animation.get_editor_property("skeleton") != target_mesh.get_editor_property("skeleton"):
        raise RuntimeError("Result skeleton does not match Manny")
    duration = animation.get_editor_property("sequence_length")
    if not 1.25 <= duration <= 1.35:
        raise RuntimeError(f"Unexpected retargeted duration: {duration}")
    animation.set_editor_property("enable_root_motion", False)
    save(animation)
    report = {
        "animation": animation.get_path_name(), "duration": duration,
        "source_mesh": source_mesh.get_path_name(),
        "source_animation": source_animation.get_path_name(),
        "target_mesh": target_mesh.get_path_name(),
        "source_root": str(source_controller.get_retarget_root()),
        "target_root": str(target_controller.get_retarget_root()),
        "source_chains": [str(c.chain_name) for c in source_controller.get_retarget_chains()],
        "target_chains": [str(c.chain_name) for c in target_controller.get_retarget_chains()],
        "retargeted_assets": len(results), "uses_t0_reference_pose": False,
        "source_hashes": source_hashes(),
        "source_dependencies_explicitly_saved": True,
    }
    (PROJECT / "tmp/martelo-game/import-report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("MARTELO_RETARGET_READY " + json.dumps(report))
    return animation


if __name__ == "__main__":
    main()
