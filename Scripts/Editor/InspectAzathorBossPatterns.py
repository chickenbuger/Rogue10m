"""Read-only editor snapshot of the boss connection and preserved monster data."""
import json
from pathlib import Path
import unreal

ROOT = Path("D:/Project/Rogue10m")
EVIDENCE = ROOT / "Feature/doc/evidence/azathor-boss-patterns-20261011"
OWN = "/Game/Rogue10m/World/OpenWorld/Monsters"
SPECIES = ("VerdantSlime", "Sporeling", "CrimsonSlime", "IronShell", "ThornWarden", "AbyssOverlordAzathor")


def path(value):
    return value.get_path_name() if value else None


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if levels.is_in_play_in_editor():
        raise RuntimeError("Inspection requires idle Editor")
    report = {
        "world": path(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()),
        "pie": False,
        "dirty_content": [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
        "dirty_maps": [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
        "species": {}, "materials": {},
    }
    fields = ("monster_id", "display_name", "level", "monster_rank", "max_health", "attack_damage", "experience_reward", "walk_speed", "skeletal_mesh", "attack_pattern_profile")
    for kind in SPECIES:
        asset = unreal.load_asset(OWN + "/DA_OW_" + kind)
        if not isinstance(asset, unreal.Rogue10mMonsterDataAsset):
            raise RuntimeError("Missing owned monster data: " + kind)
        row = {}
        for name in fields:
            value = asset.get_editor_property(name)
            row[name] = path(value) if isinstance(value, unreal.Object) else (value if isinstance(value, (int, float, bool)) else str(value))
        profile = asset.get_editor_property("attack_pattern_profile")
        row["basic"] = {name: profile.get_editor_property(name) for name in ("attack_range", "attack_arc_degrees", "attack_half_height", "attack_windup_seconds", "attack_recovery_seconds", "attack_cooldown_seconds", "attack_damage_multiplier")}
        row["animations"] = {name: path(profile.get_editor_property(name)) for name in ("idle_animation", "movement_animation", "attack_animation", "hit_animation", "death_animation")}
        try:
            row["boss_patterns_enabled"] = profile.get_editor_property("enable_boss_patterns")
        except Exception:
            row["boss_patterns_enabled"] = None  # Old reflected module, before rebuild.
        report["species"][kind] = row
    bp = unreal.load_asset("/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_FinalBoss")
    report["boss_blueprint_data"] = path(unreal.get_default_object(bp.generated_class()).get_monster_data())
    application = EVIDENCE / "application.json"
    if application.exists() and report["species"]["AbyssOverlordAzathor"]["boss_patterns_enabled"]:
        profile = unreal.load_asset(OWN + "/Azathor/DA_Azathor_BossPatterns")
        expected = json.loads(application.read_text(encoding="utf-8"))["profile_values"]
        actual = {}
        for name in expected:
            value = profile.get_editor_property(name)
            actual[name] = path(value) if isinstance(value, unreal.Object) else value
        for name, value in expected.items():
            match = abs(actual[name] - value) < 0.001 if isinstance(value, (float, int)) else actual[name] == value
            if not match:
                raise RuntimeError("Saved boss tuning mismatch: " + name)
        report["boss_tuning"] = actual
        master = unreal.load_asset(OWN + "/Azathor/M_Azathor_Telegraph")
        report["owned_material"] = {"domain": str(master.get_editor_property("material_domain")),
            "blend": str(master.get_editor_property("blend_mode")),
            "scalar_parameters": [str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(master)]}
        report["owned_material_instances"] = {}
        for name, shape in (("MI_Azathor_FrontalWarning", 0.0), ("MI_Azathor_ZoneWarning", 1.0)):
            instance = unreal.load_asset(OWN + "/Azathor/" + name)
            actual_shape = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(instance, "Shape")
            if instance.get_editor_property("parent") != master or abs(actual_shape - shape) > 0.001:
                raise RuntimeError("Saved warning instance mismatch: " + name)
            report["owned_material_instances"][name] = {"parent": path(master), "shape": actual_shape}
    for name in ("/Game/Stylized_Village/MasterMaterials/MM_Decal/Material/MM_Decal", "/Game/Stylized_Village/MasterMaterials/MM_Decal/Material/MM_Decal_ColorRoughness", "/Game/StarterContent/Particles/Materials/M_Radial_Gradient", "/Game/AdvancedPortalsSystemVFX/Materials/M_Portal_Ring"):
        asset = unreal.load_asset(name)
        if isinstance(asset, unreal.Material):
            report["materials"][name] = {"domain": str(asset.get_editor_property("material_domain")), "blend": str(asset.get_editor_property("blend_mode")), "scalar_parameters": [str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(asset)], "vector_parameters": [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(asset)], "texture_parameters": [str(n) for n in unreal.MaterialEditingLibrary.get_texture_parameter_names(asset)]}
    function = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Gradient/RadialGradientExponential")
    report["radial_function"] = path(function)
    report["function_api"] = [name for name in dir(unreal.MaterialEditingLibrary) if "function" in name.lower() or "input" in name.lower()]
    stage = getattr(unreal, "_azathor_inspection_stage", "before")
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / ("inspection-" + stage + ".json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[Azathor] Read-only " + stage + " snapshot saved; six species loaded")


if __name__ == "__main__":
    main()
