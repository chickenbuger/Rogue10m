"""Create owned boss profile/decals and connect ONLY the existing Azathor DA.

Run after the reflected native rebuild, in an idle Editor with no unsaved work.
Existing base profile, encounter Blueprint, placed actor, other species and maps
are never saved or modified by this script.
"""
import json
from pathlib import Path
import unreal

ROOT = Path("D:/Project/Rogue10m")
EVIDENCE = ROOT / "Feature/doc/evidence/azathor-boss-patterns-20261011"
OWN = "/Game/Rogue10m/World/OpenWorld/Monsters"
FOLDER = OWN + "/Azathor"
DATA = OWN + "/DA_OW_AbyssOverlordAzathor"
BASE = OWN + "/DA_OW_BasicAttack_AbyssOverlordAzathor"
PROFILE = FOLDER + "/DA_Azathor_BossPatterns"
MATERIAL = FOLDER + "/M_Azathor_Telegraph"


def require(name):
    asset = unreal.load_asset(name)
    if not asset:
        raise RuntimeError("Missing asset " + name)
    return asset


def create(name, klass, factory):
    existing = unreal.load_asset(name)
    if existing:
        if not isinstance(existing, klass):
            raise RuntimeError("Unexpected existing asset type: " + name)
        return existing, False
    folder, short = name.rsplit("/", 1)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(short, folder, klass, factory)
    if not asset:
        raise RuntimeError("Could not create " + name)
    return asset, True


def build_material():
    material, fresh = create(MATERIAL, unreal.Material, unreal.MaterialFactoryNew())
    if not fresh:
        if not {"Shape", "Opacity", "Progress", "Glow"} <= set(map(str, unreal.MaterialEditingLibrary.get_scalar_parameter_names(material))):
            raise RuntimeError("Existing owned material is incomplete; inspect before replacing")
        errors = list(unreal.MaterialEditingLibrary.recompile_material(material))
        if errors:
            raise RuntimeError("Owned warning material compile failed: " + str(errors))
        return material
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    lib = unreal.MaterialEditingLibrary

    def node(klass, **properties):
        value = lib.create_material_expression(material, klass, -600, 0)
        if not value:
            raise RuntimeError("Cannot create material expression " + str(klass))
        for key, item in properties.items():
            value.set_editor_property(key, item)
        return value

    def wire(a, b, input_name, output=""):
        if not lib.connect_material_expressions(a, output, b, input_name):
            raise RuntimeError("Cannot connect material input " + input_name)

    def constant(value):
        return node(unreal.MaterialExpressionConstant, r=value)

    def scalar(name, default):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default, group="Azathor Warning")

    def multiply(a, b):
        value = node(unreal.MaterialExpressionMultiply)
        wire(a, value, "A")
        wire(b, value, "B")
        return value

    def add(a, b):
        value = node(unreal.MaterialExpressionAdd)
        wire(a, value, "A")
        wire(b, value, "B")
        return value

    uv = node(unreal.MaterialExpressionTextureCoordinate)
    center = node(unreal.MaterialExpressionConstant2Vector, r=0.5, g=0.5)
    functions = {
        "box": require("/Engine/Functions/Engine_MaterialFunctions01/Gradient/BoxMask-2D"),
        "circle": require("/Engine/Functions/Engine_MaterialFunctions01/Gradient/RadialGradientExponential"),
    }

    def mask(kind, size):
        value = node(unreal.MaterialExpressionMaterialFunctionCall, material_function=functions[kind])
        if kind == "box":
            wire(uv, value, "A")
            wire(center, value, "B")
            wire(node(unreal.MaterialExpressionConstant2Vector, r=size, g=size), value, "Bounds")
            wire(constant(0.002), value, "Edge Falloff")
        else:
            wire(uv, value, "UVs")
            wire(center, value, "CenterPosition")
            wire(constant(size), value, "Radius")
            wire(constant(100.0), value, "Density")
        return value

    def outlined(kind):
        outer = mask(kind, 1.0 if kind == "box" else 0.5)
        inner = mask(kind, 0.9 if kind == "box" else 0.44)
        border = node(unreal.MaterialExpressionSubtract)
        wire(outer, border, "A")
        wire(inner, border, "B")
        return add(border, multiply(inner, constant(0.24)))

    shape = node(unreal.MaterialExpressionLinearInterpolate)
    wire(outlined("box"), shape, "A")
    wire(outlined("circle"), shape, "B")
    wire(scalar("Shape", 0.0), shape, "Alpha")
    progress = add(constant(0.55), multiply(scalar("Progress", 0.0), constant(0.45)))
    opacity = multiply(shape, multiply(scalar("Opacity", 0.75), progress))
    color = node(unreal.MaterialExpressionVectorParameter, parameter_name="WarningColor", default_value=unreal.LinearColor(1.0, 0.18, 0.025, 1.0), group="Azathor Warning")
    emissive = multiply(color, scalar("Glow", 3.0))
    for expression, prop in ((opacity, unreal.MaterialProperty.MP_OPACITY), (emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR), (color, unreal.MaterialProperty.MP_BASE_COLOR), (constant(0.8), unreal.MaterialProperty.MP_ROUGHNESS)):
        if not lib.connect_material_property(expression, "", prop):
            raise RuntimeError("Cannot connect material output " + str(prop))
    lib.layout_material_expressions(material)
    errors = list(lib.recompile_material(material))
    if errors:
        raise RuntimeError("Warning material compile failed: " + str(errors))
    return material


