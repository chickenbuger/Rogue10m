"""Validate the approved three-profile selection layout and input boundaries."""

import unreal


WIDGET_PATH = "/Game/Widget/Lobby/WBP_CharacterLobby"
BACKGROUND_TEXTURE_PATH = "/Game/Rogue10m/UI/Textures/CharacterLobby/T_CharacterLobby_Background"
PREVIEW_MATERIAL_PATH = "/Game/Material/UI/M_CharacterPreviewTransparent"
MAX_SLOT_PREVIEW_WIDTH = 384.0
EXPECTED_PREVIEW_Y = 12.0
EXPECTED_PREVIEW_WIDTH = 360.0
EXPECTED_PREVIEW_HEIGHT = 600.0
EXPECTED_SLOT_X = (350.0, 780.0, 1210.0)
MIN_ACTION_BUTTON_Z_ORDER = 8

FORBIDDEN_STAGE_WIDGETS = {
    "UI_LobbyBackdrop",
    "UI_SelectedCharacterInfoText",
    "UI_StatusText",
    "UI_Slot1StageGlow",
    "UI_Slot2StageGlow",
    "UI_Slot3StageGlow",
}
REQUIRED_STAGE_WIDGETS = {
    "UI_LobbyBackgroundImage",
    "UI_BackgroundShade",
    "UI_BottomBar",
    "UI_SelectionStagePanel",
    "UI_Slot1PreviewImage",
    "UI_Slot2PreviewImage",
    "UI_Slot3PreviewImage",
    "UI_CharacterPreviewImage",
    "UI_CreationPanel",
    "UI_Slot1Button",
    "UI_Slot2Button",
    "UI_Slot3Button",
    "UI_NewCharacterButton",
    "UI_DeleteCharacterButton",
    "UI_EnterGameButton",
    "UI_TitleText",
    "UI_SubtitleText",
    "UI_TopBar",
    "UI_HeaderGoldLine",
    "UI_ActionGoldLine",
    "UI_SelectionHintText",
}
for _index in range(1, 4):
    REQUIRED_STAGE_WIDGETS.update({
        f"UI_Slot{_index}Text",
        f"UI_Slot{_index}DetailText",
        f"UI_Slot{_index}SelectedBadge",
        f"UI_Slot{_index}EmptyButton",
        f"UI_Slot{_index}EmptyPlusH",
        f"UI_Slot{_index}EmptyPlusV",
    })


def fail(message):
    raise RuntimeError(message)


def nearly_equal(left, right, tolerance=0.01):
    return abs(left - right) <= tolerance


def require_layout(widget, name, expected):
    slot = widget.get_editor_property("slot")
    if not isinstance(slot, unreal.CanvasPanelSlot):
        fail(f"Expected CanvasPanelSlot for {name}")
    position, size = slot.get_position(), slot.get_size()
    actual = (position.x, position.y, size.x, size.y)
    if not all(nearly_equal(left, right) for left, right in zip(actual, expected)):
        fail(f"Unexpected {name} layout: {actual}; expected {expected}")


def require_text(widget, expected):
    actual = str(widget.get_text())
    if actual != expected:
        fail(f"Unexpected {widget.get_name()} text: {actual!r}; expected {expected!r}")


