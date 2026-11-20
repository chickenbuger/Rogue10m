"""Import the common skill web's registered medallions and approved charcoal texture."""
from pathlib import Path

import unreal


DESTINATION = "/Game/UI/CommonSkillTree"
SOURCE = Path(__file__).resolve().parents[2] / "Art/UI/CommonSkillTree"
TEXTURES = {"T_CommonSkillMedallions": (1024, 1024), "T_CommonSkillCharcoal": (256, 256)}


def import_texture(name, expected_size):
    path = f"{DESTINATION}/{name}"
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.filename = str(SOURCE / f"{name}.png")
        task.destination_path = DESTINATION
        task.destination_name = name
        task.automated = True
        task.replace_existing = False
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Failed to load imported common skill art {path}")
    if (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) != expected_size:
        raise RuntimeError(f"Unexpected common skill texture dimensions: {path}")

    texture.modify()
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {path}")
    unreal.log(f"Common skill art verified: {path} {expected_size[0]}x{expected_size[1]}")


def main():
    for name, dimensions in TEXTURES.items():
        import_texture(name, dimensions)
    unreal.log("RESULT=COMMON_SKILL_TREE_ART_IMPORTED")


if __name__ == "__main__":
    main()
