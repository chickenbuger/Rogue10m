"""Create an arms-only Manny asset through Unreal Editor APIs and the C++ authoring helper.

Run after compiling the Editor target. This saves only the dedicated arms asset.
Original mesh, skeleton, animation assets and material instances are never saved.
"""
from pathlib import Path
import unreal

SOURCE = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
TARGET = "/Game/Rogue10m/Character/SK_FirstPersonArms"


def main():
    source = unreal.load_asset(SOURCE)
    if not isinstance(source, unreal.SkeletalMesh):
        raise RuntimeError(f"Missing source skeletal mesh: {SOURCE}")
    character_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
    if character_class is None:
        raise RuntimeError("Missing player Blueprint class")
    character_cdo = unreal.get_default_object(character_class)
    first_person_mesh = character_cdo.get_editor_property("first_person_mesh")
    actual_source = first_person_mesh.get_skeletal_mesh_asset() if first_person_mesh else None
    if actual_source != source:
        actual_path = actual_source.get_path_name() if actual_source else "None"
        raise RuntimeError(f"Player CDO first-person mesh is {actual_path}; expected {SOURCE}")
    unreal.log(f"FIRST_PERSON_ARMS_SOURCE_CDO mesh={actual_source.get_path_name()}")
    target = unreal.load_asset(TARGET)
    if target is None:
        target = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
    if not isinstance(target, unreal.SkeletalMesh) or target == source:
        raise RuntimeError("Expected a separate arms skeletal mesh")
    if target.get_editor_property("skeleton") != source.get_editor_property("skeleton"):
        raise RuntimeError("Target skeleton differs from source; refusing to edit")
    result = Path(unreal.Paths.project_dir()) / "tmp/arms-only/asset-result.txt"
    result.parent.mkdir(parents=True, exist_ok=True)
    result.write_text("FIRST_PERSON_ARMS_FAILED: command did not execute\n", encoding="utf-8")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, "Rogue10m.CreateFirstPersonArms")
    report = result.read_text(encoding="utf-8-sig")
    if "FIRST_PERSON_ARMS_PASSED" not in report:
        raise RuntimeError(f"Arms geometry validation failed: {report}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False):
        raise RuntimeError("Failed to save the arms-only asset")
    unreal.log(report)
    unreal.log(f"RESULT=FIRST_PERSON_ARMS_SAVED asset={target.get_path_name()}")


if __name__ == "__main__":
    main()
