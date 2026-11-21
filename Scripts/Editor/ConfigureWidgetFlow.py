"""Move widget assets into their runtime roles and build the basic main menu."""

from pathlib import Path
import sys

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import CreateCharacterCustomizationAssets as base  # noqa: E402


OLD_COMPONENT_ROOT = "/Game/Widget/Menu"
COMPONENT_ROOT = "/Game/Widget/Component"
OLD_LOBBY_PATH = "/Game/Widget/Character/WBP_CharacterLobby"
LOBBY_PATH = "/Game/Widget/Lobby/WBP_CharacterLobby"
MAIN_MENU_PATH = "/Game/Widget/Menu/WBP_MainMenu"
BACKGROUND_TEXTURE_PATH = "/Game/Rogue10m/UI/Textures/CharacterLobby/T_CharacterLobby_Background"


def log(message):
    unreal.log(f"[Rogue10mWidgetFlow] {message}")


def move_existing_assets():
    if (
        unreal.EditorAssetLibrary.does_directory_exist(OLD_COMPONENT_ROOT)
        and not unreal.EditorAssetLibrary.does_directory_exist(COMPONENT_ROOT)
    ):
        if not unreal.EditorAssetLibrary.rename_directory(
            OLD_COMPONENT_ROOT, COMPONENT_ROOT
        ):
            raise RuntimeError("Menu -> Component 디렉터리 이동 실패")
        log(f"공용 위젯 이동: {COMPONENT_ROOT}")

    old_lobby = unreal.EditorAssetLibrary.does_asset_exist(OLD_LOBBY_PATH)
    new_lobby = unreal.EditorAssetLibrary.does_asset_exist(LOBBY_PATH)
    if old_lobby and not new_lobby:
        if not unreal.EditorAssetLibrary.rename_asset(
            OLD_LOBBY_PATH, LOBBY_PATH
        ):
            raise RuntimeError("Character Lobby 에셋 이동 실패")
        log(f"캐릭터 로비 이동: {LOBBY_PATH}")


def create_or_load_main_menu():
    existing = unreal.load_asset(MAIN_MENU_PATH)
    if existing:
        return existing

    parent_class = unreal.load_class(
        None, "/Script/Rogue10m.Rogue10mMainMenuWidget"
    )
    if not parent_class:
        raise RuntimeError("Rogue10mMainMenuWidget C++ 클래스를 찾지 못했습니다.")
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    package_path, asset_name = MAIN_MENU_PATH.rsplit("/", 1)
    widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, unreal.WidgetBlueprint, factory
    )
    if not widget:
        raise RuntimeError("WBP_MainMenu 생성 실패")
    return widget


def canvas_place(slot, x, y, width, height, z=0):
    base.set_canvas_layout(
        slot,
        (x, y),
        (width, height),
        anchors=(0.0, 0.0),
        alignment=(0.0, 0.0),
        z_order=z,
    )


def canvas_stretch(slot, z=0):
    slot.set_anchors(
        unreal.Anchors(
            minimum=unreal.Vector2D(0.0, 0.0),
            maximum=unreal.Vector2D(1.0, 1.0),
        )
    )
    slot.set_alignment(unreal.Vector2D(0.0, 0.0))
    slot.set_offsets(unreal.Margin(0.0, 0.0, 0.0, 0.0))
    slot.set_z_order(z)

def add_menu_button(widget, root, name, label, y, color):
    button, slot = base.add(
        widget, unreal.Button, name, root, variable=True
    )
    canvas_place(slot, 680, y, 560, 94, 4)
    button.set_background_color(color)
    text, text_slot = base.make_text(
        widget, f"{name}_Label", label, button, font_size=30
    )
    base.set_fill_alignment(text_slot)
    text.set_editor_property("justification", unreal.TextJustify.CENTER)


def build_main_menu():
    widget = create_or_load_main_menu()
    base.clear_tree(widget)
    root, _ = base.add(widget, unreal.CanvasPanel, "UI_MainMenuRoot")

    background_texture = unreal.load_asset(BACKGROUND_TEXTURE_PATH)
    if background_texture:
        background, slot = base.add(
            widget, unreal.Image, "UI_MainMenuBackgroundImage", root
        )
        canvas_stretch(slot, 0)
        background.set_brush_from_texture(background_texture, False)

    shade, shade_slot = base.make_border(
        widget,
        "UI_MainMenuShade",
        root,
        unreal.LinearColor(0.005, 0.012, 0.024, 0.68),
        0.0,
    )
    canvas_stretch(shade_slot, 1)

    title, title_slot = base.make_text(
        widget, "UI_MainMenuTitle", "ROGUE 10M", root, font_size=82
    )
    canvas_place(title_slot, 510, 205, 900, 130, 3)
    title.set_editor_property("justification", unreal.TextJustify.CENTER)

    subtitle, subtitle_slot = base.make_text(
        widget,
        "UI_MainMenuSubtitle",
        "ADVENTURE BEGINS",
        root,
        font_size=28,
    )
    canvas_place(subtitle_slot, 610, 340, 700, 64, 3)
    subtitle.set_editor_property("justification", unreal.TextJustify.CENTER)

    add_menu_button(
        widget,
        root,
        "UI_StartGameButton",
        "게임 시작",
        520,
        unreal.LinearColor(0.08, 0.38, 0.66, 0.96),
    )
    add_menu_button(
        widget,
        root,
        "UI_QuitGameButton",
        "게임 종료",
        638,
        unreal.LinearColor(0.08, 0.11, 0.16, 0.96),
    )

    unreal.BlueprintEditorLibrary.compile_blueprint(widget)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        widget, only_if_is_dirty=False
    ):
        raise RuntimeError("WBP_MainMenu 저장 실패")
    log(f"기본 메인 메뉴 생성: {MAIN_MENU_PATH}")


def fix_redirectors():
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        assets = registry.get_assets_by_path(
            "/Game/Widget", recursive=True, include_only_on_disk_assets=False
        )
        redirectors = []
        for asset_data in assets:
            if str(asset_data.asset_class_path.asset_name) == "ObjectRedirector":
                asset = asset_data.get_asset()
                if asset:
                    redirectors.append(asset)
        if redirectors:
            unreal.AssetToolsHelpers.get_asset_tools().fixup_referencers(
                redirectors
            )
            log(f"Redirector 정리: {len(redirectors)}개")
    except Exception as error:
        unreal.log_warning(f"Redirector 자동 정리 생략: {error}")


def main():
    move_existing_assets()
    build_main_menu()
    fix_redirectors()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


if __name__ == "__main__":
    main()
