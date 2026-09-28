"""Validate the shared player animation, retarget and Niagara wiring."""

import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ConfigureCommonCharacterAnimation as config


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    common_bp = config.require_asset(config.COMMON_ANIM_BP_PATH)
    common_class = common_bp.generated_class()
    check(common_class is not None, "Common AnimBP generated class is missing")
    anim_graph = next(
        (
            graph
            for graph in unreal.BlueprintEditorLibrary.list_graphs(common_bp)
            if graph.get_name() == "AnimGraph"
        ),
        None,
    )
    check(anim_graph is not None, "Common AnimBP AnimGraph is missing")
    graph_editor = unreal.BlueprintGraphEditor.get_graph_editor(anim_graph)
    graph_nodes = list(graph_editor.list_all_nodes())
    check(
        any(isinstance(node, unreal.AnimGraphNode_StateMachine) for node in graph_nodes),
        "Common AnimBP locomotion state machine is missing",
    )
    check(
        any(isinstance(node, unreal.AnimGraphNode_Slot) for node in graph_nodes),
        "Common AnimBP montage Slot node is missing",
    )

    for locomotion_path in (
        "/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run",
        "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump",
        "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop",
        "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Land",
    ):
        config.require_asset(locomotion_path)

    montages = {
        name: config.require_asset(f"{config.ANIMATION_ROOT}/{name}")
        for name in config.ANIMATION_SOURCES
    }
    for name, montage in montages.items():
        check(isinstance(montage, unreal.AnimMontage), f"Not an AnimMontage: {name}")
        check(montage.get_editor_property("skeleton") is not None, f"Montage skeleton missing: {name}")

    effects = {
        name: config.require_asset(config.effect_target_path(name))
        for name in config.NIAGARA_SOURCES
    }
    for name, effect in effects.items():
        check(isinstance(effect, unreal.NiagaraSystem), f"Not a Niagara System: {name}")

    motion_data = config.require_asset(config.MOTION_DATA_PATH)
    check(
        motion_data.get_editor_property("dodge_montage") == montages["AM_Dodge_Roll"],
        "Motion Data dodge montage mismatch",
    )
    check(
        not motion_data.get_editor_property("enable_motion_effects"),
        "Motion VFX must be disabled for animation-only presentation",
    )
    for property_name in (
        "walk_footstep_effect",
        "run_footstep_effect",
        "jump_effect",
        "double_jump_effect",
        "land_effect",
        "dodge_effect",
    ):
        check(motion_data.get_editor_property(property_name) is not None, f"Motion VFX missing: {property_name}")

    check(
        motion_data.get_editor_property("walk_footstep_interval") >= 0.65,
        "Walk footstep interval is too dense",
    )
    check(
        motion_data.get_editor_property("run_footstep_interval") >= 0.35,
        "Run footstep interval is too dense",
    )
    check(
        motion_data.get_editor_property("minimum_footstep_speed") >= 100.0,
        "Footstep minimum speed is too low",
    )
    scale_limits = {
        "walk_effect_scale": 0.12,
        "run_effect_scale": 0.16,
        "jump_effect_scale": 0.18,
        "double_jump_effect_scale": 0.20,
        "land_effect_scale": 0.20,
        "dodge_effect_scale": 0.16,
    }
    for property_name, expected in scale_limits.items():
        actual = motion_data.get_editor_property(property_name)
        check(abs(actual - expected) < 0.001, f"Motion VFX scale mismatch: {property_name}")

    duration_limits = {
        "walk_effect_emission_duration": 0.08,
        "run_effect_emission_duration": 0.10,
        "jump_effect_emission_duration": 0.12,
        "double_jump_effect_emission_duration": 0.14,
        "land_effect_emission_duration": 0.14,
        "dodge_effect_emission_duration": 0.12,
    }
    for property_name, expected in duration_limits.items():
        actual = motion_data.get_editor_property(property_name)
        check(abs(actual - expected) < 0.001, f"Motion VFX duration mismatch: {property_name}")
    check(
        abs(motion_data.get_editor_property("motion_effect_time_dilation") - 3.0) < 0.001,
        "Motion VFX time dilation mismatch",
    )

    dust_source = config.require_asset(config.MOTION_DUST_NIAGARA_SOURCE)
    check(
        isinstance(dust_source, unreal.NiagaraSystem),
        "Converted motion dust source is not a Niagara System",
    )
    dependency_options = unreal.AssetRegistryDependencyOptions()
    dependencies = unreal.AssetRegistryHelpers.get_asset_registry().get_dependencies(
        config.MOTION_DUST_NIAGARA_SOURCE,
        dependency_options,
    )
    converter_dependencies = [
        str(package_name)
        for package_name in dependencies
        if str(package_name).startswith("/CascadeToNiagaraConverter/")
    ]
    check(
        not converter_dependencies,
        "Motion dust source has editor-only converter dependencies: "
        + ", ".join(converter_dependencies),
    )

    root_asset_classes = {
        str(asset_data.asset_name): str(asset_data.asset_class_path.asset_name)
        for asset_data in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(
            unreal.Name(config.VFX_ROOT), recursive=False
        )
    }
    for effect_name in config.MOTION_NIAGARA_NAMES:
        effect = effects[effect_name]
        source = unreal.EditorAssetLibrary.get_metadata_tag(
            effect, config.SOURCE_METADATA_TAG
        )
        check(
            source == config.NIAGARA_SOURCES[effect_name],
            f"Motion VFX source mismatch: {effect_name}",
        )
        check(
            root_asset_classes.get(effect_name) in (None, "ObjectRedirector"),
            f"Legacy motion Niagara is still active: {effect_name}",
        )

    for asset_name, setting in config.ATTACK_SETTINGS.items():
        montage_name, cast_name, charge_name, enable_combo, next_name = setting
        cast_scale, cast_duration = config.ATTACK_VFX_TUNING[asset_name]
        skill = config.require_asset(f"{config.ATTACK_ROOT}/{asset_name}")
        check(skill.get_editor_property("attack_montage") == montages[montage_name], f"Montage mismatch: {asset_name}")
        check(
            not skill.get_editor_property("enable_attack_effects"),
            f"Attack VFX must be disabled for animation-only presentation: {asset_name}",
        )
        check(skill.get_editor_property("cast_effect") == effects[cast_name], f"Cast VFX mismatch: {asset_name}")
        check(skill.get_editor_property("impact_effect") == effects["NS_Punch_Impact"], f"Impact VFX mismatch: {asset_name}")
        expected_charge = effects[charge_name] if charge_name else None
        check(skill.get_editor_property("charge_effect") == expected_charge, f"Charge VFX mismatch: {asset_name}")
        check(skill.get_editor_property("enable_combo") == enable_combo, f"Combo flag mismatch: {asset_name}")
        expected_next = config.require_asset(f"{config.ATTACK_ROOT}/{next_name}") if next_name else None
        check(skill.get_editor_property("next_combo_skill") == expected_next, f"Combo link mismatch: {asset_name}")
        check(abs(skill.get_editor_property("cast_effect_scale") - cast_scale) < 0.001, f"Cast scale mismatch: {asset_name}")
        check(abs(skill.get_editor_property("charge_effect_scale") - 0.18) < 0.001, f"Charge scale mismatch: {asset_name}")
        check(abs(skill.get_editor_property("impact_effect_scale") - 0.28) < 0.001, f"Impact scale mismatch: {asset_name}")
        check(abs(skill.get_editor_property("cast_effect_emission_duration") - cast_duration) < 0.001, f"Cast duration mismatch: {asset_name}")
        check(abs(skill.get_editor_property("impact_effect_emission_duration") - 0.14) < 0.001, f"Impact duration mismatch: {asset_name}")
        check(abs(skill.get_editor_property("attack_effect_time_dilation") - 2.0) < 0.001, f"Attack time dilation mismatch: {asset_name}")
        check(
            root_asset_classes.get(cast_name) in (None, "ObjectRedirector"),
            f"Legacy attack Niagara is still active: {cast_name}",
        )

    for blueprint_path in config.CHARACTER_BLUEPRINTS:
        blueprint = config.require_asset(blueprint_path)
        cdo = unreal.get_default_object(blueprint.generated_class())
        check(
            cdo.get_editor_property("animation_source_anim_class") == common_class,
            f"Character does not use the common source AnimBP: {blueprint_path}",
        )
        check(
            cdo.get_editor_property("appearance_anim_class") is not None,
            f"Retarget AnimBP is missing: {blueprint_path}",
        )

    unarmed = config.require_asset("/Game/DataAsset/SkillProfile/DA_SkillProfile_Unarmed")
    stone_fist = config.require_asset("/Game/DataAsset/SkillProfile/DA_SkillProfile_StoneFist")
    check(unarmed.get_editor_property("max_jump_count") == 1, "Unarmed jump count must be 1")
    check(stone_fist.get_editor_property("max_jump_count") == 2, "StoneFist jump count must be 2")
    unreal.log(
        "RESULT=PASSED locomotion=walk/run/jump/fall/land montages=5 "
        "niagara=12 converted_dust_source=1 converter_runtime_dependencies=0 "
        "subtle_motion_vfx=6 organized_vfx=movement6/attack6 "
        "presentation=animation_only motion_vfx=off attack_vfx=off "
        "races=6 attacks=8 jump_profiles=1/2"
    )


main()