def main():
    background_texture = unreal.EditorAssetLibrary.load_asset(
        BACKGROUND_TEXTURE_PATH
    )
    if not background_texture:
        fail(f"Lobby background texture missing: {BACKGROUND_TEXTURE_PATH}")

    preview_material = unreal.EditorAssetLibrary.load_asset(
        PREVIEW_MATERIAL_PATH
    )
    if not preview_material:
        fail(f"Preview transparency material missing: {PREVIEW_MATERIAL_PATH}")
    if (
        preview_material.get_editor_property("material_domain")
        != unreal.MaterialDomain.MD_UI
    ):
        fail("Preview material is not in the User Interface domain")
    if (
        preview_material.get_editor_property("blend_mode")
        != unreal.BlendMode.BLEND_TRANSLUCENT
    ):
        fail("Preview material is not translucent")

    widget = unreal.EditorAssetLibrary.load_asset(WIDGET_PATH)
    if not widget:
        fail(f"Character Lobby Widget missing: {WIDGET_PATH}")

    tool = unreal.get_default_object(unreal.UMGToolSet)
    tree = tool.call_method("GetWidgets", (widget,))
    widgets = {
        info.widget.get_name(): info.widget
        for info in tree.widgets
        if info.widget
    }
    names = set(widgets)
    missing = sorted(REQUIRED_STAGE_WIDGETS - names)
    if missing:
        fail(f"Required lobby widgets missing: {missing}")
    forbidden = sorted(FORBIDDEN_STAGE_WIDGETS & names)
    if forbidden:
        fail(f"Forbidden lobby widgets remain: {forbidden}")

    require_text(widgets["UI_TitleText"], "ROGUE 10M")
    require_text(widgets["UI_SubtitleText"], "캐릭터 선택")
    require_text(widgets["UI_SelectionHintText"], "캐릭터를 더블클릭하면 접속합니다.")
    require_layout(widgets["UI_TopBar"], "UI_TopBar", (0, 0, 1920, 88))
    require_layout(widgets["UI_BottomBar"], "UI_BottomBar", (0, 930, 1920, 150))
    require_layout(widgets["UI_SelectionStagePanel"], "UI_SelectionStagePanel", (0, 105, 1920, 735))
    require_layout(widgets["UI_SelectionHintText"], "UI_SelectionHintText", (620, 864, 680, 40))
    for name, expected in (
        ("UI_NewCharacterButton", (460, 966, 275, 68)),
        ("UI_DeleteCharacterButton", (770, 966, 275, 68)),
        ("UI_EnterGameButton", (1080, 960, 385, 80)),
    ):
        require_layout(widgets[name], name, expected)

    background = widgets["UI_LobbyBackgroundImage"]
    if (
        background.get_editor_property("visibility")
        != unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
    ):
        fail("Lobby background blocks mouse input")
    background_slot = background.get_editor_property("slot")
    anchors = background_slot.get_anchors()
    offsets = background_slot.get_offsets()
    if (
        anchors.minimum != unreal.Vector2D(0.0, 0.0)
        or anchors.maximum != unreal.Vector2D(1.0, 1.0)
    ):
        fail(f"Lobby background is not full-screen stretched: {anchors}")
    if any(
        abs(value) > 0.01
        for value in (
            offsets.left,
            offsets.top,
            offsets.right,
            offsets.bottom,
        )
    ):
        fail(f"Lobby background stretch offsets are not zero: {offsets}")

    shade = widgets["UI_BackgroundShade"]
    shade_color = shade.get_editor_property("brush_color")
    if shade_color.a > 0.01:
        fail(f"Lobby background shade must be transparent: alpha={shade_color.a}")

    for name in ("UI_BackgroundShade", "UI_BottomBar"):
        if (
            widgets[name].get_editor_property("visibility")
            != unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
        ):
            fail(f"Decorative widget blocks mouse input: {name}")

    for name, item in widgets.items():
        if isinstance(item, unreal.TextBlock):
            if item.get_editor_property("visibility") == unreal.SlateVisibility.VISIBLE:
                # Text nested inside creation controls is converted below by the
                # authoring pass. No label should steal the parent button hit.
                fail(f"Text label participates in hit testing: {name}")
        if isinstance(item, unreal.Image) and "PreviewImage" not in name:
            if item.get_editor_property("visibility") not in (
                unreal.SlateVisibility.HIT_TEST_INVISIBLE,
                unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE,
            ):
                fail(f"Decorative image blocks mouse input: {name}")

    preview_layouts = []
    for index in range(1, 4):
        preview = widgets[f"UI_Slot{index}PreviewImage"]
        slot = preview.get_editor_property("slot")
        position = slot.get_position()
        size = slot.get_size()
        preview_layouts.append((position.y, size.x, size.y))
        require_layout(preview, preview.get_name(), (
            EXPECTED_SLOT_X[index - 1], EXPECTED_PREVIEW_Y,
            EXPECTED_PREVIEW_WIDTH, EXPECTED_PREVIEW_HEIGHT,
        ))
        if size.x > MAX_SLOT_PREVIEW_WIDTH:
            fail(
                f"Slot {index} preview width {size.x} exceeds "
                f"{MAX_SLOT_PREVIEW_WIDTH}"
            )
        if (
            preview.get_editor_property("visibility")
            != unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
        ):
            fail(f"Slot {index} preview blocks mouse input")

        x = EXPECTED_SLOT_X[index - 1]
        require_layout(widgets[f"UI_Slot{index}Button"], f"UI_Slot{index}Button", (x, 714, 360, 108))
        require_layout(widgets[f"UI_Slot{index}EmptyButton"], f"UI_Slot{index}EmptyButton", (x + 70, 140, 220, 400))
        require_layout(widgets[f"UI_Slot{index}SelectedBadge"], f"UI_Slot{index}SelectedBadge", (x + 102, 814, 156, 34))
        require_text(widgets[f"UI_Slot{index}Text"], f"슬롯 {index}")
        require_text(widgets[f"UI_Slot{index}DetailText"], "캐릭터 생성하기")
        empty_button = widgets[f"UI_Slot{index}EmptyButton"]
        if not isinstance(empty_button, unreal.Button):
            fail(f"Empty slot {index} does not expose a real creation button")
        if empty_button.get_editor_property("visibility") != unreal.SlateVisibility.VISIBLE:
            fail(f"Empty slot {index} is not clickable in the template")
        if empty_button.get_editor_property("slot").get_z_order() < MIN_ACTION_BUTTON_Z_ORDER:
            fail(f"Empty slot {index} sits behind its preview")
        badge = widgets[f"UI_Slot{index}SelectedBadge"]
        if badge.get_editor_property("visibility") != unreal.SlateVisibility.COLLAPSED:
            fail(f"Slot {index} selected badge must start hidden until profile state is known")

    for index, (y, width, height) in enumerate(preview_layouts, start=1):
        if not nearly_equal(y, EXPECTED_PREVIEW_Y):
            fail(f"Slot {index} preview Y differs: {y}")
        if not nearly_equal(width, EXPECTED_PREVIEW_WIDTH):
            fail(f"Slot {index} preview width differs: {width}")
        if not nearly_equal(height, EXPECTED_PREVIEW_HEIGHT):
            fail(f"Slot {index} preview height differs: {height}")

    for name in (
        "UI_Slot1Button",
        "UI_Slot2Button",
        "UI_Slot3Button",
        "UI_NewCharacterButton",
        "UI_DeleteCharacterButton",
        "UI_EnterGameButton",
    ):
        slot = widgets[name].get_editor_property("slot")
        if slot.get_z_order() < MIN_ACTION_BUTTON_Z_ORDER:
            fail(f"Action button ZOrder is too low: {name}")

    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(
        WIDGET_PATH
    )
    if not generated_class:
        fail("Character Lobby GeneratedClass missing")
    lobby_cdo = unreal.get_default_object(generated_class)
    if not lobby_cdo.get_editor_property("customization_catalog"):
        fail("Character Lobby CustomizationCatalog default reference missing")
    if not lobby_cdo.get_editor_property("preview_transparency_material"):
        fail("Character Lobby PreviewTransparencyMaterial reference missing")

    unreal.log(
        "[Rogue10mCharacterSelectionStageValidation] PASSED: "
        "navy/gold 3-profile concept + 360x600 previews + creation hit targets + full background"
    )


if __name__ == "__main__":
    main()
