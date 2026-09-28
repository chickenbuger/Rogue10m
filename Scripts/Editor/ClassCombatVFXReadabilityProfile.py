"""Shared Overwatch-inspired readability budgets for class combat VFX."""

from __future__ import annotations


EFFECT_TYPE_PATH = "/Game/Rogue10m/VFX/Character/Combat/NET_CombatReadable"

TARGET_STYLE_IDS = (
    "Dagger",
    "Shuriken",
    "DualDaggers",
    "LongSword",
    "GreatSword",
    "DualBlades",
    "Shield",
    "SwordBuckler",
    "Staff",
    "Knuckle",
)

VARIANT_SCALE = {
    "Primary01": 0.76,
    "Primary02": 0.86,
    "Primary03": 0.96,
    "Special": 1.05,
    "Charged": 1.10,
}

VARIANT_DURATION = {
    "Primary01": 0.78,
    "Primary02": 0.86,
    "Primary03": 0.94,
    "Special": 1.00,
    "Charged": 1.05,
}

STYLE_PROFILES = {
    "Dagger": {
        "cast": 0.17, "charge": 0.18, "impact": 0.26,
        "cast_duration": 0.090, "impact_duration": 0.110, "time_dilation": 3.40,
        "fp_cast": 0.28, "fp_charge": 0.10, "fp_impact": 0.58,
        "fp_duration": 0.52, "fp_offhand": 0.70, "offset": (0.0, 14.0, -12.0),
    },
    "Shuriken": {
        "cast": 0.15, "charge": 0.18, "impact": 0.24,
        "cast_duration": 0.085, "impact_duration": 0.105, "time_dilation": 3.50,
        "fp_cast": 0.28, "fp_charge": 0.10, "fp_impact": 0.60,
        "fp_duration": 0.50, "fp_offhand": 0.70, "offset": (4.0, 16.0, -13.0),
    },
    "DualDaggers": {
        "cast": 0.14, "charge": 0.17, "impact": 0.24,
        "cast_duration": 0.085, "impact_duration": 0.105, "time_dilation": 3.60,
        "fp_cast": 0.27, "fp_charge": 0.10, "fp_impact": 0.58,
        "fp_duration": 0.50, "fp_offhand": 0.70, "offset": (0.0, 24.0, -20.0),
    },
    "LongSword": {
        "cast": 0.19, "charge": 0.20, "impact": 0.29,
        "cast_duration": 0.100, "impact_duration": 0.115, "time_dilation": 3.10,
        "fp_cast": 0.26, "fp_charge": 0.10, "fp_impact": 0.55,
        "fp_duration": 0.52, "fp_offhand": 0.70, "offset": (0.0, 17.0, -14.0),
    },
    "GreatSword": {
        "cast": 0.22, "charge": 0.22, "impact": 0.32,
        "cast_duration": 0.105, "impact_duration": 0.120, "time_dilation": 2.80,
        "fp_cast": 0.24, "fp_charge": 0.10, "fp_impact": 0.50,
        "fp_duration": 0.52, "fp_offhand": 0.70, "offset": (0.0, 21.0, -18.0),
    },
    "DualBlades": {
        "cast": 0.16, "charge": 0.18, "impact": 0.27,
        "cast_duration": 0.090, "impact_duration": 0.110, "time_dilation": 3.40,
        "fp_cast": 0.26, "fp_charge": 0.10, "fp_impact": 0.55,
        "fp_duration": 0.50, "fp_offhand": 0.70, "offset": (0.0, 26.0, -20.0),
    },
    "Shield": {
        "cast": 0.18, "charge": 0.21, "impact": 0.30,
        "cast_duration": 0.100, "impact_duration": 0.120, "time_dilation": 3.00,
        "fp_cast": 0.26, "fp_charge": 0.10, "fp_impact": 0.54,
        "fp_duration": 0.52, "fp_offhand": 0.70, "offset": (0.0, -19.0, -16.0),
    },
    "SwordBuckler": {
        "cast": 0.18, "charge": 0.20, "impact": 0.29,
        "cast_duration": 0.100, "impact_duration": 0.115, "time_dilation": 3.10,
        "fp_cast": 0.26, "fp_charge": 0.10, "fp_impact": 0.55,
        "fp_duration": 0.52, "fp_offhand": 0.70, "offset": (0.0, 18.0, -15.0),
    },
    "Staff": {
        "cast": 0.22, "charge": 0.24, "impact": 0.30,
        "cast_duration": 0.105, "impact_duration": 0.120, "time_dilation": 3.00,
        "fp_cast": 0.24, "fp_charge": 0.10, "fp_impact": 0.52,
        "fp_duration": 0.50, "fp_offhand": 0.70, "offset": (8.0, 20.0, -18.0),
    },
    "Knuckle": {
        "cast": 0.15, "charge": 0.18, "impact": 0.26,
        "cast_duration": 0.085, "impact_duration": 0.105, "time_dilation": 3.50,
        "fp_cast": 0.28, "fp_charge": 0.10, "fp_impact": 0.58,
        "fp_duration": 0.50, "fp_offhand": 0.70, "offset": (0.0, 24.0, -19.0),
    },
}


def has_profile(style_id: str) -> bool:
    return style_id in STYLE_PROFILES


def get_skill_tuning(style_id: str, variant_name: str) -> dict[str, float | tuple[float, float, float]]:
    profile = STYLE_PROFILES[style_id]
    scale = VARIANT_SCALE[variant_name]
    duration = VARIANT_DURATION[variant_name]
    return {
        "cast_effect_scale": profile["cast"] * scale,
        "charge_effect_scale": profile["charge"],
        "impact_effect_scale": profile["impact"] * scale,
        "cast_effect_emission_duration": profile["cast_duration"] * duration,
        "impact_effect_emission_duration": profile["impact_duration"] * duration,
        "attack_effect_time_dilation": profile["time_dilation"],
        "first_person_cast_scale_multiplier": profile["fp_cast"],
        "first_person_charge_scale_multiplier": profile["fp_charge"],
        "first_person_impact_scale_multiplier": profile["fp_impact"],
        "first_person_emission_duration_multiplier": profile["fp_duration"],
        "first_person_off_hand_effect_scale_multiplier": profile["fp_offhand"],
        "first_person_effect_offset": profile["offset"],
    }


def calculate_first_person_budget(style_id: str, variant_name: str) -> dict[str, float]:
    tuning = get_skill_tuning(style_id, variant_name)
    return {
        "cast_scale": tuning["cast_effect_scale"] * tuning["first_person_cast_scale_multiplier"],
        "charge_scale": tuning["charge_effect_scale"] * tuning["first_person_charge_scale_multiplier"],
        "impact_scale": tuning["impact_effect_scale"] * tuning["first_person_impact_scale_multiplier"],
        "cast_duration": tuning["cast_effect_emission_duration"] * tuning["first_person_emission_duration_multiplier"],
        "impact_duration": tuning["impact_effect_emission_duration"] * tuning["first_person_emission_duration_multiplier"],
    }
