"""Import and validate the gothic monster-info frame as a UI Texture2D."""

from __future__ import annotations

import os
import unreal


SOURCE_FILE = "D:/Project/Rogue10m/Content/UI/HUD/Gothic/T_HUD_GothicMonsterInfoFrame.png"
DESTINATION_PATH = "/Game/UI/HUD/Gothic"
ASSET_NAME = "T_HUD_GothicMonsterInfoFrame"
ASSET_PATH = f"{DESTINATION_PATH}/{ASSET_NAME}"
OUTPUT_PATH = "D:/Project/Rogue10m/Saved/GothicMonsterInfoFrameValidation.txt"


def configure(texture) -> None:
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.modify()


def import_texture():
    texture = (
        unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
        if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH)
        else None
    )
    if not texture:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", SOURCE_FILE)
        task.set_editor_property("destination_path", DESTINATION_PATH)
        task.set_editor_property("destination_name", ASSET_NAME)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if not texture:
        raise RuntimeError(f"Texture import failed: {ASSET_PATH}")
    return texture


def main() -> None:
    if not os.path.exists(SOURCE_FILE):
        raise RuntimeError(f"Source PNG missing: {SOURCE_FILE}")

    texture = import_texture()
    configure(texture)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError(f"Texture save failed: {ASSET_PATH}")

    width = texture.blueprint_get_size_x()
    height = texture.blueprint_get_size_y()
    lines = [
        f"asset={ASSET_PATH}",
        f"size={width}x{height}",
        f"lod_group={texture.get_editor_property('lod_group')}",
        f"never_stream={texture.get_editor_property('never_stream')}",
    ]
    if width != 840 or height != 136:
        lines.append("RESULT=FAILED")
        raise RuntimeError(f"Unexpected imported texture size: {width}x{height}")
    lines.append("RESULT=PASSED")

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
        output_file.write("\n".join(lines))
    for line in lines:
        unreal.log(f"[Rogue10mMonsterInfoFrame] {line}")


if __name__ == "__main__":
    main()
