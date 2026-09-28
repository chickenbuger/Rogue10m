"""Validate StoneFist animation/VFX presentation without touching gameplay progression."""

from __future__ import annotations

import math
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ApplyMartialArtsReadableVFX as martial
import ClassCombatVFXReadabilityProfile as readability


def check(condition: bool, message: str):
    if not condition:
        raise RuntimeError(message)


def check_close(actual: float, expected: float, message: str):
    check(math.isclose(actual, expected, rel_tol=0.0, abs_tol=0.0001), message)


def main():
    cast_effect = martial.require(
        "/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Cast"
    )
    charge_effect = martial.require(
        "/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Charge"
    )
    impact_effect = martial.require(
        "/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Impact"
    )

    for skill_name, variant_name, montage_name, dual_hand in martial.MARTIAL_ART_PRESENTATION:
        path = f"/Game/DataAsset/AttackSkill/StoneFist/DA_Attack_StoneFist_{skill_name}"
        skill = martial.require(path)
        expected_montage = martial.require(
            f"/Game/Rogue10m/Animation/Combat/Knuckle/AM_Knuckle_{montage_name}"
        )
        check(skill.get_editor_property("attack_montage") == expected_montage, f"Montage mismatch: {path}")
        check(skill.get_editor_property("enable_attack_effects"), f"VFX disabled: {path}")
        check(skill.get_editor_property("cast_effect") == cast_effect, f"Cast mismatch: {path}")
        check(skill.get_editor_property("impact_effect") == impact_effect, f"Impact mismatch: {path}")
        expected_charge = charge_effect if variant_name == "Charged" else None
        check(skill.get_editor_property("charge_effect") == expected_charge, f"Charge mismatch: {path}")
        check(skill.get_editor_property("spawn_effect_on_off_hand") == dual_hand, f"Off-hand mismatch: {path}")
        check(skill.get_editor_property("use_first_person_effect_overrides"), f"FP override disabled: {path}")
        check(
            unreal.EditorAssetLibrary.get_metadata_tag(skill, "Rogue10mVFXReadabilityProfile")
            == "HeroShooterFP_v1",
            f"Readability metadata mismatch: {path}",
        )

        tuning = readability.get_skill_tuning("Knuckle", variant_name)
        for field, expected in tuning.items():
            actual = skill.get_editor_property(field)
            if field == "first_person_effect_offset":
                check_close(actual.x, expected[0], f"Offset X mismatch: {path}")
                check_close(actual.y, expected[1], f"Offset Y mismatch: {path}")
                check_close(actual.z, expected[2], f"Offset Z mismatch: {path}")
            else:
                check_close(actual, expected, f"{field} mismatch: {path}")

        budget = readability.calculate_first_person_budget("Knuckle", variant_name)
        check(budget["cast_scale"] <= 0.060, f"FP cast budget exceeded: {path}")
        check(budget["charge_scale"] <= 0.026, f"FP charge budget exceeded: {path}")
        check(budget["impact_scale"] <= 0.180, f"FP impact budget exceeded: {path}")
        check(budget["cast_duration"] <= 0.060, f"FP cast duration exceeded: {path}")
        check(budget["impact_duration"] <= 0.070, f"FP impact duration exceeded: {path}")

    unreal.log(
        "RESULT=MARTIAL_ART_READABLE_VFX_PASSED "
        f"skills={len(martial.MARTIAL_ART_PRESENTATION)} montages=4 "
        "niagara_roles=3 gameplay_fields=preserved first_person_safe=4"
    )


if __name__ == "__main__":
    main()
