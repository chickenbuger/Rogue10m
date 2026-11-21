"""Author the project-owned, vertex-colored material used by melee VFX."""
import unreal


FOLDER = "/Game/Rogue10m/VFX/Character/Combat/Melee20261121"
NAME = "M_MeleeReadability"
TAG = "Rogue10m.MeleeVfxMaterial"
VERSION = "1"


def main():
    assets = unreal.EditorAssetLibrary
    graph = unreal.MaterialEditingLibrary
    path = f"{FOLDER}/{NAME}"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if not isinstance(material, unreal.Material) or assets.get_metadata_tag(material, TAG) != VERSION:
            raise RuntimeError(f"Unexpected existing material at {path}")
        print(f"RESULT=MELEE_VFX_MATERIAL_EXISTS path={material.get_path_name()}")
        return

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew()
    )
    if not material:
        raise RuntimeError(f"Cannot create {path}")
    material.set_editor_properties({
        "blend_mode": unreal.BlendMode.BLEND_ADDITIVE,
        "shading_model": unreal.MaterialShadingModel.MSM_UNLIT,
        "two_sided": True,
    })
    colors = graph.create_material_expression(material, unreal.MaterialExpressionVertexColor, -460, 0)
    strength = graph.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -460, 150)
    strength.set_editor_properties({"parameter_name": "EmissiveStrength", "default_value": 4.0})
    glow = graph.create_material_expression(material, unreal.MaterialExpressionMultiply, -175, 0)
    connected = (
        graph.connect_material_expressions(colors, "", glow, "A"),
        graph.connect_material_expressions(strength, "", glow, "B"),
        graph.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR),
        graph.connect_material_property(colors, "A", unreal.MaterialProperty.MP_OPACITY),
    )
    if not all(connected):
        raise RuntimeError(f"Cannot connect {path} material outputs")
    issues = list(graph.recompile_material(material) or [])
    if issues:
        raise RuntimeError(f"Material compilation failed: {issues}")
    assets.set_metadata_tag(material, TAG, VERSION)
    if not assets.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Cannot save {path}")
    print(f"RESULT=MELEE_VFX_MATERIAL_AUTHORED path={material.get_path_name()}")


if __name__ == "__main__":
    main()
