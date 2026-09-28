"""Import class HUD frames, build WBP_BottomHUD, and place it once in Main HUD."""

from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path("D:/Project/Rogue10m")
SOURCE_DIR = PROJECT_ROOT / "Content/UI/HUD/ClassThemes"
TEXTURE_DESTINATION = "/Game/UI/HUD/ClassThemes"
BOTTOM_HUD_PATH = "/Game/Widget/Parts/WBP_BottomHUD"
MAIN_HUD_PATH = "/Game/Widget/WBP_Rogue10mMainHUD"
SHOWCASE_PATH = "/Game/Widget/Debug/WBP_ClassBottomHUDShowcase"

THEME_TEXTURE_FILES = {
    "Wizard": "T_HUD_Bottom_Wizard.png",
    "Warrior": "T_HUD_Bottom_Warrior.png",
    "MartialArtist": "T_HUD_Bottom_MartialArtist.png",
    "Rogue": "T_HUD_Bottom_Rogue.png",
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


def find_info(widget_blueprint, name: str):
    for info in widget_infos(widget_blueprint):
        if info.widget.get_name() == name:
            return info
    raise RuntimeError(f"Widget not found: {name}")


def add(widget_blueprint, widget_class, name: str, parent=None, variable=False):
    info = umg("AddWidget", widget_blueprint, widget_class, name, parent, -1)
    if not info.widget:
        raise RuntimeError(f"Failed to add widget: {name}")
    if variable:
        umg("ToggleWidgetAsVariable", widget_blueprint, info.widget, True)
    return info.widget, info.slot


def clear_tree(widget_blueprint) -> None:
    roots = [
        info.widget
        for info in widget_infos(widget_blueprint)
        if not info.parent and not info.named_slot_host
    ]
    for root in roots:
        if not umg("RemoveWidget", widget_blueprint, root):
            raise RuntimeError(f"Failed to clear widget root: {root.get_name()}")


def create_or_load_widget(path: str, parent_class_path: str):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return load_widget(path)

    parent_class = unreal.load_class(None, parent_class_path)
    if not parent_class:
        raise RuntimeError(f"Widget parent class not found: {parent_class_path}")
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    name = path.rsplit("/", 1)[1]
    package_path = path.rsplit("/", 1)[0]
    widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        package_path,
        unreal.WidgetBlueprint,
        factory,
    )
    if not widget:
        raise RuntimeError(f"Failed to create Widget Blueprint: {path}")
    return widget


def compile_and_save(path: str, widget_blueprint) -> None:
    if not umg("CompileWidgetBlueprint", widget_blueprint):
        raise RuntimeError(f"Widget Blueprint compile failed: {path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(widget_blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Widget Blueprint save failed: {path}")
    unreal.log(f"[Rogue10mClassBottomHUD] Compiled and saved: {path}")


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


def import_textures():
    tasks = []
    for filename in THEME_TEXTURE_FILES.values():
        source_path = SOURCE_DIR / filename
        if not source_path.exists():
            raise RuntimeError(f"Theme texture source missing: {source_path}")
        asset_path = f"{TEXTURE_DESTINATION}/{source_path.stem}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source_path))
        task.set_editor_property("destination_path", TEXTURE_DESTINATION)
        task.set_editor_property("destination_name", source_path.stem)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        tasks.append(task)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    textures = {}
    for theme, filename in THEME_TEXTURE_FILES.items():
        path = f"{TEXTURE_DESTINATION}/{Path(filename).stem}"
        texture = unreal.EditorAssetLibrary.load_asset(path)
        if not texture:
            raise RuntimeError(f"Theme texture import failed: {path}")
        configure_texture(texture)
        textures[theme] = texture
    return textures


def set_canvas_slot(slot, *, anchors, offsets, alignment=(0.0, 0.0), z_order=0):
    slot.set_anchors(
        unreal.Anchors(
            minimum=unreal.Vector2D(anchors[0], anchors[1]),
            maximum=unreal.Vector2D(anchors[2], anchors[3]),
        )
    )
    slot.set_alignment(unreal.Vector2D(alignment[0], alignment[1]))
    slot.set_offsets(unreal.Margin(*offsets))
    slot.set_z_order(z_order)


def set_text_style(text_block, size: int, color: unreal.LinearColor) -> None:
    font = text_block.get_editor_property("font")
    font.size = size
    text_block.set_editor_property("font", font)
    text_block.set_color_and_opacity(unreal.SlateColor(specified_color=color))
    text_block.set_shadow_offset(unreal.Vector2D(1.0, 1.0))
    text_block.set_shadow_color_and_opacity(unreal.LinearColor(0.0, 0.0, 0.0, 0.94))
    text_block.set_editor_property("justification", unreal.TextJustify.CENTER)


