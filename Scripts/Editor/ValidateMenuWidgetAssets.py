"""Compile and validate organized Rogue10m menu Widget Blueprints."""
import unreal

ASSETS = {
    "/Game/Widget/Component/Inventory/WBP_InventoryCell": {"UI_InventoryCellFrame", "UI_InventoryCellFill"},
    "/Game/Widget/Component/Inventory/WBP_InventoryItem": {
        "UI_InventoryItemSize",
        "UI_InventoryItemIcon",
        "UI_InventoryItemQuantityText",
        "UI_InventoryItemRarityBackground",
        "UI_InventoryItemPreviewBorder",
    },
    "/Game/Widget/Component/Inventory/WBP_InventoryItemTooltip": {
        "UI_ItemNameText",
        "UI_ItemDescriptionText",
        "UI_ItemWeightText",
        "UI_ItemTooltipSize",
        "UI_ItemActionHintText",
        "UI_ItemIcon",
        "UI_ItemStatsSection",
        "UI_ItemStatsContainer",
        "UI_EquippedItemPanel",
        "UI_EquippedItemIcon",
        "UI_EquippedItemNameText",
        "UI_EquippedItemDescriptionText",
        "UI_EquippedItemStatsContainer",
        "UI_ComparisonStatsContainer",
    },
    "/Game/Widget/Component/Equipment/WBP_EquipmentSlotAction": {
        "UI_ActionDismissButton",
        "UI_EquipmentSlotActionSize",
        "UI_EquipmentSlotActionFrame",
        "UI_EquipmentItemNameText",
        "UI_UnequipButton",
        "UI_UnequipButtonText",
        "UI_ActionResultText",
    },
    "/Game/Widget/Component/Inventory/WBP_BagTab": {"UI_BagTabButton", "UI_BagNameText"},
    "/Game/Widget/Component/Inventory/WBP_InventoryWindow": {
        "UI_WindowRoot",
        "UI_WindowDragHandle",
        "UI_InventoryGridFrame",
        "UI_InventoryGrid",
        "UI_InventoryItemCanvas",
    },
    "/Game/Widget/Component/Equipment/WBP_EquipmentWindow": {
        "UI_WindowRoot",
        "UI_WindowDragHandle",
        "UI_EquipmentSlotContainer",
        "UI_CharacterPreviewImage",
        "UI_EquipmentStatsFrame",
        "UI_EquipmentStatsContainer",
        "UI_StatsTitleText",
        "UI_AttackStatText",
        "UI_DefenseStatText",
        "UI_MaxHealthStatText",
        "UI_CriticalChanceStatText",
        "UI_AttackSpeedStatText",
        "UI_MoveSpeedStatText",
        "UI_WeaponSlotIcon",
        "UI_HeadSlotIcon",
        "UI_ChestSlotIcon",
        "UI_HandsSlotIcon",
        "UI_RingSlotIcon",
        "UI_FeetSlotIcon",
        "UI_NecklaceSlotIcon",
        "UI_WeaponSlotLocationText",
        "UI_HeadSlotLocationText",
        "UI_ChestSlotLocationText",
        "UI_HandsSlotLocationText",
        "UI_RingSlotLocationText",
        "UI_FeetSlotLocationText",
        "UI_NecklaceSlotLocationText",
    },
    "/Game/Widget/Component/SkillTree/WBP_SkillTreeEntry": {"UI_SkillIconImage", "UI_SkillNameText"},
    "/Game/Widget/Component/SkillTree/WBP_SkillTreeWindow": {"UI_SkillListContainer"},
}
CLASS_REFERENCES = {
    "/Game/Widget/Component/Inventory/WBP_InventoryWindow": {
        "inventory_cell_widget_class": "/Game/Widget/Component/Inventory/WBP_InventoryCell",
        "inventory_item_widget_class": "/Game/Widget/Component/Inventory/WBP_InventoryItem",
    },
    "/Game/Widget/Component/Equipment/WBP_EquipmentWindow": {
        "equipment_item_tooltip_widget_class": "/Game/Widget/Component/Inventory/WBP_InventoryItemTooltip",
        "equipment_slot_action_widget_class": "/Game/Widget/Component/Equipment/WBP_EquipmentSlotAction",
    },
    "/Game/Widget/Component/SkillTree/WBP_SkillTreeWindow": {
        "skill_tree_entry_widget_class": "/Game/Widget/Component/SkillTree/WBP_SkillTreeEntry",
    },
}
STARTER_EQUIPMENT_STATS = {
    "/Game/DataAsset/Item/Starter/DA_Item_IronLongsword": {
        "attack_power_bonus": 12.0,
        "critical_chance_bonus": 0.02,
    },
    "/Game/DataAsset/Item/Starter/DA_Item_IronHelmet": {
        "defense_bonus": 5.0,
        "max_health_bonus": 10.0,
    },
    "/Game/DataAsset/Item/Starter/DA_Item_GuardianHelmet": {
        "defense_bonus": 10.0,
        "max_health_bonus": 20.0,
    },
    "/Game/DataAsset/Item/Starter/DA_Item_LeatherArmor": {
        "defense_bonus": 12.0,
        "max_health_bonus": 25.0,
    },
    "/Game/DataAsset/Item/Starter/DA_Item_LeatherBoots": {
        "defense_bonus": 3.0,
        "move_speed_bonus": 20.0,
    },
    "/Game/DataAsset/Item/Starter/DA_Item_NoviceRing": {
        "attack_power_bonus": 3.0,
        "critical_chance_bonus": 0.03,
        "attack_speed_bonus": 0.04,
    },
}
RARITY_COLORS = {
    unreal.Rogue10mItemRarity.COMMON: (0.92, 0.92, 0.92, 0.22),
    unreal.Rogue10mItemRarity.RARE: (0.08, 0.28, 1.0, 0.34),
    unreal.Rogue10mItemRarity.EPIC: (0.52, 0.12, 0.9, 0.36),
    unreal.Rogue10mItemRarity.UNIQUE: (1.0, 0.32, 0.025, 0.38),
    unreal.Rogue10mItemRarity.MYTHIC: (0.92, 0.025, 0.02, 0.40),
}
STARTER_ITEM_RARITIES = {
    "/Game/DataAsset/Item/Starter/DA_Item_HealthPotion": unreal.Rogue10mItemRarity.COMMON,
    "/Game/DataAsset/Item/Starter/DA_Item_IronLongsword": unreal.Rogue10mItemRarity.RARE,
    "/Game/DataAsset/Item/Starter/DA_Item_IronHelmet": unreal.Rogue10mItemRarity.RARE,
    "/Game/DataAsset/Item/Starter/DA_Item_GuardianHelmet": unreal.Rogue10mItemRarity.EPIC,
    "/Game/DataAsset/Item/Starter/DA_Item_LeatherArmor": unreal.Rogue10mItemRarity.EPIC,
    "/Game/DataAsset/Item/Starter/DA_Item_LeatherBoots": unreal.Rogue10mItemRarity.COMMON,
    "/Game/DataAsset/Item/Starter/DA_Item_NoviceRing": unreal.Rogue10mItemRarity.UNIQUE,
}
CHARACTER_DATA_PATH = "/Game/DataAsset/Character/DA_Character_Default"
CHARACTER_BASE_STATS = {
    "max_health": 100.0,
    "max_stamina": 100.0,
    "max_mana": 100.0,
    "attack_power": 10.0,
    "defense": 0.0,
    "critical_chance": 0.05,
    "critical_damage_multiplier": 1.5,
    "attack_speed_multiplier": 1.0,
    "walk_speed": 600.0,
    "sprint_speed": 900.0,
}


