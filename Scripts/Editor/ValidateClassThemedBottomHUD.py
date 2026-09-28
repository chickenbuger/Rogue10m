"""Validate class-themed Bottom HUD textures, UMG bindings, and Main ownership."""

from __future__ import annotations

import os

import unreal


BOTTOM_HUD_PATH = "/Game/Widget/Parts/WBP_BottomHUD"
MAIN_HUD_PATH = "/Game/Widget/WBP_Rogue10mMainHUD"
SHOWCASE_PATH = "/Game/Widget/Debug/WBP_ClassBottomHUDShowcase"
OUTPUT_PATH = "D:/Project/Rogue10m/Saved/ClassBottomHUDValidation.txt"
TEXTURE_PATHS = [
    "/Game/UI/HUD/ClassThemes/T_HUD_Bottom_Wizard",
    "/Game/UI/HUD/ClassThemes/T_HUD_Bottom_Warrior",
    "/Game/UI/HUD/ClassThemes/T_HUD_Bottom_MartialArtist",
    "/Game/UI/HUD/ClassThemes/T_HUD_Bottom_Rogue",
]
BOTTOM_BINDINGS = {
    "UI_WizardThemeFrame",
    "UI_WarriorThemeFrame",
    "UI_MartialArtistThemeFrame",
    "UI_RogueThemeFrame",
    "HealthBarWidget",
    "StaminaBarWidget",
    "ManaBarWidget",
    "IdentityBarWidget",
    "IdentityWidget",
    "SkillSlotContainer",
    "ItemSlotContainer",
    "ProgressionWidget",
    "UI_ThemeNameText",
    "UI_ResourceTypeText",
}
OLD_MAIN_BOTTOM_WIDGETS = {
    "HealthBarWidget",
    "StaminaBarWidget",
    "ManaBarWidget",
    "IdentityBarWidget",
    "ProgressionWidget",
    "SkillSlotPanelWidget",
    "ItemSlotPanelWidget",
    "IdentityWidget",
    "SkillSlotContainer",
    "ItemSlotContainer",
    "UI_GothicBarLeftFrame",
    "UI_GothicBarRightFrame",
    "UI_GothicMedallionFrame",
    "UI_GothicExperienceFrame",
    "UI_HealthRolePreview",
    "UI_StaminaRolePreview",
}

UMG_TOOL = unreal.get_default_object(unreal.UMGToolSet)


def umg(method: str, *args):
    return UMG_TOOL.call_method(method, args)


def load_widget(path: str):
    widget = unreal.EditorAssetLibrary.load_asset(path)
    if not widget:
        raise RuntimeError(f"Widget Blueprint not found: {path}")
    return widget


def widget_infos(widget_blueprint):
    return [info for info in umg("GetWidgets", widget_blueprint).widgets if info.widget]