def build_bottom_hud(textures) -> None:
    bottom = create_or_load_widget(
        BOTTOM_HUD_PATH,
        "/Script/Rogue10m.Rogue10mBottomHUDWidget",
    )
    clear_tree(bottom)
    canvas, _ = add(bottom, unreal.CanvasPanel, "UI_BottomHUDCanvas")

    frame_names = {
        "Wizard": "UI_WizardThemeFrame",
        "Warrior": "UI_WarriorThemeFrame",
        "MartialArtist": "UI_MartialArtistThemeFrame",
        "Rogue": "UI_RogueThemeFrame",
    }
    for theme, widget_name in frame_names.items():
        frame, slot = add(bottom, unreal.Image, widget_name, canvas, variable=True)
        frame.set_brush_from_texture(textures[theme], True)
        frame.set_visibility(
            unreal.SlateVisibility.HIT_TEST_INVISIBLE
            if theme == "MartialArtist"
            else unreal.SlateVisibility.COLLAPSED
        )
        set_canvas_slot(
            slot,
            anchors=(0.0, 0.0, 1.0, 0.0),
            offsets=(0.0, 0.0, 0.0, 208.0),
            z_order=0,
        )

    vital_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/Widget/Parts/WBP_VitalBar"
    )
    progression_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/Widget/Parts/WBP_LevelExperiencePanel"
    )
    identity_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/Widget/Parts/WBP_Identity"
    )
    quick_slot_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/Widget/Parts/WBP_QuickSlot"
    )
    if not all((vital_class, progression_class, identity_class, quick_slot_class)):
        raise RuntimeError("A required HUD part Widget Blueprint class is missing")

    health, health_slot = add(
        bottom, vital_class, "HealthBarWidget", canvas, variable=True
    )
    set_canvas_slot(
        health_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(150.0, 72.0, 650.0, 34.0),
        z_order=3,
    )

    stamina, stamina_slot = add(
        bottom, vital_class, "StaminaBarWidget", canvas, variable=True
    )
    set_canvas_slot(
        stamina_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(1120.0, 72.0, 650.0, 34.0),
        z_order=3,
    )

    mana, mana_slot = add(bottom, vital_class, "ManaBarWidget", canvas, variable=True)
    mana.set_visibility(unreal.SlateVisibility.COLLAPSED)
    set_canvas_slot(
        mana_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(1120.0, 72.0, 650.0, 34.0),
        z_order=4,
    )

    identity, identity_slot = add(
        bottom, identity_class, "IdentityWidget", canvas, variable=True
    )
    set_canvas_slot(
        identity_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(912.0, 42.0, 96.0, 116.0),
        z_order=5,
    )

    identity_bar, identity_bar_slot = add(
        bottom, vital_class, "IdentityBarWidget", canvas, variable=True
    )
    identity_bar.set_visibility(unreal.SlateVisibility.COLLAPSED)
    set_canvas_slot(
        identity_bar_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(820.0, 187.0, 280.0, 15.0),
        z_order=5,
    )

    skill_box, skill_box_slot = add(
        bottom, unreal.HorizontalBox, "SkillSlotContainer", canvas, variable=True
    )
    set_canvas_slot(
        skill_box_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(270.0, 132.0, 300.0, 56.0),
        z_order=4,
    )

    item_box, item_box_slot = add(
        bottom, unreal.HorizontalBox, "ItemSlotContainer", canvas, variable=True
    )
    set_canvas_slot(
        item_box_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(1350.0, 132.0, 300.0, 56.0),
        z_order=4,
    )

    for index in range(5):
        _, skill_slot = add(
            bottom,
            quick_slot_class,
            f"UI_SkillPreviewSlot{index + 1}",
            skill_box,
        )
        skill_slot.set_padding(unreal.Margin(3.0, 0.0, 3.0, 0.0))
        _, item_slot = add(
            bottom,
            quick_slot_class,
            f"UI_ItemPreviewSlot{index + 1}",
            item_box,
        )
        item_slot.set_padding(unreal.Margin(3.0, 0.0, 3.0, 0.0))

    progression, progression_slot = add(
        bottom,
        progression_class,
        "ProgressionWidget",
        canvas,
        variable=True,
    )
    set_canvas_slot(
        progression_slot,
        anchors=(0.0, 1.0, 1.0, 1.0),
        offsets=(0.0, -52.0, 0.0, 52.0),
        z_order=8,
    )

    theme_name, theme_name_slot = add(
        bottom, unreal.TextBlock, "UI_ThemeNameText", canvas, variable=True
    )
    theme_name.set_text("권사")
    set_text_style(theme_name, 13, unreal.LinearColor(0.86, 0.72, 0.38, 1.0))
    set_canvas_slot(
        theme_name_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(880.0, 162.0, 160.0, 20.0),
        z_order=7,
    )

    resource_name, resource_name_slot = add(
        bottom, unreal.TextBlock, "UI_ResourceTypeText", canvas, variable=True
    )
    resource_name.set_text("스테미나")
    set_text_style(resource_name, 11, unreal.LinearColor(0.94, 0.74, 0.30, 1.0))
    set_canvas_slot(
        resource_name_slot,
        anchors=(0.0, 0.0, 0.0, 0.0),
        offsets=(1570.0, 53.0, 120.0, 18.0),
        z_order=6,
    )

    for label, x in (("SKILL", 335.0), ("ITEM", 1415.0)):
        text, text_slot = add(bottom, unreal.TextBlock, f"UI_{label}Label", canvas)
        text.set_text(label)
        set_text_style(text, 10, unreal.LinearColor(0.72, 0.62, 0.44, 0.88))
        set_canvas_slot(
            text_slot,
            anchors=(0.0, 0.0, 0.0, 0.0),
            offsets=(x, 112.0, 160.0, 18.0),
            z_order=5,
        )

    compile_and_save(BOTTOM_HUD_PATH, bottom)