def color_tuple(color):
    return (color.r, color.g, color.b, color.a)


def colors_nearly_equal(actual, expected, tolerance=0.0001):
    return all(abs(actual_value - expected_value) <= tolerance
               for actual_value, expected_value in zip(actual, expected))



def load_blueprint_class(path):
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not generated_class:
        raise RuntimeError(f"Missing generated class: {path}")
    return generated_class


def main():
    tool = unreal.get_default_object(unreal.UMGToolSet)
    if not hasattr(unreal.Rogue10mInventoryComponent, "try_unequip_item_to_first_available_grid"):
        raise RuntimeError("TryUnequipItemToFirstAvailableGrid is not exposed to Blueprint/Python")

    for path, required in ASSETS.items():
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            raise RuntimeError(f"Missing asset: {path}")
        tree = tool.call_method("GetWidgets", (asset,))
        names = {info.widget.get_name() for info in tree.widgets if info.widget}
        missing = required - names
        if missing:
            raise RuntimeError(f"Missing widgets in {path}: {sorted(missing)}")
        if path.endswith("/WBP_InventoryItem"):
            slots = {
                info.widget.get_name(): info.slot
                for info in tree.widgets
                if info.widget and info.slot
            }
            expected_layers = {
                "UI_InventoryItemRarityBackground": 0,
                "UI_InventoryItemPreviewBorder": 1,
                "UI_InventoryItemQuantityText": 2,
            }
            for widget_name, expected_layer in expected_layers.items():
                actual_layer = slots[widget_name].get_editor_property("layer")
                if actual_layer != expected_layer:
                    raise RuntimeError(
                        f"Invalid layer {widget_name}: {actual_layer}, expected {expected_layer}"
                    )
        if path.endswith("/WBP_InventoryItemTooltip"):
            widgets = {
                info.widget.get_name(): info.widget
                for info in tree.widgets
                if info.widget
            }
            tooltip_width = widgets["UI_ItemTooltipSize"].get_editor_property("width_override")
            expected_width = 322.0 if "UI_ItemKindText" in widgets else 280.0
            if abs(tooltip_width - expected_width) > 0.01:
                raise RuntimeError(f"Invalid default Tooltip width: {tooltip_width}")
            equipped_visibility = widgets["UI_EquippedItemPanel"].get_visibility()
            if equipped_visibility != unreal.SlateVisibility.COLLAPSED:
                raise RuntimeError(
                    f"Equipped comparison panel must default to Collapsed: {equipped_visibility}"
                )
            action_hint_visibility = widgets["UI_ItemActionHintText"].get_visibility()
            if action_hint_visibility != unreal.SlateVisibility.COLLAPSED:
                raise RuntimeError(
                    "Item action hint must default to Collapsed: "
                    f"{action_hint_visibility}"
                )
        if path.endswith("/WBP_EquipmentSlotAction"):
            widgets = {
                info.widget.get_name(): info.widget
                for info in tree.widgets
                if info.widget
            }
            result_visibility = widgets["UI_ActionResultText"].get_visibility()
            if result_visibility != unreal.SlateVisibility.COLLAPSED:
                raise RuntimeError(
                    f"Equipment action result must default to Collapsed: {result_visibility}"
                )
        if "UI_BagSizeText" in names:
            raise RuntimeError("UI_BagSizeText still exists")
        if path.endswith("/WBP_InventoryWindow") and "UI_BagTabContainer" in names:
            raise RuntimeError("UI_BagTabContainer still exists in the single-bag window")
        if not tool.call_method("CompileWidgetBlueprint", (asset,)):
            raise RuntimeError(f"Compile failed: {path}")
        unreal.log(f"[Rogue10mMenuValidation] OK {path}: {sorted(names)}")

    for owner_path, properties in CLASS_REFERENCES.items():
        owner_cdo = unreal.get_default_object(load_blueprint_class(owner_path))
        for property_name, expected_path in properties.items():
            actual_class = owner_cdo.get_editor_property(property_name)
            expected_class = load_blueprint_class(expected_path)
            if actual_class != expected_class:
                raise RuntimeError(
                    f"Invalid class reference {owner_path}.{property_name}: "
                    f"expected {expected_path}, got {actual_class}"
                )
            unreal.log(
                f"[Rogue10mMenuValidation] REF {owner_path}.{property_name} -> {expected_path}"
            )
    inventory_item_cdo = unreal.get_default_object(
        load_blueprint_class("/Game/Widget/Component/Inventory/WBP_InventoryItem")
    )
    tooltip_class = inventory_item_cdo.get_editor_property(
        "inventory_item_tooltip_widget_class"
    )
    if not tooltip_class:
        raise RuntimeError("Inventory Item Tooltip native Soft Class is not configured")
    unreal.log(
        f"[Rogue10mMenuValidation] TOOLTIP CLASS -> {tooltip_class}"
    )
    for asset_path, expected_stats in STARTER_EQUIPMENT_STATS.items():
        item_asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not item_asset:
            raise RuntimeError(f"Missing starter equipment asset: {asset_path}")
        equipment_stats = item_asset.get_editor_property("equipment_stats")
        for property_name, expected_value in expected_stats.items():
            actual_value = equipment_stats.get_editor_property(property_name)
            if abs(actual_value - expected_value) > 0.0001:
                raise RuntimeError(
                    f"Invalid equipment stat {asset_path}.{property_name}: "
                    f"expected {expected_value}, got {actual_value}"
                )
        unreal.log(
            f"[Rogue10mMenuValidation] EQUIPMENT STATS {asset_path}: "
            f"{expected_stats}"
        )
    rarity_probe = unreal.EditorAssetLibrary.load_asset(
        "/Game/DataAsset/Item/Starter/DA_Item_HealthPotion"
    )
    original_rarity = rarity_probe.get_editor_property("rarity")
    try:
        for rarity, expected_color in RARITY_COLORS.items():
            rarity_probe.set_editor_property("rarity", rarity)
            actual_color = color_tuple(
                rarity_probe.get_inventory_rarity_background_color()
            )
            if not colors_nearly_equal(actual_color, expected_color):
                raise RuntimeError(
                    f"Invalid rarity color {rarity}: expected {expected_color}, "
                    f"got {actual_color}"
                )
            unreal.log(
                f"[Rogue10mMenuValidation] RARITY COLOR {rarity}: {actual_color}"
            )
    finally:
        rarity_probe.set_editor_property("rarity", original_rarity)

    for asset_path, expected_rarity in STARTER_ITEM_RARITIES.items():
        item_asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        icon_scale = float(item_asset.get_editor_property("inventory_icon_scale"))
        if abs(icon_scale - 1.0) > 0.0001:
            raise RuntimeError(
                f"Invalid starter icon scale {asset_path}: expected 1.0, got {icon_scale}"
            )
        unreal.log(f"[Rogue10mMenuValidation] ICON SCALE {asset_path}: {icon_scale}")
        actual_rarity = item_asset.get_editor_property("rarity")
        if actual_rarity != expected_rarity:
            raise RuntimeError(
                f"Invalid starter rarity {asset_path}: expected {expected_rarity}, "
                f"got {actual_rarity}"
            )
        unreal.log(f"[Rogue10mMenuValidation] RARITY {asset_path}: {actual_rarity}")

    character_data = unreal.EditorAssetLibrary.load_asset(CHARACTER_DATA_PATH)
    if not character_data:
        raise RuntimeError(f"Missing default character data asset: {CHARACTER_DATA_PATH}")
    for property_name, expected_value in CHARACTER_BASE_STATS.items():
        actual_value = float(character_data.get_editor_property(property_name))
        if abs(actual_value - expected_value) > 0.0001:
            raise RuntimeError(
                f"Invalid character base stat {property_name}: "
                f"expected {expected_value}, got {actual_value}"
            )
        unreal.log(
            f"[Rogue10mMenuValidation] CHARACTER STAT {property_name}: {actual_value}"
        )
    unreal.log("[Rogue10mMenuValidation] All organized menu widgets passed.")


if __name__ == "__main__":
    main()
