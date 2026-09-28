"""Enable confirmed-hit cues on four dedicated brawler assets; create an isolated flash material."""
import unreal
ROOT = "/Game/DataAsset/AttackSkill/BasicBrawler"
VFX_ROOT = "/Game/Rogue10m/VFX/Character/Combat/Unarmed"
MATERIAL_PATH = VFX_ROOT + "/M_BrawlerHitFlash"
STRENGTHS = {"LeftJab": 0.22, "RightJab": 0.22, "RightStraight": 0.28, "RightHook": 0.36}

def require(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset

def main():
    impact = require(VFX_ROOT + "/NS_Punch_Impact")
    material = unreal.load_asset(MATERIAL_PATH)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_BrawlerHitFlash", VFX_ROOT, unreal.Material, unreal.MaterialFactoryNew())
        if not material:
            raise RuntimeError("Cannot create brawler flash material")
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("two_sided", True)
        color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property("constant", unreal.LinearColor(2.0, 1.3, 0.55, 1.0))
        opacity = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
        opacity.set_editor_property("parameter_name", "HitOpacity")
        opacity.set_editor_property("default_value", 0.0)
        unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
        unreal.MaterialEditingLibrary.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    for name, scale in STRENGTHS.items():
        skill = require(ROOT + "/DA_BasicBrawler_" + name)
        skill.set_editor_properties({
            "enable_attack_effects": True,
            "impact_effect": impact,
            "impact_effect_scale": scale,
            "impact_effect_emission_duration": 0.10 if name != "RightHook" else 0.14,
            # Confirmed impact only: the animation silhouette remains readable on misses.
            "cast_effect": None,
            "charge_effect": None,
            "first_person_impact_scale_multiplier": 0.72,
        })
        unreal.EditorAssetLibrary.save_loaded_asset(skill, only_if_is_dirty=False)
        if not skill.get_editor_property("enable_attack_effects") or skill.get_editor_property("impact_effect") != impact:
            raise RuntimeError("Impact settings failed: " + name)
        if skill.get_editor_property("cast_effect") or skill.get_editor_property("charge_effect"):
            raise RuntimeError("Unexpected non-impact effect: " + name)
    if material.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_TRANSLUCENT:
        raise RuntimeError("Brawler flash material is not translucent")
    unreal.log("RESULT=BRAWLER_HIT_FEEDBACK_ASSETS_PASSED skills=4 material=M_BrawlerHitFlash impact=NS_Punch_Impact")

if __name__ == "__main__":
    main()
