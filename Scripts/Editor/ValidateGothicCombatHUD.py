"""Validate gothic HUD texture assets and preserved main HUD placement."""

from __future__ import annotations

import os
import unreal


MAIN_HUD_PATH = "/Game/Widget/WBP_Rogue10mMainHUD"
QUICK_SLOT_PATH = "/Game/Widget/Parts/WBP_QuickSlot"
OUTPUT_PATH = "D:/Project/Rogue10m/Saved/GothicCombatHUDValidation.txt"
UMG_TOOL = unreal.get_default_object(unreal.UMGToolSet)

BASELINE_MAIN_LAYOUT = {
    "HealthBarWidget": ((670.0, 930.0), (360.0, 34.0), 2),
    "StaminaBarWidget": ((1160.0, 930.0), (360.0, 34.0), 2),
    "ProgressionWidget": ((0.0, -112.0), (0.0, 112.0), 0),
    "IdentityWidget": ((1048.0, 930.0), (96.0, 116.0), 3),
    "MonsterInfoWidget": ((750.0, 36.0), (420.0, 68.0), 2),
    "SystemLogPanelWidget": ((24.0, 770.0), (360.0, 180.0), 1),
    "ItemAcquisitionFeedWidget": ((1316.0, 332.0), (260.0, 160.0), 1),
    "SkillSlotPanelWidget": ((760.0, 968.0), (280.0, 82.0), 2),
    "ItemSlotPanelWidget": ((1160.0, 968.0), (230.0, 72.0), 2),
}

ORNAMENTS = {
    "UI_GothicBarLeftFrame",
    "UI_GothicBarRightFrame",
    "UI_GothicMedallionFrame",
}

TEXTURES = {
    "/Game/UI/HUD/Gothic/T_HUD_GothicBarLeft",
    "/Game/UI/HUD/Gothic/T_HUD_GothicBarRight",
    "/Game/UI/HUD/Gothic/T_HUD_GothicMedallion",
    "/Game/UI/HUD/Gothic/T_HUD_GothicExperienceFrame",
    "/Game/UI/HUD/Gothic/T_HUD_GothicSlotFrame",
}


def umg(method: str, *args):
    return UMG_TOOL.call_method(method, args)


def nearly_equal(a: float, b: float) -> bool:
    return abs(a - b) <= 0.01


def validate() -> list[str]:
    lines = []
    failures = []
    main_hud = unreal.EditorAssetLibrary.load_asset(MAIN_HUD_PATH)
    if not main_hud:
        raise RuntimeError(f"Missing main HUD: {MAIN_HUD_PATH}")

    infos = [info for info in umg("GetWidgets", main_hud).widgets if info.widget]
    by_name = {info.widget.get_name(): info for info in infos}

    for name, (expected_position, expected_size, expected_z) in BASELINE_MAIN_LAYOUT.items():
        info = by_name.get(name)
        if not info or not info.slot:
            failures.append(f"Missing baseline widget: {name}")
            continue
        position = info.slot.get_position()
        size = info.slot.get_size()
        z_order = info.slot.get_z_order()
        valid = (
            nearly_equal(position.x, expected_position[0])
            and nearly_equal(position.y, expected_position[1])
            and nearly_equal(size.x, expected_size[0])
            and nearly_equal(size.y, expected_size[1])
            and z_order == expected_z
        )
        lines.append(
            f"baseline {name}: position=({position.x},{position.y}) "
            f"size=({size.x},{size.y}) z={z_order} valid={valid}"
        )
        if not valid:
            failures.append(f"Baseline placement changed: {name}")

    missing_ornaments = sorted(ORNAMENTS - set(by_name))
    if missing_ornaments:
        failures.append(f"Missing ornaments: {', '.join(missing_ornaments)}")
    lines.append(f"ornaments={sorted(ORNAMENTS & set(by_name))}")

    for texture_path in sorted(TEXTURES):
        texture = unreal.EditorAssetLibrary.load_asset(texture_path)
        valid = texture is not None
        lines.append(f"texture {texture_path}: valid={valid}")
        if not valid:
            failures.append(f"Missing texture: {texture_path}")

    quick_slot = unreal.EditorAssetLibrary.load_asset(QUICK_SLOT_PATH)
    quick_names = {
        info.widget.get_name()
        for info in umg("GetWidgets", quick_slot).widgets
        if info.widget
    }
    for required in {"UI_SlotFrame", "UI_IconImage", "UI_KeyText", "UI_CooldownText"}:
        if required not in quick_names:
            failures.append(f"Missing quick-slot binding: {required}")
    lines.append(f"quick_slot_bindings={sorted(quick_names)}")

    if failures:
        lines.append("RESULT=FAILED")
        lines.extend(f"FAIL: {failure}" for failure in failures)
    else:
        lines.append("RESULT=PASSED")
    return lines


def main() -> None:
    lines = validate()
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
        output_file.write("\n".join(lines))
    for line in lines:
        unreal.log(f"[Rogue10mCombatHUD] {line}")
    if lines[-1].startswith("FAIL:") or "RESULT=FAILED" in lines:
        raise RuntimeError("Gothic combat HUD validation failed")


if __name__ == "__main__":
    main()
