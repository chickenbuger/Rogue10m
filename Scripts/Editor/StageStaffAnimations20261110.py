"""Stage full-duration owned Fab Staff clips; never activate gameplay assets.

Run ONLY after the coordinator grants a serial Unreal Editor slot, supplying both
    -RogueStaffStageLease=CoordinatorGranted -RogueStaffStageLabel=FreshLabel01
to the editor command line. These arguments assert a lease; they do not acquire it.
The label's asset AND evidence directories must be new. Failed candidates remain
for review. No vendor skeleton compatibility edits, DA/montage activation, clip
trimming, root-key edits, or gameplay timing edits occur here.

Manny outputs are candidates for the hidden animation playback mesh. HumanMale
outputs are offline review derivatives for the rendered body, not replacements
for that playback mesh or its existing runtime RetargetPoseFromMesh graph.
"""

from pathlib import Path
import hashlib
import json
import math
import re
import shlex
import traceback

import unreal


BASE = "/Game/Rogue10m/Animation/Combat/Staff20261110"
SOURCE_MESH = "/Game/CombatMagicAnims/Demo/Mannequins/Meshes/SKM_Manny_Simple"
MANNY_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
HUMAN_MESH = "/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody"
SOURCES = (
    ("StaffStrike", "/Game/CombatMagicAnims/Animations/AS_StaffStrike"),
    ("StaffShot", "/Game/CombatMagicAnims/Animations/AS_StaffShot"),
)
BONES = (
    "root", "pelvis", "head", "hand_l", "hand_r", "middle_01_l",
    "middle_01_r", "foot_l", "foot_r",
)
SAMPLE_HZ = 30


def command_value(arguments, name):
    prefix = "-" + name + "="
    values = [argument[len(prefix):] for argument in arguments if argument.startswith(prefix)]
    if len(values) != 1:
        raise RuntimeError("Exactly one explicit " + prefix + "value is required")
    return values[0]


def requested_label():
    # Non-posix parsing preserves Windows backslashes. Values here contain no spaces.
    arguments = [item.strip('"') for item in shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)]
    if command_value(arguments, "RogueStaffStageLease") != "CoordinatorGranted":
        raise RuntimeError("A coordinator-granted serial editor lease must be explicitly asserted")
    label = command_value(arguments, "RogueStaffStageLabel")
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,23}", label):
        raise RuntimeError("Use a fresh 1..24 character ASCII letter/digit/underscore label, starting with a letter")
    return label


def require(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing required asset: " + path)
    return asset


def asset_path(asset):
    return asset.get_path_name()


def sequence_metadata(clip):
    model = clip.get_editor_property("data_model_interface")
    rate = model.get_frame_rate()
    if rate.denominator <= 0 or rate.numerator <= 0:
        raise RuntimeError("Invalid animation sample rate: " + asset_path(clip))
    length = float(clip.get_play_length())
    if not math.isfinite(length) or length <= 0:
        raise RuntimeError("Invalid animation duration: " + asset_path(clip))
    return {
        "asset": asset_path(clip),
        "skeleton": asset_path(clip.get_editor_property("skeleton")),
        "duration_seconds": length,
        "number_of_frames": int(model.get_number_of_frames()),
        "frame_rate_numerator": int(rate.numerator),
        "frame_rate_denominator": int(rate.denominator),
        "enable_root_motion": bool(clip.get_editor_property("enable_root_motion")),
        "force_root_lock": bool(clip.get_editor_property("force_root_lock")),
    }


def package_files(content, object_path):
    package = object_path.split(".", 1)[0]
    if not package.startswith("/Game/"):
        raise RuntimeError("Expected project Content package: " + object_path)
    stem = content / package.removeprefix("/Game/")
    files = []
    for extension in (".uasset", ".uexp", ".ubulk", ".uptnl"):
        filename = stem.with_suffix(extension)
        # Refuse redirected source files as well as a redirected Content directory.
        resolved = filename.resolve()
        if not resolved.is_relative_to(content):
            raise RuntimeError("Content package resolves outside project Content: " + str(filename))
        if filename.is_file():
            files.append(filename)
    if not stem.with_suffix(".uasset").is_file():
        raise RuntimeError("Original package must already be saved: " + object_path)
    return files


def hash_files(files):
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None for path in files}


