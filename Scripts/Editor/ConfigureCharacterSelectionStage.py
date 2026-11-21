"""Apply the approved navy/gold three-profile lobby Designer layout.

Only the lobby Widget Blueprint is rewritten; existing art and data are reused.
"""

from pathlib import Path
import sys

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import CreateCharacterCustomizationAssets as base  # noqa: E402


WIDGET_PATH = "/Game/Widget/Lobby/WBP_CharacterLobby"
BACKGROUND_TEXTURE_PATH = (
    "/Game/Rogue10m/UI/Textures/CharacterLobby/T_CharacterLobby_Background"
)
PREVIEW_MATERIAL_PATH = "/Game/Material/UI/M_CharacterPreviewTransparent"
BACKGROUND_SOURCE = (
    PROJECT_ROOT
    / "SourceArt"
    / "UI"
    / "CharacterLobby"
    / "T_CharacterLobby_Background.png"
)

TOP_BAR = unreal.LinearColor(0.012, 0.026, 0.045, 0.94)
BOTTOM_BAR = unreal.LinearColor(0.010, 0.024, 0.042, 0.94)
CARD = unreal.LinearColor(0.025, 0.055, 0.085, 0.94)
CREATION_FRAME = unreal.LinearColor(0.018, 0.035, 0.06, 0.96)
GOLD = unreal.LinearColor(0.63, 0.40, 0.12, 1.0)
PALE_GOLD = unreal.LinearColor(0.92, 0.68, 0.28, 1.0)
IVORY = unreal.LinearColor(0.96, 0.91, 0.78, 1.0)
SOFT_TEXT = unreal.LinearColor(0.86, 0.88, 0.90, 1.0)
PRIMARY = unreal.LinearColor(0.40, 0.22, 0.045, 1.0)
SLOT_X = (350, 780, 1210)
PREVIEW_SIZE = (360, 600)
STAGE_Y = 105
NAMEPLATE_Y = 714


def log(message):
    unreal.log(f"[Rogue10mCharacterSelectionStage] {message}")


