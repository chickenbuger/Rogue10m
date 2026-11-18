"""Create a new owned capsule-expanded basic-cone warning; no monster or vendor asset edits.

Root invokes after the native rebuild and owns the separate boss DA/variant connection.
Existing owned box/circle warning materials cannot represent the padded cone boundary.
This derivative reuses their WarningColor/Opacity/Progress/Glow parameter contract and
engine UV/material nodes. No Fab acquisition, downloads or licences are involved.
"""
import hashlib
import json
from pathlib import Path
import unreal

MATERIAL_PATH = "/Game/Rogue10m/World/OpenWorld/Monsters/Azathor/M_Azathor_BasicConeWarning_1103"
HLSL = r"""
// DeferredDecal.usf swizzles .zyx: UV.x is projector local Z (forward), UV.y local Y (right).
float R = max(0.0, TargetCapsuleRadius);
float Extent = max(0.01, AttackRange) + R;
float2 P = (UV - 0.5) * (2.0 * Extent);
float D = length(P);
float Padding = asin(saturate(R / max(max(D, R), 1.e-8)));
float HalfAngle = radians(clamp(ArcDegrees, 1.0, 180.0) * 0.5) + Padding;
float ForwardDot = D * D > 1.e-8 ? P.x / D : 0.0;
return (D <= Extent && ForwardDot >= cos(HalfAngle)) ? 1.0 : 0.0;
"""


def build_basic_material(path=MATERIAL_PATH):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError("Preserve existing basic material; inspect it before any retry: " + path)
    folder, name = path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Could not create owned basic warning material")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    lib = unreal.MaterialEditingLibrary

    def node(klass, **properties):
        value = lib.create_material_expression(material, klass, -600, 0)
        if not value:
            raise RuntimeError("Cannot create " + str(klass))
        for key, item in properties.items():
            value.set_editor_property(key, item)
        return value

    def connect(a, b, name):
        if not lib.connect_material_expressions(a, "", b, name):
            raise RuntimeError("Cannot connect input " + name)

    def scalar(name, value):
        return node(unreal.MaterialExpressionScalarParameter,
                    parameter_name=name, default_value=value, group="Boss Warning")

    def multiply(a, b):
        result = node(unreal.MaterialExpressionMultiply)
        connect(a, result, "A"); connect(b, result, "B")
        return result

    cone = node(unreal.MaterialExpressionCustom)
    custom_inputs = []
    for name in ("UV", "AttackRange", "TargetCapsuleRadius", "ArcDegrees"):
        value = unreal.CustomInput()
        value.set_editor_property("input_name", name)
        custom_inputs.append(value)
    cone.set_editor_property("inputs", custom_inputs)
    cone.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    cone.set_editor_property("code", HLSL)
    cone.set_editor_property("description", "Actual locked basic geometry + selected capsule")
    connect(node(unreal.MaterialExpressionTextureCoordinate), cone, "UV")
    connect(scalar("AttackRange", 300.0), cone, "AttackRange")
    connect(scalar("TargetCapsuleRadius", 42.0), cone, "TargetCapsuleRadius")
    connect(scalar("ArcDegrees", 100.0), cone, "ArcDegrees")

    color = node(unreal.MaterialExpressionVectorParameter, parameter_name="WarningColor",
                 default_value=unreal.LinearColor(1.0, 0.62, 0.025, 1.0), group="Boss Warning")
    # Full warning is present from the first frame; progress affects brightness, not its footprint.
    progress = multiply(scalar("Progress", 0.0), node(unreal.MaterialExpressionConstant, r=0.15))
    brightness = node(unreal.MaterialExpressionAdd)
    connect(progress, brightness, "A")
    connect(node(unreal.MaterialExpressionConstant, r=0.85), brightness, "B")
    emissive = multiply(color, multiply(scalar("Glow", 3.5), brightness))
    opacity = multiply(cone, scalar("Opacity", 0.72))
    for expression, prop in ((opacity, unreal.MaterialProperty.MP_OPACITY),
                             (emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                             (color, unreal.MaterialProperty.MP_BASE_COLOR),
                             (node(unreal.MaterialExpressionConstant, r=0.8), unreal.MaterialProperty.MP_ROUGHNESS)):
        if not lib.connect_material_property(expression, "", prop):
            raise RuntimeError("Cannot connect material output " + str(prop))
    lib.layout_material_expressions(material)
    errors = list(lib.recompile_material(material))
    if errors:
        raise RuntimeError("Basic warning compile errors: " + str(errors))
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
        raise RuntimeError("Basic warning save result is uncertain; read back before retry")
    return material


def main():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    report = project / "Feature/doc/evidence/boss-readability-20261103/basic-material-authoring.json"
    if report.exists():
        raise RuntimeError("Existing material authoring report must be preserved")
    report.parent.mkdir(parents=True, exist_ok=True)
    material = build_basic_material()
    result = {
        "status": "saved_material_only_boss_connection_is_root_owned",
        "material": material.get_path_name(),
        "shape": "same_native_capsule_expanded_cone_predicate",
        "projection": "UV.x_forward_UV.y_right; narrow phase only; physical overlap, LOS and selected-target gates remain",
        "hlsl_sha256": hashlib.sha256(HLSL.encode()).hexdigest(),
        "source_decision": "Owned existing warning box/circle cannot express this padded cone; engine nodes and same parameter contract reused.",
        "tuning_candidate_not_automatically_applied": {
            "attack_windup_seconds": 1.0, "attack_recovery_seconds": 1.380952,
            "frontal_wave_windup_seconds": 1.6, "frontal_wave_recovery_seconds": 0.945455,
            "abyss_zone_windup_seconds": 1.9, "abyss_zone_recovery_seconds": 1.085714,
        },
        "unchanged_by_script": "monster DA, profiles, HP, damage, cooldowns, vendor assets, maps",
        "visual_validation": "pending_actual_rendered_ready_hit_recovery",
    }
    report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[BossReadability] " + json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