def apply_main_hud() -> None:
    main = load_widget(MAIN_HUD_PATH)
    infos = widget_infos(main)
    roots = [info.widget for info in infos if not info.parent and not info.named_slot_host]
    if len(roots) != 1 or roots[0].get_class().get_name() != "CanvasPanel":
        raise RuntimeError("Main HUD must have one CanvasPanel root")
    root = roots[0]

    for info in list(infos):
        if info.widget.get_name() in OLD_MAIN_BOTTOM_WIDGETS:
            if not umg("RemoveWidget", main, info.widget):
                raise RuntimeError(f"Failed to remove old main-HUD child: {info.widget.get_name()}")

    for info in widget_infos(main):
        if info.widget.get_name() == "BottomHUDWidget":
            if not umg("RemoveWidget", main, info.widget):
                raise RuntimeError("Failed to replace existing BottomHUDWidget")

    bottom_class = unreal.EditorAssetLibrary.load_blueprint_class(BOTTOM_HUD_PATH)
    if not bottom_class:
        raise RuntimeError("WBP_BottomHUD generated class is missing")
    _, slot = add(main, bottom_class, "BottomHUDWidget", root, variable=True)
    set_canvas_slot(
        slot,
        anchors=(0.0, 1.0, 1.0, 1.0),
        offsets=(0.0, -260.0, 0.0, 260.0),
        z_order=6,
    )
    compile_and_save(MAIN_HUD_PATH, main)


def build_showcase() -> None:
    showcase = create_or_load_widget(SHOWCASE_PATH, "/Script/UMG.UserWidget")
    clear_tree(showcase)
    canvas, _ = add(showcase, unreal.CanvasPanel, "UI_ShowcaseCanvas")

    background, background_slot = add(
        showcase, unreal.Border, "UI_ShowcaseBackground", canvas
    )
    background.set_brush_color(unreal.LinearColor(0.006, 0.009, 0.008, 1.0))
    set_canvas_slot(
        background_slot,
        anchors=(0.0, 0.0, 1.0, 1.0),
        offsets=(0.0, 0.0, 0.0, 0.0),
        z_order=0,
    )

    bottom_class = unreal.EditorAssetLibrary.load_blueprint_class(BOTTOM_HUD_PATH)
    enum_class = getattr(unreal, "Rogue10mBottomHUDTheme", None)
    if not bottom_class or not enum_class:
        raise RuntimeError("Bottom HUD class or theme enum is unavailable")
    theme_values = [
        enum_class.WIZARD,
        enum_class.WARRIOR,
        enum_class.MARTIAL_ARTIST,
        enum_class.ROGUE,
    ]
    theme_labels = ["마법사", "전사", "권사", "도적"]

    for index, (theme_value, label) in enumerate(zip(theme_values, theme_labels)):
        child, child_slot = add(
            showcase,
            bottom_class,
            f"UI_{label}BottomHUD",
            canvas,
        )
        child.set_editor_property("theme_override", theme_value)
        set_canvas_slot(
            child_slot,
            anchors=(0.0, 0.0, 0.0, 0.0),
            offsets=(0.0, float(index * 260), 1920.0, 260.0),
            z_order=1,
        )

    compile_and_save(SHOWCASE_PATH, showcase)


def main() -> None:
    if not hasattr(unreal, "UMGToolSet"):
        raise RuntimeError("UE5.8 UMGToolSet plugin is not loaded")
    if unreal.EditorAssetLibrary.does_asset_exist(BOTTOM_HUD_PATH):
        bottom = load_widget(BOTTOM_HUD_PATH)
        if any(info.widget.get_name() == "UI_CombatScaleBox" for info in widget_infos(bottom)):
            raise RuntimeError("Responsive HUD is installed. Use ApplyResponsiveCombatHUD.py; the legacy 1920px layout would overwrite its shared scaling hierarchy.")
    textures = import_textures()
    build_bottom_hud(textures)
    apply_main_hud()
    build_showcase()
    for texture in textures.values():
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("[Rogue10mClassBottomHUD] Application completed")


if __name__ == "__main__":
    main()
