"""Import generated gothic HUD textures and apply them to existing UMG layouts."""

from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path("D:/Project/Rogue10m")
SOURCE_DIR = PROJECT_ROOT / "Content/UI/HUD/Gothic"
DESTINATION_PATH = "/Game/UI/HUD/Gothic"
MAIN_HUD_PATH = "/Game/Widget/WBP_Rogue10mMainHUD"
QUICK_SLOT_PATH = "/Game/Widget/Parts/WBP_QuickSlot"
VITAL_BAR_PATH = "/Game/Widget/Parts/WBP_VitalBar"
IDENTITY_PATH = "/Game/Widget/Parts/WBP_Identity"
SKILL_PANEL_PATH = "/Game/Widget/Parts/WBP_SkillSlotPanel"
ITEM_PANEL_PATH = "/Game/Widget/Parts/WBP_ItemSlotPanel"
MONSTER_INFO_PATH = "/Game/Widget/Parts/WBP_MonsterInfo"
SYSTEM_LOG_PATH = "/Game/Widget/Parts/WBP_SystemLogPanel"

TEXTURE_FILES = {
    "bar_left": "T_HUD_GothicBarLeft.png",
    "bar_right": "T_HUD_GothicBarRight.png",
    "medallion": "T_HUD_GothicMedallion.png",
    "experience": "T_HUD_GothicExperienceFrame.png",
    "slot": "T_HUD_GothicSlotFrame.png",
    "monster_info": "T_HUD_GothicMonsterInfoFrame.png",
}

ORNAMENT_WIDGETS = {
    "UI_GothicBarLeftFrame": ("bar_left", (638.0, 912.0), (410.0, 76.0)),
    "UI_GothicBarRightFrame": ("bar_right", (1142.0, 912.0), (410.0, 76.0)),
    "UI_GothicMedallionFrame": ("medallion", (985.0, 841.0), (222.0, 197.0)),
}

UMG_TOOL = unreal.get_default_object(unreal.UMGToolSet)


def umg(method: str, *args):
    return UMG_TOOL.call_method(method, args)


def load_widget_blueprint(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Widget Blueprint not found: {path}")
    return asset


def widget_infos(widget_blueprint):
    return [info for info in umg("GetWidgets", widget_blueprint).widgets if info.widget]


def find_widget(widget_blueprint, name: str):
    for info in widget_infos(widget_blueprint):
        if info.widget.get_name() == name:
            return info.widget
    return None


def root_canvas(widget_blueprint):
    roots = [
        info.widget
        for info in widget_infos(widget_blueprint)
        if not info.parent and not info.named_slot_host
    ]
    if len(roots) != 1 or roots[0].get_class().get_name() != "CanvasPanel":
        raise RuntimeError(f"Expected one CanvasPanel root, found: {roots}")
    return roots[0]


def set_canvas_layout(slot, position, size, z_order=0):
    slot.set_anchors(
        unreal.Anchors(
            minimum=unreal.Vector2D(0.0, 0.0),
            maximum=unreal.Vector2D(0.0, 0.0),
        )
    )
    slot.set_alignment(unreal.Vector2D(0.0, 0.0))
    slot.set_position(unreal.Vector2D(*position))
    slot.set_size(unreal.Vector2D(*size))
    slot.set_z_order(z_order)


def import_textures():
    tasks = []
    for filename in TEXTURE_FILES.values():
        source_path = SOURCE_DIR / filename
        if not source_path.exists():
            raise RuntimeError(f"Texture source missing: {source_path}")
        asset_path = f"{DESTINATION_PATH}/{source_path.stem}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            texture = unreal.EditorAssetLibrary.load_asset(asset_path)
            configure_texture(texture)
            continue

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source_path))
        task.set_editor_property("destination_path", DESTINATION_PATH)
        task.set_editor_property("destination_name", source_path.stem)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    textures = {}
    for key, filename in TEXTURE_FILES.items():
        asset_path = f"{DESTINATION_PATH}/{Path(filename).stem}"
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not texture:
            raise RuntimeError(f"Texture import failed: {asset_path}")
        configure_texture(texture)
        textures[key] = texture
    return textures


def configure_texture(texture) -> None:
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.modify()


def remove_existing_ornaments(widget_blueprint) -> None:
    for name in ORNAMENT_WIDGETS:
        widget = find_widget(widget_blueprint, name)
        if widget and not umg("RemoveWidget", widget_blueprint, widget):
            raise RuntimeError(f"Failed to remove existing ornament: {name}")


