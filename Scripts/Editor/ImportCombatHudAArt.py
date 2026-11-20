"""Import the approved Layout A atlas as an uncompressed Unreal UI texture."""

from pathlib import Path
from struct import unpack

import unreal


SOURCE = Path(__file__).resolve().parents[2] / "SourceArt/UI/CombatHudA/T_HudA_Atlas.png"
DESTINATION = "/Game/UI/HUD/CombatHudA"


def main():
    with SOURCE.open("rb") as source:
        signature, chunk_size, chunk_type, width, height, depth, color = unpack(">8sI4sIIBB", source.read(26))
    if (signature, chunk_size, chunk_type, depth, color) != (b"\x89PNG\r\n\x1a\n", 13, b"IHDR", 8, 6):
        raise RuntimeError(f"Expected an 8-bit RGBA PNG: {SOURCE}")

    task = unreal.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = DESTINATION
    task.destination_name = SOURCE.stem
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{DESTINATION}/{SOURCE.stem}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D) or not task.imported_object_paths:
        raise RuntimeError(f"Atlas import failed: {asset_path}")
    if (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) != (width, height):
        raise RuntimeError(f"Unreal atlas dimensions differ from the source: {asset_path}")

    texture.modify()
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError(f"Unable to save {asset_path}")
    unreal.log(f"RESULT=COMBAT_HUD_A_ATLAS_IMPORTED {asset_path} {width}x{height}")


if __name__ == "__main__":
    main()