class Stage:
    def __init__(self, project, content, label, evidence):
        self.project = project
        self.content = content
        self.label = label
        self.root = BASE + "/" + label
        self.evidence = evidence
        self.created = []

    def save(self, asset):
        if not asset_path(asset).split(".", 1)[0].startswith(self.root + "/"):
            raise RuntimeError("Refusing to save any original or shared package: " + asset_path(asset))
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("Candidate save failed: " + asset_path(asset))

    def create(self, name, asset_class, factory):
        destination = self.root + "/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(destination):
            raise RuntimeError("Refusing to overwrite candidate: " + destination)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, self.root, asset_class, factory)
        if not asset:
            raise RuntimeError("Candidate creation failed: " + destination)
        self.created.append(asset_path(asset))
        return asset

    def rig(self, name, mesh):
        rig = self.create(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        controller = unreal.IKRigController.get_controller(rig)
        if (not controller.set_skeletal_mesh(mesh)
                or not controller.apply_auto_generated_retarget_definition()
                or not controller.apply_auto_fbik()):
            raise RuntimeError("Auto rig failed: " + name)
        self.save(rig)
        return rig

    def retargeter(self, name, source_mesh, target_mesh, source_rig, target_rig):
        retargeter = self.create(name, unreal.IKRetargeter, unreal.IKRetargetFactory())
        controller = unreal.IKRetargeterController.get_controller(retargeter)
        source_side = unreal.RetargetSourceOrTarget.SOURCE
        target_side = unreal.RetargetSourceOrTarget.TARGET
        controller.set_ik_rig(source_side, source_rig)
        controller.set_ik_rig(target_side, target_rig)
        controller.set_preview_mesh(source_side, source_mesh)
        controller.set_preview_mesh(target_side, target_mesh)
        controller.remove_all_ops()
        controller.add_default_ops()
        controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
        controller.reset_retarget_pose(controller.get_current_retarget_pose_name(target_side), [], target_side)
        controller.auto_align_all_bones(target_side)

        # Existing UE5.8 project policy: actual root-to-root separation and FK
        # until a body-specific FBIK pass is visually validated. Only NEW rigs change.
        roots = {}
        # Auto humanoid rigs need not contain a chain named Root. The Root Motion
        # operation takes actual skeleton bones, not retarget-chain identifiers.
        # Keep the auto-rig pelvis/retarget root unchanged and verify both meshes.
        for side, mesh in ((source_side, source_mesh), (target_side, target_mesh)):
            component = unreal.SkeletalMeshComponent()
            component.set_skeletal_mesh_asset(mesh)
            if component.get_bone_index("root") < 0:
                raise RuntimeError("Root Motion requires an actual root bone: " + asset_path(mesh))
            roots[side] = unreal.Name("root")
        root_operations = []
        ik_operations = []
        for index in range(controller.get_num_retarget_ops()):
            operation = controller.get_op_controller(index)
            if isinstance(operation, unreal.IKRetargetRootMotionController):
                root_operations.append((index, operation))
            if isinstance(operation, unreal.IKRetargetRunIKRigController):
                ik_operations.append(index)
        if len(root_operations) != 1 or len(ik_operations) != 1:
            raise RuntimeError("Unexpected UE5.8 retarget operation layout")
        root_index, root_operation = root_operations[0]
        root_operation.set_source_root_bone(roots[source_side])
        root_operation.set_target_root_bone(roots[target_side])
        controller.set_retarget_op_enabled(root_index, True)
        controller.set_retarget_op_enabled(ik_operations[0], False)
        target_chains = {str(chain.get_editor_property("chain_name"))
                         for chain in unreal.IKRigController.get_controller(target_rig).get_retarget_chains()}
        cleared = []
        for accessory in ("Tail", "Cape", "Tabard_Back", "Tabard_Front"):
            if accessory in target_chains:
                controller.set_source_chain("None", accessory)
                cleared.append(accessory)
        self.save(retargeter)
        return retargeter, {
            "asset": asset_path(retargeter),
            "source_mesh": asset_path(source_mesh),
            "target_mesh": asset_path(target_mesh),
            "source_root": str(roots[source_side]),
            "target_root": str(roots[target_side]),
            "run_ik_enabled": False,
            "unmapped_accessories": cleared,
            "status": "isolated_candidate_not_visually_validated",
        }

    def retarget(self, name, clip, source_mesh, target_mesh, retargeter, original_path, original_sha, role):
        destination = self.root + "/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(destination):
            raise RuntimeError("Refusing to overwrite retarget output: " + destination)
        inputs = unreal.IKRetargetBatchOperationInputs()
        inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(asset_path(clip))]
        inputs.source_mesh = source_mesh
        inputs.target_mesh = target_mesh
        inputs.ik_retarget_asset = retargeter
        inputs.search = clip.get_name()
        inputs.replace = name
        inputs.target_path = self.root
        inputs.use_source_path = False
        inputs.include_referenced_assets = False
        inputs.overwrite_existing_files = False
        unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        candidate = require(destination)
        self.created.append(asset_path(candidate))
        if candidate.get_editor_property("skeleton") != target_mesh.get_editor_property("skeleton"):
            raise RuntimeError("Retarget output has the wrong target skeleton: " + destination)
        source_metadata = sequence_metadata(clip)
        target_metadata = sequence_metadata(candidate)
        if abs(source_metadata["duration_seconds"] - target_metadata["duration_seconds"]) > 1.e-4:
            raise RuntimeError("Retarget changed full source duration: " + destination)
        for key in ("number_of_frames", "frame_rate_numerator", "frame_rate_denominator",
                    "enable_root_motion", "force_root_lock"):
            if source_metadata[key] != target_metadata[key]:
                raise RuntimeError("Retarget changed source sampling/root flag: " + key + "/" + destination)
        # Do not trim, rescale time, flatten root keys, or change root-motion flags.
        tags = {
            "Rogue10m.Source": original_path,
            "Rogue10m.SourceSHA256": original_sha,
            "Rogue10m.ImmediateSource": asset_path(clip),
            "Rogue10m.StageLabel": self.label,
            "Rogue10m.PlaybackRole": role,
            "Rogue10m.Status": "Staged unvalidated; full-duration retarget; no DA activation",
        }
        for key, value in tags.items():
            unreal.EditorAssetLibrary.set_metadata_tag(candidate, key, value)
        self.save(candidate)
        target_metadata.update({
            "original_source": original_path,
            "original_source_uasset_sha256": original_sha,
            "immediate_source": asset_path(clip),
            "role": role,
            "retargeter": asset_path(retargeter),
            "full_source_duration_preserved": True,
        })
        return candidate, target_metadata

    def pose_samples(self, clip, mesh):
        options = unreal.AnimPoseEvaluationOptions()
        options.optional_skeletal_mesh = mesh
        options.extract_root_motion = False
        options.incorporate_root_motion_into_pose = True
        options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
        options.set_editor_property("should_retarget", False)
        duration = float(clip.get_play_length())
        rows = []
        for index in range(math.ceil(duration * SAMPLE_HZ) + 1):
            time = min(index / SAMPLE_HZ, duration)
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, time, options)
            actual_names = list(map(str, unreal.AnimPoseExtensions.get_bone_names(pose)))
            available = {name.casefold(): name for name in actual_names}
            if not set(BONES).issubset(available):
                raise RuntimeError("Required pose bones missing: " + asset_path(clip))
            bones = {}
            for bone in BONES:
                transform = unreal.AnimPoseExtensions.get_bone_pose(pose, available[bone], unreal.AnimPoseSpaces.WORLD)
                position = [transform.translation.x, transform.translation.y, transform.translation.z]
                rotation = [transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w]
                scale = [transform.scale3d.x, transform.scale3d.y, transform.scale3d.z]
                if not all(math.isfinite(value) for value in position + rotation + scale):
                    raise RuntimeError("Nonfinite pose: " + asset_path(clip) + "/" + bone)
                bones[bone] = {"position_cm": position, "rotation_xyzw": rotation, "scale": scale}
            rows.append({"time_seconds": time, "bones": bones})
        report = {
            "asset": asset_path(clip),
            "mesh": asset_path(mesh),
            "skeleton": asset_path(clip.get_editor_property("skeleton")),
            "duration_seconds": duration,
            "sample_hz": SAMPLE_HZ,
            "pose_space": "AnimPoseSpaces.WORLD in offline mesh evaluation; not level-world coordinates",
            "evaluation": {"type": "RAW", "should_retarget": False, "extract_root_motion": False,
                           "incorporate_root_motion_into_pose": True},
            "contact_and_trim_review": "pending measured editor review; no timing defaults selected",
            "samples": rows,
        }
        filename = clip.get_name() + "-poses.json"
        (self.evidence / filename).write_text(json.dumps(report, indent=2), encoding="utf-8")
        track_count = int(clip.get_editor_property("data_model_interface").get_num_bone_tracks())
        max_hand_displacement = max(math.dist(row["bones"][hand]["position_cm"], rows[0]["bones"][hand]["position_cm"])
                                    for row in rows for hand in ("hand_l", "hand_r"))
        if track_count < 20 or max_hand_displacement < 2.0:
            raise RuntimeError("Empty/static staff motion candidate: " + asset_path(clip))
        root0 = rows[0]["bones"]["root"]["position_cm"]
        return {
            "pose_file": filename,
            "bone_track_count": track_count,
            "max_hand_displacement_cm": max_hand_displacement,
            "samples": len(rows),
            "root_xy_travel_cm": max(math.dist(row["bones"]["root"]["position_cm"][:2], root0[:2]) for row in rows),
            "foot_z_range_cm": [min(row["bones"][foot]["position_cm"][2] for row in rows for foot in ("foot_l", "foot_r")),
                                max(row["bones"][foot]["position_cm"][2] for row in rows for foot in ("foot_l", "foot_r"))],
            "hand_path_length_cm": {hand: sum(math.dist(a["bones"][hand]["position_cm"], b["bones"][hand]["position_cm"])
                                               for a, b in zip(rows, rows[1:])) for hand in ("hand_l", "hand_r")},
            "quality_pass": False,
            "meaning": "finite offline samples only; grip, contact, retarget appearance and runtime remain unvalidated",
        }