def import_background():
    existing = unreal.EditorAssetLibrary.load_asset(
        BACKGROUND_TEXTURE_PATH
    )
    if existing:
        log("기존 배경 Texture를 변경 없이 재사용합니다.")
        return existing
    if not BACKGROUND_SOURCE.is_file():
        raise RuntimeError(f"배경 원본과 Texture 누락: {BACKGROUND_SOURCE}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(BACKGROUND_SOURCE))
    task.set_editor_property(
        "destination_path", "/Game/Rogue10m/UI/Textures/CharacterLobby"
    )
    task.set_editor_property(
        "destination_name", "T_CharacterLobby_Background"
    )
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(BACKGROUND_TEXTURE_PATH)
    if not texture:
        raise RuntimeError("캐릭터 선택 배경 Texture Import 실패")
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(
        texture, only_if_is_dirty=False
    )
    log(f"누락 배경 Texture 최초 생성: {BACKGROUND_TEXTURE_PATH}")
    return texture


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


def widget_map(widget):
    tree = base.umg("GetWidgets", widget)
    return {info.widget.get_name(): info.widget for info in tree.widgets if info.widget}


def upsert(widget, widget_class, name, parent=None, variable=False):
    """Keep required BindWidget nodes present at every structural compile.

    Official MoveWidget marks structure only after attaching the existing node
    to its new parent. Never delete/recreate a required control during this pass.
    """
    item = widget_map(widget).get(name)
    if item is None:
        return base.add(widget, widget_class, name, parent, variable=variable)
    if not isinstance(item, widget_class):
        raise RuntimeError(f"기존 위젯 형식 불일치: {name}")
    if parent is not None and item.get_parent() != parent:
        info = base.umg("MoveWidget", widget, item, parent, -1)
        if not info.widget:
            raise RuntimeError(f"기존 위젯 이동 실패: {name}")
        return info.widget, info.slot
    return item, item.get_editor_property("slot")


def border(widget, name, parent, color, padding=0.0):
    item, slot = upsert(widget, unreal.Border, name, parent)
    item.set_brush_color(color)
    item.set_padding(unreal.Margin(padding, padding, padding, padding))
    return item, slot


def make_button(
    widget,
    parent,
    name,
    label,
    x,
    y,
    width,
    height,
    color=CARD,
):
    result, button_slot = upsert(widget, unreal.Button, name, parent, variable=True)
    canvas_place(button_slot, x, y, width, height, 8)
    result.set_background_color(color)
    result.set_visibility(unreal.SlateVisibility.VISIBLE)
    if label is not None:
        text = result.get_content()
        if text is None:
            text, text_slot = upsert(widget, unreal.TextBlock, name + "_Label", result)
            base.set_fill_alignment(text_slot)
        if not isinstance(text, unreal.TextBlock):
            raise RuntimeError(f"기존 버튼 라벨 형식 불일치: {name}")
        text.set_text(label)
        text.set_color_and_opacity(unreal.SlateColor(specified_color=IVORY))
        text.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        text.set_editor_property("justification", unreal.TextJustify.CENTER)
        font = text.get_editor_property("font")
        font.size = 30 if name == "UI_EnterGameButton" else (23 if height >= 58 else 18)
        text.set_editor_property("font", font)
    return result


def label(widget, parent, name, value, bounds, font_size=20, color=IVORY,
          variable=False, center=True):
    text, slot = upsert(widget, unreal.TextBlock, name, parent, variable=variable)
    text.set_text(value)
    font = text.get_editor_property("font")
    font.size = font_size
    text.set_editor_property("font", font)
    if isinstance(slot, unreal.CanvasPanelSlot):
        canvas_place(slot, *bounds, z=4)
    else:
        base.set_fill_alignment(slot)
    text.set_color_and_opacity(unreal.SlateColor(specified_color=color))
    text.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    text.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
    text.set_text_overflow_policy(unreal.TextOverflowPolicy.ELLIPSIS)
    text.set_editor_property(
        "justification", unreal.TextJustify.CENTER if center else unreal.TextJustify.LEFT
    )
    return text


def ornament_rect(widget, parent, name, bounds, color=GOLD, z=4):
    image, slot = upsert(widget, unreal.Image, name, parent)
    image.set_color_and_opacity(color)
    image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    canvas_place(slot, *bounds, z=z)
    return image


def diamond(widget, parent, name, center, side=7, color=PALE_GOLD):
    image = ornament_rect(
        widget, parent, name,
        (center[0] - side / 2, center[1] - side / 2, side, side), color
    )
    image.set_render_transform_pivot(unreal.Vector2D(0.5, 0.5))
    image.set_render_transform_angle(45)


def add_empty_slot(widget, stage, index, x):
    button = make_button(
        widget, stage, f"UI_Slot{index}EmptyButton", None,
        x + 70, 140, 220, 400, unreal.LinearColor(0.035, 0.065, 0.10, 0.18)
    )
    canvas, canvas_slot = upsert(
        widget, unreal.CanvasPanel, f"UI_Slot{index}EmptyCanvas", button
    )
    base.set_fill_alignment(canvas_slot)
    canvas.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    # Native styling supplies the outlined frame and binds the same slot action.
    ornament_rect(widget, canvas, f"UI_Slot{index}EmptyPlusH",
                  (77, 196, 66, 5), PALE_GOLD)
    ornament_rect(widget, canvas, f"UI_Slot{index}EmptyPlusV",
                  (107, 166, 5, 66), PALE_GOLD)


def add_nameplate(widget, root, index, x):
    button = make_button(
        widget, root, f"UI_Slot{index}Button", None,
        x, NAMEPLATE_Y, 360, 108, CARD
    )
    name = f"UI_Slot{index}NameplateCanvas"
    canvas = widget_map(widget).get(name)
    if canvas is None:
        # Keep the old required TextBlock reachable while introducing its new
        # layout container; the old button is only filled after that text moves.
        canvas, _ = upsert(widget, unreal.CanvasPanel, name, root)
    elif not isinstance(canvas, unreal.CanvasPanel):
        raise RuntimeError(f"기존 이름표 컨테이너 형식 불일치: {name}")
    canvas.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    label(widget, canvas, f"UI_Slot{index}Text", f"슬롯 {index}",
          (18, 9, 324, 42), 28, variable=True)
    label(widget, canvas, f"UI_Slot{index}DetailText", "캐릭터 생성하기",
          (18, 59, 324, 29), 20, SOFT_TEXT, variable=True)
    ornament_rect(widget, canvas, f"UI_Slot{index}NameDivider",
                  (61, 53, 238, 1), unreal.LinearColor(0.54, 0.38, 0.18, 0.5))
    diamond(widget, canvas, f"UI_Slot{index}LeftDiamond", (18, 54), 5)
    diamond(widget, canvas, f"UI_Slot{index}RightDiamond", (342, 54), 5)
    canvas, canvas_slot = upsert(widget, unreal.CanvasPanel, name, button)
    base.set_fill_alignment(canvas_slot)
    badge, badge_slot = border(
        widget, f"UI_Slot{index}SelectedBadge", root, PRIMARY, 0.0
    )
    canvas_place(badge_slot, x + 102, 814, 156, 34, 10)
    badge.set_visibility(unreal.SlateVisibility.COLLAPSED)
    label(widget, badge, f"UI_Slot{index}SelectedText", "선택됨",
          (0, 0, 156, 34), 19, IVORY)


def add_stage_character(widget, parent, index, x, y, width, height):
    image, image_slot = upsert(
        widget,
        unreal.Image,
        f"UI_Slot{index}PreviewImage",
        parent,
        variable=True,
    )
    canvas_place(image_slot, x, y, width, height, 1)
    image.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)