def apply_main_hud_ornaments(widget_blueprint, textures) -> None:
    remove_existing_ornaments(widget_blueprint)
    canvas = root_canvas(widget_blueprint)
    for name, (texture_key, position, size) in ORNAMENT_WIDGETS.items():
        info = umg("AddWidget", widget_blueprint, unreal.Image, name, canvas, -1)
        if not info.widget or not info.slot:
            raise RuntimeError(f"Failed to add ornament widget: {name}")
        image = info.widget
        image.set_brush_from_texture(textures[texture_key], True)
        image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        set_canvas_layout(info.slot, position, size, z_order=0)


def set_border_texture(widget_blueprint, widget_name: str, texture) -> None:
    border = find_widget(widget_blueprint, widget_name)
    if not border:
        raise RuntimeError(f"Border not found: {widget_name}")
    border.set_brush_from_texture(texture)
    border.set_brush_color(unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    border.set_padding(unreal.Margin(0.0, 0.0, 0.0, 0.0))


def set_border_color(widget_blueprint, widget_name: str, color, padding=None) -> None:
    border = find_widget(widget_blueprint, widget_name)
    if not border:
        raise RuntimeError(f"Border not found: {widget_name}")
    border.set_brush_color(color)
    if padding is not None:
        border.set_padding(unreal.Margin(padding, padding, padding, padding))


def compile_and_save(path: str, widget_blueprint) -> None:
    if not umg("CompileWidgetBlueprint", widget_blueprint):
        raise RuntimeError(f"Widget Blueprint compile failed: {path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        widget_blueprint, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Widget Blueprint save failed: {path}")


def main() -> None:
    if not hasattr(unreal, "UMGToolSet"):
        raise RuntimeError("UE5.8 UMGToolSet plugin is not loaded")

    bottom_path = "/Game/Widget/Parts/WBP_BottomHUD"
    if unreal.EditorAssetLibrary.does_asset_exist(bottom_path):
        bottom = load_widget_blueprint(bottom_path)
        if find_widget(bottom, "UI_CombatScaleBox"):
            raise RuntimeError("Responsive HUD is installed. Use ApplyResponsiveCombatHUD.py; legacy screen-space ornaments would overlap the responsive combat core.")
    textures = import_textures()

    main_hud = load_widget_blueprint(MAIN_HUD_PATH)
    apply_main_hud_ornaments(main_hud, textures)
    compile_and_save(MAIN_HUD_PATH, main_hud)

    quick_slot = load_widget_blueprint(QUICK_SLOT_PATH)
    set_border_texture(quick_slot, "UI_SlotFrame", textures["slot"])
    compile_and_save(QUICK_SLOT_PATH, quick_slot)

    vital_bar = load_widget_blueprint(VITAL_BAR_PATH)
    set_border_color(
        vital_bar,
        "UI_BarBackground",
        unreal.LinearColor(0.008, 0.01, 0.014, 0.94),
        1.0,
    )
    compile_and_save(VITAL_BAR_PATH, vital_bar)

    identity = load_widget_blueprint(IDENTITY_PATH)
    set_border_color(
        identity,
        "UI_IdentityFrame",
        unreal.LinearColor(0.012, 0.014, 0.018, 0.38),
        1.0,
    )
    compile_and_save(IDENTITY_PATH, identity)

    for panel_path, prefix in [
        (SKILL_PANEL_PATH, "UI_SkillSlotFrame"),
        (ITEM_PANEL_PATH, "UI_ItemSlotFrame"),
    ]:
        panel = load_widget_blueprint(panel_path)
        for index in range(1, 6):
            set_border_color(
                panel,
                f"{prefix}{index}",
                unreal.LinearColor(0.01, 0.012, 0.016, 0.20),
                1.0,
            )
        compile_and_save(panel_path, panel)

    monster = load_widget_blueprint(MONSTER_INFO_PATH)
    set_border_texture(monster, "UI_MonsterInfoBackground", textures["monster_info"])
    compile_and_save(MONSTER_INFO_PATH, monster)

    system_log = load_widget_blueprint(SYSTEM_LOG_PATH)
    set_border_color(
        system_log,
        "UI_SystemLogBackground",
        unreal.LinearColor(0.01, 0.012, 0.016, 0.76),
        8.0,
    )
    compile_and_save(SYSTEM_LOG_PATH, system_log)

    for texture in textures.values():
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("[Rogue10mCombatHUD] Gothic combat HUD textures imported and applied")


if __name__ == "__main__":
    main()