def instance(name, parent, shape):
    value, fresh = create(FOLDER + "/" + name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if not fresh and value.get_editor_property("parent") != parent:
        raise RuntimeError("Owned instance has unexpected parent")
    unreal.MaterialEditingLibrary.set_material_instance_parent(value, parent)
    # UE 5.8's setter always returns false (MaterialEditingLibrary.cpp); verify readback instead.
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(value, "Shape", shape)
    if abs(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(value, "Shape") - shape) > 0.001:
        raise RuntimeError("Cannot set instance shape")
    unreal.MaterialEditingLibrary.update_material_instance(value)
    return value


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    allowed_dirty = {MATERIAL, FOLDER + "/MI_Azathor_FrontalWarning", FOLDER + "/MI_Azathor_ZoneWarning", PROFILE}
    dirty = {str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if levels.is_in_play_in_editor() or dirty - allowed_dirty or unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError("Requires idle Editor and no unsaved packages; existing work preserved")
    baseline = json.loads((EVIDENCE / "inspection-before.json").read_text(encoding="utf-8"))["species"]["AbyssOverlordAzathor"]
    data, base = require(DATA), require(BASE)
    current = data.get_editor_property("attack_pattern_profile")
    if current not in (base, unreal.load_asset(PROFILE)):
        raise RuntimeError("Boss profile changed since inspection")
    for key in ("max_health", "attack_damage", "experience_reward", "walk_speed"):
        if abs(float(data.get_editor_property(key)) - baseline[key]) > 0.001:
            raise RuntimeError("Boss baseline changed: " + key)
    # All references are validated before editing/saving the existing DA.
    frontal_clip = require("/Game/MonsterForSurvivalGame/Animation/PBR/Beholder/Beholder_Attack03_ANIM")
    zone_clip = require("/Game/MonsterForSurvivalGame/Animation/PBR/Beholder/Beholder_Attack02ST_ANIM")
    skeleton = data.get_editor_property("skeletal_mesh").get_editor_property("skeleton")
    if any(clip.get_editor_property("skeleton") != skeleton for clip in (frontal_clip, zone_clip)):
        raise RuntimeError("Boss attack skeleton mismatch")
    # Confirm the rebuilt reflected schema before creating assets.
    base.get_editor_property("enable_boss_patterns")
    material = build_material()
    frontal = instance("MI_Azathor_FrontalWarning", material, 0.0)
    zone = instance("MI_Azathor_ZoneWarning", material, 1.0)
    profile = unreal.load_asset(PROFILE)
    if not profile:
        profile = unreal.EditorAssetLibrary.duplicate_asset(BASE, PROFILE)
    if not isinstance(profile, unreal.Rogue10mMonsterAttackPatternProfile):
        raise RuntimeError("Invalid boss profile")
    profile.modify()
    values = {
        "enable_boss_patterns": True,
        "frontal_wave_range": 850.0, "frontal_wave_half_width": 180.0, "frontal_wave_half_height": 160.0,
        "frontal_wave_windup_seconds": 1.1, "frontal_wave_recovery_seconds": 0.65,
        "frontal_wave_cooldown_seconds": 6.0, "frontal_wave_damage_multiplier": 1.45,
        "abyss_zone_cast_range": 1000.0, "abyss_zone_radius": 260.0, "abyss_zone_half_height": 160.0,
        "abyss_zone_windup_seconds": 1.4, "abyss_zone_recovery_seconds": 0.8,
        "abyss_zone_cooldown_seconds": 8.0, "abyss_zone_damage_multiplier": 1.6,
        "enrage_health_threshold": 0.5, "enrage_windup_multiplier": 0.75,
        "enrage_recovery_multiplier": 0.8, "enrage_cooldown_multiplier": 0.75, "enrage_damage_multiplier": 1.25,
        "frontal_wave_telegraph_material": frontal, "abyss_zone_telegraph_material": zone,
        "frontal_wave_animation": frontal_clip, "abyss_zone_animation": zone_clip,
        "telegraph_depth": 180.0, "telegraph_ground_offset": 2.0,
    }
    for key, value in values.items():
        profile.set_editor_property(key, value)
    for key, value in baseline["basic"].items():
        if abs(float(profile.get_editor_property(key)) - value) > 0.001:
            raise RuntimeError("Basic attack was not preserved: " + key)
    for key, value in baseline["animations"].items():
        if profile.get_editor_property(key).get_path_name() != value:
            raise RuntimeError("Basic animation was not preserved: " + key)
    saved = []
    for asset in (material, frontal, zone, profile):
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            raise RuntimeError("Save failed: " + asset.get_path_name())
        saved.append(asset.get_path_name())
    data.modify()
    data.set_editor_property("attack_pattern_profile", profile)
    if not unreal.EditorAssetLibrary.save_loaded_asset(data, False):
        raise RuntimeError("Boss DA save failed")
    saved.append(data.get_path_name())
    report = {"status": "saved", "saved_assets": saved, "profile_values": {key: value.get_path_name() if isinstance(value, unreal.Object) else value for key, value in values.items()}, "boss_data": data.get_path_name(), "base_profile_unchanged": BASE, "preserved_basic": baseline["basic"], "preserved_animations": baseline["animations"], "material_source_decision": "Existing village decals use textured albedo/normal PBR, portal rings are surface materials; new emissive telegraph uses existing engine BoxMask-2D and RadialGradientExponential functions."}
    (EVIDENCE / "application.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[Azathor] Saved four owned new assets and connected the existing boss DA only")


if __name__ == "__main__":
    main()