def build_widget(background_texture):
    from ValidateCharacterCustomizationAssets import REQUIRED_WIDGETS
    widget = base.require_asset(WIDGET_PATH)
    required = set(REQUIRED_WIDGETS) | {"UI_CharacterLobbyRoot", "UI_SelectionStagePanel"}
    required.update(f"UI_Slot{index}PreviewImage" for index in range(1, 4))
    missing = sorted(required - widget_map(widget).keys())
    if missing:
        raise RuntimeError(f"기존 필수 바인딩 누락으로 편집 중단: {missing}")

    root, _ = upsert(
        widget, unreal.CanvasPanel, "UI_CharacterLobbyRoot"
    )
    root.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    background, background_slot = upsert(
        widget, unreal.Image, "UI_LobbyBackgroundImage", root
    )
    canvas_stretch(background_slot, 0)
    background.set_brush_from_texture(background_texture, False)
    background.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

    shade, shade_slot = border(
        widget,
        "UI_BackgroundShade",
        root,
        unreal.LinearColor(0.0, 0.0, 0.0, 0.0),
        0.0,
    )
    canvas_place(shade_slot, 0, 0, 1920, 1080, 1)
    shade.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

    top_bar, top_bar_slot = border(
        widget, "UI_TopBar", root, TOP_BAR, 0.0
    )
    canvas_place(top_bar_slot, 0, 0, 1920, 88, 2)
    top_bar.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    ornament_rect(widget, root, "UI_HeaderGoldLine", (0, 86, 1920, 1))
    label(widget, root, "UI_TitleText", "ROGUE 10M",
          (56, 12, 500, 64), 38, IVORY, center=False)
    label(widget, root, "UI_SubtitleText", "캐릭터 선택",
          (720, 12, 480, 64), 36, IVORY)
    for suffix, line_x, diamond_x in (("Left", 600, 714), ("Right", 1206, 1200)):
        ornament_rect(widget, root, "UI_Header" + suffix + "Rule",
                      (line_x, 44, 108, 1))
        diamond(widget, root, "UI_Header" + suffix + "Diamond", (diamond_x, 44))

    stage, stage_slot = upsert(
        widget,
        unreal.CanvasPanel,
        "UI_SelectionStagePanel",
        root,
        variable=True,
    )
    canvas_place(stage_slot, 0, STAGE_Y, 1920, 735, 3)
    stage.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    for index, x in enumerate(SLOT_X, start=1):
        add_stage_character(widget, stage, index, x, 12, *PREVIEW_SIZE)
        add_empty_slot(widget, stage, index, x)

    draft_image, draft_slot = upsert(
        widget,
        unreal.Image,
        "UI_CharacterPreviewImage",
        root,
        variable=True,
    )
    canvas_place(draft_slot, 130, 120, 900, 820, 4)

    bottom_bar, bottom_bar_slot = border(
        widget, "UI_BottomBar", root, BOTTOM_BAR, 0.0
    )
    canvas_place(bottom_bar_slot, 0, 930, 1920, 150, 4)
    bottom_bar.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    ornament_rect(widget, root, "UI_ActionGoldLine", (0, 930, 1920, 1), PALE_GOLD)

    for index, x in enumerate(SLOT_X, start=1):
        add_nameplate(widget, root, index, x)
    label(widget, root, "UI_SelectionHintText", "캐릭터를 더블클릭하면 접속합니다.",
          (620, 864, 680, 40), 22, SOFT_TEXT)
    ornament_rect(widget, root, "UI_SelectionHintRule", (470, 884, 110, 1))
    diamond(widget, root, "UI_SelectionHintDiamond", (586, 884), 5)

    make_button(
        widget,
        root,
        "UI_NewCharacterButton",
        "새 캐릭터",
        460,
        966,
        275,
        68,
    )
    make_button(
        widget,
        root,
        "UI_DeleteCharacterButton",
        "캐릭터 삭제",
        770,
        966,
        275,
        68,
    )
    make_button(
        widget,
        root,
        "UI_EnterGameButton",
        "게임 접속",
        1080,
        960,
        385,
        80,
        PRIMARY,
    )

    # Preserve the entire functional creation subtree and its existing bindings.
    creation_frame = widget_map(widget)["UI_CreationPanel"]
    if not isinstance(creation_frame, unreal.Border):
        raise RuntimeError("UI_CreationPanel 형식 불일치")
    creation_frame.set_brush_color(CREATION_FRAME)
    creation_frame.set_padding(unreal.Margin(14.0, 14.0, 14.0, 14.0))
    canvas_place(creation_frame.get_editor_property("slot"), 1080, 125, 760, 805, 7)

    # Labels, including preserved creation controls, must let Slate route clicks
    # to their owning buttons. EditableTextBox remains independently interactive.
    tree = base.umg("GetWidgets", widget)
    for info in tree.widgets:
        item = info.widget
        if isinstance(item, unreal.TextBlock):
            item.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

    missing = sorted(required - widget_map(widget).keys())
    if missing:
        raise RuntimeError(f"최종 필수 바인딩 누락: {missing}")
    if not base.umg("CompileWidgetBlueprint", widget):
        raise RuntimeError("Character Lobby Widget 컴파일 실패")
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(
        WIDGET_PATH
    )
    if not generated_class:
        raise RuntimeError("Character Lobby GeneratedClass 로드 실패")
    lobby_cdo = unreal.get_default_object(generated_class)
    lobby_cdo.set_editor_property(
        "customization_catalog",
        base.require_asset(base.CATALOG_PATH),
    )
    lobby_cdo.set_editor_property(
        "preview_transparency_material",
        base.require_asset(PREVIEW_MATERIAL_PATH),
    )
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        widget, only_if_is_dirty=False
    ):
        raise RuntimeError("Character Lobby Widget 저장 실패")
    log("승인 시안의 금색·남색 3슬롯 캐릭터 선택창 적용 완료")


def main():
    background_texture = import_background()
    build_widget(background_texture)


if __name__ == "__main__":
    main()
