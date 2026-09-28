"""Import the actual Boxing.fbx animated stack and bake it to Manny in an isolated preview folder."""
from pathlib import Path
import json
import hashlib
import unreal

ROOT = "/Game/Rogue10m/Animation/Preview/Boxing"
SOURCE_ROOT = ROOT + "/Source/Persistent"
TARGET_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
OUTPUT_ANIMATION = ROOT + "/A_Boxing_Manny"
PROJECT = Path(unreal.Paths.project_dir())
SOURCE_FBX = Path("C:/Users/PC/Downloads/Boxing.fbx")
ORIGINAL_FBX = SOURCE_FBX
SOURCE_MESH_PATH = SOURCE_ROOT + "/SK_BoxingSource"
SOURCE_ANIMATION_PATH = SOURCE_ROOT + "/SK_BoxingSource_Anim"
SOURCE_SKELETON_PATH = SOURCE_ROOT + "/SK_BoxingSource_Skeleton"


def source_hashes():
    return {
        "Boxing.OriginalSHA256": hashlib.sha256(ORIGINAL_FBX.read_bytes()).hexdigest(),
    }


def validate_source(mesh, animation):
    skeleton = mesh.get_editor_property("skeleton")
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError("Source mesh has no persistent skeleton")
    if skeleton.get_path_name().split(".")[0] != SOURCE_SKELETON_PATH:
        raise RuntimeError("Unexpected source skeleton path: " + skeleton.get_path_name())
    if animation.get_editor_property("skeleton") != skeleton:
        raise RuntimeError("Source animation and mesh skeletons differ")
    if not 1.70 <= animation.get_editor_property("sequence_length") <= 1.77:
        raise RuntimeError("Unexpected source animation duration")
    return skeleton


def require(path):
    result = unreal.EditorAssetLibrary.load_asset(path)
    if not result:
        raise RuntimeError("Missing asset: " + path)
    return result


def asset(name, cls, factory):
    path = ROOT + "/" + name
    result = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
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
        raise RuntimeError("Missing original Boxing.fbx")
    hashes = source_hashes()
    paths = unreal.EditorAssetLibrary.list_assets(SOURCE_ROOT, recursive=True) if unreal.EditorAssetLibrary.does_directory_exist(SOURCE_ROOT) else []
    if not paths:
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
        task.destination_name = "SK_BoxingSource"
        task.automated = True
        task.replace_existing = False
        task.save = True
        task.options = options
        task.factory = unreal.FbxFactory()
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        paths = unreal.EditorAssetLibrary.list_assets(SOURCE_ROOT, recursive=True)
        imported = [unreal.EditorAssetLibrary.load_asset(p) for p in paths]
        for value in imported:
            for tag, digest in hashes.items():
                unreal.EditorAssetLibrary.set_metadata_tag(value, tag, digest)
            save(value)
    else:
        imported = [unreal.EditorAssetLibrary.load_asset(p) for p in paths]
        for value in imported:
            for tag, digest in hashes.items():
                if unreal.EditorAssetLibrary.get_metadata_tag(value, tag) != digest:
                    raise RuntimeError("Source provenance missing or changed: " + value.get_path_name())
    meshes = [v for v in imported if isinstance(v, unreal.SkeletalMesh)]
    animations = [v for v in imported if isinstance(v, unreal.AnimSequence)]
    report = [{"asset":v.get_path_name(),"duration":v.get_play_length()} for v in animations]
    unreal.log("BOXING_IMPORTED_ANIMATIONS " + json.dumps(report))
    selected = [v for v in animations if 1.70 <= v.get_play_length() <= 1.77]
    if len(meshes) != 1 or len(selected) != 1:
        raise RuntimeError(f"Expected one source mesh and one active 1.733s clip, got {len(meshes)}/{len(selected)}: {report}")
    mesh, animation = meshes[0], selected[0]
    validate_source(mesh, animation)
    return mesh, animation


def make_rig(name, mesh):
    rig = asset(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)
    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError("Cannot assign rig mesh: " + name)
    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError("Known humanoid template not found: " + name)
    # Retain the source full-body stepping punch through standard humanoid goals.
    if not controller.apply_auto_fbik():
        raise RuntimeError("Cannot create standard humanoid IK: " + name)
    save(rig)
    return rig, controller


def main():
    unreal.EditorAssetLibrary.make_directory(ROOT)
    source_mesh, source_animation = import_source()
    target_mesh = require(TARGET_MESH)
    source_rig, source_controller = make_rig("IK_BoxingSource", source_mesh)
    target_rig, target_controller = make_rig("IK_BoxingManny", target_mesh)
    retargeter = asset("RTG_BoxingSource_Manny", unreal.IKRetargeter, unreal.IKRetargetFactory())
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
    inputs.replace = "A_Boxing_Manny"
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
    if not 1.70 <= duration <= 1.77:
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
    evidence = PROJECT / "Feature/doc/evidence/boxing-direct-head-20260922"
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / "import-report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    validate_output(animation, target_mesh, evidence)
    unreal.log("BOXING_RETARGET_READY " + json.dumps(report))
    return animation


def validate_output(sequence, mesh, evidence):
    import math
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("optional_skeletal_mesh", mesh)
    names = ["root","pelvis","head","neck_01","upperarm_l","lowerarm_l","hand_l","upperarm_r","lowerarm_r","hand_r","thigh_l","calf_l","foot_l","thigh_r","calf_r","foot_r"]
    rows = []
    for frame in range(53):
        time = min(frame / 30.0, sequence.get_play_length())
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
        available = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
        if any(n not in available for n in names):
            raise RuntimeError("Missing required retarget bone")
        row = {"frame":frame,"time":time,"bones":{}}
        for name in names:
            tr = unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.WORLD)
            p,q,scale = tr.translation,tr.rotation,tr.scale3d
            values = [p.x,p.y,p.z,q.x,q.y,q.z,q.w,scale.x,scale.y,scale.z]
            if not all(math.isfinite(v) for v in values):
                raise RuntimeError("Nonfinite output transform")
            row["bones"][name] = {"position":[p.x,p.y,p.z],"quaternion":[q.x,q.y,q.z,q.w],"scale":[scale.x,scale.y,scale.z]}
        rows.append(row)
    ranges = {name:[max(row["bones"][name]["position"][i] for row in rows)-min(row["bones"][name]["position"][i] for row in rows) for i in range(3)] for name in ["pelvis","head","hand_l","hand_r","foot_l","foot_r"]}
    if max(ranges["hand_l"]) < 20 or max(ranges["pelvis"]) < 15:
        raise RuntimeError("Source punch or pelvis progression was lost: " + str(ranges))
    report = {"animation":sequence.get_path_name(),"duration":sequence.get_play_length(),"frame_count":53,"ranges_cm":ranges,"samples":rows}
    (evidence/"retarget-poses.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    unreal.log("BOXING_RETARGET_POSES_PASSED " + json.dumps(ranges))


if __name__ == "__main__":
    try:
        main()
    finally:
        unreal.SystemLibrary.quit_editor()
