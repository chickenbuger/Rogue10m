"""Validate the menu map, map settings, and front-end widget layout."""

import unreal


MENU_MAP_PACKAGE = "/Game/Rogue10m/Maps/L_Menu"
GAMEPLAY_MAP_PACKAGE = "/Game/Rogue10m/Maps/StartHub"
MAIN_MENU_PATH = "/Game/Widget/Menu/WBP_MainMenu"
LOBBY_PATH = "/Game/Widget/Lobby/WBP_CharacterLobby"


def fail(message):
    raise RuntimeError(f"[Rogue10mMenuMapValidation] {message}")


def require_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        fail(f"Missing asset: {path}")
    return asset


def require_stretch_slot(widget_blueprint, widget_name):
    tool = unreal.get_default_object(unreal.UMGToolSet)
    tree = tool.call_method("GetWidgets", (widget_blueprint,))
    widget = next(
        (
            info.widget
            for info in tree.widgets
            if info.widget and info.widget.get_name() == widget_name
        ),
        None,
    )
    if not widget:
        fail(f"Missing widget: {widget_name}")
    slot = widget.slot
    anchors = slot.get_anchors()
    offsets = slot.get_offsets()
    if (
        anchors.minimum != unreal.Vector2D(0.0, 0.0)
        or anchors.maximum != unreal.Vector2D(1.0, 1.0)
    ):
        fail(f"{widget_name} does not use full-screen Stretch anchors.")
    if any(
        abs(value) > 0.01
        for value in (
            offsets.left,
            offsets.top,
            offsets.right,
            offsets.bottom,
        )
    ):
        fail(f"{widget_name} has non-zero Stretch offsets: {offsets}")


def main():
    require_asset(MENU_MAP_PACKAGE)
    require_asset(GAMEPLAY_MAP_PACKAGE)
    main_menu = require_asset(MAIN_MENU_PATH)
    lobby = require_asset(LOBBY_PATH)

    menu_game_mode = unreal.load_class(
        None, "/Script/Rogue10m.Rogue10mMenuGameMode"
    )
    if not menu_game_mode:
        fail("Native Rogue10mMenuGameMode class is unavailable.")

    if not unreal.EditorLevelLibrary.load_level(MENU_MAP_PACKAGE):
        fail("Failed to load Menu Map for World Settings validation.")
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings() if world else None
    configured_game_mode = (
        world_settings.get_editor_property("default_game_mode")
        if world_settings
        else None
    )
    if configured_game_mode != menu_game_mode:
        fail(f"Unexpected Menu Map GameMode: {configured_game_mode}")

    controller_class = unreal.load_class(
        None, "/Script/Rogue10m.Rogue10mPlayerController"
    )
    if not controller_class:
        fail("Native Rogue10mPlayerController class is unavailable.")
    controller_cdo = unreal.get_default_object(controller_class)
    gameplay_map = controller_cdo.get_editor_property("gameplay_map")
    if GAMEPLAY_MAP_PACKAGE not in str(gameplay_map):
        fail(f"Unexpected gameplay map: {gameplay_map}")

    require_stretch_slot(main_menu, "UI_MainMenuBackgroundImage")
    require_stretch_slot(main_menu, "UI_MainMenuShade")
    require_stretch_slot(lobby, "UI_LobbyBackgroundImage")

    unreal.log(
        "[Rogue10mMenuMapValidation] PASSED: "
        "Menu Map + starting hub travel target + full-screen UI"
    )


if __name__ == "__main__":
    main()
