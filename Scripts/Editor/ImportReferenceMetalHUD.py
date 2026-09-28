"""Import the generated, unmodified RGBA frame with UE UI texture settings."""
from pathlib import Path
import unreal

def main():
    name = "T_HUD_ReferenceFrame"
    destination = "/Game/UI/HUD/ReferenceMetal"
    path = destination + "/" + name
    asset = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not asset:
        task = unreal.AssetImportTask()
        task.filename = str(Path(__file__).resolve().parents[2] / "Content/UI/HUD/ReferenceMetal/T_HUD_ReferenceFrame.png")
        task.destination_path = destination
        task.destination_name = name
        task.automated = True
        task.replace_existing = False
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Reference frame import failed")
    asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    asset.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    asset.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    asset.set_editor_property("srgb", True)
    asset.set_editor_property("never_stream", True)
    asset.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    asset.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Reference frame save failed")
    if (asset.blueprint_get_size_x(), asset.blueprint_get_size_y()) != (1920, 819):
        raise RuntimeError("Unexpected reference frame atlas dimensions")
    unreal.log("RESULT=REFERENCE_FRAME_IMPORTED")

if __name__ == "__main__":
    main()