def main() -> None:
    failures: list[str] = []
    lines: list[str] = []

    for path in TEXTURE_PATHS:
        texture = unreal.EditorAssetLibrary.load_asset(path)
        if not texture:
            failures.append(f"Missing texture: {path}")
            continue
        size = (texture.blueprint_get_size_x(), texture.blueprint_get_size_y())
        ui_config = (
            texture.get_editor_property("lod_group")
            == unreal.TextureGroup.TEXTUREGROUP_UI
            and texture.get_editor_property("mip_gen_settings")
            == unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
            and texture.get_editor_property("never_stream")
        )
        lines.append(f"texture {path}: size={size}, ui_config={ui_config}")
        if size != (1920, 208):
            failures.append(f"Unexpected texture size: {path}={size}")
        if not ui_config:
            failures.append(f"Invalid UI texture settings: {path}")

    bottom = load_widget(BOTTOM_HUD_PATH)
    bottom_infos = widget_infos(bottom)
    bottom_names = {info.widget.get_name() for info in bottom_infos}
    missing_bottom = sorted(BOTTOM_BINDINGS - bottom_names)
    lines.append(f"bottom required={len(BOTTOM_BINDINGS)}, missing={missing_bottom}")
    if missing_bottom:
        failures.append(f"Bottom HUD bindings missing: {missing_bottom}")

    progression = next(
        (info for info in bottom_infos if info.widget.get_name() == "ProgressionWidget"),
        None,
    )
    if not progression or not progression.slot:
        failures.append("ProgressionWidget canvas slot missing")
    else:
        anchors = progression.slot.get_anchors()
        offsets = progression.slot.get_offsets()
        xp_valid = (
            abs(anchors.minimum.x - 0.0) < 0.01
            and abs(anchors.minimum.y - 1.0) < 0.01
            and abs(anchors.maximum.x - 1.0) < 0.01
            and abs(anchors.maximum.y - 1.0) < 0.01
            and abs(offsets.left - 0.0) < 0.01
            and abs(offsets.top + 52.0) < 0.01
            and abs(offsets.right - 0.0) < 0.01
            and abs(offsets.bottom - 52.0) < 0.01
        )
        lines.append(f"bottom XP offsets={offsets}, full_width_bottom={xp_valid}")
        if not xp_valid:
            failures.append("ProgressionWidget is not full-width 52px bottom placement")

    skill_previews = sorted(name for name in bottom_names if name.startswith("UI_SkillPreviewSlot"))
    item_previews = sorted(name for name in bottom_names if name.startswith("UI_ItemPreviewSlot"))
    lines.append(f"preview slots: skill={len(skill_previews)}, item={len(item_previews)}")
    if len(skill_previews) != 5 or len(item_previews) != 5:
        failures.append("Bottom HUD must contain five skill and five item preview slots")

    main_hud = load_widget(MAIN_HUD_PATH)
    main_infos = widget_infos(main_hud)
    main_names = {info.widget.get_name() for info in main_infos}
    old_remaining = sorted(OLD_MAIN_BOTTOM_WIDGETS & main_names)
    bottom_entries = [info for info in main_infos if info.widget.get_name() == "BottomHUDWidget"]
    lines.append(
        f"main BottomHUDWidget count={len(bottom_entries)}, old_bottom_remaining={old_remaining}"
    )
    if len(bottom_entries) != 1:
        failures.append("Main HUD must contain exactly one BottomHUDWidget")
    if old_remaining:
        failures.append(f"Main HUD still owns old bottom widgets: {old_remaining}")
    if bottom_entries:
        slot = bottom_entries[0].slot
        anchors = slot.get_anchors()
        offsets = slot.get_offsets()
        placement_valid = (
            abs(anchors.minimum.x - 0.0) < 0.01
            and abs(anchors.minimum.y - 1.0) < 0.01
            and abs(anchors.maximum.x - 1.0) < 0.01
            and abs(anchors.maximum.y - 1.0) < 0.01
            and abs(offsets.left - 0.0) < 0.01
            and abs(offsets.top + 260.0) < 0.01
            and abs(offsets.right - 0.0) < 0.01
            and abs(offsets.bottom - 260.0) < 0.01
        )
        lines.append(f"main BottomHUDWidget offsets={offsets}, placement={placement_valid}")
        if not placement_valid:
            failures.append("BottomHUDWidget is not full-width 260px bottom placement")

    showcase = load_widget(SHOWCASE_PATH)
    showcase_names = {info.widget.get_name() for info in widget_infos(showcase)}
    showcase_count = sum(name.endswith("BottomHUD") for name in showcase_names)
    lines.append(f"showcase themed instances={showcase_count}")
    if showcase_count != 4:
        failures.append("Class Bottom HUD showcase must contain four themed instances")

    lines.append("RESULT=" + ("PASSED" if not failures else "FAILED"))
    if failures:
        lines.extend(f"FAILURE: {failure}" for failure in failures)

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        output.write("\n".join(lines) + "\n")
    for line in lines:
        unreal.log(f"[Rogue10mClassBottomHUD] {line}")
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    main()
