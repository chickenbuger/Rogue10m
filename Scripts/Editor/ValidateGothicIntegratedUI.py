"""Validate Gothic UI texture imports, bindings, and main-HUD placement."""

from __future__ import annotations

import os

import unreal


OUTPUT_PATH = "D:/Project/Rogue10m/Saved/GothicIntegratedUIValidation.txt"
MAIN_HUD_PATH = "/Game/Widget/WBP_Rogue10mMainHUD"
SHOWCASE_PATH = "/Game/Widget/Debug/WBP_GothicUIShowcase"
UMG_TOOL = unreal.get_default_object(unreal.UMGToolSet)

TEXTURES = {
    "/Game/UI/HUD/Gothic/T_UI_GothicInventoryFrame": (1120, 1220),
    "/Game/UI/HUD/Gothic/T_HUD_GothicXPBottomFrame": (1920, 52),
    "/Game/UI/HUD/Gothic/T_HUD_GothicMonsterInfoFrame": (840, 136),
    "/Game/UI/HUD/Gothic/T_HUD_GothicSkillWing": (960, 208),
    "/Game/UI/HUD/Gothic/T_HUD_GothicItemWing": (960, 208),
}

REQUIRED_WIDGETS = {
    "/Game/Widget/Component/Inventory/WBP_InventoryWindow": {
        "UI_InventoryWindowFrame",
        "UI_WindowRoot",
        "UI_WindowDragHandle",
        "UI_InventoryGrid",
        "UI_InventoryItemCanvas",
        "UI_InventoryMoneyText",
        "UI_InventoryWeightText",
    },
    "/Game/Widget/Parts/WBP_LevelExperiencePanel": {
        "UI_GothicXPLevelFrame",
        "UI_LevelText",
        "UI_ExperienceBar",
        "UI_ExperienceText",
    },
    "/Game/Widget/Parts/WBP_MonsterInfo": {
        "UI_MonsterInfoBackground",
        "UI_MonsterNameText",
        "UI_MonsterHealthBar",
        "UI_MonsterHealthText",
    },
    "/Game/Widget/Parts/WBP_SkillSlotPanel": {
        "UI_GothicSkillWing",
        "UI_SkillSlotFrame1",
        "UI_SkillSlotFrame2",
        "UI_SkillSlotFrame3",
        "UI_SkillSlotFrame4",
        "UI_SkillSlotFrame5",
    },
    "/Game/Widget/Parts/WBP_ItemSlotPanel": {
        "UI_GothicItemWing",
        "UI_ItemSlotFrame1",
        "UI_ItemSlotFrame2",
        "UI_ItemSlotFrame3",
        "UI_ItemSlotFrame4",
        "UI_ItemSlotFrame5",
    },
    SHOWCASE_PATH: {"UI_ShowcaseMainHUD", "UI_ShowcaseInventory"},
}


def umg(method: str, *args):
    return UMG_TOOL.call_method(method, args)


def nearly_equal(a: float, b: float) -> bool:
    return abs(a - b) <= 0.01


def main() -> None:
    failures = []
    lines = []

    for path, expected_size in TEXTURES.items():
        texture = unreal.EditorAssetLibrary.load_asset(path)
        if not texture:
            failures.append(f"Missing texture: {path}")
            continue
        size = (texture.blueprint_get_size_x(), texture.blueprint_get_size_y())
        valid = (
            size == expected_size
            and texture.get_editor_property("lod_group")
            == unreal.TextureGroup.TEXTUREGROUP_UI
            and texture.get_editor_property("never_stream")
        )
        lines.append(f"texture {path}: size={size}, ui_config={valid}")
        if not valid:
            failures.append(f"Invalid texture configuration: {path}")

    for path, required in REQUIRED_WIDGETS.items():
        widget = unreal.EditorAssetLibrary.load_asset(path)
        if not widget:
            failures.append(f"Missing Widget Blueprint: {path}")
            continue
        names = {
            info.widget.get_name()
            for info in umg("GetWidgets", widget).widgets
            if info.widget
        }
        missing = sorted(required - names)
        lines.append(f"widget {path}: required={len(required)}, missing={missing}")
        if missing:
            failures.append(f"Missing bindings in {path}: {', '.join(missing)}")

    main_hud = unreal.EditorAssetLibrary.load_asset(MAIN_HUD_PATH)
    progression = None
    old_frame_present = False
    for info in umg("GetWidgets", main_hud).widgets:
        if not info.widget:
            continue
        if info.widget.get_name() == "ProgressionWidget":
            progression = info
        if info.widget.get_name() == "UI_GothicExperienceFrame":
            old_frame_present = True
    if not progression or not progression.slot:
        failures.append("Missing ProgressionWidget")
    else:
        offsets = progression.slot.get_offsets()
        valid = (
            nearly_equal(offsets.left, 0.0)
            and nearly_equal(offsets.top, -52.0)
            and nearly_equal(offsets.right, 0.0)
            and nearly_equal(offsets.bottom, 52.0)
            and progression.slot.get_z_order() == 6
        )
        lines.append(
            f"ProgressionWidget offsets=({offsets.left},{offsets.top},"
            f"{offsets.right},{offsets.bottom}), z={progression.slot.get_z_order()}, valid={valid}"
        )
        if not valid:
            failures.append("ProgressionWidget is not full-width 1920x52 bottom placement")
    if old_frame_present:
        failures.append("Superseded UI_GothicExperienceFrame still exists")

    expected_main_layout = {
        "HealthBarWidget": (580.0, 900.0, 360.0, 34.0, 2),
        "StaminaBarWidget": (980.0, 900.0, 360.0, 34.0, 2),
        "IdentityWidget": (912.0, 874.0, 96.0, 116.0, 4),
        "SkillSlotPanelWidget": (480.0, 936.0, 480.0, 104.0, 2),
        "ItemSlotPanelWidget": (960.0, 936.0, 480.0, 104.0, 2),
        "UI_GothicMedallionFrame": (849.0, 812.0, 222.0, 197.0, 1),
        "UI_HealthRolePreview": (581.0, 908.0, 292.0, 16.0, 1),
        "UI_StaminaRolePreview": (981.0, 908.0, 244.0, 16.0, 1),
    }
    main_infos = {
        info.widget.get_name(): info
        for info in umg("GetWidgets", main_hud).widgets
        if info.widget
    }
    for name, expected in expected_main_layout.items():
        info = main_infos.get(name)
        if not info or not info.slot:
            failures.append(f"Missing main HUD layout widget: {name}")
            continue
        offsets = info.slot.get_offsets()
        actual = (
            offsets.left,
            offsets.top,
            offsets.right,
            offsets.bottom,
            info.slot.get_z_order(),
        )
        valid = all(nearly_equal(actual[i], expected[i]) for i in range(4)) and actual[4] == expected[4]
        lines.append(f"layout {name}: actual={actual}, valid={valid}")
        if not valid:
            failures.append(f"Invalid main HUD layout: {name}")

    lines.append("RESULT=FAILED" if failures else "RESULT=PASSED")
    lines.extend(f"FAIL: {failure}" for failure in failures)
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
        output_file.write("\n".join(lines))
    for line in lines:
        unreal.log(f"[Rogue10mGothicUI] {line}")
    if failures:
        raise RuntimeError("Gothic integrated UI validation failed")


if __name__ == "__main__":
    main()