def main():
    label = requested_label()  # No filesystem/asset mutation before explicit lease+label validation.
    project = Path(unreal.Paths.project_dir()).resolve()
    content = (project / "Content").resolve()
    if content != project / "Content" or not (project / "Rogue10m.uproject").is_file():
        raise RuntimeError("Expected physical Rogue10m checkout with independent Content directory")
    root = BASE + "/" + label
    package_directory = content / root.removeprefix("/Game/")
    evidence = project / "Feature/doc/evidence/staff-animation-stage-20261110" / label
    if not package_directory.resolve().is_relative_to(content) or not evidence.resolve().is_relative_to(project):
        raise RuntimeError("Candidate/evidence directory resolves outside the physical checkout")
    if package_directory.exists() or unreal.EditorAssetLibrary.does_directory_exist(root) or evidence.exists():
        raise RuntimeError("Fresh asset and evidence label required; preserve earlier candidates")
    source_mesh, manny, human = require(SOURCE_MESH), require(MANNY_MESH), require(HUMAN_MESH)
    sources = [(name, path, require(path)) for name, path in SOURCES]
    for mesh in (source_mesh, manny, human):
        if not isinstance(mesh, unreal.SkeletalMesh) or not mesh.get_editor_property("skeleton"):
            raise RuntimeError("Expected a saved skeletal mesh with a valid skeleton")
    for _, path, clip in sources:
        if not isinstance(clip, unreal.AnimSequence) or clip.get_editor_property("skeleton") != source_mesh.get_editor_property("skeleton"):
            raise RuntimeError("Fab source clip/source mesh skeleton mismatch: " + path)
        sequence_metadata(clip)
    protected_assets = [source_mesh, manny, human] + [clip for _, _, clip in sources]
    protected_assets += [mesh.get_editor_property("skeleton") for mesh in (source_mesh, manny, human)]
    files = {filename for asset in protected_assets for filename in package_files(content, asset_path(asset))}
    # Record key shared runtime packages as an additional no-activation guard.
    for path in (
        "/Game/Character/Customization/Retargeting/IKR_Manny_To_Hu_M",
        "/Game/Character/Customization/Retargeting/ABP_Retarget_Hu_M",
        "/Game/DataAsset/Character/DA_Character_Default",
        "/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_Staff",
        "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Primary01",
        "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Primary02",
        "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Primary03",
        "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Special",
        "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Charged",
    ):
        if (content / path.removeprefix("/Game/")).with_suffix(".uasset").is_file():
            files.update(package_files(content, path))
    files = sorted(files)
    before = hash_files(files)
    evidence.mkdir(parents=True, exist_ok=False)
    stage = Stage(project, content, label, evidence)
    report = {
        "status": "staging_started_unvalidated",
        "label": label,
        "engine": unreal.SystemLibrary.get_engine_version(),
        "project_directory": str(project),
        "content_directory": str(content),
        "asset_root": root,
        "editor_lease_asserted_by_command_line": True,
        "editor_lease_acquired_by_script": False,
        "activated": False,
        "gameplay_timing_edited": False,
        "montages_or_data_assets_written": False,
        "vendor_or_global_skeletons_saved": False,
        "mesh_roles": {
            "source_owned_fab": {"mesh": asset_path(source_mesh), "skeleton": asset_path(source_mesh.get_editor_property("skeleton"))},
            "hidden_manny_playback_candidate": {"mesh": asset_path(manny), "skeleton": asset_path(manny.get_editor_property("skeleton"))},
            "rendered_human_male_offline_review": {"mesh": asset_path(human), "skeleton": asset_path(human.get_editor_property("skeleton"))},
        },
        "original_files_before_sha256": before,
        "sources": [],
        "retargeters": [],
        "candidates": [],
        "future_edit_manifest": [{"source": path, "trim_range_seconds": None, "release_time_seconds": None,
                                  "contact_time_seconds": None, "status": "pending measured editor review"} for _, path in SOURCES],
        "limitations": [
            "No rendered captures, physical input, full-run play, montage slot/blend or runtime retarget validation.",
            "No clip trimming, source replacement, root-key editing, arbitrary release timing or hit-delay selection.",
            "Finite pose and duration checks do not establish visible animation quality or weapon grip/contact correctness.",
            "Human derivatives are offline review assets; live playback remains on the hidden Manny source until separately approved activation.",
        ],
    }
    manifest = evidence / "animation-stage.json"
    manifest.write_text(json.dumps(report, indent=2), encoding="utf-8")
    failure = None
    try:
        if not unreal.EditorAssetLibrary.make_directory(root):
            raise RuntimeError("Could not create fresh candidate folder: " + root)
        magic_rig = stage.rig("IK_Staff_MagicSource", source_mesh)
        manny_rig = stage.rig("IK_Staff_Manny", manny)
        human_rig = stage.rig("IK_Staff_HumanMale", human)
        to_manny, metadata = stage.retargeter("RTG_Staff_Magic_Manny", source_mesh, manny, magic_rig, manny_rig)
        report["retargeters"].append(metadata)
        to_human, metadata = stage.retargeter("RTG_Staff_Manny_HumanMale", manny, human, manny_rig, human_rig)
        report["retargeters"].append(metadata)
        for name, path, original in sources:
            original_file = (content / path.removeprefix("/Game/")).with_suffix(".uasset")
            original_sha = before[str(original_file)]
            source_metadata = sequence_metadata(original)
            source_metadata.update(stage.pose_samples(original, source_mesh))
            report["sources"].append(source_metadata)
            playback, metadata = stage.retarget("A_Staff_" + name + "_Manny", original, source_mesh, manny,
                                               to_manny, path, original_sha, "hidden_manny_playback_candidate")
            metadata.update(stage.pose_samples(playback, manny))
            report["candidates"].append(metadata)
            review, metadata = stage.retarget("A_Staff_" + name + "_HumanMale", playback, manny, human,
                                             to_human, path, original_sha, "rendered_human_male_offline_review_only")
            metadata.update(stage.pose_samples(review, human))
            report["candidates"].append(metadata)
        report["status"] = "staged_unvalidated_not_activated"
    except Exception as exception:
        failure = exception
        report["status"] = "failed_partial_candidates_preserved_not_activated"
        report["failure"] = traceback.format_exc()
    after = {}
    unverifiable = {}
    for filename in files:
        try:
            after.update(hash_files([filename]))
        except Exception as exception:
            unverifiable[str(filename)] = str(exception)
    changed = [filename for filename, digest in before.items()
               if filename not in unverifiable and after.get(filename) != digest]
    report["original_files_after_sha256"] = after
    report["original_files_changed"] = changed
    report["original_files_unverifiable"] = unverifiable
    report["original_files_preserved"] = not changed and not unverifiable
    report["created_assets"] = stage.created
    if changed:
        report["status"] = "failed_original_preservation_check"
        failure = failure or RuntimeError("Original/shared package changed: " + ", ".join(changed))
    if unverifiable:
        report["status"] = "failed_original_preservation_check_incomplete"
        failure = failure or RuntimeError("Could not recheck original/shared packages: " + ", ".join(unverifiable))
    manifest.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if failure:
        raise failure
    unreal.log("RESULT=STAFF_ANIMATION_STAGED_UNVALIDATED candidates=4 activated=false evidence=" + str(manifest))


if __name__ == "__main__":
    main()
